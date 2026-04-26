Below is the implementation note I would use. It is focused only on **`Sup/LimSup + SumPlus`**, not `SumMinus`, not `Inf`, not `Min/Max`.

# Goal

Replace the current `flatten_SumPlusMinus_Sup(SumPlus, x)` path with a construction that is:

1. **Correct for overlapping child calls**, including multiple active local states of the same child automaton.
2. **Cleaner than the current recursive selector**, which is where the same-child skip bug lives.
3. **Efficient enough to use as the fast path**, preferably without materializing a string-based flattened `Automaton`.
4. **Semantically local**: background child calls are tracked only for validity/fairness, while one distinguished monitor tracks the `SumPlus >= threshold` objective.

The current code tries to do this with `activation`, `tracking`, and one distinguished budget-tracked token, but the selector advances from one active local state to the next child automaton, which can skip other active local states of the same child. That is the root bug. The current finalization also derives finality from `tracking_from` and the destination parent state, then inserts the destination state into `final_states`, which makes acceptance easier to reason about if we replace it with transition-labeled acceptance in the new engine.  

# Core idea

Use a **breakpoint obligation set** plus a **single SumPlus monitor**.

The background obligation machinery guarantees that all spawned child calls eventually terminate. The monitor is used only to detect child calls whose accumulated SumPlus value reaches the threshold.

Instead of emitting edge weight `1` when we *guess* a child call will succeed, emit a `success` event when the monitored child actually terminates with accumulated sum at least the threshold.

This is cleaner for `Sup` and `LimSup`:

* For `Sup`, existence of one successful child call is preserved if we mark success at return time instead of call time.
* For `LimSup`, infinitely many successful child calls imply infinitely many successful finite return times, so success-at-return is also equivalent.

# Assumption for the fast path

This version assumes real `SumPlus`: all child edge weights are nonnegative.

If negative child weights are allowed in your input language, do **not** use a capped monotone accumulator. Either reject that input for this fast path or fall back to a signed/exact construction.

Add an explicit validation pass:

```cpp
static void validate_sumplus_nonnegative(const NestedAutomaton* A) {
    for (uint32_t cid = 0; cid < A->getChildrenSize(); ++cid) {
        ChildAutomaton* child = A->getChild(cid);
        if (!child) continue;
        if (child->getStates()->size() <= 1) continue; // dummy/silent

        for (State* s : *child->getStates()) {
            if (!s) continue;
            for (Symbol* sym : *s->getAlphabet()) {
                auto* succs = s->getSuccessors(sym->getId());
                if (!succs) continue;

                for (Edge* e : *succs) {
                    if (e->getWeight()->getValue() < weight_t(0)) {
                        QUAK_FAIL("SumPlus fast path requires nonnegative child weights");
                    }
                }
            }
        }
    }
}
```

# State representation

Do not encode states as strings. Use structural keys.

```cpp
struct SupSumPlusKey {
    uint32_t parent;

    Bits A; // active child states
    Bits O; // current breakpoint obligations, O subset of A

    bool monitor_active = false;
    uint32_t monitor_state = UINT32_MAX; // global child-state id
    internal_weight_t monitor_sum = 0;   // capped at threshold
};
```

Where `Bits` is a dynamic bitset over the flattened child-state universe.

Flatten child states exactly once:

```cpp
global_child_state = cumulative_size[child_id] + local_state_id;
```

Maintain these invariants:

```cpp
O ⊆ A
monitor_active ==> A.test(monitor_state)
A never contains final child states
O never contains final child states
monitor_active ==> monitor_state is not final
monitor_sum ∈ [0, threshold]
```

The current code has `activation` and `tracking` bitvectors, but resets tracking to “all ones” and relies on later active checks to ignore irrelevant entries. The new invariant `O ⊆ A` is both smaller and easier to audit. 

# Precomputed tables

Build compact tables up front.

```cpp
struct ChildMove {
    uint32_t to_global;
    internal_weight_t cost; // scaled nonnegative child edge weight
    bool to_final;
};

struct ChildStateInfo {
    uint32_t child_id;
    uint32_t local_state_id;
    bool is_final;
};

struct ParentMove {
    uint32_t to_parent;
    uint32_t child_id;
    bool is_real_call;       // false for dummy/silent child
    uint32_t spawn_global;   // valid iff is_real_call and child init is non-final
    bool spawn_init_final;   // rare edge case
};
```

Tables:

```cpp
std::vector<ChildStateInfo> child_info;              // size = total child states
std::vector<std::vector<ChildMove>> child_post;      // child_post[global_state][symbol_id]
std::vector<std::vector<std::vector<ParentMove>>> parent_post;
// parent_post[parent_state][symbol_id] -> parent moves
```

