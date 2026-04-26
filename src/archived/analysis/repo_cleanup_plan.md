# Repository Cleanup Plan

Date: 2026-04-25

This plan is for tidying the current QuAK workspace without changing algorithm
behavior. The cleanup should be done in small, verifiable passes. Each pass
should leave the project buildable and should avoid broad refactors unless the
previous structural noise has already been removed.

## Current Problems

The repository currently mixes production source, generated build outputs,
historical source snapshots, research fragments, compiled executables,
registered tests, ad-hoc probes, benchmark outputs, and analysis notes in the
same visible areas.

The largest sources of noise are:

- In-source CMake artifacts at the repository root:
  - `CMakeCache.txt`
  - `CMakeFiles/`
  - `CTestTestfile.cmake`
  - `Makefile`
  - `Testing/`
  - `cmake_install.cmake`
- In-source CMake/build artifacts under `src/`:
  - `src/CMakeFiles/`
  - `src/Makefile`
  - `src/cmake_install.cmake`
  - `src/libquak*.a`
  - `src/libquak.so`
- Root-level executables from old builds:
  - `quak-nested`
  - `quak-experiment-single`
  - `example*`
  - `test_*`
  - `sum_*`
  - `minmax_*`
- Historical source snapshots still visible as active-looking source:
  - `src/NestedAutomaton_OLD.cpp`
  - `src/NestedAutomaton_OLD_V2.cpp`
  - `src/NestedAutomaton_mainRepo.cpp`
  - `src/NestedAutomaton_threshold_extremal.cpp`
  - `src/NestedAutomaton.cpp.pre_shared_threshold_backend.bak`
  - `src/main.cppOld`
- Loose root research fragments:
  - `backend_cached_inf_and_split_sup_extract.cpp`
  - `backend_split_sup_extract.cpp`
  - `minmax_sup_cached_mmthr_fragment.cpp`
  - `replacement_block.cpp`
  - `minmax_inf_threshold_obl_snippets.txt`
  - several one-off markdown notes
- `src/tests/` contains a mix of registered tests, optional probes, benchmark
  helpers, and markdown reports.

## Cleanup Principles

- Do not change flattening semantics while tidying the tree.
- Keep each pass separately buildable and testable.
- Prefer moving archival material over deleting it when it may contain useful
  fixtures, explanations, or implementation fragments.
- Delete only clearly generated artifacts, compiled binaries, and rebuildable
  CMake output.
- Keep production code under `src/`, registered tests under
  `src/tests/sanity_tests/` and `src/tests/correctness_tests/`, and optional
  probes under a clearly named probes or analysis tools directory.
- Keep `src/archived/` for source fragments that are intentionally not built.
- Use explicit deletion manifests because this checkout has no `.git` directory.

## Phase 0: Establish Baseline

Goal: know the starting point before deleting or moving anything.

Tasks:

- Run the current verification suite:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build --target tests experiments -j`
  - `ctest --test-dir build --output-on-failure`
  - `cmake --build build --target quak-nested -j`
- Save a short inventory note with:
  - active CMake targets
  - registered CTest tests
  - optional experiment/probe targets
  - archive directories
- Record the exact generated artifacts to delete before deleting them.

Success criteria:

- The baseline build and tests pass before structural cleanup starts.
- The generated-artifact deletion list is explicit.

## Phase 1: Add Guardrails

Goal: stop accidental in-source builds and keep future generated files ignored.

Tasks:

- Add an in-source build guard near the top of `CMakeLists.txt`:

```cmake
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are disabled. Use: cmake -S . -B build")
endif()
```

- Expand `.gitignore` to cover:
  - `/build-*/`
  - root example executables
  - root test executables
  - root probe/compare/stress executables
  - generated static/shared libraries
  - Python caches
- Keep `/build/` ignored, but do not delete it during the same pass unless the
  user explicitly wants a completely clean rebuild.

Verification:

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
- Confirm `cmake .` fails with the new guard if tested in a disposable copy or
  after removing existing in-source CMake output.

Success criteria:

- Out-of-tree configure still works.
- Future in-source configure is blocked.

## Phase 2: Remove Generated Artifacts

Goal: remove generated output from the source tree so the remaining files are
source, docs, inputs, or intentional archives.

Delete these root in-source CMake outputs:

- `CMakeCache.txt`
- `CMakeFiles/`
- `CTestTestfile.cmake`
- `Makefile`
- `Testing/`
- `cmake_install.cmake`

Delete these `src/` in-source CMake outputs:

- `src/CMakeFiles/`
- `src/Makefile`
- `src/cmake_install.cmake`
- `src/libquak-private.a`
- `src/libquak.a`
- `src/libquak.so`
- `src/libquak_old_v2.a`

Delete these root compiled executables if present:

- `example1_basic`
- `example2_value_functions`
- `example3_response_time`
- `quak-experiment-single`
- `quak-nested`
- `minmax_sup_fix_compare_cached`
- `minmax_sup_fix_compare_witness_cached`
- `minmax_sup_old_v2_probe`
- `minmax_sup_resource_probe`
- `sum_inf_backend_probe`
- `sum_inf_fix_compare`
- `sum_inf_stability_stress`
- `test_auxiliary_functions`
- `test_emptiness_correctness`
- `test_emptiness_universality`
- `test_flatten_avg_summinus`
- `test_flatten_minmax_inf`
- `test_flatten_minmax_sup`
- `test_flatten_regular`
- `test_flatten_sumplusminus_inf`
- `test_flatten_sumplusminus_sup`
- `test_pseudo_determinization`
- `test_sanity_all`
- `test_synchronization`
- `test_universality_correctness`

Keep for now:

- `build/`, because it is the current out-of-tree build used for verification.
- `build-asan/`, `build-release/`, `build-review/`, `build-sum-asan/`,
  `build-supmax-bench/` until the user decides whether stale build trees should
  be deleted or kept for comparison.

Verification:

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
- `cmake --build build --target tests experiments -j`
- `ctest --test-dir build --output-on-failure`

Success criteria:

- Generated root/source-tree artifacts are gone.
- The out-of-tree build still works.

## Phase 3: Archive Historical Source Snapshots

Goal: make it impossible to confuse historical implementations with active
production code while preserving them for reference.

Move these files into `src/archived/legacy_sources/`:

- `src/NestedAutomaton_OLD.cpp`
- `src/NestedAutomaton_OLD_V2.cpp`
- `src/NestedAutomaton_mainRepo.cpp`
- `src/NestedAutomaton_threshold_extremal.cpp`
- `src/NestedAutomaton.cpp.pre_shared_threshold_backend.bak`
- `src/main.cppOld`

Add `src/archived/legacy_sources/README.md` explaining:

- these files are intentionally not built
- they are historical snapshots or comparison baselines
- active implementations live in `src/NestedAutomaton.cpp`
- old-v2 is not an active backend

Update references only if they are active code or CMake references. Do not edit
old analysis notes in this pass unless they become actively misleading.

Verification:

- `rg -n "NestedAutomaton_OLD|NestedAutomaton_mainRepo|NestedAutomaton_threshold_extremal" CMakeLists.txt src/CMakeLists.txt src/NestedAutomaton.cpp src/NestedAutomaton.h src/tests`
- `cmake --build build --target tests experiments -j`
- `ctest --test-dir build --output-on-failure`

Success criteria:

- Historical snapshots no longer appear in active source locations.
- CMake does not reference archived snapshots.

## Phase 4: Rehome Root Research Fragments

Goal: keep the repository root focused on project entry points and user-facing
documentation.

Move implementation fragments into `src/archived/fragments/`:

- `backend_cached_inf_and_split_sup_extract.cpp`
- `backend_split_sup_extract.cpp`
- `minmax_sup_cached_mmthr_fragment.cpp`
- `replacement_block.cpp`

Move text snippets and research-only notes into `analysis/` or
`src/archived/fragments/` depending on whether they describe behavior or contain
code to preserve:

- `minmax_inf_threshold_obl_snippets.txt`
- `analysis_minmax_sup_scalable_backend.md`
- `inf_liminf_min_max_new.md`
- `inf_liminf_min_max_new_v2.md`
- `nested_emptiness_review.md`
- `nested_flattening_current_vs_old_v2.md`
- `sup-sumplus-replacement.md`
- `general_note.md`
- `assumptions.md`, if it is research context rather than user-facing docs

Review before moving:

- `experiment.py`
- `experiment_response_max.py`
- `csv_to_latex_figures.py`

These may belong under `analysis/tools/` if they are not user-facing scripts.

Verification:

- Root directory contains only expected top-level files:
  - build configuration
  - docs
  - license
  - Dockerfile
  - scripts intentionally documented as root entry points
- `cmake --build build --target tests experiments -j`

Success criteria:

- No loose code fragments remain in the root.
- Root markdown files are either user-facing docs or moved into analysis.

## Phase 5: Normalize Tests, Probes, And Experiments

Goal: separate registered correctness/sanity tests from optional investigation
tools.

Keep registered tests here:

- `src/tests/sanity_tests/`
- `src/tests/correctness_tests/`

Move ad-hoc tools into `src/tests/probes/` or `analysis/tools/`.

Candidate probe/tool files:

- `src/tests/sum_inf_fix_compare.cpp`
- `src/tests/sum_sup_fix_compare.cpp`
- `src/tests/sum_inf_backend_probe.cpp`
- `src/tests/sum_inf_stability_stress.cpp`
- `src/tests/minmax_inf_backend_probe.cpp`
- `src/tests/minmax_inf_fix_compare.cpp`
- `src/tests/minmax_inf_resource_single.cpp`
- `src/tests/minmax_sup_fix_compare.cpp`
- `src/tests/minmax_sup_resource_probe.cpp`
- `src/tests/minmax_sup_old_v2_probe.cpp`
- `src/tests/inspect_monotone_current.cpp`
- `src/tests/inspect_threshold_extremal.cpp`
- `src/tests/benchmark_oblbag.cpp`

Candidate reports/notes to move under `analysis/`:

- `src/tests/flatten_MinMax_Inf_benchmark_report.md`
- `src/tests/interval_based_sumplusminus.md`

Recommended target layout:

```text
src/tests/
  sanity_tests/
  correctness_tests/
  probes/
  benchmarks/
