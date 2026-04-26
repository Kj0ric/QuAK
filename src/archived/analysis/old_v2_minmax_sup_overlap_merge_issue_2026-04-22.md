# Old-v2 `Min/Max x Sup/LimSup` Overlap / Merge Issue

Date: 2026-04-22

## Purpose

This note documents the remaining correctness issue in the previous
`Min/Max x Sup/LimSup` backend implemented in:

- `src/NestedAutomaton_OLD_V2.cpp`

The issue is not the already-fixed same-symbol spawn bug. It is a different
bug class: overlapping invocations of the same child can be merged in the
flattened state representation when they should remain distinct.

The design goal for any follow-up fix is:

- preserve the old core idea as much as possible

meaning:

- keep the old parser-based explicit flattened construction
- keep the general witness/background split
- avoid a full migration to the newer threshold-obligation backend
- avoid a scalability regression from introducing a full multiset of child
  invocations into every flattened state

## Affected Backend

The affected code is the old-v2 implementation of:

- `NestedAutomaton::flatten_MinMax_Sup(...)`

in:

- `src/NestedAutomaton_OLD_V2.cpp`

This backend is used for both:

- `Sup x {Max_f, Min_f}`
- `LimSup x {Max_f, Min_f}`

The current remaining bad behavior is concentrated in the `Sup` cases. The same
representation problem is still relevant structurally for `LimSup`, but the
expanded checks currently expose concrete false positives only for `Sup`.

## Canonical Reproducer

The canonical failing input is:

- `src/tests/correctness_tests/inputs/max_merge_bug_complete.txt`

Its contents are:

```text
@PARENT
final: q0
a : 1, q0 -> q1
b : 0, q0 -> qd
c : 0, q0 -> qd

a : 0, q1 -> qd
b : 1, q1 -> q2
c : 0, q1 -> qd

a : 0, q2 -> qd
b : 0, q2 -> qd
c : 0, q2 -> q0

a : 0, qd -> qd
b : 0, qd -> qd
c : 0, qd -> qd

@CHILD 0

@CHILD 1
final: f
a : 0, s0 -> s1
b : 2, s0 -> s1
c : 0, s0 -> f
a : 0, s1 -> s1
b : 0, s1 -> s1
c : 0, s1 -> f
```

## Intended Semantics On This Input

The parent has one relevant cycle:

- `q0 -a-> q1 -b-> q2 -c-> q0`

On `a`, the parent invokes child `1`.
On `b`, the parent invokes child `1` again.

This creates two overlapping child invocations:

1. invocation `I_a`, spawned on `a`
2. invocation `I_b`, spawned on `b`

The child behavior is:

- from `s0`, reading `a` gives value `0` and moves to `s1`
- from `s0`, reading `b` gives value `2` and moves to `s1`
- from `s1`, reading `a` or `b` gives value `0` and stays in `s1`
- from `s0` or `s1`, reading `c` goes to final `f` with value `0`

So on the parent word `abcabcabc...`:

- `I_a` sees `a:0`, then `b:0`, then `c:0`, so:
  - `Max_f(I_a) = 0`
  - `Min_f(I_a) = 0`
- `I_b` sees `b:2`, then `c:0`, so:
  - `Max_f(I_b) = 2`
  - `Min_f(I_b) = 0`

Therefore:

- `Sup Max_f = 2`
- `Sup Min_f = 0`

Consequences:

- `Sup Max_f` must reject every threshold `> 2`
- `Sup Min_f` must reject every threshold `> 0`

## Observed Old-v2 Failure

The comparison harness `./build-review/minmax_sup_fix_compare_old_v2` reports
exactly these false positives on the reproducer:

- `Sup Max_f` at thresholds `{3, 4, 5, 6, 8, 10}`
- `Sup Min_f` at thresholds `{0.5, 1, 1.5, 2}`

These are true semantic errors, not merely disagreements between two
implementations. The regular flattening path rejects them, and the expected
return values above already show why they must reject.

## Old-v2 Core Representation

The old-v2 backend represents each flattened global state using:

- parent state id
- one `activation` bit per flattened child state
- one `tracking` bit per flattened child state
- either:
  - `@inactive@`, or
  - one distinguished witness triple `(witness_child_id, witness_child_state_id, witness_y)`

This is the relevant structure in `src/NestedAutomaton_OLD_V2.cpp`:

- `activation_from`
- `tracking_from`
- `inactive_from`
- `witness_child_id_from`
- `witness_child_state_id_from`
- `witness_y_from`

This representation works only if the combination:

- background obligations
- distinguished witness

never needs to distinguish two different invocations that occupy the same child
state at the same time.

That assumption is false on the reproducer.

## Root Cause

### 1. Coexistence is collapsed too aggressively

After reading `ab`, there can be:

- one older background invocation already at child state `s1`
- one newer invocation that is being treated as the distinguished witness,
  also at `s1`

Old-v2 has no state component that can express:

- "background token at `s1` exists"
- and also "witness token at `s1` exists"

as two separate obligations.

Instead, it stores:

- one bit for that child state in `activation`
- one bit for that child state in `tracking`
- one optional witness location

So the two invocations collapse into one location summary.

### 2. Background traversal explicitly skips the witness source location

In `explore_global_selection_min_max_supremum(...)`, old-v2 skips the witness
source location:

```cpp
if (!data->inactive_from &&
    child_id == data->witness_child_id_from &&
    child_state_id == data->witness_child_state_id_from) {
    explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
    return;
}
```

This is structurally reasonable if the witness location contains only the
distinguished witness.

