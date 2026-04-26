// Drop-in fragment for NestedAutomaton.cpp
//
// Goal:
//   Exact, semantics-preserving replacement for the current Min/Max Sup/LimSup
//   threshold-extremal backend, but with memoized obligation stepping/spawning,
//   interned bags, and compact state keys.
//
// Assumptions:
//   This fragment is pasted into src/NestedAutomaton.cpp *after* the existing
//   MMThr* helpers (MMThrFrontier / MMThrLive / mmthr_* / ChildTables / etc.)
//   and before the NestedAutomaton::flatten_MinMax_Sup wrapper.
//
// Why this is correct:
//   - OblId interns the exact same (child, guess, frontier) objects used by the
//     current MMThr threshold-obligation construction.
//   - step_obl(...) is exactly the singleton-obligation specialization of
//     step_mmthr_obl_bag(...), just memoized.
//   - spawn_code(...) is exactly spawn_mmthr_obligation(...), precomputed once.
//   - BagId interns sorted unique sets of obligations, so Key{parent,P1,P2,...}
//     is an isomorphic compact encoding of the current BuchiState_mmthr.
//   - The phase machine and accepting-state rule are copied unchanged from the
//     current threshold-extremal Min/Max construction.
//
// Recommended wiring:
//   1. Paste this fragment into NestedAutomaton.cpp.
//   2. Add the declaration to NestedAutomaton.h if you want a public entry point:
//        Automaton* flatten_MinMax_Sup_cached(value_function_t finite_aggregator,
//                                            weight_t threshold);
//   3. Change flatten_MinMax_Sup(...) to call flatten_MinMax_Sup_cached(...).

