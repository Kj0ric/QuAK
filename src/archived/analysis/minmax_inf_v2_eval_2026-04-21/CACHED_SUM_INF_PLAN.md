# Cached SumPlus/SumMinus `Inf/LimInf` Plan

## Goal

Apply the same **representation idea** used by the experimental cached
`Inf/LimInf x {Max_f, Min_f}` backend to the live
`Inf/LimInf x {SumPlus, SumMinus}` path, without forking the Sum semantics away
from the shared threshold-extremal backend.

This plan is for:
- `NestedAutomaton::flatten_SumPlusMinus_Inf(...)`
- the same binary-weight Buchi flattening used later by `Inf` and `LimInf`
  emptiness

This plan is **not** for:
- `Sup/LimSup` promotion yet
- reviving the old interval-budget code below the early `return`
- copying the Min/Max bitset-style frontier encoding into Sum

## Bottom Line

The cached idea does transfer to SumPlus/SumMinus, but only partially.

What transfers directly:
- intern obligations into small integer IDs
- intern obligation bags into small integer IDs
- cache spawn results
- cache obligation stepping
- cache bag stepping
- replace deep `ThrExtBuchiState` keys with a compact visited-state key

What does **not** transfer literally:
- the Min/Max-specific `y0/y1` frontier split
- the Min/Max-specific `mm_live[guess][progress]` precomputation
- the assumption that progress is just one bit

The correct Sum version should keep the current shared Sum semantics and only
replace the expensive representation layer.

## Current Code Facts

The live Sum `Inf/LimInf` path already uses the shared threshold-extremal
backend:
- `flatten_SumPlusMinus_Inf(...)` in `src/NestedAutomaton.cpp`
- it immediately returns `flatten_threshold_extremal_impl(this, finite_aggregator, threshold)`

The Sum semantics are already centralized in shared helpers:
- `compute_weight_scale(...)`
- `thrext_build_child_info(...)`
- `thrext_step_prog(...)`
- `thrext_discharge_ok(...)`
- `thrext_frontier_canonicalize(...)`
- `thrext_is_live(...)`
- `thrext_spawn_obligation(...)`
- `thrext_step_obl_bag(...)`

This is the key architectural fact: the Sum logic is already in the right
place. The current opportunity is to replace the **data representation and
memoization strategy**, not the meaning of SumPlus/SumMinus obligations.

## What `cached` Really Contributes

The existing Min/Max cached backend wins because it removes three concrete
costs from the live shared backend:

1. Deep visited-state keys
   - `current` stores `ThrExtBuchiState` directly in the visited map.
   - `cached` stores only `parent`, `P1_bag_id`, `P2_bag_id`, `phase`,
     `epoch_nonempty`.

2. Deep bag copying and insertion
   - `current` copies `P1` and `P2` bags repeatedly.
   - `cached` interns bags and uses `bag_add(base_bag_id, obligation_id)`.

3. Repeated stepping of the same structure
   - `current` recomputes `step(P, sym)` for identical bags and obligations.
   - `cached` memoizes both.

For Sum, these same overheads still exist in the current shared backend.
So the caching strategy is still relevant, even though the frontier payload is
different.

## Recommended Architecture

### Core Recommendation

Implement a new experimental backend:
- `flatten_SumPlusMinus_Inf_cached(...)`

but make it:
- semantically tied to the shared threshold-extremal helpers
- structurally separate from the live `flatten_threshold_extremal_impl(...)`
- explicitly experimental until differential testing and profiling clear it

### Important Non-Recommendation

Do **not** start by trying to fully genericize `MMInfCachedBuilder` into a
perfect reusable framework shared by Min/Max and Sum.

That may become the right end state, but it is too much coupled refactoring for
the first implementation. The safest first version is:

1. keep Sum semantics in the shared helper layer
2. build a Sum-specific cached builder that calls those helpers
3. validate it hard
4. only then decide whether to factor a generic cached core

This avoids mixing a semantic migration with a major refactor.

## Sum-Specific Design Constraints

### 1. Progress is scalar, not binary

For Min/Max, each obligation frontier can be split into:
- states at progress `0`
- states at progress `1`

For SumPlus/SumMinus, progress is an integer in `[0, cap]` after scaling.
So the Sum cached obligation must carry a canonical sparse frontier of:
- `(state_id, prog)` pairs

not `y0/y1`.

### 2. Progress domain is threshold-dependent

In Sum modes:
- `goal` and `cap` come from `thrext_build_child_info(...)`
- `compute_weight_scale(...)` determines the scaling
- forced-threshold cases collapse the guess space

These are semantic rules and must remain shared.

### 3. Frontier diversity can be much higher than Min/Max