It becomes wrong once the same child state can also hold a separate background
invocation. In that case, skipping the witness location skips too much: it
skips the collocated background obligation as well.

### 3. Spawn choices reuse the same collapsed state space

When the parent edge invokes a nontrivial child, old-v2 explores:

1. spawn as background only
2. if no witness exists yet, start tracking it as the new witness

That is done by mutating the same old/new activation and tracking arrays plus
the single witness triple. There is no distinct coexistence class for:

- background only at a location
- witness only at a location
- both background and witness at the same location

So overlap information is lost at the moment the new witness and an existing
background invocation collide.

## Exact Bad Behavior On The Reproducer

The failure can be seen directly in the old-v2 flattened automaton for
`Max_f` with threshold `3`.

Old-v2 constructs a binary flattened automaton that contains these relevant
states:

- `0/0000/0000/@inactive@`
- `1/0001/1111/@inactive@`
- `2/0001/0001/1/2/0`
- `3/0001/0001/1/2/0`

and these relevant edges:

```text
a : 0, 0/0000/0000/@inactive@ -> 1/0001/1111/@inactive@
b : 1, 1/0001/1111/@inactive@ -> 2/0001/0001/1/2/0
a : 0, 2/0001/0001/1/2/0 -> 3/0001/0001/1/2/0
b : 0, 2/0001/0001/1/2/0 -> 3/0001/0001/1/2/0
a : 0, 3/0001/0001/1/2/0 -> 3/0001/0001/1/2/0
b : 0, 3/0001/0001/1/2/0 -> 3/0001/0001/1/2/0
```

Interpretation:

1. Start in the initial global state with no witness and no active children.
2. After `a`, keep the spawned child only as background:
   - `0/0000/0000/@inactive@ -> 1/0001/1111/@inactive@`
3. On `b`, old-v2 starts a witness and emits a `1`-edge:
   - `1/0001/1111/@inactive@ -> 2/0001/0001/1/2/0`
4. The destination state should still remember that the older call from `a`
   remains alive as background while the newer call is the witness.
5. Instead, old-v2 has only one collapsed location summary at the colliding
   child state. The background obligation is no longer distinguishable there.
6. From that point, old-v2 reaches a reachable accepting SCC with supremum `1`,
   so the binary flattened automaton incorrectly accepts threshold `3`.

The important point is not the particular bit pattern. The important point is
that the state encoding has created an accepting SCC after a `1`-edge even
though no real child invocation can ever satisfy threshold `3`.

## Why This Is A Representation Bug, Not A Transition Bug

The already-fixed same-symbol issue was caused by the wrong order of processing
for freshly spawned children. That was a local transition bug.

This overlap/merge issue is different:

- the backend is losing semantic information in the flattened state itself
- once that information is gone, later transitions cannot recover it

So another small local transition tweak is unlikely to be enough unless it adds
at least a small amount of new state information.

## Goal For The Fix

The goal is not to discard the old design.

The goal is:

- preserve the old core idea
- repair the missing coexistence information only where it is needed
- avoid exploding the state space into a general multiset of child invocations

In practical terms, a good fix should try to keep:

- one main distinguished witness
- compact bitset-style background tracking
- the same explicit parser/worklist construction style

while refining only the part of the representation that currently conflates:

- background-only
- witness-only
- background-plus-witness

at the same child state.

## Potential Ways To Achieve This Goal

### Option 1. Add a dedicated "shadow background at witness location" flag

Minimal direction:

- keep the current witness triple
- keep global activation/tracking bitsets
- add one extra flag saying whether a separate background obligation also exists
  at the witness location

This is the closest to the current old-v2 idea. It may be enough for the
specific reproducer, but it needs careful checking for cases where the witness
location changes and where multiple collocated obligations accumulate over time.

### Option 2. Refine each child-state summary into a small coexistence class

Instead of plain activation/tracking bits, use a tiny finite class per child
state, for example something like:

- empty
- background only
- witness only
- background + witness

For `Max_f`, a slightly richer split may be needed if low/high witness progress
must remain distinguishable at a colliding state.

This is still much closer to old-v2 than a full obligation bag, and it matches
the underlying reason the current masked `Inf` fix works: coexistence must be
represented explicitly.

### Option 3. Refine only the witness child-state, keep all others bitset-based

Compromise direction:

- keep ordinary bitsets for non-witness locations
- attach a small side structure only for the witness location and possibly the
  witness child

This may preserve scalability better than a global per-state refinement, but it
is more delicate because witness movement can change which location receives the
special treatment.

## Recommendation

If the explicit objective is to preserve the old core idea, the best starting
point is likely:

- Option 1 if a very small patch is preferred and we are willing to test
  aggressively for hidden edge cases
- Option 2 if we want the cleanest semantically defensible repair while still
  staying close to the old architecture

Option 3 is plausible, but harder to reason about cleanly.

## Which `cpp` File This Note Should Be Combined With

If this note is going to live next to the implementation it describes, it
should be combined with:

- `src/NestedAutomaton_OLD_V2.cpp`

Reason:

- the issue is specific to the old-v2 `flatten_MinMax_Sup(...)` backend
- the current active backend in `src/NestedAutomaton.cpp` is not using this
  representation
- the "preserve the old core idea" goal is about repairing the old-v2 design,
  not documenting the current shared threshold backend

More specifically, the best place to associate it is the `MinMax x Sup/LimSup`
section around:

- `data_min_max_supremum_t`
- `explore_global_selection_min_max_supremum(...)`
- `explore_global_child_transition_min_max_supremum(...)`
- `NestedAutomaton::flatten_MinMax_Sup(...)`
