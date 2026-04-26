### What `{Inf,LimInf} × {Min,Max}` flattening is *really* doing

For these four combinations, the outer objective is **“eventually always above threshold”** in the `LimInf` case (finitely many bad returns allowed), and **“always above threshold”** in the `Inf` case (no bad returns allowed). In both cases, the reduction target is a **0/1-weighted Büchi automaton** such that:

* every non-silent parent step that spawns a child call emits a weight in `{0,1}`,
* and the resulting 0/1 word has `Inf ≥ 1` (i.e., all 1s) or `LimInf ≥ 1` (i.e., eventually all 1s)

iff the original nested run has the desired threshold property.

The hard part is not the boolean thresholding—it’s simultaneously handling:

1. **child nondeterminism**,
2. **many overlapping child calls**,
3. **the “all calls terminate” nesting semantics**, and
4. **outer Büchi acceptance of the parent**.

That’s why both versions implement an epoch/phase mechanism and do something “Büchi-like” over child-termination obligations.

---

## Old version (OLD_V2) `flatten_MinMax_Inf`: global synchronous product + Cartesian explosion

### 1) State representation (old)

The old “simplified” `flatten_MinMax_Inf` builds a *single* flat automaton state that encodes:

* the parent control state
* for every child state (flattened across all children): flags such as “active” and “tracked”
* an `epoch_nonempty` bit
* a `parent_phase` (the classic 0/1/2: active / waiting / final-pulse style)

You can see this in the `simple_work_item` + `encode_simple_state` machinery in the old file. 

### 2) Transition generation (old): explicit product over all active children

At each parent step and each symbol, it:

1. collects, for every **currently active child-token location**, the set of possible child moves on that symbol;
2. enumerates the **Cartesian product** of those move sets; and
3. for each combination, emits a successor flat state + a 0/1 weight.

The “generate all combinations” step is explicit and is the core scalability killer. 

So per parent symbol-step, runtime is roughly:

[
\prod_{i \in \text{active tokens}} \deg_i
]

where `deg_i` is the number of child transitions from that token’s child-state on that symbol. With a handful of overlapping calls and nondeterministic children, this becomes hopeless quickly.

### 3) How it encodes weights (old)

The old algorithm *does not guess* at spawn time. Instead it tries to compute “this step is good/bad” globally and emits:

* weight **1** when the chosen global child-move combination keeps everything “okay”
* weight **0** when some active child “fails” (e.g., Max terminates without ever seeing a ≥threshold edge, Min takes a <threshold edge, etc.)

This is described in your comparison doc: old “does not guess, does synchronous product on all active children, and enumerates the Cartesian product every step.” 

This *can* be conceptually elegant for `Inf/LimInf` because `Inf/LimInf` ignore finite-prefix timing differences, so emitting a 0 “when failure is detected” is viable. But the implementation cost is the product blow-up.

### 4) Silent-step handling mismatch (old vs current pipeline)

Old code “charges” silent parent steps with weight 1 (instead of using a dedicated `SILENT` weight) and therefore doesn’t rely on the newer “silent-transition removal” stage. Your comparison doc calls out this cross-cutting difference. 

Given your current `isNonEmpty` pipeline removes silent transitions for monotone-threshold backends, new flatteners really want to emit `SILENT` consistently.

### 5) Correctness hazards: token overlap / merging

The old Min/Max–Inf approach also has a known correctness hazard when multiple child calls “merge” into the same abstracted child-state bucket; your current file even carries a special “masked fix” variant to address overlap in a product-style construction. The comparison doc explicitly mentions an overlap bug + the masked fix. 

Even if you repaired overlap, the fundamental product explosion still dominates.

**Bottom line:** the old approach is “clean” in idea but not scalable due to explicit cross products, and it also doesn’t match the current silent-edge conventions.

---

## Current version `flatten_MinMax_Inf`: generic threshold-obligation backend (correct + scalable, but not specialized)

Your *current live* `flatten_MinMax_Inf` just forwards to the generic construction:

```cpp
return flatten_threshold_extremal_impl(this, finite_aggregator, threshold);
```



That generic backend:

* emits 0/1 weights by **guessing** (0 or 1) on each non-silent call edge,
* spawns a **threshold obligation** whose job is to validate that guess by tracking a frontier of possible child configurations until termination,
* maintains two bags `P1` (untracked) and `P2` (tracked) plus the epoch/phase bits, and
* uses “tracked must discharge” style Büchi acceptance (final pulse + parent-final + epoch_nonempty).