Even after canonicalization, Sum obligations can vary by many different
progress values. This means:
- more unique obligations are likely
- interning pressure may be higher
- memory pressure may be higher
- hash cost on frontiers may be higher

Because of that, the Sum cached plan should be more conservative about memory
than the Min/Max prototype.

## Data Model

## Cached Obligation

Recommended first-version obligation representation:

```cpp
struct SumCachedObl {
    uint32_t child = 0;
    uint8_t guess = 0;
    ThrExtFrontier conf;              // canonical sorted (state, prog) frontier
    std::vector<OblId> step_cache;    // per-symbol memo
};
```

Where:
- `conf` is always canonicalized with `thrext_frontier_canonicalize(...)`
- `guess` remains the same 0/1 threshold guess used by the shared backend
- `step_cache[sym]` stores one of:
  - `OBL_UNKNOWN`
  - `OBL_DEAD`
  - `OBL_DISCHARGED`
  - a real `OblId`

## Cached Bag

Recommended bag representation:

```cpp
struct SumCachedBag {
    std::vector<OblId> obls;          // sorted unique obligation IDs
    std::vector<BagId> step_next;     // memoized bag step per parent symbol
    std::vector<uint8_t> step_any_discharged;
};
```

This is directly analogous to the Min/Max cached builder and should transfer
almost unchanged.

## Cached Flattened-State Key

Recommended visited-state key:

```cpp
struct Key {
    uint32_t parent = 0;
    BagId P1 = 0u;
    BagId P2 = 0u;
    uint8_t phase = 1u;
    uint8_t epoch_nonempty = 0u;
};
```

This should be identical in meaning to the Min/Max cached backend and to the
shared threshold-extremal loop.

## Interning and Hashing Strategy

### Obligation interning

Intern obligations by hashing:
- `child`
- `guess`
- frontier length
- each `(state, prog)` pair in canonical order

Equality must check:
- same child
- same guess
- same frontier

### Bag interning

Intern bags by hashing:
- obligation-count
- each `OblId` in sorted order

### Important memory recommendation

For the Sum version, prefer child-local step-cache sizing:
- obligation `step_cache` should be sized to `child_tab[child].alph`
- not the full parent alphabet if those differ

Reason:
- Sum is more likely to create many unique obligations than Min/Max
- a full-width per-obligation global-alphabet cache can become a memory sink

The bag-level cache still has to be parent-alphabet sized because bag stepping
is queried at the parent-symbol level.

## Shared Semantics That Must Not Be Forked

The Sum cached backend must call the existing shared helpers rather than
reimplementing their logic.

### Keep shared

Keep these in the shared layer:
- `compute_weight_scale(...)`
- `thrext_build_child_info(...)`
- `thrext_step_prog(...)`
- `thrext_discharge_ok(...)`
- `thrext_frontier_canonicalize(...)`
- `thrext_is_live(...)`

### Also factor out shared low-level frontier helpers

Before writing the Sum cached builder, extract two more shared helpers from the
current generic backend:

1. `thrext_spawn_frontier(...)`
   - input: `(child, symbol, guess, child_tab, child_info)`
   - output:
     - reject/dead
     - discharged immediately
     - canonical frontier

2. `thrext_step_frontier(...)`
   - input: `(child, guess, frontier, symbol, child_tab, child_info)`
   - output:
     - dead
     - discharged
     - canonical frontier

Then:
- make current `thrext_spawn_obligation(...)` wrap `thrext_spawn_frontier(...)`
- make current `thrext_step_obl_bag(...)` use `thrext_step_frontier(...)`
- make the new Sum cached builder use those exact helpers too

This is the single most important anti-drift step in the plan.

## Semantics Note: One-State Children

The current threshold-extremal code still skips children with:
- `c->getStates()->size() < 2`

That is not the desired long-term semantics.

Planned semantic rule:
- one-state child with a final state: treat as silent
- one-state child with a non-final state: disallow / reject as malformed

For this Sum cached project:
- do not silently hard-code the current `size() < 2` test into new logic as a
  permanent semantic choice
- isolate child classification behind one helper so this rule can later be
  fixed in one place for both `current` and `cached_sum`

Do not patch that global behavior as part of the first Sum cached prototype.
Just avoid making it harder to fix later.

## Detailed Implementation Plan

## Phase 0: Prepare the shared semantic surface

### Goal

Make the live generic backend and the future Sum cached backend share the same
low-level frontier semantics.

### Changes

1. Add a helper for child participation classification.
   It should distinguish at least:
   - enabled tracked child
   - silent child
   - invalid singleton non-final child

2. Factor `thrext_spawn_frontier(...)` out of the current
   `thrext_spawn_obligation(...)`.

3. Factor `thrext_step_frontier(...)` out of the current
   `thrext_step_obl_bag(...)`.

