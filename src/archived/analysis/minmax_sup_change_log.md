# Nested Emptiness Change Log

This file is intended to be maintained as a rolling work log.

Use it as:

- a summary of what changed on each day
- a record of which conclusions superseded earlier notes
- a checklist for selectively carrying updates into the repository

Older dated notes still contain useful detail, but some conclusions in them may
have been overtaken by later fixes. When that happens, this file should record
the newer status explicitly.

The focus is currently:

- `Min/Max x Sup/LimSup`

but adjacent `Inf/LimInf` work is included when it is part of the same backend
development thread or affects later conclusions.

No dated analysis material is currently archived here before `2026-04-21`.

## 2026-04-21

### Scope

The dated material for this day covers two adjacent tracks:

- `Min/Max x Inf/LimInf` backend evaluation, especially the `cached` sparse
  implementation versus `current` and `threshold_obl`
- experimental cached `Inf/LimInf x {SumPlus, SumMinus}` work, kept separate
  from production dispatch

These changes are not the same slice as the later `Sup/LimSup` fixes, but they
are part of the same nested-emptiness backend evolution and should be tracked in
the same rolling document.

### Main outcomes

#### 1. `Min/Max x Inf/LimInf` evaluation favored the cached direction

The main `2026-04-21` evaluation concluded:

- `current`, `cached`, and `threshold_obl` agreed on the confirmed bundled
  correctness cases in the saved matrix
- `cached` was the strongest overall scalability direction among the evaluated
  production candidates
- `threshold_obl` remained a useful comparison backend, but was not a clear
  default replacement for the live path

Important confirmed findings from the dated notes:

- the repeated-use cached crash was root-caused as a concrete use-after-free in
  vector-backed interning arenas
- that crash was fixed
- the refreshed comparison harness and ASan reproducer then completed cleanly
- `v2` still mismatched on the known overlap witness
  `max_merge_bug_complete.txt` for `Inf/LimInf`
- one-state-child semantics were documented but still unresolved in production
  code

Performance conclusion from the dated evaluation:

- the live `current` backend spends real work on deep bag copying and repeated
  stepping
- the cached sparse representation removes those costs directly
- on the saved small resource-consumption family, `cached` was the best overall
  backend

Recommended direction recorded on that day:

- keep the current obligation semantics
- evolve the production path toward the cached sparse/interned implementation
- do not switch production dispatch to `threshold_obl` by default

Primary dated materials:

- `analysis/minmax_inf_v2_eval_2026-04-21/README.md`
- `analysis/minmax_inf_v2_eval_2026-04-21/RESULTS.md`
- `analysis/minmax_inf_v2_eval_2026-04-21/IMPLEMENTATION_PLAN.md`
- `analysis/minmax_inf_v2_eval_2026-04-21/CACHED_PROMOTION_PLAN.md`

#### 2. Experimental cached `SumPlus/SumMinus x Inf/LimInf` backend was added

An experimental cached Sum backend was added as a separate option, without
changing production dispatch.

Key implementation outcome:

- a new experimental entry point
  `flatten_SumPlusMinus_Inf_cached(...)` was added
- the live production dispatcher remained unchanged
- the change reused shared threshold-extremal semantics where possible instead
  of forking Sum semantics

Correctness and stability status recorded on that day:

- the Sum sanity suite passed
- cached-vs-current comparison matched on the saved Sum cases
- production universality regressions still passed after the shared-helper
  refactor
- the dedicated stability stress tool passed in normal runs
- `ASan+UBSan` runs passed without reported memory or UB findings

Performance conclusion from the dated notes:

- cached was not universally faster on tiny Sum inputs
- on heavier `SumMinus` thresholds and larger response-negative families, cached
  scaled better and pushed the timeout frontier outward
- the cached representation idea therefore transferred to Sum, but only at the
  representation/memoization level rather than by copying Min/Max-specific
  frontier semantics

Primary dated materials:

- `analysis/sum_cached_inf_eval_2026-04-21/README.md`
- `analysis/sum_cached_inf_eval_2026-04-21/STABILITY_PASS.md`
- `analysis/minmax_inf_v2_eval_2026-04-21/CACHED_SUM_INF_PLAN.md`

#### 3. Carry-forward interpretation for later work

The `2026-04-21` material matters for later days in three ways:

- it established `cached` as the preferred optimization direction for
  `Min/Max x Inf/LimInf`
- it identified one-state-child semantics as an unresolved policy/implementation
  gap
