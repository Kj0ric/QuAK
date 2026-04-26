# Patch Plan: Treat Child Final States as Termination Opportunities, Not Terminal States

## 0. Executive Summary

Current flattening code often treats child final states as terminal control states. That is too strong for general NQA semantics: a child may terminate when it reaches an accepting state, but if that accepting state has outgoing transitions, the child may also choose to continue and terminate at a later accepting visit.

The requested semantic change is:

> Reaching a child final state creates a **choice to stop now**. If the current guessed return value is not satisfied, or if the implementation elects not to stop, the child may continue from that final state like any other state.

The main implementation rule is:

```cpp
// after taking edge st --sym/w--> to and updating progress/accumulator
if (T.is_final[to] && outcome_matches_guess(...)) {
    discharge_now();              // choose to terminate at this accepting visit
} else {
    // includes wrong-final targets
    if (live_after_continuing(to, updated_progress)) {
        carry_continuation(to, updated_progress);
    }
}
```

Do **not** add a generic rule that discharges just because the *current* stored state is final. That would allow retroactive termination after the run already chose to continue through that final state.

This plan covers the current shared threshold-extremal backend, regular flattening, cached Min/Max paths, cached Sum paths, tests, and lower-priority legacy paths.

---

## 1. Semantic Contract to Preserve

### 1.1 Child-run semantics

A child invocation consumes a finite non-empty subword starting at the parent call position. It terminates at an accepting child state. If accepting states have outgoing transitions, then a visit to an accepting state is a termination opportunity, not an obligation to stop.

### 1.2 Tracker invariant

Every child-obligation tracker should interpret a frontier entry:

```text
(state, progress)
```

as:

```text
The child has consumed input up to the current global position and has chosen not to terminate yet.
```

Therefore, if `state` is final, it means the child visited an accepting state previously and deliberately continued. It cannot terminate retroactively without consuming another symbol. It can only terminate again after taking a future edge into a final state.

### 1.3 Liveness invariant

The meaning of target-aware liveness must be:

```text
live(child, guess, state, progress) == true
iff after continuing from (state, progress), there exists a non-empty future continuation
that eventually takes an edge into a final state with a return value matching guess.
```

This is why liveness should **not** seed `(final_state, matching_progress)` directly. Seeds should be states that can discharge by taking **one more edge** into a matching final state.

---

## 2. Affected Code Areas

Current file: `NestedAutomaton.cpp`.

Approximate function locations in the current snapshot:

| Area | Functions / classes | Approximate line starts |
|---|---|---:|
| Regular child return-value enumeration | `computeMinMaxReturnValuesParentAware`, `computeSumBReturnValuesParentAware`, `computeMinMaxReturnValues`, `computeSumBReturnValues` | 291, 393, 576, 655 |
| Regular flattening liveness and obligations | `TargetAwareLive::build`, `step_obl_bag_finite`, `spawn_obligation_finite` | 1192, 1372, 1439 |
| Shared threshold-extremal backend | `thrext_build_mm_live`, `thrext_build_sum_cutoffs`, `thrext_step_frontier`, `thrext_spawn_frontier`, `flatten_threshold_extremal_impl` | 4174, 4274, 4441, 4530, 4593 |
| Cached Min/Max threshold backend | `build_mmthr_live`, `step_mmthr_obl_bag`, `spawn_mmthr_obligation` | 8385, 8500, 8576 |
| Cached Sup/LimSup witness path | `MMSupCachedBuilder`, `flatten_MinMax_Sup_witness_cached_impl` | 9178, 9537-ish |
| Cached Inf/LimInf Min/Max path | `MMInfCachedBuilder` | 10042 |
| Cached Inf/LimInf Sum path | `SumInfCachedBuilder`, `flatten_SumPlusMinus_Inf_cached_impl` | 10653, 10971 |
| Limit-average SumMinus synchronization | `synchronizeChildren`, `flatten_Avg_SumMinus` | 2239, 2776 |
| Legacy parser-based paths | Old code after early `return flatten_threshold_extremal_impl(...)`; `flatten_MinMax_Inf_v1`, `flatten_MinMax_Inf_v2`, `flatten_MinMax_Inf_masked` | multiple |

---

## 3. Shared Threshold-Extremal Backend

This should be patched first because several public flatteners and cached builders depend on it.

### 3.0 Patch boundary for the first threshold-extremal pass

Keep the first pass tight.

Patch only:

- `thrext_build_mm_live`
- `thrext_build_sum_cutoffs`
- `thrext_step_frontier`
- `thrext_spawn_frontier`

Leave unchanged in this stage:

- `thrext_is_live`
- `thrext_build_child_info`
- `thrext_step_obl_bag`
- `thrext_spawn_obligation`
- `flatten_threshold_extremal_impl`
- `advance_phase_thrext(...)`
- the `epoch_nonempty` acceptance rule

Reason:

- once the live tables / sum cutoffs treat finals as continuation states, and
  step/spawn stop dropping wrong-final targets, the outer threshold-extremal
  Buchi machine already has the intended semantics
- `thrext_is_live(...)` is a consumer of `mm_live`, `min_extra`, and
  `max_extra`; it does not need a semantic rewrite if those structures are
  rebuilt correctly
- `thrext_step_obl_bag(...)` and `thrext_spawn_obligation(...)` are wrappers
  around the two frontier helpers and should inherit the fix automatically

### 3.1 Patch `thrext_build_mm_live`

Current issue:

- Final source states are skipped.
- Final target states are not added to the reverse product graph.
- Final states are not marked live in `info.mm_live`.

Required behavior:

- Include final states as ordinary continuation states in the product graph.
- Include edges into final states in the product graph, because a wrong-final target may be continued from.
- Seed only states that can discharge by taking one future edge into a final state whose updated progress matches the guess.
- Mark final states live if they can continue to a later matching final.

Important review points:

- Do **not** change the two-point progress domain or `thrext_step_prog(...)`.
- Do **not** seed current final states directly.
- `thrext_is_live(...)` should stay unchanged; it should start reading the
  repaired `info.mm_live` table.

Patch shape:

```cpp
static void thrext_build_mm_live(const ChildTables& T, ThrExtChildInfo& info) {
    const uint32_t prod_sz = T.n_states * 2u;
    std::vector<std::vector<uint32_t>> rev(prod_sz);

    auto node_id = [](uint32_t st, uint8_t p) -> uint32_t {
        return (st << 1u) | static_cast<uint32_t>(p);
    };

    // Build continuation graph over ALL states, including finals.
    for (uint32_t st = 0; st < T.n_states; ++st) {
        for (uint8_t p = 0; p <= 1u; ++p) {
            for (uint32_t sym = 0; sym < T.alph; ++sym) {
                const uint32_t cell = T.idx(st, sym);
                const uint32_t b = T.off[static_cast<size_t>(cell)];
                const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                for (uint32_t pos = b; pos < e; ++pos) {
                    const auto& tr = T.edges[static_cast<size_t>(pos)];
                    if (tr.to >= T.n_states) continue;

                    const uint8_t p2 = static_cast<uint8_t>(
                        thrext_step_prog(info, p, tr.w));

                    // Important: include final targets as continuation nodes.
                    rev[node_id(tr.to, p2)].push_back(node_id(st, p));
                }
            }
        }
    }

    for (uint8_t guess = 0; guess <= 1u; ++guess) {
        std::vector<uint8_t> seen(prod_sz, 0u);
        std::deque<uint32_t> q;

        // Seed states that can stop by taking ONE future edge into a matching final.
        for (uint32_t st = 0; st < T.n_states; ++st) {
            for (uint8_t p = 0; p <= 1u; ++p) {
                bool seed = false;
                for (uint32_t sym = 0; sym < T.alph && !seed; ++sym) {
                    const uint32_t cell = T.idx(st, sym);
                    const uint32_t b = T.off[static_cast<size_t>(cell)];
                    const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                    for (uint32_t pos = b; pos < e; ++pos) {
                        const auto& tr = T.edges[static_cast<size_t>(pos)];
                        if (tr.to >= T.n_states) continue;
                        if (!T.is_final[tr.to]) continue;

                        const uint8_t p2 = static_cast<uint8_t>(
                            thrext_step_prog(info, p, tr.w));
                        if (thrext_discharge_ok(info, guess, p2)) {
                            seed = true;
                            break;
                        }
                    }
                }

                if (seed) {
                    const uint32_t u = node_id(st, p);
                    if (!seen[u]) {
                        seen[u] = 1u;
                        q.push_back(u);
                    }
                }
            }
        }

        while (!q.empty()) {
            const uint32_t v = q.front();
            q.pop_front();
            for (uint32_t u : rev[v]) {
                if (!seen[u]) {
                    seen[u] = 1u;
                    q.push_back(u);
                }
            }
        }

        for (uint8_t p = 0; p <= 1u; ++p) {
            info.mm_live[guess][p].assign(T.n_states, 0u);
        }

        // Mark finals too if they can continue to a matching final later.
        for (uint32_t st = 0; st < T.n_states; ++st) {
            for (uint8_t p = 0; p <= 1u; ++p) {
                if (seen[node_id(st, p)]) {
                    info.mm_live[guess][p][st] = 1u;
                }
            }
        }
    }
}
```

