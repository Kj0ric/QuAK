# Plan: Witness-Cached `Sup/LimSup x SumPlus/SumMinus`

Date: 2026-04-24

## Goal

Adapt the successful witness-cached `Sup/LimSup x {Max_f, Min_f}` shape to:

- `Sup x SumPlus`
- `LimSup x SumPlus`
- `Sup x SumMinus`
- `LimSup x SumMinus`

The intended outcome is a faster specialized flattener for the sum cases without
reviving the older string/budget-set `flatten_SumPlusMinus_Sup(...)`
implementation.

The implementation should be staged as experimental first. Promotion to the live
`flatten_SumPlusMinus_Sup(...)` path should happen only after the new backend
matches the current threshold-extremal backend and the regular `SumB` oracle on
trusted cases.

Review update:

- the plan remains experimental-first
- the current threshold-extremal backend is a regression oracle, not a complete
  semantic oracle
- trusted regular-oracle and hand-fixture checks are still required before
  promotion

## Existing Pieces To Reuse

Relevant live code:

- `src/NestedAutomaton.cpp`
  - `TermCachedBuilder`
  - `MMSupCachedBuilder`
  - `flatten_MinMax_Sup_witness_cached_impl(...)`
  - `SumInfCachedBuilder`
  - `thrext_*` helpers used by `SumPlus/SumMinus`
- `src/NestedAutomaton.h`
  - private flattener declarations
- `src/tests/probes/minmax_sup_fix_compare.cpp`
  - comparison harness pattern for `Sup/LimSup x {Max_f, Min_f}`
- `src/tests/probes/sum_inf_fix_compare.cpp`
  - comparison harness pattern for cached sum obligations

The key reuse decision:

- use `TermCachedBuilder` for background child termination only
- use the sum threshold-obligation machinery for exactly one distinguished
  witness child
- keep the same `B1/B2 + W1/W2 + phase + epoch_nonempty` acceptance discipline
  as `flatten_MinMax_Sup_witness_cached_impl(...)`

Do not use the older recursive selector or `BudgetSet` path for this backend.

## Semantics

For each real parent call, the flattened automaton has two choices:

1. Spawn the child as background only.
   - This adds a termination obligation to `B1`.
   - The output edge has weight `0`.
2. If no witness is currently active, spawn the child as the witness.
   - This creates a sum threshold obligation with `guess = 1`.
   - The output edge has weight `1`.
   - The run remains accepting only if this witness obligation eventually
     discharges with value `>= threshold`.

This deliberately emits weight `1` at witness spawn time, matching the current
Min/Max witness-cached backend. For `Sup` and `LimSup`, this is equivalent to
emitting success at return time because an accepting run cannot keep a witness
forever: `W2` participates in the same epoch-empty condition as background
obligations. Every accepted weight-`1` witness spawn therefore corresponds to a
later successful child return, and every successful child return can be selected
at its spawn.

## Sum Threshold Meaning

Reuse the existing `thrext_*` semantics.

For `SumPlus`:

- interesting thresholds are `x > 0`
- `x <= 0` is already trivial in `isNonEmpty(...)`
- progress is `min(goal, accumulated_gain)`
- `goal = ceil(x * scale)`
- `guess = 1` means the child can return with accumulated gain at least `goal`
- child edge weights must be non-negative, as the shared threshold backend
  already expects

For `SumMinus`:

- interesting thresholds are `x <= 0`
- `x > 0` is already trivial false in `isNonEmpty(...)`
- progress is `min(goal + 1, accumulated_loss)`
- `goal = floor((-x) * scale)`
- `guess = 1` means the child can return without exceeding the allowed loss
- reuse the current `thrext_*` semantics exactly: the implementation
  accumulates absolute edge costs, so it does not require child edges to be
  non-positive

Do not introduce a second scaling rule. The new backend must call
`thrext_build_child_info(...)` through the same path as `SumInfCachedBuilder`,
so fractional threshold behavior stays aligned with the live backend.

Do not add a new sign restriction for `SumMinus` unless the project explicitly
decides to narrow the accepted input language. The experimental backend should
match the current absolute-cost semantics.

## Implementation Shape

### 1. Expose Single-Obligation Stepping For Sum

Minimal patch:

- keep `SumInfCachedBuilder` in place
- add a public `OblStep` and `step_obl_public(...)`, mirroring
  `MMSupCachedBuilder::step_obl_public(...)`
- use `spawn_code(child, 1, symbol)` for witness spawns

Example API shape:

```cpp
struct OblStep {
    bool ok = true;
    bool discharged = false;
    OblId next = OBL_DEAD;
};

OblStep step_obl_public(OblId id, uint32_t sym);
```

The sentinel handling should match `MMSupCachedBuilder::step_obl_public(...)`:

```cpp
OblStep step_obl_public(OblId id, uint32_t sym) {
    if (id == OBL_DEAD || id == OBL_UNKNOWN) {
        return OblStep{false, false, OBL_DEAD};
    }
    if (id == OBL_DISCHARGED) {
        return OblStep{true, true, OBL_DISCHARGED};
    }

    const OblId r = step_obl(id, sym);
    if (r == OBL_DEAD) {
        return OblStep{false, false, OBL_DEAD};
    }
    if (r == OBL_DISCHARGED) {
        return OblStep{true, true, OBL_DISCHARGED};
    }
    return OblStep{true, false, r};
}
```

