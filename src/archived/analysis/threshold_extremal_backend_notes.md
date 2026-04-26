# Historical threshold-obligation backend notes

This file was moved out of `src/` because it is design history, not live
source-tree documentation. It describes the shared threshold-obligation idea and
some helper names from an earlier implementation. Current public wrappers in
`NestedAutomaton.cpp` route to the cached or witness-cached implementations:

- `flatten_SumPlusMinus_Sup_witness_cached`
- `flatten_SumPlusMinus_Inf_cached`
- `flatten_MinMax_Sup_witness_cached`
- `flatten_MinMax_Inf_cached`

The original note follows.

## What changed

The monotone/extremal cases

- `{Sup, LimSup, Inf, LimInf} × {Min_f, Max_f, SumPlus, SumMinus}`

now use one shared threshold-obligation construction instead of four separate custom flatteners.

The new backend is implemented as `flatten_threshold_extremal_impl(...)` and the public entry points

- `flatten_SumPlusMinus_Sup`
- `flatten_SumPlusMinus_Inf`
- `flatten_MinMax_Sup`
- `flatten_MinMax_Inf`

now delegate to it.

`isNonEmpty(...)` was updated so all monotone/extremal cases go through this backend, and silent transitions are removed before the final outer emptiness check. The `SumPlus + LimSupAvg` fast path was updated as well.

## Core idea

Each child invocation becomes one boolean obligation:

- `guess = 1` means the invocation must return `>= threshold`
- `guess = 0` means the invocation must return `< threshold`

The flattened state uses the same high-level shape as the regular `OblBag` backend:

- parent control state
- `P1` = pending obligations for the next epoch
- `P2` = tracked obligations of the current epoch
- Büchi phase

This keeps the proven phase/epoch discipline, but shrinks each child-local obligation to a threshold-progress frontier instead of an exact return-value frontier.

## Child-local progress domains

### `Max_f`
Progress bit:

- `0` = no edge `>= threshold` seen yet
- `1` = already seen such an edge

### `Min_f`
Progress bit:

- `1` = all edges so far are `>= threshold`
- `0` = a bad edge was already seen

### `SumPlus`
Progress scalar:

- `p = min(B, accumulated_gain)`
- `B = ceil(threshold * scale)`

At a fixed local state:

- for `guess = 1`, larger `p` dominates
- for `guess = 0`, smaller `p` dominates

### `SumMinus`
Progress scalar:

- `p = min(B + 1, accumulated_loss)`
- `B = floor((-threshold) * scale)`

At a fixed local state:

- for `guess = 1`, smaller `p` dominates
- for `guess = 0`, larger `p` dominates

## Why this is more efficient

The old monotone backends explicitly synchronised all currently active child states on each symbol. In the worst case they had to explore a Cartesian product of local choices, so one step could cost roughly

`Π_i outdeg(active_i)`.

The new backend steps obligations independently. One step is roughly

`Σ_obligation Σ_frontier-state outdeg(local_state)`

plus bag insertion/canonicalization.

That removes the main blow-up source.

### Specific gains over the old pipeline

#### `Min_f/Max_f + Inf/LimInf`
The old active simplified backend merged child histories at the same local state and explicitly materialized the product of active-child moves. The new backend keeps separate obligations and steps them independently, so it fixes the merge bug and removes the explicit product.

#### `Min_f/Max_f + Sup/LimSup`
The old backend tracked one distinguished witness plus a recursive background exploration over all other active children. The new backend treats all child invocations uniformly as obligations; there is no witness/background split and no recursive global move product.

#### `SumPlus/SumMinus + Sup/LimSup`
The old backend used one tracked token plus background activation/tracking vectors and recursively enumerated compatible successor assignments. The new backend keeps one obligation per invocation with a compact threshold-progress frontier.

#### `SumPlus/SumMinus + Inf/LimInf`
The old backend propagated interval sets across a global selection recursion. The new backend reuses the same threshold-obligation engine and only changes the child-local scalar progress semantics.

## Liveness pruning

The new backend keeps the coarse child reachability table from `ChildTables::live`, and adds threshold-aware pruning:

- for `Min_f` / `Max_f`: reverse BFS on the small product graph `(local_state, progress_bit)`
- for `SumPlus` / `SumMinus`: per-state minimum/maximum remaining gain/loss cutoffs

This lets spawn/step reject dead threshold states early.

## Silent-step handling

The new flatteners emit `SILENT` on silent parent transitions, just like `flatten_regular(...)`.

This means callers must remove silent transitions before applying the final outer `Sup/LimSup/Inf/LimInf` emptiness check. `isNonEmpty(...)` now does that for all monotone/extremal combinations, and the `SumPlus + LimSupAvg` fast path was updated too.

## Behavioral/engineering notes

- The new backend assumes the same monotone-sign discipline as the old monotone pipeline:
  - `SumPlus`: child weights are non-negative
  - `SumMinus`: child weights are non-positive
- Positive `SumPlus` thresholds now use `ceil(threshold * scale)` rather than nearest-integer rounding.
- Negative `SumMinus` thresholds use `floor((-threshold) * scale)`.
- The public monotone flatteners now return SILENT-aware automata. If they are called directly outside `isNonEmpty(...)`, the caller should run silent-removal before the final outer emptiness check.

## Expected practical effect

Compared to the current custom monotone pipeline, this should usually:

- reduce branching when many child invocations overlap
- reduce memory pressure by avoiding global cross-products of child moves
- make `Min_f/Max_f + Inf/LimInf` both sound and substantially faster
- simplify maintenance by replacing four special-purpose constructions with one shared threshold-obligation engine

The one place where further optimization is still possible is the local frontier representation itself:

- `Min_f/Max_f` can be specialized further to pure two-bitset frontiers
- `SumPlus/SumMinus` can be specialized further to dense per-state scalar arrays

The current implementation already removes the dominant product explosion without requiring another round of intrusive refactoring.
