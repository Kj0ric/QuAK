# Potential Incorrectness Risks In The Archived Split-Witness `Min/Max x Sup/LimSup` Backend

Date: 2026-04-23

## Scope

This note is about the archived split-witness backend implemented in:

- `src/NestedAutomaton_OLD_V2.cpp`
- main body: lines `5236-5754`

It is not the live `quak` path.

In the current library:

- `NestedAutomaton::flatten_MinMax_Sup(...)` immediately returns the shared
  threshold-extremal backend in `src/NestedAutomaton.cpp:5933-5938`
- `NestedAutomaton::flatten_MinMax_Sup_split_witness(...)` is only an alias to
  `flatten_MinMax_Sup(...)` in `src/NestedAutomaton.cpp:6034-6036`
- `isNonEmpty(...)` routes `Sup/LimSup x {Max_f, Min_f}` to
  `flatten_MinMax_Sup(...)` in `src/NestedAutomaton.cpp:10252-10256`

So this note is about an archived experimental construction and the
`build-review` / `quak_old_v2` comparison path, not the production backend.

## Repo-Local Assumptions

The analysis below uses the repository's own conventions, not a generic nested
automata formalism.

- `@CHILD 0` is the implicit dummy/silent child:
  `src/tests/sanity_tests/inputs/README.txt:90-94`
- non-dummy children are not allowed to contain `SILENT` transitions:
  `src/NestedAutomaton_OLD_V2.cpp:7701-7720`
- the split-witness backend is evaluated as a `0/1` regular automaton and then
  checked via `isNonEmpty_withFinal(...)`, exactly like the compare harness in
  `src/tests/probes/minmax_sup_fix_compare.cpp:34-56`

## Current Evidence Level

The repo does contain nontrivial evidence in favor of this backend on the
currently checked matrix.

Current reruns give:

- `build-review/minmax_sup_fix_compare_split_old_v2`:
  `queries=1344 oracle_queries=1344 mismatches=0`
- `build-review/minmax_sup_split_witness_probe`:
  `mismatches=0/46`

So the points below should be read as:

- structurally plausible reasons the backend could still be incorrect
- not already-established live failures on the currently checked fixture set

## What The Backend Is Trying To Compute

The split-witness construction keeps:

- one distinguished child invocation called the witness
- all other active child invocations summarized as background obligations

The flattened state contains:

- parent state id
- `bg_activation`: one bit per child-local state
- `bg_tracking`: one bit per child-local state
- `accept_phase`
- `epoch_nonempty`
- either `@inactive@` or a witness tuple:
  - `witness_child_id`
  - `witness_child_state_id`
  - `witness_y`
  - `witness_tracked`

Code:

- state/data layout: `src/NestedAutomaton_OLD_V2.cpp:5236-5290`
- initial state: `src/NestedAutomaton_OLD_V2.cpp:5688-5712`

The witness carries the threshold summary:

- `Max_f`: `y` starts at `0`, then flips to `1` once some child edge meets the
  threshold
- `Min_f`: `y` starts at `1`, then drops to `0` forever once some child edge
  misses the threshold

Code:

- `min_max_y_update_split(...)`: `src/NestedAutomaton_OLD_V2.cpp:5301-5309`

Parent-step handling:

- always try "spawn as background"
- if there is no current witness, also try "spawn as witness"

Code:

- `explore_global_parent_transition_min_max_sup_split(...)`:
  `src/NestedAutomaton_OLD_V2.cpp:5547-5609`

Weight generation:

- flattened edge weight becomes `1` only when the witness terminates
  successfully on the current symbol

Code:

- `src/NestedAutomaton_OLD_V2.cpp:5523-5534`

Acceptance:

- compute whether the raw destination tracking state is all zero
- latch parent-final and non-vacuous completion information separately
- if raw completion happens, reset tracking for the next epoch
- mark the destination final when both acceptance halves have been seen, via a
  one-state accepting pulse

Code:

- `explore_global_finalization_min_max_sup_split(...)`:
  `src/NestedAutomaton_OLD_V2.cpp:5322-5411`

## Issue 1: Historical Acceptance Bug On Source vs Destination Tracking