4. Keep `thrext_spawn_obligation(...)` and `thrext_step_obl_bag(...)` as thin
   wrappers on top of the new helpers so current behavior stays unchanged.

### Exit criteria

- no change in current `flatten_SumPlusMinus_Inf(...)` behavior
- no change in current `flatten_MinMax_Inf(...)` behavior
- existing threshold-extremal tests still pass

## Phase 1: Add an experimental public entry point

### Goal

Introduce the Sum cached backend without touching production dispatch.

### Changes

1. Add to `NestedAutomaton.h`:

```cpp
Automaton* flatten_SumPlusMinus_Inf_cached(value_function_t finite_aggregator,
                                           weight_t threshold);
```

2. Add a test-only wrapper in `src/tests/sanity_tests/test_common.h`.

3. Add a compare harness similar to the Min/Max tooling:
   - `sum_inf_fix_compare.cpp`
   - `sum_inf_backend_probe.cpp`

### Exit criteria

- the new entry point exists
- production still uses `flatten_SumPlusMinus_Inf(...)`
- nothing dispatches to cached by default

## Phase 2: Build `SumInfCachedBuilder`

### Goal

Implement the cached representation without changing semantics.

### Changes

1. Construct child tables exactly as the live shared backend does.

2. Compute `weight_scale` exactly once with `compute_weight_scale(A)`.

3. Build `ThrExtChildInfo` with `thrext_build_child_info(...)`.

4. Reserve bag ID `0` as the empty bag.

5. Precompute spawn results for `(child, guess, symbol)` using
   `thrext_spawn_frontier(...)`.

6. Implement `intern_obl(...)`.

7. Implement `intern_bag(...)`.

8. Implement `bag_add_obl(base, add)`.

9. Implement `step_obl(id, sym)` by calling `thrext_step_frontier(...)`.

10. Implement `step_bag(id, sym)` by reusing `step_obl(...)`.

11. Implement the cached worklist loop using the compact key.

### Important details

#### Forced guesses

If `child_info[child].forced` is true:
- the disallowed guess should be precomputed as `OBL_DEAD`
- do not waste state space on the impossible branch

#### Symbol domains

If `sym >= child_tab[child].alph`:
- spawn is dead
- step is dead

Keep that behavior identical to the shared backend.

#### Reference safety

Do not hold `Bag&` or `Obl&` references across calls that may intern new bags or
obligations.

The Min/Max cached prototype previously crashed on exactly this pattern.
The Sum implementation should adopt the safe pattern from the start:
- read by ID
- after any call that may append to the arena, write back through
  `bags[bid]` / `obls[id]`

### Exit criteria

- `flatten_SumPlusMinus_Inf_cached(...)` builds and runs
- no production dispatch changes yet

## Phase 3: Differential correctness validation

### Goal

Prove that the cached Sum backend is just a representation change.

### Differential oracles

Primary oracle:
- `flatten_regular(...)`

Secondary oracle:
- current `flatten_SumPlusMinus_Inf(...)`

### Required comparisons

For each input/query pair, compare:
- emptiness result after silent removal
- flattened state count
- flattened transition count

If convenient, also compare printed flattened automata on small witnesses.

### Minimum required test families

1. Existing universal correctness suite inputs that already cover:
   - `SumPlus`
   - `SumMinus`
   - `Inf`
   - `LimInf`
   - fractional thresholds
   - nondeterministic children
   - multiple children
   - pump loops

2. New Sum-specific boundary tests:
   - `SumPlus`, threshold `<= 0`, forced guess `1`
   - `SumMinus`, threshold `> 0`, forced guess `0`
   - exact threshold boundary
   - exact `cap` boundary
   - zero-cost cycle with delayed final exit
   - positive-gain cycle hidden behind a zero-cost exit
   - dead spawn on first child symbol
   - immediate discharge on first child symbol

3. Repeated-call stability tests:
   - repeated cached calls on one `NestedAutomaton`
   - alternating `current` and `cached_sum`
   - fresh reload vs reused instance

### Exit criteria

- zero result mismatches against `regular`
- zero result mismatches against `current`
- any flat-size mismatch is investigated and explained before proceeding

## Phase 4: Instrumentation and profiling

### Goal

Verify that caching is paying for the right reasons.

### Reuse existing stats plumbing

The current Min/Max experiment stats are already close to what is needed:
- state-map lookups/inserts
- spawn count
- bag-step count and hits
- obligation-step count and hits
- frontier observations
- unique obligations/bags

For Sum, add a few extra counters if needed:
- frontier progress-pair count
- average distinct progress values per frontier
- spawn reject vs discharge vs nonempty counts
- forced-guess skip count

### Benchmark families

