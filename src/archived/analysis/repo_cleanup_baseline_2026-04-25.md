# Repository Cleanup Baseline

Date: 2026-04-25

This note records the Phase 0 baseline for `analysis/repo_cleanup_plan.md`.
No cleanup edits have been made yet.

## Verification

Commands run from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target tests experiments -j
ctest --test-dir build --output-on-failure
cmake --build build --target quak-nested -j
```

Result:

- Configure succeeded.
- `tests` and `experiments` targets built successfully.
- CTest passed: 15/15 tests.
- `quak-nested` built successfully.

## Active CMake Targets

Project/library/CLI targets:

- `quak`
- `quak-private`
- `quakso`
- `quak-nested`

Aggregate targets:

- `tests`
- `experiments`
- `examples`

Registered test targets:

- `test_flatten_regular`
- `test_flatten_avg_summinus`
- `test_flatten_sumplusminus_sup`
- `test_flatten_sumplusminus_inf`
- `test_flatten_minmax_sup`
- `test_flatten_minmax_inf`
- `test_emptiness_universality`
- `test_pseudo_determinization`
- `test_synchronization`
- `test_auxiliary_functions`
- `test_sanity_all`
- `test_emptiness_correctness`
- `test_split_final_differential`
- `test_sum_sup_witness_edge_cases`
- `test_universality_correctness`

Experiment/probe targets:

- `quak-experiment-single`
- `sum_inf_fix_compare`
- `sum_sup_fix_compare`
- `sum_sup_fix_compare_witness_cached`
- `sum_sup_fix_compare_full`
- `sum_sup_fix_compare_witness_cached_full`
- `sum_inf_backend_probe`
- `sum_inf_stability_stress`
- `minmax_sup_fix_compare`
- `minmax_sup_fix_compare_cached`
- `minmax_sup_fix_compare_witness_cached`
- `minmax_sup_resource_probe`

Example targets:

- `example1_basic`
- `example2_value_functions`
- `example3_response_time`

## Registered CTest Tests

CTest currently registers 15 tests:

- `test_flatten_regular`
- `test_flatten_avg_summinus`
- `test_flatten_sumplusminus_sup`
- `test_flatten_sumplusminus_inf`
- `test_flatten_minmax_sup`
- `test_flatten_minmax_inf`
- `test_emptiness_universality`
- `test_pseudo_determinization`
- `test_synchronization`
- `test_auxiliary_functions`
- `test_sanity_all`
- `test_emptiness_correctness`
- `test_split_final_differential`
- `test_sum_sup_witness_edge_cases`
- `test_universality_correctness`

## Existing Archive Directories

- `src/archived/`
- `src/archived/experiments_oblbag/`
- `src/archived/experiments_oblbag/results/`
- `src/archived/experiments_oblbag/test_automata/`

## Generated Artifact Deletion Manifest

Root in-source CMake artifacts currently present:

- `CMakeCache.txt`
- `CMakeFiles/`
- `CTestTestfile.cmake`
- `Makefile`
- `Testing/`
- `cmake_install.cmake`

`src/` in-source CMake/library artifacts currently present:

- `src/CMakeFiles/`
- `src/Makefile`
- `src/cmake_install.cmake`
- `src/libquak-private.a`
- `src/libquak.a`
- `src/libquak.so`
- `src/libquak_old_v2.a`

Root compiled executables currently present:

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

Additional root generated library artifacts currently present:

- `libquak-private.a`
- `libquak.a`
- `libquak.so`

These root libraries are generated artifacts but are not explicitly listed in
Phase 2 of the cleanup plan. Confirm whether to delete them with the Phase 2
artifact purge.

## Historical Source Archive Candidates

Present in `src/`:

- `src/NestedAutomaton_OLD.cpp`
- `src/NestedAutomaton_OLD_V2.cpp`
- `src/NestedAutomaton_mainRepo.cpp`
- `src/NestedAutomaton_threshold_extremal.cpp`
- `src/NestedAutomaton.cpp.pre_shared_threshold_backend.bak`
- `src/main.cppOld`

Active-reference scan:

```bash
rg -n "NestedAutomaton_OLD|NestedAutomaton_mainRepo|NestedAutomaton_threshold_extremal|OLD_V2|old_v2|threshold_extremal_impl" \
  CMakeLists.txt src/CMakeLists.txt src/NestedAutomaton.cpp src/NestedAutomaton.h src/tests
```

Result: no matches.

## Notes

- A `.git/` directory exists, but `git status --short` reports the visible
  project files as untracked. Continue using explicit manifests for deletion
  and move passes.
- `build/` is the verified out-of-tree build and should be kept through the
  first cleanup passes.
- `build-asan/`, `build-release/`, `build-review/`, `build-sum-asan/`, and
  `build-supmax-bench/` are outside the Phase 2 deletion list and should be
  left alone unless explicitly cleaned later.