Status:

- confirmed historically
- already fixed in the current archived source

The repo already records one genuine split-witness correctness bug:

- `analysis/minmax_sup_change_log.md:417-443`

The bug was:

- epoch completion was previously checked on the source tracking state
- but the last tracked obligation may disappear on the current symbol itself
- so the true epoch boundary is visible only in the raw destination tracking
  state before reset

The current archived code now uses destination-side zero-tracking:

- `src/NestedAutomaton_OLD_V2.cpp:5328-5340`

Why this matters:

- it shows the acceptance plumbing is subtle
- small changes in when zero-tracking is tested can change nonemptiness

This is not an open issue anymore, but it is strong evidence that the backend
deserves careful scrutiny.

## Issue 2: Background Traversal Processes At Most One Active State Per Child

Status:

- confirmed historically on a complete deterministic one-child counterexample
- fixed in the current checkout of `src/NestedAutomaton_OLD_V2.cpp`
- high-risk structural concern

The key traversal is:

- `explore_global_selection_min_max_sup_split(...)`
- `src/NestedAutomaton_OLD_V2.cpp:5414-5478`

The suspicious part is:

- after selecting one active local state of child `child_id`
- the recursion continues with `child_id + 1`
- not with `child_state_id + 1` on the same child

Concretely:

- `5444`: final successor branch jumps to next child
- `5460`: non-final successor branch jumps to next child

This means that on a single parent symbol, the backend can process at most one
active local state per child.

Why this is dangerous:

- if the same child has two simultaneously active background tokens in
  different local states, only one of those states is propagated
- the other one is silently forgotten in the destination summary

Likely consequence:

- false positives are the main risk
- an old background obligation can disappear even though it should continue
  blocking the run

Confirmed counterexample:

- fixture:
  `src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt`
- results:
  - current live backend: `Sup=1`, `LimSup=0`
  - archived split-witness backend: `Sup=1`, `LimSup=1`
- direct compare probe:
  - `analysis/issue2_compare_probe` reports
    `split=1 regular=0` for `LimSup x Max_f` at threshold `1`

Patch now applied in this checkout:

- `src/NestedAutomaton_OLD_V2.cpp:5444` and `5460` no longer jump directly to
  `child_id + 1`
- they now continue with `child_state_id + 1` of the same child
- the same adjustment was made in the non-fresh `succs == null` branch at
  `5469`, so traversal consistently finishes the current child before moving on

Post-patch validation:

- `build-review/minmax_sup_split_witness_probe`:
  `mismatches=0/46`
- `build-review/minmax_sup_fix_compare_split_old_v2`:
  `queries=1344 oracle_queries=1344 mismatches=0`

Why this example isolates Issue 2:

- the child is deterministic and complete on all non-final states
- there is only one real child, so the bug does not depend on interactions
  between different children
- the critical overlap happens after the prefix `bc`
  - the old invocation should move `s1 -> s2`
  - the fresh invocation spawned on `c` stays in `s0`
- the split-witness inactive state after the initial `b` is
  `0/00010/11111/@inactive@`
- on `c`, the flattened automaton goes to `0/00100/00100/@inactive@`
  instead of a state carrying both background obligations

So the observed bad step is exactly the one-state-per-child traversal loss:

- the fresh lower-index `s0` background copy is processed first
- recursion jumps to `child_id + 1`
- the older `s1 -> s2` blocker is never propagated

Why the mismatch is semantic, not just local:

- once the old blocker is dropped, the split-witness flattening admits the
  accepting lasso `b (ccbb)^omega`
- along that lasso it revisits final states infinitely often and gets a `1`
  edge on every second `b`
- in the true nested semantics, the original `b`-spawned child reaches `s2`
  and then never terminates, so `LimSup >= 1` should be false

Why this matters for later checks:

- any argument about acceptance timing or epoch boundaries can be confounded by
  this bug class, because "tracking all zero" may happen only because a token
  was silently dropped

Recommended isolated test shape:

- deterministic and complete child
- two overlapping invocations of the same child
- they occupy different local states on the same symbol
- one is old and should persist
- one is fresh and should not erase the old one

