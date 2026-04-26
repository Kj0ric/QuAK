# Cached Sum `Inf/LimInf` Experimental Implementation

## Scope

This note records the experimental implementation of a cached
`Inf/LimInf x {SumPlus, SumMinus}` backend modeled on the existing cached
Min/Max backend.

Related benchmark note:
- [RESPONSE_NEGATIVE_BENCHMARKS.md](./RESPONSE_NEGATIVE_BENCHMARKS.md)
- [RESPONSE_NEGATIVE_SCALING.md](./RESPONSE_NEGATIVE_SCALING.md)
- [STABILITY_PASS.md](./STABILITY_PASS.md)

The goal was:
- keep the changes separate from production dispatch
- compile the experimental backend
- check correctness before any performance work
- only run performance probes once correctness looked clean

## Code Changes

The production dispatcher was **not** changed.

The main implementation change is a new experimental entry point:
- [src/NestedAutomaton.h](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.h:86)
- [src/NestedAutomaton.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:9542)

Added pieces:
- `NestedAutomaton::flatten_SumPlusMinus_Inf_cached(...)`
- `SumInfCachedBuilder`
- shared `thrext_step_frontier(...)`
- shared `thrext_spawn_frontier(...)`

These are intentionally separate from the live production path:
- `flatten_SumPlusMinus_Inf(...)` still dispatches to the shared threshold
  backend
- no production caller was flipped to the cached Sum implementation

Supporting experimental/test changes:
- [src/tests/sanity_tests/test_common.h](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/sanity_tests/test_common.h:27)
- [src/tests/sanity_tests/test_flatten_sumplusminus_inf.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/sanity_tests/test_flatten_sumplusminus_inf.cpp:1)
- [src/tests/probes/sum_inf_fix_compare.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/probes/sum_inf_fix_compare.cpp:1)
- [src/tests/probes/sum_inf_backend_probe.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/probes/sum_inf_backend_probe.cpp:1)
- [src/tests/probes/sum_inf_stability_stress.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/probes/sum_inf_stability_stress.cpp:1)
- [CMakeLists.txt](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/CMakeLists.txt:121)

## Correctness Checks

### 1. Sum sanity suite

Command run:
- `./test_flatten_sumplusminus_inf`

Result:
- `18/18` tests passed
- this includes the new `test_flatten_SumPlusMinus_Inf_cached_matches_current`
  regression

### 2. Cached-vs-current comparison

Command run:
- `./sum_inf_fix_compare`

Result:
- cached matched current on all saved comparison cases
- compared dimensions:
  - emptiness result after silent removal
  - flattened state count
  - flattened transition count

Saved matrix used by the harness:
- `baseline_det` with `SumPlus`
- `baseline_fractional` with `SumPlus` and `SumMinus`
- `deep_nondet_binary` with `SumPlus`
- `positive_only_nondet` with `SumPlus`
- `child_pump_loop` with `SumMinus`
- `epsilon_boundary` with `SumPlus` and `SumMinus`

### 3. Production correctness regression

Command run:
- `./test_universality_correctness`

Result:
- `320/320` tests passed

Why this matters:
- the shared-helper refactor touched the live threshold-extremal code
- this confirms the existing production SumPlus/SumMinus path still behaves
  correctly after the refactor

## Performance

There are now three performance layers in the bundle:
- small ad hoc probes on saved correctness inputs
- the baseline negative response-time sweep on `n,k <= 5`
- the larger negative response-time scaling sweep

### 1. Initial ad hoc probes

The first probe set used small thresholds and mostly measured constant overhead.
On those cases cached was often slightly slower or roughly neutral.

Representative results:
- `deep_nondet_binary`, `Inf`, `SumPlus`, threshold `8`
  - current: `0.532 ms`
  - cached: `0.549 ms`
- `positive_only_nondet`, `Inf`, `SumPlus`, threshold `4`
  - current: `0.208 ms`
  - cached: `0.252 ms`
- `child_pump_loop`, `Inf`, `SumMinus`, threshold `-1`
  - current: `0.267 ms`
  - cached: `0.362 ms`

These runs were useful as a sanity check, but they were too small to answer the
real scaling question.

### 2. First threshold-heavy signal

The first meaningful win appeared on a heavier `SumMinus` threshold:
- `child_pump_loop`, `SumMinus`, threshold `-20`

`Inf`:
- current: `10.496 ms`
- cached: `8.818 ms`

`LimInf`:
- current: `13.357 ms`
- cached: `8.953 ms`

This was the first case where the cached representation clearly paid back its
overhead.

### 3. Small-grid response-time family (`n,k <= 5`)

The baseline note is:
- [RESPONSE_NEGATIVE_BENCHMARKS.md](./RESPONSE_NEGATIVE_BENCHMARKS.md)

On that small grid:
- cached was slightly faster overall for `Inf`
- cached was slower overall for `LimInf`
- the heaviest point `response_n5_k5` already favored cached

The follow-up reruns changed how that note should be read:
- several apparent cached slowdowns disappeared under 20 repetitions
- the remaining stable losses were only on tiny cases where the hash/interning
  overhead does not amortize