Avoid renaming `SumInfCachedBuilder` in the first patch. It is really a cached
sum threshold-obligation interner, but a rename would create noise unrelated to
the behavioral change.

### 2. Add Experimental Flattener Entry Point

Add a private method:

```cpp
Automaton* flatten_SumPlusMinus_Sup_witness_cached(value_function_t finite_aggregator,
                                                   weight_t threshold);
```

Add a static implementation with the same structure as the Min/Max
witness-cached implementation:

```cpp
static Automaton* flatten_SumPlusMinus_Sup_witness_cached_impl(
    NestedAutomaton* A,
    value_function_t finite_aggregator,
    weight_t threshold);
```

Initial dispatch should not replace production. Keep the method callable from
tests through `NestedAutomatonTester`.

Code-order requirement:

- `SumInfCachedBuilder` is currently defined after
  `flatten_MinMax_Sup_witness_cached_impl(...)`
- place `flatten_SumPlusMinus_Sup_witness_cached_impl(...)` after the full
  `SumInfCachedBuilder` definition, or move `SumInfCachedBuilder` earlier
- a forward declaration is not enough if the implementation constructs the
  builder and calls its methods

### 3. Clone The Proven Sup Witness-Cached Skeleton

The new implementation should follow
`flatten_MinMax_Sup_witness_cached_impl(...)` closely.

Use:

```cpp
TermCachedBuilder term(A);
SumInfCachedBuilder witness(A, finite_aggregator, threshold);
```

Use the same state key shape:

```cpp
struct Key {
    uint32_t parent = 0;
    TermCachedBuilder::BagId B1 = 0u;
    TermCachedBuilder::BagId B2 = 0u;
    SumInfCachedBuilder::OblId W1 = NO_WITNESS;
    SumInfCachedBuilder::OblId W2 = NO_WITNESS;
    acc_phase_t phase = ACC_WAIT_parent;
    bool epoch_nonempty = false;
};
```

Preserve these invariants:

- `B1` contains background obligations for the next epoch
- `B2` contains background obligations currently being waited on
- `W1` is the possible witness obligation for the next epoch
- `W2` is the possible witness obligation currently being waited on
- at most one of `W1` and `W2` is active at a time
- a state is final iff `phase == ACC_WAIT_P2EMPTY`, `B2` is empty, `W2` is
  inactive, and `epoch_nonempty` is true

Use the same phase transition logic:

```cpp
prev_empty_src = (B2 == 0 && W2 == NO_WITNESS);
front_nonempty_src = (B1 != 0 || W1 != NO_WITNESS);

phase_after_current =
    phase == ACC_WAIT_parent
        ? (parent_is_final ? ACC_WAIT_P2EMPTY : ACC_WAIT_parent)
        : (prev_empty_src ? ACC_WAIT_parent : ACC_WAIT_P2EMPTY);

reset_epoch = (phase == ACC_WAIT_P2EMPTY && prev_empty_src);
```

This phase memory is required for the same issue-5 class already fixed in the
Min/Max witness-cached backend.

Also copy the full epoch-nonempty update from the Min/Max skeleton:

```cpp
bool epoch_nonempty_to =
    reset_epoch ? front_nonempty_src : current.epoch_nonempty;

if (B2res.any_discharged || W2res.discharged) {
    epoch_nonempty_to = true;
}
if (!is_silent) {
    epoch_nonempty_to = true;
}
```

The `front_nonempty_src` case is not optional. Without it, an epoch whose only
next obligations live in `W1/B1` at reset time can be misclassified as empty.

### 4. Successor Generation

For each dequeued key and symbol:

1. Step `B1` with `term.step_bag(...)`; reject if dead.
2. Step `B2` if non-empty; reject if dead.
3. Step `W1` and `W2` with the new sum `step_obl_public(...)`; reject if dead.
4. Compute `prev_empty_src`, `front_nonempty_src`, and the promoted
   `B1_base/B2_base/W1_base/W2_base` values.
5. Iterate parent successors on the symbol.
6. If the parent edge is silent:
   - carry promoted bags/witnesses through
   - emit a `SILENT` edge
7. If the parent edge is a real child call:
   - background branch:
     - use `term.spawn_code(child, symbol)`
     - add it to `B1` unless it is already discharged
     - emit weight `0`
   - witness branch:
     - allowed only when neither `W1_base` nor `W2_base` is active after
       source-empty promotion
     - use `witness.spawn_code(child, 1, symbol)`
     - if it is non-empty, put it in `W1`
     - if it is immediately discharged, leave `W1/W2` inactive
     - emit weight `1`

Keep the current Min/Max rule that a witness-spawn branch does not also add a
background obligation for the same child call. The witness itself participates
in epoch emptiness.

The promotion step must happen before branching on parent edges. The base values
for each parent edge are:

```cpp
TermCachedBuilder::BagId B1_base = 0u;
TermCachedBuilder::BagId B2_base = 0u;
SumInfCachedBuilder::OblId W1_base = NO_WITNESS;
SumInfCachedBuilder::OblId W2_base = NO_WITNESS;

if (prev_empty_src) {
    B1_base = 0u;
    B2_base = B1res.next;
    W1_base = NO_WITNESS;
    W2_base = W1res.next;
} else {
    B1_base = B1res.next;
    B2_base = B2res.next;
    W1_base = W1res.next;
    W2_base = W2res.next;
}
```

Then compute `is_silent` for the concrete parent edge and apply the
`epoch_nonempty_to` formula above. This ordering is part of the correctness
argument, not just an implementation detail.

### 5. Silent Handling

The new flattener should emit `SILENT` on silent parent transitions, just like
the current threshold-extremal backend and the Min/Max witness-cached backend.

Every direct correctness check must remove silent transitions before applying
the outer `Sup` or `LimSup` emptiness check:

```cpp
Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
```

Do not compare raw flattened automata with `isNonEmpty_withFinal(...)` unless
the harness is intentionally testing pre-silent-removal behavior.

## Files To Change

Implementation:

- `src/NestedAutomaton.cpp`
  - add `SumInfCachedBuilder::step_obl_public(...)`
  - add `flatten_SumPlusMinus_Sup_witness_cached_impl(...)`
  - add `NestedAutomaton::flatten_SumPlusMinus_Sup_witness_cached(...)`
- `src/NestedAutomaton.h`
  - declare the private method
- `src/tests/sanity_tests/test_common.h`
  - add a tester wrapper

Verification harnesses:

- add `src/tests/probes/sum_sup_fix_compare.cpp`
- add CMake targets:
  - `sum_sup_fix_compare`
  - `sum_sup_fix_compare_witness_cached`

Optional benchmark harness:

- add `src/tests/sum_sup_resource_probe.cpp` only after correctness is green

## Correctness Harness

Model `src/tests/probes/sum_sup_fix_compare.cpp` on
`src/tests/probes/minmax_sup_fix_compare.cpp`.

Modes:

- default: current `flatten_SumPlusMinus_Sup(...)`
- `SUM_SUP_USE_WITNESS_CACHED=1`: new witness-cached flattener

For each query:

1. Build the specialized flat automaton.
2. Remove silent transitions.
3. Check `isNonEmpty_withFinal(infVal, 1)`.
4. Compare the boolean result against the current backend.
5. Compare against the regular oracle where trusted.

For correctness, do not require flattened state or transition counts to match
the current threshold-extremal backend. The new witness-cached construction is
supposed to use a different state representation. Log sizes for benchmarking
and regression triage, but treat only result mismatches as correctness failures.

Primary query dimensions:

- `infVal in {Sup, LimSup}`
- `finVal in {SumPlus, SumMinus}`
- `SumPlus` thresholds:
  - `0`, `0.5`, `1`, `1.5`, `2`, `3`, `4`, `5`, `8`, `10`
- `SumMinus` thresholds:
  - `0`, `-0.5`, `-1`, `-1.5`, `-2`, `-3`, `-5`, `-8`, `-10`

Fixture set:

- `baseline_det.txt`
- `baseline_fractional.txt`
- `deep_nondet_binary.txt`
- `positive_only_nondet.txt`
- `child_pump_loop.txt`
- `epsilon_boundary.txt`
- `sup_initial_final_child.txt`
- `split_witness_issue5_phase_same_step_control.txt`
- `split_witness_issue5_phase_async_idle.txt`
- `split_witness_issue5_phase_async_active_false_negative.txt`
- `split_witness_issue2_limsup_false_positive.txt`
- `phase_parent_final_then_empty.txt`
- `threshold_extremal_sumplus_wrong_final_positive.txt`
- `baseline_det_neg.txt`
- `baseline_fractional_neg.txt`
- `child_pump_loop_neg.txt`
- `epsilon_boundary_neg.txt`
- `phase_parent_final_then_empty_summinus.txt`
- `threshold_extremal_summinus_wrong_final_low_guess.txt`

Oracle rules:

- current threshold-extremal backend is the first regression oracle for the new
  witness-cached backend, but not the only semantic oracle
- regular `SumB` oracle should be used on trusted `SumPlus` cases, following
  `analysis/mainrepo_sumplus_smoke_probe.cpp`
- for `SumMinus`, first require agreement with the current threshold backend;
  add a separate regular-oracle path only for cases where the existing SumMinus
  reduction is already known to be semantically aligned
- add hand-checked fixtures when neither regular `SumB` nor the current
  threshold backend gives independent confidence

Additional targeted edge cases:

- `W1` is the only nonempty next-epoch obligation when an epoch resets
- a witness discharges before it is promoted into `W2`
- `W2` discharges on the same symbol where a new witness is spawned
- no non-silent parent transitions occur after a finite prefix
- `SumMinus` with mixed-sign child edges, to confirm absolute-cost semantics
- direct flattener calls for trivial thresholds:
  - `SumPlus` with `x <= 0`
  - `SumMinus` with `x > 0`

## Detailed Handcrafted W1/W2 Fixture Plan

Purpose:

- isolate the correctness risks that are specific to the split witness slots
  `W1/W2`
- make each fixture small enough that the intended accepting or rejecting run
  can be checked by inspection
- test the witness-cached backend directly, not only through the broad
  comparison matrix
