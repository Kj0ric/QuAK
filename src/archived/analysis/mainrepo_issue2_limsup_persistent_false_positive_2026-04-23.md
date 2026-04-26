# Main-Repo `LimSup x Max_f` Issue 2 Still Persists After The Surgical Patch

Date: 2026-04-23

## Scope

This note explains the only remaining mismatch from the broader comparison
between:

- the patched main-repo copy in
  [NestedAutomaton_mainRepo_sup_max_compare.cpp](./NestedAutomaton_mainRepo_sup_max_compare.cpp)
- the live threshold-extremal implementation in
  [src/NestedAutomaton.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:1)

for:

- `isNonEmpty(Sup, Max_f, threshold)`
- `isNonEmpty(LimSup, Max_f, threshold)`

using all correctness fixtures under `src/tests/correctness_tests/inputs/`
and thresholds derived from the actual parent/child weight sets.

The remaining mismatch is exactly:

- `split_witness_issue2_limsup_false_positive`
  - `LimSup x Max_f` at thresholds `0.5` and `1.0`

Direct probe on the patched main-repo copy versus the regular oracle:

```text
threshold=0.5 sup_main=1 limsup_main=1 sup_regular=1 limsup_regular=0
threshold=1 sup_main=1 limsup_main=1 sup_regular=1 limsup_regular=0
```

So:

- `Sup x Max_f` is still correct on this fixture
- `LimSup x Max_f` is still a false positive in the patched main-repo copy

## Exact Input

This is the full fixture as it exists in the repo:

File:

- [split_witness_issue2_limsup_false_positive.txt](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt:1)

```text
# Archived split-witness false positive for LimSup x Max_f at threshold 1.
#
# Current live backend:
#   isNonEmpty(Sup, Max_f, 1)    = 1
#   isNonEmpty(LimSup, Max_f, 1) = 0
#
# Archived old-v2 split-witness backend:
#   split_witness.sup.max.thr1    = 1
#   split_witness.limsup.max.thr1 = 1
#
# The critical overlap is after reading the prefix "bc":
# - the old background copy from the initial b-step should move s1 -> s2
# - the fresh copy spawned on c stays in s0
# - the split-witness traversal processes only one active state of Child 1,
#   so it keeps the fresh s0 copy and drops the old s2 blocker
#
# After that loss, the flattened automaton admits the accepting lasso
#   b (ccbb)^omega
# with infinitely many 1-edges, while the true nested semantics still has the
# original nonterminating s2 obligation alive forever.

@PARENT
final: all
a : 1, q -> q
b : 1, q -> q
c : 1, q -> q

@CHILD 0

@CHILD 1
final: f
a : 0, s0 -> s0
b : 0, s0 -> s1
c : 1, s0 -> s0

a : 0, s1 -> s2
b : 0, s1 -> f
c : 0, s1 -> s2

a : 0, s2 -> s2
b : 0, s2 -> s2
c : 0, s2 -> s2
```

## Why The Patch Did Not Fix It

The surgical patch fixed three different bugs:

- fresh spawn must consume the spawning symbol
- completion must be detected from `tracking_to`
- acceptance credit must persist across steps

Those changes are real, and they removed the previous `20`-query mismatch set.
But they do **not** change the structural traversal in
`explore_global_selection_min_max_supremum(...)`.

That function still explores at most **one active state per child per symbol**.

The critical structure is:

1. scan child states in increasing order
2. stop at the first active state
3. process one successor of that state
4. recurse to `child_id + 1`, not to the next state of the same child

In the patched copy this is still visible in:

- `explore_global_selection_min_max_supremum(...)` in
  [NestedAutomaton_mainRepo_sup_max_compare.cpp](./NestedAutomaton_mainRepo_sup_max_compare.cpp)
- `explore_global_parent_transition_min_max_supremum(...)` in
  [NestedAutomaton_mainRepo_sup_max_compare.cpp](./NestedAutomaton_mainRepo_sup_max_compare.cpp)

The decisive combination is:

```cpp
// parent transition injects the fresh spawn into old_activation
data->old_activation[ii] = 1;
```

and then, once selection handles one active child-state:

```cpp
explore_global_selection_min_max_supremum(child_id + 1, 0, data);
```

That means the backend still behaves like:

- "pick one active state of child 1 for this symbol"

instead of:

- "advance all active states of child 1 for this symbol"

## Exact Failure Path On This Input

The surviving bug does **not** require the witness/background same-state case.
It already appears on the plain background branch.

### Step 1: read `b`

Take the background-only spawn branch.

The child spawned on `b` starts in `s0` and consumes `b`:

- `s0 --b/0--> s1`

So after the first letter, child 1 has one active background copy in `s1`.

### Step 2: read `c`

Again take the background-only branch.

Before selection starts, parent-transition code injects the fresh `c`-spawn into
`old_activation` at child state `s0`.

So at the start of child-1 selection on this step, the active set is:

- old copy from the first `b`: `s1`
- fresh copy spawned on `c`: `s0`

Now selection scans states in order:

- `s0` comes before `s1`
- `s0` is active
- it processes `s0 --c/1--> s0`

But after processing `s0`, the recursion jumps to the next child:

- it does **not** continue with child 1 state `s1`

So the old background copy never gets its required transition:

- intended: `s1 --c/0--> s2`
- actual in the patched copy: that transition is skipped entirely

The destination set is therefore computed as if only the fresh `s0` copy existed.

That is the exact persistent defect.

## Why This Creates A `LimSup` False Positive

In the true nested semantics, the first `b`-spawned child should become:

- `s1` after the `b`
- then `s2` after the next `c`

and `s2` is a non-final sink forever. So one unresolved child remains active for
the rest of the run, which blocks the repeated epoch completions needed for
`LimSup`.

In the patched main-repo copy, that blocker is deleted on the second letter.
After that, the flattened automaton can follow the lasso already named in the
fixture header:

```text
b (ccbb)^omega
```

and repeatedly:

- spawn a fresh good child on `c`
- obtain a `1` edge for the tracked good child
- finish the tracked obligations again

Since the parent is final on every step, once the original blocker is lost the
patched backend can emit infinitely many accepting pulses. That is why it
returns `true` for `LimSup x Max_f` at thresholds `0.5` and `1.0`.

The regular oracle and the live threshold-extremal backend keep the blocker, so
they correctly reject.

## Why The Live Backend Rejects

The live threshold-extremal path in [src/NestedAutomaton.cpp](/home/ege/Desktop/QuAK-playground%20%2828.01.26%2015%E2%88%B612%29/src/NestedAutomaton.cpp:1)
does not use the "one active state per child" recursion.

Its state carries obligation bags:

- `P1`
- `P2`

inside `ThrExtBuchiState`, and new obligations are inserted with
`thrext_bag_add(...)`.

That representation keeps all outstanding child obligations simultaneously, so
after the prefix `bc` it can still remember both:

- the fresh `s0` copy
- the old blocker that moved to `s2`

With the `s2` blocker preserved, the repeated `LimSup` acceptance pulses never
materialize, and the live backend correctly rejects.

## Bottom Line

The remaining Issue 2 mismatch persists for one precise reason:

- the patched main-repo copy still advances only one active state per child on
  each symbol

The fresh-spawn and acceptance-phase fixes were necessary, but they do not
touch this older overlap-loss mechanism. On this fixture, that older mechanism
is exactly what still causes the `LimSup x Max_f` false positive.
