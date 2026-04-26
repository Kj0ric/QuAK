# Cached Backend Promotion Plan

## Goal

Promote `flatten_MinMax_Inf_cached(...)` from an experimental backend to the
default production implementation for Min/Max × Inf/LimInf, while preserving
the current shared obligation semantics and keeping a safe rollback path.

Production means all of the following:
- correctness parity with the accepted reference suite
- no repeat-use crashes or lifecycle bugs
- explicit handling of the agreed one-state-child semantics
- performance that is at least neutral on small sparse cases and clearly better
  on the medium cases where `current` wastes work
- bounded maintenance risk: no semantic fork that drifts away from the shared
  threshold-obligation logic

## Current Status

From the evaluation in this folder:
- `cached` is the most promising scalability direction
- `cached` removes the real hot spots seen in `current`
- `cached` is faster overall on the saved resource family
- the previous repeated-use cached crash has been root-caused and fixed
- one-state-child semantics are not yet implemented consistently anywhere

This means promotion should be treated as a staged engineering project, not as a
single dispatcher change.

## Promotion Strategy

Use a five-phase promotion path:

1. lock semantics and invariants
2. stabilize the cached implementation
3. build a production-grade regression and benchmark gate
4. refactor for maintainability and semantic sharing
5. roll out behind a controlled switch, then flip the default

The dispatcher should move only at the end of phase 5.

## Phase 1: Lock Semantics And Invariants

### Objective

Remove the semantic ambiguity that would otherwise make production failures hard
to interpret.

### Required decisions

The following must become explicit code-level behavior:
- one-state child with final unique state: treat as silent
- one-state child with non-final unique state: reject as invalid input

### Required implementation work

1. Add a single helper that classifies a child for Min/Max Inf/LimInf flattening:
   - absent child
   - silent singleton child
   - enabled tracked child
   - invalid singleton child

2. Use that helper consistently in:
   - `flatten_MinMax_Inf(...)`
   - `flatten_MinMax_Inf_cached(...)`
   - `flatten_MinMax_Inf_threshold_obl(...)`
   - any shared child-table construction path touched by these backends

3. Replace accidental failures with intentional behavior:
   - no `map::at` abort for final singleton children
   - no parser-shape accident for invalid singleton children
   - invalid singleton children should fail with a clear diagnostic

### Required tests

Add dedicated regressions for:
- final singleton child behaves like silence
- non-final singleton child is rejected
- behavior is consistent across `current`, `cached`, `threshold_obl`, and
  `regular` where applicable

### Exit criteria

Phase 1 is done only when:
- the agreed semantics are implemented
- the old one-state diagnostic failures disappear
- all new tests are committed and passing

## Phase 2: Stabilize The Cached Backend

### Objective

Turn the crash fix into a durable stability guarantee and audit for the same
class of lifetime bug elsewhere in the cached implementation.

### Fixed failure

The resolved bug was:
- references into `bags` and `obls` were kept across calls to `intern_bag(...)`
  and `intern_obl(...)`
- those functions can grow the underlying `std::vector`
- vector growth invalidated the references
- later writes became heap use-after-free

### Required debugging plan

1. Keep the minimal repeated-call reproducer and add it to the repo as a test.

2. Add defensive checks in the cached builder:
   - bounds assertions on bag and obligation IDs
   - assertions that `step_next` and `step_cache` tables match alphabet size
   - assertions around state-map insertion and lookup invariants

3. Audit the cached builder for any remaining references or iterators that can
   survive vector growth.

4. Keep sanitizer coverage on the cached regression set.

### Likely stabilization work items

These are the next places to audit for the same bug class:
- interned obligation storage
- interned bag storage
- bag-step cache vectors
- any helper that returns references into `bags` or `obls`
- any future optimization that adds references or iterators into interning
  arenas

### Required tests

Add repeat-use regressions for:
- repeated cached flatten on the same `NestedAutomaton`
- repeated cached flatten on freshly reloaded `NestedAutomaton` objects
- alternating `current` and `cached` calls in one process
- repeated runs on:
  - `deep_nondet_binary.txt`
  - `nondet_child_binary.txt`
  - `resource_n3_k2.txt`
  - `max_merge_bug_complete.txt`

### Exit criteria

Phase 2 is done only when:
- the fixed use-after-free has a committed regression test
- the repeat-use regression suite passes reliably
- sanitizer runs are clean on the cached regression set

## Phase 3: Build The Production Gate

### Objective

Make promotion testable and repeatable. The production decision should come from
a standing gate, not from one-off manual confidence.

### Required correctness gate

The gate should compare:
- `cached`
- `current`
- `threshold_obl`
- `regular`

on at least these families:
- existing bundled correctness witnesses
- overlap witness
- silent-parent cases
- immediate discharge cases
- stuck/background control cases
- singleton-child semantics cases
- resource family slices

