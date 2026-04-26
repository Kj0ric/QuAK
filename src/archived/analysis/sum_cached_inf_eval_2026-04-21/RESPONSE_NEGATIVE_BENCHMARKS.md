# Negative Response-Time Benchmarks

## Status Update

This note is the original small-grid baseline (`n,k <= 5`).

It should now be read together with:
- [RESPONSE_NEGATIVE_SCALING.md](./RESPONSE_NEGATIVE_SCALING.md)

That later note adds:
- 20-repetition reruns on selected small-grid outliers
- the larger `n,k` scaling sweep
- the updated interpretation that cached does scale better on the larger
  `SumMinus` response-time family

## Family

This benchmark family is the response-time generator with the child automaton
accumulating `-1` instead of `+1` on `r` and `o`:

- generator: [samples/nwa_gen_response_negative.py](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/samples/nwa_gen_response_negative.py:1)
- generated inputs: `samples/generated_response_time_negative/response_n{n}_k{k}.txt`

The benchmark grid used here is:
- `1 <= n <= 5`
- `1 <= k <= 5`
- only cases with `k >= n`

This yields `15` instances.

## Query

Each instance was evaluated with:
- `finVal = SumMinus`
- `infVal in {Inf, LimInf}`
- threshold `= -k`

The threshold choice `-k` is the negative analogue of the positive response-time
family’s natural threshold `k`.

## Measurement Setup

Backends compared:
- `current`
- `cached`

Method:
- harness: [sum_inf_backend_probe.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/probes/sum_inf_backend_probe.cpp:1)
- repetitions: `3` runs per `(backend, inf, n, k)` point
- timeout: `60s` per run
- metric used for aggregation: mean `elapsed_ms`

No timeouts occurred.

Saved data:
- raw: [raw/response_negative_perf_raw.csv](./raw/response_negative_perf_raw.csv)
- per-instance summary: [raw/response_negative_perf_summary.csv](./raw/response_negative_perf_summary.csv)
- cached/current ratio table: [raw/response_negative_perf_ratio.csv](./raw/response_negative_perf_ratio.csv)
- overall rollup: [raw/response_negative_perf_overall.csv](./raw/response_negative_perf_overall.csv)
- diagonal-only rollup (`n=k`): [raw/response_negative_perf_diagonal.csv](./raw/response_negative_perf_diagonal.csv)

## Main Results

### Overall average over all 15 instances

`Inf`:
- current: `1.415 ms`
- cached: `1.343 ms`
- cached is about `5%` faster on average

`LimInf`:
- current: `2.606 ms`
- cached: `3.016 ms`
- cached is about `16%` slower on average

### Diagonal average (`n = k`)

`Inf`:
- current: `1.619 ms`
- cached: `1.420 ms`
- cached is about `12%` faster on average

`LimInf`:
- current: `3.019 ms`
- cached: `2.881 ms`
- cached is about `5%` faster on average

## Instance-Level Pattern

The performance story is mixed rather than monotone.

### Clear wins for `cached`

`Inf`:
- `(n=2, k=3)`: ratio `0.46`
- `(n=2, k=4)`: ratio `0.48`
- `(n=3, k=4)`: ratio `0.62`
- `(n=5, k=5)`: ratio `0.48`

`LimInf`:
- `(n=4, k=4)`: ratio `0.80`
- `(n=5, k=5)`: ratio `0.89`

### Clear losses for `cached`

`Inf`:
- `(n=3, k=5)`: ratio `1.73`
- `(n=4, k=4)`: ratio `2.52`
- `(n=4, k=5)`: ratio `1.27`

`LimInf`:
- `(n=2, k=4)`: ratio `2.36`
- `(n=3, k=3)`: ratio `1.91`
- `(n=1, k=4)`: ratio `1.91`

## Heaviest Measured Point

For the largest generated diagonal case, `response_n5_k5.txt`:

`Inf`:
- current: `5.344 ms`
- cached: `2.567 ms`

`LimInf`:
- current: `10.177 ms`
- cached: `9.059 ms`

So on the heaviest point in this sweep, `cached` wins for both `Inf` and
`LimInf`, with a strong win for `Inf`.

## Assessment

This negative response-time family does show the cached backend’s potential, but
not uniformly.

What the data says:
- `cached` is already competitive on this family
- `cached` tends to improve on the heavier diagonal end
- `cached` does not dominate the whole grid
- `LimInf` remains the less stable mode performance-wise

As a baseline note, this remains useful:
- it shows the small-instance regime is mixed
- it shows the first sign of a cached win on the heaviest `n,k <= 5` point

But it is no longer the full story.

The current interpretation after the reruns and larger sweep is:
- many apparent small-grid cached losses were measurement-noise or tiny-instance
  overhead effects
- cached becomes materially better once the family reaches the larger threshold
  regime
- cached also expands the set of `SumMinus` instances that finish within the
  `60s` timeout

For the updated conclusion, use
[RESPONSE_NEGATIVE_SCALING.md](./RESPONSE_NEGATIVE_SCALING.md).
