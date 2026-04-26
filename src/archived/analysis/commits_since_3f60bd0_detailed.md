# Detailed Commit Inventory Since `3f60bd0`

This document inventories every commit in `QuAK REPO` from base commit
`3f60bd06154d401672b1b0ed9fad9ef3db506d5b` through
`0bbce0fd97620b31bb2a96b3c8e370825d3e41b1`.

Source commands used:

```bash
git -C "QuAK REPO" log --reverse --numstat --summary --find-renames 3f60bd0..HEAD
git -C "QuAK REPO" show <commit>
git -C "QuAK REPO" diff --stat 3f60bd0..HEAD
```

Aggregate range summary:

- 20 commits in the range.
- 30 files changed in the final aggregate diff.
- 1606 insertions and 37 deletions in the final aggregate diff.
- Two CI retrigger commits have no tree changes.
- One merge commit has no additional tree changes beyond the merged branch.

## Quick Index

| Commit | Date | Subject | Net files |
| --- | --- | --- | --- |
| `b1c53f661` | 2026-04-02 | fix CI: trigger on playground branch, build tests before running ctest | 2 |
| `d9d7d8d57` | 2026-04-02 | trigger CI | 0 |
| `6766ce9da` | 2026-04-02 | trigger CI after re-enabling workflow | 0 |
| `81383032c` | 2026-04-02 | fix: remove CLI guard incorrectly blocking SumMinus+extremal emptiness | 1 |
| `14a33d3d` | 2026-04-02 | add macOS, continue-on-error for Windows | 1 |
| `080bfcfe2` | 2026-04-02 | fix: reject child automata without transitions and free NQA leaks | 2 |
| `b4efd54d3` | 2026-04-02 | test: cover non-nested CLI commands | 2 |
| `1e0c50fc5` | 2026-04-02 | test: add Sup/Max edge-case tests | 4 |
| `ed352c043` | 2026-04-02 | ci: rename main workflow and add sanitizer template | 2 |
| `9aea777ab` | 2026-04-02 | ci: rename main workflow file | 1 rename |
| `94cba36c1` | 2026-04-03 | fix CLI: allow universality check for nested automata with finVal SumPlus and SumMinus | 1 |
| `66581d7b5` | 2026-04-03 | fix: support mixed-sign sumplus/summinus and add nested validation | 13 |
| `6be58c8ea` | 2026-04-03 | fix: add trivial cases for isUniversal SumPlus/SumMinus; add mixed-sign universality tests | 2 |
| `996d2725f` | 2026-04-03 | fix: use sum of absolute values for SumPlus/SumMinus definitions | 6 |
| `dac0532ef` | 2026-04-03 | fix CLI: reject LimInfAvg+SumPlus for non-emptiness check at parse time | 1 |
| `981558cae` | 2026-04-04 | fix: recurse with finVal, not SumB, in mixed-sign projection for LimAvg cases | 4 |
| `75d6c9be0` | 2026-04-04 | Merge pull request #6: Correctness testing and bug fixes for AE prep | 0 |
| `3cd84045a` | 2026-04-04 | refactor: extract shared CLI test helpers into test_cli_helpers.h | 3 |
| `892658bbd` | 2026-04-25 | minor ssync | 2 |
| `0bbce0fd9` | 2026-04-22 authored, 2026-04-25 committed | fix: reject mixed-sign children for LimAvg+SumPlus/SumMinus by default | 7 |

## Commit Details

### `b1c53f661` - fix CI: trigger on playground branch, build tests before running ctest

Files:

- `.github/workflows/cmake-multi-platform.yml`: 5 insertions, 2 deletions.
- `src/tests/sanity_tests/test_common.h`: 1 insertion.

Exact changes:

- Extends the CI workflow branch trigger set so the `playground` branch is included.
- Adds an explicit test-build step before `ctest`, preventing CI from running `ctest` before excluded test executables have been built.
- Adds `#include <algorithm>` to `src/tests/sanity_tests/test_common.h`, needed by newer GCC versions for uses such as `std::all_of`.

Merge relevance:

- Low semantic risk.
- Keep this unless replacing the workflow entirely.
- The `<algorithm>` include should be retained or independently provided by the merged test helper.

### `d9d7d8d57` - trigger CI

Files:

- No tree changes.

Exact changes:

