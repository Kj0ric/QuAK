# SumPlus/SumMinus Sup Witness: Open Issues

Date: 2026-04-24

Context: these issues were found while adding W1/W2 witness-cached fixtures for
`Sup/LimSup x SumPlus/SumMinus`. They are distinct. The first was a
`flatten_regular(...)` acceptance bug. The second is a real public
`isNonEmpty(...)` bug.

## Issue 1: Regular `SumB` Oracle Is Not Valid For Finite Non-Silent Activity

Status: fixed in `flatten_regular(...)`.

Primary fixture:

- `src/tests/correctness_tests/inputs/sum_sup_no_nonsilent_after_prefix.txt`

Observed behavior:

- The fixture has one successful non-silent child call and then only a silent
  accepting parent loop.
- The public threshold path rejects:

```bash
./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_no_nonsilent_after_prefix.txt non-empty Sup SumPlus 1
./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_no_nonsilent_after_prefix.txt non-empty LimSup SumPlus 1
```

- The current threshold flattener and the W1/W2 witness-cached flattener also
  reject for `Sup` and `LimSup`.
- Before the fix, the regular `SumB` oracle accepted for `Sup`.
- After the fix, the regular `SumB` oracle rejects for both `Sup` and `LimSup`.

Expected result:

- Reject for both `Sup` and `LimSup`.

Reason:

- Nested acceptance is not just "the accumulated finite value is large enough".
  The parent run must visit parent final states infinitely often and invoke
  non-silent children infinitely often.
- In this fixture, the only non-silent child activity is a finite prefix. The
  eventual accepting parent loop is silent, so there is no accepting nested run.
- The threshold flatteners encode this by making the final state a transient
  pulse before the eventual silent SCC. The eventual silent SCC is non-final.
- Before the fix, `flatten_regular(...)` marked an acceptance checkpoint final
  without requiring the current epoch to contain any non-silent child call.
- The fixed `flatten_regular(...)` tracks whether the current acceptance epoch
  has seen a non-silent child call and only marks non-vacuous checkpoints final.

Impact:

- `flatten_regular(SumB, ...)` can now be used as the regular oracle for this
  fixture.
- Tests keep this fixture as a direct threshold-vs-witness regression and also
  check it against the regular `SumB` oracle.

Current mitigation:

- `test_sum_sup_witness_edge_cases.cpp` re-enables the regular oracle check for
  `sum_sup_no_nonsilent_after_prefix`.

Follow-up:

- Keep the fixture.
- Keep the epoch non-vacuity regression enabled in comparison harnesses.
- Do not classify this specific mismatch as a W1/W2 implementation bug.

## Issue 2: `SumPlus` Non-Positive Threshold Shortcut Is Unsound

Status: fixed in the public `NestedAutomaton::isNonEmpty(...)` API.

Primary fixture:

- `src/tests/correctness_tests/inputs/sum_sup_background_blocks_acceptance.txt`

Problematic code:

- `src/NestedAutomaton.cpp`, in `NestedAutomaton::isNonEmpty(...)`
- Current shortcut:

```cpp
if ((finVal == SumPlus || finVal == SumMinus)) {
    if (x <= 0 && finVal == SumPlus) return true;
    if (x > 0 && finVal == SumMinus) return false;
}
```

Observed behavior:

```bash
./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_background_blocks_acceptance.txt non-empty Sup SumPlus 0
./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_background_blocks_acceptance.txt non-empty LimSup SumPlus 0
```

- Before the fix, actual public result: accepts.
- Current threshold flattener result: rejects.
- W1/W2 witness-cached flattener result: rejects.
- After the fix, actual public result: rejects.

Expected result:

- Reject for both `Sup` and `LimSup`.

Reason:

- `SumPlus >= 0` is value-trivial only after an accepting nested run exists.
- The shortcut skips the accepting-run existence check entirely.
- In the background-blocker fixture, an initial background child obligation is
  never discharged on the accepting word. Later witness candidates can return,
  but the run is still not accepting because the background obligation remains
  pending.
- Returning `true` for every `SumPlus` query with `x <= 0` therefore creates
  false positives whenever the nested language is empty or all candidate runs
  violate pending-obligation / non-silent acceptance requirements.

Impact:

- The CLI and public `isNonEmpty(...)` can report non-empty for an automaton
  with no accepting nested run.
- The bug is broader than the new W1/W2 backend because the shortcut runs before
  flattening and before the special `SumPlus + LimSupAvg` fast path.
- The analogous `SumMinus` shortcut for `x > 0` is not symmetric: since
  `SumMinus` values are never positive, returning `false` for positive
  thresholds is value-impossible and remains safe.

Fix:

- `finVal == SumPlus && x <= 0` now calls a structural nested-language check
  instead of returning `true` unconditionally.
- The structural check builds `flatten_regular(SumB, 0)` and looks for a
  reachable accepting SCC in the resulting obligation automaton that also has an
  internal non-silent edge.
- This keeps pending child obligations, parent Büchi acceptance, and infinitely
  many non-silent child calls in the shortcut decision.
- `finVal == SumMinus && x > 0` remains the value-impossible `false` shortcut.
- Added public API regressions for:
  - `sum_sup_background_blocks_acceptance` rejecting public
    `Sup/LimSup/LimSupAvg x SumPlus` at thresholds `0` and `-1`.
  - `sum_sup_no_nonsilent_after_prefix` rejecting public
    `Sup/LimSup/LimSupAvg x SumPlus` at thresholds `0` and `-1`.
  - `sum_sup_witness_immediate_discharge` accepting public
    `Sup/LimSup/LimSupAvg x SumPlus` at thresholds `0` and `-1`.
  - `sum_sup_summinus_mixed_sign_abs_cost` rejecting public
    `Sup/LimSup x SumMinus` at threshold `1`.

Follow-up:

- After fixing the shortcut, update the comparison harness so the regular
  `SumB` oracle is not used as the authority for threshold-`0` acceptance-sensitive
  SumPlus fixtures.