### 3.2 Patch `thrext_build_sum_cutoffs`

Current issue:

- The reverse graph ignores outgoing edges from final states.
- This prevents `min_extra` / `max_extra` from recognizing that a wrong-final state may continue and later satisfy the guess.

Required changes:

1. Remove the `if (T.is_final[st]) continue;` skip from the reverse-graph construction.
2. Continue to seed `min_extra[final] = 0` for all finals. This is safe because wrong-final cases will not be made live by zero extra unless the current progress already matches the guess, in which case the edge into that final would have discharged already.
3. Leave the current `max_extra` seed loop unchanged. In the live implementation it already seeds every `T.live` state with `0`; the missing piece is that reverse edges from final states must exist so positive gain can propagate through a wrong-final continuation.
4. `thrext_is_live(...)` should stay unchanged for sum modes as well; once `min_extra` and `max_extra` are computed on the repaired graph, its inequalities become correct again.

Minimal patch:

```cpp
for (uint32_t st = 0; st < T.n_states; ++st) {
    // Do NOT skip final states here.
    for (uint32_t sym = 0; sym < T.alph; ++sym) {
        ...
        rev[tr.to].push_back(ThrExtRevCostEdge{st, cost});
    }
}
```

### 3.3 Patch `thrext_step_frontier`

Current issue:

```cpp
if (T.is_final[tr.to]) {
    if (thrext_discharge_ok(info, guess, prog2)) {
        next_conf.clear();
        return ThrExtStepStatus::DISCHARGED;
    }
    continue;
}
```

This drops wrong-final targets.

Replace with:

```cpp
if (T.is_final[tr.to] &&
    thrext_discharge_ok(info, guess, prog2)) {
    next_conf.clear();
    return ThrExtStepStatus::DISCHARGED;
}

// Includes wrong-final targets.
if (!thrext_is_live(T, info, guess, tr.to, prog2)) continue;
next_conf.push_back(ThrExtConf{tr.to, prog2});
```

### 3.4 Patch `thrext_spawn_frontier`

Current issue: same wrong-final drop at spawn.

Replace:

```cpp
if (T.is_final[tr.to]) {
    if (thrext_discharge_ok(info, guess, prog2)) {
        return ThrExtSpawnStatus::EMPTY;
    }
    continue;
}

if (!thrext_is_live(T, info, guess, tr.to, prog2)) continue;
conf.push_back(ThrExtConf{tr.to, prog2});
```

with:

```cpp
if (T.is_final[tr.to] &&
    thrext_discharge_ok(info, guess, prog2)) {
    return ThrExtSpawnStatus::EMPTY;
}

// Includes wrong-final targets.
if (!thrext_is_live(T, info, guess, tr.to, prog2)) continue;
conf.push_back(ThrExtConf{tr.to, prog2});
```

### 3.5 Detailed execution plan for the threshold-extremal stage

1. Add or enable failing regressions A, D, E, and F against the shared public
   entry points:
   - `flatten_MinMax_Sup`
   - `flatten_MinMax_Inf`
   - `flatten_SumPlusMinus_Sup`
   - `flatten_SumPlusMinus_Inf`
2. Patch `thrext_build_mm_live`.
   Verify:
   - final source states are no longer skipped
   - final target states appear in `rev`
   - final states can be marked live
   - no direct-current-final seeding was introduced
3. Patch `thrext_build_sum_cutoffs`.
   Verify:
   - the reverse graph includes outgoing edges from final states
   - `min_extra` still seeds finals only
   - the current `max_extra` seed loop is left unchanged
4. Patch `thrext_step_frontier` and `thrext_spawn_frontier`.
   Verify:
   - matching finals still discharge immediately
   - wrong-final targets are kept iff `thrext_is_live(...)`
   - canonicalization logic is unchanged
5. Re-run targeted validation before touching dependent cached builders:

```bash
cmake --build build --target \
  test_emptiness_correctness \
  test_flatten_minmax_sup \
  test_flatten_minmax_inf \
  test_flatten_sumplusminus_sup \
  test_flatten_sumplusminus_inf -j

./build/test_emptiness_correctness
./build/test_flatten_minmax_sup
./build/test_flatten_minmax_inf
./build/test_flatten_sumplusminus_sup
./build/test_flatten_sumplusminus_inf
```

6. Treat this stage as the baseline for later comparisons. If it is still red,
   do not draw conclusions about cached or witness-based paths yet.

### 3.6 Backend coverage after this patch

This patch fixes or partially fixes:

- `flatten_threshold_extremal_impl`
- `flatten_MinMax_Sup`
- `flatten_MinMax_Inf`
- `flatten_SumPlusMinus_Sup`
- `flatten_SumPlusMinus_Inf`
- `SumInfCachedBuilder`
- Any path that delegates to `thrext_spawn_frontier` / `thrext_step_frontier`

It does **not** fix `flatten_regular` or `MMSupCachedBuilder`, which have independent liveness and step logic.

---

## 4. Regular Flattening

Patch this second, but keep it as a self-contained semantic fix for the live `flatten_regular(...)` path. The outer Buchi/epoch machine is not the problem here. The mismatch comes from the child-return layer treating final child states as mandatory sinks.

### 4.1 Scope of the first `flatten_regular` patch

Current live `flatten_regular(...)` uses:

- `computeChildReturnValuesParentAware(...)` for guess enumeration
- `TargetAwareLive::build(...)` for target-aware pruning
- `step_obl_bag_finite(...)` and `spawn_obligation_finite(...)` for obligation dynamics

Therefore the first correctness patch should change only:

- `computeMinMaxReturnValuesParentAware`
- `computeSumBReturnValuesParentAware`
- `TargetAwareLive::build`
- `step_obl_bag_finite`
- `spawn_obligation_finite`

Leave unchanged in the first patch:

- `BuchiState_obl`
- `advance_phase(...)`
- `flatten_regular(...)` `P1/P2` boundary logic
- final marking `phase == ACC_WAIT_P2EMPTY && P2.empty()`

Also leave `computeMinMaxReturnValues` and `computeSumBReturnValues` out of the first patch. They are not on the current live `flatten_regular` path and can be aligned later as cleanup or consistency work. This keeps the change surgical and avoids mixing the semantic repair with unrelated refactoring.

### 4.2 Patch parent-aware return-value enumeration

Affected functions:

- `computeMinMaxReturnValuesParentAware`
- `computeSumBReturnValuesParentAware`

Current behavior records a return value when a final state is reached and does not enqueue that final state for continuation. It also contains current-state checks of the form "if the child state is final, record and stop exploring successors".

Required behavior:

- When an edge reaches a final state, record the return value.
- Also enqueue the resulting continuation state if it has not been visited.
- Remove current-state final handling that says "record and stop exploring".
- Treat finality as something observed on the incoming edge, not as a property that forces the worklist to stop at that node.

Use this helper pattern:

```cpp
auto push_continuation = [&](State* parent_st,
                             State* child_st,
                             AccType acc_like...) {
    WorkState ws = {parent_st, child_st, acc_like...};
    if (!visited.contains(ws)) {
        visited.insert(ws);
        worklist.push(ws);     // even if child_st is final
    }
};

auto record_if_final = [&](State* child_st, AccType acc_like...) {
    if (child_st->getFinal()) {
        return_values.insert(compute_return(acc_like));
    }
};
```

Then, for every edge:

```cpp
record_if_final(c2, next_acc_like...);
push_continuation(m2, c2, next_acc_like...);
```

Remove patterns like:

```cpp
if (ccur->getFinal()) {
    return_values.insert(val);
    continue;
}
```

Important:

- recording when popping a final state is harmless as a set duplicate, but it blurs the intended invariant; prefer recording only when entering a final state by edge
- for `SumB`, the continuation state is still `(parent_state, child_state, sum, hit)`, and the recorded return remains `hit != 0 ? hit : sum`
- this stage is mandatory because `flatten_regular(...)` builds its guess domain from these functions; if they miss continuation returns, the obligation layer cannot recover them later

### 4.3 Patch `TargetAwareLive::build`

Current issue:

- Final states are excluded from product nodes.
- Wrong-final edges are not represented as continuation states.

Required behavior:

- `live(child, guess, st, acc)` should mean:
  after choosing not to terminate at `(st, acc)` now, there exists a non-empty continuation that eventually reaches a final state with matching guessed return