Checks to record:
- emptiness result for `Inf`
- emptiness result for `LimInf`
- flat state count
- flat transition count

### Required performance gate

Record at least:
- end-to-end elapsed time
- peak RSS
- flat state count
- flat transition count

Use at least three families:
- small sparse correctness cases
- resource family
- one dense-frontier-oriented family

### Required acceptance thresholds

Recommended promotion thresholds:
- zero mismatches against accepted reference cases
- no crash in repeat-use or sanitizer runs
- no more than `5%` slowdown versus `current` on the small sparse suite overall
- at least `10-15%` win versus `current` on the medium resource cases overall
- no substantial peak-RSS regression on the largest saved cases

### Required artifacts

Keep generating a folder like this one with:
- raw CSVs
- summary tables
- mismatch reports
- a short interpretation file

That makes future regressions auditable.

## Phase 4: Refactor For Maintainability

### Objective

Make `cached` cheap enough to maintain that using it as production does not
create a second independent semantics engine.

### Required refactors

1. Share semantic helpers.
   Any child classification, liveness decision, or epoch logic that is purely
   semantic should live in a shared helper used by both `current` and `cached`.

2. Isolate representation-specific code.
   Keep only these parts backend-specific:
   - obligation/bag storage representation
   - memoization tables
   - visited-state key representation

3. Keep instrumentation optional and local.
   The stats path should stay available for benchmarking, but it should not make
   the production path harder to read.

4. Document invariants next to the code.
   The cached backend needs short comments for:
   - sentinel ID values
   - bag interning invariants
   - canonicalization rules for `y0`/`y1`
   - meaning of cache-hit counters

### Recommended file structure

Prefer this direction:
- keep public entry points in `NestedAutomaton.cpp`
- move large cached-builder helpers into a dedicated private implementation
  section or helper file if the function body becomes too large
- keep tests and probes separate from production code

### Exit criteria

Phase 4 is done only when:
- semantic fixes no longer require parallel edits in multiple backends
- the cached code has clear invariants and a bounded responsibility surface

## Phase 5: Controlled Rollout

### Objective

Change the live default without losing observability or rollback safety.

### Rollout steps

1. Add a temporary production switch.
   Examples:
   - compile-time macro
   - internal runtime flag
   - test-only dispatcher override

2. Keep `current` callable during rollout.
   Do not delete it immediately. It remains the rollback backend and comparison
   point until the new default has been exercised enough.

3. Make `cached` the default dispatcher target.
   Flip `flatten_MinMax_Inf(...)` to `cached` only after phases 1-4 complete.

4. Keep post-flip comparison tooling.
   Retain at least:
   - the backend probe
   - the differential harness
   - one benchmark harness

5. Delay cleanup.
   Delete dead experimental scaffolding only after the default has remained
   stable across another round of regression and performance runs.

### Rollback plan

If any of the following appears after the switch:
- correctness mismatch
- repeat-use crash
- significant performance regression on sparse cases
- major memory regression

then rollback is:
- restore dispatcher to `current`
- keep `cached` behind the comparison switch
- save the failing artifact bundle and debug there

## Concrete Task List

### Workstream A: Semantics

- implement child classification helper
- patch singleton-child handling in all relevant backends
- add singleton-child tests
- replace accidental aborts with explicit outcomes

### Workstream B: Stability

- create repeated-call cached regression test
- keep sanitizer coverage on the fixed path
- audit for remaining vector-arena invalidation risks
- rerun repeat-use suite

### Workstream C: Validation

- expand the probe-driven matrix
- capture new artifact folder after each major fix
- compare `cached` to `current` and `regular`

### Workstream D: Promotion

- add dispatcher switch
- make `cached` default
- keep `current` as rollback path until one full post-promotion validation pass

## Suggested Order Of Execution

1. Implement singleton-child semantics and tests.
2. Add repeated-call cached reproducer.
3. Commit the fixed cached crash regression and run sanitizers on it.
4. Re-run the full differential matrix.
5. Re-run resource and dense-frontier performance suites.
6. Compare new results to this evaluation folder.
7. If the gates pass, add the dispatcher switch and flip default to `cached`.
8. Run one more full validation bundle after the flip.
9. Only then consider pruning unused experimental code.

## Promotion Decision Rule

Promote `cached` only if all of these are true at the same time:
- semantics gaps are closed
- repeated-use crash fix is covered by committed regression tests
- sanitizer regressions are clean
- correctness matches accepted references
- performance remains neutral-to-better on sparse cases
- performance is clearly better on the medium resource family

If any one of those is false, `cached` stays experimental.

## Bottom Line

The right production plan is:
- treat `cached` as the successor to `current`
- stabilize it first
- promote it only through a measured gate

The wrong production plan is:
- flip the dispatcher immediately because the first benchmark averages look good

The benchmark data justifies investment. It does not yet justify promotion.