namespace {

static inline uint64_t mmthr_cached_mix64(uint64_t x) {
    x ^= (x >> 33);
    x *= 0xff51afd7ed558ccdULL;
    x ^= (x >> 33);
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= (x >> 33);
    return x;
}

static inline void mmthr_cached_hash_combine(uint64_t& h, uint64_t x) {
    h ^= mmthr_cached_mix64(x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

class MMThrCachedBuilder {
public:
    using OblId = uint32_t;
    using BagId = uint32_t;

    static constexpr OblId OBL_UNKNOWN    = 0xFFFFFFFFu;
    static constexpr OblId OBL_DEAD       = 0xFFFFFFFEu;
    static constexpr OblId OBL_DISCHARGED = 0xFFFFFFFDu;

    static constexpr BagId BAG_UNKNOWN = 0xFFFFFFFFu;
    static constexpr BagId BAG_DEAD    = 0xFFFFFFFEu;

    explicit MMThrCachedBuilder(NestedAutomaton* A_,
                                bool finite_is_max_,
                                const weight_t& threshold_)
        : A(A_)
        , finite_is_max(finite_is_max_)
        , threshold(threshold_)
        , alph_size(static_cast<uint32_t>(A_->getAlphabetSize()))
        , k(static_cast<uint32_t>(A_->getChildrenSize()))
        , child_tab(k)
        , spawn(static_cast<size_t>(k) * 2u * static_cast<size_t>(alph_size), OBL_DEAD) {

        for (uint32_t i = 0; i < k; ++i) {
            ChildAutomaton* c = A->getChild(i);
            if (!c) continue;
            if (c->getStates()->size() < 2) continue;
            build_child_tables(c, child_tab[i]);
        }

        live = build_mmthr_live(child_tab, finite_is_max, threshold);

        bags.push_back(Bag{});                 // bag 0 == empty bag
        bags[0].step_next.assign(alph_size, 0u);
        bags[0].step_any_discharged.assign(alph_size, 0u);

        precompute_spawns();
    }

    inline bool child_enabled(uint32_t child) const {
        return child < k && child_tab[child].child != nullptr;
    }

    inline OblId spawn_code(uint32_t child, uint8_t guess, uint32_t sym) const {
        return spawn[(static_cast<size_t>(child) * 2u + static_cast<size_t>(guess)) *
                         static_cast<size_t>(alph_size) +
                     static_cast<size_t>(sym)];
    }

    struct BagStep {
        bool ok = true;
        BagId next = 0u;
        bool any_discharged = false;
    };

    BagId bag_add_obl(BagId base, OblId add) {
        if (add == OBL_DEAD || add == OBL_DISCHARGED || add == OBL_UNKNOWN) return base;
        if (base >= bags.size()) return base;

        const uint64_t cache_key = (static_cast<uint64_t>(base) << 32) | static_cast<uint64_t>(add);
        auto it = bag_add_cache.find(cache_key);
        if (it != bag_add_cache.end()) return it->second;

        const std::vector<OblId>& v = bags[base].obls;
        if (std::binary_search(v.begin(), v.end(), add)) {
            bag_add_cache.emplace(cache_key, base);
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
        bag_add_cache.emplace(cache_key, res);
        return res;
    }

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

    uint32_t alph_size = 0;
    uint32_t k = 0;

private:
    struct Obl {
        uint32_t child = 0;
        uint8_t guess = 0;
        MMThrFrontier fr;
        std::vector<OblId> step_cache;
    };

    struct Bag {
        std::vector<OblId> obls;
        std::vector<BagId> step_next;
        std::vector<uint8_t> step_any_discharged;
    };

    NestedAutomaton* A = nullptr;
    bool finite_is_max = false;
    weight_t threshold = 0;

    std::vector<ChildTables> child_tab;
    MMThrLive live;

    std::vector<Obl> obls;
    std::unordered_map<uint64_t, std::vector<OblId>> obl_buckets;

    std::vector<Bag> bags;
    std::unordered_map<uint64_t, std::vector<BagId>> bag_buckets;
    std::unordered_map<uint64_t, BagId> bag_add_cache;

    std::vector<OblId> spawn;

    inline OblId& spawn_code_ref(uint32_t child, uint8_t guess, uint32_t sym) {
        return spawn[(static_cast<size_t>(child) * 2u + static_cast<size_t>(guess)) *
                         static_cast<size_t>(alph_size) +
                     static_cast<size_t>(sym)];
    }

    inline size_t frontier_words(uint32_t child) const {
        return mmthr_word_count(child_tab[child].n_states);
    }

    OblId intern_obl(uint32_t child, uint8_t guess, MMThrFrontier&& fr) {
        if (child >= k || !child_enabled(child)) return OBL_DEAD;

        mmthr_frontier_canonicalize(fr, finite_is_max, guess);
        if (mmthr_frontier_empty(fr)) return OBL_DEAD;

        uint64_t h = 0;
        mmthr_cached_hash_combine(h, child);
        mmthr_cached_hash_combine(h, guess);
        mmthr_cached_hash_combine(h, static_cast<uint64_t>(fr.y0.size()));
        for (uint64_t w : fr.y0) mmthr_cached_hash_combine(h, w);
        mmthr_cached_hash_combine(h, static_cast<uint64_t>(fr.y1.size()));
        for (uint64_t w : fr.y1) mmthr_cached_hash_combine(h, w);

        auto& bucket = obl_buckets[h];
        for (OblId id : bucket) {
            const Obl& O = obls[id];
            if (O.child == child && O.guess == guess && O.fr == fr) {
                return id;
            }
        }

        const OblId new_id = static_cast<OblId>(obls.size());
        Obl O;
        O.child = child;
        O.guess = guess;
        O.fr = std::move(fr);
        O.step_cache.assign(alph_size, OBL_UNKNOWN);
        obls.push_back(std::move(O));
        bucket.push_back(new_id);
        return new_id;
    }

    BagId intern_bag(std::vector<OblId>&& ids) {
        if (ids.empty()) return 0u;

        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

        uint64_t h = 0;
        mmthr_cached_hash_combine(h, static_cast<uint64_t>(ids.size()));
        for (OblId id : ids) mmthr_cached_hash_combine(h, id);

        auto& bucket = bag_buckets[h];
        for (BagId bid : bucket) {
            if (bags[bid].obls == ids) return bid;
        }

        const BagId new_id = static_cast<BagId>(bags.size());
        Bag B;
        B.obls = std::move(ids);
        B.step_next.assign(alph_size, BAG_UNKNOWN);
        B.step_any_discharged.assign(alph_size, 0u);
        bags.push_back(std::move(B));
        bucket.push_back(new_id);
        return new_id;
    }

    void precompute_spawns() {
        for (uint32_t child = 0; child < k; ++child) {
            if (!child_enabled(child)) continue;

            for (uint8_t guess = 0u; guess <= 1u; ++guess) {
                for (uint32_t sym = 0; sym < alph_size; ++sym) {
                    MMThrOblEntry spawned;
                    const MMThrSpawnStatus st = spawn_mmthr_obligation(
                        child, sym, guess, spawned, child_tab, live, finite_is_max, threshold);

                    if (st == MMThrSpawnStatus::REJECT) {
                        spawn_code_ref(child, guess, sym) = OBL_DEAD;
                    } else if (st == MMThrSpawnStatus::EMPTY) {
                        spawn_code_ref(child, guess, sym) = OBL_DISCHARGED;
                    } else {
                        spawn_code_ref(child, guess, sym) =
                            intern_obl(child, guess, std::move(spawned.key.fr));
                    }
                }
            }
        }
    }

    OblId step_obl(OblId id, uint32_t sym) {
        if (id >= obls.size()) return OBL_DEAD;

        Obl& O = obls[id];
        const OblId cached = O.step_cache[sym];
        if (cached != OBL_UNKNOWN) return cached;

        if (!child_enabled(O.child)) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        const ChildTables& T = child_tab[O.child];
        if (sym >= T.alph) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        MMThrFrontier next;
        mmthr_frontier_zero(next, frontier_words(O.child));
        bool discharged = false;

        auto step_one_class = [&](const std::vector<uint64_t>& bits, uint8_t y) {
            mmthr_for_each_set_bit(bits, [&](uint32_t st) {
                if (discharged) return;
                if (st >= T.n_states) return;
                if (!live.is_live(O.child, O.guess, st, y)) return;

                const uint32_t cell = T.idx(st, sym);
                const uint32_t b = T.off[static_cast<size_t>(cell)];
                const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                for (uint32_t p = b; p < e; ++p) {
                    const auto& tr = T.edges[static_cast<size_t>(p)];
                    if (tr.to >= T.n_states) continue;

                    const uint8_t y2 = mmthr_y_update(finite_is_max, y, tr.w, threshold);
                    if (T.is_final[tr.to]) {
                        if (y2 == O.guess) {
                            discharged = true;
                            return;
                        }
                        continue;
                    }

                    if (!live.is_live(O.child, O.guess, tr.to, y2)) continue;
                    if (y2 == 0u) mmthr_bits_set(next.y0, tr.to);
                    else          mmthr_bits_set(next.y1, tr.to);
                }
            });
        };

        step_one_class(O.fr.y0, 0u);
        if (!discharged) step_one_class(O.fr.y1, 1u);

        if (discharged) {
            O.step_cache[sym] = OBL_DISCHARGED;
            return OBL_DISCHARGED;
        }

        mmthr_frontier_canonicalize(next, finite_is_max, O.guess);
        if (mmthr_frontier_empty(next)) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        const OblId nid = intern_obl(O.child, O.guess, std::move(next));
        O.step_cache[sym] = nid;
        return nid;
    }
};

static Automaton* flatten_MinMax_Sup_cached_mmthr_impl(NestedAutomaton* A,
                                                       value_function_t finite_aggregator,
                                                       weight_t threshold) {
    const bool finite_is_max = (finite_aggregator == Max_f);
    if (!finite_is_max && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Sup_cached requires Max_f or Min_f");
    }

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    MMThrCachedBuilder builder(A, finite_is_max, threshold);

    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(A->getAlphabetSize());
    for (size_t i = 0; i < A->getAlphabetSize(); ++i) {
        Symbol* original = A->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

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

    struct Key {
        uint32_t parent = 0;
        MMThrCachedBuilder::BagId P1 = 0u;
        MMThrCachedBuilder::BagId P2 = 0u;
        uint8_t phase = 1u;            // 0 = active, 1 = waiting, 2 = final_pulse
        uint8_t epoch_nonempty = 0u;

        bool operator==(const Key& o) const {
            return parent == o.parent && P1 == o.P1 && P2 == o.P2 &&
                   phase == o.phase && epoch_nonempty == o.epoch_nonempty;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& key) const noexcept {
            uint64_t h = 0;
            mmthr_cached_hash_combine(h, key.parent);
            mmthr_cached_hash_combine(h, key.P1);
            mmthr_cached_hash_combine(h, key.P2);
            mmthr_cached_hash_combine(h, key.phase);
            mmthr_cached_hash_combine(h, key.epoch_nonempty);
            return static_cast<size_t>(mmthr_cached_mix64(h));
        }
    };

    std::unordered_map<Key, State*, KeyHash> state_map;
    state_map.reserve(4096);
    std::deque<Key> worklist;
    unsigned int state_counter = 0;

    const weight_t global_min = weight_t(0);
    const weight_t global_max = weight_t(1);

    Key init;
    init.parent = static_cast<uint32_t>(A->getInitial()->getId());

    std::ostringstream ss;
    ss << "bmmsupcache_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
    state_map.emplace(init, init_state);
    worklist.push_back(init);

    while (!worklist.empty()) {
        const Key current = worklist.front();
        worklist.pop_front();

        State* current_state = state_map.find(current)->second;
        State* parent_state = A->getStates()->at(current.parent);

        for (uint32_t symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            const auto P1res = builder.step_bag(current.P1, symbol_id);
            if (!P1res.ok) continue;

            bool tracked_discharged = false;
            MMThrCachedBuilder::BagId P2_step = 0u;
            if (current.P2 != 0u) {
                const auto P2res = builder.step_bag(current.P2, symbol_id);
                if (!P2res.ok) continue;
                P2_step = P2res.next;
                tracked_discharged = P2res.any_discharged;
            }

            SetStd<Edge*>* succs = parent_state->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* parent_edge : *succs) {
                const uint32_t q_prime = static_cast<uint32_t>(parent_edge->getTo()->getId());
                const uint32_t child_index = static_cast<uint32_t>(
                    edgeWeightToChildIndex(parent_edge->getWeight()->getValue()));
                const bool is_silent =
                    (child_index >= builder.k) || !builder.child_enabled(child_index);

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

                auto get_or_create = [&](const Key& key) -> State* {
                    auto it = state_map.find(key);
                    if (it != state_map.end()) return it->second;

                    std::ostringstream s2;
                    s2 << "bmmsupcache_" << state_counter++;
                    State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                    state_map.emplace(key, ns);
                    worklist.push_back(key);
                    return ns;
                };

                const auto P2_next = epoch_complete ? P1res.next : P2_step;
                const auto baseP1  = epoch_complete ? 0u        : P1res.next;
                const uint8_t phase_next = (epoch_complete && new_phase == 0u) ? 2u : new_phase;
                const uint8_t epoch_flag = static_cast<uint8_t>(epoch_nonempty ? 1u : 0u);

                if (is_silent) {
                    const Key nxt{q_prime, baseP1, P2_next, phase_next, epoch_flag};
                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(SILENT)),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                    continue;
                }

                for (uint8_t guess = 0u; guess <= 1u; ++guess) {
                    const auto sc = builder.spawn_code(child_index, guess, symbol_id);
                    if (sc == MMThrCachedBuilder::OBL_DEAD) continue;

                    auto P1_next = baseP1;
                    if (sc != MMThrCachedBuilder::OBL_DISCHARGED) {
                        P1_next = builder.bag_add_obl(baseP1, sc);
                    }

                    const Key nxt{q_prime, P1_next, P2_next, phase_next, epoch_flag};
                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(static_cast<unsigned int>(guess))),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                }
            }
        }
    }

    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [key, st] : state_map) {
        new_states->insert(st->getId(), st);
        if (key.phase == 2u && key.epoch_nonempty != 0u &&
            A->getStates()->at(key.parent)->getFinal()) {
            st->setFinal(true);
        }
    }

    const std::string name = "BuchiMinMaxSupCached(" + A->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights,
                         global_min, global_max, init_state);
}

} // namespace

Automaton* NestedAutomaton::flatten_MinMax_Sup_cached(value_function_t finite_aggregator,
                                                      weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Sup_cached requires Max_f or Min_f");
    }
    return flatten_MinMax_Sup_cached_mmthr_impl(this, finite_aggregator, threshold);
}

// Suggested one-line replacement in the current wrapper:
//
// Automaton* NestedAutomaton::flatten_MinMax_Sup(value_function_t finite_aggregator,
//                                                weight_t threshold) {
//     if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
//         QUAK_FAIL("flatten_MinMax_Sup requires Max_f or Min_f");
//     }
//     return this->flatten_MinMax_Sup_cached(finite_aggregator, threshold);
// }