- Product graph includes `(state, accumulator)` for all states, including finals.
- Reverse edges include final targets.
- Seeds are all states that can take one edge into a final state whose updated accumulator matches the guess.
- The live table marks final states if they have a future continuation to a matching final.

Change the reverse-graph construction from:

```cpp
if (T.is_final[st]) continue;
...
if (T.is_final[to]) continue;
```

to no final-state skips.

Change seeding and final marking from:

```cpp
if (T.is_final[st]) continue;
```

to include all states. Do not seed based on the current state being final.

Important:

- keep the coarse `T.live[to]` pruning; it is still useful and still sound
- do not give final states zero-step liveness just because they are final
- the whole point is "future accepting visit after opting not to stop now", not "already final implies live"

### 4.4 Patch `step_obl_bag_finite`

Current issue:

```cpp
if (T.is_final[to]) {
    if (discharge_ok_finite(finVal, acc2, ent.key.guess, bound)) {
        discharged = true;
        break;
    }
    continue; // wrong-return branch => drop
}
```

Replace with:

```cpp
if (T.is_final[to] &&
    discharge_ok_finite(finVal, acc2, ent.key.guess, bound)) {
    discharged = true;
    break;
}

// Includes wrong-final targets.
if (!live.is_live(i, ent.key.guess, to, acc2)) continue;
next_conf.push_back(ConfPair{to, acc2});
```

This keeps the existing existential discharge rule: a matching final still discharges immediately. The only change is that wrong-final targets are no longer dropped automatically; they are retained only if liveness says they can later satisfy the same guess.

### 4.5 Patch `spawn_obligation_finite`

Current issue: same wrong-final drop.

Replace:

```cpp
if (T.is_final[to]) {
    if (discharge_ok_finite(finVal, acc2, guess, bound)) {
        return SpawnStatus::EMPTY;
    }
    continue;
}

if (!live.is_live(child_idx, guess, to, acc2)) continue;
conf.push_back(ConfPair{to, acc2});
```

with:

```cpp
if (T.is_final[to] &&
    discharge_ok_finite(finVal, acc2, guess, bound)) {
    return SpawnStatus::EMPTY;
}

// Includes wrong-final targets.
if (!live.is_live(child_idx, guess, to, acc2)) continue;
conf.push_back(ConfPair{to, acc2});
```

`SpawnStatus::EMPTY` should still mean:

- the spawn symbol already took the child to a matching final, so the invocation can terminate immediately

Wrong-final targets should instead remain inside the spawned configuration set if they are live continuations.

### 4.6 Do not patch the outer `flatten_regular(...)` state machine

No change is needed in:

- `BuchiState_obl`
- `advance_phase(...)`
- `flatten_regular(...)` phase transitions
- the acceptance rule `phase == ACC_WAIT_P2EMPTY && P2.empty()`

A carried final child configuration is not a discharged obligation. It still occupies `P1` or `P2` until the obligation actually chooses a matching accepting visit and disappears. The existing epoch/Buchi control therefore remains correct.

Also keep `last_guess` out of this fix. It may still be dead state-space noise, but it is not the semantic bug being addressed here.

### 4.7 Tests and rollout for `flatten_regular`

Add correctness tests before patching, not only sanity smoke tests.

Recommended `flatten_regular` regression set:

- `Max_f`: wrong final first, later high final
- `Max_f`: negative control where no later matching final exists
- `Min_f`: wrong final first, later low final
- `SumB`: wrong final first, later accepting continuation changes the bounded return
- no retroactive termination: a run that reaches a final early but only succeeds after an additional symbol

Then add a split-final differential oracle:

- transform each final child state `f` into `f_stop` and `f_cont`
- duplicate incoming edges to both
- keep outgoing edges only from `f_cont`
- compare the patched `flatten_regular(A)` against `flatten_regular(split_finals(A))`

Use this differential test as the main oracle for this patch. Until Section 4 is complete, `flatten_regular(...)` should not be treated as a trustworthy oracle for the threshold-specialized backends.

---

## 5. Cached Min/Max Threshold Backend

Patch this after the shared threshold backend and regular flattening. This covers the optimized paths under discussion.

### 5.1 Patch `build_mmthr_live`

This is the bitset analogue of `thrext_build_mm_live`.

Required changes:

- Do not skip final source states.
- Do not skip final target states in the reverse product graph.
- Seed only states that can take one future edge into a final with `y2 == guess`.
- Mark final states live if they can continue to a later matching final.

Patch rule:

```cpp
for (uint32_t st = 0; st < T.n_states; ++st) {
    // Do NOT skip finals.
    for (uint8_t y = 0; y <= 1u; ++y) {
        ...
        const uint8_t y2 = mmthr_y_update(...);
        // Do NOT skip final target.
        rev[node_id(tr.to, y2)].push_back(node_id(st, y));
    }
}
```

Similarly, remove `if (T.is_final[st]) continue;` from seeding and marking.

### 5.2 Patch `step_mmthr_obl_bag`

Replace the final-target block with:

```cpp
const uint8_t y2 = mmthr_y_update(finite_is_max, y, tr.w, threshold);

if (T.is_final[tr.to] && y2 == ent.key.guess) {
    discharged = true;
    return;
}

// Includes wrong-final targets.
if (!live.is_live(cid, ent.key.guess, tr.to, y2)) continue;
if (y2 == 0u) mmthr_bits_set(next.y0, tr.to);
else          mmthr_bits_set(next.y1, tr.to);
```

### 5.3 Patch `spawn_mmthr_obligation`

Replace the final-target block with:

```cpp
const uint8_t y2 = mmthr_y_update(finite_is_max, init_y, tr.w, threshold);

if (T.is_final[tr.to] && y2 == guess) {
    return MMThrSpawnStatus::EMPTY;
}

// Includes wrong-final targets.
if (!live.is_live(child_idx, guess, tr.to, y2)) continue;
if (y2 == 0u) mmthr_bits_set(fr.y0, tr.to);
else          mmthr_bits_set(fr.y1, tr.to);
```

### 5.4 Patch `MMSupCachedBuilder::step_obl`

This builder has private transition logic, so patching only the global `step_mmthr_obl_bag` is not enough.

Replace:

```cpp
if (T.is_final[tr.to]) {
    if (y2 == O.guess) {
        discharged = true;
        return;
    }
    continue;
}
```

with:

```cpp
if (T.is_final[tr.to] && y2 == O.guess) {
    discharged = true;
    return;
}

// Includes wrong-final targets.
if (!live.is_live(O.child, O.guess, tr.to, y2)) continue;
if (y2 == 0u) mmthr_bits_set(next.y0, tr.to);
else          mmthr_bits_set(next.y1, tr.to);
```

`MMSupCachedBuilder::precompute_spawns` delegates to `spawn_mmthr_obligation`, so it is fixed once `spawn_mmthr_obligation` and `build_mmthr_live` are patched.

### 5.5 `TermCachedBuilder` should remain unchanged

`TermCachedBuilder` only proves that a background child can terminate. If a child reaches any final state, immediate termination is a valid choice, so final-state continuation is not needed for termination-only obligations.

Do not change:

```cpp
if (T.is_final[tr.to]) return OBL_DISCHARGED;
```

### 5.6 Patch `MMInfCachedBuilder`

`MMInfCachedBuilder` uses `thrext_build_child_info` for liveness, so `thrext_build_mm_live` helps, but it has private spawn/step loops that still drop wrong finals.

Patch both:

- `MMInfCachedBuilder::precompute_spawns`
- `MMInfCachedBuilder::step_obl`

Use the same rule as `MMSupCachedBuilder`:

```cpp
if (T.is_final[tr.to] && y2 == guess) {
    discharge_or_spawn_empty;
} else if (is_live(child, guess, tr.to, y2)) {
    carry(tr.to, y2);
}
```

---

## 6. Cached Sum Inf/LimInf Path

`SumInfCachedBuilder` delegates spawn and step to the shared `thrext_spawn_frontier` and `thrext_step_frontier`.

After patching the shared threshold backend:

- `SumInfCachedBuilder::precompute_spawns` should automatically carry wrong finals because `thrext_spawn_frontier` does.
- `SumInfCachedBuilder::step_obl` should automatically carry wrong finals because `thrext_step_frontier` does.

Still run specific SumPlus/SumMinus wrong-final tests because this path also depends on correct `thrext_build_sum_cutoffs` liveness.

---

## 7. Sup/LimSup Witness-Cached Path

The outer state machine in `flatten_MinMax_Sup_witness_cached_impl` does not need a semantic rewrite.

Keep:

```text
(parent, B1, B2, W1, W2, phase, epoch_nonempty)
```

Where:

- `B1/B2` use `TermCachedBuilder` and remain termination-only.
- `W1/W2` use `MMSupCachedBuilder` and must support final-state continuation.

Patch only the witness builder and shared `MMThrLive` logic as described above.

### 7.1 Important same-symbol handoff invariant

The implementation steps old obligations before processing the parent edge. This means an old witness may discharge on the same symbol on which the parent spawns a new child. That remains correct.

Keep the existing invariant:

```cpp
assert(W1_base == NO_WITNESS || W2_base == NO_WITNESS);
```

Consider upgrading this to a debug/logging check or a hard failure in non-release builds if the project has a convention for invariant failures.

---

## 8. Limit-Average SumMinus Synchronization Path

Affected functions:

- `synchronizeChildren`
- `flatten_Avg_SumMinus`

Current behavior freezes or terminates children once they reach a final state. This is a different construction than the threshold-obligation trackers, so patching it directly is more invasive.

Recommended plan:

### 8.1 Preferred low-risk approach: normalize before synchronization

Before entering the `determinizeWithMacroAlphabet -> synchronizeChildren -> flatten_Avg_SumMinus` pipeline, apply a stop/continue final-state splitting transformation to children.

This transformation is exact and allows the existing synchronization code to continue assuming terminal finals.

For every original final child state `f`, create:

```text
f_stop  final, no outgoing transitions
f_cont  non-final, outgoing transitions copied from f
```

For every original transition into `f`:

```text
u --a/w--> f_stop
u --a/w--> f_cont
```

For every original outgoing transition from `f`:

```text
f_cont --a/w--> target'
```

Where `target'` is duplicated if the target is also final.

If the original initial state is final, use the corresponding `f_cont` as the initial state. Do not allow zero-length child termination at spawn; child invocations consume the spawn symbol.

This approach avoids rewriting `pending_accept` and multiset logic.

### 8.2 Direct patch alternative

If direct patching is required instead of normalization:

- Remove logic in `synchronizeChildren` that skips exploration of final child states when `pending_accept == false`.
- Replace `pending_accept` with a choice point: after an edge into a final state on a silent parent step, either:
  - stop and set pending-flush behavior, or
  - continue from the final target as a normal child state.
- In `flatten_Avg_SumMinus`, do not skip `ustep` construction for final states. Final states of `U_sync` should only be terminal for stop-copy states, not for continuation states.

This is riskier than normalization and should be treated as a separate task.

---

## 9. Legacy / Archived Paths

Several parser-based implementations are effectively bypassed by early returns to `flatten_threshold_extremal_impl`, for example around the current calls at approximately lines 4868, 5421, 5933, and 7596.

Recommended handling:

1. If the old code is unreachable, delete it or mark it explicitly deprecated.
2. If any legacy paths are still exposed for experiments, either patch them using the same rule or exclude them from semantic correctness comparisons.
3. Do not use legacy paths as correctness oracles until they are patched.

Likely affected legacy paths include:

- Old `flatten_MinMax_Sup` body after the early return.
- Old `flatten_SumPlusMinus_Sup` body after the early return.
- Old `flatten_SumPlusMinus_Inf` body after the early return.
- `flatten_MinMax_Inf_v1`.
- `flatten_MinMax_Inf_v2`.
- `flatten_MinMax_Inf_masked`.

---

## 10. Testing Plan

Add tests at three levels:

1. Direct child-return tests.
2. Backend/flattening regression tests.
3. Randomized differential tests against final-state splitting normalization.

### 10.1 Regression A: `Max_f` high witness through a wrong final

Child `C`:

```text
s0 --a/0--> f0     f0 final
f0 --b/1--> f1     f1 final
```

Threshold: `1`.

Expected finite return values for `Max_f`:

```text
{0, 1}
```

Expected threshold result:

```text
Max_f >= 1 is achievable by consuming ab and stopping at f1.
```

Use a parent that repeatedly spawns `C` on `a` and has a silent `b` transition, so `(ab)^ω` is accepted.

Assertions:

```text
flatten_threshold_extremal(Max_f, threshold=1), checked under Sup >= 1: true
flatten_threshold_extremal(Max_f, threshold=1), checked under LimSup >= 1: true
flatten_MinMax_Sup_cached(Max_f, threshold=1), checked under Sup/LimSup >= 1: true
flatten_MinMax_Sup_witness_cached(Max_f, threshold=1), checked under Sup/LimSup >= 1: true
flatten_regular(Max_f), child returns include 1
```

This test should fail before the patch.

### 10.2 Regression B: `Max_f` negative control

Child:

```text
s0 --a/0--> f0     f0 final
f0 --b/0--> f1     f1 final
```

Threshold: `1`.

Expected:

```text
Max_f >= 1 is not achievable.
```

Assertions:

```text
all patched threshold-based Sup/LimSup checks return false for threshold 1
flatten_regular(Max_f) returns only {0}
```

This guards against carrying wrong finals without liveness pruning.

### 10.3 Regression C: multiple final continuations

Child:

```text
s0 --a/0--> f0     f0 final
f0 --b/0--> f1     f1 final
f1 --c/1--> f2     f2 final
```

Threshold: `1`.

Expected:

```text
Max_f >= 1 is achievable only by continuing through two accepting states.
```

This catches incomplete liveness patches that carry one wrong final but still fail to mark final states live for further continuation.

### 10.4 Regression D: `Min_f` low guess through a wrong final

Child:

```text
s0 --a/1--> f0     f0 final
f0 --b/0--> f1     f1 final
```

Threshold: `1`.

Expected finite return values for `Min_f`:

```text
{1, 0}
```

For threshold-extremal low guess:

```text
Guess 0 must be spawnable after a, carried through f0, and discharged after b.
```

This is most useful for:

- `flatten_regular(Min_f)`
- `flatten_threshold_extremal(Min_f)`
- `flatten_MinMax_Inf_threshold_obl(Min_f)`
- `MMInfCachedBuilder`

### 10.5 Regression E: `SumPlus` high through a wrong final

Child:

```text
s0 --a/2--> f0     f0 final
f0 --b/3--> f1     f1 final
```

Threshold: `5`.

Expected:

```text
SumPlus >= 5 is achievable by continuing through f0.
```

Assertions:

```text
flatten_threshold_extremal(SumPlus, threshold=5), Sup/LimSup >= 1: true
flatten_SumPlusMinus_Inf_cached(SumPlus, threshold=5), Inf/LimInf behavior agrees with threshold_extremal on matching objective tests
```

### 10.6 Regression F: `SumMinus` low/bad through a wrong final

Use cost accumulation under `SumMinus`.

Child:

```text
s0 --a/-1--> f0     f0 final
f0 --b/-4--> f1     f1 final
```

Threshold: `-3`.

The child can stop after `a` with value `-1 >= -3`, or continue to `ab` with value `-5 < -3`.

Expected:

```text
Both high and low threshold outcomes are semantically possible.
```

Assertions:

```text
threshold-extremal high guess remains possible on a
threshold-extremal low guess becomes possible on ab
cached SumMinus Inf path agrees with shared threshold backend
```

### 10.7 Regression G: no retroactive termination

Child:

```text
s0 --a/0--> f0     f0 final
f0 --b/0--> q      q non-final
q  --c/1--> f1     f1 final
```

Threshold: `1`, finite aggregator `Max_f`.

Expected:

```text
abc can witness Max_f >= 1.
ab cannot witness Max_f >= 1.
```

This guards against incorrectly treating a stored final state as automatically dischargeable on the next global step.

### 10.8 Regression H: same-symbol witness handoff

Parent:

```text
p0 --a / C1 --> p1
p1 --b / C2 --> p1
```

Child `C1`:

```text
s0 --a/0--> f0     f0 final
f0 --b/1--> f1     f1 final
```

Child `C2`:

```text
t0 --b/1--> g      g final
```

Expected:

```text
On b, the old witness can discharge through C1, and C2 can be selected as the next witness on the same symbol.
```

Assertions:

```text
flatten_MinMax_Sup_witness_cached(Max_f, threshold=1), LimSup >= 1: true
flatten_threshold_extremal(Max_f, threshold=1), LimSup >= 1: true
```

### 10.9 Regression I: terminal-final no-change baseline

For every benchmark where all child final states have no outgoing transitions:

```text
patched decision == old decision
```

When graph construction is deterministic enough, also compare:

```text
state count and edge count unchanged or within a small expected tolerance
```

---

## 11. Differential Oracle: Final-State Splitting

Implement a test-only normalization function:

```cpp
NestedAutomaton* split_child_finals_for_testing(const NestedAutomaton* A);
```

For each real child:

1. Non-final states are copied normally.
2. Every final state `f` becomes:
   - `f_stop`: final, no outgoing transitions.
   - `f_cont`: non-final, outgoing transitions copied from `f`.
