# Negative Response-Time Scaling Update

## Purpose

This note extends the earlier small-grid benchmark in
[RESPONSE_NEGATIVE_BENCHMARKS.md](./RESPONSE_NEGATIVE_BENCHMARKS.md) to answer a
more specific question:

- does the experimental cached `Inf/LimInf x SumMinus` backend actually scale
  better once `n` and `k` become large enough?

The short answer is yes. On the larger negative response-time family, cached is
no longer merely competitive; it is materially better on medium-large points
and solves some instances within the `60s` budget that the current backend does
not.

## Families

### 1. Baseline small grid

Checked-in generator:
- [samples/nwa_gen_response_negative.py](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/samples/nwa_gen_response_negative.py:1)

Generated directory:
- `samples/generated_response_time_negative/`

Grid:
- `1 <= n <= 5`
- `1 <= k <= 5`
- only cases with `k >= n`

### 2. Larger scaling family

Generated directory:
- `samples/generated_response_time_negative_large/`

This larger family uses the same negative child behavior as the baseline family:
- child `1` accumulates `-1` on `r` and `o`
- child accepts on `g`
- query threshold is `-k`

The parent encoding was written in the same reachable-state style as the
positive response-time generator:
- only reachable `(pending_count, oldest_age)` pairs are emitted
- this removes dead parent states without changing the semantics from the
  initial state

Measured larger points:
- diagonal pilot:
  - `(6,6), (8,8), (10,10), (12,12), (14,14), (16,16)`
- off-diagonal sweep:
  - `(6,8), (6,10), (6,12), (6,16)`
  - `(8,10), (8,12), (8,16)`
  - `(10,12), (10,16)`
  - `(12,16), (14,16)`

## Query and Method

All runs used:
- `finVal = SumMinus`
- `infVal in {Inf, LimInf}`
- threshold `= -k`
- timeout `= 60s` per run

Backends compared:
- `current`
- `cached`

Probe harness:
- [src/tests/probes/sum_inf_backend_probe.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/probes/sum_inf_backend_probe.cpp:1)

Saved raw outputs:
- baseline small-grid raw:
  - [raw/response_negative_perf_raw.csv](./raw/response_negative_perf_raw.csv)
  - [raw/response_negative_perf_summary.csv](./raw/response_negative_perf_summary.csv)
  - [raw/response_negative_perf_ratio.csv](./raw/response_negative_perf_ratio.csv)
- 20-rep reruns on selected small-grid points:
  - [raw/response_negative_selected_20rep.csv](./raw/response_negative_selected_20rep.csv)
  - [raw/response_negative_selected_small_20rep.csv](./raw/response_negative_selected_small_20rep.csv)
- larger diagonal pilot:
  - [raw/response_negative_large_pilot.csv](./raw/response_negative_large_pilot.csv)
- larger off-diagonal sweep:
  - [raw/response_negative_large_offdiag.csv](./raw/response_negative_large_offdiag.csv)

The large-grid runs were used as response-time measurements. They are not the
primary correctness evidence. Correctness remains grounded in the saved
cached-vs-current comparison matrix and the dedicated stability pass.

## Baseline Small-Grid Update

The original `n,k <= 5` sweep was mixed:
- cached was slightly faster overall for `Inf`
- cached was slower overall for `LimInf`
- the heaviest point `response_n5_k5` already favored cached

The important update is that several apparent small-grid cached losses were not
stable under rerun. The 20-repetition follow-up on selected worst cases showed:
- `Inf, n=4, k=4`: cached faster (`3.19 ms` vs `3.63 ms`)
- `Inf, n=3, k=5`: cached faster (`5.42 ms` vs `6.52 ms`)
- `LimInf, n=2, k=4`: cached faster (`3.62 ms` vs `3.92 ms`)
- `LimInf, n=3, k=3`: cached faster (`2.72 ms` vs `3.01 ms`)

What remained after the reruns were only tiny-instance near-ties or small
losses where the interning/hash overhead is not yet amortized:
- `Inf, n=1, k=3`: current `0.633 ms`, cached `0.692 ms`
- `LimInf, n=2, k=2`: current `1.313 ms`, cached `1.347 ms`

So even the baseline note should now be read as:
- small cases are noisy and often too small to show the cached advantage
- the real question is the medium-large threshold regime

## Larger Diagonal Results

The diagonal pilot is the clearest view of the scaling crossover.