## Issue 3: The Background Summary Loses Multiplicity

Status:

- unresolved
- no mismatch reproduced yet on the current Issue 3 search families
- still needs stronger directed checks before it can be downgraded
- medium/high structural concern

The background is stored only as two bit-vectors:

- `bg_activation`
- `bg_tracking`

Code:

- `src/NestedAutomaton_OLD_V2.cpp:5240-5248`
- `src/NestedAutomaton_OLD_V2.cpp:5265-5277`

This representation says only:

- "some active background token exists at local state `s`"
- "some tracked background token exists at local state `s`"

It does not store:

- how many such tokens exist
- whether two tokens at the same local state came from different invocations
- whether two collocated tokens should later make different nondeterministic
  choices

Important distinction:

- the split-witness backend was introduced partly to avoid the older
  witness/background overlap bug in `flatten_MinMax_Sup(...)`
- but it only separates witness from background
- it still does not separate background from background

Likely consequence:

- false positives when two background copies must be kept distinct
- especially if they meet in the same local state before diverging again

The risk is highest for:

- nondeterministic children
- or constructions where multiple invocations merge to one local state and then
  later need to be distinguished again semantically

Current search status:

- search driver:
  `analysis/issue3_search.cpp`
- completed search families:
  - `all_collocated`: `512` split-vs-regular checks, `0` mismatches
  - `forced_collision`: `3584` split-vs-regular checks, `0` mismatches
- attempted but not completed:
  - broader random same-state search under `final: all`
  - broader random same-state search with a two-state parent Buchi pattern

Interpretation:

- these completed searches are useful negative evidence against an easy-to-hit
  same-state multiplicity bug
- they are not a proof that Issue 3 is benign
- the broader searches were stopped before completion, so they do not change
  the issue status either way

What still needs to be checked:

- a more directed local oracle for the background component
- in particular, a comparison between:
  - the split summary for one tracked plus one untracked copy in the same child
    state
  - a multiset-aware local successor computation for those two copies
- that would test the real semantic question directly, rather than relying only
  on blind search

Recommended isolated test shape:

- one child
- two background copies reach the same local state
- under the future letters, one copy can still terminate but the other should
  remain as an obligation
- the bitset summary cannot express that difference

## Issue 4: Existing Background Tokens With No Successor Are Silently Dropped

Status:

- not an issue under the repository's completeness assumption
- only relevant if incomplete children are admitted deliberately

In the background step:

- if `succs` is null for a freshly spawned background token, the branch fails
- if `succs` is null for an already-active background token, the code just
  skips it and continues

Code:

- `src/NestedAutomaton_OLD_V2.cpp:5465-5469`

By contrast:

- the witness path does fail on missing successors:
  `src/NestedAutomaton_OLD_V2.cpp:5507-5510`

So the policy is asymmetric:

- witness missing successor => reject branch
- fresh background missing successor => reject branch
- old background missing successor => silently forget it

Why this would be dangerous in a more general setting:

- if incomplete children are semantically admissible, an already-active child
  invocation that cannot move on the current letter should not simply vanish

Likely consequence outside the completeness assumption:

- false positives on incomplete children

But under the working assumption for this repo analysis:

- nested automata are taken to be complete on the relevant non-final states
- so this branch should be unreachable in the intended input class

Conclusion for this note:

- do not treat Issue 4 as an active correctness concern for the assumed model
- only revisit it if we intentionally test robustness beyond the completeness
  assumption

## Issue 5: No Explicit Phase Bit In The Acceptance Discipline

Status:

- historically real
- fixed in the current old-v2 checkout
- no longer an active correctness concern here

The original old-v2 split-witness backend marked a state final only when:

- raw destination tracking is all zero
- and the parent destination state is final on that same step

Code:

- `src/NestedAutomaton_OLD_V2.cpp:5328-5340`

There was no explicit persisted acceptance phase that separated:

- "the epoch has just completed"
- from
- "the parent Büchi condition is witnessed afterwards"

Why this was problematic:

- this acceptance condition is stronger than a generic two-phase obligation
  construction
