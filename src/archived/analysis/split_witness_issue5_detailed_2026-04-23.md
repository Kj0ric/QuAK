# Detailed Analysis And Fix Of Archived Split-Witness Issue 5

Date: 2026-04-23

## Scope

This note is about the archived split-witness backend in:

- `src/NestedAutomaton_OLD_V2.cpp`
- main state layout: `5236-5290`
- acceptance/finalization logic: `5322-5411`

It is not the live production backend.

This document expands the shorter note:

- `analysis/split_witness_issue5_phase_bug_2026-04-23.md`

It now serves two purposes:

- preserve the historical pre-fix failure analysis
- record the localized old-v2 patch that fixes the bug in this checkout

## Current Status

Issue 5 was a real old-v2 false-negative bug, and it is fixed in the current
checkout.

The current archived split-witness backend now agrees with the regular oracle
on both previously failing phase families:

- `src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt`
- `src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt`

Current validation:

- `./build-review/minmax_sup_fix_compare_split_old_v2`
  - `queries=1344 oracle_queries=1344 mismatches=0`
- `./build-review/minmax_sup_split_witness_probe`
  - `mismatches=0/46`
- `./analysis/issue2_compare_probe` on the two phase fixtures
  - all relevant `Sup/LimSup x Max_f @ 1` checks now give `split=1 regular=1`

## Executive Summary

Issue 5 was a real false-negative bug in the archived split-witness backend.

There is a deterministic, complete, one-real-child example where:

- the regular oracle accepts
- the archived split-witness backend rejects
- there is never more than one active real child at a time

So this was not the confirmed Issue 2 overlap-loss bug.

The root cause is not merely "acceptance is checked at the wrong time" in a
loose sense. The precise bug is:

- when an epoch completes, the backend immediately resets tracking for the next
  epoch
- but it stores no persistent bit saying "an accepting epoch has just
  completed"
- therefore the emitted successor state conflates:
  - "fresh next epoch, no pending acceptance credit"
  - with
  - "previous epoch completed successfully, but parent-final may be witnessed
    later"

If a new real child is spawned before the parent later visits a final state,
that pending acceptance credit is lost.

The current old-v2 patch fixes this by adding:

- an explicit split acceptance phase in the encoded state
- a persisted `epoch_nonempty` bit for the currently open epoch

## The Three-Fixture Family

The current repo contains three Issue 5 fixtures:

- same-step control:
  `src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt`
- async-idle control:
  `src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_idle.txt`
- actual phase-only repro:
  `src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt`

### Same-Step Control

The parent becomes final on the same `a` that terminates the child:

- fixture:
  `split_witness_issue5_phase_same_step_control.txt:10-40`
- key parent path:
  - `c : 1, p0 -> p1`
  - `a : 0, p1 -> p2`
- child behavior:
  - `c : 0, s0 -> s1`
  - `a : 1, s1 -> f`

Result:

- regular oracle: accepts
- archived split-witness: accepts

This is the sanity check that same-step coincidence still works.

### Async-Idle Control

The child terminates on `a`, and the parent becomes final later on `b`, but no
real child is active on that later `b`:

- fixture:
  `split_witness_issue5_phase_async_idle.txt:11-41`
- key parent path:
  - `c : 1, p0 -> p1`
  - `a : 0, p1 -> p2`
  - `b : 0, p2 -> p0`

Result:

- regular oracle: accepts
- archived split-witness: accepts

This matters because it shows that "completion and parent-final on different
symbols" is not enough by itself to trigger the bug.

### Actual Reproducer

The child terminates on the first `a`, a new epoch starts on `c`, and only then
does the parent become final on `b`:

- fixture:
  `split_witness_issue5_phase_async_active_false_negative.txt:15-51`
- key parent path:
  - `c : 1, p0 -> p1`
  - `a : 0, p1 -> p2`
  - `c : 1, p2 -> p3`
  - `b : 0, p3 -> p4`
  - `a : 0, p4 -> p0`
