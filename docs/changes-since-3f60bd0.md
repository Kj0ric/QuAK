# Changes Since `3f60bd0`

## Scope

This note summarizes all updates in the range `3f60bd061..3cd84045a` (from `2026-02-10` to `2026-04-04`).

High-level totals:

- 18 commits in the range
- 25 files changed
- 1113 insertions, 34 deletions
- One merge commit: `75d6c9be0` (`Merge pull request #6: Correctness testing and bug fixes for AE prep`)

Most of the work falls into five themes:

1. Correctness fixes for nested automata, especially `SumPlus` and `SumMinus`
2. Better validation and clearer CLI support rules
3. Major expansion of regression and backward-compatibility tests
4. Build-system updates so the new tests are compiled and runnable in CI
5. CI workflow cleanup and platform coverage improvements

## 1. Mixed-Sign `SumPlus` and `SumMinus` Semantics

The most important functional change is in [src/NestedAutomaton.cpp](/home/ege/Documents/repositories/QuAK/src/NestedAutomaton.cpp). The code now explicitly supports nested automata whose child automata contain both positive and negative weights when using `SumPlus` or `SumMinus`.

### What changed

Commit `66581d7b5` introduced three new helper paths:

- `validateNested()`
- `childWeightsNeedProjection(value_function_t finVal)`
- `projectChildWeightsForAggregator(value_function_t finVal)`

The new design first checks whether a nested automaton contains child weights whose signs are incompatible with the requested aggregator:

- `SumPlus` needs `|x|` semantics
- `SumMinus` needs `-|x|` semantics

If such weights exist, the implementation now clones the child automata and projects each child weight into the correct sign-normalized form before running the normal decision procedures.

This matters because older logic effectively assumed sign-homogeneous children. On mixed-sign inputs, that could silently compute the wrong child return values and then feed the wrong sequence into `Inf`, `Sup`, `LimInf`, `LimSup`, `LimInfAvg`, or `LimSupAvg`.

### Semantic correction

Commit `996d2725f` tightened the semantics further. Before this fix:

- `SumPlus` behaved like "sum of positive weights"
- `SumMinus` behaved like "negated sum of negative weights"

After the fix, the implementation matches the documented meaning:

- `SumPlus = sum(|w_i|)`
- `SumMinus = -sum(|w_i|)`

This is now reflected in [README.md](/home/ege/Documents/repositories/QuAK/README.md) and exercised by the new `mixed_sign.txt` correctness fixture.

### LimAvg regression fix

Commit `981558cae` fixed a subtle regression introduced by the projection work. The projected automaton was being re-analyzed through a `SumB` shortcut instead of preserving the original `finVal`. That broke `LimInfAvg` and `LimSupAvg` cases by capping child values too early.

The clearest example is the new regression fixture `tc_bug_limavg_sumplus.txt`, where alternating children should produce:

- `SumPlus` sequence `[8, 4, 8, 4, ...]`
- Limiting average `6`

Before the fix, the shortcut truncated part of that behavior and incorrectly made `LimSupAvg + SumPlus` fail at threshold `6`.

## 2. Universality and Trivial-Case Handling

Commit `6be58c8ea` improves `NestedAutomaton::isUniversal()` for `SumPlus` and `SumMinus`.

### Why this was needed

Some universality queries are mathematically trivial:

- `SumPlus` is always non-negative
- `SumMinus` is always non-positive

That means:

- if `finVal == SumPlus` and `x <= 0`, universality is immediately `true`
- if `finVal == SumMinus` and `x > 0`, universality is immediately `false`

Before this guard existed, the code still tried to reduce these cases to `SumB`, which could produce a negative effective bound and then fail in `flatten_regular()`.

### Net effect

The code now answers these cases directly and avoids unnecessary flattening work and avoidable runtime failures.

## 3. CLI Support Matrix Corrections

Several commits corrected the CLI layer in [src/quak-nested-main.cpp](/home/ege/Documents/repositories/QuAK/src/quak-nested-main.cpp), which had drifted away from what the backend actually supported.

### Removed an outdated non-emptiness restriction

Commit `81383032c` removed a stale guard that rejected `SumMinus` non-emptiness queries unless the infinite aggregator was `LimInfAvg` or `LimSupAvg`.

That restriction was no longer valid. The backend already handled more cases, especially through monotonic flattening paths, but the CLI still blocked them before execution.

### Enabled nested universality for `SumPlus` and `SumMinus`

Commit `94cba36c1` removed another outdated check that forbade `SumPlus` and `SumMinus` for nested universality.

After this change, nested `universal` supports:

- `Max_f`, `Min_f`, `SumB`, `SumPlus`, `SumMinus`
- with `Inf`, `Sup`, `LimInf`, and `LimSup`

Average-based universality remains unsupported, and the CLI still rejects `LimInfAvg` and `LimSupAvg` in that mode.