```

or:

```text
analysis/tools/
analysis/reports/
src/tests/sanity_tests/
src/tests/correctness_tests/
```

Preferred decision:

- Put C++ tools that still compile as CMake experiment targets under
  `src/tests/probes/`.
- Put non-compiled reports/scripts under `analysis/`.

Update CMake paths after moving files.

Verification:

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
- `cmake --build build --target tests experiments -j`
- `ctest --test-dir build --output-on-failure`

Success criteria:

- `src/tests/` layout communicates what is registered and what is optional.
- All experiment/probe targets still build.

## Phase 6: Promote Useful Differential Cases

Goal: keep correctness value from old differential tools without keeping every
comparison harness forever.

Approach:

- Inspect the archived and active probe cases.
- Select compact cases that previously caught cached or witness-cached bugs.
- Convert them into registered correctness tests.
- Prefer tests that assert semantic results against public/default wrappers.
- Keep direct `cached` or `witness_cached` checks only where they test the
  intended backend contract.

Candidate sources:

- `src/archived/minmax_sup_threshold_extremal_split_witness_archive.cpp`
- `src/tests/probes/sum_sup_fix_compare.cpp`
- `src/tests/probes/sum_inf_fix_compare.cpp`
- `src/tests/probes/minmax_sup_fix_compare.cpp`
- `src/tests/probes/minmax_inf_fix_compare.cpp`

Candidate correctness areas:

- `test_split_final_differential.cpp`
- `test_sum_sup_witness_edge_cases.cpp`
- `test_emptiness_correctness.cpp`

Verification:

- Add tests first, then run:
  - `cmake --build build --target tests -j`
  - `ctest --test-dir build --output-on-failure`

Success criteria:

- Important differential coverage is preserved as registered tests.
- Old probes can be treated as optional or archived.

## Phase 7: Simplify CMake

Goal: make build targets auditable.

Tasks:

- Add helper functions:

```cmake
function(add_quak_test TEST_SOURCE)
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})
  target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(${TEST_NAME} PUBLIC quak quak-private)
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_target_properties(${TEST_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endfunction()
```

```cmake
function(add_quak_experiment TARGET_NAME SOURCE)
  add_executable(${TARGET_NAME} ${SOURCE})
  target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(${TARGET_NAME} PUBLIC quak quak-private)
  set_target_properties(${TARGET_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endfunction()
```

- Use explicit lists for:
  - sanity tests
  - correctness tests
  - probes/experiments
  - examples
- Consider moving test/probe target declarations into:
  - `src/tests/CMakeLists.txt`
  - `examples/CMakeLists.txt`
- Do not change target names in the same pass unless required.

Verification:

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
- `cmake --build build --target tests experiments examples -j`
- `ctest --test-dir build --output-on-failure`

Success criteria:

- Adding a new test or probe requires touching one obvious list.
- No old-v2 or archived target names remain in active CMake.

## Phase 8: Split `NestedAutomaton.cpp`

Goal: reduce the main implementation file after the repository tree is clean.

Do this only after phases 1 through 7 are stable.

Suggested split:

```text
src/NestedAutomaton.cpp
src/NestedAutomaton_regular.cpp
src/NestedAutomaton_sum_sup.cpp
src/NestedAutomaton_sum_inf.cpp
src/NestedAutomaton_minmax_sup.cpp
src/NestedAutomaton_minmax_inf.cpp
src/NestedAutomaton_child_tables.cpp
src/NestedAutomaton_threshold_helpers.cpp
```

Alternative split if private helpers are too entangled:

```text
src/NestedAutomaton.cpp
src/NestedAutomatonFlattenRegular.cpp
src/NestedAutomatonFlattenSum.cpp
src/NestedAutomatonFlattenMinMax.cpp
src/NestedAutomatonFlattenShared.cpp
```

Rules:

- Keep public declarations in `NestedAutomaton.h` stable.
- Move code without changing behavior first.
- Avoid renaming helper functions during the move.
- Compile after each slice.
- Only after the split should naming cleanup happen.

Verification after each slice:

- `cmake --build build --target quak -j`
- `cmake --build build --target tests -j`
- Targeted tests for the moved algorithm family.

Final verification:

- `cmake --build build --target tests experiments examples quak-nested -j`
- `ctest --test-dir build --output-on-failure`

Success criteria:

- `NestedAutomaton.cpp` is smaller and mostly contains orchestration/public
  wrappers.
- Algorithm-family files are discoverable.
- No behavior changed.

## Phase 9: Document Final Shape

Goal: make the cleaned structure self-explanatory.

Add or update:

- `README.md`: short build/test commands and project map.
- `src/archived/README.md`: archive policy.
- `src/tests/README.md`: registered tests vs probes.
- `analysis/README.md`: what belongs in analysis and what should not.

Also document active flattening dispatch:

- `SumPlus/SumMinus + Sup/LimSup` uses `flatten_SumPlusMinus_Sup_witness_cached`.
- `SumPlus/SumMinus + Inf/LimInf` uses `flatten_SumPlusMinus_Inf_cached`.
- `Max_f/Min_f + Sup/LimSup` uses `flatten_MinMax_Sup_witness_cached`.
- `Max_f/Min_f + Inf/LimInf` uses `flatten_MinMax_Inf_cached`.

Success criteria:

- A new contributor can tell where production code, tests, probes, archives,
  generated results, and analysis notes belong.

## Suggested Execution Order

1. Phase 0: baseline verification.
2. Phase 1: guardrails.
3. Phase 2: generated artifact purge.
4. Phase 3: historical source archive.
5. Phase 4: root fragment cleanup.
6. Phase 5: tests/probes layout.
7. Phase 6: differential-to-correctness promotion.
8. Phase 7: CMake simplification.
9. Phase 8: split `NestedAutomaton.cpp`.
10. Phase 9: docs.

## Stop Points

Stop and reassess if:

- A file planned for deletion is referenced by active CMake.
- A moved probe no longer builds.
- A historical source file is still needed for a current comparison.
- Any registered CTest test fails after a move-only pass.
- The cleanup requires semantic changes to production code.

## Final Verification Checklist

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target tests experiments examples quak-nested -j
ctest --test-dir build --output-on-failure
```

Scan:

```bash
rg -n "OLD_V2|old_v2|NestedAutomaton_OLD|NestedAutomaton_mainRepo|threshold_extremal_impl" \
  CMakeLists.txt src/CMakeLists.txt src/NestedAutomaton.cpp src/NestedAutomaton.h src/tests
```

Expected final state:

- No generated CMake output in the source tree.
- No root-level compiled executables.
- Historical source snapshots live under `src/archived/`.
- Registered tests and optional probes are separated.
- CMake target lists are easy to audit.
- The active build and all registered tests pass.