You can see the same skeleton in the generic backend snippet around spawning obligations and updating `P1/P2` and phases. 

### Why this is fundamentally more scalable than the old code

The key difference is: it **does not enumerate the product** of all child nondeterministic choices.
Each child call’s nondeterminism is handled locally by maintaining a **set (frontier)** of possible child states, i.e., subset-construction style. So cost is closer to:

[
\sum_{i \in \text{active obligations}} (\text{size of frontier}_i \times \text{outdegree})
]

instead of the Cartesian product across obligations.

### What’s still suboptimal for `{Min,Max}×{Inf,LimInf}`

For Min/Max thresholds, child progress is just a **1-bit** (`y ∈ {0,1}`), but the generic backend still uses a general-purpose “frontier as a vector of (state,progress)” and then repeatedly canonicalizes/sorts/merges it.

So it’s correct and already far better than the old product, but there’s performance headroom.

---

## Best replacement for your objectives: make the specialized Min/Max threshold-obligation construction the live one (and optimize it)

You already have (in `NestedAutomaton.cpp`) a *specialized* version for exactly this case:

* `MMThrFrontier` = two bitsets (`y0` and `y1`) instead of a vector of configurations
* a specialized liveness precompute `build_mmthr_live`
* specialized obligation stepping `step_mmthr_obl_bag`
* specialized spawns `spawn_mmthr_obligation`
* and the full construction `flatten_MinMax_Inf_threshold_obl`

This is the right direction for **efficiency + scalability**, because:

* bitsets are dramatically cheaper than sorting vectors when frontiers get large,
* equality/ordering on frontiers becomes “memcmp-like” on words,
* and liveness filtering stays fast.

### But: as written, it still has two big performance problems

1. **`step_mmthr_obl_bag` inserts obligations one-by-one into a sorted vector** (`mmthr_bag_add`) which is `O(m^2)` in number of obligations in the worst case.
2. `flatten_MinMax_Inf_threshold_obl` uses an ordered `MapStd` keyed by a big struct and also copies whole `BuchiState_mmthr` objects into the worklist. 

So the best replacement is:

* **Switch `flatten_MinMax_Inf` to use the MMThr specialized construction**, *not* the generic backend.
* **Fix the O(m²) bag build in `step_mmthr_obl_bag`** by collecting then `sort+unique`.
* **Use `std::unordered_map` for the state map** with a custom hash, and store *pointers* to keys in the worklist to avoid repeated deep copies.

That gives you:

* correctness aligned with the current silent-transition removal pipeline (uses `SILENT` edges)
* scalable avoidance of Cartesian product
* specialized Min/Max bitset speedups
* much better asymptotics in “many active calls” regimes

---

# Replacement code (drop-in patch)

Below is a concrete patch you can apply. It keeps your existing MMThr machinery, but turns it into a high-performance implementation and makes it the live path for `{Inf,LimInf}×{Min,Max}`.

## 1) Make `flatten_MinMax_Inf` call the specialized MMThr construction

Replace the body of `NestedAutomaton::flatten_MinMax_Inf` (currently returning `flatten_threshold_extremal_impl`) with:

```cpp
Automaton* NestedAutomaton::flatten_MinMax_Inf(value_function_t finite_aggregator,
                                              weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf requires Max_f or Min_f.");
    }

    // Specialized, bitset-based obligation construction for Min/Max thresholds.
    // Faster and smaller than the generic thrext backend for this case.
    return flatten_MinMax_Inf_threshold_obl(finite_aggregator, threshold);
}
```

This flips the live algorithm away from the generic backend.

Also: delete the dead legacy code below the old early return (it’s currently unreachable and just adds compile time + confusion). 

---

## 2) Fix `step_mmthr_obl_bag` to avoid O(m²) bag insertion

Replace your current `step_mmthr_obl_bag` with the version below.

Key changes:

* `out.reserve(in.size())`
* push entries unsorted
* `std::sort + std::unique` once at the end
* keep `mmthr_bag_add` for the “add one spawned obligation” case (cheap)