### Added an explicit rejection for an actually unsupported combination

Commit `dac0532ef` added a parse-time error for `LimInfAvg + SumPlus` in nested non-emptiness.

This is the opposite of the earlier fixes: instead of removing a stale restriction, it adds a correct one. The backend does not support that combination, so the CLI now reports the problem immediately with a message pointing users to `docs/CLI.md`.

### Documentation updates

[docs/CLI.md](/home/ege/Documents/repositories/QuAK/docs/CLI.md) and [README.md](/home/ege/Documents/repositories/QuAK/README.md) were updated so the documented support matrix matches the actual code again.

## 4. Parser and Nested-Input Validation

This range also hardens nested-input validation so malformed automata fail early and predictably.

### Child automata must have transitions

Commit `080bfcfe2` updates [src/Parser.cpp](/home/ege/Documents/repositories/QuAK/src/Parser.cpp) to reject child automata with no transitions.

Why this matters:

- child initial states are inferred from the first transition
- a child with no transitions therefore has no determinable initial state
- letting that file through would defer the failure to a less obvious point in analysis

The parser now fails immediately with a direct explanation.

### Child automata may not contain `SILENT` transitions

Commit `66581d7b5` adds `validateNested()` in `NestedAutomaton`, and one of its checks is that non-dummy child automata must not contain `SILENT` transitions.

That rule was already implicit in the model, but it is now enforced at runtime with an error message that identifies the offending child index.

### Other robustness fixtures

The new error-handling tests introduce fixtures for:

- undefined child indices
- `SILENT` transitions inside a child
- single-state nested inputs
- large alphabets

The goal is not only to catch invalid files, but also to confirm that unusual but valid inputs do not crash the flattening code.

## 5. Test Suite Expansion

This is the largest testing update in the range.

### New CLI-based correctness tests

Commit `b4efd54d3` adds [src/tests/correctness_tests/test_non_nested_backward_compat.cpp](/home/ege/Documents/repositories/QuAK/src/tests/correctness_tests/test_non_nested_backward_compat.cpp).

This is important because the recent work is heavily nested-focused, while QuAK still supports non-nested CLI commands. The new test covers:

- scalar queries such as `non-empty`, `top-value`, `bottom-value`, `constant`, `safe`, `live`
- comparison commands such as `isIncluded`, `isIncludedBool`, `isEquivalent`, `isEquivalentBool`
- decomposition-style commands such as `safetyComponent`, `livenessComponent`, and `decompose`

The file does not just check exit codes. It also verifies key output strings and compares generated component files so `decompose` stays consistent with the dedicated commands.

### New error-handling test binary

Commit `66581d7b5` adds [src/tests/correctness_tests/test_error_handling.cpp](/home/ege/Documents/repositories/QuAK/src/tests/correctness_tests/test_error_handling.cpp).

This exercises three layers of failure handling:

- CLI argument validation
- parser-level structural validation
- nested-runtime validation through `validateNested()`

It also includes smoke tests for edge cases that should succeed without crashing.

### Mixed-sign correctness coverage

Commits `66581d7b5`, `6be58c8ea`, `996d2725f`, and `981558cae` expand:

- [test_emptiness_correctness.cpp](/home/ege/Documents/repositories/QuAK/src/tests/correctness_tests/test_emptiness_correctness.cpp)
- [test_universality_correctness.cpp](/home/ege/Documents/repositories/QuAK/src/tests/correctness_tests/test_universality_correctness.cpp)

These additions do three things:

1. add a deterministic mixed-sign automaton where `Max_f`, `Min_f`, `SumB`, `SumPlus`, and `SumMinus` all differ in meaningful ways
2. add alternating-child regression coverage for the LimAvg projection bug
3. add more `SumMinus` LimAvg checks so both `LimSupAvg` and `LimInfAvg` paths are exercised

The non-emptiness correctness suite grows from 460 tests to 492 tests in this range.

### Sup/Max edge-case sanity tests

Commit `1e0c50fc5` strengthens [src/tests/sanity_tests/test_flatten_minmax_sup.cpp](/home/ege/Documents/repositories/QuAK/src/tests/sanity_tests/test_flatten_minmax_sup.cpp) with two targeted fixtures:

- `tc11_sup_max_cycle_true.txt`
- `tc12_sup_max_doomed_false.txt`

The first confirms a real positive case: repeated child calls should make `Sup/Max_f` and `LimSup/Max_f` succeed at threshold `1`.

The second checks the flattening structure more directly. It verifies that doomed overlap states are redirected to the synthetic `@sink@` state, which is how the implementation prunes impossible tracked-child configurations.

### Test helper refactor

Commit `3cd84045a` extracts duplicated shell and file helpers into [src/tests/correctness_tests/test_cli_helpers.h](/home/ege/Documents/repositories/QuAK/src/tests/correctness_tests/test_cli_helpers.h).