1. Positive resource-style family for `SumPlus`
   - non-negative costs
   - increasing thresholds
   - multiple children

2. Negative-cost family for `SumMinus`
   - varying depth and branching
   - increasingly negative thresholds

3. Fractional family
   - several decimal scales
   - thresholds that stress ceil/floor rounding

4. Reuse-heavy family
   - designed so many structurally identical obligations reappear

5. Reuse-poor family
   - designed so nearly every frontier is novel

The last two are important because Sum may benefit less than Min/Max if
frontier diversity is too high.

### Exit criteria

- clear evidence that cached removes bag-copy cost
- meaningful bag-step / obligation-step cache-hit rates on medium cases
- memory remains acceptable

## Phase 5: Hardening against memory blow-up

### Goal

Make the Sum cached backend robust enough for medium and larger thresholds.

### Risks to address

1. Too many unique obligations
   - Sum progress values can create far more frontier variants than Min/Max.

2. Too much per-obligation cache memory
   - especially if global alphabet is large.

3. Interning cost dominates
   - hashing long frontier vectors may erase the gain from compact visited keys.

### Mitigations

1. Use child-local `step_cache` sizes.

2. Add optional reservation heuristics for arenas and buckets.

3. Cache a frontier fingerprint inside the obligation if profiling shows
   interning hash cost is high.

4. Keep the first version sparse.
   Do not introduce dense per-progress bitsets or layered tables for Sum.

5. If memory is still too high, prefer lazy step-cache allocation over
   over-engineering the frontier representation.

### Exit criteria

- no major RSS regression vs `current` on medium benchmark cases
- no obvious pathological slowdown from hashing

## Phase 6: Promotion gate

### Goal

Define a hard bar before production switch.

### Promotion requirements

All of these should be true at once:

1. Correctness
   - zero mismatches vs `regular`
   - zero mismatches vs `current`

2. Stability
   - repeated-call regressions pass
   - sanitizer runs are clean

3. Performance
   - clearly faster than `current` on the main medium benchmark families
   - no catastrophic regression on reuse-poor inputs

4. Memory
   - acceptable RSS on medium and larger thresholds

5. Maintainability
   - Sum semantics still live in shared helpers
   - cached code has a narrow responsibility surface

### Rollout sequence

1. keep `flatten_SumPlusMinus_Inf_cached(...)` experimental
2. add backend selectors in benchmark / compare harnesses
3. collect evidence
4. if the gate passes, switch `flatten_SumPlusMinus_Inf(...)` to cached
5. keep the old shared-generic path behind a debug/testing switch for a while

## File-by-File Plan

### `src/NestedAutomaton.h`

- add `flatten_SumPlusMinus_Inf_cached(...)`
- keep stats plumbing reusable

### `src/NestedAutomaton.cpp`

- factor:
  - `thrext_spawn_frontier(...)`
  - `thrext_step_frontier(...)`
  - child classification helper
- add:
  - `SumInfCachedBuilder`
  - `flatten_SumPlusMinus_Inf_cached_impl(...)`
  - public wrapper

### `src/tests/sanity_tests/test_common.h`

- add tester wrapper for the Sum cached flattener

### new test/probe files

- `src/tests/probes/sum_inf_fix_compare.cpp`
- `src/tests/probes/sum_inf_backend_probe.cpp`

These should mirror the Min/Max tooling where useful, but target:
- `regular`
- `current`
- `cached_sum`

## Things Not To Do

Do not do any of these in the first implementation:

1. Do not copy the dead interval-budget code from the old Sum implementation.

2. Do not reimplement `compute_weight_scale(...)`, threshold rounding, or Sum
   liveness cutoffs inside the cached builder.

3. Do not switch production dispatch before differential testing is complete.

4. Do not attempt a grand generic cached refactor shared by Min/Max and Sum
   before there is a working Sum cached prototype.

5. Do not carry over the original cached Min/Max stale-reference bug pattern.

## Recommended Order of Work

1. Factor shared `spawn_frontier` / `step_frontier`.
2. Add experimental Sum cached entry point and test wrappers.
3. Implement `SumInfCachedBuilder`.
4. Build correctness comparison harness.
5. Run differential tests.
6. Add benchmark families and instrumentation.
7. Hardening pass for memory and repeated-use safety.
8. Decide on promotion only after data is convincing.

## Final Recommendation

The safest way to apply the cached idea to SumPlus/SumMinus is:

1. treat the current shared threshold-extremal Sum logic as the semantic
   source of truth
2. add a separate experimental cached Sum backend that reuses that logic
3. validate it against `regular` and the live backend
4. promote it only if it delivers a clear constant-factor win without semantic
   drift

That gives the project the likely performance gain from interning and
memoization without re-opening the much riskier problem of re-deriving the Sum
algorithm itself.
