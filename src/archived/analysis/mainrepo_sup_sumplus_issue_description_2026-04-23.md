# Main-Repo `Sup/LimSup x SumPlus` Issues Against The `SumB` Oracle

Date: 2026-04-23

## Scope

This note describes the known correctness problems in the copied main-repo
`Sup/LimSup x SumPlus` backend in:

- [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp)

The reference semantics is the standard reduction:

1. if needed, project child weights for `SumPlus`
2. run `flatten_regular(SumB, threshold)`
3. remove silent transitions for the outer value function
4. check nonemptiness at threshold `threshold`

That is exactly the oracle used in:

- [mainrepo_sumplus_sup_compare.cpp](./mainrepo_sumplus_sup_compare.cpp)
- [mainrepo_sumplus_smoke_probe.cpp](./mainrepo_sumplus_smoke_probe.cpp)

The current note is about the copied main-repo backend only. The original
`src/NestedAutomaton_mainRepo.cpp` was not modified.

## Backend Shape

The copied main-repo `Sup-SumPlus` backend is still the older explicit
single-witness construction:

- state payload:
  - parent state id
  - one activation bit per flattened child state
  - one tracking bit per flattened child state
  - either `@inactive@` or one distinguished witness triple plus a `BudgetSet`
- no persisted acceptance phase
- no “spawned on the current symbol” marker

Relevant code:

- helper/state definitions:
  [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3562)
- finalization:
  [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3641)
- background selection:
  [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3710)
- tracked-token transition:
  [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3782)
- parent spawn handling:
  [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3884)
- entry point:
  [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3976)

Structurally, this backend still has the same four bug classes that we already
identified and fixed in the copied `Sup-Max` backend:

1. fresh spawn/current-symbol bug
2. same-step completion bug
3. missing acceptance-phase memory
4. one-active-state-per-child overlap loss

For `SumPlus`, all four are now confirmed on existing fixtures.

## Confirmed Mismatch Set

Using the targeted oracle smoke probe:

- [mainrepo_sumplus_smoke_probe.cpp](./mainrepo_sumplus_smoke_probe.cpp)

the copied backend disagrees with the `SumB` oracle on the following known
families.

### Fresh spawn / current symbol

- `sup_initial_final_child`
  - `Sup x SumPlus` at thresholds `0.5`, `1.0`
  - `LimSup x SumPlus` at thresholds `0.5`, `1.0`

Observed:

```text
sup_initial_final_child,Sup,0.5,oracle=1,specialized=0,isNonEmpty=0
sup_initial_final_child,Sup,1,oracle=1,specialized=0,isNonEmpty=0
sup_initial_final_child,LimSup,0.5,oracle=1,specialized=0,isNonEmpty=0
sup_initial_final_child,LimSup,1,oracle=1,specialized=0,isNonEmpty=0
```

### Same-step completion not counted

- `split_witness_issue5_phase_same_step_control`
  - `Sup x SumPlus` at thresholds `0.5`, `1.0`
  - `LimSup x SumPlus` at thresholds `0.5`, `1.0`

Observed:

```text
split_witness_issue5_phase_same_step_control,Sup,0.5,oracle=1,specialized=0,isNonEmpty=0
split_witness_issue5_phase_same_step_control,Sup,1,oracle=1,specialized=0,isNonEmpty=0
split_witness_issue5_phase_same_step_control,LimSup,0.5,oracle=1,specialized=0,isNonEmpty=0
split_witness_issue5_phase_same_step_control,LimSup,1,oracle=1,specialized=0,isNonEmpty=0
```

### Missing acceptance-phase memory

- `split_witness_issue5_phase_async_active_false_negative`
  - `Sup x SumPlus` at thresholds `0.5`, `1.0`
  - `LimSup x SumPlus` at thresholds `0.5`, `1.0`

Observed:

```text
split_witness_issue5_phase_async_active_false_negative,Sup,0.5,oracle=1,specialized=0,isNonEmpty=0
split_witness_issue5_phase_async_active_false_negative,Sup,1,oracle=1,specialized=0,isNonEmpty=0
split_witness_issue5_phase_async_active_false_negative,LimSup,0.5,oracle=1,specialized=0,isNonEmpty=0
split_witness_issue5_phase_async_active_false_negative,LimSup,1,oracle=1,specialized=0,isNonEmpty=0
```

### Overlap loss / one-active-state-per-child

- `split_witness_issue2_limsup_false_positive`
  - `LimSup x SumPlus` at thresholds `0.5`, `1.0`

Observed:

```text
split_witness_issue2_limsup_false_positive,LimSup,0.5,oracle=0,specialized=1,isNonEmpty=1
split_witness_issue2_limsup_false_positive,LimSup,1,oracle=0,specialized=1,isNonEmpty=1
```

## Issue 1: Fresh Spawn Does Not Consume The Spawning Symbol

### Root cause

On a real parent edge, the backend injects the summoned child into
`old_activation[ii]` immediately:

- [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:3911-3917](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3911)

but there is no state saying:

- “this token was spawned on the current symbol”

The background path still treats a final child state as already terminated:

- [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:3735-3738](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3735)

and the tracked path still short-circuits on `child_state->getFinal()` before
consuming the current symbol:

- [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:3794-3807](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3794)

For positive `SumPlus` thresholds, a fresh tracked spawn starts with:

- `budget_from.has_unlimited = true`

at:

- [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:3924-3927](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3924)

but `contains_zero()` does **not** accept that unlimited state:

- [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:3262-3275](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3262)

So a fresh initial-final child is incorrectly rejected before it gets to read
the spawning symbol.

### Confirmed witness

- `sup_initial_final_child`

This is the direct `SumPlus` analogue of the already confirmed `Sup-Max`
fresh-spawn bug.

### Fix status

Known.

The same patch shape used in the copied `Sup-Max` backend should be ported:

1. add transient spawn freshness fields to `data_supremum_t`
2. set them in `explore_global_parent_transition_supremum(...)`
3. in background selection:
   - do not short-circuit a freshly spawned initial-final child
   - if it has no move on the current symbol, fail that branch
4. in tracked-token transition:
   - do not evaluate the witness as already final if it was freshly spawned on
     the current symbol

This is the same “fresh-spawn/current-symbol” fix that is now present in the
patched copied `Sup-Max` backend.

## Issue 2: Same-Step Completion Is Not Counted As Acceptance

### Root cause

Finalization still declares an epoch boundary only if:

- `tracking_from` was already all-zero in the source state

at:

- [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:3646-3650](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3646)

So it does **not** recognize:

- “the last tracked obligation finished on this symbol”

when that completion is only visible in the raw destination tracking state.

This is exactly the same bug we had to fix in `Sup-Max`.

### Confirmed witness

- `split_witness_issue5_phase_same_step_control`

The oracle accepts because completion and parent-final coincide on the same
step. The copied backend still rejects.

### Fix status

Known.

The finalization rule needs the same structural change as in patched `Sup-Max`:

1. compute `completion_now` from raw `tracking_to`, not `tracking_from`
2. do that **before** resetting obligations for the next epoch
3. separate:
   - “parent-final seen”
   - “completion seen”
4. mark the destination final when both halves have been seen, not only when
   they happen to coincide in the source-state test

For `SumPlus`, this patch can be ported almost directly. The main difference is
that the state already carries `BudgetSet` instead of `witness_y`.

## Issue 3: No Persisted Acceptance-Phase Memory

### Root cause

The encoded state stores:

- parent id
- activation bits
- tracking bits
- witness child / state / budget

but no acceptance phase and no `epoch_nonempty` memory:

- [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:3654-3678](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3654)
- [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:4020-4027](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L4020)