### `(n=6, k=6)`

Mixed:
- `Inf`: current `9.69 ms`, cached `17.89 ms`
- `LimInf`: current `32.17 ms`, cached `20.08 ms`

At this size, cached is not yet dominant.

### `(n=8, k=8)`

Cached wins both:
- `Inf`: cached `118.82 ms` vs current `173.26 ms`
- `LimInf`: cached `297.49 ms` vs current `409.66 ms`

### `(n=10, k=10)`

Cached wins clearly:
- `Inf`: cached `1975.60 ms` vs current `4169.27 ms`
- `LimInf`: cached `3720.49 ms` vs current `6807.43 ms`

### `(n=12, k=12)`

This is the first strong feasibility frontier:
- current: `TIMEOUT` for both `Inf` and `LimInf`
- cached:
  - `Inf`: `21142.81 ms`
  - `LimInf`: `37962.16 ms`

### `(n=14, k=14)` and `(n=16, k=16)`

Both backends timed out for both `Inf` and `LimInf`.

## Larger Off-Diagonal Results

The off-diagonal sweep shows that the scaling improvement is not only an
`n=k` artifact. It also appears when the response horizon `k` grows faster than
the pending-count limit `n`.

### Fixed `n=6`, increasing `k`

`(6,8)`:
- `Inf`: current `153.37 ms`, cached `119.38 ms`
- `LimInf`: current `305.52 ms`, cached `261.02 ms`

`(6,10)`:
- `Inf`: current `1609.93 ms`, cached `803.11 ms`
- `LimInf`: current `2727.69 ms`, cached `1703.27 ms`

`(6,12)`:
- `Inf`: current `7951.49 ms`, cached `3354.44 ms`
- `LimInf`: current `12650.50 ms`, cached `6882.36 ms`

`(6,16)`:
- `Inf`: current `TIMEOUT`, cached `28885.56 ms`
- `LimInf`: both `TIMEOUT`

This is a clean scaling pattern:
- cached advantage grows as `k` grows
- `Inf` benefits first
- `LimInf` benefits too, but reaches the timeout wall earlier

### Larger off-diagonal points

`(8,10)`:
- `Inf`: current `3804.95 ms`, cached `1758.95 ms`
- `LimInf`: current `5968.18 ms`, cached `3352.81 ms`

`(8,12)`:
- `Inf`: current `34351.98 ms`, cached `12548.44 ms`
- `LimInf`: current `50885.52 ms`, cached `23486.47 ms`

`(10,12)`:
- current: `TIMEOUT` for both
- cached:
  - `Inf`: `25151.59 ms`
  - `LimInf`: `48386.52 ms`

`(8,16)`, `(10,16)`, `(12,16)`, `(14,16)`:
- both backends timed out for both modes

## Interpretation

The current scaling picture for `SumMinus` is now much clearer than it was
after the small-grid sweep.

### 1. There is a real crossover

Cached is not universally faster at tiny sizes, but once the family reaches the
medium-large threshold regime, cached becomes decisively better.

The crossover is already visible by:
- diagonal `n=k=8`
- off-diagonal `n=6, k=10`

### 2. Cached expands the feasible region

This is more important than modest constant-factor wins.

Examples:
- `(12,12)`: current times out, cached finishes both modes
- `(10,12)`: current times out, cached finishes both modes
- `(6,16)`, `Inf`: current times out, cached finishes

So cached is not just shaving milliseconds; it is changing which instances fit
inside the `60s` budget.

### 3. `Inf` benefits first and more strongly

Both modes improve, but `Inf` consistently reaches the “large win” regime
earlier. `LimInf` still benefits, but it remains the harder mode.

### 4. There is still a hard wall

Neither backend handles the full larger family under `60s`.
The common timeout wall is around:
- diagonal `n>=14, k>=14`
- many `k=16` off-diagonal points

So the cached backend improves the frontier, but it does not remove the
combinatorial blow-up.

## Current Takeaway

For `Inf/LimInf x SumMinus`, the larger negative response-time family changes
the interpretation of the project state:

- the earlier small-grid results understated the cached backend
- cached does scale better on the first genuinely large family we tried
- the scaling advantage is large enough to matter operationally
- the timeout frontier moves in cached’s favor

This means the Sum cached backend is no longer just an interesting experiment on
the performance side. At least for `SumMinus`, it now has strong benchmark
evidence behind it.