3. Every transition into a final state is duplicated to both stop and continue copies.
4. Every transition into a non-final state is copied normally.
5. If the original initial state is final, the new initial should be `f_cont`, not `f_stop`.
6. Child 0 dummy behavior should be preserved.

Then compare decisions:

```text
patched_flatten(A) == patched_flatten(split(A))
```

Run this for:

| Parent objective | Child objective | Flattening path |
|---|---|---|
| Sup / LimSup | Max_f / Min_f | `flatten_threshold_extremal`, `flatten_MinMax_Sup_cached`, `flatten_MinMax_Sup_witness_cached` |
| Inf / LimInf | Max_f / Min_f | `flatten_threshold_extremal`, `flatten_MinMax_Inf_threshold_obl`, `MMInfCachedBuilder` path if exposed |
| Sup / LimSup | SumPlus / SumMinus | `flatten_threshold_extremal` |
| Inf / LimInf | SumPlus / SumMinus | `flatten_threshold_extremal`, `flatten_SumPlusMinus_Inf_cached` |
| Any supported regular parent | Max_f / Min_f / SumB | `flatten_regular` |

Random generation parameters:

```text
alphabet size:       1..3
child count:         1..3 real children
child states:        2..5
parent states:       1..4
weights:             {-2, -1, 0, 1, 2, 3}
final probability:   20%..50%
final outgoing:      allowed
thresholds:          {-2, -1, 0, 1, 2, 3}
```

Filter generated instances to avoid trivial no-child or no-accepting-parent cases unless explicitly testing them.

---

## 12. Acceptance Criteria

A patch should be considered acceptable when:

1. All targeted regression tests above pass.
2. Random split-final differential tests pass for at least several thousand small generated automata.
3. Existing benchmark decisions do not change for terminal-final children.
4. `flatten_threshold_extremal` and `flatten_MinMax_Sup_witness_cached` agree at the decision level for Sup/LimSup Min/Max threshold emptiness.
5. No test relies on exact graph equality between `threshold_extremal` and witness-cached flattening; only compare decision results.
6. The code contains a short comment near each liveness builder explaining the new invariant:

```cpp
// Final child states are continuation states too. Reaching a final state is a
// termination opportunity, not mandatory termination. Liveness is seeded only
// by one future edge into a matching final to avoid retroactive termination.
```

---

## 13. Implementation Order

Recommended order:

1. Add failing regression fixtures A, D, and E before patching.
2. Patch the shared threshold-extremal backend:
   - `thrext_build_mm_live`
   - `thrext_build_sum_cutoffs`
   - `thrext_step_frontier`
   - `thrext_spawn_frontier`
3. Patch regular flattening:
   - parent-aware return-value enumeration only:
     - `computeMinMaxReturnValuesParentAware`
     - `computeSumBReturnValuesParentAware`
   - `TargetAwareLive::build`
   - `step_obl_bag_finite`
   - `spawn_obligation_finite`
   - keep `BuchiState_obl` / phase logic unchanged
   - align `computeMinMaxReturnValues` / `computeSumBReturnValues` later if still needed outside the live path
4. Patch cached Min/Max threshold backend:
   - `build_mmthr_live`
   - `step_mmthr_obl_bag`
   - `spawn_mmthr_obligation`
   - `MMSupCachedBuilder::step_obl`
   - `MMInfCachedBuilder::{precompute_spawns, step_obl}`
5. Run all deterministic terminal-final benchmarks.
6. Add split-final normalization differential tests.
7. Decide how to handle the limit-average SumMinus synchronization path:
   - preferably normalize before synchronization;
   - otherwise patch `synchronizeChildren` and `flatten_Avg_SumMinus` directly.
8. Delete, deprecate, or patch legacy parser-based paths.

---

## 14. Common Mistakes to Avoid

### Mistake 1: Seeding final states directly in liveness

Do not do this:

```cpp
if (T.is_final[st] && outcome_matches_guess(...)) live[st] = true;
```

That permits retroactive termination from a final state that was already passed.

### Mistake 2: Patching step/spawn without patching liveness

If step/spawn carry wrong-final targets but liveness still excludes final states, the important witnesses remain pruned.

### Mistake 3: Comparing witness-cached and threshold-extremal by exact edge weights

The witness-cached Sup/LimSup construction intentionally emits `1` only for a selected witness and may emit `0` for other high-valued background children. Compare decision results, not exact flattened traces.

### Mistake 4: Patching `TermCachedBuilder`

Termination-only obligations do not need to continue through finals. Early termination is enough for background children.

### Mistake 5: Ignoring SumPlus/SumMinus cutoffs

For sum modes, final-state continuation depends on `thrext_build_sum_cutoffs`. Patching only `thrext_step_frontier` and `thrext_spawn_frontier` is insufficient.

---

## 15. Minimal Code Review Checklist

For each patched tracker, check that:

- [ ] Final source states are included in continuation/liveness graphs.
- [ ] Final target states are included as continuation nodes.
- [ ] Seeds are based on one future edge into a matching final, not current finalness.
- [ ] Step discharges only on an edge into a final with matching outcome.
- [ ] Wrong-final targets are carried if live.
- [ ] Matching-final targets discharge and are not also required to be carried.
- [ ] No retroactive “current final means discharge” rule was introduced.
- [ ] Tests include a case that must continue through at least two final states.
- [ ] Tests include a negative control where continuation never reaches the threshold.

---

## 16. Open TODOs After Current Patches

The following items remain after the completed `flatten_regular`, shared
threshold-extremal, and cached Min/Max / witness-cached patches.

### 16.1 Investigate `test_universality_correctness`

Status: done.

This issue had two separate causes.

First, universality needed accepted-domain semantics. Rejected words have no
value, so a universality query against a constant automaton must range only over
accepted words of the queried automaton. This matters for fixtures such as
`child_pump_loop`: ultimately-all-`a` words let spawned children run forever, so
those words are rejecting and cannot determine bottom values.

The implementation now threads an explicit boolean through the inclusion path:

- `Automaton::isUniversal(...)` calls inclusion with target-language-domain
  filtering enabled.
- `Automaton::isIncludedIn(...)`, the antichain path, the booleanized path, and
  `FORKLIFT::inclusion(...)` all accept and forward the boolean.
- When the constant automaton appears on the left of a universality reduction,
  FORKLIFT ignores a candidate counterexample lasso if the target automaton does
  not accept that lasso.

Second, FORKLIFT's lasso membership check itself was too weak for Buchi
acceptance. It accepted a lasso when a final state was reachable before a later
cycle, even if the repeated cycle contained no final state. That gave a false
acceptance result for flattened `a^omega` lassos whose only final visit was
transient.

The fix was kept inside the existing DFS structure:

- `membership_query_dfs2(...)` now maintains the active cycle stack separately
  from the stem-reachability stack.
- `membership_query_dfs3(...)` carries whether a final has been seen on the
  threshold-good suffix.
- Closing a cycle now succeeds only if the closed repeated segment contains a
  final state.
- The target-domain filter reuses this corrected `fast_membership(...)` with
  `B->getMinDomain()` as the threshold, so every edge is threshold-good and the
  check reduces to Buchi acceptance. The temporary standalone SCC/Tarjan
  acceptance helper was removed as overkill.

The remaining failures were stale test expectations. For `child_pump_loop`,
accepted words contain infinitely many `b`s. Therefore:

- positive fixture:
  - `Inf` / `LimInf` keep the old bottoms `Max_f=2`, `Min_f=1`,
    `SumB=3`, `SumPlus=3`
  - `Sup` / `LimSup` bottoms are `4` for `Max_f`, `Min_f`, `SumB`,
    and `SumPlus`, because every accepted word has infinitely many `b`-started
    children with value `4`
  - `SumMinus` is unbounded below only for `Inf` / `LimInf`; for `Sup` /
    `LimSup` the bottom is `-4`
- negated fixture:
  - `Max_f` and `Min_f` bottom at `-4` for all four outer aggregators
  - `SumB` bottoms at `-10` for `Inf` / `LimInf`, where long `a` blocks can hit
    the bound
  - `SumB` bottoms at `-4` for `Sup` / `LimSup`, where `b^omega` is the worst
    accepted word and any `a` block raises the sup/limsup via an `ab` suffix

No tests were excluded or skipped. The expectations were corrected to match the
accepted-domain semantics.

Verification:

```bash
make -C build tests -j2
./build/test_universality_correctness
./build/test_emptiness_correctness
ctest --test-dir build --output-on-failure
```

Observed results:

- `test_universality_correctness`: `320/320` passed
- `test_emptiness_correctness`: `483/483` passed
- full CTest: `13/13` passed

### 16.2 Add Direct Cached Sum Inf/LimInf Regressions

Status: done.