- if the intended semantics only require eventual witnessing of both phenomena,
  but not coincidence on the same symbol, this encoding is too strict

That suspicion is now fully resolved. Two independent phase families were
checked against the regular oracle:

- `phase_parent_final_then_empty.txt`
  - parent-final first, completion later
- `split_witness_issue5_phase_async_active_false_negative.txt`
  - completion first, new epoch starts, parent-final later

Both were real old-v2 mismatches before the patch.

The localized fix now applied in `src/NestedAutomaton_OLD_V2.cpp` adds:

- an explicit split acceptance phase:
  - `SPLIT_ACC_IDLE`
  - `SPLIT_ACC_PARENT`
  - `SPLIT_ACC_COMPLETE`
  - `SPLIT_ACC_PULSE`
- a persisted `epoch_nonempty` bit

and encodes both into the emitted split-witness state key.

Post-fix validation:

- `./analysis/issue2_compare_probe` on both phase families now reports
  `split=1 regular=1` for the relevant `Sup/LimSup x Max_f @ 1` checks
- `./build-review/minmax_sup_fix_compare_split_old_v2`
  - `queries=1344 oracle_queries=1344 mismatches=0`
- `./build-review/minmax_sup_split_witness_probe`
  - `mismatches=0/46`

So Issue 5 should now be treated as:

- a confirmed historical bug class
- fixed and regression-covered in the current old-v2 checkout

## Issue 6: `can_reach_final` Pruning Is Very Coarse

Status:

- low-risk as an unsoundness source by itself
- worth documenting, but not a primary suspect

The helper:

- `compute_can_reach_final_child(...)`
- `src/NestedAutomaton_OLD_V2.cpp:4636-4672`

computes only plain graph reachability to a final child state.

It ignores:

- thresholds
- parent control
- future word constraints

This means:

- the pruning is only a definite-doom filter
- it can miss many semantically doomed obligations
- but that incompleteness alone is not obviously unsound

So this is probably:

- a weakness in pruning power
- not the main source of correctness bugs

## Why The Existing Zero-Mismatch Comparison Does Not Fully Discharge These Risks

The existing comparison evidence is real, but limited.

The compare harness:

- uses the split-witness flattening as the specialized backend:
  `src/tests/probes/minmax_sup_fix_compare.cpp:34-47`
- compares it to the regular oracle:
  `src/tests/probes/minmax_sup_fix_compare.cpp:50-56`
- runs a fixed matrix of inputs, thresholds, and objective pairs:
  `src/tests/probes/minmax_sup_fix_compare.cpp:101-145`

This is strong evidence against broad, easy-to-hit bugs.

It is not a proof against narrow bugs that require:

- carefully staged overlap patterns
- incomplete children
- nondeterministic same-state multiplicity
- or specially isolated phase-only behavior

So the current repo state is best summarized as:

- split-witness is much healthier than the original old-v2 `flatten_MinMax_Sup`
- but it still relies on lossy summaries and deserves targeted adversarial checks

## Suggested Investigation Order

To avoid confounding one bug class with another, the cleanest order is:

1. Issue 2 is now confirmed.
   Reason:
   one-child, two-local-state overlap really does produce a semantic mismatch.

2. Issue 3 remains open and still needs stronger directed checks.
   Reason:
   the completed search families found no mismatch, but they only provide
   negative evidence and do not yet discharge the multiset-vs-bitset concern.

3. Revisit Issue 5 only after Issues 2-3 are neutralized.
   Reason:
   otherwise any apparent phase bug may just be token loss in disguise.

## Practical Conclusion

At a code-reading level, there are still credible reasons the archived
split-witness backend could be incorrect on adversarial inputs:

- lossy one-state-per-child background propagation
- lossy multiplicity-free background summary
- silent dropping of old background tokens on missing successors
- acceptance tied directly to epoch completion, without explicit phasing

At the same time, the current repo evidence says:

- none of these concerns currently show up on the checked 1248-query matrix
- at least one earlier acceptance bug was already fixed

So the right working stance is:

- not "already disproved"
- not "known broken everywhere"
- but "narrow, plausible unsoundness risks remain and should be tested
  adversarially"