- it recorded an unresolved control-case mismatch around
  `complete_nonterminating_background.txt`, which later had to be re-checked
  carefully instead of being trusted blindly

### Files and artifacts touched on or summarized for `2026-04-21`

Core and experimental code:

- `src/NestedAutomaton.cpp`
- `src/NestedAutomaton.h`

Experimental Sum test/support files:

- `src/tests/probes/sum_inf_fix_compare.cpp`
- `src/tests/probes/sum_inf_backend_probe.cpp`
- `src/tests/probes/sum_inf_stability_stress.cpp`
- `src/tests/sanity_tests/test_flatten_sumplusminus_inf.cpp`
- `src/tests/sanity_tests/test_common.h`
- `CMakeLists.txt`

Analysis bundles:

- `analysis/minmax_inf_v2_eval_2026-04-21/`
- `analysis/sum_cached_inf_eval_2026-04-21/`

## 2026-04-22

### Scope

Most of the work today was around:

- correctness of `Sup/LimSup x {Max_f, Min_f}`
- comparison between the current backend and the previous `old_v2` backend
- making `flatten_regular(...)` trustworthy again as an oracle for this slice
- preserving and repairing the old-v2 core idea via a separate experimental
  split-witness backend
- cleaning up parser / final-state semantics that were masking backend bugs

### Main outcomes

By the end of the day:

- the current `Sup/LimSup x {Max_f, Min_f}` path had no known counterexample in
  the checked suite
- the old-v2 specialized backend still had a representation issue, but the
  separate split-witness variant was repaired to match the regular oracle on the
  current trusted sweep
- the regular oracle path for `Sup/LimSup x {Max_f, Min_f}` was fixed and is now
  trusted again on the current matrix
- the parser/build ambiguity "empty final set means all states final" was
  removed
- the compare harness no longer needs case exclusions for the currently checked
  `Sup/LimSup x {Max_f, Min_f}` matrix

Final verification state at the end of the day:

- `./build-review/test_sanity_all` -> `58/58`
- `./build-review/test_emptiness_correctness` -> `467/467`
- `./build-review/minmax_sup_fix_compare` -> `queries=1248 oracle_queries=1248 mismatches=0`
- `./build-review/minmax_sup_fix_compare_split_old_v2` -> `queries=1248 oracle_queries=1248 mismatches=0`
- `./build-review/minmax_sup_split_witness_probe` -> `mismatches=0/38`

### Detailed log

#### 1. Baseline verification and confidence assessment

We first mapped the live and historical implementations for:

- `Sup x {Max_f, Min_f}`
- `LimSup x {Max_f, Min_f}`

Key conclusion:

- the current `quak` build uses the shared threshold backend in
  `src/NestedAutomaton.cpp`
- the previous specialized construction lives in
  `src/NestedAutomaton_OLD_V2.cpp`

Confidence at that point:

- current backend: high practical confidence, based on passing correctness tests
- previous backend: low confidence, because targeted regressions already
  falsified it

Related note:

- `analysis/minmax_sup_limsup_verification_2026-04-22.md`

#### 2. Old-v2 same-symbol spawn bug

Two concrete old-v2 failures were analyzed first:

- `sup_initial_final_child.txt`
- `sup_initial_final_child_min_bad_current_symbol.txt`

Root cause:

- a child spawned on the current parent symbol could be treated as if it had
  already existed before that symbol
- this let old-v2 terminate a fresh child immediately from its initial-final
  state without forcing it to consume the spawning symbol

Effect:

- false negative for `Max_f`
- false positive for `Min_f`

Fix:

- a minimal "fresh spawn on this parent edge" marker was added inside the old-v2
  exploration state
- same-symbol consumption was forced for that fresh spawn in both the witness
  path and the background path

Result:

- the focused old-v2 probe went from `4/8` mismatches to `0/8`

This was intentionally a small local repair that did not increase the asymptotic
state space.

#### 3. Remaining old-v2 overlap / merge bug

After the same-symbol fix, the next remaining issue was the overlap / merge
class, centered on:

- `src/tests/correctness_tests/inputs/max_merge_bug_complete.txt`

The important observation was:

- old-v2 can collapse a background invocation and a witness invocation when they
  occupy the same child state
- that loses a real background obligation and yields false positives

This was documented separately in:

- `analysis/old_v2_minmax_sup_overlap_merge_issue_2026-04-22.md`

Design goal recorded there:

- preserve the old core idea as much as possible, instead of immediately
  replacing it with the newer threshold-obligation backend

#### 4. Experimental split-witness backend

To preserve the old-v2 structure while repairing the overlap problem, a separate
experimental backend was added:

- `NestedAutomaton::flatten_MinMax_Sup_split_witness(...)`

Shape:

- background obligations remain in compact bitsets
- the witness is stored separately
- an explicit `witness_tracked` bit is carried

Relevant code and plumbing:

- declaration in `src/NestedAutomaton.h`
- current-library stub / alias in `src/NestedAutomaton.cpp`
- actual old-v2 implementation in `src/NestedAutomaton_OLD_V2.cpp`
- test wrapper in `src/tests/sanity_tests/test_common.h`
- dedicated probe in `src/tests/minmax_sup_split_witness_probe.cpp`
- compare-harness support in `src/tests/probes/minmax_sup_fix_compare.cpp`

At first this experimental backend handled the known witness/background overlap
cases well, but later fixes exposed an additional acceptance bug described below
in item 8.

#### 5. Oracle-side fixes in `Automaton`

The regular oracle path had two correctness problems for extremal objectives.

##### 5a. Empty accepting language was not returning bottom

Problem:

- `compute_top_with_final(...)` could treat "no accepting SCC" as if the value
  were `min_domain`
- that caused false positives for low thresholds

Fix:

- `top_LimSup_with_final()` now returns true bottom via `lowest()` when the
  accepting language is empty

##### 5b. Silent replacement for extremal objectives was too weak

Problem:

- `removeSilentTransitions(...)` used real domain values for silent replacement
  (`min_domain` / max-like values)
- that blurred the distinction between silence and real observations

Fix:

- `Sup/LimSup`: silent transitions now use `min_domain - 1`
- `Inf/LimInf`: silent transitions now use `max_domain + 1`
- helper signatures were extended so transformed domains are forced explicitly,
  instead of being inferred only from copied edge weights

##### 5c. Parser-copy path had to preserve empty final sets

Problem:

- even after the top/silent fixes, parser-based copies could still accidentally
  turn an empty final set into "all states final"

Fix:

- parser-based copy/trim paths in `Automaton.cpp` were updated so an explicitly
  empty final set remains empty

Files:

- `src/Automaton.h`
- `src/Automaton.cpp`

Tests added:

- direct regular-oracle regressions in
  `src/tests/correctness_tests/test_emptiness_correctness.cpp`
- direct sentinel-domain checks in the same file

#### 6. Parser / final-state semantics cleanup

While debugging `sup_background_collision_fresh_nomove`, we found that a major
source of confusion was the global rule:

- empty final set in a parser -> mark all states final

That rule was removed.

New semantics implemented today:

- every file automaton must have a `final: ...` clause
- the clause must be nonempty
- `final: all` means all states are accepting
- otherwise `final:` contains an explicit list of state names
- `@CHILD 0` is the only exception; it remains the built-in dummy child

Parser changes:

- support flags for explicit final-state intent were added
- `final: all` is parsed as a dedicated mode
- missing `final:` is now an error for the parent and for every real child

Automaton changes:

- `Automaton::build(...)` no longer interprets an empty final set as
  "all states final"
- all-final is now triggered only by the explicit `final: all` parser flag

Documentation and tests:

- `src/tests/sanity_tests/inputs/README.txt`
- `src/tests/sanity_tests/test_sanity_all.cpp`

Fixture migration:

- affected parent / automaton fixtures were updated to use `final: all`
  explicitly where they had previously relied on omission

Important note:

- this is an intentional input-format behavior change
- if selectively carrying changes, this item should be treated as a separate
  policy decision, not as a purely internal bug fix

#### 7. `sup_background_collision_fresh_nomove`

This file originally triggered a confusing low-threshold discrepancy.

What it exposed:

- the split-witness flattening could produce an automaton with an empty final
  set
- the old parser/build semantics then converted that into "all states final"
- that created spurious low-threshold acceptance

Once the parser/final-state semantics were fixed, that issue disappeared.

Current status at end of day:

- the direct regular-oracle regression passes
- the specialized and split-witness paths agree with the oracle on this file
- the compare harness exclusion for this file was removed

#### 8. Split-witness acceptance bug on `max_merge_bug_complete`

After the parser/final cleanup, the split-witness backend started showing a new
false-negative pattern on:

- `max_merge_bug_complete.txt`

Symptoms:

- it rejected low thresholds that should have been accepted
- dumped flattenings showed the correct `1`-edges were present
- but the flattened automaton had no final states at all

Root cause:

- epoch completion in the split-witness backend was checked on the source
  tracking state
- in this example, the last tracked obligation is discharged on the current
  symbol itself
