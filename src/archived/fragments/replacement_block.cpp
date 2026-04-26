
// Work item for simplified Inf/LimInf + Min_f/Max_f construction.
//
// For Max_f we must distinguish four coexistence classes per flattened child state:
//   UL = untracked + low-so-far
//   UH = untracked + high-seen
//   TL = tracked   + low-so-far
//   TH = tracked   + high-seen
//
// For Min_f the finite-state summary has no "seen_high" dimension, but we still
// need to distinguish tracked vs. untracked tokens:
//   U  = untracked
//   T  = tracked
//
// The active state vector therefore stores a class-mask per flattened child state.
// Multiple classes may coexist at the same child state.
struct simple_work_item {
    std::string global_from;
    unsigned int parent_state_id;
    unsigned int parent_phase;  // 0 = active, 1 = waiting, 2 = final_pulse
    std::vector<unsigned int> child_mask;  // see bit layout below
    bool epoch_nonempty;  // true iff the current epoch has seen a non-silent step or a tracked completion
};

enum : unsigned int {
    SIMPLE_MIN_U  = 1u << 0,
    SIMPLE_MIN_T  = 1u << 1,

    SIMPLE_MAX_UL = 1u << 0,
    SIMPLE_MAX_UH = 1u << 1,
    SIMPLE_MAX_TL = 1u << 2,
    SIMPLE_MAX_TH = 1u << 3,
};

static inline bool simple_mask_has_active(unsigned int mask, bool is_max) {
    if (is_max) {
        return (mask & (SIMPLE_MAX_UL | SIMPLE_MAX_UH | SIMPLE_MAX_TL | SIMPLE_MAX_TH)) != 0u;
    }
    return (mask & (SIMPLE_MIN_U | SIMPLE_MIN_T)) != 0u;
}

static inline bool simple_mask_has_tracked(unsigned int mask, bool is_max) {
    if (is_max) {
        return (mask & (SIMPLE_MAX_TL | SIMPLE_MAX_TH)) != 0u;
    }
    return (mask & SIMPLE_MIN_T) != 0u;
}

static inline unsigned int simple_spawn_mask(bool is_max) {
    return is_max ? SIMPLE_MAX_UL : SIMPLE_MIN_U;
}

static inline unsigned int simple_promote_mask(unsigned int mask, bool is_max) {
    if (is_max) {
        unsigned int promoted = 0u;
        if (mask & SIMPLE_MAX_UL) promoted |= SIMPLE_MAX_TL;
        if (mask & SIMPLE_MAX_UH) promoted |= SIMPLE_MAX_TH;
        if (mask & SIMPLE_MAX_TL) promoted |= SIMPLE_MAX_TL;
        if (mask & SIMPLE_MAX_TH) promoted |= SIMPLE_MAX_TH;
        return promoted;
    }

    if ((mask & (SIMPLE_MIN_U | SIMPLE_MIN_T)) != 0u) {
        return SIMPLE_MIN_T;
    }
    return 0u;
}

// Helper to encode state for the simplified Inf/LimInf construction.
static std::string encode_simple_state(unsigned int parent_id,
                                       unsigned int phase,
                                       const std::vector<unsigned int>& child_mask,
                                       unsigned int total,
                                       bool epoch_nonempty) {
    std::string s;
    s.reserve(64 + total * 4);
    s += std::to_string(parent_id);
    s += "/";
    s += std::to_string(phase);
    s += "/";

    for (unsigned int i = 0; i < total; ++i) {
        s += std::to_string(child_mask[i]);
        if (i + 1 < total) s += ",";
    }

    s += "/";
    s += (epoch_nonempty ? "E" : "e");
    return s;
}

