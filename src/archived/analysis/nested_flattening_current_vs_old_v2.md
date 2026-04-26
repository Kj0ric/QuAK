# Nested Flattening: Current vs `OLD_V2`

This note compares the **algorithms that actually run today** in
`src/NestedAutomaton.cpp` with the algorithms implemented in
`src/NestedAutomaton_OLD_V2.cpp`.

The focus is not on refactoring or line-by-line code motion. The point is to
explain the algorithmic ideas behind the flatteners, what information each one
stores in the flattened state, and how that changes the construction.

## Scope

Only the cases with a real algorithmic change are discussed in detail.

These pieces are effectively unchanged between the two files:

- `flatten_regular`
- `makeCompleteNested`
- `determinizeWithMacroAlphabet`
- `synchronizeChildren`
- `compute_c_bound`
- `flatten_Avg_SumMinus`

The significant differences are all in the threshold-based flatteners:

- `SumPlus` / `SumMinus` with `Sup` / `LimSup`
- `SumPlus` / `SumMinus` with `Inf` / `LimInf`
- `Max_f` / `Min_f` with `Sup` / `LimSup`
- `Max_f` / `Min_f` with `Inf` / `LimInf`

One important clarification:

- In the current file, the old specialized threshold bodies are still present
  below early `return` statements.
- The live algorithm is the shared backend centered around
  `flatten_threshold_extremal_impl`.
- So when this note says "current algorithm", it means the live behavior, not
  the dead legacy code still left in the file.

## Big Picture

`OLD_V2` uses **specialized flatteners** tailored to each semantic family.
Those flatteners are built around the structure of the specific objective:

- single distinguished witness token for `Sup`-style cases
- exact budget vectors for `Inf`-style sum cases
- single monotone witness bit for `Sup`-style min/max cases
- full synchronous product over all active children for `Inf`-style min/max

The current implementation replaces those with a single
**threshold-obligation construction**:

- a flattened state stores the parent control state plus two bags of
  obligations, `P1` and `P2`
- each obligation says: "this child call was guessed to end up on one side of
  the threshold, and here is the frontier of child configurations still
  consistent with that guess"
- an obligation carries only the information needed to keep the guess viable
  until the child terminates

Conceptually, `OLD_V2` often reasons in terms of a **special role**:
"which active child is the witness?" or "what exact budget does each active
child still have?". The current code instead reasons in terms of
**spawned commitments**: every non-silent child call may create a pending claim
that must later be discharged.

That shift matters differently in each objective family.

## Case 1: `SumPlus` / `SumMinus` with `Sup` / `LimSup`

### `OLD_V2` idea

`OLD_V2::flatten_SumPlusMinus_Sup` treats the flattening as a
**single-witness problem**.

The flattened state stores:

- the parent state
- activation bits for all child states
- tracking bits for the current Buchi epoch
- either:
  - `@inactive@`, meaning no child is currently the quantitative witness, or
  - one explicit witness `(child_id, child_state, budget_set)`

The key idea is:

- many children may be active in the background
- but only **one** active child contributes to the 0/1 weight used to witness
  threshold achievement

When a parent edge calls a nontrivial child, `OLD_V2` branches in two ways:

1. Spawn the child only as a background child.
   The flattened edge gets weight `0`.
2. If no witness is currently active, nominate the spawned child as the
   witness.
   The flattened edge gets weight `1`, and the witness is initialized with an
   "unlimited" budget.

The witness budget is an exact `BudgetSet` of values still compatible with
success. Child edges update that budget exactly by interval arithmetic.

Background children are much cheaper:

- they only carry activation/tracking bits
- they are propagated symbol by symbol
- they matter for Buchi epoch discipline, not for the quantitative edge weight

So the old algorithm is:

- one distinguished quantitative thread
- everything else is qualitative liveness bookkeeping

This is a very direct encoding of a `Sup` intuition:
"it is enough that some call eventually achieves the threshold".

### Current idea

The current backend no longer picks one distinguished witness.

Instead, a flattened state stores:

- parent control
- `P1` and `P2`, two bags of obligations
- epoch metadata (`parent_phase`, `epoch_nonempty`)

Each obligation contains:

- the child index
- a guess bit: child return `< threshold` or `>= threshold`
- a frontier of child states, where each frontier entry also stores a progress
  value

For sum objectives, that progress is capped accumulated cost. The frontier is
then canonicalized per child state by keeping only the "best" progress value
for the guessed side of the threshold.

The important change is that the current algorithm can keep
**multiple pending threshold bets alive at the same time**.

It does not ask:

- "which active child is the witness?"

