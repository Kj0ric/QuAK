# Sum Cached Stability Pass

## Goal

This note records the dedicated stability pass for the experimental cached
`Inf/LimInf x {SumPlus, SumMinus}` backend.

The purpose of this pass was narrower than the earlier correctness and
performance work:
- stress repeated in-process use of the experimental cached backend
- alternate backend call order to catch order-sensitive bugs
- exercise the shared stats path as part of the loop
- run the same workload under `ASan+UBSan`

Production dispatch was **not** changed for this pass.

## Experimental Tooling

Added experimental stress executable:
- [src/tests/probes/sum_inf_stability_stress.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/probes/sum_inf_stability_stress.cpp:1)

Build hook:
- [CMakeLists.txt](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/CMakeLists.txt:121)

What the stress executable does:
- runs both `current` and `cached` in the same process
- compares emptiness result, flattened state count, and flattened transition
  count on every check
- alternates backend order every iteration
- enables the shared experiment-stats path on a rotating subset of checks
- supports a `core` and `extended` case profile

This is intentionally a stability tool, not a performance benchmark.

## Workloads

### Core profile

The `core` profile includes `12` cases:
- the saved SumPlus/SumMinus correctness cases from `sum_inf_fix_compare`
- `child_pump_loop` at the heavier `SumMinus` threshold `-20`
- `response_n5_k5` from the negative response-time family

### Extended profile

The `extended` profile includes the `core` cases plus:
- `response_n6_k8`
- `response_n8_k8`

These larger negative response-time instances are included to exercise the
cached backend beyond the tiny regime without making the sanitizer pass
prohibitively slow.

## Normal-Build Pass

Commands run:
- `./test_flatten_sumplusminus_inf`
- `./sum_inf_fix_compare`
- `./sum_inf_stability_stress 100 core`
- `./sum_inf_stability_stress 10 extended`

Saved outputs:
- [raw/test_flatten_sumplusminus_inf_after_stability_tool.txt](./raw/test_flatten_sumplusminus_inf_after_stability_tool.txt)
- [raw/sum_inf_fix_compare_after_stability_tool.txt](./raw/sum_inf_fix_compare_after_stability_tool.txt)
- [raw/sum_inf_stability_core_100.txt](./raw/sum_inf_stability_core_100.txt)
- [raw/sum_inf_stability_extended_10.txt](./raw/sum_inf_stability_extended_10.txt)

Results:
- `test_flatten_sumplusminus_inf`: `18/18` pass
- `sum_inf_fix_compare`: all saved cached-vs-current comparisons match
- `sum_inf_stability_stress 100 core`: `PASS total_checks=2400`
- `sum_inf_stability_stress 10 extended`: `PASS total_checks=280`

Interpretation:
- no crash
- no mismatch
- no order-sensitive failure when alternating `current` and `cached`
- no failure when the shared stats path is enabled during the loop

## Sanitizer Pass

Separate sanitized build directory:
- `build-sum-asan/`

Configuration:
- `Debug`
- `IPO` disabled
- `-fsanitize=address,undefined -fno-omit-frame-pointer`

Commands run:
- `./build-sum-asan/test_flatten_sumplusminus_inf`
- `./build-sum-asan/sum_inf_fix_compare`
- `./build-sum-asan/sum_inf_stability_stress 20 core`
- `./build-sum-asan/sum_inf_stability_stress 3 extended`

Environment:
- `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1`
- `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`

Saved outputs:
- [raw/test_flatten_sumplusminus_inf_asan.txt](./raw/test_flatten_sumplusminus_inf_asan.txt)
- [raw/sum_inf_fix_compare_asan.txt](./raw/sum_inf_fix_compare_asan.txt)
- [raw/sum_inf_stability_core_20_asan.txt](./raw/sum_inf_stability_core_20_asan.txt)
- [raw/sum_inf_stability_extended_3_asan.txt](./raw/sum_inf_stability_extended_3_asan.txt)

Results:
- `test_flatten_sumplusminus_inf`: `18/18` pass under sanitizers
- `sum_inf_fix_compare`: all saved comparisons match under sanitizers
- `sum_inf_stability_stress 20 core`: `PASS total_checks=480`
- `sum_inf_stability_stress 3 extended`: `PASS total_checks=84`

I also scanned the sanitizer logs for:
- `AddressSanitizer`
- `UndefinedBehaviorSanitizer`
- `LeakSanitizer`
- `runtime error:`
- `ERROR:`

No matches were found.

## Bottom Line

For the dedicated stability criterion, the experimental cached Sum backend now
looks clean:
- repeated in-process use is stable on the saved workload
- alternating `current`/`cached` order does not expose stale-state bugs
- the shared stats path does not destabilize the run
- `ASan+UBSan` did not report memory or UB findings on the exercised matrix

What this stability pass does **not** prove:
- it does not make a statement about default-promotion policy by itself
- it does not replace broader randomized testing
- it does not resolve the separate singleton-child semantic policy question

But for the specific question of repeat-use and sanitizer stability, this pass
is a positive result.

In other words:
- stability is no longer a live blocker for the experimental Sum cached backend
- if promotion is blocked, it has to be blocked by some other criterion
