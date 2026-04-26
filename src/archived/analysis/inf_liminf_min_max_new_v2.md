You’re seeing exactly what the code structure suggests you *would* see on this benchmark family:

* The old “simple” Min/Max×Inf/LimInf flattener is fundamentally a **global Cartesian-product construction** over all active child tokens (and it’s also the one that needed the masked fix to avoid merging two tokens that land in the same child state). In the old V2 code you can literally see the “generate all combinations” loop that multiplies move-sets across active children . This is the main scalability killer (time *and* memory), plus it builds states via a `Parser`+string encoding pipeline in that older style (high constant factors).
* The current live code doesn’t use that anymore: `flatten_MinMax_Inf` immediately dispatches into the **shared threshold-extremal obligation backend** (`flatten_threshold_extremal_impl`) and everything below it in that function is dead/unreachable legacy . That backend is explicitly documented in-file as the unified obligation construction for the monotone/extremal cases  and implements the familiar **P1/P2 breakpoint discipline + final-pulse** while stepping and spawning obligations .

Your benchmark result (“same language/booleans/sizes; only constant factors differ; current slightly faster on larger n,k≤3”) is consistent with the fact that the “threshold_obl” bitset version pays for full-width bitsets and clearing/allocating them frequently, while the current shared backend uses sparse frontiers (vectors) and is often faster when frontiers aren’t dense.

---

## What I would keep vs. throw away for {Inf,LimInf}×{Min_f,Max_f}

### Throw away (as production code)

1. **Old V2 “simple” Cartesian-product implementation**
   It enumerates the product of per-child move sets (“Generate all combinations…”) . That scales like ∏ᵢ |movesᵢ| per symbol-step and is the wrong asymptotic shape for real instances.
2. **The masked fix version as a *default***
   It fixes the “merge two tokens in same child state” bug by tracking a 4-way class per state; that’s correct, but it’s still the “global configuration vector” style and retains the same basic product explosion and high per-state memory footprint (vector over all child states). It’s fine as a regression reference, not as the main backend.

### Keep (conceptually) and improve (implementation)

3. **The current shared `flatten_threshold_extremal_impl` approach**
   It’s the right high-level construction: obligations avoid Cartesian products by keeping each call independent in a bag, stepping each obligation locally, and using the breakpoint P1/P2 scheme with final pulse .
   It also integrates silent-parent handling via a distinguished `SILENT` weight (later removed), which is both correct and convenient .

---

## Where the current backend is still “wasting” work for this specific case

Even though the shared backend is the right algorithmic idea, for Min/Max×Inf/LimInf it still pays some avoidable constant factors:

1. **Repeated deep copying of obligation bags per parent edge / guess**
   In the inner loop it assigns `P2_next = P1_step` / `P1_next = P1_step` repeatedly and mutates vectors (clear + insert) . That means lots of vector copies of bags (and those bags contain vectors of frontiers).
2. **State-map keys are “deep”**
   `ThrExtBuchiState` keys include whole `P1` and `P2` vectors of obligations, and each obligation key includes a frontier vector . If `MapStd` is an ordered map (very likely, given `operator<` is defined and `.contains` is used), comparison costs can get expensive as states scale.
3. **Stepping the same obligation/bag many times**
   If two different parent states share the same obligation bag (very common), you still recompute `thrext_step_obl_bag` again and again .

These are exactly the kinds of constant-factor issues that show up as “the two backends match semantically; only runtime/memory constants differ” in your benchmark notes.

---

## Best replacement code I’d ship for this case

### Design goal

Keep the **same semantics** as the current shared obligation construction, but eliminate the largest constant factors by:

* **Interning** obligations and bags into small integer IDs (structural sharing).
* **Memoizing**:

  * spawn results per (child, guess, symbol),
  * step results per (obligation-id, symbol),
  * and (optionally) per (bag-id, symbol).
* Using a **compact state key** `(parent_state_id, P1_bag_id, P2_bag_id, phase, epoch_nonempty)` in an `unordered_map`, instead of deep vector keys.

This is still the “threshold-obligation” construction, but made *fast*.

### Drop-in code

Below is a self-contained replacement for `NestedAutomaton::flatten_MinMax_Inf(...)` that you can paste into `NestedAutomaton.cpp`. It does **not** change any public APIs, and it keeps the same 0/1/SILENT encoding so your existing harness + silent removal should continue to work.

**Placement:** put the `namespace { ... }` helper block **above** `NestedAutomaton::flatten_MinMax_Inf` (after `ChildTables`, `build_child_tables`, and `ThrExtChildInfo` are available). Then replace the body of `flatten_MinMax_Inf` with the short wrapper at the end.

