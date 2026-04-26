# Merge TODO

This file tracks merge items intentionally left open while porting commits from
`QuAK REPO`.

## Mixed-Sign SumPlus/SumMinus Absolute Semantics

Opened: 2026-04-26

Related upstream commits:

- `66581d7b5` introduced mixed-sign projection and nested validation.
- `6be58c8ea` added universality guards and mixed-sign universality tests.
- `996d2725f` changed `SumPlus`/`SumMinus` to absolute-value semantics.
- `981558cae` fixed recursive projection to keep the original finite aggregator.
- `0bbce0fd9` later rejected mixed-sign LimAvg paths by default.

Desired local policy:

- `SumPlus` should mean `sum(abs(w_i))`.
- `SumMinus` should mean `-sum(abs(w_i))`.
- Mixed-sign children should be handled consistently across supported
  non-emptiness and universality checks.
- Avoid a permanent special rejection for `LimAvg + SumPlus/SumMinus` unless the
  implementation cannot be made sound.

Decision guidance:

- Prefer automatic absolute-value normalization over rejecting mixed-sign child
  weights. Rejection is simpler operationally, but it conflicts with the
  intended semantics above: if `SumPlus` and `SumMinus` are defined through
  absolute values, mixed-sign children are valid inputs, not malformed inputs.
- Do not normalize only one algorithm family. The current local state is partly
  normalized already: non-`LimAvg` `SumMinus` threshold paths effectively use
  absolute edge costs, while non-`LimAvg` `SumPlus` can still fail on negative
  child weights, and universality still lowers to raw `SumB` without projection.
  The implementation should not leave these paths with different interpretations
  of the same finite aggregator.
- Treat a direct mixed-sign rejection as an acceptable temporary fallback only
  for `LimAvg + SumPlus/SumMinus`, and only if that pipeline cannot be made
  sound promptly. This should be documented as an implementation limitation, not
  as the final semantics.
- If the fallback is used, keep it narrow: reject only the affected mixed-sign
  `LimAvg` paths with an explicit error. Do not reject mixed-sign
  `Sup`/`LimSup`/`Inf`/`LimInf` paths once they can be normalized consistently.

Likely implementation route:

- Normalize child weights into a temporary nested automaton before running
  `SumPlus`/`SumMinus` algorithms when mixed signs are present.
- For `SumPlus`, normalize each child edge weight to `abs(w)`.
- For `SumMinus`, normalize each child edge weight to `-abs(w)`.
- Leave parent weights unchanged.
- Recurse with the same finite aggregator, not `SumB`, so each infinite
  aggregator path uses its own algorithm.
- Apply the same projection policy to nested universality before its `SumB`
  lowering, otherwise universality continues to interpret mixed-sign children as
  raw signed sums.
- Revisit existing mixed-sign `LimAvg` diagnostics that intentionally rely on
  raw signed accumulation. Under the formal absolute-value convention, those
  expectations are no longer valid and should either be rewritten or moved under
  a temporary rejection/error-handling test.

Test port notes:

- Defer `6be58c8ea`'s mixed-sign universality tests until the absolute-value
  implementation is ported.
- Do not use the intermediate expectations from `6be58c8ea`:
  `SumPlus = 7` and `SumMinus = -2` for child weights `[3, -2, 4]`.
- Under the desired local semantics, the expected values for that fixture are
  `SumPlus = 9` and `SumMinus = -9`.
- `996d2725f` was reviewed during the merge and intentionally deferred here;
  do not apply its README/test/code changes until the absolute-value
  implementation is handled end to end.
- `981558cae` was reviewed and intentionally deferred here. Its main rule is
  still required for the eventual implementation: after projecting child
  weights, recurse with the same finite aggregator (`SumPlus` or `SumMinus`),
  not `SumB`, so LimAvg paths keep their own bound logic.
- `0bbce0fd9` was reviewed and intentionally deferred here. It provides a
  fallback policy if sound LimAvg normalization proves too risky: reject
  mixed-sign `LimAvg + SumPlus/SumMinus` inputs by default with an explicit
  error, plus a targeted CLI/error-handling test. Do not adopt this rejection
  policy unless we decide not to implement full absolute-value normalization.

Status: open. Do not port the intermediate mixed-sign behavior from
`66581d7b5` blindly.