Automaton* NestedAutomaton::flatten_MinMax_Inf(value_function_t finite_aggregator,
                                               weight_t threshold) {
    const bool is_max = (finite_aggregator == Max_f);
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf requires Max_f or Min_f");
    }

    // Build cumulative_size for child-state flattening.
    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i <= this->getChildrenSize(); ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int total_child_states = cumulative_size[this->getChildrenSize()];

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);

    // Sink state with self-loops.
    parser->states.insert("@sink@");
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

    // Initial state: parent at initial, waiting phase, no active children.
    std::vector<unsigned int> init_mask(total_child_states, 0u);
    const bool init_epoch_nonempty = false;
    std::string initial_state = encode_simple_state(
        (unsigned int)this->initial->getId(),
        1u,
        init_mask,
        total_child_states,
        init_epoch_nonempty
    );

    parser->states.insert(initial_state);
    parser->initial = initial_state;

    // Worklist-based exploration.
    std::vector<simple_work_item> worklist;
    worklist.push_back({
        initial_state,
        (unsigned int)this->initial->getId(),
        1u,
        init_mask,
        init_epoch_nonempty
    });

    while (!worklist.empty()) {
        simple_work_item current = std::move(worklist.back());
        worklist.pop_back();

        State* parent_state = this->getStates()->at(current.parent_state_id);

        for (Symbol* symbol : *parent_state->getAlphabet()) {
            auto* parent_succs = parent_state->getSuccessors(symbol->getId());
            if (!parent_succs) continue;

            for (Edge* parent_edge : *parent_succs) {
                const unsigned int next_parent_id = (unsigned int)parent_edge->getTo()->getId();
                const unsigned int child_id = (unsigned int)parent_edge->getWeight()->getValue().to_float();
                const bool is_silent = (this->getChild(child_id)->getStates()->size() == 1);

                // Start from the current state and handle phase transition first.
                std::vector<unsigned int> spawned_mask = current.child_mask;
                unsigned int new_phase = current.parent_phase;
                const bool reset_epoch = (current.parent_phase == 2u);
                if (reset_epoch) {
                    new_phase = 0u;  // start the next epoch
                }

                // Spawn the called child (if non-silent). New spawns are ACTIVE but NOT TRACKED.
                if (!is_silent) {
                    const unsigned int spawn_state = (unsigned int)this->getChild(child_id)->initial->getId();
                    const unsigned int spawn_idx = cumulative_size[child_id] + spawn_state;
                    spawned_mask[spawn_idx] |= simple_spawn_mask(is_max);

                    if (new_phase == 1u) {  // was waiting
                        new_phase = 0u;
                    }
                }

                struct ChildMove {
                    unsigned int to_idx;
                    weight_t edge_w;
                    bool to_final;
                };

                struct ClassBucket {
                    bool tracked;
                    bool high_seen;  // only relevant for Max_f
                    std::vector<ChildMove> moves;
                };

                // Build one move-bucket per ACTIVE CLASS, not per active state.
                const std::vector<unsigned int> snapshot_mask = spawned_mask;
                std::vector<ClassBucket> buckets;
                bool has_stuck_child = false;

                auto push_bucket = [&](unsigned int cid,
                                       unsigned int sid,
                                       bool tracked_class,
                                       bool high_seen_class) {
                    ChildAutomaton* child = this->getChild(cid);
                    State* child_state = child->getStates()->at(sid);

                    // Active finals should have terminated in the previous step already.
                    // Skip them defensively instead of propagating them.
                    if (child_state->getFinal()) {
                        return;
                    }

                    auto* child_succs = child_state->getSuccessors(symbol->getId());
                    if (!child_succs || child_succs->empty()) {
                        has_stuck_child = true;
                        return;
                    }

                    ClassBucket bucket;
                    bucket.tracked = tracked_class;
                    bucket.high_seen = high_seen_class;
                    bucket.moves.reserve(child_succs->size());

                    for (Edge* ce : *child_succs) {
                        const unsigned int to_sid = (unsigned int)ce->getTo()->getId();
                        bucket.moves.push_back({
                            cumulative_size[cid] + to_sid,
                            ce->getWeight()->getValue(),
                            ce->getTo()->getFinal()
                        });
                    }

                    buckets.push_back(std::move(bucket));
                };

                for (unsigned int cid = 0; cid < this->getChildrenSize() && !has_stuck_child; ++cid) {
                    ChildAutomaton* child = this->getChild(cid);
                    auto* child_states = child->getStates();
                    if (!child_states) continue;

                    for (unsigned int sid = 0; sid < child_states->size() && !has_stuck_child; ++sid) {
                        const unsigned int idx = cumulative_size[cid] + sid;
                        const unsigned int mask = snapshot_mask[idx];
                        if (!simple_mask_has_active(mask, is_max)) continue;

                        if (is_max) {
                            if (mask & SIMPLE_MAX_UL) push_bucket(cid, sid, false, false);
                            if (mask & SIMPLE_MAX_UH) push_bucket(cid, sid, false, true);
                            if (mask & SIMPLE_MAX_TL) push_bucket(cid, sid, true, false);
                            if (mask & SIMPLE_MAX_TH) push_bucket(cid, sid, true, true);
                        } else {
                            if (mask & SIMPLE_MIN_U) push_bucket(cid, sid, false, false);
                            if (mask & SIMPLE_MIN_T) push_bucket(cid, sid, true, false);
                        }
                    }
                }

                if (has_stuck_child) {
                    parser->edges.insert({
                        { symbol->getName(), weight_t(0) },
                        { current.global_from, "@sink@" }
                    });
                    continue;
                }

                // Fresh next-state writes: do not mutate or read from snapshot_mask in place.
                std::vector<unsigned int> next_mask(total_child_states, 0u);
                bool base_epoch_nonempty = reset_epoch ? false : current.epoch_nonempty;
                if (!is_silent) {
                    base_epoch_nonempty = true;  // non-silent step makes the epoch non-vacuous
                }

                std::function<void(size_t, bool, bool)> explore_bucket;
                explore_bucket = [&](size_t bucket_idx, bool any_failure, bool epoch_nonempty) {
                    if (bucket_idx == buckets.size()) {
                        std::vector<unsigned int> result_mask = next_mask;
                        unsigned int result_phase = new_phase;
                        bool is_final = false;

                        bool any_tracked_active = false;
                        for (unsigned int i = 0; i < total_child_states; ++i) {
                            if (simple_mask_has_tracked(result_mask[i], is_max) &&
                                simple_mask_has_active(result_mask[i], is_max)) {
                                any_tracked_active = true;
                                break;
                            }
                        }

                        if (!any_tracked_active && new_phase == 0u) {
                            // Epoch boundary: all currently tracked obligations have discharged.
                            for (unsigned int i = 0; i < total_child_states; ++i) {
                                result_mask[i] = simple_promote_mask(result_mask[i], is_max);
                            }
                            result_phase = 2u;  // final pulse
                            if (epoch_nonempty) {
                                is_final = this->getStates()->at(next_parent_id)->getFinal();
                            }
                        }

                        const std::string next_state = encode_simple_state(
                            next_parent_id,
                            result_phase,
                            result_mask,
                            total_child_states,
                            epoch_nonempty
                        );

                        parser->edges.insert({
                            { symbol->getName(), any_failure ? weight_t(0) : weight_t(1) },
                            { current.global_from, next_state }
                        });

                        if (is_final) {
                            parser->final_states.insert(next_state);
                        }

                        if (!parser->states.contains(next_state)) {
                            parser->states.insert(next_state);
                            worklist.push_back({
                                next_state,
                                next_parent_id,
                                result_phase,
                                std::move(result_mask),
                                epoch_nonempty
                            });
                        }
                        return;
                    }

                    const ClassBucket& bucket = buckets[bucket_idx];
                    for (const ChildMove& move : bucket.moves) {
                        if (is_max) {
                            const bool edge_high = (move.edge_w >= threshold);
                            const bool now_high = bucket.high_seen || edge_high;

                            if (move.to_final) {
                                // The class terminates on this symbol.
                                const bool fail_here = !now_high;
                                const bool epoch_nonempty_here = epoch_nonempty || bucket.tracked;
                                explore_bucket(bucket_idx + 1,
                                               any_failure || fail_here,
                                               epoch_nonempty_here);
                            } else {
                                const unsigned int add_mask =
                                    now_high
                                        ? (bucket.tracked ? SIMPLE_MAX_TH : SIMPLE_MAX_UH)
                                        : (bucket.tracked ? SIMPLE_MAX_TL : SIMPLE_MAX_UL);

                                const unsigned int saved = next_mask[move.to_idx];
                                next_mask[move.to_idx] |= add_mask;

                                explore_bucket(bucket_idx + 1, any_failure, epoch_nonempty);

                                next_mask[move.to_idx] = saved;
                            }
                        } else {
                            const bool edge_high = (move.edge_w >= threshold);

                            if (!edge_high) {
                                // Min_f is irreversibly failed as soon as we see a low edge.
                                const bool epoch_nonempty_here = epoch_nonempty || bucket.tracked;
                                explore_bucket(bucket_idx + 1, true, epoch_nonempty_here);
                            } else if (move.to_final) {
                                // Successful termination.
                                const bool epoch_nonempty_here = epoch_nonempty || bucket.tracked;
                                explore_bucket(bucket_idx + 1, any_failure, epoch_nonempty_here);
                            } else {
                                const unsigned int add_mask = bucket.tracked ? SIMPLE_MIN_T : SIMPLE_MIN_U;
                                const unsigned int saved = next_mask[move.to_idx];
                                next_mask[move.to_idx] |= add_mask;

                                explore_bucket(bucket_idx + 1, any_failure, epoch_nonempty);

                                next_mask[move.to_idx] = saved;
                            }
                        }
                    }
                };

                explore_bucket(0u, false, base_epoch_nonempty);
            }
        }
    }

    std::string newname = "unnested_simple(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    return unnested;
}
