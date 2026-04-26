# Main-Repo `Sup x Max_f` Issues Against Live Threshold-Extremal

Date: 2026-04-23

## Scope

This note describes the concrete correctness issues exposed when comparing:

- the copied main-repo implementation in
  [analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp](./NestedAutomaton_mainRepo_sup_max_compare.cpp)
- the live implementation in
  [src/NestedAutomaton.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:5928)

for:

- `isNonEmpty(Sup, Max_f, threshold)`

The live implementation routes `flatten_MinMax_Sup(...)` to the shared
threshold-extremal backend:

- [src/NestedAutomaton.cpp:5928-5933](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:5928)

The main-repo copy still uses the older single-witness explicit flattening:

- [analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp:4683-5189](./NestedAutomaton_mainRepo_sup_max_compare.cpp#L4683)

This note is about the main-repo behavior only. The live threshold-extremal
path is used as the reference implementation in this comparison.

## Differential Result

Using:

- [analysis/sup_max_nonempty_matrix.cpp](./sup_max_nonempty_matrix.cpp)

and running the same `372` queries against both implementations, the result was:

- `main-repo copy`: `372` queries
- `live current`: `372` queries
- `mismatches`: `20`

All `20` mismatches have the same polarity:

- main-repo copy returns `0`
- live threshold-extremal returns `1`

The mismatching queries are:

- `sup_initial_final_child`
  - thresholds `{-1, 0, 0.5, 1}`
- `sup_initial_final_child_min_bad_current_symbol`
  - thresholds `{-1, 0}`
- `max_merge_bug_complete`
  - thresholds `{-1, 0, 0.5, 1, 1.5, 2}`
- `split_witness_issue5_phase_same_step_control`
  - thresholds `{-1, 0, 0.5, 1}`
- `split_witness_issue5_phase_async_active_false_negative`
  - thresholds `{-1, 0, 0.5, 1}`

There were no mismatches in the opposite direction.

## High-Level Diagnosis

The `20` mismatches come from three distinct problems in the main-repo
`Sup/ LimSup x Min_f/Max_f` flattener.

1. Freshly spawned initial-final children are treated as if they already
   existed before the current parent symbol.
2. A tracked child finishing on the current symbol does not count as an epoch
   completion for acceptance on that same step.
3. There is no persisted acceptance-phase state, so acceptance credit is lost
   if completion and parent-final happen on different later steps.

All three issues are visible in the main-repo explicit flattener’s state model.

The encoded state stores only:

- parent state id
- one activation bit per flattened child state
- one tracking bit per flattened child state
- either `@inactive@` or one distinguished witness triple

See the destination encoding in:

- [analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp:4809-4827](./NestedAutomaton_mainRepo_sup_max_compare.cpp#L4809)

There is no explicit acceptance phase and no “fresh spawn on this symbol” bit.

## Issue 1: Fresh Spawn Does Not Consume The Spawning Symbol

### Affected mismatches

- `sup_initial_final_child`
- `sup_initial_final_child_min_bad_current_symbol`

### Intended semantics

The focused regression tests in the repo already state the intended behavior:

- [src/tests/correctness_tests/test_emptiness_correctness.cpp:1413-1426](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/correctness_tests/test_emptiness_correctness.cpp:1413)
- [src/tests/correctness_tests/test_emptiness_correctness.cpp:1433-1448](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/correctness_tests/test_emptiness_correctness.cpp:1433)

Those tests require:

- `sup_initial_final_child`
  - accept threshold `1`
  - reject threshold `2`
- `sup_initial_final_child_min_bad_current_symbol`
  - accept threshold `0`
  - reject threshold `1`

### Witness shape

`sup_initial_final_child.txt` uses a child whose initial state is already final,
but the good value is obtained only by consuming the current parent symbol:

- [src/tests/correctness_tests/inputs/sup_initial_final_child.txt](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/correctness_tests/inputs/sup_initial_final_child.txt:1)

`sup_initial_final_child_min_bad_current_symbol.txt` is the zero-valued control:

- [src/tests/correctness_tests/inputs/sup_initial_final_child_min_bad_current_symbol.txt](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/correctness_tests/inputs/sup_initial_final_child_min_bad_current_symbol.txt:1)

### Main-repo root cause

On a non-silent parent edge, the main-repo flattener injects the spawned child
into `old_activation` immediately:

- [analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp:5026-5035](./NestedAutomaton_mainRepo_sup_max_compare.cpp#L5026)

But there is no marker saying:

- “this token was spawned on the current parent symbol”

That missing distinction is fatal because both background and witness logic can
terminate a child from its initial-final state before consuming the symbol that
spawned it.

Background path:

- [analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp:4883-4886](./NestedAutomaton_mainRepo_sup_max_compare.cpp#L4883)

Witness path:

- [analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp:4937-4945](./NestedAutomaton_mainRepo_sup_max_compare.cpp#L4937)

In the witness path, if the fresh child is seen as already final, success is
judged using the pre-symbol `witness_y_from` bit. For `Max_f`, that bit starts
at `0` on spawn:

- [analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp:5037-5043](./NestedAutomaton_mainRepo_sup_max_compare.cpp#L5037)

So a child that should consume the spawning symbol and immediately succeed is
instead rejected before the symbol is processed.

### Concrete effect

This causes false negatives:

- `sup_initial_final_child`
  - thresholds `-1, 0, 0.5, 1`
- `sup_initial_final_child_min_bad_current_symbol`
  - thresholds `-1, 0`

The main-repo backend is not “too permissive” here. It is too strict because it
fails to give the fresh child its same-symbol transition.

## Issue 2: Same-Step Completion Does Not Produce Acceptance

### Affected mismatches

- `split_witness_issue5_phase_same_step_control`
- `max_merge_bug_complete` at thresholds `<= 2`

### Main-repo root cause

The main-repo finalization code declares an epoch boundary only if tracking was
already empty in the source state:

- [analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp:4789-4794](./NestedAutomaton_mainRepo_sup_max_compare.cpp#L4789)

In particular:

- `global_final` is set only when `tracking_from` is all-zero
- and the destination parent state is final

This means the backend does **not** treat:

- “the last tracked obligation finished on the current symbol”

as enough to trigger an accepting pulse on that step.

So if a tracked child terminates on the very symbol that also takes the parent
to a final state, the main-repo backend can still miss acceptance.

### `split_witness_issue5_phase_same_step_control`

This fixture is explicitly the control where:

- epoch completion and parent finality coincide on the same `a`

See:

- [src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt:1)

The historical Issue 5 note classifies this file as a control that should
accept:

- [analysis/split_witness_issue5_phase_bug_2026-04-23.md:38-43](./split_witness_issue5_phase_bug_2026-04-23.md#L38)
- [analysis/split_witness_issue5_phase_bug_2026-04-23.md:71-73](./split_witness_issue5_phase_bug_2026-04-23.md#L71)

The main-repo backend rejects it for thresholds `<= 1`, so its acceptance
discipline is even stricter than the historical old-v2 control behavior.

### `max_merge_bug_complete` on low thresholds

This file is historically associated with an old-v2 overlap/merge bug, but the
current mismatch set here is different.

The intended semantics on the file are:

- `Sup Max_f = 2`

See:

- [analysis/old_v2_minmax_sup_overlap_merge_issue_2026-04-22.md:86-124](./old_v2_minmax_sup_overlap_merge_issue_2026-04-22.md#L86)

So thresholds `-1, 0, 0.5, 1, 1.5, 2` should all be accepted, and only
thresholds `> 2` should reject.

The main-repo mismatches are exactly on those low thresholds:

- thresholds `-1, 0, 0.5, 1, 1.5, 2`

That pattern does **not** match the old high-threshold overlap false positives.
Instead it matches the same-step acceptance problem:

- the good invocation spawned on `b` achieves value `2`
- it terminates on the later `c`
- that same `c` returns the parent to final `q0`

So the winning witness finishes on the same step that should generate the
accepting pulse. The main-repo finalization rule misses that because it only
checks `tracking_from`, not “became empty on this step”.

## Issue 3: No Remembered Acceptance Phase

### Affected mismatches

- `split_witness_issue5_phase_async_active_false_negative`

### Witness shape

This fixture isolates the asynchronous phase ordering:

1. one epoch completes on `a`
2. a new epoch starts on `c`
3. only afterwards does the parent become final on `b`

See:

- [src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt:1)
- [analysis/split_witness_issue5_phase_bug_2026-04-23.md:49-53](./split_witness_issue5_phase_bug_2026-04-23.md#L49)

### Main-repo root cause

The main-repo encoded state has:

- activation bits
- tracking bits
- optional witness triple

but it does **not** persist any acceptance-phase information beyond that:

- [analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp:4809-4827](./NestedAutomaton_mainRepo_sup_max_compare.cpp#L4809)

The effect is the same root cause described historically for Issue 5:

- a state is marked final only when completion and parent-final coincide on the
  same step
- there is no remembered “half-credit” saying one side of the Büchi acceptance
  has already happened

The historical description fits the main-repo behavior exactly:

- [analysis/split_witness_issue5_phase_bug_2026-04-23.md:94-110](./split_witness_issue5_phase_bug_2026-04-23.md#L94)

Once a new child starts before the later parent-final visit, the main-repo
backend has no way to remember that the earlier epoch already completed
successfully.

### Concrete effect

This yields false negatives on:

- `split_witness_issue5_phase_async_active_false_negative`
  - thresholds `-1, 0, 0.5, 1`

The backend loses acceptance credit when:

- completion happened earlier
- a new epoch has already started
- the later parent-final visit should still complete the Büchi condition

## Why The Live Threshold-Extremal Path Does Not Show These Mismatches

The live path in [src/NestedAutomaton.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:4615) differs in two key ways.

### 1. Spawn semantics consume the current symbol through obligation spawning

The live backend does not inject a fresh child by mutating `old_activation` and
then reusing ordinary child-state traversal. Instead it spawns obligations via:

- [src/NestedAutomaton.cpp:4782-4792](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:4782)

where the spawning symbol is part of `thrext_spawn_obligation(...)`.

So there is no separate bug class where a fresh initial-final child can be
treated as if it had already existed before the current symbol.

### 2. Acceptance phase is remembered explicitly

The live threshold-extremal backend carries a phase bit:

- [src/NestedAutomaton.cpp:4034-4038](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:4034)

and marks final states using:

- remembered phase `ACC_WAIT_P2EMPTY`
- empty tracked bag `P2`
- non-vacuous epoch flag `epoch_nonempty`

See:

- [src/NestedAutomaton.cpp:4850-4854](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:4850)

That is exactly the piece the main-repo backend lacks.

## Summary By Witness Family

### `sup_initial_final_child`

Underlying issue:

- fresh spawn from an initial-final child is evaluated before consuming the
  spawning symbol

Observed consequence:

- false negative up to threshold `1`

### `sup_initial_final_child_min_bad_current_symbol`

Underlying issue:

- same fresh-spawn/current-symbol bug

Observed consequence:

- false negative up to threshold `0`

### `max_merge_bug_complete`

Underlying issue in this comparison:

- missed same-step accepting pulse when the winning child terminates on the same
  step that returns the parent to final

Important nuance:

- this is **not** the old high-threshold overlap/merge false-positive pattern
- the mismatches here are low-threshold false negatives on values that should
  be accepted because `Sup Max_f = 2`

### `split_witness_issue5_phase_same_step_control`

Underlying issue:

- same-step completion is not recognized as an epoch boundary for acceptance

Observed consequence:

- false negative even in the control case where completion and parent-final are
  intended to coincide

### `split_witness_issue5_phase_async_active_false_negative`

Underlying issue:

- no remembered acceptance phase

Observed consequence:

- false negative when completion and parent-final occur in separate later steps
  with a new epoch already started in between

## Bottom Line

The main-repo `Sup x Max_f` backend is not failing for one single reason.

It has at least three separable correctness problems:

1. same-symbol fresh-spawn handling
2. same-step completion/finality handling
3. missing remembered acceptance phase

The live threshold-extremal backend in `src/NestedAutomaton.cpp` avoids all
three, which matches the observed differential result:

- main-repo copy rejects `20` queries that the live implementation accepts
- there are no mismatches in the opposite direction on the checked matrix
