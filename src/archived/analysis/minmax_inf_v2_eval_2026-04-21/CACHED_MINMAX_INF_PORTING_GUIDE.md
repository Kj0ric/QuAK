# Cached MinMax `Inf/LimInf` Porting Guide

This document explains how to carry the current cached
`Inf/LimInf x {Max_f, Min_f}` implementation into another version of
`NestedAutomaton.cpp`.

The companion code-oriented reference is:

- [CACHED_MINMAX_INF_PORTING_REFERENCE.cpp](./CACHED_MINMAX_INF_PORTING_REFERENCE.cpp)

This guide is the integration playbook. The `.cpp` file is the transplant map.

## Goal

Move the current cached Min/Max backend to a different `NestedAutomaton.cpp`
without:

- changing semantics
- reintroducing the old use-after-free bug
- silently drifting away from the shared threshold backend
- accidentally mixing cached and non-cached state representations

## What The Cached Backend Is

The cached backend is **not** a separate semantics implementation.

It keeps the same Min/Max threshold-obligation semantics as the shared
threshold backend and only changes:

- how obligations are represented
- how bags are represented
- how flattened states are keyed
- how spawn/step transitions are memoized

In the current tree:

- the shared threshold backend is the semantic base
- the cached backend is an optimized Min/Max-specific representation layer on
  top of that base

That distinction matters. If you port the cached backend but fork the Min/Max
semantics, the port is already wrong.

## What The Cached Backend Depends On

The cached implementation assumes the target version has, or will receive, the
following prerequisites:

### Mandatory prerequisites

- `edgeWeightToChildIndex(...)`
- `ChildTables`
- `build_child_tables(...)`
- `thrext_int_t`
- `THREXT_INF`
- `ThrExtMode`
- `ThrExtChildInfo`
- `thrext_build_child_info(...)`

### Strongly recommended prerequisites

- the experiment stats plumbing
- `ScopedStatsTimer`
- `mmexp_enabled()`
- the public stats accessors in `NestedAutomaton`

These are not essential for semantics, but they make the port testable and
comparable to the current implementation.

## The Current Structure

In the current source tree, the relevant pieces are split like this:

### Header

- `src/NestedAutomaton.h`
  - `MinMaxInfExperimentStats`
  - `setMinMaxInfExperimentStatsEnabled(...)`
  - `resetMinMaxInfExperimentStats()`
  - `getMinMaxInfExperimentStats()`
  - `flatten_MinMax_Inf_cached(...)`

### Shared backend prerequisites

- `src/NestedAutomaton.cpp`
  - `edgeWeightToChildIndex(...)`
  - `ChildTables`
  - `build_child_tables(...)`
  - shared threshold helper types
  - `ThrExtChildInfo`
  - `thrext_build_child_info(...)`

### Cached-only block

- `src/NestedAutomaton.cpp`
  - `mm_mix64(...)`
  - `mm_hash_combine(...)`
  - `MMInfCachedBuilder`
  - `flatten_MinMax_Inf_cached_impl(...)`
  - `NestedAutomaton::flatten_MinMax_Inf_cached(...)`

## Recommended Integration Strategy

Do not try to "merge pieces by feel". Port in controlled layers.

### Strategy A: target already has the shared threshold backend

This is the easy case.

You only need to add:

1. the header declarations
2. the experiment stats plumbing if missing
3. the cached-only block
4. the public cached wrapper

### Strategy B: target does not yet have the shared threshold backend

This is the hard case.

You should port in this order:

1. `edgeWeightToChildIndex(...)`
2. `ChildTables`
3. `build_child_tables(...)`
4. shared threshold helper types
5. `ThrExtChildInfo`
6. `thrext_build_child_info(...)`
7. experiment stats plumbing
8. cached-only block
9. public cached wrapper

Do not start with `MMInfCachedBuilder`. It depends on the shared helper layer.

## Exact Integration Order

Follow this order even if the target file is similar to the current one.

### Step 1. Add the public header delta

Carry these into the target `NestedAutomaton.h`:

- `MinMaxInfExperimentStats`
- the three static stats accessors
- `flatten_MinMax_Inf_cached(...)`

Why first:

- it makes the backend visible to tests and tooling
- it makes missing source dependencies obvious at compile time

### Step 2. Add or verify the child-table layer

The cached backend uses `ChildTables` exactly like the shared backend.