- child behavior:
  - `c : 0, s0 -> s1`
  - `a : 1, s1 -> f`
  - `b : 0, s1 -> s1`
  - `c : 0, s1 -> s1`

Historical pre-fix results:

- regular oracle:
  - `Sup x Max_f @ 1`: true
  - `LimSup x Max_f @ 1`: true
- archived split-witness before the patch:
  - `Sup x Max_f @ 1`: false
  - `LimSup x Max_f @ 1`: false

## Why The Reproducer Is Clean

This example avoids the confounders from earlier split-witness bugs.

- only one real child exists
- the child is deterministic
- the child is complete on non-final states
- there is never more than one active real child at a time
- there is no same-child overlap
- there is no multiplicity collapse
- there is no incomplete-child dropping

So if the split-witness backend disagrees with the regular oracle here, the
problem is in the acceptance discipline itself.

## Oracle Comparison Before The Patch

Using:

```text
./analysis/issue2_compare_probe \
  src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt \
  src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_idle.txt \
  src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt
```

we get:

```text
src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt inf=Sup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt inf=LimSup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_idle.txt inf=Sup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_idle.txt inf=LimSup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt inf=Sup fin=Max_f threshold=1 split=0 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt inf=LimSup fin=Max_f threshold=1 split=0 regular=1
```

So the archived split-witness backend originally handled:

- same-step completion plus parent-final
- delayed parent-final while no real child is active

but fails when:

- an epoch completes
- a new epoch starts before the later parent-final visit

## Expected Nested Behavior On The Reproducer

Consider the ultimately periodic word:

- `(c a c b a)^omega`

Along this word:

1. `c` from `p0` spawns Child 1 and moves it `s0 -> s1`
2. the first `a` terminates that child with value `1`
3. the next `c` starts a new child and again moves it `s0 -> s1`
4. `b` keeps that child alive in `s1` while the parent reaches final state `p4`
5. the last `a` terminates the second child with value `1`

This repeats forever.

Important structural facts:

- every spawned real child terminates
- infinitely many successful `1`-valued terminations occur
- the parent visits its final state infinitely often
- there is no background debt that survives forever

So the regular oracle has no reason to reject this behavior, and indeed it
accepts.

## What The Archived Split-Witness Flattening Produced Before The Patch

Dumping the archived split-witness flattening with:

```text
./analysis/split_phase_probe \
  src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt
```

showed the critical symptom immediately:

```text
INITIAL = 0/00000/00000/@inactive@
FINAL = (none)
```

The relevant lasso fragment in the dumped automaton is:

```text
0/00000/00000/@inactive@
  --c:0-->
1/00000/11111/1/3/0/1

1/00000/11111/1/3/0/1
  --a:1-->
2/00000/11111/@inactive@

2/00000/11111/@inactive@
  --c:0-->
3/00000/11111/1/3/0/1

3/00000/11111/1/3/0/1
  --b:0-->
4/00000/00000/1/3/0/1

4/00000/00000/1/3/0/1
  --a:1-->
0/00000/11111/@inactive@
```

There are `1`-edges on the two `a` steps, but no final state is ever reached.

That is the exact historical false-negative pattern.

## What "Epoch Complete" Means In This Backend

The helper:

- `tracking_all_zero(...)`: `src/NestedAutomaton_OLD_V2.cpp:3548-3552`

is just a plain bitwise zero test.

In finalization, the backend defines:

```text
destination_tracking_all_zero =
    tracking_all_zero(bg_tracking_to) &&
    (inactive_to || witness_tracked_to == 0u)
```

Code:

- `src/NestedAutomaton_OLD_V2.cpp:5328-5330`

So "the current epoch has completed" means:

- no tracked background obligation remains
- and either there is no witness, or the witness is no longer tracked

That part is not the bug by itself.

## The Exact Root Cause

The problem is the combination of three design choices in
`explore_global_finalization_min_max_sup_split(...)`.

### 1. Acceptance Is Tested Only On The Completion Step