### 4. Larger scaling sweep

The detailed large-family note is:
- [RESPONSE_NEGATIVE_SCALING.md](./RESPONSE_NEGATIVE_SCALING.md)

This larger sweep is the current best performance evidence for `SumMinus`.

Key diagonal points:
- `(6,6)` is still mixed
  - `Inf`: current `9.69 ms`, cached `17.89 ms`
  - `LimInf`: current `32.17 ms`, cached `20.08 ms`
- `(8,8)` already favors cached on both modes
  - `Inf`: `118.82 ms` vs `173.26 ms`
  - `LimInf`: `297.49 ms` vs `409.66 ms`
- `(10,10)` is a clear cached win
  - `Inf`: `1975.60 ms` vs `4169.27 ms`
  - `LimInf`: `3720.49 ms` vs `6807.43 ms`
- `(12,12)` moves the timeout frontier
  - current: timeout on both modes
  - cached: `21.14 s` (`Inf`), `37.96 s` (`LimInf`)

Key off-diagonal points:
- `(6,10)`
  - `Inf`: `803.11 ms` vs `1609.93 ms`
  - `LimInf`: `1703.27 ms` vs `2727.69 ms`
- `(6,12)`
  - `Inf`: `3354.44 ms` vs `7951.49 ms`
  - `LimInf`: `6882.36 ms` vs `12650.50 ms`
- `(8,12)`
  - `Inf`: `12548.44 ms` vs `34351.98 ms`
  - `LimInf`: `23486.47 ms` vs `50885.52 ms`
- `(10,12)`
  - current: timeout on both modes
  - cached: `25.15 s` (`Inf`), `48.39 s` (`LimInf`)

Current performance interpretation:
- cached is not universally faster on tiny inputs
- for `SumMinus`, cached does scale better once the family reaches medium-large
  thresholds
- cached also expands the set of instances that fit inside the `60s` budget

## Instrumentation

The most informative instrumented case remains:
- `child_pump_loop`
- `LimInf`
- `SumMinus`
- threshold `-20`

### Current backend

- elapsed: `11.422 ms`
- state-map lookups: `1110`
- state-map inserts: `319`
- spawn calls: `406`
- step-bag calls: `873`
- bag-copy ops: `771`
- bag-copy entries: `2372`
- frontier observations: `3957`
- frontier config total: `3957`
- frontier capacity total: `11871`
- unique obligations: `14`
- unique bags: `267`

### Cached backend

- elapsed: `7.041 ms`
- state-map lookups: `715`
- state-map inserts: `319`
- spawn calls: `406`
- step-bag calls: `873`
- step-bag cache hits: `337`
- step-obligation calls: `2025`
- step-obligation cache hits: `1997`
- bag-add calls: `384`
- bag-add cache hits: `130`
- bag-copy ops: `0`
- frontier observations: `28`
- frontier config total: `28`
- frontier capacity total: `84`
- unique obligations: `14`
- unique bags: `391`

Interpretation:
- cached removes the bag-copy cost entirely
- cached reduces state-map traffic
- the main win comes from repeated frontier reuse

For a smaller mixed case:
- `deep_nondet_binary`, `LimInf`, `SumPlus`, threshold `30`

current:
- elapsed: `0.203 ms`
- bag-copy ops: `10`
- frontier observations: `12`

cached:
- elapsed: `0.242 ms`
- step-bag cache hits: `4`
- bag-copy ops: `0`
- frontier observations: `8`

Interpretation:
- cached still removes work
- but the instance is too small for the saved work to pay back the interning
  overhead

## Stability

The dedicated stability note is:
- [STABILITY_PASS.md](./STABILITY_PASS.md)

Summary:
- normal-build repeat-use stress passed
  - `100` core iterations, `2400` checks
  - `10` extended iterations, `280` checks
- `ASan+UBSan` stress passed
  - `20` core iterations, `480` checks
  - `3` extended iterations, `84` checks
- no sanitizer findings were reported

This removes the previously open repeat-use/sanitizer stability concern.

## Current Assessment

The current state is materially stronger than the original “research-state”
assessment.

What is now established:
- the cached Sum backend compiles and remains isolated from production dispatch
- it matches the live Sum backend on the saved comparison matrix
- the broader production correctness suite still passes after the shared-helper
  refactor
- the dedicated repeat-use and sanitizer stability pass is clean
- on the larger negative response-time `SumMinus` family, cached scales better
  and extends the timeout frontier

What is still true:
- the strong scaling evidence is for `SumMinus`, not for a broad `SumPlus`
  benchmark family
- tiny-instance regressions still exist, though they are small and no longer
  look structurally important
- this bundle does not settle the separate singleton-child semantics question

## Promotion View

If stability was the last gating concern, that concern is now cleared.

Under the evidence in this bundle, the experimental cached backend now has a
credible case for promotion, especially for `Inf/LimInf x SumMinus`.

The conservative version of that statement is:
- ready for controlled promotion
- no longer blocked by repeat-use or sanitizer instability
- backed by real larger-instance scaling wins on the best available SumMinus
  family