Build `child_post` so that final child states have no outgoing moves. A child transition into a final state should have `to_final = true` and should **not** put the final state into `A` or `O`.

# Threshold scaling

Positive thresholds must be converted with **ceiling**, not nearest rounding.

```cpp
static internal_weight_t threshold_to_internal_ceil(weight_t x,
                                                    internal_weight_t scale) {
    double z = static_cast<double>(x.to_float()) * static_cast<double>(scale);
    return static_cast<internal_weight_t>(std::ceil(z - 1e-9));
}

static internal_weight_t child_weight_to_internal(weight_t w,
                                                  internal_weight_t scale) {
    double z = static_cast<double>(w.to_float()) * static_cast<double>(scale);
    return static_cast<internal_weight_t>(std::llround(z));
}
```

For `SumPlus` with `x > 0`:

```cpp
internal_weight_t scale = compute_weight_scale(this);
internal_weight_t threshold = threshold_to_internal_ceil(x, scale);

if (threshold == 0) {
    QUAK_FAIL("positive threshold converted to zero; scale computation is insufficient");
}
```

The current path uses `to_internal(threshold, scale)`, which rounds to nearest. That can lower a threshold like `0.24` on a `0.1` grid to `0.2`, causing false positives. 

# Transition event labels

Do not make acceptance a property of the destination key alone. Use transition labels:

```cpp
struct Succ {
    SupSumPlusKey to;
    bool epoch_accept; // parent Büchi + breakpoint completion event
    bool success;      // monitored child returned with sum >= threshold
};
```

This avoids a subtle issue in the current parser-based approach: the same destination key can be reached through both accepting and non-accepting finalization events, but the parser representation can only mark the destination state as final globally.

In the direct engine, acceptance is checked on labeled transitions.

# Breakpoint semantics

`A` contains all currently active child states.

`O` contains the active child states that must terminate before the next breakpoint completion.

At a transition from state `k`:

```cpp
bool closing_epoch = k.O.empty();
Bits O_start = closing_epoch ? Bits(total_child_states) : k.O;
```

If `O` is empty at the source, the old epoch has already completed. During this transition, no old obligations are propagated. After computing the next active set `A1`, start the next epoch with:

```cpp
O1 = A1;
```

If `O` is not empty, propagate only the old obligations:

```cpp
O1 = propagated_O;
```

The transition has a parent/Büchi acceptance event exactly when:

```cpp
epoch_accept = closing_epoch && parent_is_final[parent_to];
```

This matches the intent of the current `tracking_from`-based finalization, but represents it as a transition event instead of a global state flag. 

# Monitor semantics

The monitor tracks one distinguished child invocation.

```cpp
struct MonitorPost {
    bool valid = false;

    bool forced = false;
    uint32_t forced_from = UINT32_MAX;
    ChildMove forced_move;

    bool monitor_active_to = false;
    uint32_t monitor_state_to = UINT32_MAX;
    internal_weight_t monitor_sum_to = 0;

    bool success = false;
};
```

If the monitor is active, it must advance on the current symbol. For each possible child move:

```cpp
sum2 = std::min(threshold, old_sum + move.cost);
```

If the move reaches final:

```cpp
success = (sum2 >= threshold);
monitor_active_to = false;
```

If the move does not reach final:

```cpp
monitor_active_to = true;
monitor_state_to = move.to_global;
monitor_sum_to = sum2;
```

If there is no move from the monitor state on this symbol, the branch is invalid.

Important: if a monitored child returns below threshold, this is **not** a sink/failure branch. It is simply an unsuccessful monitored candidate. The background obligation still terminated correctly.

That is cleaner than the current implementation, which emits weight `1` at monitor start and later sends the branch to failure if the chosen witness does not validate. 

# Starting a monitor

Only start a monitor at a new real child spawn. Do not start monitoring an already-active unmonitored child later, because you do not know its accumulated prefix sum.

For each real parent call, there are two choices when no monitor is active:

```cpp
// Choice 1: do not monitor this new call.
monitor remains inactive.

// Choice 2: monitor this new call.
monitor starts at child initial state before consuming the current symbol.
```

If a monitor is already active, do not start a second one.

One monitor is enough:

* For `Sup`, choose the one successful call.
* For `LimSup`, choose one successful call, wait for it to return, then choose a later successful call, forever.

# Background post operation

The central operation is:

```cpp
post_background(symbol_id, A_start, O_start, optional forced move)
    -> all possible pairs (A1, Oprop1)
```

It advances every active child state in `A_start` on the current symbol. If an active state has no legal move, the branch is invalid.