When raw destination tracking is all zero, the code does:

- detect completion on the raw destination state
- immediately reset tracking for the next epoch
- mark the destination final only if the parent destination state is final on
  that same step

Code:

- `5328-5340`
- `5394-5395`

So acceptance is tied to the coincidence:

- "the current epoch just completed now"
- and
- "the parent is final now"

There is no later acceptance path that says:

- "the epoch completed earlier, and the parent is final now"

### 2. Completion Credit Is Not Stored In The State Key

The encoded split-witness state contains:

- parent id
- `bg_activation`
- `bg_tracking`
- either `@inactive@` or the witness tuple

Code:

- state/work-item fields: `5236-5290`
- emitted string key: `5362-5380`

There is no additional field such as:

- `phase`
- `final_pulse`
- `epoch_completed`
- `acceptance_pending`

So after completion, the successor state has no persistent memory that a good
epoch has just ended.

### 3. Reset For The Next Epoch Destroys The Only Evidence Of The Previous One

When `destination_tracking_all_zero` holds, the code does:

```text
bg_tracking_to = track_them_all;
if (!inactive_to) witness_tracked_to = 1u;
global_final = parent_state_to.isFinal();
```

Code:

- `5331-5340`

That means:

- the raw zero-tracking destination is used only transiently inside the current
  step
- the emitted successor state is already the reset next-epoch state

So two semantically different situations collapse into the same encoded state:

1. "the previous epoch completed successfully; a later parent-final visit should
   still count"
2. "we are simply in a fresh next epoch with no pending acceptance credit"

That collapse is the real bug.

## Step-By-Step Failure On The Reproducer

### After The First `c`

The split-witness branch picks Child 1 as the witness:

```text
0/00000/00000/@inactive@
  --c-->
1/00000/11111/1/3/0/1
```

Interpretation:

- parent moved `p0 -> p1`
- a witness is active
- tracking is armed for the current epoch

### After The First `a`

The witness terminates with value `1`, so the current epoch completes:

```text
1/00000/11111/1/3/0/1
  --a:1-->
2/00000/11111/@inactive@
```

This is the crucial moment.

Semantically, the system should now remember:

- "an accepting epoch has just completed"

But the emitted state stores only:

- parent `p2`
- reset tracking `11111`
- inactive witness

There is no separate marker that a completion event has happened.

### After The Next `c`

A new epoch starts before the parent reaches a final state:

```text
2/00000/11111/@inactive@
  --c-->
3/00000/11111/1/3/0/1
```

At this point the missing completion memory matters.

The state now looks like an ordinary active epoch state. The earlier successful
completion is no longer represented anywhere.

### On The Later `b`

The parent reaches final state `p4`, but the new witness is still active:

```text
3/00000/11111/1/3/0/1
  --b:0-->
4/00000/00000/1/3/0/1
```

This state is not final because:

- the witness is still tracked
- so `destination_tracking_all_zero` is false

Therefore the parent-final visit does not count.

### On The Final `a`

The new witness terminates:

```text
4/00000/00000/1/3/0/1
  --a:1-->
0/00000/11111/@inactive@
```

Now completion occurs again, but the parent destination is `p0`, which is not
final.

So this step also does not create a final state.

The cycle repeats forever with:

- completion on `a`
- parent-final on `b`
- but never both on the same step

Since the backend stored no persistent completion credit, the automaton ends up
with `FINAL = (none)`.

## Why The Async-Idle Control Still Passes

The async-idle control is important because it shows the issue is narrower than
"later parent-final is always lost".

In that control:

- the child completes on `a`
- the later `b` is a dummy-child step
- no real child is active on that `b`

So on the later `b`, the raw destination tracking is again all zero, and the
same-step finality test succeeds even without any persistent phase memory.

That is why:

- async delay alone is not enough
- the bug needs a new active epoch to start before the parent-final visit

## Why This Is Not Issue 2 In Disguise

The confirmed Issue 2 bug needed:

- two simultaneously active local states of the same child
- with one silently dropped during propagation

