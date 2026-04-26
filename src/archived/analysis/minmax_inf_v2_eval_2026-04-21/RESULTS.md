# Results

## Executive Summary

The stitched v2 note still points in two different directions:
- switch the live path to the existing MMThr `threshold_obl` backend
- or keep the current obligation semantics but make the implementation sparse,
  interned, and memoized

The new measurements support the second direction much more strongly.

Main outcome:
- `current`, `cached`, and `threshold_obl` agree on the confirmed bundled
  correctness cases that were tested here.
- `cached` is the fastest backend overall on the small resource-consumption
  family saved in `raw/resource_small_perf.csv`.
- `threshold_obl` is competitive and occasionally best, but it is not a clear
  default winner.
- the previous cached repeat-use crash is fixed; the refreshed comparison
  harness and ASan reproducer now both complete cleanly.
- `cached` is still not ready to promote by default because one-state-child
  semantics are still unresolved in production code and the promotion gate is
  not yet committed as a standing regression suite.

## Baseline Sanity

`raw/test_flatten_minmax_inf.txt`:
- `test_flatten_minmax_inf` still passes `21/21`.
- The existing overlap regression for `threshold_obl` still passes.

## Differential Correctness

Primary raw files:
- `raw/correctness_matrix.csv`
- `raw/correctness_mismatches.txt`

Confirmed findings:
- `current`, `cached`, `masked`, and `threshold_obl` all matched `regular` on
  the standard bundled witnesses in this matrix.
- `v2` still mismatches on the known overlap witness
  `max_merge_bug_complete.txt` for both `Inf` and `LimInf`.

Unresolved control case:
- `complete_nonterminating_background.txt` differs from `regular`:
  `regular=1`, while every specialized backend returns `0`.
- This file is already labeled in the repository as an exploratory control input
  rather than a confirmed bug reproducer, so this mismatch should be treated as
  unresolved scope rather than as a promotion blocker by itself.

Backend agreement:
- `current`, `cached`, and `threshold_obl` produced the same boolean answers on
  every query in `raw/correctness_matrix.csv`.

## One-State Child Diagnostics

Primary raw file:
- `raw/one_state_diagnostics.txt`

Observed behavior:
- A one-state final child currently aborts with `std::out_of_range: map::at` in
  every tested backend, including `regular`.
- A one-state non-final child is not representable cleanly in the current text
  format used here: the parser rejects the empty `final:` declaration before the
  backend runs.

Interpretation:
- The intended semantics are documented in `SEMANTICS.md`.
- The codebase does not enforce them yet.
- This is a real gap that needs an explicit implementation pass.

## Instrumentation

Primary raw file:
- `raw/instrumentation_matrix.csv`

The counters confirm that the stitched note identified real hot spots in the
live backend.

Representative case: `resource_n3_k2`, `Inf`, `Max_f`, threshold `2`
- `current`: `state_map_lookup_calls=53829`, `bag_copy_ops=49527`,
  `bag_copy_entries=58806`, `step_bag_calls=70380`, `step_bag_cache_hits=0`,
  `time_step_bag_ms=15.047486`, `time_state_map_ms=11.678471`
- `cached`: `state_map_lookup_calls=28515`, `bag_copy_ops=0`,
  `step_bag_calls=70380`, `step_bag_cache_hits=3525`,
  `step_obl_cache_hits=3273`, `bag_add_cache_hits=2382`,
  `time_step_bag_ms=4.969665`, `time_state_map_ms=3.620527`

Representative case: `max_merge_bug_complete`, `Inf`, `Max_f`, threshold `1`
- `current`: `bag_copy_ops=38`, `bag_copy_entries=14`,
  `unique_bag_step_keys=9`, `time_step_bag_ms=0.027021`
- `cached`: `bag_copy_ops=0`, `step_bag_cache_hits=15`,
  `bag_add_cache_hits=1`, `unique_bag_step_keys=6`,
  `time_step_bag_ms=0.008856`

What this means:
- The current live backend spends real work on copying bags and re-stepping
  equivalent structures.
- The cached backend removes the bag-copy cost entirely and converts part of
  the repeated stepping into cache hits.
- The promising part of the stitched v2 recommendation is therefore real.

## Small Resource Benchmark

Primary raw files:
- `raw/resource_small_perf.csv`
- `raw/resource_small_perf_summary.txt`
- `raw/resource_small_perf_by_instance.txt`

Setup:
- family: `samples/generated_resource_consumption/resource_n{1..3}_k{1..3}.txt`
- objective: `Max_f` with `Inf` and `LimInf`
- threshold: `k`
- repetitions: `2`
- timeout: `60s` per run

Results:
- `cached,Inf avg_ms=492.438694`
- `current,Inf avg_ms=570.797246`
- `threshold_obl,Inf avg_ms=586.006196`
- `cached,LimInf avg_ms=1187.092449`
- `current,LimInf avg_ms=1246.549038`
- `threshold_obl,LimInf avg_ms=1275.637903`

Structural agreement:
- all three backends produced the same flat state counts, the same transition
  counts, and the same boolean results on every saved resource instance

Interpretation:
- On this family, the cached sparse backend is the best overall performer.
- The live backend remains competitive.
- The MMThr `threshold_obl` backend is not the best default on this family.

## Dense-Frontier Synthetic Family

Primary raw files:
- `raw/dense_frontier_perf.csv`
- `raw/dense_frontier_perf_summary.txt`
- `raw/dense_frontier_perf_by_instance.txt`

Setup:
- family: `raw/inputs/dense_frontier_n{4,6,8}.txt`
- objective: `Max_f` with `Inf` and `LimInf`
- threshold: `1`
- repetitions: `5`

Results:
- `threshold_obl,Inf avg_ms=0.258448`
- `cached,Inf avg_ms=0.275354`
- `current,Inf avg_ms=0.298749`
- `cached,LimInf avg_ms=0.430534`
- `threshold_obl,LimInf avg_ms=0.460743`
- `current,LimInf avg_ms=0.504317`

Important caveat:
- this family did not expand the flat automaton size; all three instances still
  collapsed to a `7`-state, `14`-transition quotient
- it therefore stresses frontier-processing constants more than global state
  explosion

Interpretation:
- Even on a family intended to favor dense-frontier processing, `threshold_obl`
  is not uniformly dominant.
- `threshold_obl` is best on `Inf` here, while `cached` is best on `LimInf`.
- The data still argues for improving the sparse obligation implementation
  rather than replacing it wholesale with MMThr.

## Repeated In-Process Stability And Root Cause

Primary raw files:
- `raw/minmax_inf_fix_compare_live.txt`
- `raw/cached_repro_asan_after_fix.txt`

Previous root cause:
- the cached backend stored `Bag&` and `Obl&` references into interning arenas
  backed by `std::vector`
- it then called `intern_bag(...)` or `intern_obl(...)`, which could grow those
  vectors and reallocate them
- after reallocation, the old references were dangling, but the code still
  wrote through them
- this was a concrete use-after-free, not a vague repeated-run instability

Applied fix:
- after interning, cached write-back now goes through `bags[bid]` and `obls[id]`
  instead of stale references

Observed behavior:
- the refreshed comparison harness now completes successfully
- the ASan reproducer for repeated cached calls on `deep_nondet_binary.txt`
  completes cleanly
- no repeat-use crash was seen in the refreshed validation runs

Interpretation:
- the repeat-use crash blocker is resolved
- cached stability is now materially better than in the original evaluation
- the remaining blockers are semantic coverage and promotion hardening, not this
  specific memory-corruption bug
