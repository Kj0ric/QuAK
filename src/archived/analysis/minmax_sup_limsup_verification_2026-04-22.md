# Min/Max x Sup/LimSup Verification Notes

Date: 2026-04-22

## Scope

This note records the current verification status of the nested emptiness
implementations for:

- `Sup x {Max_f, Min_f}`
- `LimSup x {Max_f, Min_f}`

for both:

- the current `quak` library (`src/NestedAutomaton.cpp`)
- the previous `quak_old_v2` library (`src/NestedAutomaton_OLD_V2.cpp`)

## Main Results

### Current backend

Evidence:

- `./build-review/test_emptiness_correctness` passed `464/464`
- `./build-review/minmax_sup_fix_compare` reported:
  - `queries=1200`
  - `oracle_queries=1104`
  - `mismatches=0`

Interpretation:

- the specialized `flatten_MinMax_Sup(...)` path agrees with end-to-end
  `isNonEmpty(...)` on all sampled `Sup/LimSup x {Max_f, Min_f}` queries
- on the cases where `flatten_regular(...)` is a trustworthy oracle, the current
  specialized path also agrees with that oracle

### Previous backend (`old_v2`)

Evidence:

- the same-symbol spawn bug was fixed in `src/NestedAutomaton_OLD_V2.cpp`
- `./build-review/minmax_sup_old_v2_probe` now reports `mismatches=0/8`
- but `./build-review/minmax_sup_fix_compare_old_v2` still reports:
  - `queries=1200`
  - `oracle_queries=1104`
  - `mismatches=10`

All remaining mismatches are on:

- `src/tests/correctness_tests/inputs/max_merge_bug_complete.txt`

and all are false positives of the old-v2 `Sup` backend:

- `Sup, Max_f, threshold in {3,4,5,6,8,10}`
- `Sup, Min_f, threshold in {0.5,1,1.5,2}`

## Important Oracle Caveat

`flatten_regular(...)` is not a reliable oracle for the blocker cases:

- `sup_background_obligation_blocker.txt`
- `complete_nonterminating_background.txt`

On those inputs, both the current and old-v2 specialized backends reject, while
`flatten_regular(...)` accepts for low thresholds. The current targeted
correctness tests support the specialized behavior there, so those cases are
excluded from oracle-based comparison in `src/tests/probes/minmax_sup_fix_compare.cpp`.

## Targeted Regressions Added

The current correctness suite now includes:

- current-symbol consumption for initial-final children
- bad-current-symbol rejection
- background-child blocker rejection for thresholds `-1, 0, 0.5, 1`
- overlap/merge regression on `max_merge_bug_complete.txt`

Relevant files:

- `src/tests/correctness_tests/test_emptiness_correctness.cpp`
- `src/tests/correctness_tests/test_correctness_common.h`
- `src/tests/probes/minmax_sup_fix_compare.cpp`

## Current Assessment

### Current backend

No counterexample found in the expanded checks above.

### Previous backend

Still unsound on the overlap/merge class.

The previous implementation appears to preserve too little information when
multiple invocations of the same child coexist in the same local state. The
remaining failures are not same-symbol spawn bugs; they are overlap/merge bugs.

## Overlap / Merge Class

Canonical reproducer:

- `src/tests/correctness_tests/inputs/max_merge_bug_complete.txt`

Shape of the bug:

- the parent creates overlapping invocations of the same child on `a` and `b`
- both invocations can end up in child state `s1` at the same time
- old-v2 stores only:
  - one activation bit per flattened child state
  - one tracking bit per flattened child state
  - one distinguished witness `(child,state,y)`
- this cannot represent "background token at `s1`" and "witness token at `s1`"
  as two distinct obligations when they collide

Observed consequence:

- old-v2 reports false positives on this file for `Sup`
- `Max_f` mismatches at thresholds `{3,4,5,6,8,10}`
- `Min_f` mismatches at thresholds `{0.5,1,1.5,2}`

Root-cause location in old-v2:

- `src/NestedAutomaton_OLD_V2.cpp`
- background traversal skips the witness source location:
  - `explore_global_selection_min_max_supremum(...)`
- the state encoding has no coexistence class for "background + witness at the
  same child state"

This is the remaining correctness gap in `quak_old_v2` after the same-symbol
spawn fix.

## Commands Run

```bash
cmake -S . -B build-review -DCMAKE_BUILD_TYPE=Debug
cmake --build build-review --target \
  minmax_sup_fix_compare \
  minmax_sup_fix_compare_old_v2 \
  minmax_sup_old_v2_probe \
  test_emptiness_correctness -j

./build-review/minmax_sup_old_v2_probe
./build-review/minmax_sup_fix_compare
./build-review/minmax_sup_fix_compare_old_v2
./build-review/test_emptiness_correctness
```