```cpp
// ============================================================================
// Fast cached/interned MinMax × Inf/LimInf threshold-obligation flattener
//
// Key idea: same P1/P2 breakpoint + final-pulse discipline as the shared ThrExt
// backend, but represent obligations/bags by interned IDs and memoize stepping.
// This avoids repeated deep copies of bags/frontiers across parent edges.
// ============================================================================

namespace {

static inline uint64_t mm_mix64(uint64_t x) {
    // splitmix64-style finalizer
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

static inline void mm_hash_combine(uint64_t& h, uint64_t v) {
    h ^= mm_mix64(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
}

static inline void mm_sort_unique(std::vector<uint32_t>& v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
}

// Remove overlaps according to the same preference rule used by the ThrExt
// frontier canonicalizer for MIN_F/MAX_F: keep prog=1 if guess==1 else keep prog=0.
static inline void mm_canonicalize_sets(uint8_t guess,
                                        std::vector<uint32_t>& y0,
                                        std::vector<uint32_t>& y1) {
    mm_sort_unique(y0);
    mm_sort_unique(y1);

    if (y0.empty() || y1.empty()) return;

    if (guess == 1u) {
        // remove y0 ∩ y1
        std::vector<uint32_t> out;
        out.reserve(y0.size());
        size_t i = 0, j = 0;
        while (i < y0.size()) {
            if (j >= y1.size() || y0[i] < y1[j]) {
                out.push_back(y0[i++]);
            } else if (y0[i] == y1[j]) {
                ++i; ++j;
            } else {
                ++j;
            }
        }
        y0.swap(out);
    } else {
        // guess==0: remove y1 ∩ y0
        std::vector<uint32_t> out;
        out.reserve(y1.size());
        size_t i = 0, j = 0;
        while (i < y1.size()) {
            if (j >= y0.size() || y1[i] < y0[j]) {
                out.push_back(y1[i++]);
            } else if (y1[i] == y0[j]) {
                ++i; ++j;
            } else {
                ++j;
            }
        }
        y1.swap(out);
    }
}

struct MMInfBuilder {
    using OblId = uint32_t;
    using BagId = uint32_t;

    // Sentinels for obligation-step/spawn caches (OblId-coded).
    static constexpr OblId OBL_UNKNOWN    = 0xFFFFFFFFu;
    static constexpr OblId OBL_DEAD       = 0xFFFFFFFEu; // no viable continuation
    static constexpr OblId OBL_DISCHARGED = 0xFFFFFFFDu; // can terminate now

    // Sentinels for bag-step caches (BagId-coded).
    static constexpr BagId BAG_UNKNOWN = 0xFFFFFFFFu;
    static constexpr BagId BAG_DEAD    = 0xFFFFFFFEu;

    NestedAutomaton* A = nullptr;
    const bool finite_is_max;
    const weight_t threshold;

    const uint32_t alph_size;
    const uint32_t k;

    std::vector<ChildTables> child_tab;
    std::vector<ThrExtChildInfo> child_info;

    struct Obl {
        uint32_t child = 0;
        uint8_t  guess = 0;
        std::vector<uint32_t> y0; // states with prog=0
        std::vector<uint32_t> y1; // states with prog=1
        uint64_t hash = 0;
        // Per symbol: OBL_UNKNOWN / OBL_DEAD / OBL_DISCHARGED / new OblId
        std::vector<OblId> step_cache;
    };

    struct Bag {
        std::vector<OblId> obls; // sorted unique
        uint64_t hash = 0;
        // Per symbol: BAG_UNKNOWN / BAG_DEAD / next BagId
        std::vector<BagId> step_next;
        // Per symbol: whether *any* obligation discharged on that symbol step
        std::vector<uint8_t> step_any_discharged;
    };

    // Interned objects
    std::vector<Obl> obls;
    std::unordered_map<uint64_t, std::vector<OblId>> obl_buckets;

    std::vector<Bag> bags;
    std::unordered_map<uint64_t, std::vector<BagId>> bag_buckets;

    // Cache bag + single obligation -> bag result (big win because spawns repeat)
    std::unordered_map<uint64_t, BagId> bag_add_cache;

    // Precomputed spawn table:
    // index = (child*2 + guess)*alph_size + sym
    // value = OBL_DEAD (reject), OBL_DISCHARGED (empty), or OblId (nonempty)
    std::vector<OblId> spawn;

    explicit MMInfBuilder(NestedAutomaton* A_,
                          bool finite_is_max_,
                          const weight_t& threshold_)
        : A(A_)
        , finite_is_max(finite_is_max_)
        , threshold(threshold_)
        , alph_size(static_cast<uint32_t>(A_->getAlphabetSize()))
        , k(static_cast<uint32_t>(A_->getChildrenSize()))
        , child_tab(k)
        , child_info(k)
        , spawn(static_cast<size_t>(k) * 2u * static_cast<size_t>(alph_size), OBL_DEAD) {

        // Build child tables + mm_live (reuse ThrExtChildInfo builder for correctness).
        const value_function_t finVal = finite_is_max ? Max_f : Min_f;
        for (uint32_t i = 0; i < k; ++i) {
            ChildAutomaton* c = A->getChild(i);
            if (!c) continue;
            if (c->getStates()->size() < 2) continue;
            build_child_tables(c, child_tab[i]);
            thrext_build_child_info(child_tab[i], finVal, threshold, /*weight_scale=*/1u, child_info[i]);
        }

        // Bag 0: empty bag (step is always empty and never discharges).
        bags.push_back(Bag{});
        bags[0].hash = 0;
        bags[0].step_next.assign(alph_size, 0u);
        bags[0].step_any_discharged.assign(alph_size, 0u);

        // Precompute all spawn results once.
        precompute_spawns();
    }

    inline bool child_enabled(uint32_t child) const {
        return child < k && child_tab[child].child && child_info[child].enabled;
    }

    inline uint8_t init_y() const { return finite_is_max ? 0u : 1u; }

    inline uint8_t step_y(uint8_t y, const weight_t& edge_w) const {
        const bool high = !(edge_w < threshold);
        if (finite_is_max) {
            return static_cast<uint8_t>((y != 0u || high) ? 1u : 0u);
        }
        return static_cast<uint8_t>((y != 0u && high) ? 1u : 0u);
    }

    inline bool is_live(uint32_t child, uint8_t guess, uint32_t st, uint8_t y) const {
        if (child >= k) return false;
        if (guess > 1u || y > 1u) return false;

        const ChildTables& T = child_tab[child];
        const ThrExtChildInfo& info = child_info[child];
        if (!T.child || !info.enabled) return false;
        if (st >= T.n_states) return false;

        // Global reachability-to-final prune from ChildTables::live.
        if (!T.live.empty() && !T.live[st]) return false;

        // For min/max modes, mm_live[guess][y][st] is the target-aware liveness.
        const auto& v = info.mm_live[guess][y];
        if (v.empty()) return false;
        return v[st] != 0u;
    }

    // -------- Interning: obligations --------

    OblId intern_obl(uint32_t child,
                     uint8_t guess,
                     std::vector<uint32_t>&& y0,
                     std::vector<uint32_t>&& y1) {
        // Canonicalize (sorted unique + overlap removal)
        mm_canonicalize_sets(guess, y0, y1);
        if (y0.empty() && y1.empty()) return OBL_DEAD;

        uint64_t h = 0;
        mm_hash_combine(h, child);
        mm_hash_combine(h, guess);
        mm_hash_combine(h, static_cast<uint64_t>(y0.size()));
        for (uint32_t x : y0) mm_hash_combine(h, x);
        mm_hash_combine(h, static_cast<uint64_t>(y1.size()));
        for (uint32_t x : y1) mm_hash_combine(h, x);

        auto& bucket = obl_buckets[h];
        for (OblId id : bucket) {
            const Obl& O = obls[id];
            if (O.child == child && O.guess == guess && O.y0 == y0 && O.y1 == y1) {
                return id;
            }
        }

        const OblId new_id = static_cast<OblId>(obls.size());
        Obl O;
        O.child = child;
        O.guess = guess;
        O.y0 = std::move(y0);
        O.y1 = std::move(y1);
        O.hash = h;
        O.step_cache.assign(alph_size, OBL_UNKNOWN);
        obls.push_back(std::move(O));
        bucket.push_back(new_id);
        return new_id;
    }

    // -------- Interning: bags --------

    BagId intern_bag(std::vector<OblId>&& ids) {
        if (ids.empty()) return 0u;

        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

        uint64_t h = 0;
        mm_hash_combine(h, static_cast<uint64_t>(ids.size()));
        for (OblId id : ids) mm_hash_combine(h, id);

        auto& bucket = bag_buckets[h];
        for (BagId bid : bucket) {
            const Bag& B = bags[bid];
            if (B.obls == ids) return bid;
        }

        const BagId new_id = static_cast<BagId>(bags.size());
        Bag B;
        B.obls = std::move(ids);
        B.hash = h;
        B.step_next.assign(alph_size, BAG_UNKNOWN);
        B.step_any_discharged.assign(alph_size, 0u);
        bags.push_back(std::move(B));
        bucket.push_back(new_id);
        return new_id;
    }

    BagId bag_add_obl(BagId base, OblId add) {
        if (add == OBL_DEAD || add == OBL_DISCHARGED || add == OBL_UNKNOWN) return base;
        if (base >= bags.size()) return base;

        const uint64_t key = (static_cast<uint64_t>(base) << 32) | static_cast<uint64_t>(add);
        auto it = bag_add_cache.find(key);
        if (it != bag_add_cache.end()) return it->second;

        const std::vector<OblId>& v = bags[base].obls;
        if (std::binary_search(v.begin(), v.end(), add)) {
            bag_add_cache[key] = base;
            return base;
        }

        std::vector<OblId> out;
        out.reserve(v.size() + 1u);

        bool inserted = false;
        for (OblId x : v) {
            if (!inserted && add < x) {
                out.push_back(add);
                inserted = true;
            }
            out.push_back(x);
        }
        if (!inserted) out.push_back(add);

        const BagId res = intern_bag(std::move(out));
        bag_add_cache[key] = res;
        return res;
    }

    // -------- Spawn cache --------

    inline size_t spawn_index(uint32_t child, uint8_t guess, uint32_t sym) const {
        return (static_cast<size_t>(child) * 2u + static_cast<size_t>(guess)) * static_cast<size_t>(alph_size)
             + static_cast<size_t>(sym);
    }

    inline OblId spawn_code(uint32_t child, uint8_t guess, uint32_t sym) const {
        return spawn[spawn_index(child, guess, sym)];
    }

    void precompute_spawns() {
        for (uint32_t child = 0; child < k; ++child) {
            if (!child_enabled(child)) continue;

            const ChildTables& T = child_tab[child];
            if (T.init >= T.n_states) continue;

            for (uint8_t guess = 0; guess <= 1u; ++guess) {
                for (uint32_t sym = 0; sym < alph_size; ++sym) {
                    // default is OBL_DEAD
                    if (sym >= T.alph) {
                        spawn[spawn_index(child, guess, sym)] = OBL_DEAD;
                        continue;
                    }

                    std::vector<uint32_t> next0, next1;
                    const uint8_t y0 = init_y();

                    const uint32_t cell = T.idx(T.init, sym);
                    const uint32_t b = T.off[static_cast<size_t>(cell)];
                    const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                    for (uint32_t pos = b; pos < e; ++pos) {
                        const auto& tr = T.edges[static_cast<size_t>(pos)];
                        if (tr.to >= T.n_states) continue;

                        const uint8_t y2 = step_y(y0, tr.w);

                        if (T.is_final[tr.to]) {
                            if (y2 == guess) {
                                // can discharge immediately
                                spawn[spawn_index(child, guess, sym)] = OBL_DISCHARGED;
                                goto spawn_done;
                            }
                            continue;
                        }

                        if (!is_live(child, guess, tr.to, y2)) continue;
                        (y2 ? next1 : next0).push_back(tr.to);
                    }

                    mm_canonicalize_sets(guess, next0, next1);
                    if (next0.empty() && next1.empty()) {
                        spawn[spawn_index(child, guess, sym)] = OBL_DEAD;
                    } else {
                        spawn[spawn_index(child, guess, sym)] = intern_obl(child, guess, std::move(next0), std::move(next1));
                    }

                spawn_done:
                    ;
                }
            }
        }
    }

    // -------- Obligation stepping (memoized per obligation+symbol) --------

    OblId step_obl(OblId id, uint32_t sym) {
        if (id >= obls.size()) return OBL_DEAD;
        Obl& O = obls[id];

        if (sym >= alph_size) return OBL_DEAD;

        const OblId cached = O.step_cache[sym];
        if (cached != OBL_UNKNOWN) return cached;

        const uint32_t child = O.child;
        const uint8_t guess = O.guess;

        if (!child_enabled(child)) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        const ChildTables& T = child_tab[child];
        if (sym >= T.alph) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        std::vector<uint32_t> next0, next1;
        bool discharged = false;

        auto process = [&](const std::vector<uint32_t>& src, uint8_t y) {
            for (uint32_t st : src) {
                if (!is_live(child, guess, st, y)) continue;

                const uint32_t cell = T.idx(st, sym);
                const uint32_t b = T.off[static_cast<size_t>(cell)];
                const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                for (uint32_t pos = b; pos < e; ++pos) {
                    const auto& tr = T.edges[static_cast<size_t>(pos)];
                    if (tr.to >= T.n_states) continue;

                    const uint8_t y2 = step_y(y, tr.w);

                    if (T.is_final[tr.to]) {
                        if (y2 == guess) {
                            discharged = true;
                            return;
                        }
                        continue;
                    }

                    if (!is_live(child, guess, tr.to, y2)) continue;
                    (y2 ? next1 : next0).push_back(tr.to);
                }
                if (discharged) return;
            }
        };

        process(O.y0, 0u);
        if (!discharged) process(O.y1, 1u);

        if (discharged) {
            O.step_cache[sym] = OBL_DISCHARGED;
            return OBL_DISCHARGED;
        }

        mm_canonicalize_sets(guess, next0, next1);
        if (next0.empty() && next1.empty()) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        const OblId nid = intern_obl(child, guess, std::move(next0), std::move(next1));
        O.step_cache[sym] = nid;
        return nid;
    }

    // -------- Bag stepping (memoized per bag+symbol) --------

    struct BagStep {
        bool ok = true;
        BagId next = 0;
        bool any_discharged = false;
    };

    BagStep step_bag(BagId bid, uint32_t sym) {
        if (bid == 0u) return BagStep{true, 0u, false};
        if (bid >= bags.size()) return BagStep{false, 0u, false};

        Bag& B = bags[bid];
        const BagId cached = B.step_next[sym];
        if (cached != BAG_UNKNOWN) {
            if (cached == BAG_DEAD) return BagStep{false, 0u, false};
            return BagStep{true, cached, B.step_any_discharged[sym] != 0u};
        }

        std::vector<OblId> out;
        out.reserve(B.obls.size());
        bool any_d = false;

        for (OblId oid : B.obls) {
            const OblId r = step_obl(oid, sym);
            if (r == OBL_DEAD) {
                B.step_next[sym] = BAG_DEAD;
                B.step_any_discharged[sym] = 0u;
                return BagStep{false, 0u, false};
            }
            if (r == OBL_DISCHARGED) {
                any_d = true;
                continue;
            }
            out.push_back(r);
        }

        const BagId nb = intern_bag(std::move(out));
        B.step_next[sym] = nb;
        B.step_any_discharged[sym] = any_d ? 1u : 0u;
        return BagStep{true, nb, any_d};
    }
};

static Automaton* flatten_MinMax_Inf_cached_impl(NestedAutomaton* A,
                                                 value_function_t finite_aggregator,
                                                 weight_t threshold) {
    const bool finite_is_max = (finite_aggregator == Max_f);
    if (!finite_is_max && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf requires Max_f or Min_f");
    }

    // Standard reset pattern used by other flatteners.
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    // Build caches (child tables + mm_live + spawn/step memoization).
    MMInfBuilder B(A, finite_is_max, threshold);

    // Copy alphabet
    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(A->getAlphabetSize());
    for (size_t i = 0; i < A->getAlphabetSize(); ++i) {
        Symbol* original = A->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    // Weights: {0, 1, SILENT}
    MapStd<weight_t, Weight*> weight_register;
    MapArray<Weight*>* new_weights = new MapArray<Weight*>(3);

    auto get_weight = [&](const weight_t& value) -> Weight* {
        if (!weight_register.contains(value)) {
            Weight* w = new Weight(value);
            new_weights->insert(w->getId(), w);
            weight_register.insert(value, w);
        }
        return weight_register.at(value);
    };

    get_weight(weight_t(0));
    get_weight(weight_t(1));
    get_weight(weight_t(SILENT));

    const weight_t global_min = weight_t(0);
    const weight_t global_max = weight_t(1);

    // Compact Buchi state key
    struct Key {
        uint32_t parent = 0;
        MMInfBuilder::BagId P1 = 0;
        MMInfBuilder::BagId P2 = 0;
        uint8_t phase = 1u;          // 0 = active, 1 = waiting, 2 = final pulse
        uint8_t epoch_nonempty = 0u; // boolean
        bool operator==(const Key& o) const {
            return parent == o.parent && P1 == o.P1 && P2 == o.P2 &&
                   phase == o.phase && epoch_nonempty == o.epoch_nonempty;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& k) const noexcept {
            uint64_t h = 0;
            mm_hash_combine(h, k.parent);
            mm_hash_combine(h, k.P1);
            mm_hash_combine(h, k.P2);
            mm_hash_combine(h, k.phase);
            mm_hash_combine(h, k.epoch_nonempty);
            return static_cast<size_t>(mm_mix64(h));
        }
    };

    std::unordered_map<Key, State*, KeyHash> state_map;
    state_map.reserve(4096);

    std::deque<Key> worklist;
    unsigned int state_counter = 0;

    Key init;
    init.parent = static_cast<uint32_t>(A->getInitial()->getId());
    init.P1 = 0u;
    init.P2 = 0u;
    init.phase = 1u;
    init.epoch_nonempty = 0u;

    std::ostringstream ss;
    ss << "bmm_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
    state_map.emplace(init, init_state);
    worklist.push_back(init);

    while (!worklist.empty()) {
        const Key current = worklist.front();
        worklist.pop_front();

        State* current_state = state_map.find(current)->second;
        State* parent_state = A->getStates()->at(current.parent);

        for (uint32_t symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            const auto P1res = B.step_bag(current.P1, symbol_id);
            if (!P1res.ok) continue;

            bool tracked_discharged = false;
            MMInfBuilder::BagId P2_step = 0u;
            if (current.P2 != 0u) {
                const auto P2res = B.step_bag(current.P2, symbol_id);
                if (!P2res.ok) continue;
                P2_step = P2res.next;
                tracked_discharged = P2res.any_discharged;
            }

            SetStd<Edge*>* succs = parent_state->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* parent_edge : *succs) {
                const uint32_t q_prime = static_cast<uint32_t>(parent_edge->getTo()->getId());
                const size_t child_index_sz = edgeWeightToChildIndex(parent_edge->getWeight()->getValue());
                const uint32_t child_index = static_cast<uint32_t>(child_index_sz);

                const bool is_silent =
                    (child_index >= B.k) ||
                    !B.child_enabled(child_index);

                uint8_t new_phase = current.phase;
                const bool reset_epoch = (current.phase == 2u);
                if (reset_epoch) new_phase = 0u;

                bool epoch_nonempty = reset_epoch ? false : (current.epoch_nonempty != 0u);
                if (tracked_discharged) epoch_nonempty = true;
                if (!is_silent) {
                    epoch_nonempty = true;
                    if (new_phase == 1u) new_phase = 0u;
                }

                const bool epoch_complete = (current.P2 == 0u) || (P2_step == 0u);

                if (is_silent) {
                    const MMInfBuilder::BagId P2_next = epoch_complete ? P1res.next : P2_step;
                    const MMInfBuilder::BagId P1_next = epoch_complete ? 0u : P1res.next;
                    const uint8_t phase_next = (epoch_complete && new_phase == 0u) ? 2u : new_phase;

                    Key nxt{q_prime, P1_next, P2_next, phase_next, static_cast<uint8_t>(epoch_nonempty ? 1u : 0u)};

                    auto it = state_map.find(nxt);
                    if (it == state_map.end()) {
                        std::ostringstream s2;
                        s2 << "bmm_" << state_counter++;
                        State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                        state_map.emplace(nxt, ns);
                        worklist.push_back(nxt);
                        it = state_map.find(nxt);
                    }

                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(SILENT)),
                                        current_state,
                                        it->second);
                    current_state->addSuccessor(ne);
                    it->second->addPredecessor(ne);
                    continue;
                }

                // Non-silent: two guesses 0/1
                for (uint8_t guess = 0u; guess <= 1u; ++guess) {
                    const MMInfBuilder::OblId sc = B.spawn_code(child_index, guess, symbol_id);
                    if (sc == MMInfBuilder::OBL_DEAD) continue; // REJECT

                    const MMInfBuilder::BagId P2_next = epoch_complete ? P1res.next : P2_step;
                    const MMInfBuilder::BagId baseP1  = epoch_complete ? 0u : P1res.next;

                    MMInfBuilder::BagId P1_next = baseP1;
                    if (sc != MMInfBuilder::OBL_DISCHARGED) {
                        P1_next = B.bag_add_obl(baseP1, sc);
                    }

                    const uint8_t phase_next = (epoch_complete && new_phase == 0u) ? 2u : new_phase;

                    Key nxt{q_prime, P1_next, P2_next, phase_next, static_cast<uint8_t>(epoch_nonempty ? 1u : 0u)};

                    auto it = state_map.find(nxt);
                    if (it == state_map.end()) {
                        std::ostringstream s2;
                        s2 << "bmm_" << state_counter++;
                        State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                        state_map.emplace(nxt, ns);
                        worklist.push_back(nxt);
                        it = state_map.find(nxt);
                    }

                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(static_cast<unsigned int>(guess))),
                                        current_state,
                                        it->second);
                    current_state->addSuccessor(ne);
                    it->second->addPredecessor(ne);
                }
            }
        }
    }

    // Build automaton + acceptance (same as ThrExt final condition: final-pulse & nonempty epoch & parent final)
    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& kv : state_map) {
        const Key& kkey = kv.first;
        State* st = kv.second;
        new_states->insert(st->getId(), st);

        if (kkey.phase == 2u && kkey.epoch_nonempty != 0u && A->getStates()->at(kkey.parent)->getFinal()) {
            st->setFinal(true);
        }
    }

    const std::string name = "BuchiMMThrCached(" + A->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights, global_min, global_max, init_state);
}

} // namespace

// ============================================================================
// Replacement: Min/Max with outer Inf/LimInf
// ============================================================================
Automaton* NestedAutomaton::flatten_MinMax_Inf(value_function_t finite_aggregator,
                                               weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf requires Max_f or Min_f");
    }

    // Use the cached/interned backend instead of the generic ThrExt one.
    // (Same semantics, lower constant factors for Min/Max.)
    return flatten_MinMax_Inf_cached_impl(this, finite_aggregator, threshold);
}
```

