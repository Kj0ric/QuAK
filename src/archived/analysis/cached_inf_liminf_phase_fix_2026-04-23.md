# Cached `Inf/LimInf` Phase Fixes

## Summary

On 2026-04-23, both cached `Inf/LimInf` backends were brought back onto the same
acceptance discipline as the fixed live threshold-extremal path:

- `flatten_MinMax_Inf_cached_impl(...)`
- `flatten_SumPlusMinus_Inf_cached_impl(...)`

The bug was the same phase-lag acceptance failure:

- the cached state remembered only a pulse-style `phase` plus `epoch_nonempty`
- epoch rotation used `current.P2 == 0 || P2_step == 0`
- a state was accepting only when the destination itself was a final pulse with
  a final parent

That loses the required memory of "the parent was final earlier in this epoch,
and the tracked obligations emptied later."

## Fix

Both cached backends now use the same remembered two-phase condition as
`flatten_regular(...)` and the fixed shared threshold-extremal helper:

- cached `MinMax_Inf`: [src/NestedAutomaton.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:9352)
- cached `SumPlusMinus_Inf`: [src/NestedAutomaton.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:9917)

Concretely:

- state keys store only `acc_phase_t phase`
- `phase_after_current` is computed from the current state
- epoch boundary is `current.P2 == 0u`
- accepting states satisfy `phase == ACC_WAIT_P2EMPTY && P2 == 0u`

## Regression Coverage

Permanent regressions now include:

- cached `MinMax_Inf` phase-lag cases in
  [src/tests/probes/minmax_inf_fix_compare.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/probes/minmax_inf_fix_compare.cpp:108)
- cached `SumPlusMinus_Inf` phase-lag cases in
  [src/tests/probes/sum_inf_fix_compare.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/probes/sum_inf_fix_compare.cpp:84)
- permanent `SumMinus` phase-lag fixture:
  [src/tests/correctness_tests/inputs/phase_parent_final_then_empty_summinus.txt](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/correctness_tests/inputs/phase_parent_final_then_empty_summinus.txt:1)

## Verification

### Cached `MinMax_Inf`

Dedicated probe after relinking against the rebuilt current library:

```text
phase_parent_final_then_empty.txt
  Inf    x Max_f: cached=1 regular=1
  Inf    x Min_f: cached=1 regular=1
  LimInf x Max_f: cached=1 regular=1
  LimInf x Min_f: cached=1 regular=1
```

Broader comparison via a direct build of `src/tests/probes/minmax_inf_fix_compare.cpp`
shows the new phase-lag cases fixed for `current`, `cached`, and `v2`.

There is still an unrelated pre-existing mismatch on `test_empt_2_impossible`
for both `current` and `cached`; this fix did not target that issue.

### Cached `SumPlusMinus_Inf`

Focused phase probes after relinking:

```text
phase_parent_final_then_empty.txt
  Inf    x SumPlus  @ 1:  current=1 cached=1
  LimInf x SumPlus  @ 1:  current=1 cached=1

phase_parent_final_then_empty_summinus.txt
  Inf    x SumMinus @ -3: current=1 cached=1
  LimInf x SumMinus @ -3: current=1 cached=1
```

Broader comparison:

```text
./build-review/sum_inf_fix_compare
```

now matches on all listed cases, including the new `SumPlus` and `SumMinus`
phase-lag regressions.

## Conclusion

The cached `Inf/LimInf` phase-lag bug is fixed for:

- `Min_f`
- `Max_f`
- `SumPlus`
- `SumMinus`

The fix was acceptance-only surgery. The cached bag/obligation builders were not
rewritten.
