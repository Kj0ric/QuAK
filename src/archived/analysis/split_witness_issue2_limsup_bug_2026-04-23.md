# Archived Split-Witness LimSup False Positive From Dropped Background Overlap

Date: 2026-04-23

## Scope

This note is about the archived split-witness backend in:

- `src/NestedAutomaton_OLD_V2.cpp`
- main entry point:
  `NestedAutomaton::flatten_MinMax_Sup_split_witness(...)`
  at `src/NestedAutomaton_OLD_V2.cpp:5655-5754`

It is not about the current production backend. In the live code,
`flatten_MinMax_Sup(...)` immediately delegates to the threshold-extremal
construction, and `flatten_MinMax_Sup_split_witness(...)` is only an alias:

- `src/NestedAutomaton.cpp:5933-6037`

The current backend gets this case right.

## Executive Summary

There is a real archived split-witness bug on a complete deterministic
one-child example:

- `Sup x Max_f` at threshold `1` is correctly reported as nonempty
- `LimSup x Max_f` at threshold `1` should be empty
- the archived split-witness backend reports it as nonempty

The immediate reason is a wrong transition in the flattened automaton:

- when the same child has two simultaneously active background copies in
  different local states
- the split-witness traversal propagates only one of those states
- the older blocking copy is silently dropped

That bad edge is sufficient to create a bogus accepting SCC with infinitely
many `1`-edges, so the `LimSup` emptiness check returns `true`.

## Counterexample Fixture

Saved as:

- `src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt`

Automaton:

```quak
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

Important properties:

- the child is deterministic
- the child is complete on all non-final states
- there is only one real child
- `s2` is a nonterminating blocker
- a child can get value `1` only by reading `c` while in `s0`

## Expected Semantics

This nested automaton should satisfy:

- `Sup(Max_f) >= 1`
- `LimSup(Max_f) < 1`

### Why `Sup >= 1`

Take the word:

- `c b^omega`

The first spawned child sees:

- `c`: `s0 -> s0` with weight `1`
- `b`: `s0 -> s1`
- next `b`: `s1 -> f`

So that child terminates with `Max_f = 1`.

Every later child spawned on `b` terminates in one more `b`, but only with
value `0`. Therefore the return sequence is:

- `1, 0, 0, 0, ...`

So `Sup = 1`.

### Why `LimSup < 1`

To get infinitely many returns with value `1`, infinitely many children must:

- read `c` while still in `s0`
- and later terminate

But once a child has moved from `s1` to `s2`, it can never terminate. In
particular, after the prefix:

- `b c`

the child spawned on the initial `b` should evolve as:

- `s0 --b--> s1`
- `s1 --c--> s2`

and remain alive forever.

So any run that tries to keep generating new `1`-valued children using `c`
also keeps the original `s2` blocker alive forever. Under the nested
semantics, such runs are invalid because not all started children return.

Hence `LimSup >= 1` should be false.

## Observed Results

### Current live backend

Command:

```bash
./quak-nested \
  src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt \
  non-empty Sup Max_f 1 \
  non-empty LimSup Max_f 1