---

## Why this is the “best” replacement for this case

* It **keeps the good idea** (obligation construction, no Cartesian products) that the current code already uses .
* It **directly targets the remaining hot spots** you’re measuring as “constant factor only”:

  * avoids repeated `P1_next = P1_step` / `P2_next = P1_step` deep copies in the innermost loops  by representing P1/P2 as bag IDs.
  * makes state-map keys compact (IDs), instead of “deep vectors in the key” .
  * memoizes spawn/step so repeated obligations/bags aren’t recomputed.
* It preserves the same **SILENT-edge protocol** (so your existing `flatten + silent-removal + emptiness check` pipeline remains valid) .

---

## What I would do with the existing backends after this

* Keep `flatten_threshold_extremal_impl` as the general backend (it’s already the default for minmax), but *for Min/Max specifically* I’d prefer this cached version as the default because it attacks the biggest remaining cost center.
* Keep `flatten_MinMax_Inf_threshold_obl` (the bitset-based “threshold_obl” you benchmarked) as a **debug/regression backend**. It’s conceptually nice, but its full-width bitset handling is exactly why it can lose on sparse frontiers and why you’re seeing only “competitive, sometimes faster on small cases” behavior.
* Delete or `#if 0` the dead legacy code in `flatten_MinMax_Inf` (everything after the early return) because it’s currently unreachable anyway .

