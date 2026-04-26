# Archived Split-Witness Issue 5: Reproduced, Root-Caused, And Fixed

Date: 2026-04-23

## Scope

This note is about the archived split-witness backend in:

- `src/NestedAutomaton_OLD_V2.cpp`
- the `flatten_MinMax_Sup_split_witness(...)` path

It is not the live production backend.

## Current Status

Issue 5 was a real old-v2 false-negative bug, and it is fixed in the current
checkout.

The bug was originally reproduced on a deterministic, complete, one-real-child
fixture where:

- the regular oracle accepted
- the archived split-witness backend rejected
- there was never more than one active real child at a time

That mismatch was not explained by the confirmed Issue 2 overlap-loss bug.

After the patch, the archived split-witness backend now matches the regular
oracle on both of the previously failing phase families:

- `phase_parent_final_then_empty.txt`
- `split_witness_issue5_phase_async_active_false_negative.txt`

It also remains green on the existing overlap and background tests.

## Historical Reproducer Family

Three closely related Issue 5 fixtures are kept in the repo.

Controls:

- `src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt`
- `src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_idle.txt`

Historical failing repro:

- `src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt`

The key shape of the historical mismatch was:

1. one epoch completed on `a`
2. a new epoch started on `c`
3. only afterwards did the parent become final on `b`

The child is deterministic and complete on its non-final states:

- `c` starts an invocation
- `a` is the only good termination letter with value `1`
- `b` and `c` keep the invocation alive with value `0`

So the family isolates acceptance timing, not background-state loss.

## Historical Failure Before The Patch

Using:

- `./analysis/issue2_compare_probe <file>`

the three fixtures originally gave:

- `split_witness_issue5_phase_same_step_control.txt`
  - `Sup x Max_f @ 1`: `split=1 regular=1`
  - `LimSup x Max_f @ 1`: `split=1 regular=1`
- `split_witness_issue5_phase_async_idle.txt`
  - `Sup x Max_f @ 1`: `split=1 regular=1`
  - `LimSup x Max_f @ 1`: `split=1 regular=1`
- `split_witness_issue5_phase_async_active_false_negative.txt`
  - `Sup x Max_f @ 1`: `split=0 regular=1`
  - `LimSup x Max_f @ 1`: `split=0 regular=1`

So old-v2 handled:

- same-step completion plus parent-final
- delayed parent-final while no real child was active

but failed when:

- completion happened earlier
- a new epoch had already started
- the parent-final visit came afterwards

## Root Cause

The old-v2 finalization logic used to mark a state final only when both held on
the same destination step:

- raw destination tracking was all zero
- the destination parent state was final

There was no persisted acceptance-phase information in the encoded state key.

That meant the emitted successor state could not distinguish:

- "the previous epoch completed successfully and is still waiting for the other
  acceptance half"
- from
- "this is just an ordinary fresh epoch with no pending acceptance credit"

Once a new real child started before the later parent-final visit, the old
backend lost the only evidence that a good completion had already happened.

## Patch Summary

The fix in `src/NestedAutomaton_OLD_V2.cpp` adds two pieces of persisted state
to the archived split-witness machine:

- `accept_phase_from` with four states:
  - `SPLIT_ACC_IDLE`
  - `SPLIT_ACC_PARENT`
  - `SPLIT_ACC_COMPLETE`
  - `SPLIT_ACC_PULSE`
- `epoch_nonempty_from`

The patch also:

- encodes both fields into the emitted split-witness state key
- tracks whether the current parent edge is real
- latches the two acceptance halves independently:
  - parent-final seen
  - non-vacuous completion seen
- emits a one-state accepting pulse when both halves have been seen

This is the smallest localized patch that fixes both phase orders without
reworking witness/background propagation.

## Current Results After The Patch

Re-running:

```text
./analysis/issue2_compare_probe \
  src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt \
  src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt
```

now gives:

```text
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Sup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimSup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt inf=Sup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt inf=LimSup fin=Max_f threshold=1 split=1 regular=1
```

Broader validation:

- `./build-review/minmax_sup_fix_compare_split_old_v2`
  - `queries=1344 oracle_queries=1344 mismatches=0`
- `./build-review/minmax_sup_split_witness_probe`
  - `mismatches=0/46`

The dedicated split-witness probe now includes the formerly failing async-active
Issue 5 fixture as a passing regression.

## Repo State Kept

The repo now keeps:

- the full three-fixture Issue 5 family under
  `src/tests/correctness_tests/inputs/`
- the async-active Issue 5 repro in
  `src/tests/minmax_sup_split_witness_probe.cpp`
- synced extract copies:
  - `backend_split_sup_extract.cpp`
  - `backend_cached_inf_and_split_sup_extract.cpp`

So Issue 5 remains documented as a historical old-v2 bug, but it is no longer
an active correctness concern in this checkout.
