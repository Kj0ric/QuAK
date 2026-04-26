/*
Porting reference for the current cached `Inf/LimInf x {Max_f, Min_f}` backend.

This file is intentionally not part of the build. It is a transplant guide in
`.cpp` form so the implementation can be carried to another version of
`NestedAutomaton.cpp` without re-deriving the design.

Scope:
- current cached backend: `flatten_MinMax_Inf_cached(...)`
- source of truth: current `src/NestedAutomaton.cpp` and `src/NestedAutomaton.h`
- not a SumPlus/SumMinus plan

What this backend actually is:
- It keeps the shared threshold-obligation semantics from the live backend.
- It changes only the representation and memoization strategy.
- It is Min/Max-specific because it stores each obligation frontier as two
  state sets (`y0`, `y1`) instead of a generic `(state, prog)` frontier.

Current live source locations:
- public header declarations:
  `src/NestedAutomaton.h:12-42`, `src/NestedAutomaton.h:58-60`,
  `src/NestedAutomaton.h:89`
- shared threshold backend prerequisites:
  `src/NestedAutomaton.cpp:189-193`
  `src/NestedAutomaton.cpp:1061-1148`
  `src/NestedAutomaton.cpp:3847-4563`
- cached-only implementation:
  `src/NestedAutomaton.cpp:8874-9500`

Use this file in one of two ways:

1. Target version already has the shared threshold backend.
   Then only port:
   - the header delta
   - the cached-only implementation block
   - the public wrapper

2. Target version does not have the shared threshold backend.
   Then port, in order:
   - `edgeWeightToChildIndex(...)`
   - `ChildTables` + `build_child_tables(...)`
   - the shared threshold helper block up to and including
     `thrext_build_child_info(...)`
   - the experiment stats plumbing
   - the cached-only implementation block
   - the public wrapper

Non-negotiable invariants:

1. This backend must continue to reuse `thrext_build_child_info(...)`.
   Do not fork the Min/Max semantics into a second copy. The cached backend is
   meant to reuse the same liveness tables and threshold semantics as the
   shared backend.

2. This backend is Min/Max-only.
   The `y0`/`y1` representation works because Min/Max progress is binary. Do
   not reuse it for SumPlus/SumMinus.

3. The flattened state key is:
   `(parent_state_id, P1_bag_id, P2_bag_id, phase, epoch_nonempty)`.
   This is the whole reason state-map operations become cheap compared to the
   generic backend's deep `ThrExtBuchiState` key.

4. The bag and obligation arenas are interned by ID.
   If you lose interning, you lose most of the benefit.

5. Do not hold references into `obls` or `bags` across `intern_obl(...)` or
   `intern_bag(...)`.
   Those functions can `push_back(...)` into `std::vector` arenas and
   invalidate references. This exact bug existed and was fixed.

6. Current singleton-child behavior is inherited from the shared backend.
   The constructor skips children with `states()->size() < 2`, so those edges
   are currently treated as silent in the cached backend too. If the target
   version has different singleton-child semantics, adjust this deliberately.

Minimal header delta to carry:

`src/NestedAutomaton.h`

```cpp
struct MinMaxInfExperimentStats {
    uint64_t state_map_lookup_calls = 0;
    uint64_t state_map_insert_calls = 0;

    uint64_t spawn_calls = 0;
    uint64_t unique_spawn_keys = 0;

    uint64_t step_bag_calls = 0;
    uint64_t unique_bag_step_keys = 0;
    uint64_t step_bag_cache_hits = 0;

    uint64_t step_obl_calls = 0;
    uint64_t step_obl_cache_hits = 0;

    uint64_t bag_add_calls = 0;
    uint64_t bag_add_cache_hits = 0;

    uint64_t bag_copy_ops = 0;
    uint64_t bag_copy_entries = 0;

    uint64_t frontier_observations = 0;
    uint64_t frontier_config_total = 0;
    uint64_t frontier_capacity_total = 0;

    uint64_t unique_obligation_count = 0;
    uint64_t unique_bag_count = 0;

    double time_step_bag_ms = 0.0;
    double time_state_map_ms = 0.0;
    double time_bag_copy_ms = 0.0;
};

class NestedAutomaton : public Automaton {
    friend class NestedAutomatonTester;
private:
    static void setMinMaxInfExperimentStatsEnabled(bool enabled);
    static void resetMinMaxInfExperimentStats();
    static MinMaxInfExperimentStats getMinMaxInfExperimentStats();
public:
    Automaton* flatten_MinMax_Inf_cached(value_function_t finite_aggregator,
                                         weight_t threshold);
};
```

Hard dependencies from the current `NestedAutomaton.cpp`:

- `edgeWeightToChildIndex(...)`
  Current location: `src/NestedAutomaton.cpp:189-193`

- `ChildTables` and `build_child_tables(...)`
  Current location: `src/NestedAutomaton.cpp:1061-1148`

- shared threshold types
  Current location: `src/NestedAutomaton.cpp:3847-3890`
  Required symbols:
  - `thrext_int_t`
  - `THREXT_INF`
  - `ThrExtMode`
  - `ThrExtConf`
  - `ThrExtFrontier`
  - `ThrExtOblKey`
  - `ThrExtOblEntry`
  - `ThrExtOblBag`

- experiment stats plumbing
  Current location: `src/NestedAutomaton.cpp:3891-3997`
  Required symbols:
  - `MinMaxInfExperimentContext`
  - `ScopedStatsTimer`
  - `g_minmax_inf_experiment`
  - `mmexp_enabled()`
  - `mmexp_record_thr_spawn(...)`
  - public `NestedAutomaton::{set,reset,get}MinMaxInfExperimentStats`

- shared Min/Max child semantics
  Current location: `src/NestedAutomaton.cpp:4034-4413`
  Required symbols:
  - `ThrExtChildInfo`
  - `thrext_build_child_info(...)`

Soft dependencies:

- `mmexp_record_thr_bag(...)`, `mmexp_record_thr_bag_copy(...)`
  These are useful for instrumentation parity, but if the target version does
  not care about stats you can remove the instrumentation path entirely.

Exact copy units for the cached-only block:

1. Hash helpers
   Current location: `src/NestedAutomaton.cpp:8874-8885`
   Required symbols:
   - `mm_mix64(...)`
   - `mm_hash_combine(...)`

2. `MMInfCachedBuilder`
   Current location: `src/NestedAutomaton.cpp:8887-9286`
   This should be copied as one unit.

3. `flatten_MinMax_Inf_cached_impl(...)`
   Current location: `src/NestedAutomaton.cpp:9288-9490`
   This should be copied as one unit.

4. public wrapper
   Current location: `src/NestedAutomaton.cpp:9494-9500`

Why the cached block should move as a unit:

- `MMInfCachedBuilder` depends on the sentinel values of its own arenas.
- `flatten_MinMax_Inf_cached_impl(...)` depends on the builder's exact `Key`
  shape and `spawn_code(...)` / `step_bag(...)` / `bag_add_obl(...)` contract.
- Splitting the block tends to create subtle mismatches in `BAG_DEAD`,
  `OBL_DISCHARGED`, or phase handling.

Critical bug fix that must remain in any port:

The original cached prototype had a use-after-free bug because it held
references to `bags[bid]` and `obls[id]` across `intern_bag(...)` and
`intern_obl(...)`, which can reallocate the underlying vectors.

The fixed code is:

```cpp
const BagId nb = intern_bag(std::move(out));
bags[bid].step_next[sym] = nb;
bags[bid].step_any_discharged[sym] = any_d ? 1u : 0u;
```

and:

```cpp
const OblId nid = intern_obl(O.child, O.guess, std::move(next0), std::move(next1));
obls[id].step_cache[sym] = nid;
```

Do not rewrite those back to `B.step_next[...]` or `O.step_cache[...]` after an
interning call.

Transplant procedure
====================

Step 1. Confirm the target file already has the shared threshold backend.
If it already has `ThrExtChildInfo` and `thrext_build_child_info(...)`, use
them. If it does not, port the prerequisite blocks first.

Step 2. Add the header delta.
Without this, the cached entry point and stats plumbing will not be reachable.

Step 3. Copy the cached-only implementation block as a unit.
This means:
- hash helpers
- `MMInfCachedBuilder`
- `flatten_MinMax_Inf_cached_impl(...)`
- public wrapper

Step 4. Keep the existing child enable rule unless intentionally changing it.
The constructor currently does:

```cpp
if (!c) continue;
if (c->getStates()->size() < 2) continue;
build_child_tables(c, child_tab[i]);
thrext_build_child_info(child_tab[i], finVal, threshold, 1u, child_info[i]);
```

If the target version handles singleton children differently, change this on
purpose and re-test semantics. Do not "clean it up" casually.

Step 5. Preserve the flattened-state `Key` exactly.
That exact state shape is what makes the cached version cheap to hash and
compare:

```cpp
struct Key {
    uint32_t parent = 0;
    MMInfCachedBuilder::BagId P1 = 0u;
    MMInfCachedBuilder::BagId P2 = 0u;
    uint8_t phase = 1u;
    uint8_t epoch_nonempty = 0u;
};
```

Step 6. Preserve the phase logic exactly.
The cached flattener is not a new algorithm. It is the same epoch/final-pulse
discipline as the live shared backend, just with compact IDs.

Step 7. Preserve the spawn codes exactly.
`spawn_code(...)` can return:
- `OBL_DEAD`
- `OBL_DISCHARGED`
- a real `OblId`

The caller relies on those three cases.

Step 8. Preserve bag interning and bag-add memoization.
The core performance gains come from:
- `intern_obl(...)`
- `intern_bag(...)`
- `bag_add_cache`
- `step_cache` inside each obligation
- `step_next` inside each bag

Step 9. Keep `reserve(...)` and fixed-size per-symbol caches.
They are not decorative. They reduce allocator churn and are part of why the
cached backend wins on larger cases.

Step 10. Re-run the exact crash check after porting.
The old failure mode showed up under repeated in-process calls. A single run is
not enough to trust the port.

Recommended verification after transplant
=========================================

1. Build succeeds with the target version.

2. Re-run the direct Min/Max sanity suite:
- `./test_flatten_minmax_inf`

3. Re-run the repeated comparison harness:
- `/tmp/minmax_inf_fix_compare`

4. If the target version keeps ASan support, re-run the dedicated reproducer:
- `/tmp/repro_cached_deep_asan`

5. Compare `current` vs `cached` on the small resource family again.
The cached backend should remain a constant-factor optimization, not a semantic
change.

Short checklist
===============

- copy header declarations
- copy cached-only implementation as a unit
- reuse shared Min/Max helper semantics
- preserve `Key`
- preserve sentinel values
- preserve bag/obligation interning
- preserve post-intern writes through `bags[bid]` / `obls[id]`
- preserve epoch/final-pulse logic
- verify with repeated in-process runs

Optional exact wrapper to carry verbatim:

```cpp
Automaton* NestedAutomaton::flatten_MinMax_Inf_cached(value_function_t finite_aggregator,
                                                      weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf_cached requires Max_f or Min_f");
    }
    return flatten_MinMax_Inf_cached_impl(this, finite_aggregator, threshold);
}
```

Optional note on what not to port:

- do not copy the dead legacy body under `flatten_MinMax_Inf(...)`
- do not copy `threshold_obl` if the goal is specifically the cached backend
- do not mix the generic `ThrExtBuchiState` path with the cached `Key` path
  inside one worklist loop
*/

namespace cached_minmax_inf_porting_reference {
// Intentionally empty. This file is documentation in `.cpp` form.
}