- keep the current threshold-extremal backend as a regression oracle, but also
  record hand expectations for every targeted fixture

Test artifacts to add:

- new fixtures under `src/tests/correctness_tests/inputs/`
- a small targeted test source, preferably
  `src/tests/correctness_tests/test_sum_sup_witness_edge_cases.cpp`
- CMake registration for the new test so `make tests` and `ctest` pick it up
- optional inclusion of the same fixtures in `sum_sup_fix_compare.cpp` after
  the hand expectations are stable

Harness shape:

1. For each fixture, build a fresh `NestedAutomaton`.
2. Run both:
   - current `flatten_SumPlusMinus_Sup(...)`
   - experimental `flatten_SumPlusMinus_Sup_witness_cached(...)`
3. Remove silent transitions from each flat automaton before the outer query.
4. Query with `isNonEmpty_withFinal(infVal, weight_t(1))`.
5. Assert:
   - witness-cached result equals the hand expectation
   - current backend result equals witness-cached result unless the fixture is
     intentionally documenting a known current-backend bug
   - trusted `SumPlus` fixtures also agree with the regular `SumB` oracle where
     the finite-threshold reduction is straightforward
6. Print state and transition counts only as diagnostics. They must not be part
   of the pass/fail condition.

Implementation helper:

```cpp
static bool eval_sum_sup_flat(NestedAutomaton* nwa,
                              bool witness_cached,
                              value_function_t infVal,
                              value_function_t finVal,
                              weight_t threshold) {
    Automaton* flat = witness_cached
        ? NestedAutomatonTester::flatten_SumPlusMinus_Sup_witness_cached(nwa, finVal, threshold)
        : NestedAutomatonTester::flatten_SumPlusMinus_Sup(nwa, finVal, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}
```

Fixture design rules:

- use two or three parent states unless the case genuinely needs more
- use one real child for the selected witness and, where needed, one separate
  background child
- make parent weights explicit:
  - `0` for silent/dummy calls
  - `1` for the witness candidate child
  - `2` for a background-only blocker child
- keep alphabets tiny, typically `a`, `b`, and `c`
- make the parent final-state pattern part of the fixture comment
- document the intended accepting word or rejecting reason in comments at the
  top of each input file
- prefer threshold `1` for `SumPlus` and thresholds `0`, `-1`, or `-2` for
  `SumMinus`, unless the case needs fractional behavior

Concrete W1/W2-first implementation scope:

- do this before adding the broader background, no-nonsilent, `SumMinus`, and
  trivial-threshold fixtures
- add exactly three `SumPlus` fixtures that exercise split witness slot
  ordering:
  - W1 discharges during epoch reset
  - witness spawn discharges immediately and must not leave stale W1/W2 state
  - W2 discharges on the same symbol where a new witness must be spawnable
- keep the broad `sum_sup_fix_compare.cpp` matrix unchanged until the dedicated
  hand test is green

Repo-state note:

- `flatten_SumPlusMinus_Sup_witness_cached(...)` and the
  `sum_sup_fix_compare_witness_cached*` targets already exist in this checkout
- `src/tests/sanity_tests/test_common.h` already has the witness-cached tester
  wrapper
- `src/tests/correctness_tests/test_correctness_common.h` still needs the same
  wrapper before the dedicated correctness test can use the private flattener

Concrete files for the W1/W2-first patch:

- add fixture constants to `CorrectnessTestFiles`:
  - `SUM_SUP_W1_DISCHARGE_ON_RESET`
  - `SUM_SUP_WITNESS_IMMEDIATE_DISCHARGE`
  - `SUM_SUP_W2_DISCHARGE_AND_RESPAWN`
- add a correctness helper wrapper:

  ```cpp
  static Automaton* flatten_SumPlusMinus_Sup_witness_cached(
      NestedAutomaton* nwa,
      value_function_t finite_aggregator,
      weight_t threshold) {
      return nwa->flatten_SumPlusMinus_Sup_witness_cached(finite_aggregator,
                                                          threshold);
  }
  ```

- add `src/tests/correctness_tests/test_sum_sup_witness_edge_cases.cpp`
- register it in `CORRECTNESS_TEST_SOURCES` and in the `tests` custom target in
  `CMakeLists.txt`
- after the dedicated test passes, add the three fixtures to
  `src/tests/probes/sum_sup_fix_compare.cpp` with `run_sumplus=true`,
  `run_summinus=false`, and `trusted_sumplus_oracle=true`

### Fixture 1: W1 Discharges During Epoch Reset

File:

- `src/tests/correctness_tests/inputs/sum_sup_w1_discharge_on_reset.txt`

Purpose:

- directly catch a missing `front_nonempty_src` latch at `reset_epoch`
- the critical transition is silent and discharges `W1` before it can become
  `W2`, so no `W2res.discharged` event exists to recover the epoch credit

Intended word:

- `(c a b)^omega`

Expected hand result:

- `Sup x SumPlus @ 1`: true
- `LimSup x SumPlus @ 1`: true
- current threshold-extremal, witness-cached, and regular `SumB` oracle should
  agree

Exact fixture body:

```text
# W1 reset fixture for SumPlus witness-cached Sup/LimSup.
# Intended word: (c a b)^omega.
#
# c spawns Child 1 as W1 while source p0 is final, so the destination has
# phase ACC_WAIT_P2EMPTY, empty B2/W2, and active W1.
# a is silent and discharges that W1 during reset before it can become W2.
# b sees parent-final p2 with no active obligations and closes the accepting
# epoch. A missing front_nonempty_src reset latch can falsely reject.

@PARENT
final: p0 p2
c : 1, p0 -> p1
a : 0, p1 -> p2
b : 0, p2 -> p0

@CHILD 0

@CHILD 1
final: f
c : 0, s0 -> s1
a : 0, s0 -> rej
b : 0, s0 -> rej

a : 1, s1 -> f
b : 0, s1 -> s1
c : 0, s1 -> s1

a : 0, rej -> rej
b : 0, rej -> rej
c : 0, rej -> rej
```

Test assertions:

- evaluate both current and witness-cached threshold flatteners after silent
  removal for `Sup` and `LimSup`
- assert both are true
- assert both equal the regular `SumB` oracle at threshold `1`

### Fixture 2: Witness Spawn Discharges Immediately

File:

- `src/tests/correctness_tests/inputs/sum_sup_witness_immediate_discharge.txt`

Purpose:

- verify that `witness.spawn_code(child, 1, symbol) == OBL_DISCHARGED` leaves
  both `W1` and `W2` inactive
- catch stale-slot bugs where an already discharged spawn is later promoted
  into `W2` and blocks acceptance

Intended word:

- `a^omega`

Expected hand result:

- `Sup x SumPlus @ 1`: true
- `LimSup x SumPlus @ 1`: true
- current threshold-extremal, witness-cached, and regular `SumB` oracle should
  agree

Exact fixture body:

```text
# Immediate witness-discharge fixture for SumPlus witness-cached Sup/LimSup.
# Intended word: a^omega.
#
# Each witness spawn reaches Child 1's final state on the spawn symbol itself.
# The witness branch should emit weight 1 and keep W1/W2 inactive.

@PARENT
final: p
a : 1, p -> p

@CHILD 0

@CHILD 1
final: f
a : 1, s0 -> f
```

Test assertions:

- evaluate current and witness-cached threshold flatteners for `Sup` and
  `LimSup`; assert both are true
- assert both equal the regular `SumB` oracle at threshold `1`
- additionally inspect the raw witness-cached flat automaton's initial
  `a`-successors and assert a weight-`1` edge exists

### Fixture 3: W2 Discharges And New Witness Spawns On Same Symbol

File:

- `src/tests/correctness_tests/inputs/sum_sup_w2_discharge_and_respawn.txt`

Purpose:

- enforce the ordering "step old `W2`, then decide whether a new witness may be
  spawned"
- a plain nonemptiness assertion is not strong enough here because a buggy
  implementation may recover on a later call; add a structural raw-flat check
  for the specific prefix that reaches active `W2`

Critical prefix:

- `c d` reaches a flattened state where:
  - the witness spawned on `c` has been promoted into `W2`
  - `W2` is at child state `s2`
  - the parent is at `p2`
- on the next `a`:
  - old `W2` steps `s2 -> f` and discharges
  - the parent calls Child 1 again
  - a correct backend exposes a weight-`1` `a`-edge for the fresh witness
    spawn, with that fresh witness in `W1`

Intended word:

- `c d (a d)^omega`

Expected hand result:

- `Sup x SumPlus @ 1`: true
- `LimSup x SumPlus @ 1`: true
- current threshold-extremal, witness-cached, and regular `SumB` oracle should
  agree
- raw witness-cached structural check after prefix `c:1, d:*` must find an
  outgoing `a` edge with weight `1`

Exact fixture body:

```text
# W2 discharge plus same-symbol respawn fixture for SumPlus witness-cached
# Sup/LimSup.
# Intended word: c d (a d)^omega.
#
# Prefix c d creates active W2 at child state s2. On a, that W2 discharges
# while the parent simultaneously calls Child 1 again. The fresh call starts at
# s0 and moves to s1, so the witness-cached flattener must allow a new W1 on
# the same symbol.

@PARENT
final: p2
c : 1, p0 -> p1
d : 0, p1 -> p2
a : 1, p2 -> p2
d : 0, p2 -> p2

@CHILD 0

@CHILD 1
final: f
c : 0, s0 -> s1
a : 0, s0 -> s1
d : 0, s0 -> rej

d : 0, s1 -> s2
a : 0, s1 -> rej
c : 0, s1 -> rej

a : 1, s2 -> f
d : 0, s2 -> s2
c : 0, s2 -> rej

a : 0, rej -> rej
c : 0, rej -> rej
d : 0, rej -> rej
```

Structural helper for fixture 3:

```cpp
static std::vector<State*> step_states(Automaton* flat,
                                       const std::vector<State*>& from,
                                       const std::string& symbol,
                                       const weight_t* required_weight);

static bool has_successor_weight(Automaton* flat,
                                 const std::vector<State*>& from,
                                 const std::string& symbol,
                                 weight_t required_weight);
```

Use it as:

```cpp
Automaton* flat = NestedAutomatonTester::
    flatten_SumPlusMinus_Sup_witness_cached(&nwa, SumPlus, weight_t(1));
std::vector<State*> frontier{flat->getInitial()};
const weight_t one(1);
frontier = step_states(flat, frontier, "c", &one);
frontier = step_states(flat, frontier, "d", nullptr);
TEST_ASSERT_TRUE(has_successor_weight(flat, frontier, "a", weight_t(1)),
                 "active W2 discharge should permit same-symbol witness respawn");
delete flat;
```

The `d` step intentionally does not require a concrete emitted weight because
the raw flattener emits `SILENT` for silent parent transitions. The `c` and `a`
steps do require weight `1` so the check follows the witness branch, not the
background branch.

### Dedicated Test Source Shape

Keep the new test source small and local to these fixtures:

```cpp
using ThresholdFlattenFn =
    Automaton* (*)(NestedAutomaton*, value_function_t, weight_t);

static bool eval_threshold(NestedAutomaton* nwa,
                           ThresholdFlattenFn flatten,
                           value_function_t infVal,
                           weight_t threshold) {
    Automaton* flat = flatten(nwa, SumPlus, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}

static bool eval_regular_sumplus_oracle(NestedAutomaton* nwa,
                                        value_function_t infVal,
                                        weight_t threshold) {
    Automaton* flat = nwa->flatten_regular(SumB, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);
    delete non_silent;
    delete flat;
    return result;
}
```

For each fixture and each `infVal in {Sup, LimSup}`:

1. parse a fresh `NestedAutomaton`
2. assert current threshold-extremal result is true
3. assert witness-cached result is true
4. assert current and witness-cached results match
5. assert witness-cached result matches the regular `SumB` oracle

The test source should have three named test functions:

- `test_w1_discharge_on_reset_keeps_epoch_nonempty()`
- `test_immediate_witness_discharge_leaves_no_stale_slot()`
- `test_w2_discharge_allows_same_symbol_respawn()`

Only the third test needs the raw-flat structural prefix check.

Target fixtures:

1. `sum_sup_w1_discharge_on_reset.txt`

   Goal: catch a missing `front_nonempty_src` update.

   Shape:

   - spawn a witness into `W1`
   - reach a source state with `phase == ACC_WAIT_P2EMPTY`, empty `B2/W2`, and
     active `W1`
   - take a silent parent transition where the witness discharges while still in
     `W1`
   - return to an accepting parent cycle with no `W2res.discharged` event that
     could compensate for a missing reset latch

   Expected:

   - `LimSup x SumPlus` at threshold `1` is nonempty
   - a buggy implementation that resets `epoch_nonempty` to false without
     considering active `W1/B1` can reject this case

2. `sum_sup_witness_immediate_discharge.txt`

   Goal: check that a witness which discharges while still in the front slot is
   handled as completed, not promoted into a stale active `W2`.

   Shape:

   - spawn a witness whose first or second child step reaches a final state with
     enough `SumPlus` gain
   - arrange the parent so the source `B2/W2` is empty when that discharge is
     observed
   - no separate background obligation is added for the same child call

   Expected:

   - `Sup x SumPlus` and `LimSup x SumPlus` at threshold `1` are nonempty
   - the witness slot is inactive after discharge, so the accepting cycle can
     close

3. `sum_sup_w2_discharge_and_respawn.txt`

   Goal: check the ordering "step old `W2`, then decide whether a new witness
   may be spawned".

   Shape:

   - enter a state with active `W2`
   - on the next symbol, the active `W2` discharges
   - the same parent transition calls a fresh child that should be selectable
     as the next witness
   - the loop repeats

   Expected:

   - `LimSup x SumPlus` at threshold `1` is nonempty
   - an implementation that tests witness availability before stepping `W2`
     can falsely reject

4. `sum_sup_background_blocks_acceptance.txt`

   Goal: ensure non-witness children still block epoch completion until they
   terminate.

   Shape:

   - each loop has one successful witness candidate and one background child
   - the background child is forced onto a nonterminating path, so every
     accepting-looking parent cycle still carries an undischargeable
     background obligation
   - the parent otherwise has an accepting final cycle

   Expected:

   - the accepting result is false
   - a buggy implementation that only tracks the distinguished witness and
     forgets background termination can falsely accept

5. `sum_sup_no_nonsilent_after_prefix.txt`

   Goal: check that a finite prefix with a successful witness is not enough when
   the parent has only silent behavior afterward.

   Shape:

   - one real child call can satisfy the threshold
   - after that, the parent moves into a final state with only silent
     self-loops

   Expected:

   - the post-silent-removal decision rejects because there is no infinite
     non-silent accepting behavior
   - this should be checked for `Sup` and `LimSup`

6. `sum_sup_summinus_mixed_sign_abs_cost.txt`

   Goal: lock in the current `SumMinus` absolute-cost semantics.

   Shape:

   - child has mixed-sign edge weights, for example `+1` followed by `-2`
   - accumulated absolute loss is `3`
   - parent can repeat the same child as a witness

   Expected:

   - `SumMinus` threshold `-3` accepts
   - `SumMinus` threshold `-2` rejects
   - both current and witness-cached backends agree