This is not a behavior change, but it reduces duplication between the new CLI-driven tests and makes future command-line regression tests easier to write.

## 6. Build-System Changes

[CMakeLists.txt](/home/ege/Documents/repositories/QuAK/CMakeLists.txt) was updated to make the new tests part of the normal test build.

### What changed

- `test_non_nested_backward_compat.cpp` and `test_error_handling.cpp` were added to `CORRECTNESS_TEST_SOURCES`
- those tests now depend on `quak-nested`
- they receive `QUAK_NESTED_PATH` as a compile definition so they can invoke the right binary
- the `tests` custom target now depends on `quak-nested` explicitly

### Why it matters

The new tests execute the CLI, not just internal library functions. Without these build dependencies, CI could compile the test binary but still fail because the CLI executable was absent or not locatable.

## 7. Memory and Cleanup Fixes

Commit `080bfcfe2` also contains two small but useful cleanup changes in [src/NestedAutomaton.cpp](/home/ege/Documents/repositories/QuAK/src/NestedAutomaton.cpp):

- a temporary finals set in `synchronizeChildren()` was changed from a leaked heap allocation to a scoped object, then later simplified further
- `flatten_Avg_SumMinus()` now deletes `ffinals` before returning

These are not user-visible feature changes, but they tighten the implementation around paths that are now being exercised more heavily by the new tests.

## 8. CI and Workflow Updates

The workflow configuration under `.github/workflows/` was significantly cleaned up.

### Main build-and-test workflow

Commit `b1c53f661` fixes an important CI gap:

- CI now triggers on both `main` and `playground`
- the workflow explicitly builds the `tests` target before running `ctest`

That second point matters because many QuAK tests are marked `EXCLUDE_FROM_ALL`; they will not exist unless the `tests` target is built first.

The same commit also adds a missing `<algorithm>` include in [src/tests/sanity_tests/test_common.h](/home/ege/Documents/repositories/QuAK/src/tests/sanity_tests/test_common.h), which was needed for newer GCC builds.

### Platform coverage

Commit `14a33d3db` expands CI to macOS and marks Windows as `continue-on-error`. That gives more signal from the matrix without making Windows flakiness block the rest of the pipeline.

### Workflow naming and sanitizer template

Commits `ed352c043` and `9aea777ab`:

- rename the main workflow file to [ci-build-and-test.yml](/home/ege/Documents/repositories/QuAK/.github/workflows/ci-build-and-test.yml)
- rename the workflow itself to `CI (Build and Test)`
- add [memory-sanitizers.yml.template](/home/ege/Documents/repositories/QuAK/.github/workflows/memory-sanitizers.yml.template)

The sanitizer file is intentionally inactive. It is a template for optional AddressSanitizer runs and can be enabled later by renaming it to a real workflow filename.

### CI retrigger commits

Commits `d9d7d8d57` and `6766ce9da` do not change product behavior. They exist to retrigger or restore workflow execution after CI configuration edits.

## 9. Commit-by-Commit Quick Reference

- `b1c53f661` - Fix CI triggers and ensure tests are built before `ctest`
- `d9d7d8d57` - CI retrigger only
- `6766ce9da` - CI retrigger after workflow re-enable
- `81383032c` - Remove outdated CLI block on `SumMinus` non-emptiness
- `14a33d3db` - Add macOS CI and make Windows non-blocking
- `080bfcfe2` - Reject child automata without transitions and fix small leaks
- `b4efd54d3` - Add non-nested CLI backward-compatibility tests
- `1e0c50fc5` - Add focused `Sup/Max_f` sanity and sink-pruning tests
- `ed352c043` - Rename workflow display name and add sanitizer template
- `9aea777ab` - Rename workflow file
- `94cba36c1` - Allow nested universality with `SumPlus` and `SumMinus`
- `66581d7b5` - Add mixed-sign support and nested validation
- `6be58c8ea` - Add trivial universality cases and mixed-sign universality tests
- `996d2725f` - Correct `SumPlus` and `SumMinus` to absolute-value semantics
- `dac0532ef` - Reject unsupported `LimInfAvg + SumPlus` at CLI parse time
- `981558cae` - Fix LimAvg regression in mixed-sign projection recursion
- `75d6c9be0` - Merge pull request #6
- `3cd84045a` - Refactor shared CLI test helpers into a common header

## Bottom Line

This range is primarily a correctness-hardening pass for nested quantitative automata.

The key result is that nested `SumPlus` and `SumMinus` are now better defined, better validated, and much better tested, especially on mixed-sign children and LimAvg paths. The CLI support matrix now matches the backend more closely, malformed nested inputs are rejected earlier, non-nested functionality has fresh regression coverage, and CI now builds and runs the relevant tests in a more realistic way.