`SumInfCachedBuilder` delegates its spawn and step logic to the repaired
`thrext_spawn_frontier` and `thrext_step_frontier`, so it should already inherit
the threshold-extremal final-state continuation fix. Direct regressions now
cover the cached path because this path also depends on the repaired sum cutoff
computation.

Added test cases:

- `flatten_SumPlusMinus_Inf_cached(SumPlus, threshold)` on a wrong-final-then-high continuation fixture.
- `flatten_SumPlusMinus_Inf_cached(SumMinus, threshold)` on a wrong-final-then-low continuation fixture.
- Compare decisions against `flatten_SumPlusMinus_Inf`, not exact graph shape.

The tests live in `test_emptiness_correctness.cpp`:

- `test_sum_inf_cached_sumplus_wrong_final_can_continue`
- `test_sum_inf_cached_summinus_wrong_final_low_guess_is_spawnable`

Verification:

```bash
cmake --build build --target test_emptiness_correctness test_flatten_sumplusminus_inf -j2
./build/test_emptiness_correctness
./build/test_flatten_sumplusminus_inf
ctest --test-dir build --output-on-failure
```

Observed results:

- `test_emptiness_correctness`: `485/485` passed
- `test_flatten_sumplusminus_inf`: `18/18` passed
- full CTest: `13/13` passed

### 16.3 Add Split-Final Differential Tests

Status: done.

Implemented the test-only normalization oracle from Section 11:

```cpp
NestedAutomaton* split_child_finals_for_testing(const NestedAutomaton* A);
```

The helper preserves child 0 unchanged and splits real child final states into
explicit stop and continue copies. Deterministic targeted tests now compare
decisions between an automaton and its stop/continue-final split form for the
repaired public paths:

- `flatten_regular`
- shared threshold-extremal public paths:
  - `flatten_MinMax_Sup`
  - `flatten_MinMax_Inf`
  - `flatten_SumPlusMinus_Sup`
  - `flatten_SumPlusMinus_Inf`
- `flatten_MinMax_Sup_cached`
- `flatten_MinMax_Sup_witness_cached`
- `flatten_MinMax_Inf_cached`
- `flatten_SumPlusMinus_Inf_cached`

Added tests in `test_emptiness_correctness.cpp`:

- `test_split_final_regular_decisions_match_explicit_stop_continue`
- `test_split_final_threshold_decisions_match_explicit_stop_continue`
- `test_split_final_cached_decisions_match_explicit_stop_continue`

Verification:

```bash
cmake --build build --target test_emptiness_correctness -j2
./build/test_emptiness_correctness
ctest --test-dir build --output-on-failure
```

Observed results:

- `test_emptiness_correctness`: `488/488` passed
- full CTest: `13/13` passed

Randomized coverage was added in a standalone CTest target:

- `test_split_final_differential`

The generator is structured to avoid degenerate all-reject cases:

- two-symbol parent alphabet
- three real children
- each real child contains a forced `s0 --a--> f0 --b--> f1`
  final-continuation skeleton
- parent contains a forced accepting cycle that repeatedly spawns real children
- generated children use non-negative weights so the SumPlus threshold backend's
  precondition is respected

The default run uses `1000` generated cases. With the current backend matrix this
produces `80000` original-vs-split decision comparisons while keeping full CTest
runtime under 30 seconds. An earlier dense attempt with random extra edges was
too expensive for a default target, so random extra edges are intentionally not
enabled in this randomized oracle.

Additional verification:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_split_final_differential -j2
./build/test_split_final_differential
ctest --test-dir build --output-on-failure
```

Observed additional results:

- `test_split_final_differential`: `1000` generated cases, `80000` decision
  comparisons, `44000` true and `36000` false original decisions, passed in
  about `21.1s`
- full CTest: `14/14` passed in about `23.9s`

### 16.4 Handle Limit-Average `SumMinus` Synchronization

Status: done.

The `synchronizeChildren -> flatten_Avg_SumMinus` pipeline still treats final
states as terminal. This is separate from the obligation-tracker fixes.

Assumptions and constraints:

- This item covers only `finVal == SumMinus` with `LimInfAvg` or `LimSupAvg`
  in `NestedAutomaton::isNonEmpty(...)`.
- `synchronizeChildren()` is written for a pseudo-deterministic pre-sync NWA and
  uses `first_edge_or_null(...)` for both parent and child transitions.
- `synchronizeChildren()` currently treats final child states as terminal in two
  places:
  - it stops BFS exploration when `child_is_final(...) && !pending_accept`
  - it freezes a child that reached a final on a silent parent step via
    `pending_accept`
- `flatten_Avg_SumMinus(...)` also assumes terminal synchronized-child finals:
  it skips `ustep` construction for final `U_sync` states and drops active
  instances when `ustep.to_final` is true.
- Because of those assumptions, directly removing final-state skips is not a
  surgical fix. It would require redesigning `pending_accept`, synchronized
  child final metadata, and multiset drop behavior together.

Review conclusion:

- The stop/continue normalization is the right local production fix for this
  codebase.
- The helper must be a deep-copying production normalizer, not a reuse of the
  test-only split oracle and not a shallow child-pointer wrapper.
- The split must happen before the deterministic/complete branch is chosen, and
  `compute_c_bound(...)` must run on the exact pre-sync automaton that will be
  passed to `synchronizeChildren()`.
- Test wording must not claim that a positive continuation improves formal
  `SumMinus` if the intended semantics are `SumMinus(x) = -sum(abs(x_i))`.
  Mixed-sign fixtures are acceptable only as diagnostics for the current signed
  implementation convention, and should be labeled that way.

Preferred implementation: normalize before preprocessing.

Add a production stop/continue final-splitting normalization for real children,
then run the existing deterministic/complete preprocessing on the normalized
NWA:

```text
original NWA
  -> split continuable child finals
  -> makeCompleteNested or determinizeWithMacroAlphabet as needed
  -> synchronizeChildren
  -> flatten_Avg_SumMinus
```

Important ordering:

- Do **not** split immediately before `synchronizeChildren()` after the
  deterministic/complete branch has already been chosen.
- The split intentionally duplicates incoming edges to a final into stop and
  continue targets. That creates nondeterministic same-symbol child edges.
- If the original NWA was deterministic, the normalized NWA may no longer be
  deterministic. Therefore the existing branch decision must be rerun on the
  normalized NWA, so `determinizeWithMacroAlphabet()` can convert the
  stop/continue choice into macro-symbol determinism before synchronization.
- Splitting before completion is also required. `makeCompleteNested()` skips
  final child states when adding missing sink transitions. If `f_cont` were
  created after completion, it would be a new non-final continuation state that
  completion never saw.

#### 16.4.1 Add a production normalization helper

Add a private helper in `NestedAutomaton`:

```cpp
NestedAutomaton* splitContinuableChildFinals() const;
```

Return `nullptr` if no real child final state has outgoing transitions. The
caller can then keep using `this` and avoid graph churn for terminal-final
inputs.

Scope:

- If no split is needed, return `nullptr` without constructing a replacement
  automaton.
- If any split is needed, deep-copy the whole NWA ownership graph. Preserve
  child `0` semantically unchanged by deep-copying it, not by aliasing the
  original pointer.
- For each real child `i > 0`, split only final states with outgoing
  transitions.
- Leave terminal finals with no outgoing transitions as ordinary copied finals.
- Preserve child domains, weights, symbol ids, and initial-state semantics.
- Preserve the parent graph by copy or by the existing private
  `NestedAutomaton(const Automaton* parent, MapArray<ChildAutomaton*>*)`
  constructor, but ensure the returned NWA owns all children it will delete.

Transformation for every splittable final state `f`:

```text
f_stop  final, no outgoing transitions
f_cont  non-final, outgoing transitions copied from f
```

Transition copy rules:

- incoming edge to non-final `q`: copy to `q_copy`
- incoming edge to terminal final `f`: copy to `f_copy`
- incoming edge to splittable final `f`: duplicate to `f_stop` and `f_cont`
- outgoing edge from splittable final `f`: copy only from `f_cont`
- outgoing edge into another splittable final target still duplicates to that
  target's stop and continue copies
- if the original initial state is a splittable final, use `f_cont` as the new
  initial state, because child invocations still consume a non-empty word

Implementation shape:

- First scan all real children and record which final states are splittable.
- Build explicit old-state to new-state maps:
  - `normal_copy[old_id]` for non-final and unsplit terminal-final states
  - `stop_copy[old_id]` / `cont_copy[old_id]` for splittable finals
- Use a target-expansion helper for each old edge:

```cpp
std::vector<State*> targets_for_old_target(State* old_to);
```

- Map the source of each copied edge with a continuation-source helper:

```cpp
State* source_for_old_state(State* old_from); // f_cont for splittable finals
```

- Do not create any outgoing edge from `f_stop`.
- Do not assume old state ids survive the split. Old ids should index mapping
  vectors only; new ids come from the newly constructed `State` objects and are
  inserted into the new `MapArray` by those new ids.

Keep this helper production-local. The existing
`NestedAutomatonTester::split_child_finals_for_testing(...)` can remain as a
test oracle, but production code should not depend on test headers.

#### 16.4.2 Patch the `SumMinus + LimAvg` branch

In `NestedAutomaton::isNonEmpty(...)`, replace the current
`SumMinus + LimAvg` preprocessing block with this ownership shape:

```cpp
NestedAutomaton* split_nwa = this->splitContinuableChildFinals();
NestedAutomaton* base_nwa = split_nwa ? split_nwa : this;