None of that happens here.

- there is only one real child
- it is either inactive or in one active state
- no same-child overlap is ever present

So this mismatch cannot be explained by lossy background propagation.

## Minimal Repair Shape

Any real fix needs one extra piece of state that survives completion.

Examples:

- a dedicated phase bit
- a one-step `final_pulse`
- an `epoch_completed` / `acceptance_pending` latch

Without such a field, the backend cannot distinguish:

- "fresh next epoch"
- from
- "next epoch started, but acceptance from the previous epoch is still pending a
  parent-final witness"

As long as those two situations collapse to the same encoded split-witness
state, this false negative is structurally unavoidable.

## Applied Fix

The localized old-v2 patch keeps the witness/background propagation logic
unchanged and changes only the acceptance bookkeeping.

### New Persisted State

The archived split-witness state now carries:

- `accept_phase_from`
- `epoch_nonempty_from`

with the acceptance phase values:

- `SPLIT_ACC_IDLE`
- `SPLIT_ACC_PARENT`
- `SPLIT_ACC_COMPLETE`
- `SPLIT_ACC_PULSE`

The encoded split-witness state key now includes both fields, so the backend no
longer aliases:

- a fresh next epoch with no pending acceptance information
- and a next epoch that already carries one half of the acceptance condition

### Acceptance Update Rule

The patch now records the two acceptance halves independently:

- whether a parent-final visit has been seen
- whether a non-vacuous completion has been seen

It also persists whether the currently open epoch is non-vacuous via
`epoch_nonempty_from`, with a transient `current_parent_edge_is_real` flag to
mark that a real child was started on the current parent step.

In finalization:

- raw completion is still detected on the raw destination tracking state before
  reset
- parent-final and completion are latched independently
- when both have been seen, the successor is stored with
  `SPLIT_ACC_PULSE` and marked final

This fixes both orderings:

- parent-final first, completion later
- completion first, parent-final later

without turning finality into edge-only history.

### Why `epoch_nonempty` Is Still Needed

The acceptance phase alone is not enough.

The backend must still distinguish:

- a genuinely completed epoch that saw real child activity
- from a vacuous silent epoch that should not create acceptance credit

So the patch keeps a separate `epoch_nonempty` bit in the state key, matching
the same non-vacuity discipline used in the corrected cached and live `Inf`
backends.

## Post-Fix Validation

### Focused Phase Fixtures

Re-running:

```text
./analysis/issue2_compare_probe \
  src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt \
  src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt \
  src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_idle.txt \
  src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt
```

now gives:

```text
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=Sup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt inf=LimSup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt inf=Sup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt inf=LimSup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_idle.txt inf=Sup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_idle.txt inf=LimSup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt inf=Sup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt inf=LimSup fin=Max_f threshold=1 split=1 regular=1
```

So the old-v2 split-witness backend now handles:

- same-step completion plus parent-final
- delayed parent-final while no real child is active
- completion-first, then later parent-final after a new epoch starts
- parent-final-first, then later completion

### Broad Oracle Comparison

The full archived split-witness compare harness is now green:

```text
./build-review/minmax_sup_fix_compare_split_old_v2
queries=1344 oracle_queries=1344 mismatches=0
```

### Dedicated Split-Witness Probe

The dedicated split-witness probe now also includes the previously failing
async-active Issue 5 fixture as a passing regression:

```text
./build-review/minmax_sup_split_witness_probe
mismatches=0/46
```

## Current Bottom Line

Issue 5 was a real acceptance-discipline bug in the archived split-witness
backend. The root cause was exactly what the historical analysis identified:
the state representation had no place to store one half of the acceptance
condition across steps.

That root cause is now fixed in the current checkout by:

- storing an explicit split acceptance phase
- storing `epoch_nonempty`
- encoding both into the state key

So this document should now be read as:

- a historical record of the bug and why it happened
- plus the validation record that the patched old-v2 split-witness backend now
  matches the regular oracle on the previously failing phase families
