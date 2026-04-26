# Methods

## Backends

### `regular`

`regular` is the generic reference construction:
- It flattens the nested automaton with the ordinary weighted backend.
- It keeps the original threshold domain instead of collapsing everything to a
  binary acceptance signal.
- It is the semantic reference used for differential comparison.

Why it matters:
- If a specialized backend disagrees with `regular` on the same query, that is
  a correctness issue until proven otherwise.

### `current`

`current` is the live Min/Max `Inf`/`LimInf` implementation:
- `flatten_MinMax_Inf(...)` dispatches to the shared threshold-extremal
  obligation backend.
- It represents each active child call as an obligation carrying a child index,
  a guess bit, and a sparse frontier split by progress class.
- It uses two obligation bags, `P1` and `P2`, to implement the breakpoint-style
  epoch discipline.
- It treats silent parent edges by propagating the current bags without spawning
  a new obligation.

Cost profile:
- Sparse frontiers are good when only a small part of a child automaton is
  active.
- The expensive parts are deep bag copying, deep state keys in the visited map,
  and recomputing the same bag steps repeatedly.

### `cached`

`cached` is the experimental backend added for this evaluation:
- It keeps the same high-level obligation semantics as `current`.
- It interns obligations and bags into integer IDs.
- It memoizes obligation stepping, bag stepping, and bag-plus-spawn insertion.
- Its visited-state key is compact: parent state ID plus `P1` bag ID, `P2` bag
  ID, phase, and epoch flag.

Intended advantage:
- Avoid repeated deep copies and repeated structural comparisons of large sparse
  bags.

Main risk:
- It is a new implementation of the same semantics, so any mismatch against
  `regular` or `current` must be treated seriously until explained.

### `threshold_obl`

`threshold_obl` is the existing Min/Max-specific MMThr backend:
- It is already specialized to the binary threshold-obligation setting.
- It uses Min/Max-specific frontier structures instead of the fully generic
  sparse `(state, progress)` frontier representation.
- It is the specialization suggested by the second half of the stitched v2 note.

Expected behavior:
- It can win when frontiers are dense enough that bitset-style processing pays
  off.
- It can lose on sparse cases because it pays fixed-width frontier costs.

### `masked`

`masked` is the archived fixed version of the old product-style family:
- It repaired the old overlap bug by distinguishing progress classes more
  carefully.
- It is useful as a regression reference.
- It is not the preferred production direction because it still inherits the
  heavier global-configuration style.

### `v2`

`v2` is the archived old implementation discussed in the stitched note:
- It is included only when useful as historical context or as a sanity check.
- It is not treated as a production candidate in this evaluation.

## Experiment Families

### Semantics note

This is documentation work, not a code change:
- record the intended one-state-child rule
- note explicitly that the broader codebase is not being patched here

### Instrumentation

Instrumentation is collected from the live shared threshold-extremal backend and
the cached prototype.

Measured counters:
- state-map lookup and insertion counts
- spawn counts and unique spawn keys
- bag-step and obligation-step counts and cache hits
- bag-add counts and cache hits
- bag-copy counts and copied-entry totals
- frontier observation totals and aggregate densities
- unique obligation and bag counts
- accumulated time spent in bag stepping, state-map work, and bag copying

Purpose:
- identify whether the stitched v2 note is pointing at real hot spots
- quantify whether interning and memoization actually remove those hot spots

### Differential checks

Each backend is compared on the same input/query pair:
- flatten
- remove silent transitions
- run emptiness for `Inf` or `LimInf`

Primary signal:
- boolean agreement with `regular`

Secondary signals:
- flat state count
- flat transition count

### Performance checks

Performance runs measure:
- end-to-end time for flatten + silent-removal + emptiness
- flat state and transition counts
- peak RSS as reported by `getrusage`

The benchmark families used here are:
- bundled correctness witnesses
- small resource-consumption instances
- selected medium cases where sparse-vs-dense frontier behavior is visible