You need:

- compact CSR-like child transition storage
- child final-state flags
- child liveness flags
- child initial state ID

If the target version already has a similar helper, do not silently substitute
it unless it is behaviorally identical.

### Step 3. Add or verify the shared Min/Max child semantics

This is the crucial part.

The cached backend depends on `thrext_build_child_info(...)` to prepare:

- Min/Max mode
- threshold handling
- `mm_live[guess][prog]`

The cached backend should reuse that logic. It should not create a second copy
of Min/Max liveness semantics.

### Step 4. Add the stats plumbing

If the target version does not need experiment stats, you can omit this layer.
But the safest port keeps it, because it makes regression checking much easier.

The stats plumbing also gives you immediate evidence that the cached path still
reduces:

- bag copying
- deep state-map work
- repeated obligation stepping

### Step 5. Copy the cached-only implementation block as a unit

Copy these together:

- `mm_mix64(...)`
- `mm_hash_combine(...)`
- `MMInfCachedBuilder`
- `flatten_MinMax_Inf_cached_impl(...)`
- `NestedAutomaton::flatten_MinMax_Inf_cached(...)`

Do not inline pieces into the shared backend loop. Keep the cached path as its
own implementation until the port is fully verified.

### Step 6. Verify that the cached builder still reuses shared semantics

After the copy, confirm these are still true:

- the constructor still calls `build_child_tables(...)`
- the constructor still calls `thrext_build_child_info(...)`
- the builder does not reimplement Min/Max threshold logic separately

If any of those are false, the port drifted semantically.

### Step 7. Compile before touching anything else

Do not "clean up" the code before you have a successful build.

Early cleanup risks changing:

- child enable rules
- phase transitions
- sentinel handling
- hash/equality assumptions

## What Must Stay Identical

The following parts are not incidental. They are the cached backend.

### 1. The flattened-state key

The cached path uses a compact key:

- parent state ID
- `P1` bag ID
- `P2` bag ID
- phase
- `epoch_nonempty`

This is what replaces the deep `ThrExtBuchiState` key from the generic backend.

If you change this shape, you are no longer porting the same backend.

### 2. Obligation interning

Each logical obligation becomes one interned `OblId`.

That requires:

- canonical representation
- hash-based bucket lookup
- equality check before allocating a new ID

If the target version stores fresh obligation objects everywhere, it loses the
main constant-factor win.

### 3. Bag interning

Each logical bag becomes one interned `BagId`.

That requires:

- sorting
- duplicate removal
- hash-based bucket lookup
- one canonical stored bag per logical bag

### 4. Spawn memoization

Spawn is precomputed once per:

- child
- guess
- symbol

That table must keep the same 3-way result shape:

- dead
- immediately discharged
- concrete obligation ID

### 5. Obligation-step memoization

Each obligation keeps a per-symbol cache.

That cache is essential. Without it, repeated symbolic stepping falls back
toward the generic backend’s cost profile.

### 6. Bag-step memoization

Each bag keeps:

- `step_next[sym]`
- `step_any_discharged[sym]`

This prevents repeated full bag traversal for the same symbol.

## The Critical Bug You Must Not Reintroduce

The first cached implementation had a real memory bug.

### Root cause

It took references like:

- `Bag& B = bags[bid]`
- `Obl& O = obls[id]`

Then it called:

- `intern_bag(...)`
- `intern_obl(...)`

Those can `push_back(...)` into `std::vector` arenas, which may reallocate and
invalidate all references into the vector.

After that, the old code wrote through stale references.

### Correct fixed pattern

After interning, write back through indexed access:

```cpp
const BagId nb = intern_bag(std::move(out));
bags[bid].step_next[sym] = nb;
bags[bid].step_any_discharged[sym] = any_d ? 1u : 0u;
```

and:

```cpp
const OblId nid = intern_obl(O.child, O.guess, std::move(next0), std::move(next1));
obls[id].step_cache[sym] = nid;
```

### Porting rule

If a port refactors the builder, never keep references into arena-backed
vectors across any call that may allocate.

## Semantics That Are Inherited, Not Re-Decided

The cached backend inherits several semantics from the shared backend.

### Child enable rule

The constructor currently skips children with fewer than 2 states:

- `if (c->getStates()->size() < 2) continue;`

That means singleton children are currently treated as non-enabled in this
backend too.

