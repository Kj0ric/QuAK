# Commands

The evaluation used the following compiled helpers:

- `/tmp/minmax_inf_backend_probe`
  built from `src/tests/probes/minmax_inf_backend_probe.cpp`
- `/tmp/minmax_inf_fix_compare`
  built from `src/tests/probes/minmax_inf_fix_compare.cpp`
- `/tmp/minmax_inf_resource_single`
  built from `src/tests/probes/minmax_inf_resource_single.cpp`
- `./test_flatten_minmax_inf`
  rebuilt with `make test_flatten_minmax_inf`

The main generated artifacts are:

- `raw/test_flatten_minmax_inf.txt`
- `raw/minmax_inf_fix_compare_live.txt`
- `raw/correctness_matrix.csv`
- `raw/correctness_mismatches.txt`
- `raw/instrumentation_matrix.csv`
- `raw/resource_small_perf.csv`
- `raw/resource_small_perf_summary.txt`
- `raw/resource_small_perf_by_instance.txt`
- `raw/dense_frontier_perf.csv`
- `raw/dense_frontier_perf_summary.txt`
- `raw/dense_frontier_perf_by_instance.txt`
- `raw/cached_repro_asan_after_fix.txt`
- `raw/one_state_diagnostics.csv`
- `raw/one_state_diagnostics.txt`

The benchmark probe format is:

```text
/tmp/minmax_inf_backend_probe <backend> <file> <Inf|LimInf> <Max_f|Min_f> <threshold> <stats:0|1>
```

Backends used in this evaluation:
- `current`
- `cached`
- `threshold_obl`
- `masked`
- `v2`
- `regular`
