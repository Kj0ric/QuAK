# MinMax Inf/LimInf V2 Evaluation

This folder contains the implementation notes, raw outputs, and summary reports
for the evaluation of the stitched recommendations in
[`inf_liminf_min_max_new_v2.md`](../inf_liminf_min_max_new_v2.md).

Scope:
- item 1: record the intended one-state-child semantics
- item 2: instrument the live shared threshold-obligation backend
- item 3: prototype the cached sparse backend as a separate implementation
- item 4: run differential checks across backends
- item 5: run targeted performance comparisons
- item 6: document the findings and the recommended implementation plan

Contents:
- [SEMANTICS.md](./SEMANTICS.md): explicit semantic decisions used for this evaluation
- [METHODS.md](./METHODS.md): clear description of each backend and each experiment family
- [COMMANDS.md](./COMMANDS.md): build and run commands used to generate the artifacts
- [RESULTS.md](./RESULTS.md): consolidated findings after the runs
- [IMPLEMENTATION_PLAN.md](./IMPLEMENTATION_PLAN.md): recommended path to a production-quality implementation
- [CACHED_PROMOTION_PLAN.md](./CACHED_PROMOTION_PLAN.md): detailed staged plan to move `cached` to production
- [CACHED_SUM_INF_PLAN.md](./CACHED_SUM_INF_PLAN.md): detailed plan to apply the cached representation idea to `Inf/LimInf x {SumPlus, SumMinus}`
- `raw/`: command outputs, CSV files, and benchmark logs

Important constraint for this evaluation:
- The one-state-child rule is documented here, but this work does not attempt to
  patch unrelated code paths to enforce it globally.