- Empty/no-op commit used only to retrigger CI.

Merge relevance:

- No file content to merge.

### `6766ce9da` - trigger CI after re-enabling workflow

Files:

- No tree changes.

Exact changes:

- Empty/no-op commit used only to retrigger CI.

Merge relevance:

- No file content to merge.

### `81383032c` - fix: remove CLI guard incorrectly blocking SumMinus+extremal emptiness

Files:

- `src/quak-nested-main.cpp`: 7 deletions.

Exact changes:

- Removes a nested CLI validation block that rejected `non-empty ... SumMinus ...` unless `VALF` was `LimInfAvg` or `LimSupAvg`.
- After the commit, nested non-emptiness requests with `SumMinus` are allowed to reach the backend for `Inf`, `Sup`, `LimInf`, and `LimSup` as well.

Deleted behavior:

- The old parse-time error was:
  `SumMinus finite aggregator only supports LimInfAvg or LimSupAvg.`

Merge relevance:

- Important support-matrix correction.
- When merging, do not reintroduce the old CLI restriction.

### `14a33d3d` - add macOS, continue-on-error for Windows

Files:

- `.github/workflows/cmake-multi-platform.yml`: 9 insertions, 1 deletion.

Exact changes:

- Expands the CI matrix to include macOS.
- Marks Windows as `continue-on-error`, so Windows failures provide signal but do not block the whole workflow.

Merge relevance:

- Workflow-only.
- Keep if the target repository uses GitHub Actions.

### `080bfcfe2` - fix: reject child automata without transitions and free NQA leaks

Files:

- `src/NestedAutomaton.cpp`: 11 insertions, 7 deletions.
- `src/Parser.cpp`: 3 insertions.

Exact changes:

- In `NestedAutomaton::synchronizeChildren()`:
  - Removes a leaked heap allocation of a temporary finals set.
  - Uses a scoped `SetStd<State*>` while copying final-state flags onto rebuilt parent states.
- In `NestedAutomaton::flatten_Avg_SumMinus()`:
  - Deletes `ffinals` before constructing and returning the flattened `Automaton`.
- In `readNestedFile()`:
  - Adds a child validation check after parsing:
    non-dummy child parsers with an empty `initial` are rejected because the initial state is inferred from the first transition.

New parser failure condition:

- A child automaton with no transitions now fails with:
  `A child automaton has no transitions (initial state cannot be determined). Check the automaton description .txt file.`

Merge relevance:

- The memory cleanup is safe to keep.
- The parser rule is semantically relevant. If the newer outer parser intentionally supports edge-less non-dummy children, this needs a conscious decision; otherwise keep the rejection.

### `b4efd54d3` - test: cover non-nested CLI commands

Files:

- `CMakeLists.txt`: 8 insertions, 1 deletion.
- `src/tests/correctness_tests/test_non_nested_backward_compat.cpp`: new file, 183 insertions.

Exact changes:

- Registers a new correctness test executable `test_non_nested_backward_compat`.
- Adds a dependency from that test executable to `quak-nested`.
- Defines `QUAK_NESTED_PATH="$<TARGET_FILE:quak-nested>"` so the test calls the built CLI binary, not an assumed path.
- Adds `quak-nested` and `test_non_nested_backward_compat` to the `tests` custom target.
- Adds a CLI regression test covering:
  - scalar commands: `non-empty`, `top-value`, `bottom-value`, `constant`, `safe`, `live`;
  - comparison commands: `isIncluded`, `isIncludedBool`, `isEquivalent`, `isEquivalentBool`;
  - component commands: `safetyComponent`, `livenessComponent`, `decompose`.
- The component tests write temp files, check that they are non-empty, and verify that `decompose` outputs match the dedicated safety/liveness component commands after canonicalizing line order.

Merge relevance:

- Keep the test intent.
- In the outer tree, this should be registered through the existing `add_quak_test()` helper rather than duplicating upstream's older per-test CMake boilerplate.

### `1e0c50fc5` - test: add Sup/Max edge-case tests

Files:

- `src/tests/sanity_tests/inputs/tc11_sup_max_cycle_true.txt`: new file, 30 insertions.
- `src/tests/sanity_tests/inputs/tc12_sup_max_doomed_false.txt`: new file, 34 insertions.
- `src/tests/sanity_tests/test_common.h`: 2 insertions.
- `src/tests/sanity_tests/test_flatten_minmax_sup.cpp`: 65 insertions.