7. `sum_sup_trivial_threshold_direct.txt`

   Goal: separate direct flattener behavior from the public `isNonEmpty(...)`
   short-circuits.

   Shape:

   - reuse the smallest deterministic fixture or add a tiny one-state fixture
   - call the flattener directly through the tester wrapper

   Expected:

   - `SumPlus` with threshold `0` and a non-silent accepting parent cycle should
     behave consistently after silent removal
   - `SumMinus` with positive threshold should not accidentally produce a
     witness success path that contradicts the public short-circuit

Execution order:

1. Add the first three fixtures only:
   - `sum_sup_w1_discharge_on_reset.txt`
   - `sum_sup_witness_immediate_discharge.txt`
   - `sum_sup_w2_discharge_and_respawn.txt`
2. Add `test_sum_sup_witness_edge_cases.cpp` with hard-coded expectations.
3. Run the targeted test and `sum_sup_fix_compare_witness_cached`.
4. Add the background, no-nonsilent, `SumMinus`, and trivial-threshold cases.
5. Re-run the targeted test, the quick oracle-backed harness, and the bounded
   full comparison harness.

Commands:

```bash
cmake --build build --target \
  test_sum_sup_witness_edge_cases \
  sum_sup_fix_compare \
  sum_sup_fix_compare_witness_cached \
  sum_sup_fix_compare_witness_cached_full -j

./build/test_sum_sup_witness_edge_cases
./build/sum_sup_fix_compare
./build/sum_sup_fix_compare_witness_cached
./build/sum_sup_fix_compare_witness_cached_full
```

Success criteria:

- every hand fixture has a comment with its intended accepting word or rejecting
  reason
- the targeted test has no result mismatches
- quick oracle-backed checks still report `mismatches=0`
- the bounded full witness comparison still reports `mismatches=0`
- any fixture that exposes a current-backend bug is documented separately and
  is not silently folded into the regression oracle

## Detailed ASan/UBSan Validation Plan

Purpose:

- catch memory errors and undefined behavior in the new witness-cached path
- stress the sentinel-heavy logic around `OBL_DEAD`, `OBL_DISCHARGED`,
  `OBL_UNKNOWN`, and `NO_WITNESS`
- validate both the new targeted fixtures and the broader comparison harnesses
  under sanitizer instrumentation

Use a fresh sanitizer build directory instead of relying on the existing
`build-asan` or `build-sum-asan` directories. Those directories exist, but they
may have been configured before the newest targets were added.

Configure:

```bash
cmake -S . -B build-sum-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
```

Build the targeted sanitizer set:

```bash
cmake --build build-sum-sanitize --target \
  test_flatten_sumplusminus_sup \
  test_emptiness_correctness \
  test_universality_correctness \
  sum_sup_fix_compare \
  sum_sup_fix_compare_witness_cached \
  sum_sup_fix_compare_witness_cached_full -j
```

After the handcrafted fixture test is added, include it in the build command:

```bash
cmake --build build-sum-sanitize --target test_sum_sup_witness_edge_cases -j
```

Runtime environment:

```bash
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:strict_string_checks=1
export UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1
```

If leak detection reports broad pre-existing ownership leaks unrelated to this
backend, keep the report and run a second targeted pass with:

```bash
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1:strict_string_checks=1
```

Do not disable address checks or UBSan checks to make the run pass.

Run order:

1. Build-only gate:

   ```bash
   cmake --build build-sum-sanitize --target sum_sup_fix_compare_witness_cached -j
   ```

2. Small direct-flattener smoke:

   ```bash
   ./build-sum-sanitize/test_flatten_sumplusminus_sup
   ```

3. New targeted W1/W2 fixtures:

   ```bash
   ./build-sum-sanitize/test_sum_sup_witness_edge_cases
   ```

4. Quick current-vs-witness and oracle-backed checks:

   ```bash
   ./build-sum-sanitize/sum_sup_fix_compare
   ./build-sum-sanitize/sum_sup_fix_compare_witness_cached
   ```

5. Broader correctness tests:

   ```bash
   ./build-sum-sanitize/test_emptiness_correctness
   ./build-sum-sanitize/test_universality_correctness
   ```

6. Bounded full comparison:

   ```bash
   ./build-sum-sanitize/sum_sup_fix_compare_witness_cached_full
   ```

   This target already disables the expensive regular oracle in full mode and
   skips the known threshold-10 current-backend stress cells. Under sanitizers it
   may still be much slower, so keep the tracker output and kill only after the
   active query is known.

Timeout policy:

- use a generous per-command timeout for sanitizer runs, for example 5 to 10
  minutes for correctness binaries and 10 to 20 minutes for the bounded full
  comparison
- if a sanitizer run appears stuck, first inspect the last tracker line or
  active query
- split the run by target or fixture before assuming a sanitizer problem
- do not treat a performance timeout as a sanitizer pass

Logging:

- create a dated directory such as
  `analysis/sum_sup_witness_sanitize_2026-04-24/`
- save:
  - configure log
  - build log
  - one stdout/stderr log per executed binary
  - a short `summary.md` with command, status, runtime, and sanitizer finding
- for any sanitizer failure, include the exact reproduction command and the
  smallest fixture or harness query that triggers it

Triage rules:

- any ASan use-after-free, out-of-bounds access, double free, or invalid free in
  the witness-cached path is promotion-blocking