It asks:

- "which guessed threshold obligations are still alive, and what frontier of
  child runs still supports each one?"

### Why this is a real algorithmic change

`OLD_V2` encodes `Sup` through a **single quantitative token**.

Current encodes it through a **set of independently spawned obligations**.

This changes the shape of the flattened state fundamentally:

- old: one explicit budget-bearing witness plus background activity
- current: many obligations, each with its own local progress summary

The old algorithm is closer to a "pick one witness run" construction.
The current one is closer to a "track all unresolved threshold claims" construction.

## Case 2: `SumPlus` / `SumMinus` with `Inf` / `LimInf`

### `OLD_V2` idea

`OLD_V2::flatten_SumPlusMinus_Inf` is not witness-based.
Here the old algorithm takes the opposite direction:
it tracks **all currently active child calls explicitly**.

The flattened state stores:

- parent state
- for every flattened child-state position, an exact `BudgetSet`
- activation bits
- tracking bits
- parent epoch phase

The intuition is:

- under `Inf`-style semantics, every active child relevant to the current
  epoch matters
- there is no distinguished witness
- therefore the flattener should propagate all active child calls
  simultaneously

The old construction does exactly that.

For every symbol:

- it enumerates successors of every active non-final child state
- it computes the new exact budget by interval subtraction
- if multiple paths land in the same child state, it intersects their budget
  sets to keep only mutually consistent values

This is a very concrete algorithm:

- each active child carries its exact remaining threshold budget
- the flattened state literally stores the full quantitative state of the
  current active multiset

Termination is also handled exactly:

- a final child edge checks whether the old budget and the terminating edge can
  satisfy the threshold
- failure changes the flattened edge weight to `0`, but exploration can still
  continue, which is important for `LimInf`

So the old algorithm is a **global exact-budget synchronous construction**.

### Current idea

The current backend no longer stores a full vector of exact budgets for all
active children.

Instead, it returns to obligation tracking:

- every spawned child call may create an obligation with a guessed side of the
  threshold
- for sum objectives, each obligation stores only local progress plus viability
  summaries
- precomputed `min_extra` and `max_extra` values summarize how much additional
  cost can still be forced or avoided from a child state

This means the current construction reasons about feasibility through:

- local progress inside an obligation
- static viability summaries for the child automaton
- a frontier of child states still compatible with the guess

It does **not** carry the full exact quantitative state of every active child.

### Why this is a real algorithmic change

This is the cleanest old-vs-current contrast in the whole file.

`OLD_V2` idea:

- exact global quantitative state
- one budget set per active child-state position
- merge by budget intersection

Current idea:

- local threshold obligations
- summarized future viability
- no exact budget vector for the whole active set

So the difference is not "specialized vs unified" in a cosmetic sense.
It is:

- **global exact propagation** in `OLD_V2`
- **local summarized obligations** in current

## Case 3: `Max_f` / `Min_f` with `Sup` / `LimSup`

### `OLD_V2` idea

`OLD_V2::flatten_MinMax_Sup` mirrors the old sum-sup construction, but replaces
numeric budget tracking with a single monotone bit `y`.

The flattened state stores:

- parent state
- activation bits
- tracking bits
- either no witness, or one explicit witness
  `(witness_child_id, witness_child_state, witness_y)`

The bit `y` means:

- for `Max_f`: has some edge `>= threshold` been seen so far?
- for `Min_f`: have all edges so far stayed `>= threshold`?

Again, the central idea is:

- exactly one active child may be the quantitative witness
- all other active children are only background obligations

Parent spawning branches into:

1. background-only spawn with flattened weight `0`
2. witness spawn with flattened weight `1`, if no witness exists

The witness transition updates `y` monotonically:

- `Max_f`: `y := y OR pass`
- `Min_f`: `y := y AND pass`

The witness must terminate with `y = 1`.

There is also additional doomed-state pruning:

- `OLD_V2` precomputes `can_reach_final`
- if a tracked active child can never reach a final child state, the run can no
  longer complete an epoch and is sent to sink

So the old algorithm is still a **single-witness construction**, just with a
boolean monotone progress measure instead of a budget set.

### Current idea

Current again removes the distinguished witness and turns the problem into
obligations.

Each obligation stores:

- which child call it belongs to
- the guessed side of the threshold
- a frontier of child states with a local progress value `prog`

For min/max objectives, `prog` is just the old witness bit in abstract form:

- `0` or `1`

The crucial difference is that current does not keep one explicit witness child.
It can carry multiple min/max threshold obligations simultaneously.

Instead of `can_reach_final`, current precomputes `mm_live` tables:

- for every child state
- for each guess
- for each local progress bit

these tables answer whether the guess is still live from that configuration.

### Why this is a real algorithmic change

The underlying local progress value is similar in both versions:
it is still the old monotone bit.

But the outer construction is very different.

`OLD_V2` says:

- choose one witness child
- update one witness bit
- propagate everyone else only as background obligations

Current says:

- spawn and carry arbitrary many threshold obligations
- each obligation has its own local bit
- liveness is checked through precomputed viability tables

So even though the child-local progress notion is similar, the
**global search structure** is different.

## Case 4: `Max_f` / `Min_f` with `Inf` / `LimInf`

### `OLD_V2` idea

This is the biggest conceptual difference.

`OLD_V2::flatten_MinMax_Inf` is a dedicated simplified algorithm based on the
observation that for `Inf`-style semantics:

- there is no need to guess a witness
- all relevant active children must succeed

So the old flattener does not build obligations at all.
Instead, it keeps a direct global synchronous representation.

The flattened state stores:

- parent control
- parent phase
- per child-state status
- per child-state tracked bit
- `epoch_nonempty`

For `Max_f`:

- status is `inactive`, `active+low`, or `active+high`
- "high" means some edge `>= threshold` has already been seen on that child run

For `Min_f`:

- status is just active/inactive
- a low edge is immediate failure, because once `Min_f` sees a bad edge it can
  never recover

At each parent step, the old algorithm:

1. spawns any newly called child as active
2. collects possible moves for every active child
3. enumerates the Cartesian product of those move sets
4. emits a flattened edge with weight `1` exactly when the whole combination
   succeeds, and `0` otherwise

This is a very strong global idea:

- the flattened edge summarizes whether **all simultaneously active children**
  satisfied the step condition

No threshold guess is stored, no witness is chosen, and no per-call obligation
is spawned.

This is an "all active children at once" product construction.

### Current idea

The current implementation no longer uses that simplification as the live path.

Instead, it falls back to the same threshold-obligation machinery used for the
other threshold cases:

- spawn obligations on child calls
- attach a guessed side of the threshold
- maintain frontiers and discharge conditions until finality

So the current algorithm abandons the old global synchronous-product viewpoint.

### Why this is a real algorithmic change

This is not a minor variant.
It is a different way of thinking about the problem.

`OLD_V2`:

- exploit `Inf` semantics aggressively
- avoid guessing entirely
- maintain the exact global product of all active children
- compute the flattened edge weight from whole-step success/failure

Current:

- reduce the case to local obligations again
- do not keep the direct global product as the main live construction

So this is the largest semantic jump between the two versions.

## Cross-Cutting Difference: Silent Parent Steps

There is also a genuine cross-cutting change in how threshold flatteners encode
silent parent steps.

In `OLD_V2`, the specialized flatteners typically used the identity weight of
the relevant 0/1 objective:

- `0` in the `Sup`-style witness constructions
- `1` in the `Inf`-style sum construction

In the current live backend, silent parent steps are emitted as actual
`SILENT` edges, not as ordinary `0` or `1` weighted edges.

This does not just affect printing. It changes the interface between the
flatteners and the later silent-transition removal pass.

## What The Old File Is Trying To Optimize For

Seen as a whole, `OLD_V2` is not merely "older code". It reflects four
different objective-specific insights:

- `Sup`-style sum: one quantitative witness is enough
- `Inf`-style sum: all active quantitative budgets must be propagated exactly
- `Sup`-style min/max: one witness bit is enough
- `Inf`-style min/max: no guessing is needed; check all active children at once

These ideas make the old code heterogeneous, but each heterogeneity is tied to
an objective-specific simplification.

## What The Current File Is Trying To Optimize For

The current live path gives up several of those specialized simplifications in
exchange for a single obligation-based model:

- one state shape
- one spawn/step/discharge discipline
- one Buchi epoch mechanism
- objective-specific differences pushed into local progress and viability rules

So the current code is not just a cleaned-up version of the old algorithms.
It changes the algorithmic center of gravity:

- from exact specialized encodings
- to one obligation calculus that is reused across threshold cases

## Bottom Line

The real differences are these:

- `OLD_V2` often stores the **thing being optimized directly**:
  one witness, one witness bit, or an exact budget vector.
- The current implementation stores **pending threshold claims** and only the
  local progress needed to keep those claims alive.

That distinction is mild in the `Sup` min/max case, large in the sum cases, and
very large in `MinMax + Inf`, where `OLD_V2` had a bespoke global-product
algorithm and the current code no longer does.