NestedAutomaton* pre_sync_owned = nullptr;
NestedAutomaton* pre_sync = nullptr;
std::vector<bool> complete_flags;

if (base_nwa->isDeterministicNested() &&
    base_nwa->isCompleteNested(&complete_flags)) {
    pre_sync = base_nwa;
} else if (base_nwa->isDeterministicNested() &&
           !base_nwa->isCompleteNested(&complete_flags)) {
    pre_sync_owned = base_nwa->makeCompleteNested(&complete_flags);
    pre_sync = pre_sync_owned;
} else {
    pre_sync_owned = base_nwa->determinizeWithMacroAlphabet();
    pre_sync = pre_sync_owned;
}

c_bound = compute_c_bound(pre_sync);
sync_nwa = pre_sync->synchronizeChildren();
flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
```

Cleanup requirements:

- Delete `sync_nwa` as today.
- Delete `pre_sync_owned` if it was allocated.
- Delete `split_nwa` if it was allocated.
- Do not delete `base_nwa` when it aliases `this`.
- Keep `flat` and `nonSilent` cleanup unchanged.
- Replace the old `det_nwa` ownership role with `pre_sync_owned`, or make
  `det_nwa` strictly owned-only. Do not let one pointer sometimes mean an alias
  and sometimes mean an owned object.

Review points:

- `compute_c_bound(...)` must use the exact pre-sync automaton passed to
  `synchronizeChildren()`, because splitting changes the child state count.
- This also fixes the existing inconsistency where the branch may compute
  `c_bound` from `this` but synchronize a completed or macro-determinized NWA.
- Terminal-final inputs should skip normalization and keep the old fast path.
  They may still see a corrected `c_bound` when completion or determinization is
  used; treat that as a correctness fix and watch performance.
- Do not change `synchronizeChildren()` or `flatten_Avg_SumMinus()` in this
  patch except for comments if needed; the normalization should preserve their
  terminal-final model.

#### 16.4.3 Tests to add before patching

Add a targeted LimAvg/SumMinus fixture with a final state that has outgoing
transitions and compare the original automaton against the explicit
stop/continue split oracle.

Important `SumMinus` test caveat:

- If this branch is meant to implement formal `SumMinus(x) = -sum(abs(x_i))`,
  then positive continuation weights do not improve a child return. For example,
  `-5` followed by `+4` has formal value `-9`, not `-1`.
- The codebase also has signed-sum conventions in nearby reductions, so a
  mixed-weight fixture can be useful as an implementation diagnostic, but it
  must be labeled as such and should not be presented as a formal `SumMinus`
  oracle.
- With all non-positive child weights, existential emptiness for threshold
  `>= x` may prefer early stopping, so a hand threshold that exposes the
  continuation bug may be hard or impossible for some fixtures. In that case,
  use the explicit split oracle and an ordering test rather than overstating a
  decision-changing expected value.

Signed-implementation diagnostic, only if that convention is intentionally
supported in this branch:

```text
s0 --a/-5--> f0     f0 final
f0 --b/+4--> f1     f1 final
```

Under a raw signed-sum interpretation the continued value would be `-1`, which
separates it from the forced early stop at `-5`. Do not use this expectation if
the branch is enforcing formal absolute-value `SumMinus`.

Test oracle:

- Before the production patch, compare the original fixture against the existing
  test-only explicit split fixture:

```cpp
original.isNonEmpty(LimSupAvg, SumMinus, threshold)
split.isNonEmpty(LimSupAvg, SumMinus, threshold)
```

- Pick a threshold that separates the forced-stop and continued behaviors, then
  assert the patched original matches the explicit split, if such a threshold is
  valid for the chosen `SumMinus` convention.
- Repeat for `LimInfAvg` if the constructed ultimately-periodic witness gives
  the same stable value.

Add an ordering regression:

- Build an originally deterministic and complete child where the only
  nondeterminism introduced by normalization is the duplicated incoming edge to
  a splittable final.
- The test should compare the patched production path against the explicit split
  oracle.
- This guards against the incorrect ordering:

```text
decide deterministic/complete
  -> split finals
  -> synchronizeChildren   // unsafe: first_edge_or_null keeps one branch
```

because `synchronizeChildren()` would otherwise silently choose only stop or
continue.

Also add a terminal-final baseline:

- run existing `limavg_adversarial_summinus_*` fixtures before and after the
  patch
- assert their expected thresholds remain unchanged
- add one explicit fixture whose final states have no outgoing transitions and
  confirm `splitContinuableChildFinals()` returns `nullptr` indirectly by
  checking no determinization-only behavior or decision change is introduced

#### 16.4.4 Extend differential coverage

After the targeted regression is green, extend the split-final differential
oracle to this path with a small default budget:

- generated children should include at least one splittable final
- include mixed-weight cases only if they are explicitly testing the current
  signed implementation convention
- include all-non-positive cases for formal `SumMinus` compatibility, but do not
  require a hand-picked threshold improvement unless one is semantically valid
- compare only decisions, not state counts or edge weights
- keep the case count low enough that full CTest stays under the current
  runtime envelope

Suggested test names:

- `test_limavg_summinus_final_continuation_matches_split`
- `test_limavg_summinus_terminal_finals_unchanged`
- `test_split_final_limavg_summinus_decisions_match_explicit_stop_continue`

#### 16.4.5 Detailed execution plan

Apply this item in small phases. Each phase should have a clear verification
point before moving on.

Phase 0: baseline and convention check.

1. Confirm the current `SumMinus + LimAvg` branch behavior in
   `NestedAutomaton::isNonEmpty(...)`:
   - it currently owns `det_nwa` only when completion or macro-determinization
     allocates a replacement
   - it computes `c_bound` from `this`
   - it calls `synchronizeChildren()` on either `this`, `det_nwa`, or the
     completed automaton
2. Confirm the implementation convention for this path before writing a
   decision-separating mixed-weight test:
   - if formal `SumMinus = -sum(abs(w_i))` is intended, do not use a positive
     continuation as an improvement oracle
   - if current signed-sum behavior is intentionally supported here, label the
     mixed-weight regression as a signed-implementation diagnostic
3. Run the current baseline to know whether the branch is already green before
   new failures are introduced:

```bash
cmake --build build --target test_emptiness_correctness test_synchronization -j2
./build/test_emptiness_correctness
./build/test_synchronization
```

Phase 1: add focused failing tests.

1. Add test file constants in `test_correctness_common.h` for the new fixtures.
2. Add one targeted original-vs-explicit-split oracle test:
   - load the original NWA
   - build `NestedAutomatonTester::split_child_finals_for_testing(&original)`
   - compare `isNonEmpty(LimSupAvg, SumMinus, x)` on both
   - repeat for `LimInfAvg` only if the fixture has a stable ultimately-periodic
     value for both average variants
3. Add the ordering regression:
   - fixture starts deterministic and complete
   - normalization introduces exactly the duplicated stop/continue choice
   - test compares production original against explicit split
   - this catches splitting after the deterministic/complete branch decision
4. Add the terminal-final baseline:
   - reuse existing `limavg_adversarial_summinus_*` expectations
   - add one small fixture with only terminal finals if needed
   - assert decisions remain unchanged
5. Verify that at least one new targeted test fails before the production patch
   if a decision-separating fixture is semantically valid. If formal `SumMinus`
   makes such a decision test impractical, the pre-patch signal may instead be
   the ordering-oracle mismatch.

Phase 2: add the production normalizer.

1. Add the declaration to `NestedAutomaton.h` private methods:

```cpp
NestedAutomaton* splitContinuableChildFinals() const;
```

2. Implement it in `NestedAutomaton.cpp` near existing child-copy helpers, before
   `synchronizeChildren()` or near `NestedAutomaton::removeSilentTransitions(...)`.
3. Helper structure:
   - `child_has_splittable_final(const ChildAutomaton*)`
   - `state_has_outgoing(State*)`
   - `copy_child_with_continuable_final_split(const ChildAutomaton*)`
   - optionally `copy_child_without_split(const ChildAutomaton*)` wrapping the
     `ChildAutomaton` copy constructor
4. Overall normalizer algorithm:

```text
scan children i > 0 for final states with outgoing transitions
if none found:
    return nullptr