This is not a random optimization. It mirrors the current shared-backend
behavior. If the target version handles singleton children differently, you
must decide that deliberately and retest everything.

### Epoch/final-pulse discipline

The cached backend keeps the same:

- `P1`
- `P2`
- `phase`
- `epoch_nonempty`

discipline as the shared threshold backend.

Do not simplify this while porting. It is semantic, not cosmetic.

### Silent edge handling

Silent parent transitions stay silent in the cached backend too.

The cached code preserves this by treating non-enabled child edges as silent
and emitting `SILENT` weights on those transitions.

## What You Should Not Change During Port

Avoid these “cleanup” moves until after the port is verified.

### Do not replace the builder’s sentinels

The builder relies on distinct sentinel codes for:

- unknown
- dead
- discharged

Changing these casually is an easy way to corrupt cache behavior.

### Do not merge the cached worklist loop into the generic one

The cached backend is easiest to verify when it stays a separate flattener.

### Do not replace the hash helpers with unrelated ones mid-port

You can improve hashing later. During the port, stable behavior matters more
than hash micro-optimizations.

### Do not change child indexing conventions

The backend depends on `edgeWeightToChildIndex(...)` behaving exactly as the
current code expects.

## Detailed Verification Plan

After the code compiles, verify in this order.

### 1. Sanity tests

Run the direct Min/Max sanity suite first:

```bash
./test_flatten_minmax_inf
```

This quickly catches broken phase or discharge logic.

### 2. Repeated in-process comparison

Run:

```bash
/tmp/minmax_inf_fix_compare
```

This matters because the original crash showed up under repeated in-process
usage, not necessarily on a single one-off run.

### 3. ASan reproducer if available

Run:

```bash
/tmp/repro_cached_deep_asan
```

This is the best check against reintroducing the arena invalidation bug.

### 4. Small resource-family comparison

Compare `current` and `cached` again on the small resource family. The exact
numbers may shift by target version, but the cached backend should still behave
like a constant-factor optimization, not a semantic divergence.

### 5. Stats sanity

If stats are enabled, check that cached still shows:

- near-zero bag-copy activity
- meaningful `step_obl_cache_hits`
- meaningful `step_bag_cache_hits`
- reduced deep state-map pressure compared to the generic path

## Failure Modes To Watch For

These are the most likely bad ports.

### Build succeeds but behavior drifts

Typical cause:

- local reimplementation of Min/Max semantics instead of reusing
  `thrext_build_child_info(...)`

### Performance regresses sharply

Typical causes:

- interning accidentally removed
- `bag_add_cache` omitted
- obligation or bag step caches not initialized correctly
- deep generic key accidentally retained

### Repeated comparison crashes

Typical cause:

- stale references into `obls` or `bags` across arena growth

### Correctness only fails on overlap-heavy cases

Typical causes:

- incorrect bag-add canonicalization
- duplicate obligations retained in a bag
- sentinel mix-up between dead and discharged

## Practical Porting Checklist

- add `MinMaxInfExperimentStats` to the header
- add the three stats accessors
- add `flatten_MinMax_Inf_cached(...)` declaration
- verify `edgeWeightToChildIndex(...)`
- verify `ChildTables`
- verify `build_child_tables(...)`
- verify `ThrExtChildInfo`
- verify `thrext_build_child_info(...)`
- copy `mm_mix64(...)`
- copy `mm_hash_combine(...)`
- copy `MMInfCachedBuilder`
- copy `flatten_MinMax_Inf_cached_impl(...)`
- copy the public cached wrapper
- verify the fixed post-intern writes
- build
- run Min/Max sanity tests
- run repeated in-process comparison
- run ASan reproducer if available

## Recommended Use Of The Companion `.cpp` Reference

Use the `.cpp` reference file when you need:

- the exact dependency list
- the exact non-regression rules
- the exact sentinel and arena cautions

Use this Markdown guide when you need:

- the order of operations
- the integration strategy
- the verification sequence
- the likely failure modes

## Bottom Line

The safest way to port cached Min/Max `Inf/LimInf` is:

1. keep the shared threshold backend as the semantic base
2. move the cached Min/Max block as a unit
3. preserve the compact `Key` and arena interning design
4. preserve the fixed no-stale-reference pattern
5. verify with repeated in-process runs, not just single tests