If you want, I can apply the same “interned + memoized IDs” idea to the shared `flatten_threshold_extremal_impl` next (so SumPlus/SumMinus benefit too), but for your stated scope this replacement is the most direct way to improve Min/Max×Inf/LimInf without reopening the whole shared backend.



---
---
---

## What we’re deciding for `{Inf, LimInf} × {Min_f, Max_f}`

This case is a **threshold** case in your codebase:

* You build a **0/1-weighted** flattened Büchi automaton.
* Then `isNonEmpty_withFinal(infVal, 1)` decides the threshold query for `infVal ∈ {Inf, LimInf}` (because on `{0,1}`, `Inf ≥ 1` means “no 0 ever”, and `LimInf ≥ 1` means “only finitely many 0”).
  The flattening’s job is to ensure each non-silent parent step emits **weight = 1 iff that invoked child call can terminate with value ≥ threshold**, else 0.

In *current* `NestedAutomaton.cpp`, the function that corresponds to this whole case is exactly:

```cpp
Automaton* NestedAutomaton::flatten_MinMax_Inf(value_function_t finite_aggregator,
                                               weight_t threshold)
```

and right now it **just calls the shared threshold backend** and returns immediately (the “simplified” old code below is dead/unreachable) .

You asked for:

1. maximal efficiency/scalability (primary),
2. elegance (secondary),
   and you explicitly asked me to **check correctness in detail**.

