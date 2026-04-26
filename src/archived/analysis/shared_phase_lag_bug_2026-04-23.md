# Shared Phase-Lag Counterexample: Live Threshold-Extremal and Archived Split-Witness

Date: 2026-04-23

## Scope

This note tests the claim that the following pattern breaks **both**:

- the archived split-witness `Min_f/Max_f + Sup/LimSup` backend
- the current live shared threshold-extremal backend used by
  `flatten_MinMax_Sup(...)` and `flatten_MinMax_Inf(...)`

and now also records the result for:

- the current cached `flatten_MinMax_Inf_cached(...)` backend

The proposed counterexample is now saved as:

- `src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt`

The pattern is intentionally clean:

- one real child plus dummy child `0`
- deterministic child behavior on the witness word
- no same-child overlap-loss issue
- no incomplete-child dropping on the witness word
- no same-state multiplicity collapse

So this isolates a **phase/acceptance** problem rather than the earlier
background-propagation bug from archived split-witness `issue2`.

## Executive Summary

The claim is confirmed.

Observed on the new fixture at threshold `1`:

- `flatten_regular(...)` accepts for all of:
  - `Sup`, `LimSup`, `Inf`, `LimInf`
  - with both `Max_f` and `Min_f`
- the **current live** backend rejects all eight of those queries
- the **current cached `Inf/LimInf`** backend rejects all four `Inf/LimInf`
  queries for both `Max_f` and `Min_f`
- the **archived split-witness** backend rejects the `Sup/LimSup` queries for
  both `Max_f` and `Min_f`

So this is:

- a **new current live bug** in the shared threshold-extremal backend
- and simultaneously another **archived split-witness phase bug**

The two backends fail for the same high-level reason:

- they require "parent final" and "tracked epoch empties" to coincide too
  tightly
- instead of remembering a parent-final visit until the tracked obligations
  empty

## The Fixture

File:

- `src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt`

Intended semantics stated in the fixture header:

- the unique non-sink word is `(ab)^omega`
- every real child invocation returns `1`
- the parent visits final state `p2` on every `b`
- therefore the original nested automaton should satisfy:
  - `Sup`, `LimSup`, `Inf`, `LimInf`
  - for both `Max_f` and `Min_f`
  - all at threshold `1`

## Why This Is Not The Old Overlap-Loss Bug

This counterexample does **not** rely on:

- two active copies of the same child being collapsed into one background bit
- the one-state-per-child recursion bug from archived split-witness `issue2`
- source-vs-destination tracking ambiguity
- incomplete-child rejection on the witness word

Instead, it forces a clean alternation:

- a parent-final visit happens on one step
- the currently tracked obligations empty on the next step

The bug only appears if the backend forgets the earlier parent-final visit.

## Current Live Backend: Code Path

The active Min/Max routing in `src/NestedAutomaton.cpp` does use the live
shared threshold-extremal helper:

- `flatten_MinMax_Sup(...)` returns `flatten_threshold_extremal_impl(...)`
  at `src/NestedAutomaton.cpp:5933-5938`
- `flatten_MinMax_Inf(...)` also routes there
- `isNonEmpty(...)` dispatches `Min_f/Max_f × {Sup, LimSup, Inf, LimInf}`
  through those wrappers

The acceptance logic in the active helper is the weaker pulse-style scheme:

- state stores `parent_phase` and `epoch_nonempty`
- final states are marked only when
  - `gs.parent_phase == 2u`
  - `gs.epoch_nonempty`
  - `gs.parent_state->getFinal()`

See:

- `src/NestedAutomaton.cpp:4664-4669`
- `src/NestedAutomaton.cpp:4722-4755`
- `src/NestedAutomaton.cpp:4815-4820`
- `src/NestedAutomaton.cpp:4857-4859`

By contrast, the regular obligation backend uses the stronger remembered phase:

- `advance_phase(...)` at `src/NestedAutomaton.cpp:1515-1519`
- finals when `phase == ACC_WAIT_P2EMPTY && P2.empty()`
  at `src/NestedAutomaton.cpp:1760`

Notably, the separate file `src/NestedAutomaton_threshold_extremal.cpp`
contains that stronger remembered-phase logic for threshold-extremal too:

- `advance_phase_thrext(...)` at `src/NestedAutomaton_threshold_extremal.cpp:3517-3521`
- finals at `src/NestedAutomaton_threshold_extremal.cpp:4242`

But the current build in this checkout uses the active code in
`src/NestedAutomaton.cpp`, not that separate file.

## Commands Run

All probes below were compiled against the **current** `build-review`
libraries, not stale binaries.

### Live current-build probe

Compiled against:

- `build-review/src/libquak.a`
- `build-review/src/libquak-private.a`

Probe source:

- `analysis/tmp_phase_live_query.cpp`

Run:

```bash
/tmp/phase_live_query \
  src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt
```

### Live regular-oracle probe

Compiled against:

- `build-review/src/libquak.a`
- `build-review/src/libquak-private.a`

Probe source:

- `analysis/tmp_phase_regular_query.cpp`

Run:

```bash
/tmp/phase_regular_query \
  src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt
```

### Current old-v2 split-witness probe

Compiled against:

- `build-review/src/libquak_old_v2.a`
- `build-review/src/libquak-private.a`

Probe source:

- `analysis/tmp_phase_split_compare.cpp`

Run:

```bash
/tmp/phase_split_compare \
  src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt
```

### Current cached `Inf/LimInf` probe

Compiled against:

- `build-review/src/libquak.a`
- `build-review/src/libquak-private.a`

Probe source:

- `analysis/tmp_phase_cached_inf_query.cpp`

Run:

```bash
/tmp/phase_cached_inf_query \
  src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt
```

## Observed Outputs

### Current live backend

```text
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Sup fin=Max_f threshold=1 live=0
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Sup fin=Min_f threshold=1 live=0
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimSup fin=Max_f threshold=1 live=0
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimSup fin=Min_f threshold=1 live=0
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Inf fin=Max_f threshold=1 live=0
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Inf fin=Min_f threshold=1 live=0
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimInf fin=Max_f threshold=1 live=0
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimInf fin=Min_f threshold=1 live=0
```

### Current live `flatten_regular(...)` oracle

```text
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Sup fin=Max_f threshold=1 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Sup fin=Min_f threshold=1 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimSup fin=Max_f threshold=1 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimSup fin=Min_f threshold=1 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Inf fin=Max_f threshold=1 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Inf fin=Min_f threshold=1 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimInf fin=Max_f threshold=1 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimInf fin=Min_f threshold=1 regular=1
```

### Current old-v2 split-witness vs regular

```text
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Sup fin=Max_f threshold=1 split=0 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Sup fin=Min_f threshold=1 split=0 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimSup fin=Max_f threshold=1 split=0 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimSup fin=Min_f threshold=1 split=0 regular=1
```

### Current cached `Inf/LimInf` vs regular

```text
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Inf fin=Max_f threshold=1 cached=0 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Inf fin=Min_f threshold=1 cached=0 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimInf fin=Max_f threshold=1 cached=0 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimInf fin=Min_f threshold=1 cached=0 regular=1
```

## Interpretation

### Archived split-witness

The archived split-witness backend does get this fixture wrong.

This is **not** the old overlap-loss bug from `issue2`. It is another
phase-memory failure in the same broad family as archived `issue5`:

- parent-final and epoch-empty happen on different steps
- the backend does not preserve enough acceptance history across that lag

### Current live threshold-extremal backend

The current live backend also gets this fixture wrong.

The regular oracle accepts all eight queries, while the current shared
threshold-extremal path rejects all eight. On this fixture, that is strong
evidence that the bug is in the live threshold-extremal acceptance layer, not
in the underlying nested semantics.

The current active implementation in `src/NestedAutomaton.cpp` uses:

- `parent_phase`
- `epoch_nonempty`
- a final-state condition requiring the pulse state itself to have a final
  parent

That is weaker than the remembered two-phase discipline used by
`flatten_regular(...)`.

### Current cached `Inf/LimInf` backend

The current cached `flatten_MinMax_Inf_cached(...)` backend also gets this
fixture wrong.

Its implementation in `src/NestedAutomaton.cpp` uses the same acceptance shape
as the failing active threshold-extremal path:

- the cached state key stores `phase` and `epoch_nonempty`
  at `src/NestedAutomaton.cpp:9369-9379`
- transitions update them with the same pulse-style logic
  at `src/NestedAutomaton.cpp:9445-9509`
- finals are marked only when
  `key.phase == 2u && key.epoch_nonempty != 0u && parent_final`
  at `src/NestedAutomaton.cpp:9526-9528`

So the cached backend is not an escape hatch for this input; it reproduces the
same phase-lag rejection.

## Classification

This issue should be tracked as:

- **archived split-witness**: another real phase-only false negative
- **current live backend**: a new correctness bug in the active shared
  threshold-extremal implementation
- **current cached `Inf/LimInf` backend**: the same phase-lag correctness bug
  in the cached implementation

So the answer to "is this about both?" is yes.

## Consequence

The current shared threshold-extremal backend is not semantically equivalent to
the regular obligation backend on this pattern.

The likely minimal repair direction is exactly the one suggested in the claim:

- replace the weaker pulse-style acceptance bookkeeping in the active
  threshold-extremal path with the same remembered two-phase discipline used by
  `flatten_regular(...)`

That means the backend must remember:

- "a parent-final visit has already happened for the current acceptance cycle"

until the tracked obligations empty, instead of requiring both to coincide in
the same pulse state.