The traversal must iterate over a flat list of active global child states. It must **not** recurse by child automaton and then jump to `child_id + 1`, which is the current bug.

```cpp
struct ForcedMove {
    bool enabled = false;
    uint32_t from = UINT32_MAX;
    ChildMove move;
};

struct BackgroundPost {
    Bits A1;
    Bits Oprop1;
};
```

Reference implementation shape:

```cpp
void enumerate_background_post(
    size_t pos,
    const std::vector<uint32_t>& sources,
    uint32_t symbol_id,
    const Bits& O_start,
    const ForcedMove& forced,
    Bits& A1,
    Bits& Oprop1,
    std::vector<BackgroundPost>& out,
    const std::vector<std::vector<ChildMove>>& child_post,
    const std::vector<ChildStateInfo>& child_info
) {
    if (pos == sources.size()) {
        out.push_back({A1, Oprop1});
        return;
    }

    const uint32_t src = sources[pos];

    // Final child states should normally never be in A_start, but be robust.
    if (child_info[src].is_final) {
        enumerate_background_post(
            pos + 1, sources, symbol_id, O_start, forced,
            A1, Oprop1, out, child_post, child_info
        );
        return;
    }

    std::vector<ChildMove> moves_storage;
    const std::vector<ChildMove>* moves = nullptr;

    if (forced.enabled && forced.from == src) {
        moves_storage.push_back(forced.move);
        moves = &moves_storage;
    } else {
        moves = &child_post[src][symbol_id];
    }

    if (moves->empty()) {
        // Invalid branch. Do not emit anything.
        return;
    }

    for (const ChildMove& m : *moves) {
        const bool old_A = m.to_final ? false : A1.test(m.to_global);
        const bool old_O = m.to_final ? false : Oprop1.test(m.to_global);

        if (!m.to_final) {
            A1.set(m.to_global);

            if (O_start.test(src)) {
                Oprop1.set(m.to_global);
            }
        }

        enumerate_background_post(
            pos + 1, sources, symbol_id, O_start, forced,
            A1, Oprop1, out, child_post, child_info
        );

        if (!m.to_final) {
            A1.set_to(m.to_global, old_A);
            Oprop1.set_to(m.to_global, old_O);
        }
    }
}
```

The critical property is this:

```cpp
pos + 1
```

The continuation is over the next active source state, not the next child automaton. This is the direct fix for the same-child overlap bug.

# Successor generation

The successor generator combines parent movement, optional monitor start/advance, and background post.

```cpp
std::vector<Succ> successors(const SupSumPlusKey& k, uint32_t symbol_id) {
    std::vector<Succ> result;

    for (const ParentMove& pm : parent_post[k.parent][symbol_id]) {
        generate_for_parent_move(k, symbol_id, pm, result);
    }

    return result;
}
```

Detailed shape:

```cpp
void generate_for_parent_move(const SupSumPlusKey& k,
                              uint32_t symbol_id,
                              const ParentMove& pm,
                              std::vector<Succ>& out) {
    Bits A_start = k.A;

    if (pm.is_real_call && !pm.spawn_init_final) {
        A_start.set(pm.spawn_global);
    }

    const bool closing_epoch = k.O.empty();
    Bits O_start = closing_epoch ? Bits(total_child_states) : k.O;

    // Case 1: existing monitor, or no monitor and no new monitor start.
    if (k.monitor_active) {
        for (const MonitorPost& mp : advance_existing_monitor(k, symbol_id)) {
            emit_posts(k, pm, symbol_id, A_start, O_start,
                       closing_epoch, mp, out);
        }
    } else {
        MonitorPost no_monitor;
        no_monitor.valid = true;
        no_monitor.forced = false;
        no_monitor.monitor_active_to = false;
        no_monitor.success = false;

        emit_posts(k, pm, symbol_id, A_start, O_start,
                   closing_epoch, no_monitor, out);

        // Case 2: start monitoring the new child call, if any.
        if (pm.is_real_call && !pm.spawn_init_final) {
            for (const MonitorPost& mp :
                 start_and_advance_monitor(pm.spawn_global, symbol_id)) {
                emit_posts(k, pm, symbol_id, A_start, O_start,
                           closing_epoch, mp, out);
            }
        }
    }
}
```

Monitor advance functions:

```cpp
std::vector<MonitorPost>
advance_existing_monitor(const SupSumPlusKey& k, uint32_t symbol_id) {
    std::vector<MonitorPost> out;

    const uint32_t src = k.monitor_state;
    const auto& moves = child_post[src][symbol_id];

    for (const ChildMove& m : moves) {
        internal_weight_t sum2 =
            std::min(threshold, k.monitor_sum + m.cost);

        MonitorPost mp;
        mp.valid = true;
        mp.forced = true;
        mp.forced_from = src;
        mp.forced_move = m;

        if (m.to_final) {
            mp.monitor_active_to = false;
            mp.success = (sum2 >= threshold);
        } else {
            mp.monitor_active_to = true;
            mp.monitor_state_to = m.to_global;
            mp.monitor_sum_to = sum2;
            mp.success = false;
        }

        out.push_back(mp);
    }

    return out;
}

std::vector<MonitorPost>
start_and_advance_monitor(uint32_t spawn_global, uint32_t symbol_id) {
    std::vector<MonitorPost> out;

    const auto& moves = child_post[spawn_global][symbol_id];

    for (const ChildMove& m : moves) {
        internal_weight_t sum2 = std::min(threshold, m.cost);

        MonitorPost mp;
        mp.valid = true;
        mp.forced = true;
        mp.forced_from = spawn_global;
        mp.forced_move = m;

        if (m.to_final) {
            mp.monitor_active_to = false;
            mp.success = (sum2 >= threshold);
        } else {
            mp.monitor_active_to = true;
            mp.monitor_state_to = m.to_global;
            mp.monitor_sum_to = sum2;
            mp.success = false;
        }

        out.push_back(mp);
    }

    return out;
}
```

Then combine with background post:

```cpp
void emit_posts(const SupSumPlusKey& k,
                const ParentMove& pm,
                uint32_t symbol_id,
                const Bits& A_start,
                const Bits& O_start,
                bool closing_epoch,
                const MonitorPost& mp,
                std::vector<Succ>& out) {
    if (!mp.valid) return;

    ForcedMove forced;
    forced.enabled = mp.forced;
    forced.from = mp.forced_from;
    forced.move = mp.forced_move;

    std::vector<uint32_t> sources = A_start.to_indices();

    Bits A1(total_child_states);
    Bits Oprop1(total_child_states);
    std::vector<BackgroundPost> posts;

    enumerate_background_post(
        0, sources, symbol_id, O_start, forced,
        A1, Oprop1, posts, child_post, child_info
    );

    for (BackgroundPost& bp : posts) {
        Bits O1 = closing_epoch ? bp.A1 : bp.Oprop1;

        SupSumPlusKey to;
        to.parent = pm.to_parent;
        to.A = std::move(bp.A1);
        to.O = std::move(O1);
        to.monitor_active = mp.monitor_active_to;

        if (mp.monitor_active_to) {
            to.monitor_state = mp.monitor_state_to;
            to.monitor_sum = mp.monitor_sum_to;

            // Defensive invariant.
            if (!to.A.test(to.monitor_state)) {
                QUAK_FAIL("monitor state must be active");
            }
        }

        Succ s;
        s.to = canonicalize(std::move(to));
        s.success = mp.success;
        s.epoch_accept = closing_epoch && parent_is_final[pm.to_parent];

        out.push_back(std::move(s));
    }
}
```

`canonicalize` should assert:

```cpp
to.O &= to.A;
to.A.clear_all_final_child_states();
to.O.clear_all_final_child_states();

if (!to.monitor_active) {
    to.monitor_state = UINT32_MAX;
    to.monitor_sum = 0;
}
```

# Caching

Cache the background post, because it is the expensive part and does not depend on parent state or monitor sum.

```cpp
struct PostCacheKey {
    uint32_t symbol_id;
    Bits A_start;
    Bits O_start;

    bool forced;
    uint32_t forced_from;
    uint32_t forced_to;
    bool forced_to_final;
};
```

The forced move’s cost does not matter for background propagation; only its destination/finality matters. The cost is used by the monitor before calling background post.

```cpp
std::unordered_map<PostCacheKey, std::vector<BackgroundPost>, PostCacheHash> post_cache;
```

Use it inside `emit_posts`.

# Emptiness checks

The direct engine should not first materialize an `Automaton`. It should expose a lazy successor generator returning `Succ` records.

## For `Sup`

Need:

```text
an infinite run with infinitely many epoch_accept events,
and at least one success event somewhere.
```

Simplest robust check: search the graph over:

```cpp
struct SupSearchState {
    SupSumPlusKey key;
    bool seen_success;
};
```

A transition updates:

```cpp
seen_success_to = seen_success || succ.success;
```

The Büchi acceptance event is:

```cpp
succ.epoch_accept && seen_success_to
```

Then run a normal transition-labeled Büchi emptiness check: find a reachable SCC containing at least one accepting transition.

Equivalent implementation:

1. Explore reachable `(key, seen_success)` states lazily.
2. Run Tarjan SCC.
3. An SCC is accepting if it contains an internal transition with `epoch_accept && seen_success_to`.

## For `LimSup`

Need:

```text
infinitely many epoch_accept events,
and infinitely many success events.
```

Run SCC search on `SupSumPlusKey` states, with transition labels.

An SCC is accepting iff it contains:

```cpp
has_epoch_accept_edge == true
has_success_edge == true
```

and both edges are internal to the SCC.

Because the SCC is strongly connected, if both event edges are inside the SCC, there is an infinite run visiting both infinitely often.

# Debug-compatible flattened automaton

For early validation, it is useful to keep a debug-only materialization path:

```cpp
Automaton* build_debug_automaton_from_generator(...)
```

But production should call:

```cpp
bool NestedAutomaton::isNonEmpty_SumPlus_SupLike(value_function_t infVal,
                                                 weight_t threshold);
```

Then `isNonEmpty` dispatch becomes:

```cpp
if (finVal == SumPlus && (infVal == Sup || infVal == LimSup)) {
    return this->isNonEmpty_SumPlus_SupLike(infVal, x);
}
```

For comparison testing, temporarily keep the old path behind a flag.

# Minimal patch while implementing the new engine

Still patch the current selector immediately.

Inside the active-state branch of `explore_global_selection_supremum`, every continuation should go to the next local state of the same child:

```cpp
explore_global_selection_supremum(child_id, child_state_id + 1, data);
```

not:

```cpp
explore_global_selection_supremum(child_id + 1, 0, data);
```

Only the “finished all local states of this child” case should move to:

```cpp
explore_global_selection_supremum(child_id + 1, 0, data);
```

Also, active background states with an empty successor set should invalidate the branch, not silently disappear.

# Regression tests

Add these before the rewrite and keep them permanently.

## 1. Same-child overlap false positive

Parent:

```text
p0 initial non-final
p1 final

p0 --a / C --> p1
p1 --b / C --> p1
```

Child `C`:

```text
q0 initial
q1
qbad
qf final

q0 --a / 0 --> q1
q0 --b / 1 --> qf
q1 --b / 0 --> qbad
qbad --b / 0 --> qbad
```

Query:

```cpp
isNonEmpty(Sup, SumPlus, 1)
```

Expected:

```text
false
```

Reason: the old call reaches `qbad` and never terminates. The fresh `q0 --b--> qf` success must not erase the old `q1` obligation.

## 2. Same-child overlap under `LimSup`

Same automaton.

```cpp
isNonEmpty(LimSup, SumPlus, 1)
```

Expected:

```text
false
```

## 3. Fractional threshold

Parent:

```text
p initial final
p --a / C --> p
```

Child:

```text
q0 initial
qf final
q0 --a / 0.2 --> qf
```

Query:

```cpp
isNonEmpty(Sup, SumPlus, 0.24)
```

Expected:

```text
false
```

This catches nearest-rounding threshold bugs.

## 4. Immediate success

Parent:

```text
p initial final
p --a / C --> p
```

Child:

```text
q0 initial
qf final
q0 --a / 1 --> qf
```

Query:

```cpp
isNonEmpty(Sup, SumPlus, 1)
```

Expected:

```text
true
```

Also:

```cpp
isNonEmpty(LimSup, SumPlus, 1)
```

Expected:

```text
true
```

## 5. Long-running monitor

Parent repeatedly calls `C`.

Child:

```text
q0 initial
q1
q2
qf final

q0 --a / 0 --> q1
q1 --a / 0 --> q2
q2 --a / 1 --> qf
```

Parent:

```text
p initial final
p --a / C --> p
```

Query:

```cpp
isNonEmpty(LimSup, SumPlus, 1)
```

Expected:

```text
true
```

This checks that one monitor can wait for a multi-step successful child and then later monitor another.

# Summary of the intended replacement

The implementation should aim for this architecture:

```text
NestedAutomaton
   |
   | precompute parent/child transition tables
   v
SupSumPlusSuccessorGenerator
   - structural state key: parent, A, O, monitor
   - cached background post
   - monitor emits success at child return
   - transition-labeled epoch_accept
   |
   v
Lazy SCC emptiness
   - Sup: Büchi over (key, seen_success)
   - LimSup: SCC with success edge and epoch_accept edge
```

This avoids the fragile recursive child/local traversal, avoids string-state construction in the hot path, preserves the one-monitor efficiency advantage, and gives a direct place to reason about correctness: every active source state is advanced exactly once per input symbol, and every breakpoint obligation is either propagated or discharged.