```cpp
static bool step_mmthr_obl_bag(const MMThrOblBag& in,
                               uint32_t symbol_id,
                               MMThrOblBag& out,
                               bool* any_discharged,
                               const std::vector<ChildTables>& child_tab,
                               const MMThrLive& live,
                               bool finite_is_max,
                               const weight_t& threshold) {
    out.clear();
    if (in.empty()) return true;

    // Worst-case: every obligation survives the step.
    out.reserve(in.size());

    for (const auto& ent : in) {
        const uint32_t cid = ent.key.child;
        if (cid >= child_tab.size()) return false;

        const ChildTables& T = child_tab[cid];
        if (!T.child) return false;
        if (symbol_id >= T.alph) return false;

        MMThrFrontier next;
        mmthr_frontier_zero(next, mmthr_word_count(T.n_states));
        bool discharged = false;

        auto step_one_class = [&](const std::vector<uint64_t>& bits, uint8_t y) {
            mmthr_for_each_set_bit(bits, [&](uint32_t st) {
                if (discharged) return;
                if (st >= T.n_states) return;

                // NOTE: If you trust the invariant “frontiers contain only live states”,
                // you can remove this next check for speed.
                if (!live.is_live(cid, ent.key.guess, st, y)) return;

                const uint32_t cell = T.idx(st, symbol_id);
                const uint32_t b = T.off[static_cast<size_t>(cell)];
                const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                for (uint32_t p = b; p < e; ++p) {
                    const auto& tr = T.edges[static_cast<size_t>(p)];
                    if (tr.to >= T.n_states) continue;

                    const uint8_t y2 = mmthr_y_update(finite_is_max, y, tr.w, threshold);
                    if (T.is_final[tr.to]) {
                        if (y2 == ent.key.guess) {
                            discharged = true;
                            return;
                        }
                        continue;
                    }

                    if (!live.is_live(cid, ent.key.guess, tr.to, y2)) continue;
                    if (y2 == 0u) mmthr_bits_set(next.y0, tr.to);
                    else          mmthr_bits_set(next.y1, tr.to);
                }
            });
        };

        step_one_class(ent.key.fr.y0, 0u);
        if (!discharged) step_one_class(ent.key.fr.y1, 1u);

        if (discharged) {
            if (any_discharged) *any_discharged = true;
            continue;
        }

        mmthr_frontier_canonicalize(next, finite_is_max, ent.key.guess);
        if (mmthr_frontier_empty(next)) {
            // No way to satisfy this obligation after reading symbol_id.
            return false;
        }

        out.push_back(MMThrOblEntry{MMThrOblKey{cid, ent.key.guess, std::move(next)}});
    }

    // Canonicalize bag: sort + unique ONCE (avoids O(m^2) repeated insertion).
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return true;
}
```

This directly addresses the quadratic behavior implied by `mmthr_bag_add` being used repeatedly inside the step loop.

---

## 3) Make `flatten_MinMax_Inf_threshold_obl` use an unordered state map + pointer worklist

Replace the `state_map` / `worklist` portion of `flatten_MinMax_Inf_threshold_obl` with the version below. This keeps the logic identical, but:

* uses `std::unordered_map` (average O(1) lookup vs O(log n) with expensive comparisons),
* avoids pushing *copies* of `BuchiState_mmthr` into the worklist.

Add these helpers near the MMThr code:

```cpp
static inline void mmthr_hash_combine(std::size_t& seed, std::size_t v) noexcept {
    // Standard-ish hash combine
    seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

static inline std::size_t mmthr_hash_u64_vec(const std::vector<uint64_t>& v) noexcept {
    std::size_t h = v.size();
    for (uint64_t x : v) {
        mmthr_hash_combine(h, std::hash<uint64_t>{}(x));
    }
    return h;
}

static inline std::size_t mmthr_hash_frontier(const MMThrFrontier& fr) noexcept {
    std::size_t h = 0;
    mmthr_hash_combine(h, mmthr_hash_u64_vec(fr.y0));
    mmthr_hash_combine(h, mmthr_hash_u64_vec(fr.y1));
    return h;
}

static inline std::size_t mmthr_hash_obl_entry(const MMThrOblEntry& e) noexcept {
    std::size_t h = 0;
    mmthr_hash_combine(h, static_cast<std::size_t>(e.key.child));
    mmthr_hash_combine(h, static_cast<std::size_t>(e.key.guess));
    mmthr_hash_combine(h, mmthr_hash_frontier(e.key.fr));
    return h;
}

static inline std::size_t mmthr_hash_obl_bag(const MMThrOblBag& bag) noexcept {
    std::size_t h = bag.size();
    for (const auto& e : bag) {
        mmthr_hash_combine(h, mmthr_hash_obl_entry(e));
    }
    return h;
}

struct BuchiStateMmthrHash {
    std::size_t operator()(const BuchiState_mmthr& s) const noexcept {
        std::size_t h = 0;
        mmthr_hash_combine(h, std::hash<State*>{}(s.parent_state));
        mmthr_hash_combine(h, static_cast<std::size_t>(s.parent_phase));
        mmthr_hash_combine(h, static_cast<std::size_t>(s.epoch_nonempty ? 1u : 0u));
        mmthr_hash_combine(h, mmthr_hash_obl_bag(s.P1));
        mmthr_hash_combine(h, mmthr_hash_obl_bag(s.P2));
        return h;
    }
};

struct BuchiStateMmthrEq {
    bool operator()(const BuchiState_mmthr& a, const BuchiState_mmthr& b) const noexcept {
        return a == b;
    }
};
```