So after one good completion happens, the machine cannot remember that it is
still waiting for the other half of the Büchi condition.

That is exactly the async Issue 5 shape:

1. one epoch completes
2. a new real child starts
3. parent-final happens later

The backend forgets the earlier completion as soon as it leaves the step where
it happened.

### Confirmed witness

- `split_witness_issue5_phase_async_active_false_negative`

### Fix status

Known.

The same state-extension pattern as patched `Sup-Max` should be ported:

1. add an acceptance phase enum, for example:
   - `ACC_IDLE`
   - `ACC_PARENT`
   - `ACC_COMPLETE`
   - `ACC_PULSE`
2. add `epoch_nonempty_from`
3. encode both fields into the global state key
4. thread both through the work item / pending-state structure
5. in finalization, latch parent-final and completion independently

This fix and Issue 2’s fix belong together. A `tracking_to`-based completion
test without persisted phase memory would still miss the async-active witness.

## Issue 4: Background Processing Still Loses Overlaps

### Root cause

Background selection still processes at most one active source state of a child
per symbol.

The decisive lines are:

- on successor-final:
  [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:3746-3749](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3746)
- on ordinary successor propagation:
  [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:3752-3764](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3752)
- on no-successor:
  [NestedAutomaton_mainRepo_sup_sumplus_compare.cpp:3768-3770](./NestedAutomaton_mainRepo_sup_sumplus_compare.cpp#L3768)

All of those jump to:

- `explore_global_selection_supremum(child_id + 1, 0, data)`

instead of continuing with:

- the next source state of the same child

So once one active source state of child `i` is processed, the rest of child
`i` is skipped on that symbol.

This is the exact older overlap-loss bug we just fixed in the copied
`Sup-Max` backend.

### Confirmed witness

- `split_witness_issue2_limsup_false_positive`

For `LimSup x SumPlus`, the copied backend wrongly accepts at thresholds `0.5`
and `1.0`, while the `SumB` oracle rejects.

### Fix status

Known.

The same minimal recursion patch as in copied `Sup-Max` should be ported:

- after handling one active source state `(child_id, child_state_id)`, recurse
  to:
  - `explore_global_selection_supremum(child_id, child_state_id + 1, data)`
- not to:
  - `explore_global_selection_supremum(child_id + 1, 0, data)`

Apply that change in the non-failure branches:

- successor-to-final
- ordinary successor propagation
- no-successor non-fresh branch

and keep `child_id + 1, 0` only when the whole child has been scanned.

## Recommended Patch Order

The smallest patch sequence that matches what worked for copied `Sup-Max` is:

1. **Port the fresh-spawn/current-symbol fix**
   - add spawn freshness fields
   - guard early-final and no-successor handling
2. **Port the acceptance-phase patch**
   - add `accept_phase_from`
   - add `epoch_nonempty_from`
   - encode both in the state string
   - rewrite finalization around raw `tracking_to`
3. **Port the child-scan recursion fix**
   - continue scanning the current child after processing an active source state

This order mirrors the `Sup-Max` repair sequence and directly targets the four
confirmed `SumPlus` bug classes above.

## Bottom Line

The copied main-repo `Sup/LimSup x SumPlus` backend has the same four core
correctness problems that we already had to fix for copied `Sup-Max`, and all
four are now confirmed on existing fixtures:

1. fresh spawn does not consume the spawning symbol
2. same-step completion is not counted as acceptance
3. acceptance credit is not persisted across steps
4. background overlap is lost because only one active source state per child is
   processed on each symbol

The fixes are not speculative:

- Issues 1, 2, and 3 have the same patch shape as the already applied
  `Sup-Max` repair
- Issue 4 has the same child-scan recursion patch as the one that eliminated
  the remaining Issue 2 mismatch in copied `Sup-Max`

So this backend is not waiting on a new algorithmic idea. It needs the same
class of surgical repairs, ported to the `BudgetSet`-based `SumPlus` code path.