Exact changes:

- Adds `tc11_sup_max_cycle_true.txt`:
  - parent repeatedly invokes child 1 on a four-symbol cycle;
  - expected `isNonEmpty(Sup, Max_f, 1) = true`;
  - expected `isNonEmpty(Sup, Max_f, 2) = false`;
  - expected `isNonEmpty(LimSup, Max_f, 1) = true`.
- Adds `tc12_sup_max_doomed_false.txt`:
  - constructs a case where getting value `1` leaves an overlapping child obligation doomed;
  - expected `isNonEmpty(Sup, Max_f, 1) = false`;
  - expected `isNonEmpty(LimSup, Max_f, 1) = false`.
- Adds `SUP_MAX_CYCLE_TRUE` and `SUP_MAX_DOOMED_FALSE` paths to `TestFiles`.
- Adds tests:
  - `test_isNonEmpty_Sup_Max_cycle_true()`;
  - `test_isNonEmpty_LimSup_Max_cycle_true()`;
  - `test_flatten_MinMax_Sup_doomed_state_redirects_to_sink()`.
- The structural test checks that the flattened automaton contains `@sink@` and that a doomed tracked state redirects to it.

Merge relevance:

- Keep the fixtures if the outer cached MinMax Sup backend still has the same expected semantics.
- The exact structural assertion evolved in later work: upstream eventually checks for any redirect to `@sink@`, while the outer tree currently has its own backend-regression fixtures. Treat this as a test-intent merge, not a blind copy.

### `ed352c043` - ci: rename main workflow and add sanitizer template

Files:

- `.github/workflows/cmake-multi-platform.yml`: 1 insertion, 1 deletion.
- `.github/workflows/memory-sanitizers.yml.template`: new file, 31 insertions.

Exact changes:

- Renames the workflow display name.
- Adds an inactive memory-sanitizer workflow template.
- The sanitizer template is not a live workflow because it ends with `.template`.

Merge relevance:

- Workflow-only.
- Safe to keep as optional CI documentation/template.

### `9aea777ab` - ci: rename main workflow file

Files:

- Rename: `.github/workflows/cmake-multi-platform.yml` to `.github/workflows/ci-build-and-test.yml`.

Exact changes:

- Pure file rename with 100% similarity.

Merge relevance:

- If the target tree still has `.github/workflows/cmake-multi-platform.yml`, prefer the renamed upstream path unless the outer workflow intentionally uses a different name.

### `94cba36c1` - fix CLI: allow universality check for nested automata with finVal SumPlus and SumMinus

Files:

- `src/quak-nested-main.cpp`: 2 insertions, 5 deletions.

Exact changes:

- Removes the parse-time rejection of `SumPlus` and `SumMinus` for nested `universal`.
- Keeps the rejection of `LimInfAvg` and `LimSupAvg` for nested `universal`.
- Updates the in-code support comment to say nested universality supports:
  `Max_f`, `Min_f`, `SumB`, `SumPlus`, and `SumMinus` with `Sup`, `Inf`, `LimSup`, and `LimInf`.
- Also fixes a missing trailing newline.

Merge relevance:

- Important support-matrix correction.
- Preserve this unless the target backend no longer supports nested universality for `SumPlus`/`SumMinus`.

### `66581d7b5` - fix: support mixed-sign sumplus/summinus and add nested validation

Files:

- `CMakeLists.txt`: 3 insertions, 1 deletion.
- `README.md`: 2 insertions.
- `docs/CLI.md`: 9 insertions, 6 deletions.
- `src/NestedAutomaton.cpp`: 177 insertions, 1 deletion.
- `src/NestedAutomaton.h`: 3 insertions.
- `src/tests/correctness_tests/inputs/mixed_sign.txt`: new file, 31 insertions.
- `src/tests/correctness_tests/inputs/tc_err_silent_in_child.txt`: new file, 14 insertions.
- `src/tests/correctness_tests/inputs/tc_err_undefined_child.txt`: new file, 13 insertions.
- `src/tests/correctness_tests/inputs/tc_large_alphabet.txt`: new file, 35 insertions.
- `src/tests/correctness_tests/inputs/tc_single_state.txt`: new file, 12 insertions.
- `src/tests/correctness_tests/test_correctness_common.h`: 3 insertions.
- `src/tests/correctness_tests/test_emptiness_correctness.cpp`: 131 insertions, 4 deletions.
- `src/tests/correctness_tests/test_error_handling.cpp`: new file, 166 insertions.