allocate MapArray<ChildAutomaton*>(children_->size())
for every child id:
    if child id is 0 or this child has no splittable final:
        insert deep copy via ChildAutomaton copy constructor
    else:
        insert split copy

return new NestedAutomaton(this, copied_children)
```

5. Split-copy implementation details:
   - copy the child alphabet and weights, preserving ids
   - build `normal_copy`, `stop_copy`, and `cont_copy` vectors indexed by old
     state id
   - terminal finals without outgoing transitions use `normal_copy` and stay
     final
   - splittable finals get final `stop_copy` and non-final `cont_copy`
   - initial state maps to `cont_copy` for splittable finals, otherwise
     `normal_copy`
   - for each old edge, map source through `source_for_old_state(...)`
   - expand target through `targets_for_old_target(...)`
   - add one copied edge for each expanded target
6. Ownership and id checks:
   - never insert an original child pointer into the returned `children_`
   - never create outgoing edges from a stop copy
   - insert every new state by its new id into the new child `MapArray`
   - old ids should only index mapping vectors
   - keep resets local and consistent with nearby constructors/tests; verify
     copied symbol, state, and weight ids match their arrays

Phase 3: rewire `isNonEmpty(...)`.

1. Replace the current `SumMinus + LimAvg` block with the `split_nwa`,
   `base_nwa`, `pre_sync_owned`, `pre_sync`, `sync_nwa` ownership shape.
2. Run deterministic/complete checks on `base_nwa`, not on `this`.
3. Compute:

```cpp
c_bound = compute_c_bound(pre_sync);
```

not `compute_c_bound(this)`.
4. Ensure cleanup deletes:
   - `flat`
   - `nonSilent`
   - `sync_nwa`
   - `pre_sync_owned`
   - `split_nwa`
5. Ensure cleanup does not delete:
   - `this`
   - `base_nwa` when it aliases `this`
   - `pre_sync` when it aliases `base_nwa`
6. Keep other branches unchanged. Do not touch `synchronizeChildren()` or
   `flatten_Avg_SumMinus(...)` in this phase.

Phase 4: targeted verification.

Run the smallest relevant targets first:

```bash
cmake --build build --target test_emptiness_correctness test_synchronization -j2
./build/test_emptiness_correctness
./build/test_synchronization
```

If a test fails:

- if it fails only after terminal-final fixtures, inspect the `nullptr` fast path
  and the corrected `c_bound(pre_sync)` performance/behavior impact
- if it fails on the ordering regression, check that splitting happens before
  `isDeterministicNested()` and `isCompleteNested(...)`
- if it fails by losing one stop/continue branch, inspect use of
  `first_edge_or_null(...)` and confirm determinization happened after splitting

Phase 5: differential coverage.

1. Extend the existing split-final differential test matrix only for
   `LimInfAvg` / `LimSupAvg` with `SumMinus`.
2. Keep generated cases small:
   - one or two real children
   - two or three child states
   - at least one splittable final
   - short parent accepting cycles
   - low case count by default
3. Compare decisions only:

```text
original.isNonEmpty(Lim*Avg, SumMinus, x)
==
split.isNonEmpty(Lim*Avg, SumMinus, x)
```

4. Do not compare graph sizes, edge weights, or exact flattened traces.

Phase 6: final verification and documentation.

Run the full local suite:

```bash
ctest --test-dir build --output-on-failure
```

Then update this item from `Status: open` to a completion note with:

- files changed
- tests added
- exact commands run
- observed pass counts
- any remaining caveat about formal versus signed `SumMinus`

#### 16.4.6 Fallback direct patch, only if normalization is rejected

Directly patching the synchronized pipeline is higher risk and should not be the
first implementation.

If required, the direct design must introduce an explicit stop/continue
distinction into synchronized states. A bare `pending_accept` boolean is not
enough:

- on an edge into a final, fork a stop branch and a continuation branch
- stop branches may freeze and flush as today
- continuation branches must keep stepping from final states
- `flatten_Avg_SumMinus(...)` must know which synchronized-child finals are real
  terminal stop states and which are continuation states
- active multiset instances should drop only on stop-state finals, not merely on
  any final state

Do not implement the direct version by only removing:

```cpp
if (s_final) continue;
```

from `flatten_Avg_SumMinus(...)`; that loses the stop/continue distinction and
can introduce retroactive or mandatory-continuation behavior.

Success criterion: limit-average `SumMinus` handles outgoing final states without
silently forcing termination, and the patched original automaton agrees with the
explicit stop/continue split oracle for targeted and small generated
LimAvg/SumMinus cases.

Verification commands:

```bash
cmake --build build --target test_emptiness_correctness test_synchronization -j2
./build/test_emptiness_correctness
./build/test_synchronization
ctest --test-dir build --output-on-failure
```

Completion note:

- Production code now normalizes continuable child finals before the
  `SumMinus + LimInfAvg/LimSupAvg` synchronization pipeline.
- Added private `NestedAutomaton::splitContinuableChildFinals()` plus local
  deep-copy helpers in `NestedAutomaton.cpp`.
- The normalizer returns `nullptr` when no real child has a final state with
  outgoing transitions, so terminal-final inputs avoid graph churn.
- When normalization is needed, it deep-copies every child, splits real child
  finals with outgoing transitions into terminal stop and non-final continue
  copies, and returns a new owned NWA via the private parent-copy constructor.
- `isNonEmpty(...)` now runs deterministic/complete preprocessing on the
  normalized base NWA and computes `c_bound` from the exact `pre_sync` automaton
  passed to `synchronizeChildren()`.
- The synchronized pipeline itself remains unchanged.

Added regression coverage:

- `limavg_summinus_final_continuation_signed.txt`
- `test_limavg_summinus_final_continuation_signed_matches_split`

This regression is intentionally documented as a signed-implementation
diagnostic. It compares the production path against the explicit stop/continue
split oracle for `LimSupAvg` and `LimInfAvg` with `SumMinus`. Before the
production patch, it failed because the original path forced termination at the
first final; after the patch, it matches the explicit split.

Verification:

```bash
cmake --build build --target test_emptiness_correctness -j2
./build/test_emptiness_correctness
cmake --build build --target test_synchronization -j2
./build/test_synchronization
ctest --test-dir build --output-on-failure
```

Observed results:

- `test_emptiness_correctness`: `494/494` passed
- `test_synchronization`: `13/13` passed
- full CTest: `14/14` passed in about `26.65s`

### 16.5 Align Non-Parent-Aware Return Helpers

Status: done.

`computeMinMaxReturnValues` and `computeSumBReturnValues` now match the
final-state-continuation contract used by the parent-aware helpers.

Important scope note:

- The current live `flatten_regular` path uses
  `computeChildReturnValuesParentAware(...)`, not these non-parent-aware
  helpers.
- This item was a semantic consistency fix for the stale private
  `computeChildReturnValues(...)` wrapper, so it cannot become a bad oracle if
  reused later.

Implementation:

- `computeMinMaxReturnValues(...)` records a return value immediately after an
  edge enters a final state, then still enqueues the reached `(state, value)`
  configuration if it is new.
- `computeSumBReturnValues(...)` does the same for bounded-sum configurations:
  final targets are recorded as return opportunities and remain available for
  later continuation.
- No retroactive current-final discharge rule was added.

Added direct helper regressions in `test_emptiness_correctness.cpp` through a
test-only `NestedAutomatonTester::compute_child_return_values(...)` wrapper:

- `test_child_return_values_non_parent_aware_max_wrong_final_can_continue`
- `test_child_return_values_non_parent_aware_max_wrong_final_negative_control`
- `test_child_return_values_non_parent_aware_max_multiple_finals`
- `test_child_return_values_non_parent_aware_min_wrong_final_can_continue`
- `test_child_return_values_non_parent_aware_sumb_wrong_final_can_continue`

Verification:

```bash
cmake --build build --target test_emptiness_correctness -j2
./build/test_emptiness_correctness
ctest --test-dir build --output-on-failure
```

Observed results:

- `test_emptiness_correctness`: `493/493` passed
- full CTest: `14/14` passed in about `25.0s`

### 16.6 Resolve Legacy Parser-Based Paths

Status: intentionally deferred.

Several old parser-based implementations remain after early returns to
`flatten_threshold_extremal_impl`, and old Min/Max variants are still present.

Decision:

- Leave these legacy/parser-based paths alone for now.
- Do not use them as correctness oracles for final-state-continuation behavior.
- Revisit only if a test, experiment, or CLI path is found to call one of these
  implementations directly.

This item is not part of the current final-state-continuation patch set.
