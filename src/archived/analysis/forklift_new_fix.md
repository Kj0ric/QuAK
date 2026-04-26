# Forklift And Universality Changes From Original QuAK

This note summarizes the implemented changes relative to the original QuAK repo
copy under `QuAK REPO/`. It is intentionally limited to the Forklift
membership fix and the final-aware universality check.

## Forklift Membership

Original QuAK's `src/FORKLIFT/inclusion.cpp` used this shape in the
`FRESH membership` block:

```text
dfs1: find a reachable final product node
dfs2/dfs3: after that, accept any reachable threshold cycle
```

The cycle test was based on a global active stack `P`. That meant Forklift could
accept a lasso where a final state appeared only before the eventual cycle:

```text
q0 final, initial
q1 non-final

q0 --a/1--> q1
q1 --a/1--> q1
```

For period `a`, threshold `1`, original Forklift incorrectly reported
membership: it saw final `q0`, then accepted the high `q1` cycle even though
the repeated cycle never visits a final state.

The current version changes only the concrete membership block:

- removes the old `P` stack and `membership_query_dfs3`;
- adds `C`, a per-final-root visited set keyed by
  `(state, period_position, seen_threshold)`;
- replaces `membership_query_dfs2` with `membership_query_final_cycle`;
- accepts only a non-empty closed walk from a selected final product node back
  to that same product node, with a threshold edge somewhere on the walk.

The resulting condition is:

```text
exists reachable final product node f
and exists non-empty closed walk f -> ... -> f
whose edges include weight >= threshold
```

This still accepts the important SCC case where finality and the threshold edge
live on different smaller cycles. If both are in the same reachable SCC, there
is a closed walk from the final product node through the threshold edge and back.

This change does not alter Forklift's public interface:

- `inclusion(...)` is unchanged;
- `membership(...)` is unchanged;
- `fast_membership(...)` is still the inclusion loop's concrete target query;
- no backend selection, periodic oracle, or universality-source switch is
  introduced.

It also deliberately leaves `ContextOf`, `FixpointLoop`, witness construction,
and stem computation alone.

## Final-Aware Universality

Original QuAK had only:

```cpp
bool Automaton::isUniversal(value_function_t f, weight_t x, ...);
```

That method builds `constantAutomaton(this, x)` and checks inclusion into the
target automaton. Semantically, it checks all words over the alphabet:

```text
forall w in Sigma^omega: A(w) >= x
```

For nested automata after flattening, this is too strong. The flattened
automaton may reject words that are not valid flattened accepting runs. Nested
universality needs:

```text
forall w: if w in L(A), then A(w) >= x
```

So the current repo adds:

```cpp
bool Automaton::isUniversal_withFinal(value_function_t f, weight_t x, ...);
```

Its helper, `acceptedLanguageConstantAutomaton(A, x)`, copies `A`'s alphabet,
states, initial state, transitions, and final flags, but replaces every edge
weight with `x`. Therefore the constant-side automaton has exactly the same
accepted language as `A`, not full `Sigma^omega`.

Then:

```cpp
C->isIncludedIn(this, f, false, witness)
```

checks `x <= A(w)` only for words accepted by `A`. If `L(A)` is empty, the
result is vacuously true.

`Automaton::isUniversal(...)` remains available and keeps its original
all-words meaning.

## Nested Universality Call Site

Original nested universality flattened the nested automaton, removed silent
transitions, and then called:

```cpp
nonSilent->isUniversal(infVal, x)
```

The current repo instead calls:

```cpp
nonSilent->isUniversal_withFinal(infVal, x)
```

This makes nested universality quantify only over accepted flattened words.
Rejected flattened words no longer produce false counterexamples.

## Regression Coverage

The active coverage lives in
`src/tests/correctness_tests/test_universality_correctness.cpp`. Its focused
accepted-domain/Forklift section covers:

- partial domains ignore rejected words;
- empty accepted language is vacuously universal;
- low accepting loops fail thresholds above their value;
- nondeterministic accepting runs use best-run semantics;
- Forklift rejects a transient final followed by a high non-final loop;
- Forklift accepts combined final and threshold subcycles in the same SCC;
- `isUniversal_withFinal` rejects a high non-accepting run when accepted words
  stay below threshold.