- so the true epoch boundary is visible only in the raw destination tracking
  state before reset

Fix:

- `src/NestedAutomaton_OLD_V2.cpp` now checks destination-side zero-tracking
  before resetting obligations
- a clarifying comment was added near the fix

Probe expansion:

- `src/tests/minmax_sup_split_witness_probe.cpp` was extended to cover the low
  accepted thresholds on `max_merge_bug_complete`

Result:

- focused split probe: `0/38` mismatches
- split compare harness: `0` mismatches

#### 9. Compare-harness trust cleanup

At the beginning of the day, the compare harness excluded two
`Sup/LimSup x {Max_f, Min_f}` fixtures from regular-oracle comparison:

- `sup_background_collision_fresh_nomove`
- `complete_nonterminating_background`

By the end of the day, both exclusions were removed.

Why:

- `sup_background_collision_fresh_nomove` was a stale exclusion left over from
  the earlier parser/final-state ambiguity and the intermediate split-witness
  bug
- `complete_nonterminating_background` turned out not to be a live
  `Sup/LimSup` oracle problem anymore; direct probing showed that regular and
  specialized paths both reject it on the checked thresholds

Final status:

- `src/tests/probes/minmax_sup_fix_compare.cpp` now trusts every case in the current
  matrix
- both compare binaries report:
  - `queries=1248`
  - `oracle_queries=1248`
  - `mismatches=0`

### Files touched

Core logic:

- `src/NestedAutomaton.cpp`
- `src/NestedAutomaton_OLD_V2.cpp`
- `src/NestedAutomaton.h`
- `src/Automaton.cpp`
- `src/Automaton.h`
- `src/Parser.cpp`
- `src/Parser.h`

Tests and harnesses:

- `src/tests/probes/minmax_sup_fix_compare.cpp`
- `src/tests/minmax_sup_split_witness_probe.cpp`
- `src/tests/sanity_tests/test_common.h`
- `src/tests/sanity_tests/test_sanity_all.cpp`
- `src/tests/correctness_tests/test_emptiness_correctness.cpp`
- `src/tests/correctness_tests/test_correctness_common.h`
- multiple `.txt` fixtures under `src/tests`, `samples`, and `examples`

Build wiring:

- `CMakeLists.txt`

Documentation:

- `analysis/minmax_sup_limsup_verification_2026-04-22.md`
- `analysis/old_v2_minmax_sup_overlap_merge_issue_2026-04-22.md`
- `analysis/minmax_sup_change_log.md`
- `src/tests/sanity_tests/inputs/README.txt`

### Recommended carry-forward grouping

If these changes are selectively propagated, the most natural grouping is:

#### Group A: high-confidence correctness fixes

- extremal-oracle fixes in `Automaton`
- destination-side epoch-completion fix in the split-witness backend
- same-symbol spawn fix in old-v2
- probe and correctness-test expansions
- compare-harness trust cleanup

#### Group B: experimental / optional backend work

- the separate `flatten_MinMax_Sup_split_witness(...)` backend
- its dedicated probe and harness support

This group is useful for continued evaluation even if it is not promoted into
the main old-v2 path immediately.

#### Group C: input-format / parser policy change

- mandatory `final:` clause
- `final: all`
- no implicit "all states final" fallback
- `@CHILD 0` as the only exception
- bulk fixture migration to match the new rule

This group is semantically clean and helped expose real bugs, but it is also a
visible user-facing format change and should be carried deliberately.

#### Group D: analysis-only artifacts

- the dated analysis notes and this file

These are useful regardless of whether the code changes are carried immediately.

### Superseded earlier conclusions

The following earlier conclusions are no longer the latest status:

- the regular-oracle caveat for `sup_background_obligation_blocker` has been
  fixed by the `Automaton` changes
- the regular-oracle caveat for `sup_background_collision_fresh_nomove` is no
  longer live
- for the current `Sup/LimSup x {Max_f, Min_f}` compare harness,
  `complete_nonterminating_background` is no longer excluded
- the split-witness false negatives on low thresholds of
  `max_merge_bug_complete` were fixed later in the day

When older notes disagree with this file on those points, this file reflects the
newer status.

### Remaining open question

The main unresolved design question is still strategic rather than immediate:

- whether the split-witness backend is merely a useful experimental repair, or
  whether it should replace the old-v2 specialized path for this slice

There is no current failing regression in the checked `Sup/LimSup x {Max_f,
Min_f}` matrix, but further adversarial tests are still reasonable if the goal
is to promote it with high confidence.