Exact code changes:

- Adds private `NestedAutomaton` methods:
  - `validateNested() const`;
  - `childWeightsNeedProjection(value_function_t finVal) const`;
  - `projectChildWeightsForAggregator(value_function_t finVal) const`.
- Adds a static helper `projectChildWeightForAggregator(weight_t value, value_function_t finVal)`.
- At this commit, projection semantics are:
  - `SumPlus`: keep positive weights and replace negative weights with `0`;
  - `SumMinus`: keep negative weights and replace positive weights with `0`.
  - Commit `996d2725f` changes this later to absolute-value semantics.
- `childWeightsNeedProjection()` scans non-dummy children and returns true if:
  - `SumPlus` sees a negative child weight;
  - `SumMinus` sees a positive child weight.
- `projectChildWeightsForAggregator()` clones the parent and every child, rebuilding child alphabet, weights, states, final flags, edges, and initial state while changing the child weights according to the selected finite aggregator.
- `validateNested()` rejects `SILENT` transitions inside non-dummy child automata and identifies the child index in the error.
- `isNonEmpty()` now:
  - calls `validateNested()` first;
  - if a `SumPlus`/`SumMinus` query needs projection, projects child weights and recursively calls `projected->isNonEmpty(infVal, SumB, x, projected_bound)`.
- `isUniversal()` now:
  - calls `validateNested()` first;
  - projects mixed-sign `SumPlus`/`SumMinus` child weights and recursively calls `projected->isUniversal(infVal, SumB, x, effectiveBound)`.

Exact build/test/doc changes:

- Adds `test_error_handling.cpp` to `CORRECTNESS_TEST_SOURCES`.
- Extends `QUAK_NESTED_PATH` compile definitions to `test_error_handling`.
- Adds `test_error_handling` to the `tests` target.
- Updates README and `docs/CLI.md` to state broader `SumPlus`/`SumMinus` support and the unsupported `LimInfAvg + SumPlus` non-empty combination.
- Adds `mixed_sign.txt`, where child path `[3, -2, 4]` distinguishes `Max_f`, `Min_f`, `SumB`, `SumPlus`, and `SumMinus`.
- Adds invalid/edge fixtures:
  - undefined child index;
  - `SILENT` transition in a child;
  - large alphabet;
  - single-state nested input.
- Extends `test_emptiness_correctness.cpp` with mixed-sign expected values and additional LimAvg SumMinus checks.
- Adds CLI-driven error-handling tests for missing `SumB` bound, unsupported universal combinations, undefined child index, child `SILENT`, and valid edge cases.

Merge relevance:

- This is a major semantic commit and must be merged in small pieces.
- Later commits modify the semantics introduced here, so do not port this version alone without `996d2725f`, `981558cae`, and `0bbce0fd9`.

### `6be58c8ea` - fix: add trivial cases for isUniversal SumPlus/SumMinus; add mixed-sign universality tests

Files:

- `src/NestedAutomaton.cpp`: 6 insertions.
- `src/tests/correctness_tests/test_universality_correctness.cpp`: 79 insertions.

Exact code changes:

- Adds two early returns to `NestedAutomaton::isUniversal()`:
  - `finVal == SumPlus && x <= 0` returns `true`;
  - `finVal == SumMinus && x > 0` returns `false`.
- These avoid computing invalid negative `SumB` bounds during the later flattening reduction.

Exact test changes:

- Adds mixed-sign expected values to `test_universality_correctness.cpp`.
- Wires `mixed_sign` into expected-value lookup and file-path lookup.
- Uses the original mixed-sign fixture for `SumMinus`, rather than a negated fixture.
- Adds universal tests for `mixed_sign` across `Inf`, `Sup`, `LimInf`, and `LimSup` with all finite aggregators.

Merge relevance:

- Keep the trivial cases; they are independent of backend implementation details.

### `996d2725f` - fix: use sum of absolute values for SumPlus/SumMinus definitions

Files:

- `README.md`: 2 insertions, 2 deletions.
- `src/NestedAutomaton.cpp`: 11 insertions, 11 deletions.
- `src/tests/correctness_tests/inputs/mixed_sign.txt`: 3 insertions, 3 deletions.
- `src/tests/correctness_tests/test_emptiness_correctness.cpp`: 3 insertions, 3 deletions.
- `src/tests/correctness_tests/test_universality_correctness.cpp`: 3 insertions, 3 deletions.
- `src/tests/sanity_tests/test_flatten_minmax_sup.cpp`: 15 insertions, 13 deletions.

Exact code changes:

- Changes `projectChildWeightForAggregator()`:
  - `SumPlus` now returns `|x|`;
  - `SumMinus` now returns `-|x|`.
- Adds a comment explaining why `Symbol::RESET()`, `Weight::RESET()`, and `State::RESET()` are safe while cloning projected children.
- Simplifies `synchronizeChildren()` final-state copying by setting final flags directly, without the temporary `SetStd`.

Exact documentation/test changes:

- README now defines:
  - `SumPlus`: sum of absolute values of all child-run weights, always non-negative;
  - `SumMinus`: negated sum of absolute values of all child-run weights, always non-positive.
- Updates `mixed_sign.txt` expected values:
  - `SumPlus` changes from `7` to `9`;
  - `SumMinus` changes from `-2` to `-9`.
- Updates matching expected values in emptiness and universality correctness tests.
- Weakens the `test_flatten_MinMax_Sup_doomed_state_redirects_to_sink()` structural assertion:
  - instead of relying on an exact generated state name and exact redirect count;
  - it checks that `@sink@` exists and at least one non-sink state redirects to it.

Merge relevance:

- This is the authoritative version of `SumPlus`/`SumMinus` semantics in this commit range.
- Any merge should use `|x|` and `-|x|`, not the earlier positive-only/negative-only projection from `66581d7b5`.

### `dac0532ef` - fix CLI: reject LimInfAvg+SumPlus for non-emptiness check at parse time

Files:

- `src/quak-nested-main.cpp`: 5 insertions.

Exact changes:

- Adds a nested CLI parse-time rejection for:
  `non-empty LimInfAvg SumPlus <threshold> [bound]`.
- The error says:
  `LimInfAvg + SumPlus is not supported for non-empty. See docs/CLI.md for supported combinations.`

Merge relevance:

- Keep unless the merged backend adds real support for `LimInfAvg + SumPlus`.

### `981558cae` - fix: recurse with finVal, not SumB, in mixed-sign projection for LimAvg cases

Files:

- `src/NestedAutomaton.cpp`: 4 insertions, 2 deletions.
- `src/tests/correctness_tests/inputs/tc_bug_limavg_sumplus.txt`: new file, 34 insertions.
- `src/tests/correctness_tests/test_correctness_common.h`: 2 insertions.
- `src/tests/correctness_tests/test_emptiness_correctness.cpp`: 39 insertions.

Exact code changes:

- In `NestedAutomaton::isNonEmpty()`, changes the mixed-sign projection recursion:
  - old behavior from `66581d7b5`: recurse as `projected->isNonEmpty(infVal, SumB, x, projected_bound)`;
  - new behavior: recurse as `projected->isNonEmpty(infVal, finVal, x, bound)`.
- The reason is that LimAvg-specific algorithms need their original finite aggregator and bound logic after projection.

Exact test changes:

- Adds `tc_bug_limavg_sumplus.txt`.
- The fixture alternates two children:
  - child 1 has weights `+6, -2`, projected `SumPlus = 8`;
  - child 2 has weights `+3, +1`, `SumPlus = 4`;
  - sequence is `[8, 4, 8, 4, ...]`, limiting average `6`.
- Adds `MIXED_SIGN_ALT` path in `test_correctness_common.h`.
- Adds `MixedSignAlt` expected values in `test_emptiness_correctness.cpp`.
- Adds tests for:
  - `mixed_sign_alt, LimSupAvg, SumPlus`;
  - `mixed_sign_alt, LimInfAvg, SumMinus`;
  - `mixed_sign_alt, LimSupAvg, SumMinus`.

Merge relevance:

- The code change is superseded/qualified by `0bbce0fd9`, which rejects these mixed-sign LimAvg paths by default unless compiled with `NORMALIZE_MIXED_SIGN`.
- The regression fixture remains useful for optional normalized builds or documentation of the bug.

