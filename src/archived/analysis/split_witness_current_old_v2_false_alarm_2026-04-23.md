# Split-Witness Current `OLD_V2` False Alarm

Date: 2026-04-23

## Scope

This note is **not** an argument that the archived split-witness bug was fake.
That bug is real and is already documented in:

- `analysis/split_witness_issue2_limsup_bug_2026-04-23.md`
- `src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt`

The false alarm was narrower:

- the claim that the **current patched** `OLD_V2` split-witness backend in this
  checkout was still refuted by the stronger one-child counterexample
- and the related assumption that `backend_split_sup_extract.cpp` already
  reflected the current patched code

Both of those claims turned out to be wrong.

## Bottom Line

What was wrong:

- I mixed **stale standalone artifacts** with **current source**.
- I treated `backend_split_sup_extract.cpp` and
  `backend_cached_inf_and_split_sup_extract.cpp` as if they were current.
- I also used stale binaries (`analysis/issue2_compare_probe` and
  `./quak-nested`) as if they represented the current checkout.

What turned out to be true after retesting correctly:

- the current patched `src/NestedAutomaton_OLD_V2.cpp` rejects the stronger
  counterexample correctly
- the current live backend also rejects it correctly
- the extract files were stale copies and were **not** part of any build target

So the false alarm was about the **current patched backend**. It was **not** a
refutation of the already-known archived `issue2` bug.

## The Wrong Diagnosis

The mistaken diagnosis was:

- "the stronger counterexample still breaks the current patched split-witness
  backend"

That diagnosis was based on two bad assumptions:

1. `backend_split_sup_extract.cpp` was assumed to match the current patched
   `OLD_V2` implementation.
2. The probe binaries already present in the repo were assumed to be linked
   against the current libraries.

Both assumptions were false.

## What Was Actually Stale

### 1. The extract files were stale

The current patched source is:

- `src/NestedAutomaton_OLD_V2.cpp`

In the fixed code, the split background traversal continues within the same
child after processing one active local state:

- `src/NestedAutomaton_OLD_V2.cpp:5444`
- `src/NestedAutomaton_OLD_V2.cpp:5460`
- `src/NestedAutomaton_OLD_V2.cpp:5469`

Those calls use:

- `explore_global_selection_min_max_sup_split(child_id, child_state_id + 1, data)`

By contrast, the extracted standalone copies still had the stale buggy jump:

- `backend_split_sup_extract.cpp:282`
- `backend_split_sup_extract.cpp:298`
- `backend_split_sup_extract.cpp:307`
- `backend_cached_inf_and_split_sup_extract.cpp:2429`
- `backend_cached_inf_and_split_sup_extract.cpp:2445`
- `backend_cached_inf_and_split_sup_extract.cpp:2454`

Those stale copies were still using:

- `explore_global_selection_min_max_sup_split(child_id + 1, 0, data)`

That is exactly the old "at most one active local state per child on a letter"
bug.

### 2. The stale extract files were not even build inputs

A search over the build files found no references to:

- `backend_split_sup_extract.cpp`
- `backend_cached_inf_and_split_sup_extract.cpp`

So these files were misleading standalone extracts, but they were not the code
that `build-review/src/libquak_old_v2.a` was built from.

### 3. The earlier binaries were stale

The timestamps already show the problem:

- `analysis/issue2_compare_probe` was built at `2026-04-23 10:25`
- `build-review/src/libquak_old_v2.a` was built at `2026-04-23 10:49`
- `src/NestedAutomaton_OLD_V2.cpp` was modified at `2026-04-23 10:48`

So `analysis/issue2_compare_probe` predated the current patched `OLD_V2`
library and could not be trusted as evidence about the current backend.

Likewise for the live path:

- `./quak-nested` was built at `2026-04-21 13:08`
- `build-review/src/libquak.a` was built at `2026-04-22 17:28`
- `src/NestedAutomaton.cpp` was modified at `2026-04-22 15:22`

So `./quak-nested` was also stale relative to the current live library.

## How The Correct Retest Was Done

After identifying the stale artifacts, the retest used only the current
`build-review` libraries.

### Current old-v2 split-witness retest

I rebuilt the comparison probe against:

- `build-review/src/libquak_old_v2.a`
- `build-review/src/libquak-private.a`

using the existing source:

- `analysis/issue2_compare_probe.cpp`

This produced a fresh one-off binary:

- `/tmp/current_old_v2_compare`

### Current live-path retest

I built a small one-off probe that directly calls:

- `NestedAutomaton::isNonEmpty(...)`

and linked it against:

- `build-review/src/libquak.a`
- `build-review/src/libquak-private.a`

This produced:

- `/tmp/current_live_query`

The stronger user counterexample was encoded temporarily as a nested-automaton
input under `analysis/`, used for the retest, and then removed.

## Current-Build Results

### Stronger user counterexample

Fresh current old-v2 probe:

```text
analysis/tmp_user_split_background_counterexample.txt inf=Sup fin=Max_f threshold=1 split=0 regular=0
analysis/tmp_user_split_background_counterexample.txt inf=LimSup fin=Max_f threshold=1 split=0 regular=0
```

Fresh current live probe:

```text
analysis/tmp_user_split_background_counterexample.txt inf=Sup fin=Max_f threshold=1 live=0
analysis/tmp_user_split_background_counterexample.txt inf=LimSup fin=Max_f threshold=1 live=0
```

So the stronger example does **not** refute the current patched backend.

### Saved archived `issue2` fixture

Fresh current old-v2 probe:

```text
src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt inf=Sup fin=Max_f threshold=1 split=1 regular=1
src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt inf=LimSup fin=Max_f threshold=1 split=0 regular=0
```

Fresh current live probe:

```text
src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt inf=Sup fin=Max_f threshold=1 live=1
src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt inf=LimSup fin=Max_f threshold=1 live=0
```

So the current patched old-v2 backend also handles the previously saved
`issue2` fixture correctly.

## What Remains True

The following remains true:

- the **historical archived** split-witness backend had a real overlap-loss bug
- the stronger user example is a valid witness against that stale unpatched
  behavior

What is no longer true:

- that the **current patched** `OLD_V2` in this checkout is still broken on
  that example

That was the false alarm.

## Repository Cleanup Performed

To remove the misleading mismatch between extracts and source, the stale
standalone copies were updated to match the current patched `OLD_V2` logic:

- `backend_split_sup_extract.cpp`
- `backend_cached_inf_and_split_sup_extract.cpp`

Specifically, the stale `child_id + 1` recursion in the split-witness
background traversal was replaced with the current patched
`child_state_id + 1` recursion.

Temporary analysis files used only for retesting were deleted afterwards.

## Practical Rule Going Forward

For this checkout, the trustworthy sources of truth are:

- current source files under `src/`
- binaries or probes rebuilt against `build-review/src/libquak*.a`

The following should **not** be used as evidence about the current backend
unless rebuilt:

- `analysis/issue2_compare_probe`
- `./quak-nested`
- any standalone extracted backend file that is not part of a build target