So I’m going to:

* dissect the **OLD_V2** algorithmic idea and why it’s not scalable,
* dissect the **current shared threshold-obligation backend** and what it does for this case,
* then give you a **best replacement** for this specific function (with detailed correctness argument).

---

## 1) OLD_V2’s `{Inf,LimInf}×{Min,Max}` flattener: what it does and why it doesn’t scale

### 1.1 State shape and core “global synchronous” idea

`OLD_V2::flatten_MinMax_Inf` is described (and implemented) as a **dedicated simplified** algorithm based on the `Inf`-style observation:

* “no need to guess a witness”
* “all relevant active children must succeed”
* it maintains a *direct* global synchronous representation:

  * parent control + phase
  * per-child-state status
  * per-child-state tracked bit
  * `epoch_nonempty` 

The code makes that concrete with a flattened state record like:

* `parent_state_id`
* `parent_phase` (0 active, 1 waiting, 2 final_pulse)
* `child_status[flattened_child_state]`

  * Max: inactive / active+low / active+high (via bits: active, seen_high)
  * Min: inactive / active
* `tracked[flattened_child_state]`
* `epoch_nonempty` 

So far, that matches the doc’s Case 4 summary .

### 1.2 The expensive step: cartesian product over all active child transitions

The killer is step (3) in the md doc:

> collect moves for every active child
> enumerate the Cartesian product
> emit weight 1 iff whole combination succeeds 

That is literally what the code does:

* it collects `all_moves`, one vector per active child token,
* then materializes the cartesian product into `combinations`,
* then loops every `combo` and creates one successor state per combo. 

This is **fundamentally non-scalable**:

* if you have `m` active child tokens, each with branching `bᵢ`, you generate `∏ bᵢ` combinations *per parent edge per symbol*.
* even small nondeterminism gives you an explosive blowup.

### 1.3 Correctness intuition (why it “can” work conceptually)

It can still be *conceptually* correct for threshold checking under Inf/LimInf because for these outer objectives you only care about whether failures happen never (Inf) or infinitely often (LimInf). A “whole-step” bit (“did any active child fail at this step?”) can detect failures when they *become inevitable* (e.g., a max-call terminates without ever seeing a ≥threshold edge; a min-call takes a <threshold edge). Those events happen at particular steps, so a `0` somewhere / infinitely often corresponds to failure(s).

But **even if conceptually correct**, it’s too expensive to be the “best” implementation.

### 1.4 It also had a real correctness pitfall in practice

The current file has a “MASKED FIX” specifically for `flatten_MinMax_Inf`:

> fixes the Max_f overlap bug by tracking distinct untracked/tracked and low/high classes per flattened child state 

That’s exactly the kind of bug you get when you compress “multiple concurrent calls” into one per-(child,state) token without tracking coexistence classes. It’s fixable, but it’s already a sign this family of “global token” constructions is tricky to maintain.

---

## 2) Current live algorithm: the shared threshold-obligation backend

Your current `flatten_MinMax_Inf` immediately returns:

```cpp
return flatten_threshold_extremal_impl(this, finite_aggregator, threshold);
```

and everything below is legacy/unreachable .

The md doc frames the overall change cleanly:

* OLD: global synchronous product construction
* Current: “track all unresolved threshold claims” via obligations

### 2.1 The obligation model is the right scalability move

Instead of “each active child must pick a transition, and we product them,” the obligation approach does:

* Each child invocation spawns an **obligation**:

  * child index
  * guess bit `g ∈ {0,1}` meaning return `< threshold` or `≥ threshold`
  * a **frontier** of child configurations still compatible with that guess

For min/max, the “progress” is just a **boolean y**:

* `Max_f`: have we seen some edge ≥ threshold yet?
* `Min_f`: have all edges so far stayed ≥ threshold?

### 2.2 Why this avoids the old cartesian-product blowup

Key point:

* For a *single* child invocation, you do **subset construction style** tracking: frontier is a set of possible (state,progress) pairs reachable by *some* run on the prefix.
* Updating that frontier on a symbol is a **union** over successors.

So you no longer enumerate combinations of choices across different calls; you update each obligation’s frontier independently (and then the only combinatorics is the global number of distinct obligations/bags, which is inherent but typically far smaller than “cartesian product each step”).

That is the right direction.

### 2.3 Silent parent steps encoding changed

The md doc points out another real change:

* OLD_V2 often used identity weights for silent parent steps (0 or 1 depending on objective).
* Current backend emits actual `SILENT` edges and relies on the later silent-removal pass .

Your current live code follows that pattern for threshold cases.

---

## 3) The “best replacement” for this case in your current codebase

Here’s the most important observation:

### You already have a specialized, faster backend for exactly `{Min_f,Max_f}×{Inf,LimInf}`

In `NestedAutomaton.cpp`, you already implemented a **bitset-based** threshold-obligation backend:

```cpp
Automaton* NestedAutomaton::flatten_MinMax_Inf_threshold_obl(...)
```

It uses:

* `MMThrFrontier`: **two bitsets** `y0` and `y1` instead of a vector of (state,prog) pairs
* precomputed per-child liveness bitsets `live[guess][y]`
* the same P1/P2 epoch discipline and final-pulse acceptance logic

That backend is *strictly better suited* for min/max than the generic `flatten_threshold_extremal_impl` because:

* progress is boolean ⇒ two bitsets are the optimal representation
* no per-step sorting/canonicalization of vectors of pairs
* bit operations are cache-friendly and fast

So the best replacement for the *public entry point* `flatten_MinMax_Inf` is:

1. **stop using** the generic backend for this case,
2. **use** the bitset mmthr backend.

That yields immediate performance wins while keeping correctness discipline already aligned with your file’s “proven epoch/final-pulse discipline” .

---

## 4) Correctness audit of the mmthr backend (detailed)

I’ll be explicit about what must be true, and why the mmthr construction enforces it.

### 4.1 What the flattened 0/1 weights mean

For each non-silent parent edge taken on symbol `a` that calls child `c`, the flattened automaton nondeterministically picks `guess ∈ {0,1}` and emits a corresponding edge of weight `guess` .

Intended meaning:

* `guess = 1` means: **this child call will terminate with value ≥ threshold**
* `guess = 0` means: **this child call will terminate with value < threshold**

And later, since the flattened automaton is checked with `infVal ∈ {Inf,LimInf}` and threshold `1`, the acceptance condition becomes:

* `Inf ≥ 1` ⇔ **no 0-weight edge occurs**
* `LimInf ≥ 1` ⇔ **only finitely many 0-weight edges occur**

So that matches the desired semantics:

* Inf threshold: all calls succeed
* LimInf threshold: all but finitely many succeed

### 4.2 The min/max “progress bit” y is correct

In the mmthr code, every obligation tracks a frontier over `(state, y)` where `y ∈ {0,1}` updated by:

* `Max_f`: `y` becomes 1 once a “high” edge (≥ threshold) has been seen; monotone 0→1 only.
* `Min_f`: `y` stays 1 only if every edge so far is “high”; once a low edge (< threshold) is seen, it becomes 0; monotone 1→0 only.

This is exactly the standard threshold abstraction for min/max objectives described in your md doc for Case 3/4 .

Thus:

* For `Max_f`, reaching a final state with `y=1` means the run’s `max ≥ threshold`.
* For `Min_f`, reaching a final state with `y=1` means the run’s `min ≥ threshold`.

And similarly, `y=0` corresponds to the “< threshold” side.

### 4.3 Why a frontier bitset is an exact existential summary (no cartesian product required)

For one child call (one obligation), define:

`F_t = {(q, y) : there exists a child run from init that reads the first t symbols of the call and ends in state q with progress y}`.

This is exactly what a subset-construction frontier represents.

On symbol step `a_{t+1}`:

* Next configurations are union of successors of each `(q,y) in F_t` under symbol `a_{t+1}` with updated progress.
  That is precisely what `step_mmthr_obl_bag` computes by iterating set bits and unioning successors into `next.y0/next.y1` .

This is correct for existential semantics because:

* even though the *real* run is in a single state,
* the frontier is the set of all states reachable by *some* run,
* so membership in the frontier is “there exists a run reaching here.”

### 4.4 Discharging an obligation is correct

An obligation with guess `g` should “discharge” when the child call can terminate *now* with final progress matching `g`.

In the step code:

* when processing successors, if an edge goes to a final state and the updated `y2 == guess`, it sets `discharged = true` and the obligation is removed .

This is correct because:

* the frontier is existential: if there exists *some* configuration in the frontier that can take a final edge with the right `y`, then there exists a child run that terminates now and matches the guess.
* removing the obligation corresponds to committing to that terminating run.

Also important: final states in your model are “terminal” (no outgoing edges), consistent with how you validate determinism/completeness elsewhere, and consistent with the stepping code ignoring successors from final states.

### 4.5 The “live” pruning is sound

You compute `live[guess][y]` for each child: which child states can still reach a final transition that produces `y_final == guess`, starting from progress y.

Then during spawn/step, you only keep successors that are live for that guess and progress, via `live.is_live(...)` guards .

This pruning is sound because:

* any non-live configuration cannot possibly discharge correctly in the future, so keeping it only adds dead nondeterminism and state explosion.
* pruning preserves existence of successful runs because it only removes configurations that **cannot** participate in any successfully discharging run.

### 4.6 The epoch discipline is preserved (termination fairness + Büchi acceptance)

The mmthr flattened state is:

* parent_state pointer
* `P1` bag (untracked obligations)
* `P2` bag (tracked obligations)
* `parent_phase ∈ {waiting, active, final_pulse}`
* `epoch_nonempty` flag

The step logic is:

* step obligations in P1 and P2 on every symbol
* if P2 becomes empty, epoch completes:

  * promote P1 to new P2
  * clear P1
  * move to `final_pulse` phase when appropriate

Final states in the flattened automaton are exactly those with:

* `parent_phase == final_pulse`
* `epoch_nonempty == true`
* parent_state is final

This matches the discipline used across your flatteners:

* it prevents accepting “silent-only” loops (epoch_nonempty would stay false if no non-silent activity/discharge)
* it enforces the progression of epochs, which forces obligations to eventually become tracked and then to discharge (or reject), which corresponds to “every spawned call must eventually terminate” in the intended nested semantics.

So the acceptance condition aligns with your “proven epoch/final-pulse discipline” contract that the current shared backend also follows .

### 4.7 Full equivalence argument (threshold nonemptiness)

Let `τ = threshold`.

**(Soundness)** If the flattened automaton has an accepting run with `Inf/LimInf ≥ 1`, then the nested automaton has a nested run with value ≥ τ.

Sketch:

* Along the accepting flattened run, every time you take a non-silent parent edge, you choose `guess ∈ {0,1}` and spawn an obligation accordingly (unless it discharges immediately).
* For each such obligation that exists, the run eventually discharges it, because:

  * epochs complete infinitely often in accepting runs (final-pulse visited infinitely often),
  * at each epoch completion, P1 is promoted to tracked P2,
  * tracked obligations must disappear (discharge) for the next epoch completion to happen.
* Discharge is only possible if there exists a child run that reaches a final transition with the correct final `y == guess` (by construction of `step_mmthr_obl_bag`). 
* Therefore, for each spawned call, there exists a terminating child run on the same input word such that:

  * if the flattened edge weight was 1, the child returns ≥ τ
  * if it was 0, the child returns < τ
* Because the flattened run satisfies `Inf/LimInf ≥ 1` with threshold 1:

  * `Inf`: no 0 weights occur ⇒ every call returns ≥ τ
  * `LimInf`: only finitely many 0 weights occur ⇒ only finitely many calls return < τ
* Hence the nested outer `Inf/LimInf` over the actual call returns is ≥ τ.

**(Completeness)** If the nested automaton has a nested run with value ≥ τ, then the flattened automaton has an accepting run with `Inf/LimInf ≥ 1`.

Sketch:

* Fix a nested accepting run (parent + one child run per call) achieving value ≥ τ.
* Define the flattened run by following the same parent transitions.
* For each parent call, choose `guess = 1` iff that child run’s return is ≥ τ, else 0.
* The mmthr obligation frontier evolution can follow that chosen child run’s actual states; therefore it never becomes empty and will discharge exactly when the child run terminates.
* The epoch discipline in the flattened construction matches the nested acceptance discipline, so the resulting flattened run is Büchi-accepting.
* The weight sequence matches which calls are ≥ τ, so the `Inf/LimInf ≥ 1` condition holds.

That closes equivalence for the threshold nonemptiness query.

---

## 5) Best replacement code

### 5.1 What to change

Right now:

```cpp
Automaton* NestedAutomaton::flatten_MinMax_Inf(...)
{
    ...
    return flatten_threshold_extremal_impl(this, finite_aggregator, threshold);
    // dead legacy code below
}
```



**Best replacement** for performance/scalability is: **dispatch to the mmthr backend**.

You already have the mmthr backend implemented and it constructs the flattened automaton directly (no Parser/string state encoding), with bitset frontiers and liveness pruning .

### 5.2 Replacement implementation (drop-in)

Replace the whole body of `flatten_MinMax_Inf` in `NestedAutomaton.cpp` with:

```cpp
Automaton* NestedAutomaton::flatten_MinMax_Inf(value_function_t finite_aggregator,
                                               weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf requires Max_f or Min_f");
    }

    // Specialized bitset threshold-obligation backend for {Max_f, Min_f} × {Inf, LimInf}.
    // This avoids Cartesian-product enumeration (OLD_V2) and avoids the generic
    // vector-of-(state,progress) frontier machinery in flatten_threshold_extremal_impl.
    return this->flatten_MinMax_Inf_threshold_obl(finite_aggregator, threshold);
}
```

Why this is the best fit for your goals:

* **Scalability**: avoids OLD_V2’s cartesian-product move enumeration entirely (the md doc explicitly calls that the “all active children at once” product construction) .
* **Efficiency**: avoids generic `ThrExtFrontier` vector sorting/canonicalization costs and uses bitsets + liveness pruning (see mmthr code) .
* **Correctness discipline**: retains the exact same epoch/final-pulse structure used in the current backend and in the mmthr backend itself .

### 5.3 What *not* to do (for your stated objectives)

* Do **not** revert to OLD_V2’s simplified/global synchronous product algorithm as the live path.
  The md doc highlights that it enumerates cartesian products of moves at each step, which is exactly the non-scalable part , and the old code literally materializes the product `combinations` .
* Do **not** keep the unreachable legacy code block after the `return`—it’s harmless at runtime but makes the file harder to reason about and maintain (and obscures what is actually live). 

If you want, I can propose a clean “archive” layout that keeps old versions behind `#if QUAK_ARCHIVE_OLD_FLATTENERS` (so you keep them, but they stop distracting the live reading path). But that’s secondary to the algorithmic fix.

---

## 6) Minimal regression tests I’d run for this exact case

Since you emphasized correctness checking, here are the concrete behaviors I’d validate (each is small and catches real bugs):

1. **Pure silent parent loop**: parent has a final SCC of only silent edges
   → flattened should *not* accept (epoch_nonempty never becomes true; no final-pulse finals). This checks the phase machine / epoch_nonempty logic. (This is a known cross-cutting area in your md doc.)

2. **Single child call that can terminate immediately with success**
   → spawn returns `EMPTY` for guess=1 and the run should accept under `Inf` if parent visits finals appropriately. This exercises the immediate-discharge path in `spawn_mmthr_obligation`.

3. **Max_f overlap scenario**: two overlapping calls of the same child can be in the same state but with different “seen_high” histories
   → old simplified algorithm was vulnerable (hence masked fix exists) 
   → mmthr algorithm must handle it because it never merges histories incorrectly: y is carried in the frontier classification, and multiple calls are separate obligations (not merged by state-id alone).

4. **Child gets stuck** (no outgoing edges on some symbol in a reachable non-final state)
   → the corresponding obligation should die (frontier empties ⇒ reject) and thus no accepting run should exist.

Those tests will quickly confirm that the replacement is not just “faster,” but preserves the intended semantics.

---

## Bottom line

For `{Inf,LimInf}×{Min_f,Max_f}`, the highest-performance, most scalable replacement is to **use your existing bitset mmthr threshold-obligation backend** as the live implementation of `flatten_MinMax_Inf`, and stop routing this case through the generic `flatten_threshold_extremal_impl`. The wrapper code above is the correct “best replacement” for your objectives, and the mmthr backend’s structure matches the correctness discipline you already rely on (epoch/final-pulse + SILENT removal) while eliminating the old cartesian-product explosion.