Then, inside `flatten_MinMax_Inf_threshold_obl`, replace:

```cpp
MapStd<BuchiState_mmthr, State*> state_map;
std::deque<BuchiState_mmthr> worklist;
...
state_map[init] = init_state;
worklist.push_back(init);
...
BuchiState_mmthr current = std::move(worklist.front());
...
State* current_state = state_map[current];
...
if (!state_map.contains(nxt)) { ... state_map[nxt] = ns; worklist.push_back(nxt); }
...
Edge* ne = ... state_map[nxt]
```

with this:

```cpp
std::unordered_map<BuchiState_mmthr, State*, BuchiStateMmthrHash, BuchiStateMmthrEq> state_map;
state_map.reserve(1024);

struct WorkItem {
    const BuchiState_mmthr* key;
    State* st;
};
std::deque<WorkItem> worklist;

unsigned int state_counter = 0;

BuchiState_mmthr init;
init.parent_state = this->getInitial();
init.P1.clear();
init.P2.clear();
init.parent_phase = 1u;
init.epoch_nonempty = false;

std::ostringstream ss;
ss << "b_" << state_counter++;
State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);

// Insert init into map and queue it
{
    auto [it, inserted] = state_map.emplace(init, init_state);
    (void)inserted;
    worklist.push_back(WorkItem{&it->first, it->second});
}

MMThrOblBag P1_step, P2_step;
MMThrOblBag P1_next, P2_next;

while (!worklist.empty()) {
    WorkItem wi = worklist.front();
    worklist.pop_front();

    const BuchiState_mmthr& current = *wi.key;
    State* current_state = wi.st;

    for (unsigned int symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
        if (!step_mmthr_obl_bag(current.P1, symbol_id, P1_step, nullptr,
                                child_tab, live, finite_is_max, threshold)) {
            continue;
        }

        bool tracked_discharged = false;
        if (!current.P2.empty()) {
            if (!step_mmthr_obl_bag(current.P2, symbol_id, P2_step, &tracked_discharged,
                                    child_tab, live, finite_is_max, threshold)) {
                continue;
            }
        } else {
            P2_step.clear();
        }

        SetStd<Edge*>* succs = current.parent_state->getSuccessors(symbol_id);
        if (!succs) continue;

        for (Edge* parent_edge : *succs) {
            State* q_prime = parent_edge->getTo();
            const size_t child_index = edgeWeightToChildIndex(parent_edge->getWeight()->getValue());
            const bool is_silent =
                (child_index >= k) ||
                (this->getChild(child_index) == nullptr) ||
                (this->getChild(child_index)->getStates()->size() < 2);

            unsigned int new_phase = current.parent_phase;
            const bool reset_epoch = (current.parent_phase == 2u);
            if (reset_epoch) new_phase = 0u;

            bool epoch_nonempty = reset_epoch ? false : current.epoch_nonempty;
            if (tracked_discharged) epoch_nonempty = true;
            if (!is_silent) {
                epoch_nonempty = true;
                if (new_phase == 1u) new_phase = 0u;
            }

            const bool current_epoch_complete = current.P2.empty() || P2_step.empty();

            auto get_or_create = [&](BuchiState_mmthr&& nxt) -> State* {
                auto it = state_map.find(nxt);
                if (it != state_map.end()) return it->second;

                std::ostringstream s2;
                s2 << "b_" << state_counter++;
                State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);

                auto [it2, inserted] = state_map.emplace(std::move(nxt), ns);
                (void)inserted;

                worklist.push_back(WorkItem{&it2->first, it2->second});
                return it2->second;
            };

            if (is_silent) {
                if (current_epoch_complete) {
                    P1_next.clear();
                    P2_next = P1_step;
                } else {
                    P1_next = P1_step;
                    P2_next = P2_step;
                }

                BuchiState_mmthr nxt;
                nxt.parent_state = q_prime;
                nxt.P1 = P1_next;
                nxt.P2 = P2_next;
                nxt.parent_phase = (current_epoch_complete && new_phase == 0u) ? 2u : new_phase;
                nxt.epoch_nonempty = epoch_nonempty;

                State* to_state = get_or_create(std::move(nxt));

                Weight* w = weight_register.at(weight_t(SILENT));
                Edge* ne = new Edge(new_alphabet->at(symbol_id), w, current_state, to_state);
                current_state->addSuccessor(ne);
                to_state->addPredecessor(ne);
                continue;
            }

            for (uint8_t guess = 0u; guess <= 1u; ++guess) {
                MMThrOblEntry spawned;
                const MMThrSpawnStatus st = spawn_mmthr_obligation(
                    static_cast<uint32_t>(child_index),
                    static_cast<uint32_t>(symbol_id),
                    guess,
                    spawned,
                    child_tab,
                    live,
                    finite_is_max,
                    threshold
                );
                if (st == MMThrSpawnStatus::REJECT) continue;

                if (current_epoch_complete) {
                    P2_next = P1_step;
                    P1_next.clear();
                } else {
                    P2_next = P2_step;
                    P1_next = P1_step;
                }

                if (st == MMThrSpawnStatus::NONEMPTY) {
                    mmthr_bag_add(P1_next, std::move(spawned));
                }

                BuchiState_mmthr nxt;
                nxt.parent_state = q_prime;
                nxt.P1 = P1_next;
                nxt.P2 = P2_next;
                nxt.parent_phase = (current_epoch_complete && new_phase == 0u) ? 2u : new_phase;
                nxt.epoch_nonempty = epoch_nonempty;

                State* to_state = get_or_create(std::move(nxt));

                Weight* w = weight_register.at(weight_t(static_cast<unsigned int>(guess)));
                Edge* ne = new Edge(new_alphabet->at(symbol_id), w, current_state, to_state);
                current_state->addSuccessor(ne);
                to_state->addPredecessor(ne);
            }
        }
    }
}
```

