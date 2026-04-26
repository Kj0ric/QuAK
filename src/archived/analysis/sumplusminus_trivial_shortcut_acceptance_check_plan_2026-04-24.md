# Improved Plan: Acceptance-Preserving SumPlus/SumMinus Trivial Shortcuts

Date: 2026-04-24

## Executive Summary

The current `NestedAutomaton::isNonEmpty(...)` shortcut

```cpp
if (x <= 0 && finVal == SumPlus) return true;
```

is value-correct but not language-correct: `SumPlus` values are non-negative, but an
NQA still needs a valid accepting nested run. The shortcut must therefore be changed
from an unconditional value shortcut into a structural non-emptiness shortcut.

The original plan is mostly on the right track because it uses the existing obligation
flattening rather than a raw parent-SCC check. The main improvement is to perform the
final check directly on the obligation-flattened automaton, not through
`removeSilentTransitions(..., LimSup, false)` with threshold `0`. A direct SCC check is
more explicit and avoids relying on how silent-transition elimination encodes neutral
weights when all real collapsed child-call weights are also `0`.

## Correctness Target

For `SumPlus` with threshold `x <= 0`, return `true` iff there exists a valid accepting
nested run satisfying all three structural NQA acceptance requirements:

1. the parent visits accepting states infinitely often;
2. every invoked child terminates in an accepting state on the same input suffix; and
3. the parent performs infinitely many real non-silent child calls.

For `SumMinus` with threshold `x > 0`, keep returning `false` under the current
`>= threshold` semantics, because every `SumMinus` child value is non-positive and every
supported parent aggregate over those values is non-positive.

## Detailed Check of the Original Plan

### Parts to keep

- Keep the public shortcut shape. The shortcut is useful and avoids unnecessary
  threshold flattening for value-trivial cases.
- Use `flatten_regular(SumB, weight_t(0))` as the structural obligation checker.
  With bound `0`, successful non-silent child invocations emit guessed return value
  `0`. Internally, the SumB accumulator may still use overflow sentinels `+1` and `-1`,
  but `discharge_ok_finite(SumB, ..., guess=0, bound=0)` treats those saturated outcomes
  as matching the single collapsed guess.
- Keep pending-child obligations. A parent-only SCC check is unsound because a parent
  can loop acceptingly while an earlier child obligation remains undischargeable.
- Keep the `SumMinus x > 0` branch as a value-impossible branch, subject to the API
  validation decision below.

### Parts to revise

1. **Validate aggregator support before shortcuts.**

   The current early shortcut can answer queries before `isNonEmpty(...)` reaches its
   unsupported-combination checks. In particular, `SumPlus x <= 0` must not silently
   decide an unsupported combination unless the project intentionally documents that
   special case as supported.

   Recommendation: add a small `isSupportedNonEmptyCombination(...)` helper and call it
   before the trivial branches.

2. **Avoid `removeSilentTransitions + LimSup >= 0` for the structural helper.**

   The original plan assumes that, after silent-transition elimination, silent activity is
   strictly below the collapsed real-call value `0`. That may be true today, but it is an
   indirect dependency on `Automaton::removeSilentTransitions`. Since the helper is a
   Boolean structural check, use a graph-level SCC test on the already-flattened
   obligation automaton instead:

   - find a reachable SCC of the flattened automaton;
   - require at least one accepting flat state in that SCC; and
   - require at least one non-silent edge inside that same SCC.

   This is not a raw parent SCC check. It is an SCC check after `flatten_regular(...)`, so
   pending child obligations are already encoded in the flat state. A cycle in this graph
   represents a reusable, obligation-consistent nested suffix.

3. **Broaden tests beyond `Sup` and `LimSup`.**

   The shortcut runs before all later SumPlus/SumMinus code paths, so regressions should
   cover every parent aggregator for which the branch is intended.

## Implementation Plan

### 1. Add supported-combination validation

Add a private/static helper near `NestedAutomaton::isNonEmpty(...)`:

```cpp
static bool isSupportedNonEmptyCombination(value_function_t infVal,
                                           value_function_t finVal) {
    const bool extremal_parent =
        infVal == Sup || infVal == LimSup || infVal == Inf || infVal == LimInf;
    const bool avg_parent = infVal == LimSupAvg || infVal == LimInfAvg;

    if (finVal == Min_f || finVal == Max_f || finVal == SumB) {
        return extremal_parent || avg_parent;
    }

    if (finVal == SumMinus) {
        return extremal_parent || avg_parent;
    }

    if (finVal == SumPlus) {
        // Current implementation supports LimSupAvg + SumPlus, but not the open
        // LimInfAvg + SumPlus case.
        return extremal_parent || infVal == LimSupAvg;
    }

    return false;
}
```

Then put this at the top of `isNonEmpty(...)`, before trivial cases:

```cpp
if (!isSupportedNonEmptyCombination(infVal, finVal)) {
    QUAK_FAIL("isNonEmpty: unsupported aggregator combination");
}
```

If the project deliberately wants to preserve the historical behavior for trivial
`(LimInfAvg, SumPlus, x <= 0)` queries, do not hide that as an accident. Instead, add an
explicit comment and a dedicated test saying that the value-trivial structural subcase is
supported even though the general `(LimInfAvg, SumPlus)` emptiness problem is not.

### 2. Add an SCC predicate for the flat obligation automaton

Add this helper in `src/NestedAutomaton.cpp` after `flatten_regular(...)` is available
and before `isNonEmpty(...)`:

```cpp
static bool hasReachableAcceptingSccWithNonSilentEdge(Automaton* flat) {
    if (!flat || !flat->getStates() || !flat->getInitial()) return false;

    MapArray<State*>* states = flat->getStates();
    const size_t n = states->size();
    const size_t alph = flat->getAlphabetSize();

    // Be robust even if the Automaton constructor changes its reachability pruning.
    std::vector<uint8_t> reachable(n, 0u);
    std::queue<State*> q;
    reachable[flat->getInitial()->getId()] = 1u;
    q.push(flat->getInitial());

    while (!q.empty()) {
        State* s = q.front();
        q.pop();

        for (size_t a = 0; a < alph; ++a) {
            SetStd<Edge*>* succs = s->getSuccessors(a);
            if (!succs) continue;
            for (Edge* e : *succs) {
                if (!e || !e->getTo()) continue;
                const unsigned int tid = e->getTo()->getId();
                if (tid < n && !reachable[tid]) {
                    reachable[tid] = 1u;
                    q.push(e->getTo());
                }
            }
        }
    }

    const unsigned int nbScc = flat->getNbSCCs();
    std::vector<uint8_t> scc_has_final(nbScc, 0u);
    std::vector<uint8_t> scc_has_nonsilent_edge(nbScc, 0u);

    for (size_t sid = 0; sid < n; ++sid) {
        if (!reachable[sid]) continue;
        State* s = states->at(sid);
        if (!s) continue;

        const unsigned int cid = static_cast<unsigned int>(s->getTag());
        if (cid >= nbScc) continue;

        if (s->getFinal()) {
            scc_has_final[cid] = 1u;
        }

        for (size_t a = 0; a < alph; ++a) {
            SetStd<Edge*>* succs = s->getSuccessors(a);
            if (!succs) continue;

            for (Edge* e : *succs) {
                if (!e || !e->getTo() || !e->getWeight()) continue;
                if (static_cast<unsigned int>(e->getTo()->getTag()) != cid) continue;

                if (e->getWeight()->getValue() != weight_t(SILENT)) {
                    scc_has_nonsilent_edge[cid] = 1u;
                }
            }
        }
    }

    for (unsigned int cid = 0; cid < nbScc; ++cid) {
        if (scc_has_final[cid] && scc_has_nonsilent_edge[cid]) {
            return true;
        }
    }
    return false;
}
```

Notes:

- The non-silent edge must stay inside the SCC. An outgoing non-silent edge cannot witness
  infinitely many non-silent calls unless it is recurrent.
- The accepting state and the non-silent edge only need to be in the same SCC, not on the
  same edge. Strong connectivity gives a closed walk visiting both.
- The check uses `weight_t(SILENT)` rather than `0`, because `0` is the collapsed real-call
  weight in this construction.

### 3. Add the structural helper

```cpp
static bool hasAcceptingNonSilentNestedRun(NestedAutomaton* nwa) {
    if (!nwa) return false;

    Automaton* flat = nwa->flatten_regular(SumB, weight_t(0));
    const bool ok = hasReachableAcceptingSccWithNonSilentEdge(flat);
    delete flat;
    return ok;
}
```

Comment to include above the helper:

```cpp
// This is a Boolean nested-language check. SumB(0) is used only as a
// termination-obligation projector: all successful real child calls emit the
// single finite value 0, while silent parent transitions emit SILENT. The final
// SCC test is performed on the obligation automaton, not on the raw parent.
```

### 4. Integrate the shortcut in `isNonEmpty(...)`

Replace the current trivial block with:

```cpp
// Validate before shortcuts so value-trivial cases do not mask unsupported
// aggregator combinations.
if (!isSupportedNonEmptyCombination(infVal, finVal)) {
    QUAK_FAIL("isNonEmpty: unsupported aggregator combination");
}

// Value-trivial SumPlus branch. Values are automatically >= 0, but NQA
// acceptance is still structural and requires infinitely many real child calls
// with all obligations discharged.
if (finVal == SumPlus && x <= weight_t(0)) {
    return hasAcceptingNonSilentNestedRun(this);
}

// Value-impossible SumMinus branch under >= threshold semantics.
if (finVal == SumMinus && x > weight_t(0)) {
    return false;
}
```

Keep all later flattening code unchanged.

### 5. Tests to add

Add tests in `src/tests/correctness_tests/test_sum_sup_witness_edge_cases.cpp`, or split
them into a new file such as
`src/tests/correctness_tests/test_sumplusminus_trivial_shortcuts.cpp` if that keeps the
scope clearer.

#### A. SumPlus `x = 0` rejects unresolved background obligations

Fixture: `SUM_SUP_BACKGROUND_BLOCKS_ACCEPTANCE`

Expected `false` for:

- `Sup x SumPlus, threshold 0`
- `LimSup x SumPlus, threshold 0`
- `Inf x SumPlus, threshold 0`
- `LimInf x SumPlus, threshold 0`
- `LimSupAvg x SumPlus, threshold 0`

#### B. SumPlus `x = 0` rejects a finite non-silent prefix followed by a silent accepting loop

Fixture: `SUM_SUP_NO_NONSILENT_AFTER_PREFIX`

Expected `false` for the same parent aggregators as test A.

#### C. SumPlus `x = 0` accepts a valid recurring non-silent run

Fixture: `SUM_SUP_WITNESS_IMMEDIATE_DISCHARGE`

Expected `true` for the same parent aggregators as test A.

#### D. SumPlus `x < 0` uses the same structural predicate

Use the same three fixtures and at least one representative parent aggregator, preferably
`LimSup`, with threshold `-1`:

- unresolved background obligation: `false`
- finite non-silent prefix then silent accepting loop: `false`
- recurring non-silent run: `true`

This prevents the implementation from special-casing only `x == 0`.

#### E. SumMinus `x > 0` remains value-impossible

Fixture: `SUM_SUP_SUMMINUS_MIXED_SIGN_ABS_COST`

Expected `false` for:

- `Sup x SumMinus, threshold 1`
- `LimSup x SumMinus, threshold 1`
- `Inf x SumMinus, threshold 1`
- `LimInf x SumMinus, threshold 1`
- `LimSupAvg x SumMinus, threshold 1`
- `LimInfAvg x SumMinus, threshold 1`

#### F. Unsupported-combination behavior is explicit

If validation is added as recommended:

- `(LimInfAvg, SumPlus, threshold 0)` should raise the same unsupported-combination failure
  as `(LimInfAvg, SumPlus, threshold 1)`.

If the project intentionally supports the trivial structural subcase:

- `(LimInfAvg, SumPlus, threshold 0)` should be tested against the same three structural
  fixtures as tests A-C, and `(LimInfAvg, SumPlus, threshold 1)` should still fail or remain
  unsupported.

#### G. Optional structural-collapse sanity test

Add one tiny fixture where a real child has positive, zero, and negative edge weights but
can terminate on all variants. The `SumPlus x <= 0` shortcut should still answer according
to structural acceptance only. This pins down the intended `SumB(0)` projection behavior.

## Verification Commands

Build the affected targets:

```bash
cmake --build build --target \
  quak-nested \
  test_sum_sup_witness_edge_cases \
  test_emptiness_correctness \
  sum_sup_fix_compare \
  -j
```

Run targeted CLI checks for the three key SumPlus fixtures:

```bash
for f in Sup LimSup Inf LimInf LimSupAvg; do
  ./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_background_blocks_acceptance.txt non-empty $f SumPlus 0
  ./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_no_nonsilent_after_prefix.txt non-empty $f SumPlus 0
  ./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_witness_immediate_discharge.txt non-empty $f SumPlus 0
done
```

Expected output pattern per parent aggregator:

```text
0
0
1
```

Run negative-threshold checks:

```bash
./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_background_blocks_acceptance.txt non-empty LimSup SumPlus -1
./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_no_nonsilent_after_prefix.txt non-empty LimSup SumPlus -1
./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_witness_immediate_discharge.txt non-empty LimSup SumPlus -1
```

Expected:

```text
0
0
1
```

Run SumMinus positive-threshold checks:

```bash
for f in Sup LimSup Inf LimInf LimSupAvg LimInfAvg; do
  ./build/quak-nested src/tests/correctness_tests/inputs/sum_sup_summinus_mixed_sign_abs_cost.txt non-empty $f SumMinus 1
done
```

Expected:

```text
0
0
0
0
0
0
```

Run the regression binaries:

```bash
./build/test_sum_sup_witness_edge_cases
./build/test_emptiness_correctness
./build/sum_sup_fix_compare
```

Expected:

- all newly added shortcut tests pass;
- existing emptiness tests remain unchanged;
- `sum_sup_fix_compare` reports `mismatches=0`.

## Risk Assessment

### Risk: the SCC helper accidentally becomes a parent-only check

Mitigation: keep the helper input type as `Automaton* flat`, call it only after
`flatten_regular(SumB, 0)`, and name it `hasReachableAcceptingSccWithNonSilentEdge` rather
than something parent-specific.

### Risk: `SumB(0)` projection is implicit and fragile

Mitigation: add a code comment and the optional structural-collapse sanity test. If the
regular obligation backend changes later, replace this helper with an explicit
`flatten_termination_obligations()` mode rather than relying on SumB saturation details.

### Risk: unsupported combinations remain masked

Mitigation: add validation before shortcuts and add a test for `(LimInfAvg, SumPlus)`.

### Risk: one-state real children are ambiguous

Current code usually treats child `0` / dummy child as silent, and several paths also treat
children with fewer than two states as dummy-like. The shortcut plan should not broaden
that convention. If real one-state children are allowed in the input language, define their
semantics separately and add a fixture before changing this shortcut.

## Non-Goals

- Do not call `flatten_SumPlusMinus_Sup(...)` or `flatten_SumPlusMinus_Inf(...)` from the
  trivial `SumPlus x <= 0` shortcut.
- Do not change the W1/W2 witness-cached logic.
- Do not implement a raw parent SCC-only check.
- Do not refactor `flatten_regular(...)` unless the structural helper exposes a concrete
  performance or correctness problem.
- Do not change the semantics of `SumMinus`; only preserve the positive-threshold
  value-impossible shortcut after validation.