- any UBSan report involving sentinel arithmetic, invalid enum/state IDs,
  invalid shifts, or signed overflow is promotion-blocking
- if the same sanitizer report reproduces with the production current backend
  and not with the witness-cached-only path, document it as pre-existing and
  keep it separate from this backend's promotion gate
- if the report only appears in `sum_sup_fix_compare_witness_cached` or the new
  W1/W2 targeted test, fix it before running performance benchmarks again

Success criteria:

- sanitizer build completes without compile or link errors
- `test_flatten_sumplusminus_sup` passes with no sanitizer reports
- `test_sum_sup_witness_edge_cases` passes with no sanitizer reports after that
  test exists
- `sum_sup_fix_compare` and `sum_sup_fix_compare_witness_cached` pass with
  `mismatches=0` and no sanitizer reports
- `test_emptiness_correctness` and `test_universality_correctness` pass with no
  sanitizer reports, or any pre-existing sanitizer failures are isolated and
  documented
- the bounded full witness comparison either passes cleanly or any timeout is
  recorded as a performance limit, not a sanitizer success

## Promotion Criteria

Do not switch production dispatch until all of these pass:

```bash
cmake --build build --target \
  test_flatten_sumplusminus_sup \
  test_emptiness_correctness \
  test_universality_correctness \
  sum_sup_fix_compare \
  sum_sup_fix_compare_witness_cached -j

./build/test_flatten_sumplusminus_sup
./build/test_emptiness_correctness
./build/test_universality_correctness
./build/sum_sup_fix_compare
./build/sum_sup_fix_compare_witness_cached
ctest --test-dir build --output-on-failure
```

Promotion patch:

- change `NestedAutomaton::flatten_SumPlusMinus_Sup(...)` to return the new
  witness-cached implementation
- keep the old shared threshold-extremal implementation callable for at least
  one evaluation cycle, preferably through a tester wrapper or guarded dispatch
  path, so regressions are easy to bisect

## Benchmark Plan

Use benchmarks only after correctness is clean.

Compare:

- current threshold-extremal Sup backend
- new witness-cached Sup backend
- optionally regular `SumB` only for small oracle cases

Measure:

- wall time
- flattened state count
- flattened transition count
- number of cached obligations
- number of cached bags
- bag-step cache hits
- obligation-step cache hits

If these metrics are needed for the split Sup builder, add explicit stats
plumbing. The existing public stats path is oriented around the current
threshold/cached Inf experiments and may not expose both `TermCachedBuilder` and
`SumInfCachedBuilder` counters for the new combined backend.

Families:

- `samples/generated_response_time_3`
- `samples/generated_response_time_max`
- `samples/generated_response_time_max_1`
- negative response-time fixtures under `samples/nested/avg_resp_neg`
- targeted correctness fixtures listed above

Expected performance profile:

- tiny instances may be neutral or slightly slower due to interning overhead
- medium and large overlapping-call instances should improve by avoiding
  all-child threshold obligations and by caching repeated witness frontiers
- `LimSup` should benefit most when many failed/background calls coexist with
  a sparse stream of successful witnesses

## Risks And Checks

Risk: accepting a witness that never returns.

- Check: final-state condition must include `W2 == NO_WITNESS`.
- Check: phase promotion must move `W1` into `W2` exactly like the Min/Max
  implementation.

Risk: edge weight `1` at spawn is mistaken for unconditional success.

- Check: on every accepting run, each active witness must discharge with
  `guess = 1` before the epoch can complete.
- Check: compare against return-time semantics on small hand fixtures if a
  counterexample is suspected.

Risk: trivial thresholds behave differently when the flattener is called
directly.

- Check: include direct flattener tests for `SumPlus x <= 0` and
  `SumMinus x > 0`.
- Check: preserve existing `isNonEmpty(...)` trivial short-circuits.

Risk: SumMinus sign assumptions are implicit.

- Check: document and test the current absolute-cost semantics.
- Check: include a mixed-sign `SumMinus` fixture, or explicitly decide to
  reject it before implementation.
- Check: do not silently broaden semantics beyond the current
  threshold-extremal backend.

Risk: comparing raw flats without silent removal creates false mismatches.

- Check: every harness path removes silent transitions before the outer
  `Sup/LimSup` query.

Risk: class-name confusion around `SumInfCachedBuilder`.

- Check: keep the first patch minimal. Rename to a neutral name only after the
  backend is promoted and stable.

## Suggested Patch Order

1. Add `step_obl_public(...)` to `SumInfCachedBuilder`.
2. Add `flatten_SumPlusMinus_Sup_witness_cached_impl(...)` by cloning the
   Min/Max witness-cached skeleton and swapping in `SumInfCachedBuilder`.
   Place this implementation after the full `SumInfCachedBuilder` class unless
   the class is moved earlier.
3. Add header/tester wrappers and a compile-only target.
4. Add `sum_sup_fix_compare.cpp` and run current-vs-witness-cached comparisons.
5. Add or extend targeted correctness tests for any mismatch.
6. Add benchmark probe only after correctness is green.
7. Promote the backend by switching `flatten_SumPlusMinus_Sup(...)`.
8. Keep the comparison target around for at least one follow-up cleanup cycle.