This preserves your logic (including silent edges using `SILENT`)  but avoids the biggest data-structure bottlenecks.

---

# Why this is the best direction for `{Inf,LimInf}×{Min,Max}`

* It keeps the **obligation decomposition** that avoids the old version’s exponential Cartesian products.
* It uses a Min/Max-specific frontier representation (bitsets) that is strictly more efficient than vector+sort once frontiers grow.
* It aligns with your current “silent removal” pipeline by emitting `SILENT` edges.
* It directly addresses two performance pathologies in the current MMThr code:

  * O(m²) bag building inside `step_mmthr_obl_bag`
  * expensive ordered-map lookups and worklist deep copies in the construction loop 

---

## Two more “if you really want to squeeze it” notes (optional, but high impact)

1. **Cache `ChildTables` across calls**
   If you call `flatten_MinMax_Inf` repeatedly for different thresholds `x`, you are rebuilding CSR tables each time. `ChildTables` are threshold-independent; only the liveness masks depend on `threshold`. Caching CSR tables in the `NestedAutomaton` object typically gives a noticeable speedup.

2. **Arena / pooling for `MMThrFrontier` bitsets**
   Each frontier owns two `std::vector<uint64_t>`. If your search explores many states, allocation churn can dominate. A simple bump allocator or `std::pmr::monotonic_buffer_resource` for those vectors (or an explicit pool of word-arrays) can improve scalability further. This is more invasive, but it’s the next big lever after the patch above.

---

If you want, next we can sanity-check the *correctness* of the MMThr liveness computation for Max vs Min (it’s subtle but crucial), and then move on to `{Inf,LimInf}×{SumPlus,SumMinus}` where the frontier and dominance behavior changes a lot.
