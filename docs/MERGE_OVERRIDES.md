# Merge Overrides

This file records repository-local policy decisions that intentionally override
upstream commits while merging `QuAK REPO` into this working tree.

## Non-Nested CLI Scope

Decision date: 2026-04-26

Do not port upstream backward-compatibility coverage that assumes the historical
non-nested CLI still exposes all original QuAK operations.

For non-nested automata, the CLI intentionally exposes only the Buchi decision
checks over declared final states:

```text
non-empty VALF <weight>
universal VALF <weight>
```

These map to:

```text
Automaton::isNonEmpty_withFinal(VALF, weight)
Automaton::isUniversal_withFinal(VALF, weight)
```

All other non-nested automata operations remain available as library APIs and in
example programs, but are not part of the supported command-line interface in
this working tree.

Merge implication:

- Skip upstream commit `b4efd54d3`'s `test_non_nested_backward_compat.cpp`.
- Do not add tests that require CLI support for `constant`, `safe`, `live`,
  `top-value`, `bottom-value`, inclusion/equivalence, decomposition, monitor,
  evaluation, `stats`, `dump`, or `empty` on non-nested automata.

## Sup/Max Sanity Test Port

Decision date: 2026-04-26

Upstream commit `1e0c50fc5` was ported by intent rather than copied exactly.

Local differences:

- Added `final: all` to the new `tc11_sup_max_cycle_true.txt` and
  `tc12_sup_max_doomed_false.txt` parent sections, because this tree requires
  explicit Buchi parent final declarations.
- Replaced the brittle `@sink@`/generated-state-name structural assertion with
  the fixture's semantic check: the doomed-overlap case must reject both
  `isNonEmpty(Sup, Max_f, 1)` and `isNonEmpty(LimSup, Max_f, 1)`.

Rationale:

- The current `flatten_MinMax_Sup` path dispatches to the witness-cached backend
  and does not expose the same literal `@sink@` state shape as upstream's older
  structural test.
- The behavioral expectation is the portable regression signal and avoids
  constraining collaborator implementation details.

## Parser Weight Validation Override

Decision date: 2026-04-26

Upstream commit `66581d7b5` introduced nested validation that rejected silent
child transitions during automaton-level checks. This working tree enforces the
same policy earlier, in the parser grammar:

- Non-nested automata weights must be numeric.
- Child automata weights must be numeric.
- Nested parent weights may be numeric or the exact literal `SILENT`.
- Other nonnumeric parent tokens, including `SIL`, are parse errors rather than
  aliases for `SILENT`.

Rationale:

- Invalid input should fail before automaton construction or algorithm
  execution.
- Keeping `SILENT` parent-only preserves the documented silent parent behavior
  without permitting accidental silent child transitions.
- Rejecting aliases avoids the previous behavior where arbitrary malformed
  weights were silently interpreted as silent transitions.

## Universality Boundary Guards

Decision date: 2026-04-26

Upstream commit `6be58c8ea` added direct boundary guards for
`isUniversal(SumPlus, x <= 0)` and `isUniversal(SumMinus, x > 0)`.

Local port:

- Keep `SumPlus` nonpositive thresholds immediately universal, because
  `SumPlus` is always nonnegative and accepted-domain universality is vacuous on
  an empty domain.
- Do not port the `SumMinus` positive-threshold guard as unconditional `false`.
- Instead, return `false` only if the nested automaton has an accepted run with
  infinitely many real child calls. If it does not, accepted-domain universality
  is vacuous.

Rationale:

- `SumMinus` is always nonpositive on emitted child-value sequences, so a
  positive threshold fails when such a sequence exists.
- If the accepted emitting domain is empty, universality should remain
  vacuously true rather than failing because the parent automaton has unrelated
  structure.
