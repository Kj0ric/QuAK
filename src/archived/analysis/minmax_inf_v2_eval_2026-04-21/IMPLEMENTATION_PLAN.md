# Implementation Plan

## Target Direction

The best production target is:
- keep the live obligation-based `current` design
- evolve it toward the `cached` sparse/interned implementation
- do not switch the live path to `threshold_obl` by default yet

Reason:
- the instrumentation shows that the sparse backend really is paying avoidable
  bag-copy and repeated-step costs
- the cached prototype removes exactly those costs
- the performance data is already promising
- the repeat-use use-after-free bug has now been fixed
- MMThr is competitive, but not clearly dominant across the measured families

## Step 1: Fix Semantics Gaps First

Before promotion, implement the intended one-state-child rule explicitly:
- final one-state child: treat as silent
- non-final one-state child: reject as invalid with a clear error

Required follow-up:
- fix the current `map::at` abort on one-state final children
- make the parser or constructor reject the invalid non-final singleton case
  intentionally, not accidentally
- add dedicated regression tests for both cases

## Step 2: Lock In Cached Stability

The main repeat-use crash has been root-caused and patched, but that fix still
needs committed regression coverage and a small audit for the same bug class.

Required work:
- add a dedicated regression test that repeatedly calls the backend on the same
  `NestedAutomaton` instance
- add a regression that would have caught vector-arena reference invalidation
- verify stability on at least:
  - `deep_nondet_binary.txt`
  - `nondet_child_binary.txt`
  - `resource_n3_k2.txt`

Promotion should not proceed until this fix is locked in by tests.

## Step 3: Keep the Shared Semantics, Not Separate Semantics

The cached backend should stay semantically tied to the live shared threshold
construction.

Practical guidance:
- reuse shared child-table and liveness builders where possible
- avoid duplicating semantic corner-case logic in two places
- if a semantic fix is needed, add it in a shared helper when possible, then
  consume it from both `current` and `cached`

This reduces semantic drift between the experimental and live paths.

## Step 4: Add Promotion Gates

Do not flip the live dispatcher until all of the following are true:
- no mismatches against `regular` on the confirmed bundled cases
- no repeat-use crash under dedicated stability tests
- one-state-child semantics implemented and tested
- performance remains at least neutral on small sparse cases
- performance is clearly better on the medium resource cases where current cost
  is dominated by bag-copy and state-map work

## Step 5: Keep `threshold_obl` as a Comparison Backend

`threshold_obl` should remain available:
- as a specialized comparison point
- as a correctness cross-check
- as a candidate for families that are demonstrably bitset-friendly

But the data here does not justify making it the default replacement for
`flatten_MinMax_Inf`.

## Step 6: Recommended Implementation Order

1. Implement explicit one-state-child handling and add tests.
2. Add dedicated regressions for the fixed cached repeat-use bug.
3. Re-run the same matrices in this folder.
4. If the results stay consistent, make `cached` the new experimental primary
   candidate.
5. Only then consider switching the live dispatcher from `current` to `cached`.

## Bottom Line

The stitched v2 file is partly right:
- the live sparse obligation algorithm is the correct foundation
- its implementation still has real constant-factor waste

The best path is not:
- “just switch to MMThr”

The best path is:
- “finish and stabilize the cached sparse backend, then promote it if it keeps
  passing differential checks and remains faster on the meaningful cases”