### `75d6c9be0` - Merge pull request #6: Correctness testing and bug fixes for AE prep

Files:

- No additional tree changes in the inspected range output.

Exact changes:

- Merge commit for the preceding correctness-testing and bug-fix branch.
- The actual file changes are represented by the merged commits already listed above.

Merge relevance:

- No separate patch to apply.

### `3cd84045a` - refactor: extract shared CLI test helpers into test_cli_helpers.h

Files:

- `src/tests/correctness_tests/test_cli_helpers.h`: new file, 78 insertions.
- `src/tests/correctness_tests/test_error_handling.cpp`: 2 insertions, 69 deletions.
- `src/tests/correctness_tests/test_non_nested_backward_compat.cpp`: 15 insertions, 64 deletions.

Exact changes:

- Adds `test_cli_helpers.h` with reusable CLI-test utilities:
  - `uniquePath()`;
  - `readFile()`;
  - `runCommandExpectSuccess()`;
  - `runCommandExpectFailure()`;
  - `assertContains()`.
- Moves duplicated temp-file and process-running logic out of:
  - `test_error_handling.cpp`;
  - `test_non_nested_backward_compat.cpp`.
- Updates both tests to include `test_cli_helpers.h` and use `using namespace cli_test`.
- Replaces local `runCommand()` calls in `test_non_nested_backward_compat.cpp` with `runCommandExpectSuccess()`.

Merge relevance:

- Keep this refactor if any upstream CLI tests are kept.
- It is also used by `test_smoke.cpp` added later in `0bbce0fd9`.

### `892658bbd` - minor ssync

Files:

- `.gitignore`: 2 insertions, 3 deletions.
- `docs/changes-since-3f60bd0.md`: new file, 296 insertions.

Exact changes:

- Moves `CLAUDE.md` from a dedicated "Claude Code" ignore section into the general legacy/editor-ish ignored list near the bottom.
- Adds `AGENTS.md` to `.gitignore`.
- Adds `docs/changes-since-3f60bd0.md`.
- The new document summarizes the range `3f60bd061..3cd84045a`; it does not include commit `892658bbd` itself or the later `0bbce0fd9` behavior.

Merge relevance:

- `.gitignore` needs review because the outer tree currently has `AGENTS.md`, `CLAUDE.md`, and top-level docs that may be intentionally tracked locally.
- The summary document is useful background but is incomplete relative to upstream `HEAD`.

### `0bbce0fd9` - fix: reject mixed-sign children for LimAvg+SumPlus/SumMinus by default

Files:

- `CMakeLists.txt`: 11 insertions, 1 deletion.
- `docs/assumptions.md`: 20 insertions.
- `src/NestedAutomaton.cpp`: 16 insertions.
- `src/tests/correctness_tests/inputs/tc_err_mixed_sign_limavg.txt`: new file, 15 insertions.
- `src/tests/correctness_tests/test_emptiness_correctness.cpp`: 12 insertions, 17 deletions.
- `src/tests/correctness_tests/test_error_handling.cpp`: 17 insertions.
- `src/tests/correctness_tests/test_smoke.cpp`: new file, 122 insertions.

Exact code changes:

- Adds CMake option:
  `NORMALIZE_MIXED_SIGN`, default `OFF`.
- If enabled, CMake defines `NORMALIZE_MIXED_SIGN`.
- In `NestedAutomaton::isNonEmpty()`:
  - introduces `isLimAvgPath = (infVal == LimSupAvg || infVal == LimInfAvg)`;
  - only applies mixed-sign child projection for LimAvg paths;
  - when `NORMALIZE_MIXED_SIGN` is not defined, fails with a clear `Mixed-sign child weights are not supported for LimAvg+SumPlus/SumMinus` error;
  - when `NORMALIZE_MIXED_SIGN` is defined, prints a warning and uses the projection path.
- Non-LimAvg `Sup`, `LimSup`, `Inf`, and `LimInf` paths no longer use the projection shortcut, because `flatten_SumPlusMinus_Sup/Inf` handle absolute-value normalization internally.

Exact documentation changes:

- Adds child-weight sign requirements to `docs/assumptions.md`.
- Documents that:
  - non-LimAvg paths accept mixed-sign children and normalize internally;
  - LimAvg paths reject mixed-sign children by default;
  - automatic normalization for LimAvg paths requires rebuilding with `-DNORMALIZE_MIXED_SIGN=ON`.

Exact test changes:

- Adds `tc_err_mixed_sign_limavg.txt`, a fixture for runtime rejection of mixed-sign LimAvg `SumPlus`/`SumMinus`.
- Removes default correctness-test execution of mixed-sign LimAvg cases that now intentionally fail.
- Leaves comments pointing those error paths to `test_error_handling::testMixedSignLimAvg()`.
- Adds `testMixedSignLimAvg()` to `test_error_handling.cpp`; it expects failure for:
  `tc_err_mixed_sign_limavg.txt non-empty LimSupAvg SumMinus 0`.
- Adds `test_smoke.cpp`, a CLI smoke test covering representative paths:
  - `flatten_regular`;
  - `flatten_avg_summinus`;
  - `flatten_sumplusminus_sup`;
  - `flatten_sumplusminus_inf`;
  - `flatten_minmax_sup`;
  - `flatten_minmax_inf`;
  - universal query path.
- Registers `test_smoke` in CMake, gives it `QUAK_NESTED_PATH`, and adds it to the `tests` target.

Merge relevance:

- This is the final upstream behavior for mixed-sign LimAvg paths.
- When merging, this commit should be treated as overriding the earlier "always project mixed-sign children" behavior from `66581d7b5`/`981558cae`.

## Aggregate File-Level Result At Upstream HEAD

These are the files that differ in the aggregate upstream range `3f60bd0..HEAD`:

```text
.github/workflows/cmake-multi-platform.yml -> .github/workflows/ci-build-and-test.yml
.github/workflows/memory-sanitizers.yml.template
.gitignore
CMakeLists.txt
README.md
docs/CLI.md
docs/assumptions.md
docs/changes-since-3f60bd0.md
src/NestedAutomaton.cpp
src/NestedAutomaton.h
src/Parser.cpp
src/quak-nested-main.cpp
src/tests/correctness_tests/inputs/mixed_sign.txt
src/tests/correctness_tests/inputs/tc_bug_limavg_sumplus.txt
src/tests/correctness_tests/inputs/tc_err_mixed_sign_limavg.txt
src/tests/correctness_tests/inputs/tc_err_silent_in_child.txt
src/tests/correctness_tests/inputs/tc_err_undefined_child.txt
src/tests/correctness_tests/inputs/tc_large_alphabet.txt
src/tests/correctness_tests/inputs/tc_single_state.txt
src/tests/correctness_tests/test_cli_helpers.h
src/tests/correctness_tests/test_correctness_common.h
src/tests/correctness_tests/test_emptiness_correctness.cpp
src/tests/correctness_tests/test_error_handling.cpp
src/tests/correctness_tests/test_non_nested_backward_compat.cpp
src/tests/correctness_tests/test_smoke.cpp
src/tests/correctness_tests/test_universality_correctness.cpp
src/tests/sanity_tests/inputs/tc11_sup_max_cycle_true.txt
src/tests/sanity_tests/inputs/tc12_sup_max_doomed_false.txt
src/tests/sanity_tests/test_common.h
src/tests/sanity_tests/test_flatten_minmax_sup.cpp
```

## Merge-Planning Notes

- `NestedAutomaton.cpp` should be merged by behavior blocks, not as one file:
  1. validation/projection helpers;
  2. `validateNested()`;
  3. `isNonEmpty()` support-matrix and mixed-sign handling;
  4. `isUniversal()` trivial cases and projection handling;
  5. backend-specific code only after wrapper behavior is clear.
- CMake should preserve the outer tree's helper functions and experiment/probe targets while adding upstream's `NORMALIZE_MIXED_SIGN`, CLI tests, and `test_smoke`.
- Parser changes need a policy decision around edge-less non-dummy child automata because the outer parser has additional `final:` handling that upstream does not.
- CLI changes need a policy decision around witness support because the outer CLI currently differs substantially from upstream in witness-related plumbing.
- Test changes should be merged by intent:
  - keep upstream mixed-sign/error/smoke coverage;
  - keep outer backend regression coverage;
  - avoid making fragile generated-state-name assertions unless the merged backend guarantees those names.