```

Observed output:

```text
isNonEmpty(Sup, Max_f, threshold=1) = 1
isNonEmpty(LimSup, Max_f, threshold=1) = 0
```

This matches the intended semantics.

### Archived split-witness backend vs regular oracle

Using the one-off probe:

- `analysis/issue2_compare_probe.cpp`

Observed output:

```text
... inf=Sup    fin=Max_f threshold=1 split=1 regular=1
... inf=LimSup fin=Max_f threshold=1 split=1 regular=0
```

So the archived split-witness backend disagrees with the regular oracle only
on `LimSup`.

## Where The Wrong Flattened Edge Comes From

The critical code is:

- `explore_global_selection_min_max_sup_split(...)`
- `src/NestedAutomaton_OLD_V2.cpp:5414-5478`

The key control-flow fact is:

- after processing one active local state of child `child_id`
- the recursion jumps to `child_id + 1`
- it does not continue with the next local state of the same child

Concretely:

- `src/NestedAutomaton_OLD_V2.cpp:5444`
- `src/NestedAutomaton_OLD_V2.cpp:5460`

So on a single parent letter, this procedure can propagate at most one active
background local state per child.

## The Critical Overlap State

Using the dumped archived split-witness flattening, the relevant inactive state
after the initial `b` is:

- `0/00010/11111/@inactive@`

In this encoding:

- `00010` means the background obligation sits in Child 1 state `s1`
- `11111` means the current epoch is tracking everything
- `@inactive@` means there is no distinguished witness currently active

Now read one `c`.

What should happen semantically:

- the old background copy in `s1` should move to `s2`
- the freshly spawned copy on `c` should stay in `s0`

So after that `c`, the background summary should represent both:

- one active copy in `s2`
- one active copy in `s0`

But the dumped split-witness flattened automaton instead contains:

- `0/00010/11111/@inactive@ --c--> 0/00100/00100/@inactive@`

That destination carries only the `s2` copy. The fresh `s0` copy is gone in
that inactive branch.

In the other branch family, the opposite loss also happens: the fresh `s0`
copy survives while the older `s1 -> s2` blocker is not propagated. Either
way, the flattening is not preserving the true per-invocation successor set.

The important point is that the graph is already wrong at this local step,
before any `Sup` or `LimSup` emptiness condition is applied.

## Why This Produces A `LimSup` False Positive

Once the old blocker is dropped, the archived split-witness flattening admits
the lasso:

- `b (ccbb)^omega`

Operationally:

1. `b` creates the first pending copy in `s1`
2. the first `c` is the overlap step where the old blocker is mishandled
3. the second `c` starts a fresh witness and gives it `y = 1`
4. the next `b` moves that witness to `s1`
5. the next `b` terminates the witness with edge weight `1`
6. the automaton returns to the same final inactive shape and repeats

So the archived flattened automaton has:

- infinitely many visits to final states
- infinitely many `1`-edges

and therefore its standard `LimSup` emptiness check returns nonempty.

But under the real nested semantics, the child spawned by the very first `b`
should already have reached `s2` and should still be alive forever. So the
flattened cycle is spurious.

## Is The Root Cause “Same Flattening For Sup And LimSup”?

Not in the strong sense “the flattening must be different for the two
objectives.”

A single flattened weighted automaton can correctly serve both `Sup` and
`LimSup`, provided the graph itself is semantically correct. The current live
threshold-extremal backend does exactly that:

- both `Sup` and `LimSup` are routed through the same flattening path in
  `src/NestedAutomaton.cpp:10252-10259`
- the difference is applied later by
  `Automaton::isNonEmpty_withFinal(infVal, ...)`
  at `src/NestedAutomaton.cpp:10309`

On this same fixture, that live backend returns:

- `Sup = 1`
- `LimSup = 0`

So “same flattened graph for Sup and LimSup” is not by itself the bug.

What is true is weaker and more precise:

- the archived split-witness construction builds one shared graph for both
  objectives
- that shared graph is unsound because it drops background obligations under
  overlap
- the unsoundness happens to become visible only on `LimSup` here, because
  `Sup` was already true for independent reasons

So the symptom is objective-specific, but the immediate bug is a wrong
transition relation in the shared flattening.

## Why The Symptom Appears Only On `LimSup`

For this fixture:

- `Sup = 1` is already true in the real nested semantics
- the bug does not need to change that

What the bug changes is:

- it creates a bogus cycle with infinitely many successful witness returns

That affects `LimSup`, not `Sup`.

So the clean reading is:

- `Sup` agrees because the instance already has a genuine one-time success
- `LimSup` disagrees because the wrong flattened graph invents infinitely many
  recurring successes

## Bottom Line

The archived split-witness backend has a confirmed semantic bug on this
complete deterministic example.

The direct cause is:

- lossy background propagation in
  `explore_global_selection_min_max_sup_split(...)`

not merely the fact that the same flattened automaton is reused for both
`Sup` and `LimSup`.

If we want to test the stronger hypothesis that the split-witness design also
needs a genuinely different `LimSup`-specific flattening even when overlap is
handled correctly, the right next step is to search for a counterexample with:

- no same-child overlap at all
- but still a `Sup`/`LimSup` discrepancy in the archived backend

This fixture does not establish that stronger claim.

## Patch Status In This Checkout

The current checkout now contains a surgical fix in
`src/NestedAutomaton_OLD_V2.cpp`:

- after handling one active background local state of a child
- the traversal continues with the next local state of the same child
- it does not jump immediately to the next child anymore

Concretely, the recursive calls in the branches corresponding to:

- final successor handling
- non-final successor handling
- old-background `succs == null` skipping

were changed to continue with `child_state_id + 1`.

Validation after the patch:

- `build-review/minmax_sup_split_witness_probe` now reports
  `mismatches=0/40`, including the saved fixture
  `split_witness_issue2_limsup_false_positive.txt`
- `build-review/minmax_sup_fix_compare_split_old_v2` reports
  `queries=1296 oracle_queries=1296 mismatches=0`

So the specific `LimSup` false positive documented in this note is fixed in the
current checkout.
