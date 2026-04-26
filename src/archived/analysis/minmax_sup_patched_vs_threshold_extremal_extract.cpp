// Extracted reference file.
//
// Contents:
// - patched copied main-repo Sup/ LimSup x Min/Max backend code used for the current comparison
// - live threshold-extremal Sup backend code used by src/NestedAutomaton.cpp
//
// This file is intentionally not a build target. The code blocks are wrapped in #if 0
// so the two verbatim implementations can coexist in one place despite duplicate names.

// ===== Patched copied main-repo Sup-Max backend =====
// Source origins:
// - analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp:3548-3560
// - analysis/NestedAutomaton_mainRepo_sup_max_compare.cpp:4635-5296
#if 0
static inline bool tracking_all_zero(const std::vector<unsigned char>& v) {
    for (unsigned char b : v) {
        if (b != 0) return false;
    }
    return true;
}

static inline std::string bits_to_string(const std::vector<unsigned char>& v) {
    std::string s;
    s.reserve(v.size());
    for (unsigned char b : v) s.push_back(b ? '1' : '0');
    return s;
}
// Backward BFS: can child state reach a final state?
static std::vector<bool> compute_can_reach_final_child(ChildAutomaton* child) {
    auto* states = child->getStates();
    if (!states) return {};

    const unsigned int n = states->size();
    std::vector<bool> can_reach(n, false);
    std::vector<bool> visited(n, false);
    std::queue<unsigned int> q;
    for (unsigned int i = 0; i < n; ++i) {
        if (states->at(i)->getFinal()) {
            can_reach[i] = true;
            q.push(i);
            visited[i] = true;
        }
    }
    while (!q.empty()) {
        unsigned int cur = q.front();
        q.pop();
        for (unsigned int pred = 0; pred < n; ++pred) {
            if (visited[pred]) continue;
            State* pred_state = states->at(pred);
            for (Symbol* sym : *pred_state->getAlphabet()) {
                auto* succs = pred_state->getSuccessors(sym->getId());
                if (!succs) continue;
                for (Edge* e : *succs) {
                    if ((unsigned int)e->getTo()->getId() == cur) {
                        can_reach[pred] = true;
                        visited[pred] = true;
                        q.push(pred);
                        goto next_pred;
                    }
                }
            }
            next_pred:;
        }
    }
    return can_reach;
}

enum minmax_sup_accept_phase_t : unsigned int {
    MM_SUP_ACC_IDLE     = 0u,
    MM_SUP_ACC_PARENT   = 1u,
    MM_SUP_ACC_COMPLETE = 2u,
    MM_SUP_ACC_PULSE    = 3u,
};

static inline bool any_one(const std::vector<unsigned char>& v) {
    for (unsigned char b : v) {
        if (b) return true;
    }
    return false;
}

// Worklist item for Min_f/Max_f under Sup/LimSup: track ONE distinguished child-token,
// and represent activation/tracking as variable-length vectors (no fixed-size bitmasks).
struct min_max_sup_work_item {
    std::string global_from;
    unsigned int parent_state_id_from;

    // 0/1 per flattened child-state
    std::vector<unsigned char> activation_from;
    std::vector<unsigned char> tracking_from;

    // Distinguished token being tracked for the (0/1) edge weight.
    // If inactive_from==true, the remaining fields are ignored.
    bool inactive_from = true;
    unsigned int witness_child_id_from = 0;
    unsigned int witness_child_state_id_from = 0;
    unsigned int witness_y_from = 0; // monotone bit: Max_f starts 0, Min_f starts 1

    unsigned int accept_phase_from = MM_SUP_ACC_IDLE;
    bool epoch_nonempty_from = false;
};

// Min_f/Max_f + Sup/LimSup flattening (single token tracking)
typedef struct global_exploration_data_min_max_supremum {
    // constraints
    NestedAutomaton* A = nullptr;
    Parser* parser = nullptr;
    weight_t threshold{};
    unsigned int* cumulative_size = nullptr;
    unsigned int children_all = 0;
    unsigned int finite_is_max = 0; // 0 = Min_f, 1 = Max_f

    // epoch reset vector (all ones)
    std::vector<unsigned char> track_them_all;

    // worklist for iterative DFS
    std::vector<min_max_sup_work_item>* worklist = nullptr;

    // precomputed: can child state i reach a final state?
    std::vector<bool> can_reach_final;

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int parent_state_id_from = 0;
    std::vector<unsigned char> activation_from;
    std::vector<unsigned char> tracking_from;
    bool inactive_from = true;
    unsigned int witness_child_id_from = 0;
    unsigned int witness_child_state_id_from = 0;
    unsigned int witness_y_from = 0;
    unsigned int accept_phase_from = MM_SUP_ACC_IDLE;
    bool epoch_nonempty_from = false;

    // initialized per-symbol
    Symbol* symbol = nullptr;
    std::vector<unsigned char> old_activation;
    std::vector<unsigned char> old_tracking;
    std::vector<unsigned char> new_activation;
    std::vector<unsigned char> new_tracking;
    bool spawned_from = false;
    bool spawned_is_witness = false;
    unsigned int spawned_child_id_from = 0;
    unsigned int spawned_child_state_id_from = 0;
    bool current_parent_edge_is_real = false;

    // computed (output accumulators)
    unsigned int parent_state_id_to = 0;
    weight_t global_edge_weight = 0;
    bool inactive_to = true;
    unsigned int witness_child_id_to = 0;
    unsigned int witness_child_state_id_to = 0;
    unsigned int witness_y_to = 0;
} data_min_max_supremum_t;

static void explore_global_initialization_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_parent_transition_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_child_transition_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_selection_min_max_supremum(unsigned int child_id, unsigned int child_state_id,
                                                      data_min_max_supremum_t* data);
static void explore_global_finalization_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_failure_min_max_supremum(data_min_max_supremum_t* data);

// static inline bool tracking_all_zero(const std::vector<unsigned char>& v) {
//     for (unsigned char b : v) {
//         if (b != 0) return false;
//     }
//     return true;
// }

// static std::string bits_to_string(const std::vector<unsigned char>& v) {
//     std::string s;
//     s.reserve(v.size());
//     for (unsigned char b : v) {
//         s.push_back(b ? '1' : '0');
//     }
//     return s;
// }

static inline unsigned int min_max_y_update(const weight_t& edge_value,
                                            unsigned int y_current,
                                            const data_min_max_supremum_t* data) {
    const bool pass = !(edge_value < data->threshold); // edge_value >= threshold
    if (data->finite_is_max) {
        // Max_f: y' = y OR pass
        return (y_current != 0u || pass) ? 1u : 0u;
    }
    // Min_f: y' = y AND pass
    return (y_current != 0u && pass) ? 1u : 0u;
}

static void explore_global_failure_min_max_supremum(data_min_max_supremum_t* data) {
#ifdef DEBUG
    std::cout << "FAILURE: " << data->symbol->getName() << " from " << data->global_from << " -> @sink@" << std::endl;
#endif
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

static void explore_global_finalization_min_max_supremum(data_min_max_supremum_t* data) {
    // Compute destination activation/tracking vectors.
    std::vector<unsigned char> activation_to = data->new_activation; // copy
    std::vector<unsigned char> tracking_to   = data->new_tracking;   // copy

    const bool parent_final_to =
        data->A->getStates()->at(data->parent_state_id_to)->getFinal();
    const bool destination_tracking_all_zero = tracking_all_zero(tracking_to);

    const bool epoch_nonempty_here =
        data->epoch_nonempty_from || data->current_parent_edge_is_real;
    const bool completion_now =
        destination_tracking_all_zero && epoch_nonempty_here;

    bool parent_seen_to = false;
    bool completion_seen_to = false;
    if (data->accept_phase_from == MM_SUP_ACC_PARENT) {
        parent_seen_to = true;
    } else if (data->accept_phase_from == MM_SUP_ACC_COMPLETE) {
        completion_seen_to = true;
    }

    parent_seen_to = parent_seen_to || parent_final_to;
    completion_seen_to = completion_seen_to || completion_now;

    if (destination_tracking_all_zero) {
        tracking_to = data->track_them_all;
    }

    const bool next_epoch_active =
        (!data->inactive_to) || any_one(activation_to);
    const bool epoch_nonempty_to =
        destination_tracking_all_zero ? next_epoch_active : epoch_nonempty_here;

    unsigned int accept_phase_to = MM_SUP_ACC_IDLE;
    bool global_final = false;
    if (parent_seen_to && completion_seen_to) {
        accept_phase_to = MM_SUP_ACC_PULSE;
        global_final = true;
    } else if (parent_seen_to) {
        accept_phase_to = MM_SUP_ACC_PARENT;
    } else if (completion_seen_to) {
        accept_phase_to = MM_SUP_ACC_COMPLETE;
    }

    // Doomed-state pruning: if any position has activation=1, tracking=1,
    // and can_reach_final=false, tracking can never reach all-zero again
    // (that position is a non-final sink that perpetuates its tracking bit).
    // No more epoch boundaries → no more final states → redirect to sink.
    if (!tracking_all_zero(tracking_to)) {
        for (unsigned int i = 0; i < data->children_all; ++i) {
            if (activation_to[i] && tracking_to[i] && !data->can_reach_final[i]) {
                explore_global_failure_min_max_supremum(data);
                return;
            }
        }
    }

    // Encode destination state:
    //   parent_id/activation_bits/tracking_bits/[child_id/child_state/y | @inactive@]
    std::string global_to;
    global_to.reserve(64 + data->children_all * 2);
    global_to.append(std::to_string(data->parent_state_id_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(activation_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(tracking_to));
    global_to.push_back('/');
    global_to.append(std::to_string(accept_phase_to));
    global_to.push_back('/');
    global_to.push_back(epoch_nonempty_to ? '1' : '0');

    if (data->inactive_to) {
        global_to.append("/@inactive@");
    } else {
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_child_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_child_state_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_y_to));
    }

    data->parser->edges.insert({
        { data->symbol->getName(), data->global_edge_weight },
        { data->global_from, global_to }
    });

#ifdef DEBUG
    std::cout << "SUCCESS: " << data->symbol->getName() << " : " << data->global_edge_weight.to_float()
              << ", " << data->global_from << " -> " << global_to << std::endl;
#endif

    if (global_final) {
        data->parser->final_states.insert(global_to);
    }

    // Iterative DFS: push to worklist on first discovery
    if (!data->parser->states.contains(global_to)) {
        data->parser->states.insert(global_to);
        data->worklist->push_back({
            global_to,
            data->parent_state_id_to,
            std::move(activation_to),
            std::move(tracking_to),
            data->inactive_to,
            data->witness_child_id_to,
            data->witness_child_state_id_to,
            data->witness_y_to,
            accept_phase_to,
            epoch_nonempty_to
        });
    }
}

static void explore_global_selection_min_max_supremum(unsigned int child_id,
                                                      unsigned int child_state_id,
                                                      data_min_max_supremum_t* data) {
    // Skip the distinguished token's FROM location (handled in explore_global_child_transition_min_max_supremum)
    if (!data->inactive_from &&
        child_id == data->witness_child_id_from &&
        child_state_id == data->witness_child_state_id_from) {
        explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
        return;
    }

    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_activation[i] == 0) {
                explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
                return;
            }

            const bool freshly_spawned_background =
                data->spawned_from &&
                !data->spawned_is_witness &&
                child_id == data->spawned_child_id_from &&
                child_state_id == data->spawned_child_state_id_from;

            if (!freshly_spawned_background && states->at(child_state_id)->getFinal()) {
                // Background final states terminate immediately.
                explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (succs) {
                for (Edge* edge : *succs) {
                    // If successor is final: terminate in the same symbol (do not propagate)
                    if (edge->getTo()->getFinal()) {
                        explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
                        continue;
                    }

                    const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();
                    const unsigned char stored_tracking = data->new_tracking[ii];
                    const unsigned char stored_activation = data->new_activation[ii];

                    if (data->old_tracking[i] == 1) {
                        data->new_tracking[ii] = 1;
                    }
                    if (data->old_activation[i] == 1) {
                        data->new_activation[ii] = 1;
                    }

                    explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);

                    data->new_tracking[ii] = stored_tracking;
                    data->new_activation[ii] = stored_activation;
                }
            } else {
                if (freshly_spawned_background) {
                    explore_global_failure_min_max_supremum(data);
                } else {
                    // No successors on this symbol: treat as silent "no move" for background
                    explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
                }
            }
        } else {
            explore_global_selection_min_max_supremum(child_id + 1, 0, data);
        }
    } else {
        explore_global_finalization_min_max_supremum(data);
    }
}

static void explore_global_child_transition_min_max_supremum(data_min_max_supremum_t* data) {
    if (data->inactive_from) {
        data->inactive_to = true;
        explore_global_selection_min_max_supremum(0, 0, data);
        return;
    }

    ChildAutomaton* child = data->A->getChild(data->witness_child_id_from);
    State* child_state = child->getStates()->at(data->witness_child_state_id_from);

    const bool freshly_spawned_witness =
        data->spawned_from &&
        data->spawned_is_witness &&
        data->witness_child_id_from == data->spawned_child_id_from &&
        data->witness_child_state_id_from == data->spawned_child_state_id_from;

    // Should normally not happen if we enforce "terminate in same symbol", but handle robustly.
    if (!freshly_spawned_witness && child_state->getFinal()) {
        if (data->witness_y_from == 1u) {
            data->inactive_to = true;
            explore_global_selection_min_max_supremum(0, 0, data);
        } else {
            explore_global_failure_min_max_supremum(data);
        }
        return;
    }

    const unsigned int i = data->cumulative_size[data->witness_child_id_from] + data->witness_child_state_id_from;

    auto* succs = child_state->getSuccessors(data->symbol->getId());
    if (!succs) {
        explore_global_failure_min_max_supremum(data);
        return;
    }

    for (Edge* child_edge : *succs) {
        const unsigned int to_state_id = (unsigned int)child_edge->getTo()->getId();
        const unsigned int ii = data->cumulative_size[data->witness_child_id_from] + to_state_id;

        const unsigned char stored_tracking = data->new_tracking[ii];
        const unsigned char stored_activation = data->new_activation[ii];

        const unsigned int y_next = min_max_y_update(child_edge->getWeight()->getValue(), data->witness_y_from, data);

        // For Min_f, once y becomes 0 it can never recover, so the branch is dead.
        if (!data->finite_is_max && y_next == 0u) {
            explore_global_failure_min_max_supremum(data);
            continue;
        }

        if (child_edge->getTo()->getFinal()) {
            // Terminate in the same symbol; success requires y_next==1
            if (y_next == 1u) {
                data->inactive_to = true;
                explore_global_selection_min_max_supremum(0, 0, data);
            } else {
                explore_global_failure_min_max_supremum(data);
            }
            continue;
        }

        // Propagate activation/tracking for the witness token to its successor.
        if (data->old_tracking[i] == 1) {
            data->new_tracking[ii] = 1;
        }
        if (data->old_activation[i] == 1) {
            data->new_activation[ii] = 1;
        }

        data->inactive_to = false;
        data->witness_child_id_to = data->witness_child_id_from;
        data->witness_child_state_id_to = to_state_id;
        data->witness_y_to = y_next;

        explore_global_selection_min_max_supremum(0, 0, data);

        data->new_tracking[ii] = stored_tracking;
        data->new_activation[ii] = stored_activation;
    }
}

static void explore_global_parent_transition_min_max_supremum(data_min_max_supremum_t* data) {
    auto* succs = data->A->getStates()->at(data->parent_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    // Save witness context: each parent edge explores independently
    const bool saved_inactive = data->inactive_from;
    const unsigned int saved_w_child = data->witness_child_id_from;
    const unsigned int saved_w_state = data->witness_child_state_id_from;
    const unsigned int saved_w_y = data->witness_y_from;

    for (Edge* parent_edge : *succs) {
        data->inactive_from = saved_inactive;
        data->witness_child_id_from = saved_w_child;
        data->witness_child_state_id_from = saved_w_state;
        data->witness_y_from = saved_w_y;

        data->parent_state_id_to = static_cast<unsigned int>(parent_edge->getTo()->getId());
        const unsigned int child_id = static_cast<unsigned int>(parent_edge->getWeight()->getValue().to_float());
        data->current_parent_edge_is_real =
            (data->A->getChild(child_id)->getStates()->size() > 1);

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // Silent: no spawn, weight 0
            data->spawned_from = false;
            data->spawned_is_witness = false;
            data->spawned_child_id_from = 0;
            data->spawned_child_state_id_from = 0;
            data->global_edge_weight = 0;
            explore_global_child_transition_min_max_supremum(data);
        } else {
            const unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
            const unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

            // Mark spawned token as active in OLD arrays (so selection sees it)
            const unsigned char prev_act = data->old_activation[ii];
            data->old_activation[ii] = 1;

            // Choice 1: NOT summon as witness (spawn only as background)
            data->spawned_from = true;
            data->spawned_is_witness = false;
            data->spawned_child_id_from = child_id;
            data->spawned_child_state_id_from = summoned_child_state_id;
            data->global_edge_weight = 0;
            explore_global_child_transition_min_max_supremum(data);

            // Choice 2: start tracking as witness (only if no witness already tracked)
            if (saved_inactive) {
                data->global_edge_weight = 1;
                data->inactive_from = false;
                data->witness_child_id_from = child_id;
                data->witness_child_state_id_from = summoned_child_state_id;
                data->witness_y_from = data->finite_is_max ? 0u : 1u;
                data->spawned_from = true;
                data->spawned_is_witness = true;
                data->spawned_child_id_from = child_id;
                data->spawned_child_state_id_from = summoned_child_state_id;
                explore_global_child_transition_min_max_supremum(data);
            }

            data->old_activation[ii] = prev_act;
            data->spawned_from = false;
            data->spawned_is_witness = false;
            data->spawned_child_id_from = 0;
            data->spawned_child_state_id_from = 0;
        }
    }
}

static void explore_global_initialization_min_max_supremum(data_min_max_supremum_t* data) {
    const unsigned int n = data->children_all;

    data->old_activation.resize(n);
    data->old_tracking.resize(n);
    data->new_activation.assign(n, 0);
    data->new_tracking.assign(n, 0);

    for (unsigned int i = 0; i < n; ++i) {
        data->old_activation[i] = (i < data->activation_from.size()) ? (data->activation_from[i] ? 1 : 0) : 0;
        data->old_tracking[i]   = (i < data->tracking_from.size()) ? (data->tracking_from[i] ? 1 : 0) : 0;
    }

    auto* alphabet = data->A->getStates()->at(data->parent_state_id_from)->getAlphabet();
    if (!alphabet) return;

    // Save witness context that must be restored between symbols
    const bool saved_inactive = data->inactive_from;
    const unsigned int saved_w_child = data->witness_child_id_from;
    const unsigned int saved_w_state = data->witness_child_state_id_from;
    const unsigned int saved_w_y = data->witness_y_from;

    for (Symbol* symbol : *alphabet) {
        // Restore witness context for each symbol
        data->inactive_from = saved_inactive;
        data->witness_child_id_from = saved_w_child;
        data->witness_child_state_id_from = saved_w_state;
        data->witness_y_from = saved_w_y;
        data->spawned_from = false;
        data->spawned_is_witness = false;
        data->spawned_child_id_from = 0;
        data->spawned_child_state_id_from = 0;
        data->current_parent_edge_is_real = false;

        data->symbol = symbol;
        explore_global_parent_transition_min_max_supremum(data);
    }
}

// Returns a flattened automaton with 0/1 weights encoding threshold achievement
Automaton* NestedAutomaton::flatten_MinMax_Sup(value_function_t finite_aggregator,
                                               weight_t threshold) {
    unsigned int finite_is_max = 0u;
    if (finite_aggregator == Max_f) {
        finite_is_max = 1u;
    } else if (finite_aggregator == Min_f) {
        finite_is_max = 0u;
    } else {
        QUAK_FAIL("flatten_MinMax_Sup: requires Min_f/Max_f");
    }

    // Flatten child state space
    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->getChildrenSize() + 1; ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int children_all = cumulative_size[this->getChildrenSize()];

    // Epoch reset vector (all ones)
    std::vector<unsigned char> track_them_all(children_all, 1);

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);
    parser->states.insert("@sink@");

    // Sink self-loops on all parent symbols
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

    // Initial: no children active, no obligations discharged yet (tracking=0), no witness
    std::vector<unsigned char> zero(children_all, 0);
    std::string global_initial;
    global_initial.reserve(64 + children_all * 2);
    global_initial.append(std::to_string(this->initial->getId()));
    global_initial.push_back('/');
    global_initial.append(bits_to_string(zero)); // activation
    global_initial.push_back('/');
    global_initial.append(bits_to_string(zero)); // tracking
    global_initial.append("/0/0/@inactive@");

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    std::vector<min_max_sup_work_item> worklist;
    worklist.push_back({
        global_initial,
        (unsigned int)this->initial->getId(),
        zero,
        zero,
        true,
        0u,
        0u,
        finite_is_max ? 0u : 1u,
        MM_SUP_ACC_IDLE,
        false
    });

    // Precompute can_reach_final for doomed-state pruning
    std::vector<bool> crf_all(children_all, false);
    for (unsigned int cid = 0; cid < this->getChildrenSize(); ++cid) {
        auto crf = compute_can_reach_final_child(this->getChild(cid));
        for (unsigned int sid = 0; sid < crf.size(); ++sid) {
            crf_all[cumulative_size[cid] + sid] = crf[sid];
        }
    }

    data_min_max_supremum_t data{};
    data.A = this;
    data.parser = parser;
    data.threshold = threshold;
    data.cumulative_size = cumulative_size.data();
    data.children_all = children_all;
    data.finite_is_max = finite_is_max;
    data.track_them_all = std::move(track_them_all);
    data.worklist = &worklist;
    data.can_reach_final = std::move(crf_all);

    while (!worklist.empty()) {
        min_max_sup_work_item item = std::move(worklist.back());
        worklist.pop_back();

        data.global_from = std::move(item.global_from);
        data.parent_state_id_from = item.parent_state_id_from;
        data.activation_from = std::move(item.activation_from);
        data.tracking_from = std::move(item.tracking_from);
        data.inactive_from = item.inactive_from;
        data.witness_child_id_from = item.witness_child_id_from;
        data.witness_child_state_id_from = item.witness_child_state_id_from;
        data.witness_y_from = item.witness_y_from;
        data.accept_phase_from = item.accept_phase_from;
        data.epoch_nonempty_from = item.epoch_nonempty_from;

        explore_global_initialization_min_max_supremum(&data);
    }

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    return unnested;

#endif

// ===== Live threshold-extremal Sup-Max backend =====
// Source origin:
// - src/NestedAutomaton.cpp:1059-5933
#if 0
enum acc_phase_t : uint8_t { ACC_WAIT_parent = 0, ACC_WAIT_P2EMPTY = 1 };

struct ChildTables {
    ChildAutomaton* child = nullptr;
    uint32_t n_states = 0;
    uint32_t alph = 0;
    uint32_t init = 0;

    struct Trans {
        uint32_t to;
        weight_t w;
    };

    // CSR-like storage for (st, a) -> edges[ off[idx] .. off[idx+1] )
    std::vector<uint32_t> off;   // size = n_states*alph + 1
    std::vector<Trans> edges;

    std::vector<uint8_t> is_final;
    std::vector<uint8_t> live;

    inline uint32_t idx(uint32_t st, uint32_t a) const { return st * alph + a; }
};

static bool build_child_tables(ChildAutomaton* c, ChildTables& out) {
    if (!c) return false;
    out.child = c;
    out.n_states = (uint32_t)c->getStates()->size();
    out.alph = (uint32_t)c->getAlphabet()->size();
    out.init = (uint32_t)c->getInitial()->getId();

    out.is_final.assign(out.n_states, 0);
    for (uint32_t s = 0; s < out.n_states; ++s) {
        State* st = c->getStates()->at(s);
        out.is_final[s] = st->getFinal() ? 1 : 0;
    }

    const size_t cells = (size_t)out.n_states * (size_t)out.alph;
    out.off.assign(cells + 1, 0);

    for (uint32_t s = 0; s < out.n_states; ++s) {
        State* from = c->getStates()->at(s);
        for (uint32_t a = 0; a < out.alph; ++a) {
            SetStd<Edge*>* succs = from->getSuccessors(a);
            out.off[(size_t)out.idx(s, a) + 1] = succs ? (uint32_t)succs->size() : 0;
        }
    }

    for (size_t i = 1; i < out.off.size(); ++i) out.off[i] += out.off[i - 1];
    out.edges.resize(out.off.back());

    std::vector<uint32_t> cur = out.off;
    for (uint32_t s = 0; s < out.n_states; ++s) {
        State* from = c->getStates()->at(s);
        for (uint32_t a = 0; a < out.alph; ++a) {
            SetStd<Edge*>* succs = from->getSuccessors(a);
            if (!succs) continue;
            const uint32_t id = out.idx(s, a);
            for (Edge* e : *succs) {
                if (!e) continue;
                const uint32_t t = (uint32_t)e->getTo()->getId();
                const uint32_t pos = cur[(size_t)id]++;
                out.edges[(size_t)pos] = ChildTables::Trans{t, e->getWeight()->getValue()};
            }
        }
    }

    // Compute liveness via reverse BFS from finals (using State predecessors)
    out.live.assign(out.n_states, 0);
    std::deque<uint32_t> q;
    for (uint32_t s = 0; s < out.n_states; ++s) {
        if (out.is_final[s]) {
            out.live[s] = 1;
            q.push_back(s);
        }
    }
    while (!q.empty()) {
        uint32_t v = q.front(); q.pop_front();
        State* v_state = c->getStates()->at(v);
        for (uint32_t sym = 0; sym < out.alph; ++sym) {
            SetStd<Edge*>* preds = v_state->getPredecessors(sym);
            if (!preds) continue;
            for (Edge* e : *preds) {
                uint32_t u = (uint32_t)e->getFrom()->getId();
                if (!out.live[u]) {
                    out.live[u] = 1;
                    q.push_back(u);
                }
            }
        }
    }

    return true;
}

static inline weight_t acc0_for_finVal(value_function_t finVal) {
    if (finVal == Max_f) {
        return std::numeric_limits<float>::lowest();
    }
    else if (finVal == Min_f) {
        return std::numeric_limits<float>::infinity();
    }
    return weight_t(0);
}

// Check if accumulated value matches guessed return value (with SumB saturation)
static inline bool discharge_ok_finite(value_function_t finVal,
                                      const weight_t& acc,
                                      const weight_t& guess,
                                      const weight_t& bound) {
    if (acc == guess) return true;

    if (finVal == SumB) {
        // Handle positive saturation: guess = +bound, acc overflowed to bound+1
        if (guess == bound && acc == bound + 1) return true;
        // Handle negative saturation: guess = -bound, acc overflowed to -bound-1
        if (guess == -bound && acc == -bound - 1) return true;
        return false;
    }

    return false;
}

// Target-aware liveness: can obligation reach a matching final?
struct LiveKey {
    uint32_t child;
    weight_t guess;

    bool operator<(const LiveKey& o) const {
        if (child != o.child) return child < o.child;
        return guess < o.guess;
    }
};

struct TargetAwareLive {
    // per child: finite domain of accumulator values (closure under transitionFunction)
    std::vector<std::vector<weight_t>> acc_dom;
    std::vector<MapStd<weight_t, uint32_t>> acc_id;
    std::vector<uint32_t> dom_sz;

    // per (child, guess): live table of size n_states * dom_sz[child]
    MapStd<LiveKey, std::vector<uint8_t>> live_tab;

    uint32_t acc_index(uint32_t child, const weight_t& acc) const {
        if (child >= acc_id.size()) return UINT32_MAX;
        const auto& mp = acc_id[child];
        if (!mp.contains(acc)) return UINT32_MAX;
        return mp.at(acc);
    }

    bool is_live(uint32_t child, const weight_t& guess,
                 uint32_t st, const weight_t& acc) const {
        LiveKey k{child, guess};
        if (!live_tab.contains(k)) return true; // if not built, fall back to "no pruning"
        if (child >= dom_sz.size()) return false;
        const uint32_t aidx = acc_index(child, acc);
        if (aidx == UINT32_MAX) return false;
        const auto& vec = live_tab.at(k);
        const uint32_t idx = st * dom_sz[child] + aidx;
        if (idx >= vec.size()) return false;
        return vec[idx] != 0;
    }

    void build(const std::vector<ChildTables>& child_tab,
               const std::vector<SetStd<weight_t>>& child_return_values,
               value_function_t finVal,
               const weight_t& bound) {
        const size_t K = child_tab.size();
        acc_dom.assign(K, {});
        acc_id.assign(K, {});
        dom_sz.assign(K, 0);
        live_tab.clear();

        for (uint32_t i = 0; i < (uint32_t)K; ++i) {
            const ChildTables& T = child_tab[i];
            if (!T.child || T.n_states == 0 || T.alph == 0) continue;
            if (child_return_values[i].empty()) continue;

            // 1) collect edge weights
            SetStd<weight_t> W;
            for (const auto& tr : T.edges) W.insert(tr.w);

            // 2) build finite accumulator domain by closure
            SetStd<weight_t> dom;
            std::deque<weight_t> q;
            const weight_t init_acc = acc0_for_finVal(finVal);
            dom.insert(init_acc);
            q.push_back(init_acc);

            while (!q.empty()) {
                const weight_t acc = q.front(); q.pop_front();
                for (const weight_t& w : W) {
                    const weight_t acc2 = transitionFunction(acc, w, finVal, bound);
                    // auto ins = dom.insert(acc2);
                    // if (ins.second) q.push_back(acc2);
                    if (!dom.contains(acc2)) {
                        dom.insert(acc2);
                        q.push_back(acc2);
                    }
                }
            }

            acc_dom[i].reserve(dom.size());
            uint32_t idx = 0;
            for (const weight_t& a : dom) {
                acc_dom[i].push_back(a);
                acc_id[i].insert(a, idx++);
            }
            dom_sz[i] = (uint32_t)acc_dom[i].size();

            const uint32_t N = T.n_states;
            const uint32_t A = dom_sz[i];
            const uint32_t P = N * A;

            std::vector<std::vector<uint32_t>> rev(P);

            auto node_id = [&](uint32_t st, uint32_t aidx) -> uint32_t {
                return st * A + aidx;
            };

            for (uint32_t st = 0; st < N; ++st) {
                if (T.is_final[st]) continue;
                for (uint32_t aidx = 0; aidx < A; ++aidx) {
                    const weight_t acc = acc_dom[i][aidx];

                    for (uint32_t sym = 0; sym < T.alph; ++sym) {
                        const uint32_t cell = T.idx(st, sym);
                        const uint32_t b = T.off[(size_t)cell];
                        const uint32_t e = T.off[(size_t)cell + 1];

                        for (uint32_t p = b; p < e; ++p) {
                            const auto& tr = T.edges[(size_t)p];
                            const uint32_t to = tr.to;

                            if (to >= N) continue;
                            if (!T.live[to]) continue; // coarse pruning still helpful

                            const weight_t acc2 = transitionFunction(acc, tr.w, finVal, bound);

                            // we do NOT create product-nodes for final states (final = termination),
                            // and wrong-final edges are also "dropped" later via seeding only.
                            if (T.is_final[to]) continue;

                            const uint32_t aidx2 = acc_index(i, acc2);
                            if (aidx2 == UINT32_MAX) continue;

                            const uint32_t u = node_id(st, aidx);
                            const uint32_t v = node_id(to, aidx2);
                            rev[v].push_back(u);
                        }
                    }
                }
            }

            // 4) for each guess: seed nodes that can discharge in ONE step to a matching final,
            //         then reverse-BFS using rev[]
            for (const weight_t& guess : child_return_values[i]) {
                std::vector<uint8_t> live(P, 0);
                std::deque<uint32_t> qq;

                for (uint32_t st = 0; st < N; ++st) {
                    if (T.is_final[st]) continue;
                    for (uint32_t aidx = 0; aidx < A; ++aidx) {
                        const weight_t acc = acc_dom[i][aidx];
                        bool seed = false;

                        // check if there exists an outgoing edge into a FINAL that matches this guess
                        for (uint32_t sym = 0; sym < T.alph && !seed; ++sym) {
                            const uint32_t cell = T.idx(st, sym);
                            const uint32_t b = T.off[(size_t)cell];
                            const uint32_t e = T.off[(size_t)cell + 1];
                            for (uint32_t p = b; p < e; ++p) {
                                const auto& tr = T.edges[(size_t)p];
                                const uint32_t to = tr.to;
                                if (to >= N) continue;
                                if (!T.is_final[to]) continue;

                                const weight_t acc2 = transitionFunction(acc, tr.w, finVal, bound);
                                if (discharge_ok_finite(finVal, acc2, guess, bound)) {
                                    seed = true;
                                    break;
                                }
                            }
                        }

                        if (seed) {
                            const uint32_t u = node_id(st, aidx);
                            if (!live[u]) {
                                live[u] = 1;
                                qq.push_back(u);
                            }
                        }
                    }
                }

                // reverse BFS
                while (!qq.empty()) {
                    uint32_t v = qq.front(); qq.pop_front();
                    for (uint32_t u : rev[v]) {
                        if (!live[u]) {
                            live[u] = 1;
                            qq.push_back(u);
                        }
                    }
                }

                live_tab.insert(LiveKey{i, guess}, std::move(live));
            }
        }
    }
};


// Step obligations forward on symbol
static bool step_obl_bag_finite(value_function_t finVal,
                               const OblBag& in,
                               uint32_t symbol_id,
                               OblBag& out,
                               const std::vector<ChildTables>& child_tab,
                               const TargetAwareLive& live,
                               const weight_t& bound) {
    out.clear();
    if (in.empty()) return true;

    for (const OblEntry& ent : in) {
        const uint32_t i = ent.key.child;
        if (i >= child_tab.size()) return false;
        const ChildTables& T = child_tab[i];
        if (!T.child) return false;
        if (symbol_id >= T.alph) return false;

        bool discharged = false;
        ConfSet next_conf;
        next_conf.reserve(ent.key.conf.size()); // heuristic

        for (const ConfPair& c : ent.key.conf) {
            const uint32_t st = c.st;
            if (!live.is_live(i, ent.key.guess, st, c.acc)) continue;
            if (st >= T.n_states) continue;

            const uint32_t cell = T.idx(st, symbol_id);
            const uint32_t b = T.off[(size_t)cell];
            const uint32_t e = T.off[(size_t)cell + 1];

            for (uint32_t p = b; p < e; ++p) {
                const auto& tr = T.edges[(size_t)p];
                const uint32_t to = tr.to;
                const weight_t acc2 = transitionFunction(c.acc, tr.w, finVal, bound);

                if (T.is_final[to]) {
                    if (discharge_ok_finite(finVal, acc2, ent.key.guess, bound)) {
                        discharged = true;
                        break; // existential: choose this branch and return now
                    }
                    continue; // wrong-return branch => drop
                }

                if (!live.is_live(i, ent.key.guess, to, acc2)) continue; // target-aware pruning
                next_conf.push_back(ConfPair{to, acc2});
            }

            if (discharged) break;
        }

        if (discharged) continue; // this obligation discharged

        conf_canonicalize(next_conf);
        if (next_conf.empty()) return false; // no branch can continue to a valid return

        // set semantics: no multiplicities
        bag_push_back(out, OblEntry{OblKey{i, ent.key.guess, std::move(next_conf)}});
    }

    bag_canonicalize(out);
    return true;
}


// Spawn new obligations when encountering child calls
enum class SpawnStatus : uint8_t { REJECT = 0, EMPTY = 1, NONEMPTY = 2 };

static SpawnStatus spawn_obligation_finite(value_function_t finVal,
                                          uint32_t child_idx,
                                          uint32_t symbol_id,
                                          const weight_t& guess,
                                          OblEntry& spawned,
                                          const std::vector<ChildTables>& child_tab,
                                          const TargetAwareLive& live,
                                          const weight_t& bound) {
    if (child_idx >= child_tab.size()) return SpawnStatus::REJECT;
    const ChildTables& T = child_tab[child_idx];
    if (!T.child) return SpawnStatus::REJECT;
    if (symbol_id >= T.alph) return SpawnStatus::REJECT;

    const uint32_t init = T.init;
    if (init >= T.n_states) return SpawnStatus::REJECT;

    const uint32_t cell = T.idx(init, symbol_id);
    const uint32_t b = T.off[(size_t)cell];
    const uint32_t e = T.off[(size_t)cell + 1];

    ConfSet conf;
    for (uint32_t p = b; p < e; ++p) {
        const auto& tr = T.edges[(size_t)p];
        const uint32_t to = tr.to;
        const weight_t acc2 = transitionFunction(acc0_for_finVal(finVal), tr.w, finVal, bound);

        if (T.is_final[to]) {
            if (discharge_ok_finite(finVal, acc2, guess, bound)) {
                return SpawnStatus::EMPTY; // choose this edge and terminate immediately
            }
            continue; // wrong-return branch => drop
        }

        if (!live.is_live(child_idx, guess, to, acc2)) continue;
        conf.push_back(ConfPair{to, acc2});
    }

    conf_canonicalize(conf);
    if (conf.empty()) return SpawnStatus::REJECT;

    // set semantics: no multiplicities
    spawned = OblEntry{OblKey{child_idx, guess, std::move(conf)}};
    return SpawnStatus::NONEMPTY;
}

// Buchi automaton state for flattening
struct BuchiState_obl {
    State* parent_state;
    weight_t last_guess;
    OblBag P1;
    OblBag P2;
    acc_phase_t phase;

    BuchiState_obl()
        : parent_state(nullptr), last_guess(0), P1(), P2(), phase(ACC_WAIT_parent) {}

    BuchiState_obl(State* p, weight_t g, const OblBag& p1, const OblBag& p2, acc_phase_t ph)
        : parent_state(p), last_guess(g), P1(p1), P2(p2), phase(ph) {}

    bool operator==(const BuchiState_obl& o) const {
        return parent_state == o.parent_state
            && last_guess == o.last_guess
            && P1 == o.P1
            && P2 == o.P2
            && phase == o.phase;
    }

    bool operator<(const BuchiState_obl& o) const {
        if (parent_state != o.parent_state) return parent_state < o.parent_state;
        if (last_guess   != o.last_guess)   return last_guess   < o.last_guess;
        if (P1 != o.P1) return P1 < o.P1;     // lexicographic
        if (P2 != o.P2) return P2 < o.P2;
        return phase < o.phase;
    }
};

static inline acc_phase_t advance_phase(const BuchiState_obl& s) {
    if (s.phase == ACC_WAIT_parent) {
        return (s.parent_state->getFinal()) ? ACC_WAIT_P2EMPTY : ACC_WAIT_parent;
    } else {
        return (s.P2.empty()) ? ACC_WAIT_parent : ACC_WAIT_P2EMPTY;
    }
}

Automaton* NestedAutomaton::flatten_regular(value_function_t finVal, weight_t bound) {
    if (!(finVal == SumB || finVal == Max_f || finVal == Min_f)) {
        QUAK_FAIL("flatten_regular: finVal must be SumB / Max_f / Min_f");
    }

#ifdef OBLBAG_PERF_DEBUG
    // Reset OblBag performance stats for this run
    g_oblbag_stats.reset();
#endif

#ifdef DEBUG
    std::cout << "parent_states=" << this->getStates()->size() << std::endl;
#endif

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    MapArray<Symbol*>* new_alphabet = nullptr;
    MapArray<Weight*>* new_weights  = nullptr;

    weight_t global_min = weight_t(0), global_max = weight_t(0);

    MapStd<BuchiState_obl, State*> state_map;
    std::deque<BuchiState_obl> worklist;
    MapStd<weight_t, Weight*> weight_register;
    unsigned int state_counter = 0;

#ifdef DEBUG
    size_t max_p1_size = 0;
    size_t max_p2_size = 0;
#endif

    // 1) Return values per child
    const size_t k = this->getChildrenSize();
    std::vector<SetStd<weight_t>> child_return_values(k);

    SetStd<weight_t> global_return_values;
    global_return_values.insert(weight_t(SILENT));

    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (!child) {
#ifdef DEBUG
            std::cout << "child[" << i << "] (null)" << std::endl;
#endif
            continue;
        }
        child_return_values[i] = this->computeChildReturnValuesParentAware(i, finVal, bound);
        for (const weight_t& v : child_return_values[i]) global_return_values.insert(v);

#ifdef DEBUG
        std::cout << "child[" << i << "] states=" << child->getStates()->size()
                  << " det=" << (child->isDeterministic() ? 1 : 0)
                  << " return_values={";
        bool first_v = true;
        for (const auto& val : child_return_values[i]) {
            if (!first_v) std::cout << ",";
            std::cout << val;
            first_v = false;
        }
        std::cout << "}" << std::endl;
#endif
    }

    bool have_non_silent = false;
    for (weight_t val : global_return_values) {
        if (val == weight_t(SILENT)) continue;
        if (!have_non_silent) { global_min = val; global_max = val; have_non_silent = true; }
        else { global_min = std::min(global_min, val); global_max = std::max(global_max, val); }
    }
    if (!have_non_silent) { global_min = weight_t(0); global_max = weight_t(0); }

    // 2) Build child tables
    std::vector<ChildTables> child_tab(k);
    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* c = this->getChild(i);
        if (!c) continue;
        if (c->getStates()->size() < 2) continue;
        build_child_tables(c, child_tab[i]);
    }
    TargetAwareLive live;
    live.build(child_tab, child_return_values, finVal, bound);


    // 3) Copy alphabet + build weight objects
    {
        const size_t alph_size = this->getAlphabetSize();
        new_alphabet = new MapArray<Symbol*>(alph_size);
        for (size_t i = 0; i < alph_size; ++i) {
            Symbol* original = this->getAlphabet()->at(i);
            Symbol* copy = new Symbol(original->getName());
            new_alphabet->insert(i, copy);
        }

        new_weights = new MapArray<Weight*>(global_return_values.size());
        weight_register.clear();
        for (weight_t value : global_return_values) {
            Weight* w = new Weight(value);
            new_weights->insert(w->getId(), w);
            weight_register.insert(value, w);
        }
    }

    // 4) Initial Buchi state
    BuchiState_obl init;
    init.parent_state = this->getInitial();
    init.last_guess   = weight_t(INIT_BUCHI_VALUE);
    init.P1.clear();
    init.P2.clear();
    init.phase = ACC_WAIT_parent;

    std::ostringstream ss;
    ss << "b_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);

    state_map[init] = init_state;
    worklist.push_back(init);

    // 5) BFS construction
    OblBag P1_step, P2_step;
    OblBag P1_next, P2_next;

    while (!worklist.empty()) {
        BuchiState_obl current = std::move(worklist.front());
        worklist.pop_front();

#ifdef DEBUG
        if (current.P1.size() > max_p1_size) max_p1_size = current.P1.size();
        if (current.P2.size() > max_p2_size) max_p2_size = current.P2.size();
#endif

        State* current_state = state_map[current];
        const acc_phase_t phase_after_current = advance_phase(current);

        for (unsigned symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            if (!step_obl_bag_finite(finVal, current.P1, symbol_id, P1_step, child_tab, live, bound)) {
                continue;
            }

            if (!current.P2.empty()) {
                if (!step_obl_bag_finite(finVal, current.P2, symbol_id, P2_step, child_tab, live, bound)) {
                    continue;
                }
            } else {
                P2_step.clear();
            }

            SetStd<Edge*>* succs = current.parent_state->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* parent_edge : *succs) {
                State* q_prime = parent_edge->getTo();
                const weight_t pw = parent_edge->getWeight()->getValue();
                const bool is_silent = (pw == weight_t(0));

                if (is_silent) {
                    if (current.P2.empty()) {
                        P1_next.clear();
                        P2_next = P1_step;
                    } else {
                        P1_next = P1_step;
                        P2_next = P2_step;
                    }

                    BuchiState_obl nxt(q_prime, weight_t(SILENT), P1_next, P2_next, phase_after_current);

                    if (!state_map.contains(nxt)) {
                        std::ostringstream s2;
                        s2 << "b_" << state_counter++;
                        State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                        state_map[nxt] = ns;
                        worklist.push_back(nxt);
                    }

                    Weight* w = weight_register.at(weight_t(SILENT));
                    Edge* ne = new Edge(new_alphabet->at(symbol_id), w, current_state, state_map[nxt]);
                    current_state->addSuccessor(ne);
                    state_map[nxt]->addPredecessor(ne);

                } else {
                    const long long cid_ll = (long long)std::llround(pw.to_float());
                    if (cid_ll < 0) continue;
                    const size_t child_index = (size_t)cid_ll;
                    if (child_index >= k) continue;

                    const SetStd<weight_t>& guesses = child_return_values[child_index];
                    const bool boundary = current.P2.empty();

                    for (const weight_t& guess : guesses) {
                        OblEntry spawned;
                        const SpawnStatus st = spawn_obligation_finite(
                            finVal, (uint32_t)child_index, (uint32_t)symbol_id, guess, spawned, child_tab, live, bound
                        );
                        if (st == SpawnStatus::REJECT) continue;

                        if (boundary) {
                            P2_next = P1_step;
                            if (st == SpawnStatus::EMPTY) {
                                P1_next.clear();
                            } else {
                                P1_next.clear();
                                bag_push_back(P1_next, std::move(spawned));
                                bag_canonicalize(P1_next);
                            }
                        } else {
                            P2_next = P2_step;
                            P1_next = P1_step;
                            if (st == SpawnStatus::NONEMPTY) {
                                bag_add_one_sorted(P1_next, std::move(spawned));
                            }
                        }

                        BuchiState_obl nxt(q_prime, guess, P1_next, P2_next, phase_after_current);

                        if (!state_map.contains(nxt)) {
                            std::ostringstream s2;
                            s2 << "b_" << state_counter++;
                            State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                            state_map[nxt] = ns;
                            worklist.push_back(nxt);
                        }

                        Weight* w = weight_register.at(guess);
                        Edge* ne = new Edge(new_alphabet->at(symbol_id), w, current_state, state_map[nxt]);
                        current_state->addSuccessor(ne);
                        state_map[nxt]->addPredecessor(ne);
                    }
                }
            }
        }
    }

    // 6) Build automaton + acceptance
    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [gs, st] : state_map) {
        new_states->insert(st->getId(), st);
        if (gs.phase == ACC_WAIT_P2EMPTY && gs.P2.empty()) {
            st->setFinal(true);
        }
    }

#ifdef DEBUG
    std::cout << "(maxP1, maxP2) = (" << max_p1_size << ", " << max_p2_size << ")" << std::endl;
#endif

#ifdef OBLBAG_PERF_DEBUG
    // Print OblBag performance stats
    g_oblbag_stats.print();
#endif

    std::string name = "BuchiObl(" + this->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights, global_min, global_max, init_state);
}

NestedAutomaton* NestedAutomaton::makeCompleteNested(std::vector<bool>* complete_flags, weight_t parent_sink_w, weight_t child_sink_w) const {
    // determine which automata need completion
    std::vector<bool> local_flags;
    std::vector<bool>* flags = complete_flags;
    if (flags == nullptr) {
        this->isCompleteNested(&local_flags);
        flags = &local_flags;
    }

    bool parent_complete = (*flags)[0];

    // copy alphabet
    Symbol::RESET();
    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(this->getAlphabet()->size());
    for (size_t i = 0; i < this->getAlphabet()->size(); ++i) {
        new_alphabet->insert(i, new Symbol(this->getAlphabet()->at(i)->getName()));
    }

    // copy weights, add sink weight if needed
    Weight::RESET();
    unsigned int parent_sink_weight_id = -1;
    for (size_t i = 0; i < this->getWeights()->size(); ++i) {
        if (this->getWeights()->at(i)->getValue() == parent_sink_w) {
            parent_sink_weight_id = i;
            break;
        }
    }
    size_t num_weights = this->getWeights()->size() + ((parent_sink_weight_id > -1) ? 1 : 0);
    MapArray<Weight*>* new_weights = new MapArray<Weight*>(num_weights);
    for (size_t i = 0; i < this->getWeights()->size(); ++i) {
        new_weights->insert(i, new Weight(this->getWeights()->at(i)->getValue()));

    }
    Weight* parent_sink_weight = nullptr;
    if (!parent_complete) {
        if (parent_sink_weight_id == -1) {
            parent_sink_weight = new Weight(parent_sink_w);
            new_weights->insert(this->getWeights()->size(), parent_sink_weight);
        } else {
            parent_sink_weight = new_weights->at(parent_sink_weight_id);
        }
    }

    // Copy states, add sink state if needed
    State::RESET();
    size_t num_states = this->getStates()->size() + (parent_complete ? 0 : 1);
    MapArray<State*>* new_states = new MapArray<State*>(num_states);
    for (size_t i = 0; i < this->getStates()->size(); ++i) {
        State* os = this->getStates()->at(i);
        State* ns = new State(os->getName(), new_alphabet->size(), 0, this->getChildrenSize() - 1);
        ns->setFinal(os->getFinal());
        new_states->insert(i, ns);
    }
    State* parent_sink = nullptr;
    if (!parent_complete) {
        parent_sink = new State("@sink@", new_alphabet->size(), 0, this->getChildrenSize() - 1);
        parent_sink->setFinal(false);
        new_states->insert(this->getStates()->size(), parent_sink);
    }

    State* new_initial = new_states->at(this->getInitial()->getId());

    // Copy existing parent transitions
    for (size_t sid = 0; sid < this->getStates()->size(); ++sid) {
        State* old_state = this->getStates()->at(sid);
        State* new_from = new_states->at(sid);
        for (size_t a = 0; a < this->getAlphabet()->size(); ++a) {
            SetStd<Edge*>* succs = old_state->getSuccessors(a);
            if (succs) {
                for (Edge* e : *succs) {
                    State* new_to = new_states->at(e->getTo()->getId());
                    Weight* new_w = new_weights->at(e->getWeight()->getId());
                    Edge* new_edge = new Edge(new_alphabet->at(a), new_w, new_from, new_to);
                    new_from->addSuccessor(new_edge);
                    new_to->addPredecessor(new_edge);
                }
            }
        }
    }

    // Add missing transitions to sink (including self-loops on sink)
    if (!parent_complete) {
        for (size_t sid = 0; sid < new_states->size(); ++sid) {
            State* state = new_states->at(sid);
            for (size_t a = 0; a < new_alphabet->size(); ++a) {
                SetStd<Edge*>* succs = state->getSuccessors(a);
                if (!succs || succs->size() == 0) {
                    Edge* sink_edge = new Edge(new_alphabet->at(a), parent_sink_weight, state, parent_sink);
                    state->addSuccessor(sink_edge);
                    parent_sink->addPredecessor(sink_edge);
                }
            }
        }
    }

    // Handle children automata
    MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(this->getChildrenSize());

    for (size_t ci = 0; ci < this->getChildrenSize(); ++ci) {
        ChildAutomaton* child = this->getChild(ci);
        if (!child) continue;

        bool child_complete = (*flags)[ci + 1];

        // Copy child alphabet
        Symbol::RESET();
        MapArray<Symbol*>* child_alpha = new MapArray<Symbol*>(child->getAlphabet()->size());
        for (size_t i = 0; i < child->getAlphabet()->size(); ++i) {
            child_alpha->insert(i, new Symbol(child->getAlphabet()->at(i)->getName()));
        }

        // Copy child weights, add sink weight if needed
        Weight::RESET();
        size_t child_num_weights = child->getWeights()->size() + (child_complete ? 0 : 1);
        MapArray<Weight*>* child_weights = new MapArray<Weight*>(child_num_weights);
        unsigned int child_sink_weight_id = -1;
        for (size_t i = 0; i < child->getWeights()->size(); ++i) {
            child_weights->insert(i, new Weight(child->getWeights()->at(i)->getValue()));
            if (child->getWeights()->at(i)->getValue() == child_sink_w) {
                child_sink_weight_id = i;
            }
        }
        Weight* child_sink_weight = nullptr;
        if (!child_complete) {
            if (child_sink_weight_id == -1) {
                child_sink_weight = new Weight(child_sink_w);
                child_weights->insert(child->getWeights()->size(), child_sink_weight);
            } else {
                child_sink_weight = child_weights->at(child_sink_weight_id);
            }
        }

        // Copy child states, add sink if needed
        State::RESET();
        size_t child_num_states = child->getStates()->size() + (child_complete ? 0 : 1);
        MapArray<State*>* child_states = new MapArray<State*>(child_num_states);
        for (size_t i = 0; i < child->getStates()->size(); ++i) {
            State* os = child->getStates()->at(i);
            State* ns = new State(os->getName(), child_alpha->size(), child->getMinDomain(), child->getMaxDomain());
            ns->setFinal(os->getFinal());
            child_states->insert(i, ns);
        }
        State* child_sink = nullptr;
        if (!child_complete) {
            child_sink = new State("@sink@", child_alpha->size(), child->getMinDomain(), child->getMaxDomain());
            child_sink->setFinal(false);
            child_states->insert(child->getStates()->size(), child_sink);
        }

        // Copy existing child transitions
        for (size_t sid = 0; sid < child->getStates()->size(); ++sid) {
            State* old_state = child->getStates()->at(sid);
            State* new_from = child_states->at(sid);
            for (size_t a = 0; a < child->getAlphabet()->size(); ++a) {
                SetStd<Edge*>* succs = old_state->getSuccessors(a);
                if (succs) {
                    for (Edge* e : *succs) {
                        State* new_to = child_states->at(e->getTo()->getId());
                        Weight* new_w = child_weights->at(e->getWeight()->getId());
                        Edge* new_edge = new Edge(child_alpha->at(a), new_w, new_from, new_to);
                        new_from->addSuccessor(new_edge);
                        new_to->addPredecessor(new_edge);
                    }
                }
            }
        }

        // Add missing transitions to child sink
        if (!child_complete) {
            for (size_t sid = 0; sid < child_states->size(); ++sid) {
                State* state = child_states->at(sid);
                if (state->getFinal()) continue; // no outgoing transitions from final states
                for (size_t a = 0; a < child_alpha->size(); ++a) {
                    SetStd<Edge*>* succs = state->getSuccessors(a);
                    if (!succs || succs->size() == 0) {
                        Edge* sink_edge = new Edge(child_alpha->at(a), child_sink_weight, state, child_sink);
                        state->addSuccessor(sink_edge);
                        child_sink->addPredecessor(sink_edge);
                    }
                }
            }
        }

        State* child_new_init = child_states->at(child->getInitial()->getId());

        ChildAutomaton* new_child = new ChildAutomaton(
            child->getName(),
            child_alpha,
            child_states,
            child_weights,
            child->getMinDomain(),
            child->getMaxDomain(),
            child_new_init
        );
        new_children->insert(ci, new_child);
    }

    return new NestedAutomaton(
        "Complete(" + this->getName() + ")",
        new_alphabet,
        new_states,
        new_weights,
        0,
        this->getChildrenSize() - 1,
        new_initial,
        new_children
    );
}


std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual> NestedAutomaton::generateMacroAlphabet() {
    // Prepare automata list
    std::vector<Automaton*> automata_list;
    automata_list.push_back(const_cast<NestedAutomaton*>(this));
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (child) {
            automata_list.push_back(child);
        }
    }

    // Prepare symbol list
    std::vector<Symbol*> symbol_list;
    for (unsigned int symbol_id = 0; symbol_id < this->getAlphabetSize(); ++symbol_id) {
        symbol_list.push_back(this->getAlphabet()->at(symbol_id));
    }

    // Initialize resolver and alphabet containers
    std::vector<SetStd<Edge*>> resolver(automata_list.size());
    std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual > macro_alphabet;

    // generateResolvers(0, 0, 0, resolver, macro_alphabet, automata_list, symbol_list); // this is already done inside generateMacro
    generateMacro(macro_alphabet, automata_list, symbol_list);

    return macro_alphabet;
}

NestedAutomaton* NestedAutomaton::determinizeWithMacroAlphabet() {
    // Generate macro alphabet
    std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual> macro_alphabet = this->generateMacroAlphabet();

    // Initialize the nested automaton children
    MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(this->getChildrenSize());

    // To get a deterministic ordering from the unordered_set, copy into a vector
    std::vector<MacroSymbol*> macro_list;
    macro_list.reserve(macro_alphabet.size());
    for (MacroSymbol* m : macro_alphabet) {
        macro_list.push_back(m);
    }

    // Build a concrete Symbol alphabet corresponding to the macro_alphabet
    // IDs of new alphabet correspond to indices in macro_list
    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(macro_alphabet.size());
    Symbol::RESET();
    size_t idx = 0;
    for (MacroSymbol* m : macro_list) {
        new_alphabet->insert(idx, new Symbol("a" + std::to_string(idx)));
        ++idx;
    }

    std::vector<MapArray<Symbol*>*> children_alphabet(this->getChildrenSize(), nullptr);
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        Symbol::RESET();
        ChildAutomaton* child = this->getChild(i);
        if (child) {
            MapArray<Symbol*>* child_alpha = new MapArray<Symbol*>(macro_alphabet.size());
            for (size_t mid = 0; mid < macro_list.size(); ++mid) {
                child_alpha->insert(mid, new Symbol("a" + std::to_string(mid)));
            }
            children_alphabet[i] = child_alpha;
        }
    }

    // States stay the same
    State::RESET();
    MapArray<State*>* new_states = new MapArray<State*>(this->getStates()->size());
    for (size_t i = 0; i < this->getStates()->size(); ++i) {
        State* state = new State(this->getStates()->at(i)->getName(), new_alphabet->size(), 0, this->getChildrenSize() - 1);
        state->setFinal(this->getStates()->at(i)->getFinal());
        new_states->insert(i, state);
    }
    State* new_initial = new_states->at(this->getInitial()->getId());

    std::vector<MapArray<State*>*> children_states(this->getChildrenSize(), nullptr);
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        State::RESET();
        ChildAutomaton* child = this->getChild(i);
        if (child) {
            MapArray<State*>* copied_states = new MapArray<State*>(child->getStates()->size());
            for (size_t sid = 0; sid < child->getStates()->size(); ++sid) {
                State* os = child->getStates()->at(sid);
                State* ns = new State(os->getName(), new_alphabet->size(), child->getMinDomain(), child->getMaxDomain());
                ns->setFinal(os->getFinal());
                copied_states->insert(sid, ns);
            }
            children_states[i] = copied_states;
        }
    }

    // Weights stay the same
    Weight::RESET();
    MapArray<Weight*>* new_weights = new MapArray<Weight*>(this->getWeights()->size());
    for (size_t i = 0; i < this->getWeights()->size(); ++i) {
        Weight* weight = new Weight(this->getWeights()->at(i)->getValue());
        new_weights->insert(i, weight);
    }

    std::vector<MapArray<Weight*>*> children_weights(this->getChildrenSize(), nullptr);
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        Weight::RESET();
        ChildAutomaton* child = this->getChild(i);
        if (child) {
            MapArray<Weight*>* copied_weights = new MapArray<Weight*>(child->getWeights()->size());
            for (size_t wid = 0; wid < child->getWeights()->size(); ++wid) {
                Weight* ow = child->getWeights()->at(wid);
                Weight* nw = new Weight(ow->getValue());
                copied_weights->insert(wid, nw);
            }
            children_weights[i] = copied_weights;
        }
    }

    // Build transitions based on macro symbols: for each resolver in each macro symbol, add corresponding edges
    for (size_t macro_id = 0; macro_id < macro_list.size(); ++macro_id) {
        MacroSymbol* macro = macro_list[macro_id];

        // Edges from the parent's resolver
        SetStd<Edge*> edges = macro->getResolver()[0];
        for (Edge* edge : edges) {
            State* from_state = edge->getFrom();
            State* to_state = edge->getTo();

            // Skip edges with invalid state references
            if (from_state->getId() >= new_states->size() || to_state->getId() >= new_states->size()) {
                continue;
            }
            if (edge->getWeight()->getId() >= new_weights->size()) {
                continue;
            }

            Weight* new_weight = new_weights->at(edge->getWeight()->getId());
            Edge* new_edge = new Edge(new_alphabet->at(macro_id), new_weight, new_states->at(from_state->getId()), new_states->at(to_state->getId()));
            new_states->at(from_state->getId())->addSuccessor(new_edge);
            new_states->at(to_state->getId())->addPredecessor(new_edge);
        }

        // Edges from the children's resolvers
        size_t ai = 1; // skip parent at 0
        size_t ci = 1; // skip dummy at 0
        while (ai < macro->getResolver().size() && ci < children_states.size()) {
            const SetStd<Edge*>& edges = macro->getResolver()[ai];
            if (edges.size() == 0) {
                ++ai;
                continue;
            }

            auto* alpha  = children_alphabet[ci];
            auto* wtab   = children_weights[ci];
            auto* states = children_states[ci];

            for (Edge* e : edges) {
                State* from_src = e->getFrom();
                State* to_src   = e->getTo();

                // Skip edges from children with < 2 states (dummy children)
                if (states == nullptr || states->size() < 2) {
                    continue;
                }
                if (from_src->getId() >= states->size() || to_src->getId() >= states->size()) {
                    continue;  // Skip invalid state references
                }

                State* from = states->at(from_src->getId());
                State* to   = states->at(to_src->getId());
                if (e->getWeight()->getId() >= wtab->size()) {
                    continue;  // Skip invalid weight references
                }
                auto* w     = wtab->at(e->getWeight()->getId());

                Edge* new_e = new Edge(alpha->at(macro_id), w, from, to);
                from->addSuccessor(new_e);
                to->addPredecessor(new_e);
            }

            // We consumed one non-empty resolver bucket for this child
            ++ai;
            ++ci;
        }
    }

    // // print all transitions for debugging
    // std::cout << "Determinized Nested Automaton Transitions:" << std::endl;
    // for (size_t sid = 0; sid < new_states->size(); ++sid) {
    //     State* s = new_states->at(sid);
    //     for (size_t a = 0; a < new_alphabet->size(); ++a) {
    //         SetStd<Edge*>* succs = s->getSuccessors(a);
    //         if (succs) {
    //             for (Edge* e : *succs) {
    //                 std::cout << "From state " << s->getName() << " to state " << e->getTo()->getName() << " on symbol " << new_alphabet->at(a)->getName() << " with weight " << e->getWeight()->getValue() << std::endl;
    //             }
    //         }
    //     }
    // }
    // // print all children transitions for debugging
    // for (size_t ci = 0; ci < this->getChildrenSize(); ++ci) {
    //     ChildAutomaton* child = this->getChild(ci);
    //     if (child) {
    //         std::cout << "Child Automaton " << child->getName() << " Transitions:" << std::endl;
    //         MapArray<State*>* cstates = children_states[ci];
    //         for (size_t sid = 0; sid < cstates->size(); ++sid) {
    //             State* s = cstates->at(sid);
    //             for (size_t a = 0; a < children_alphabet[ci]->size(); ++a) {
    //                 SetStd<Edge*>* succs = s->getSuccessors(a);
    //                 if (succs) {
    //                     for (Edge* e : *succs) {
    //                         std::cout << "From state " << s->getName() << " to state " << e->getTo()->getName() << " on symbol " << children_alphabet[ci]->at(a)->getName() << " with weight " << e->getWeight()->getValue() << std::endl;
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // }

    // Construct children automata
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (child) {
            State* new_init = children_states[i]->at(child->getInitial()->getId());

            ChildAutomaton* new_child = new ChildAutomaton(
                child->getName(),
                children_alphabet[i],
                children_states[i],
                children_weights[i],
                child->getMinDomain(),
                child->getMaxDomain(),
                new_init
            );

            new_children->insert(i, new_child);
        }
    }

    // Construct and return the new nested automaton
    NestedAutomaton* det_nwa = new NestedAutomaton("PsuedoDet(" + this->getName() + ")", new_alphabet, new_states, new_weights, 0, this->getChildrenSize() - 1, new_initial, new_children);
    // det_nwa->print();
    // NOTE: det_nwa is not complete by default, but this is not necessary for the (limavg,summinus)-emptiness pipeline
    // NestedAutomaton* complete_det_nwa = det_nwa->makeCompleteNested();

    for (MacroSymbol* m : macro_alphabet) delete m;
    macro_alphabet.clear();

    // delete det_nwa;
    // return complete_det_nwa;

    return det_nwa;
}

// Synchronization + Ultimate child construction
// NOTE: Output has no marked initial state because it has many disconnected components, so its SCCs are not procesed 
NestedAutomaton* NestedAutomaton::synchronizeChildren() {
    // UTILITY LAMBDAS
    auto sat_mul_u64 = [](uint64_t a, uint64_t b) -> uint64_t {
        if (a == 0 || b == 0) return 0;
        if (a > std::numeric_limits<uint64_t>::max() / b) {
            QUAK_FAIL("Overflow in sat_mul_u64 during synchronizeChildren");
        }
        return a * b;
    };

    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e;
        return nullptr;
    };

    // Reads the payload on a parent edge in the *input* automaton (child-id encoding).
    // Returns <=0 for silent, >0 for called child-id.
    auto old_parent_called_child_id = [](const Edge* me) -> long long {
        if (!me || !me->getWeight()) return 0;
        // The project historically used to_float() for decoding.
        // We round to nearest integer to be robust to "1.0" style encodings.
        const double d = me->getWeight()->getValue().to_float();
        return static_cast<long long>(std::llround(d));
    };

    // COMPUTE CONFIGURATION BOUND X = 2 * conf(this)
    // Compute |Q_slv| = sum of states across all child automata
    uint64_t Qslv_size = 0;
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* c = this->getChild(i);
        Qslv_size += static_cast<uint64_t>(c->getStates()->size());
    }

    // Check for overflow in 2^|Q_slv|
    if (Qslv_size >= 64) {
        QUAK_FAIL("Overflow: |Q_slv| >= 64, configuration bound too large");
    }
    const uint64_t two_power_Qslv = 1ULL << Qslv_size;

    // conf(A) = |Q_m| * 2^|Q_slv|
    const uint64_t nm = static_cast<uint64_t>(this->getStates()->size());
    const uint64_t conf = sat_mul_u64(nm, two_power_Qslv);

    const uint64_t X_u64 = sat_mul_u64(2, conf);

    // BASIC SIZES, ALPHABET SOURCE
    const size_t M = this->getStates()->size();

    // Use any (non-null) child's alphabet as the canonical alphabet.
    // (Assumes all automata use the same alphabet IDs.)
    ChildAutomaton* alpha_src_child = nullptr;
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        if (this->getChild(i) && this->getChild(i)->getNbStates() > 1 && this->getChild(i)->getAlphabet()) {
            alpha_src_child = this->getChild(i);
            break;
        }
    }
    if (!alpha_src_child) QUAK_FAIL("synchronizeChildren: no child alphabet available");

    MapArray<Symbol*>* src_alpha = alpha_src_child->getAlphabet();
    const size_t A = src_alpha->size();

    // BUILD THE SYNCHRONIZED ULTIMATE child U_sync (single child)
    Symbol::RESET();
    State::RESET();
    Weight::RESET();

    // Copy alphabet for U_sync
    MapArray<Symbol*>* calpha = new MapArray<Symbol*>(A);
    for (size_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(src_alpha->at(a));
        calpha->insert(s_new->getId(), s_new);
    }

    // Compute weight statistics over all real children (id > 0)
    bool firstW = true;
    weight_t minW = weight_t(0), maxW = weight_t(0), maxAbsW = weight_t(0);

    for (size_t ci = 1; ci < this->getChildrenSize(); ++ci) {
        ChildAutomaton* child = this->getChild(ci);
        if (!child) continue;
        MapArray<Weight*>* ws = child->getWeights();
        if (!ws) continue;

        for (size_t wi = 0; wi < ws->size(); ++wi) {
            Weight* w = ws->at(wi);
            if (!w) continue;
            const weight_t v = w->getValue();

            if (firstW) { minW = maxW = v; firstW = false; }
            else {
                if (v < minW) minW = v;
                if (v > maxW) maxW = v;
            }

            const weight_t av = (v < weight_t(0)) ? -v : v;
            if (av > maxAbsW) maxAbsW = av;
        }
    }

    // If there were no real weights found, treat as all 0.
    if (firstW) { minW = maxW = maxAbsW = weight_t(0); }

    // Accumulator bound cap = X * maxAbsW
    const weight_t cap = (maxAbsW == weight_t(0))
        ? weight_t(0)
        : weight_t(static_cast<double>(X_u64)) * maxAbsW;

    weight_t accMin = -cap;
    weight_t accMax =  cap;

    // For SumMinus (all weights <= 0), we can tighten to [ -cap, 0 ].
    if (cap == weight_t(0)) {
        accMin = accMax = weight_t(0);
    } else if (maxW <= weight_t(0)) {
        accMin = -cap;
        accMax = weight_t(0);
    } else if (minW >= weight_t(0)) {
        accMin = weight_t(0);
        accMax = cap;
    }

    // Output weights are either 0 (silent parent step) or acc + child_weight (non-silent)
    const weight_t outMinBound = std::min(weight_t(0), accMin + minW);
    const weight_t outMaxBound = std::max(weight_t(0), accMax + maxW);

    // Weight register for U_sync outputs
    std::vector<Weight*> cweights_vec;
    MapStd<weight_t, Weight*> weight_register;

    auto get_weight = [&](weight_t v) -> Weight* {
        if (!weight_register.contains(v)) {
            Weight* w = new Weight(v);
            cweights_vec.push_back(w);
            weight_register.insert(v, w);
        }
        return weight_register.at(v);
    };
    // get_weight(weight_t(0)); // ensure 0 exists TODO: CHECK

    // State space of U_sync, explored on-demand
    struct SyncKey {
        uint32_t parent_id;       // parent state at the *same time step*
        uint32_t child_index;     // which original child component (ultimate-child selector)
        uint32_t child_state_id;  // local state in that child
        weight_t accumulator;     // buffered sum waiting to flush
        bool pending_accept;      // child reached accept on silent parent step, waiting to flush

        bool operator==(const SyncKey& o) const {
            return parent_id == o.parent_id
                && child_index == o.child_index
                && child_state_id == o.child_state_id
                && accumulator == o.accumulator
                && pending_accept == o.pending_accept;
        }
    };

    struct SyncKeyHash {
        size_t operator()(const SyncKey& k) const noexcept {
            size_t h = 1469598103934665603ull;
            auto mix64 = [&](uint64_t x) { h ^= x; h *= 1099511628211ull; };

            mix64(static_cast<uint64_t>(k.parent_id));
            mix64(static_cast<uint64_t>(k.child_index));
            mix64(static_cast<uint64_t>(k.child_state_id));
            mix64(static_cast<uint64_t>(std::hash<weight_t>{}(k.accumulator)));
            mix64(static_cast<uint64_t>(k.pending_accept ? 1 : 0));
            return h;
        }
    };

    std::unordered_map<SyncKey, State*, SyncKeyHash> state_map;
    std::queue<SyncKey> worklist;

    std::vector<State*> cstates_vec;

    auto child_is_final = [&](uint32_t child_index, uint32_t local_sid) -> bool {
        ChildAutomaton* c = this->getChild(child_index);
        if (!c) return false;
        State* s = c->getStates()->at(local_sid);
        return s && s->getFinal();
    };

    auto get_or_make_state = [&](const SyncKey& key) -> State* {
        auto it = state_map.find(key);
        if (it != state_map.end()) return it->second;

        std::ostringstream ss;
        ss << "U_sync_m" << key.parent_id
           << "_c" << key.child_index
           << "_s" << key.child_state_id
           << "_a" << key.accumulator
           << (key.pending_accept ? "_P" : "");

        State* ns = new State(ss.str(), A, outMinBound, outMaxBound);
        cstates_vec.push_back(ns);

        // FINAL iff underlying local state is accepting AND we are not pending (flushed).
        if (child_is_final(key.child_index, key.child_state_id) && !key.pending_accept) {
            ns->setFinal(true);
        }

        state_map.insert({key, ns});
        worklist.push(key);
        return ns;
    };

    // Seed initial states of U_sync for every *calling* parent transition.
    // Also compute the new parent payload table: -1 for silent, else init_state_id in U_sync.
    std::vector<std::vector<long long>> new_parent_payload(
        M, std::vector<long long>(A, -1LL)
    );

    // We remember at least one created "real" seed state to use as the formal initial of U_sync.
    State* some_seed_initial = nullptr;

    for (uint32_t mid = 0; mid < static_cast<uint32_t>(M); ++mid) {
        State* m_state = this->getStates()->at(mid);

        for (uint32_t a = 0; a < static_cast<uint32_t>(A); ++a) {
            Edge* me = first_edge_or_null(m_state->getSuccessors(a));
            if (!me) continue;

            const long long cid = old_parent_called_child_id(me);

            if (cid <= 0) {
                // silent in the input -> silent in the output, encoded as -1
                new_parent_payload[mid][a] = -1;
                continue;
            }

            const uint32_t child_index = static_cast<uint32_t>(cid);
            // if (child_index >= this->getChildrenSize()) QUAK_FAIL("synchronizeChildren: parent calls out-of-range child id");

            ChildAutomaton* child = this->getChild(child_index);
            // if (!child) QUAK_FAIL("synchronizeChildren: null child called by parent");

            const uint32_t s0 = static_cast<uint32_t>(child->getInitial()->getId());

            // The spawn state must encode the parent state at the call site (mid).
            SyncKey start{mid, child_index, s0, weight_t(0), false};
            State* start_state = get_or_make_state(start);

            new_parent_payload[mid][a] = static_cast<long long>(start_state->getId());

            if (!some_seed_initial) some_seed_initial = start_state;
        }
    }

    // // If the parent never calls any child, build a harmless 1-state U_sync.
    // if (!some_seed_initial) {
    //     // Create a single non-final state with 0 self-loops.
    //     SyncKey dead{0, 1, 0, weight_t(0), false}; // child_index/child_state_id unused
    //     State* dead_state = new State("U_sync_dead", A, outMinBound, outMaxBound);
    //     cstates_vec.push_back(dead_state);
    //     some_seed_initial = dead_state;

    //     for (uint32_t a = 0; a < static_cast<uint32_t>(A); ++a) {
    //         Weight* w0 = get_weight(weight_t(0));
    //         Edge* e = new Edge(calpha->at(a), w0, dead_state, dead_state);
    //         dead_state->addSuccessor(e);
    //         dead_state->addPredecessor(e);
    //     }

    //     // No worklist exploration needed.
    //     worklist = std::queue<SyncKey>();
    //     state_map.clear();
    // }

    // BFS exploration of U_sync transitions
    while (!worklist.empty()) {
        SyncKey cur = worklist.front();
        worklist.pop();

        // Terminality: final iff (accepting && !pending)
        if (child_is_final(cur.child_index, cur.child_state_id) && !cur.pending_accept) {
            continue;
        }

        State* cur_node = state_map.at(cur);

        State* m_state = this->getStates()->at(cur.parent_id);
        ChildAutomaton* child = this->getChild(cur.child_index);
        if (!child) QUAK_FAIL("synchronizeChildren: null child during exploration");

        State* s_state = child->getStates()->at(cur.child_state_id);

        for (uint32_t a = 0; a < static_cast<uint32_t>(A); ++a) {
            Edge* parent_edge = first_edge_or_null(m_state->getSuccessors(a));
            if (!parent_edge) continue;

            const long long cid = old_parent_called_child_id(parent_edge);
            const bool parent_is_silent = (cid <= 0);

            const uint32_t next_parent_id = static_cast<uint32_t>(parent_edge->getTo()->getId());

            // child step
            Edge* child_edge = nullptr;
            uint32_t next_child_state_id = cur.child_state_id;
            weight_t child_w = weight_t(0);

            if (!cur.pending_accept) {
                child_edge = first_edge_or_null(s_state->getSuccessors(a));
                if (!child_edge) continue;

                next_child_state_id = static_cast<uint32_t>(child_edge->getTo()->getId());
                child_w = child_edge->getWeight()->getValue();
            }
            // else: frozen in accepting state; no step; child_w = 0; next_child_state_id stays

            if (parent_is_silent) {
                // Emit 0, accumulate child_w, possibly set pending_accept
                const weight_t new_acc = cur.accumulator + child_w;

                if (new_acc < accMin || new_acc > accMax) {
                    // out of allowed accumulator range
                    continue;
                }

                bool new_pending = cur.pending_accept;
                if (!cur.pending_accept && child_edge && child_edge->getTo()->getFinal()) {
                    new_pending = true;
                }

                SyncKey nxt{next_parent_id, cur.child_index, next_child_state_id, new_acc, new_pending};
                State* nxt_node = get_or_make_state(nxt);

                Weight* w0 = get_weight(weight_t(0));
                Edge* e = new Edge(calpha->at(a), w0, cur_node, nxt_node);
                cur_node->addSuccessor(e);
                nxt_node->addPredecessor(e);
            } else {
                // Emit acc + child_w, reset accumulator, clear pending
                const weight_t emit = cur.accumulator + child_w;
                Weight* w_out = get_weight(emit);

                SyncKey nxt{next_parent_id, cur.child_index, next_child_state_id, weight_t(0), false};
                State* nxt_node = get_or_make_state(nxt);

                Edge* e = new Edge(calpha->at(a), w_out, cur_node, nxt_node);
                cur_node->addSuccessor(e);
                nxt_node->addPredecessor(e);
            }
        }
    }

    // finalize U_sync weights min/max
    weight_t realMin = weight_t(0), realMax = weight_t(0);
    bool firstOut = true;
    for (Weight* w : cweights_vec) {
        const weight_t v = w->getValue();
        if (firstOut) { realMin = realMax = v; firstOut = false; }
        else {
            if (v < realMin) realMin = v;
            if (v > realMax) realMax = v;
        }
    }
    if (firstOut) { realMin = realMax = weight_t(0); }

    MapArray<State*>*  cstates  = new MapArray<State*>(cstates_vec.size());
    MapArray<Weight*>* cweights = new MapArray<Weight*>(cweights_vec.size());
    for (State* st : cstates_vec)   cstates->insert(st->getId(), st);
    for (Weight* wt : cweights_vec) cweights->insert(wt->getId(), wt);

    ChildAutomaton* ultimate_synced = new ChildAutomaton(
        "U_sync(" + this->getName() + ")",
        calpha, cstates, cweights,
        realMin, realMax,
        nullptr
    );

    MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(1);
    new_children->insert(0, ultimate_synced);

    // REBUILD THE PARENT WITH NEW PAYLOADS
    // We must rebuild because we need weights -1 / initStateId, and we cannot mutate
    // the old weight list safely.

    Symbol::RESET();
    State::RESET();
    Weight::RESET();

    // Copy parent alphabet
    MapArray<Symbol*>* malpha = new MapArray<Symbol*>(A);
    for (size_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(src_alpha->at(a));
        malpha->insert(s_new->getId(), s_new);
    }

    // Copy parent states
    MapArray<State*>* mstates = new MapArray<State*>(M);
    for (uint32_t sid = 0; sid < static_cast<uint32_t>(M); ++sid) {
        State* ns = new State(this->getStates()->at(sid));
        mstates->insert(ns->getId(), ns);
    }
    State* minitial = mstates->at(this->getInitial()->getId());

    // Copy finals
    SetStd<State*>* mfinals = new SetStd<State*>();
    for (uint32_t sid = 0; sid < static_cast<uint32_t>(M); ++sid) {
        State* os = this->getStates()->at(sid);
        if (os->getFinal()) {
            State* ns = mstates->at(sid);
            ns->setFinal(true);
            mfinals->insert(ns);
        }
    }

    // Parent weights are just payload integers (-1 or U_sync state id).
    std::vector<Weight*> mweights_vec;
    MapStd<long long, Weight*> mweight_reg;

    auto get_mweight = [&](long long payload) -> Weight* {
        if (!mweight_reg.contains(payload)) {
            Weight* w = new Weight(weight_t(static_cast<double>(payload)));
            mweights_vec.push_back(w);
            mweight_reg.insert(payload, w);
        }
        return mweight_reg.at(payload);
    };

    // Create edges
    for (uint32_t sid = 0; sid < static_cast<uint32_t>(M); ++sid) {
        State* os = this->getStates()->at(sid);
        State* nf = mstates->at(sid);

        for (uint32_t a = 0; a < static_cast<uint32_t>(A); ++a) {
            Edge* oe = first_edge_or_null(os->getSuccessors(a));
            if (!oe) continue;

            State* nt = mstates->at(static_cast<uint32_t>(oe->getTo()->getId()));

            const long long payload = new_parent_payload[sid][a]; // -1 or U_sync init state id
            Weight* nw = get_mweight(payload);

            Edge* ne = new Edge(malpha->at(a), nw, nf, nt);
            nf->addSuccessor(ne);
            nt->addPredecessor(ne);
        }
    }

    // Finalize parent weight array and min/max (purely informational)
    MapArray<Weight*>* mweights = new MapArray<Weight*>(mweights_vec.size());
    weight_t pMin = weight_t(0), pMax = weight_t(0);
    bool firstP = true;
    for (Weight* w : mweights_vec) {
        mweights->insert(w->getId(), w);
        const weight_t v = w->getValue();
        if (firstP) { pMin = pMax = v; firstP = false; }
        else {
            if (v < pMin) pMin = v;
            if (v > pMax) pMax = v;
        }
    }
    if (firstP) { pMin = pMax = weight_t(0); }

    // BUILD RESULT NESTED AUTOMATON
    NestedAutomaton* result = new NestedAutomaton(
        "Sync(" + this->getName() + ")",
        malpha, mstates, mweights,
        pMin, pMax,
        minitial,
        new_children
    );
    // result->print();

    return result;
}

// c_bound = 2 * (|Q_slv| + 2) * conf(A) * |Q_slv|^{2*|Q_slv|}
static uint64_t compute_c_bound(const NestedAutomaton* A_det_pre_sync) {
    if (!A_det_pre_sync) QUAK_FAIL("compute_c_bound: null automaton");

    auto sat_mul_u64 = [](uint64_t a, uint64_t b) -> uint64_t {
        if (a == 0 || b == 0) return 0;
        if (a > std::numeric_limits<uint64_t>::max() / b) {
            QUAK_FAIL("Overflow in sat_mul_u64 during compute_c_bound");
        }
        return a * b;
    };

    auto sat_pow_u64 = [&](uint64_t base, uint64_t exp) -> uint64_t {
        uint64_t r = 1;
        while (exp > 0) {
            if (exp & 1ULL) r = sat_mul_u64(r, base);   // will QUAK_FAIL on overflow
            exp >>= 1ULL;
            if (exp) base = sat_mul_u64(base, base);    // will QUAK_FAIL on overflow
        }
        return r;
    };

    auto sat_pow2_u64 = [&](uint64_t exp) -> uint64_t {
        // Shifting by >= 64 is undefined for uint64_t.
        if (exp >= 64ULL) {
            QUAK_FAIL("Overflow in sat_pow2_u64 during compute_c_bound");
        }
        return (1ULL << exp);
    };

    // |Q_m|
    const uint64_t Qm = static_cast<uint64_t>(A_det_pre_sync->getStates()->size());

    // |Q_slv| = sum of sizes of child state sets (disjoint union).
    uint64_t Qslv = 0;
    for (size_t i = 1; i < A_det_pre_sync->getChildrenSize(); ++i) {
        ChildAutomaton* c = A_det_pre_sync->getChild(i);
        if (!c) continue;
        const uint64_t nc = static_cast<uint64_t>(c->getStates()->size());
        if (Qslv > std::numeric_limits<uint64_t>::max() - nc) {
            Qslv = std::numeric_limits<uint64_t>::max();
            break;
        }
        Qslv += nc;
    }

    if (Qslv == 0) {
        return 0; // no real children, multiplicity bound is irrelevant
    }

    // upper bound on number of configurations:
    // conf(A) <= |Q_m| * 2^{|Q_slv|}
    const uint64_t conf_upper = sat_mul_u64(Qm, sat_pow2_u64(Qslv));

    // N = (|Q_slv| + 2) * conf(A) * |Q_slv|^{2|Q_slv|}
    const uint64_t pow_part = sat_pow_u64(Qslv, sat_mul_u64(2ULL, Qslv));
    uint64_t N = sat_mul_u64(Qslv + 2ULL, conf_upper);
    N = sat_mul_u64(N, pow_part);

    // c_bound = 2 * N
    return sat_mul_u64(2ULL, N);
}

// Assumptions:
//  - "this" NWA is (pseudo-)deterministic: <= 1 edge per letter from any state in parent and in the unique child.
//  - "this" has only one child -- the synchronized ultimate child U_sync.
//  - parent edge weight payload: <0 means SILENT, >=0 means spawn from that U_sync state-id.
Automaton* NestedAutomaton::flatten_Avg_SumMinus(uint64_t c_bound) {
    // helpers
    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e;
        return nullptr;
    };

    auto parent_payload_ll = [](const Edge* me) -> long long {
        if (!me || !me->getWeight()) return -1;
        const double d = me->getWeight()->getValue().to_float();
        return static_cast<long long>(std::llround(d));
    };

    // sanity
    if (this->getChildrenSize() != 1) {
        QUAK_FAIL("flatten_Avg_SumMinus: expected exactly one child (ultimate synchronized child)");
    }
    ChildAutomaton* U = this->getChild(0);
    if (!U) QUAK_FAIL("flatten_Avg_SumMinus: null ultimate child");

    MapArray<State*>* mstates_src = this->getStates();
    const uint32_t M = static_cast<uint32_t>(mstates_src->size());

    MapArray<State*>* ustates_src = U->getStates();
    const uint32_t UQ = static_cast<uint32_t>(ustates_src->size());

    MapArray<Symbol*>* alpha_src = U->getAlphabet();
    if (!alpha_src) QUAK_FAIL("flatten_Avg_SumMinus: null alphabet in ultimate child");
    const uint32_t A = static_cast<uint32_t>(alpha_src->size());

    // precompute parent step table
    struct MStep {
        bool exists = false;
        uint32_t to = 0;
        long long payload = -1; // <0 silent, >=0 spawn-id
    };
    std::vector<std::vector<MStep>> mstep(M, std::vector<MStep>(A));

    for (uint32_t ms = 0; ms < M; ++ms) {
        State* s = mstates_src->at(ms);
        for (uint32_t a = 0; a < A; ++a) {
            Edge* e = first_edge_or_null(s->getSuccessors(a));
            if (!e) continue;
            mstep[ms][a].exists = true;
            mstep[ms][a].to = static_cast<uint32_t>(e->getTo()->getId());
            mstep[ms][a].payload = parent_payload_ll(e);
        }
    }

    // precompute child step table
    struct UStep {
        bool exists = false;
        uint32_t to = 0;
        weight_t w = weight_t(0);
        bool to_final = false;
    };
    std::vector<std::vector<UStep>> ustep(UQ, std::vector<UStep>(A));

    for (uint32_t us = 0; us < UQ; ++us) {
        State* s = ustates_src->at(us);
        const bool s_final = s && s->getFinal();
        // finals have no outgoing
        if (s_final) continue;

        for (uint32_t a = 0; a < A; ++a) {
            Edge* e = first_edge_or_null(s->getSuccessors(a));
            if (!e) continue;
            ustep[us][a].exists = true;
            ustep[us][a].to = static_cast<uint32_t>(e->getTo()->getId());
            ustep[us][a].w = e->getWeight()->getValue();
            ustep[us][a].to_final = (e->getTo() && e->getTo()->getFinal());
        }
    }

    // compute weight bounds for State constructor
    weight_t uMin = weight_t(0), uMax = weight_t(0);
    bool firstW = true;
    if (U->getWeights()) {
        for (size_t i = 0; i < U->getWeights()->size(); ++i) {
            Weight* w = U->getWeights()->at(i);
            if (!w) continue;
            const weight_t v = w->getValue();
            if (firstW) { uMin = uMax = v; firstW = false; }
            else {
                if (v < uMin) uMin = v;
                if (v > uMax) uMax = v;
            }
        }
    }
    if (firstW) { uMin = uMax = weight_t(0); }

    // Worst-case per-step sum <= (total active + 1 spawn) * maxAbs.
    weight_t maxAbs = (uMax < weight_t(0)) ? -uMin : std::max(uMax, (uMin < weight_t(0) ? -uMin : uMin));
    const double factor = static_cast<double>(c_bound) * static_cast<double>(UQ) + 1.0;
    weight_t outAbs = weight_t(factor) * maxAbs;

    weight_t outMinBound = (uMin >= weight_t(0)) ? weight_t(0) : -outAbs;
    weight_t outMaxBound = (uMax <= weight_t(0)) ? weight_t(0) : outAbs;

    // build new alphabet
    Symbol::RESET();
    State::RESET();
    Weight::RESET();

    MapArray<Symbol*>* falpha = new MapArray<Symbol*>(A);
    for (uint32_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(alpha_src->at(a));
        falpha->insert(s_new->getId(), s_new);
    }

    // weight register
    std::vector<Weight*> fweights_vec;
    MapStd<weight_t, Weight*> wreg;

    auto get_w = [&](const weight_t& v) -> Weight* {
        if (!wreg.contains(v)) {
            Weight* w = new Weight(v);
            fweights_vec.push_back(w);
            wreg.insert(v, w);
        }
        return wreg.at(v);
    };

    Weight* w_silent = get_w(SILENT); // ensure SILENT exists

    // key type: (parent_id, sparse multiset)
    // mult stored as [sid0,cnt0,sid1,cnt1,...] sorted by sid, only cnt>0.
    struct Key {
        uint32_t mid;
        std::vector<uint32_t> nz;

        bool operator==(const Key& o) const {
            return mid == o.mid && nz == o.nz;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& k) const noexcept {
            size_t h = 1469598103934665603ull;
            auto mix = [&](uint64_t x) {
                h ^= static_cast<size_t>(x);
                h *= 1099511628211ull;
            };
            mix(k.mid);
            for (uint32_t x : k.nz) mix(x);
            return h;
        }
    };

    auto encode_name = [&](const Key& k) -> std::string {
        std::ostringstream ss;
        ss << "F_m" << k.mid << "_";
        for (size_t i = 0; i + 1 < k.nz.size(); i += 2) {
            ss << k.nz[i] << ":" << k.nz[i+1] << ",";
        }
        return ss.str();
    };

    // state table + BFS
    std::unordered_map<Key, State*, KeyHash> stmap;
    std::queue<Key> work;

    std::vector<State*> fstates_vec;
    SetStd<State*>* ffinals = new SetStd<State*>();

    auto parent_is_final = [&](uint32_t mid) -> bool {
        State* ms = mstates_src->at(mid);
        return ms && ms->getFinal();
    };

    auto get_or_make = [&](const Key& k) -> State* {
        auto it = stmap.find(k);
        if (it != stmap.end()) return it->second;

        State* ns = new State(encode_name(k), A, outMinBound, outMaxBound);
        fstates_vec.push_back(ns);

        if (parent_is_final(k.mid)) {
            ns->setFinal(true);
            ffinals->insert(ns);
        }

        stmap.insert({k, ns});
        work.push(k);
        return ns;
    };

    // initial: (parent_initial, empty multiset)
    Key init;
    init.mid = static_cast<uint32_t>(this->getInitial()->getId());
    init.nz.clear();

    State* finitial = get_or_make(init);

    // Transition computation: apply letter a to sparse multiset, with optional spawn.
    auto step_multiset = [&](const std::vector<uint32_t>& nz_in,
                             bool do_spawn, uint32_t spawn_sid,
                             uint32_t a,
                             weight_t& sum_out,
                             std::vector<uint32_t>& nz_out) -> bool
    {
        sum_out = weight_t(0);

        // accumulate counts by current sid
        // (use unordered_map because nz_in is sparse)
        std::unordered_map<uint32_t, uint64_t> cur;
        cur.reserve(nz_in.size() / 2 + 2);

        for (size_t i = 0; i + 1 < nz_in.size(); i += 2) {
            cur[nz_in[i]] += static_cast<uint64_t>(nz_in[i + 1]);
        }
        if (do_spawn) {
            if (spawn_sid >= UQ) return false;
            cur[spawn_sid] += 1ull;
        }

        // advance each active bucket
        std::unordered_map<uint32_t, uint64_t> nxt;
        nxt.reserve(cur.size() + 4);

        for (const auto& kv : cur) {
            const uint32_t sid = kv.first;
            const uint64_t cnt = kv.second;
            if (cnt == 0) continue;

            if (sid >= UQ) return false;
            if (!ustep[sid][a].exists) return false;

            const UStep& st = ustep[sid][a];

            // sum += cnt * st.w
            sum_out = sum_out + (weight_t(static_cast<double>(cnt)) * st.w);

            // drop instances that terminate (reach final)
            if (st.to_final) continue;

            uint64_t& ref = nxt[st.to];
            ref += cnt;

            // enforce per-state cap <= c_bound
            if (ref > c_bound) return false;
        }

        // build canonical nz_out sorted by sid
        std::vector<std::pair<uint32_t, uint64_t>> tmp;
        tmp.reserve(nxt.size());
        for (const auto& kv : nxt) {
            if (kv.second == 0) continue;
            tmp.push_back({kv.first, kv.second});
        }
        std::sort(tmp.begin(), tmp.end(),
                  [](auto& x, auto& y){ return x.first < y.first; });

        nz_out.clear();
        nz_out.reserve(tmp.size() * 2);
        for (auto& p : tmp) {
            nz_out.push_back(p.first);
            nz_out.push_back(static_cast<uint32_t>(p.second));
        }

        return true;
    };

    // BFS build edges
    while (!work.empty()) {
        Key cur = work.front();
        work.pop();

        State* from = stmap.at(cur);

        for (uint32_t a = 0; a < A; ++a) {
            const MStep& ms = mstep[cur.mid][a];
            if (!ms.exists) continue;

            const bool parent_silent = (ms.payload < 0);
            const bool do_spawn = (!parent_silent);
            const uint32_t spawn_sid = do_spawn ? static_cast<uint32_t>(ms.payload) : 0;

            weight_t sumw;
            std::vector<uint32_t> nz_next;

            if (!step_multiset(cur.nz, do_spawn, spawn_sid, a, sumw, nz_next)) {
                continue; // reject this letter from this state
            }

            if (parent_silent && sumw != weight_t(0)) {
                QUAK_FAIL("U_sync violates synchronized silent transitions: nonzero sum on silent parent step");
            }

            Key nxtK;
            nxtK.mid = ms.to;
            nxtK.nz = std::move(nz_next);

            State* to = get_or_make(nxtK);

            Weight* ew = parent_silent ? w_silent : get_w(sumw);

            Edge* e = new Edge(falpha->at(a), ew, from, to);
            from->addSuccessor(e);
            to->addPredecessor(e);
        }
    }

    // finalize arrays and build Automaton
    // weights min/max
    weight_t fMin = weight_t(0), fMax = weight_t(0);
    bool firstOut = true;
    for (Weight* w : fweights_vec) {
        if (!w) continue;
        const weight_t v = w->getValue();
        if (firstOut) { fMin = fMax = v; firstOut = false; }
        else {
            if (v < fMin) fMin = v;
            if (v > fMax) fMax = v;
        }
    }
    if (firstOut) { fMin = fMax = weight_t(0); }

    MapArray<State*>*  fstates  = new MapArray<State*>(fstates_vec.size());
    MapArray<Weight*>* fweights = new MapArray<Weight*>(fweights_vec.size());

    for (State* st : fstates_vec)   fstates->insert(st->getId(), st);
    for (Weight* wt : fweights_vec) fweights->insert(wt->getId(), wt);

    Automaton* flat = new Automaton(
        "Flat(" + this->getName() + ")",
        falpha, fstates, fweights,
        fMin, fMax,
        finitial
    );

    return flat;
}

// Sup/LimSup + SumPlus/SumMinus flattening (Interval-based approach)
//
// Instead of tracking exact budget values, we track SETS of possible budgets
// represented as unions of intervals. This avoids the integer-only limitation
// of the enumeration-based approach.

typedef uint64_t internal_weight_t;  // Integer type with scaling for exact arithmetic

// An interval [lo, hi] of budget values (closed, inclusive)
struct BudgetInterval {
    internal_weight_t lo;
    internal_weight_t hi;
};

// A set of possible budget values, represented as:
// - fixed: disjoint sorted intervals of values in [0, abs_threshold)
// - has_unlimited: whether budget >= abs_threshold is possible
struct BudgetSet {
    std::vector<BudgetInterval> fixed;
    bool has_unlimited = false;

    bool empty() const {
        return fixed.empty() && !has_unlimited;
    }

    bool contains_zero() const {
        for (const auto& iv : fixed) {
            if (iv.lo == 0) return true;
        }
        return false;
    }

    bool contains_value(internal_weight_t v, internal_weight_t abs_threshold) const {
        if (v >= abs_threshold) return has_unlimited;
        for (const auto& iv : fixed) {
            if (v >= iv.lo && v <= iv.hi) return true;
        }
        return false;
    }

    // Check if any value > 0 exists
    bool has_positive() const {
        if (has_unlimited) return true;
        for (const auto& iv : fixed) {
            if (iv.hi > 0) return true;
        }
        return false;
    }

    // Check if any value >= 0 exists (for negative threshold success)
    bool has_non_negative() const {
        if (has_unlimited) return true;
        for (const auto& iv : fixed) {
            if (iv.hi >= 0) return true;
        }
        return false;
    }

    // For Inf termination: check if contribution v can satisfy the budget requirement
    // Success means v >= min(budget), i.e., edge provides at least what we minimally need
    bool can_satisfy(internal_weight_t v) const {
        if (fixed.empty()) return false;  // Empty budget can't be satisfied
        // Find minimum budget value - intervals are sorted, so first interval has min
        internal_weight_t min_budget = fixed[0].lo;
        return v >= min_budget;
    }
};

// Normalize a BudgetSet: sort, merge adjacent/overlapping intervals, clamp to [0, abs_threshold)
static void normalize_budget_set(BudgetSet& bs, internal_weight_t abs_threshold) {
    if (bs.fixed.empty()) return;

    // Clamp intervals to valid range
    std::vector<BudgetInterval> clamped;
    clamped.reserve(bs.fixed.size());

    for (const auto& iv : bs.fixed) {
        internal_weight_t lo = iv.lo;
        internal_weight_t hi = iv.hi;
        if (lo > hi) continue;

        // Values >= abs_threshold become unlimited
        if (hi >= abs_threshold) {
            bs.has_unlimited = true;
            if (lo >= abs_threshold) continue;
            hi = abs_threshold - 1;
        }
        clamped.push_back({lo, hi});
    }

    if (clamped.empty()) {
        bs.fixed.clear();
        return;
    }

    // Sort by lo, then by hi
    std::sort(clamped.begin(), clamped.end(), [](const BudgetInterval& a, const BudgetInterval& b) {
        return (a.lo < b.lo) || (a.lo == b.lo && a.hi < b.hi);
    });

    // Merge overlapping/adjacent intervals
    std::vector<BudgetInterval> merged;
    merged.reserve(clamped.size());
    BudgetInterval cur = clamped[0];

    for (size_t i = 1; i < clamped.size(); ++i) {
        const auto& nxt = clamped[i];
        if (nxt.lo <= cur.hi + 1) {
            // Merge
            cur.hi = std::max(cur.hi, nxt.hi);
        } else {
            merged.push_back(cur);
            cur = nxt;
        }
    }
    merged.push_back(cur);
    bs.fixed = std::move(merged);
}

// Union of two budget sets
static BudgetSet budgetset_union(const BudgetSet& a, const BudgetSet& b, internal_weight_t abs_threshold) {
    BudgetSet result;
    result.fixed.reserve(a.fixed.size() + b.fixed.size());
    result.fixed.insert(result.fixed.end(), a.fixed.begin(), a.fixed.end());
    result.fixed.insert(result.fixed.end(), b.fixed.begin(), b.fixed.end());
    result.has_unlimited = a.has_unlimited || b.has_unlimited;
    normalize_budget_set(result, abs_threshold);
    return result;
}

// Shift fixed intervals by subtracting cost (for consuming weight)
// Returns intervals that can still afford the cost
static BudgetSet budgetset_subtract_cost(const BudgetSet& bs, internal_weight_t cost, internal_weight_t abs_threshold) {
    BudgetSet result;
    result.fixed.reserve(bs.fixed.size());

    for (const auto& iv : bs.fixed) {
        if (iv.hi < cost) continue;  // Entire interval consumed
        internal_weight_t new_lo = (iv.lo >= cost) ? (iv.lo - cost) : 0;
        internal_weight_t new_hi = iv.hi - cost;
        result.fixed.push_back({new_lo, new_hi});
    }

    // Note: does NOT propagate has_unlimited (handled separately)
    normalize_budget_set(result, abs_threshold);
    return result;
}

// Generate budget set from unlimited state after spending cost
// For positive threshold: budget >= abs_threshold can guess any sum, so we get a range
// For negative threshold: budget = abs_threshold exactly (tracking actual sum), so we get a point
static BudgetSet budgetset_from_unlimited(internal_weight_t cost, internal_weight_t abs_threshold, bool negative_threshold) {
    BudgetSet result;

    if (negative_threshold) {
        // For negative threshold: deterministic tracking
        // Unlimited means budget = abs_threshold exactly
        // After spending cost, budget = abs_threshold - cost (a single point)
        if (abs_threshold >= cost) {
            internal_weight_t new_budget = abs_threshold - cost;
            if (new_budget < abs_threshold) {
                result.fixed.push_back({new_budget, new_budget});
            } else {
                result.has_unlimited = true;
            }
        }
        // If abs_threshold < cost, budget becomes negative (failure) - empty result
    } else {
        // For positive threshold: nondeterministic guessing
        // Unlimited means budget >= abs_threshold (we can guess any sum >= threshold)
        // After spending cost:
        // - Budgets in [abs_threshold, abs_threshold + cost - 1] become [0, cost - 1]
        // - Budgets >= abs_threshold + cost remain unlimited
        if (cost > 0) {
            internal_weight_t new_lo = (abs_threshold > cost) ? (abs_threshold - cost) : 0;
            internal_weight_t new_hi = abs_threshold - 1;
            if (new_lo <= new_hi) {
                result.fixed.push_back({new_lo, new_hi});
            }
        }
        result.has_unlimited = true;  // Some unlimited values remain unlimited
    }

    normalize_budget_set(result, abs_threshold);
    return result;
}

// Process budget set after consuming edge with given cost
static BudgetSet budgetset_after_edge(const BudgetSet& bs, internal_weight_t cost, internal_weight_t abs_threshold, bool negative_threshold) {
    BudgetSet result = budgetset_subtract_cost(bs, cost, abs_threshold);
    if (bs.has_unlimited) {
        BudgetSet from_unl = budgetset_from_unlimited(cost, abs_threshold, negative_threshold);
        result = budgetset_union(result, from_unl, abs_threshold);
    }
    return result;
}

// Intersect two budget sets - result contains values in BOTH sets
static BudgetSet budgetset_intersect(const BudgetSet& a, const BudgetSet& b, internal_weight_t abs_threshold) {
    BudgetSet result;

    // Intersect fixed intervals
    for (const auto& iv_a : a.fixed) {
        for (const auto& iv_b : b.fixed) {
            internal_weight_t lo = std::max(iv_a.lo, iv_b.lo);
            internal_weight_t hi = std::min(iv_a.hi, iv_b.hi);
            if (lo <= hi) {
                result.fixed.push_back({lo, hi});
            }
        }
    }

    // Unlimited intersects with unlimited
    result.has_unlimited = a.has_unlimited && b.has_unlimited;

    normalize_budget_set(result, abs_threshold);
    return result;
}

// Check if two budget sets have non-empty intersection
static bool budgetset_intersects(const BudgetSet& a, const BudgetSet& b, internal_weight_t abs_threshold) {
    BudgetSet inter = budgetset_intersect(a, b, abs_threshold);
    return !inter.empty();
}

// Convert budget set to string for state encoding
static std::string budgetset_to_string(const BudgetSet& bs) {
    std::string s;
    bool first = true;
    for (const auto& iv : bs.fixed) {
        if (!first) s.push_back(';');
        first = false;
        s.push_back('[');
        s.append(std::to_string(iv.lo));
        s.push_back(',');
        s.append(std::to_string(iv.hi));
        s.push_back(']');
    }
    if (bs.has_unlimited) {
        if (!first) s.push_back(';');
        s.append("U");
    }
    if (s.empty()) s = "{}";
    return s;
}

// Compute scale factor to convert fractional weights to integers
// Returns the smallest power of 10 that makes all weights integers
static internal_weight_t compute_weight_scale(NestedAutomaton* nwa) {
    internal_weight_t scale = 1;

    auto process_weight = [&scale](float w) {
        // Find decimal places needed for this weight
        float abs_w = (w < 0) ? -w : w;
        internal_weight_t test_scale = 1;
        for (int i = 0; i < 6; ++i) {  // Up to 6 decimal places
            float scaled = abs_w * test_scale;
            float rounded = (float)(int64_t)(scaled + 0.5f);
            if (std::abs(scaled - rounded) < 1e-6f) break;
            test_scale *= 10;
        }
        if (test_scale > scale) scale = test_scale;
    };

    // Process all child weights
    for (unsigned int c = 0; c < nwa->getChildrenSize(); ++c) {
        ChildAutomaton* child = nwa->getChild(c);
        for (State* s : *child->getStates()) {
            auto* alphabet = s->getAlphabet();
            if (!alphabet) continue;
            for (Symbol* sym : *alphabet) {
                auto* succs = s->getSuccessors(sym->getId());
                if (!succs) continue;
                for (Edge* e : *succs) {
                    process_weight(e->getWeight()->getValue().to_float());
                }
            }
        }
    }

    return scale;
}

// Scale weight and round to nearest integer for exact arithmetic
inline internal_weight_t to_internal(weight_t w, internal_weight_t scale) {
    float scaled = w.to_float() * scale;
    return static_cast<internal_weight_t>(scaled + 0.5f);
}

// Scale weight and truncate to integer (for thresholds)
// Truncation ensures correct boundary behavior for non-integer thresholds
// E.g., threshold=-0.5: abs_threshold=trunc(0.5)=0, so child_sum=-1 gives budget=-1, fail
inline internal_weight_t to_internal_trunc(weight_t w, internal_weight_t scale) {
    float scaled = w.to_float() * scale;
    return static_cast<internal_weight_t>(scaled);  // truncates toward zero
}

// Legacy version without scale (for backward compatibility where scale=1)
inline internal_weight_t to_internal(weight_t w) {
    return (internal_weight_t)w.to_float();
}

static inline bool tracking_all_zero(const std::vector<unsigned char>& v) {
    for (unsigned char b : v) {
        if (b != 0) return false;
    }
    return true;
}

static inline std::string bits_to_string(const std::vector<unsigned char>& v) {
    std::string s;
    s.reserve(v.size());
    for (unsigned char b : v) s.push_back(b ? '1' : '0');
    return s;
}

struct pending_state_t {
    std::string global_from;
    unsigned int parent_state_id_from;

    // Track ONE distinguished token with interval-based budget set
    BudgetSet budget_from;
    unsigned int child_state_id_from;
    unsigned int child_id_from;
    bool inactive_from;

    // Background bookkeeping (no 64-bit limitation)
    std::vector<unsigned char> tracking_from;   // per-flattened-child-state bits
    std::vector<unsigned char> activation_from; // per-flattened-child-state bits
};

typedef struct global_exploration_data_supremum {
    // constraints
    NestedAutomaton* A = nullptr;
    Parser* parser = nullptr;
    internal_weight_t abs_threshold{};
    internal_weight_t weight_scale = 1; // scale factor for fractional weights
    unsigned int* cumulative_size = nullptr; // prefix sums for flattening (child_id, local_state) -> global index
    unsigned int children_all = 0;
    bool negative_threshold = false; // For negative threshold: success when budget > 0 (didn't reach inflated threshold)

    // epoch reset vector (all ones)
    std::vector<unsigned char> track_them_all;

    // pending states for iterative exploration
    std::vector<pending_state_t>* pending_states = nullptr;

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int parent_state_id_from = 0;

    BudgetSet budget_from; // interval-based budget set (only valid if !inactive_from)
    unsigned int child_state_id_from = 0;
    unsigned int child_id_from = 0;
    bool inactive_from = true;

    std::vector<unsigned char> tracking_from;
    std::vector<unsigned char> activation_from;

    // initialized per-symbol
    Symbol* symbol = nullptr;

    // OLD arrays: copied from activation_from/tracking_from (the "from" state)
    std::vector<unsigned char> old_activation;
    std::vector<unsigned char> old_tracking;

    // NEW arrays: built during exploration for this symbol; must restore on backtrack
    std::vector<unsigned char> new_activation;
    std::vector<unsigned char> new_tracking;

    // computed (output)
    unsigned int parent_state_id_to = 0;
    internal_weight_t global_edge_weight = 0; // 0/1 edge weight in flattened automaton

    BudgetSet budget_to; // interval-based budget set
    unsigned int child_state_id_to = 0;
    unsigned int child_id_to = 0;
    bool inactive_to = true;
} data_supremum_t;

static void explore_global_initialization_supremum(data_supremum_t* data);
static void explore_global_parent_transition_supremum(data_supremum_t* data);
static void explore_global_child_transition_supremum(data_supremum_t* data);
static void explore_global_selection_supremum(unsigned int child_id, unsigned int child_state_id, data_supremum_t* data);
static void explore_global_finalization_supremum(data_supremum_t* data);
static void explore_global_failure_supremum(data_supremum_t* data);

static void explore_global_failure_supremum(data_supremum_t* data) {
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

// Finalize one global transition and enqueue destination if new
static void explore_global_finalization_supremum(data_supremum_t* data) {
    // Compute destination activation/tracking vectors (copies)
    std::vector<unsigned char> activation_to = data->new_activation;
    std::vector<unsigned char> tracking_to   = data->new_tracking;

    bool global_final = false;
    // Epoch boundary: tracking_from==all-zero means all obligations discharged
    if (tracking_all_zero(data->tracking_from)) {
        tracking_to  = data->track_them_all; // reset obligations
        global_final = data->A->getStates()->at(data->parent_state_id_to)->getFinal();
    }


    // Encode destination state:
    //   parent_id/activation_bits/tracking_bits/[child_id/child_state/budget | @inactive@]
    std::string global_to;
    global_to.reserve(64 + data->children_all * 2);
    global_to.append(std::to_string(data->parent_state_id_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(activation_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(tracking_to));

    if (data->inactive_to) {
        global_to.append("/@inactive@");
    } else {
        global_to.push_back('/');
        global_to.append(std::to_string(data->child_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->child_state_id_to));
        global_to.push_back('/');
        global_to.append(budgetset_to_string(data->budget_to));
    }

    // Add edge
    data->parser->edges.insert({
        { data->symbol->getName(), (weight_t)data->global_edge_weight },
        { data->global_from, global_to }
    });

    if (global_final) {
        data->parser->final_states.insert(global_to);
    }

    // Enqueue newly discovered states
    if (!data->parser->states.contains(global_to)) {
        data->parser->states.insert(global_to);

        pending_state_t ps;
        ps.global_from          = global_to;
        ps.parent_state_id_from = data->parent_state_id_to;
        ps.inactive_from        = data->inactive_to;
        ps.activation_from      = std::move(activation_to);
        ps.tracking_from        = std::move(tracking_to);

        if (!data->inactive_to) {
            ps.budget_from         = data->budget_to;
            ps.child_state_id_from = data->child_state_id_to;
            ps.child_id_from       = data->child_id_to;
        } else {
            ps.budget_from         = BudgetSet{};
            ps.child_state_id_from = 0;
            ps.child_id_from       = 0;
        }

        data->pending_states->push_back(std::move(ps));
    }
}

// Propagate background children (all except the single tracked token)
static void explore_global_selection_supremum(unsigned int child_id,
                                             unsigned int child_state_id,
                                             data_supremum_t* data) {
    // Skip the tracked token's FROM location (handled in explore_global_child_transition_supremum)
    if (!data->inactive_from &&
        child_id == data->child_id_from &&
        child_state_id == data->child_state_id_from) {
        explore_global_selection_supremum(child_id, child_state_id + 1, data);
        return;
    }

    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            // Not active: keep scanning within this child
            if (data->old_activation[i] == 0) {
                explore_global_selection_supremum(child_id, child_state_id + 1, data);
                return;
            }

            // Active but final: treat as already terminated (no propagation)
            if (states->at(child_state_id)->getFinal()) {
                explore_global_selection_supremum(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());

            if (succs) {
                for (Edge* edge : *succs) {
                    // Terminate in the same symbol: do NOT propagate to successor if successor is final
                    if (edge->getTo()->getFinal()) {
                        explore_global_selection_supremum(child_id + 1, 0, data); // FIX (1)
                        continue;
                    }

                    const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();
                    const unsigned char stored_tracking   = data->new_tracking[ii];
                    const unsigned char stored_activation = data->new_activation[ii];

                    if (data->old_tracking[i] == 1) {
                        data->new_tracking[ii] = 1;
                    }
                    if (data->old_activation[i] == 1) {
                        data->new_activation[ii] = 1;
                    }

                    explore_global_selection_supremum(child_id + 1, 0, data); // FIX (1)

                    data->new_tracking[ii]   = stored_tracking;
                    data->new_activation[ii] = stored_activation;
                }
            } else {
                // No successors on this symbol
                explore_global_selection_supremum(child_id + 1, 0, data); // FIX (1)
            }
        } else {
            // Done scanning this child: move to next child, reset local index
            explore_global_selection_supremum(child_id + 1, 0, data);
        }
    } else {
        // All children processed: emit transition
        explore_global_finalization_supremum(data);
    }
}

// Handle the single tracked token's transition (the one accumulating weight)
// Uses interval-based budget tracking for exact representation
static void explore_global_child_transition_supremum(data_supremum_t* data) {
    if (data->inactive_from) {
        data->inactive_to = true;
        explore_global_selection_supremum(0, 0, data);
        return;
    }

    ChildAutomaton* child = data->A->getChild(data->child_id_from);
    State* child_state = child->getStates()->at(data->child_state_id_from);

    // If already final: terminate now; budget must validate the guess
    if (child_state->getFinal()) {
        // For positive threshold: success if budget contains 0 (achieved threshold)
        // For negative threshold: success if budget >= 0 (didn't exceed bound)
        bool success = data->negative_threshold
            ? data->budget_from.has_non_negative()
            : data->budget_from.contains_zero();
        if (success) {
            data->inactive_to = true;
            explore_global_selection_supremum(0, 0, data);
        } else {
            explore_global_failure_supremum(data);
        }
        return;
    }

    const unsigned int i = data->cumulative_size[data->child_id_from] + data->child_state_id_from;

    auto* succs = child_state->getSuccessors(data->symbol->getId());
    if (!succs) {
        // Witness has no move: invalid
        explore_global_failure_supremum(data);
        return;
    }

    for (Edge* child_edge : *succs) {
        const unsigned int to_state_id = (unsigned int)child_edge->getTo()->getId();
        const unsigned int ii = data->cumulative_size[data->child_id_from] + to_state_id;

        const unsigned char stored_tracking   = data->new_tracking[ii];
        const unsigned char stored_activation = data->new_activation[ii];

        // Compute absolute edge cost (scaled for fractional weights)
        internal_weight_t abs_child_edge_value;
        if (child_edge->getWeight()->getValue() < 0) {
            abs_child_edge_value = to_internal(-(child_edge->getWeight()->getValue()), data->weight_scale);
        } else {
            abs_child_edge_value = to_internal(child_edge->getWeight()->getValue(), data->weight_scale);
        }

        const bool to_final = child_edge->getTo()->getFinal();

        // Propagate tracking/activation to successor only if token continues (not if it terminates now)
        if (!to_final) {
            if (data->old_tracking[i] == 1) {
                data->new_tracking[ii] = 1;
            }
            if (data->old_activation[i] == 1) {
                data->new_activation[ii] = 1;
            }
        }

        // Set token successor info
        data->child_id_to       = data->child_id_from;
        data->child_state_id_to = to_state_id;

        // Compute next budget set using interval operations
        BudgetSet next_budget = budgetset_after_edge(data->budget_from, abs_child_edge_value, data->abs_threshold, data->negative_threshold);

        if (to_final) {
            // Termination: check if we can succeed
            // For positive threshold: success if budget contains 0
            // For negative threshold: success if budget >= 0 (didn't exceed bound)
            bool success = data->negative_threshold
                ? next_budget.has_non_negative()
                : next_budget.contains_zero();

            if (success) {
                data->inactive_to = true;
                data->budget_to = BudgetSet{};  // Clear budget on success
                explore_global_selection_supremum(0, 0, data);
            } else {
                explore_global_failure_supremum(data);
            }
        } else {
            // Continue: check if any budget values remain
            if (next_budget.empty()) {
                explore_global_failure_supremum(data);
            } else {
                data->budget_to = std::move(next_budget);
                data->inactive_to = false;
                explore_global_selection_supremum(0, 0, data);
            }
        }

        data->new_tracking[ii]   = stored_tracking;
        data->new_activation[ii] = stored_activation;
    }
}

static void explore_global_parent_transition_supremum(data_supremum_t* data) {
    auto* succs = data->A->getStates()->at(data->parent_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    // Save tracked token context: each parent edge explores independently
    const unsigned int saved_child_state_id = data->child_state_id_from;
    const unsigned int saved_child_id       = data->child_id_from;
    const BudgetSet   saved_budget          = data->budget_from;
    const bool        saved_inactive        = data->inactive_from;

    for (Edge* parent_edge : *succs) {
        data->child_state_id_from = saved_child_state_id;
        data->child_id_from       = saved_child_id;
        data->budget_from         = saved_budget;
        data->inactive_from       = saved_inactive;

        data->parent_state_id_to = static_cast<unsigned int>(parent_edge->getTo()->getId());
        const unsigned int child_id = static_cast<unsigned int>(parent_edge->getWeight()->getValue().to_float());

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // Silent transition: no spawn, weight 0
            data->global_edge_weight = 0;
            explore_global_child_transition_supremum(data);
        } else {
            const unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
            const unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

            // Mark spawned child as active in OLD arrays (so selection sees it)
            const unsigned char prev_act = data->old_activation[ii];
            data->old_activation[ii] = 1;

            // Choice 1: spawn as background (not tracked for weight)
            data->global_edge_weight = 0;
            explore_global_child_transition_supremum(data);

            // Choice 2: start tracking for weight (only if no token currently tracked)
            if (saved_inactive) {
                data->global_edge_weight  = 1;                 // bet on this token
                data->child_state_id_from = summoned_child_state_id;
                data->child_id_from       = child_id;
                // Initialize budget set with unlimited (budget >= abs_threshold)
                data->budget_from         = BudgetSet{};
                data->budget_from.has_unlimited = true;
                data->inactive_from       = false;
                explore_global_child_transition_supremum(data);
            }

            data->old_activation[ii] = prev_act;
        }
    }
}

static void explore_global_initialization_supremum(data_supremum_t* data) {
    const unsigned int n = data->children_all;

    // Prepare old arrays from the stored vectors of the current global state
    data->old_activation.assign(n, 0);
    data->old_tracking.assign(n, 0);

    for (unsigned int i = 0; i < n; ++i) {
        if (i < data->activation_from.size()) data->old_activation[i] = data->activation_from[i] ? 1 : 0;
        if (i < data->tracking_from.size())   data->old_tracking[i]   = data->tracking_from[i] ? 1 : 0;
    }

    // Fresh new arrays for this symbol
    data->new_activation.assign(n, 0);
    data->new_tracking.assign(n, 0);

    // Iterate over enabled symbols of the current parent state
    auto* alphabet = data->A->getStates()->at(data->parent_state_id_from)->getAlphabet();
    if (!alphabet) return;

    // Save witness context that must be restored between symbols
    const unsigned int saved_child_state_id = data->child_state_id_from;
    const unsigned int saved_child_id       = data->child_id_from;
    const BudgetSet   saved_budget          = data->budget_from;
    const bool        saved_inactive        = data->inactive_from;

    for (Symbol* symbol : *alphabet) {
        // Restore witness context for each symbol
        data->child_state_id_from = saved_child_state_id;
        data->child_id_from       = saved_child_id;
        data->budget_from         = saved_budget;
        data->inactive_from       = saved_inactive;

        data->symbol = symbol;
        explore_global_parent_transition_supremum(data);
    }
}

// ============================================================================
// SHARED THRESHOLD-OBLIGATION BACKEND FOR MONOTONE/EXTREMAL CASES
//
// This generalizes the Min/Max Inf/LimInf threshold-obligation idea to
//   {Sup, LimSup, Inf, LimInf} x {Min_f, Max_f, SumPlus, SumMinus}
// while keeping the original file's proven epoch/final-pulse discipline.
// ============================================================================

using thrext_int_t = uint64_t;
static constexpr thrext_int_t THREXT_INF = std::numeric_limits<thrext_int_t>::max() / 4ull;

enum class ThrExtMode : uint8_t { MAX_F = 0, MIN_F = 1, SUMPLUS = 2, SUMMINUS = 3 };

struct ThrExtConf {
    uint32_t st = 0;
    thrext_int_t prog = 0;

    bool operator==(const ThrExtConf& o) const {
        return st == o.st && prog == o.prog;
    }
    bool operator<(const ThrExtConf& o) const {
        if (st != o.st) return st < o.st;
        return prog < o.prog;
    }
};

using ThrExtFrontier = std::vector<ThrExtConf>;

struct ThrExtOblKey {
    uint32_t child = 0;
    uint8_t guess = 0; // 0 => return < threshold, 1 => return >= threshold
    ThrExtFrontier conf;

    bool operator==(const ThrExtOblKey& o) const {
        return child == o.child && guess == o.guess && conf == o.conf;
    }
    bool operator<(const ThrExtOblKey& o) const {
        if (child != o.child) return child < o.child;
        if (guess != o.guess) return guess < o.guess;
        return conf < o.conf;
    }
};

struct ThrExtOblEntry {
    ThrExtOblKey key;

    bool operator==(const ThrExtOblEntry& o) const { return key == o.key; }
    bool operator<(const ThrExtOblEntry& o) const { return key < o.key; }
};

using ThrExtOblBag = std::vector<ThrExtOblEntry>;

namespace {

struct ThrExtBagStepKey {
    uint32_t symbol = 0;
    ThrExtOblBag bag;

    bool operator<(const ThrExtBagStepKey& o) const {
        if (symbol != o.symbol) return symbol < o.symbol;
        return bag < o.bag;
    }
};

struct MinMaxInfExperimentContext {
    bool enabled = false;
    MinMaxInfExperimentStats stats;
    std::set<ThrExtOblKey> unique_obls;
    std::set<ThrExtOblBag> unique_bags;
    std::set<ThrExtBagStepKey> unique_bag_steps;
    std::set<std::tuple<uint32_t, uint8_t, uint32_t>> unique_spawn_keys;

    void reset_preserving_enabled() {
        const bool keep_enabled = enabled;
        enabled = false;
        stats = MinMaxInfExperimentStats{};
        unique_obls.clear();
        unique_bags.clear();
        unique_bag_steps.clear();
        unique_spawn_keys.clear();
        enabled = keep_enabled;
    }
};

static MinMaxInfExperimentContext g_minmax_inf_experiment;

class ScopedStatsTimer {
public:
    explicit ScopedStatsTimer(double* acc)
        : acc_(acc)
        , start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedStatsTimer() {
        if (!acc_) return;
        const auto end = std::chrono::high_resolution_clock::now();
        *acc_ += std::chrono::duration<double, std::milli>(end - start_).count();
    }

private:
    double* acc_;
    std::chrono::high_resolution_clock::time_point start_;
};

static inline bool mmexp_enabled() {
    return g_minmax_inf_experiment.enabled;
}

static void mmexp_record_thr_bag(uint32_t symbol_id,
                                 const ThrExtOblBag& bag,
                                 const std::vector<ChildTables>& child_tab) {
    if (!mmexp_enabled()) return;

    auto& ctx = g_minmax_inf_experiment;
    ctx.stats.step_bag_calls++;
    ctx.unique_bag_steps.insert(ThrExtBagStepKey{symbol_id, bag});
    ctx.unique_bags.insert(bag);

    for (const ThrExtOblEntry& ent : bag) {
        ctx.unique_obls.insert(ent.key);
        ctx.stats.frontier_observations++;
        ctx.stats.frontier_config_total += ent.key.conf.size();
        if (ent.key.child < child_tab.size()) {
            ctx.stats.frontier_capacity_total += child_tab[ent.key.child].n_states;
        }
    }

    ctx.stats.unique_obligation_count = ctx.unique_obls.size();
    ctx.stats.unique_bag_count = ctx.unique_bags.size();
    ctx.stats.unique_bag_step_keys = ctx.unique_bag_steps.size();
}

static inline void mmexp_record_thr_spawn(uint32_t child, uint8_t guess, uint32_t symbol_id) {
    if (!mmexp_enabled()) return;
    auto& ctx = g_minmax_inf_experiment;
    ctx.stats.spawn_calls++;
    ctx.unique_spawn_keys.insert(std::make_tuple(child, guess, symbol_id));
    ctx.stats.unique_spawn_keys = ctx.unique_spawn_keys.size();
}

static inline void mmexp_record_thr_bag_copy(const ThrExtOblBag& src) {
    if (!mmexp_enabled()) return;
    auto& ctx = g_minmax_inf_experiment;
    ctx.stats.bag_copy_ops++;
    ctx.stats.bag_copy_entries += src.size();
}

} // namespace

void NestedAutomaton::setMinMaxInfExperimentStatsEnabled(bool enabled) {
    g_minmax_inf_experiment.enabled = enabled;
}

void NestedAutomaton::resetMinMaxInfExperimentStats() {
    g_minmax_inf_experiment.reset_preserving_enabled();
}

MinMaxInfExperimentStats NestedAutomaton::getMinMaxInfExperimentStats() {
    return g_minmax_inf_experiment.stats;
}

static inline void thrext_bag_add(ThrExtOblBag& bag, ThrExtOblEntry&& e) {
    auto it = std::lower_bound(bag.begin(), bag.end(), e);
    if (it == bag.end() || !(it->key == e.key)) {
        bag.insert(it, std::move(e));
    }
}

static inline void thrext_bag_finalize(ThrExtOblBag& bag) {
    (void)bag;
}

struct ThrExtBuchiState {
    State* parent_state = nullptr;
    ThrExtOblBag P1;
    ThrExtOblBag P2;
    acc_phase_t phase = ACC_WAIT_parent;
    bool epoch_nonempty = false;

    bool operator==(const ThrExtBuchiState& o) const {
        return parent_state == o.parent_state
            && P1 == o.P1
            && P2 == o.P2
            && phase == o.phase
            && epoch_nonempty == o.epoch_nonempty;
    }

    bool operator<(const ThrExtBuchiState& o) const {
        if (parent_state != o.parent_state) return parent_state < o.parent_state;
        if (P1 != o.P1) return P1 < o.P1;
        if (P2 != o.P2) return P2 < o.P2;
        if (phase != o.phase) return phase < o.phase;
        return epoch_nonempty < o.epoch_nonempty;
    }
};

static inline acc_phase_t advance_phase_thrext(const ThrExtBuchiState& s) {
    if (s.phase == ACC_WAIT_parent) {
        return (s.parent_state->getFinal()) ? ACC_WAIT_P2EMPTY : ACC_WAIT_parent;
    }
    return (s.P2.empty()) ? ACC_WAIT_parent : ACC_WAIT_P2EMPTY;
}

struct ThrExtChildInfo {
    bool enabled = false;
    ThrExtMode mode = ThrExtMode::MAX_F;
    weight_t raw_threshold = weight_t(0);
    thrext_int_t weight_scale = 1;
    thrext_int_t goal = 0;
    thrext_int_t cap = 0;
    bool forced = false;
    uint8_t forced_guess = 0;

    std::vector<uint8_t> mm_live[2][2];
    std::vector<thrext_int_t> min_extra;
    std::vector<thrext_int_t> max_extra;
};

static inline thrext_int_t thrext_sat_add_cap(thrext_int_t a, thrext_int_t b, thrext_int_t cap) {
    if (a >= cap || b >= cap) return cap;
    return (a > cap - b) ? cap : (a + b);
}

static inline thrext_int_t thrext_sat_add_inf(thrext_int_t a, thrext_int_t b) {
    if (a >= THREXT_INF || b >= THREXT_INF) return THREXT_INF;
    return (a > THREXT_INF - b) ? THREXT_INF : (a + b);
}

static inline thrext_int_t thrext_scale_weight(weight_t w, thrext_int_t scale) {
    const double scaled = static_cast<double>(w.to_float()) * static_cast<double>(scale);
    return static_cast<thrext_int_t>(std::llround(scaled));
}

static inline thrext_int_t thrext_scale_threshold_ceil(weight_t w, thrext_int_t scale) {
    const double scaled = static_cast<double>(w.to_float()) * static_cast<double>(scale);
    return static_cast<thrext_int_t>(std::ceil(scaled - 1e-9));
}

static inline thrext_int_t thrext_scale_threshold_floor_nonneg(weight_t w, thrext_int_t scale) {
    const double scaled = static_cast<double>(w.to_float()) * static_cast<double>(scale);
    return static_cast<thrext_int_t>(std::floor(scaled + 1e-9));
}

static inline ThrExtMode thrext_mode_from_fin(value_function_t finVal) {
    if (finVal == Max_f) return ThrExtMode::MAX_F;
    if (finVal == Min_f) return ThrExtMode::MIN_F;
    if (finVal == SumPlus) return ThrExtMode::SUMPLUS;
    if (finVal == SumMinus) return ThrExtMode::SUMMINUS;
    QUAK_FAIL("Unsupported shared threshold backend mode");
    return ThrExtMode::MAX_F;
}

static inline bool thrext_child_uses_tracking(ChildAutomaton* c) {
    return c && c->getStates()->size() >= 2;
}

static inline bool thrext_prefers_larger(const ThrExtChildInfo& info, uint8_t guess) {
    switch (info.mode) {
        case ThrExtMode::MAX_F:
        case ThrExtMode::MIN_F:
        case ThrExtMode::SUMPLUS:
            return guess == 1u;
        case ThrExtMode::SUMMINUS:
            return guess == 0u;
    }
    return false;
}

static inline thrext_int_t thrext_init_prog(const ThrExtChildInfo& info) {
    switch (info.mode) {
        case ThrExtMode::MAX_F: return 0u;
        case ThrExtMode::MIN_F: return 1u;
        case ThrExtMode::SUMPLUS: return 0u;
        case ThrExtMode::SUMMINUS: return 0u;
    }
    return 0u;
}

static inline thrext_int_t thrext_step_prog(const ThrExtChildInfo& info,
                                            thrext_int_t prog,
                                            const weight_t& edge_w) {
    switch (info.mode) {
        case ThrExtMode::MAX_F: {
            const bool high = !(edge_w < info.raw_threshold);
            return (prog != 0u || high) ? 1u : 0u;
        }
        case ThrExtMode::MIN_F: {
            const bool high = !(edge_w < info.raw_threshold);
            return (prog != 0u && high) ? 1u : 0u;
        }
        case ThrExtMode::SUMPLUS: {
            if (edge_w < weight_t(0)) {
                QUAK_FAIL("Threshold backend for SumPlus requires non-negative child weights");
            }
            const thrext_int_t cost = thrext_scale_weight(edge_w, info.weight_scale);
            return thrext_sat_add_cap(prog, cost, info.cap);
        }
        case ThrExtMode::SUMMINUS: {
            const weight_t abs_w = (edge_w < weight_t(0)) ? -edge_w : edge_w;
            const thrext_int_t cost = thrext_scale_weight(abs_w, info.weight_scale);
            return thrext_sat_add_cap(prog, cost, info.cap);
        }
    }
    return prog;
}

static inline bool thrext_discharge_ok(const ThrExtChildInfo& info,
                                       uint8_t guess,
                                       thrext_int_t prog) {
    if (info.forced) {
        return guess == info.forced_guess;
    }

    switch (info.mode) {
        case ThrExtMode::MAX_F:
        case ThrExtMode::MIN_F:
            return prog == static_cast<thrext_int_t>(guess);
        case ThrExtMode::SUMPLUS:
            return (guess == 1u) ? (prog >= info.goal) : (prog < info.goal);
        case ThrExtMode::SUMMINUS:
            return (guess == 1u) ? (prog <= info.goal) : (prog >= info.cap);
    }
    return false;
}

static void thrext_frontier_canonicalize(ThrExtFrontier& fr,
                                         const ThrExtChildInfo& info,
                                         uint8_t guess) {
    if (fr.empty()) return;

    std::sort(fr.begin(), fr.end(), [](const ThrExtConf& a, const ThrExtConf& b) {
        if (a.st != b.st) return a.st < b.st;
        return a.prog < b.prog;
    });

    ThrExtFrontier out;
    out.reserve(fr.size());

    const bool prefer_larger = thrext_prefers_larger(info, guess);
    size_t i = 0;
    while (i < fr.size()) {
        size_t j = i + 1;
        thrext_int_t best = fr[i].prog;
        while (j < fr.size() && fr[j].st == fr[i].st) {
            if (prefer_larger) {
                if (fr[j].prog > best) best = fr[j].prog;
            } else {
                if (fr[j].prog < best) best = fr[j].prog;
            }
            ++j;
        }
        out.push_back(ThrExtConf{fr[i].st, best});
        i = j;
    }

    fr = std::move(out);
}

static void thrext_build_mm_live(const ChildTables& T, ThrExtChildInfo& info) {
    const uint32_t prod_sz = T.n_states * 2u;
    std::vector<std::vector<uint32_t>> rev(prod_sz);

    auto node_id = [](uint32_t st, uint8_t p) -> uint32_t {
        return (st << 1u) | static_cast<uint32_t>(p);
    };

    for (uint32_t st = 0; st < T.n_states; ++st) {
        if (T.is_final[st]) continue;

        for (uint8_t p = 0; p <= 1u; ++p) {
            for (uint32_t sym = 0; sym < T.alph; ++sym) {
                const uint32_t cell = T.idx(st, sym);
                const uint32_t b = T.off[static_cast<size_t>(cell)];
                const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                for (uint32_t pos = b; pos < e; ++pos) {
                    const auto& tr = T.edges[static_cast<size_t>(pos)];
                    if (tr.to >= T.n_states) continue;

                    const uint8_t p2 = static_cast<uint8_t>(thrext_step_prog(info, p, tr.w));
                    if (T.is_final[tr.to]) continue;

                    rev[node_id(tr.to, p2)].push_back(node_id(st, p));
                }
            }
        }
    }

    for (uint8_t guess = 0; guess <= 1u; ++guess) {
        std::vector<uint8_t> seen(prod_sz, 0u);
        std::deque<uint32_t> q;

        for (uint32_t st = 0; st < T.n_states; ++st) {
            if (T.is_final[st]) continue;

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

                        const uint8_t p2 = static_cast<uint8_t>(thrext_step_prog(info, p, tr.w));
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

        for (uint32_t st = 0; st < T.n_states; ++st) {
            if (T.is_final[st]) continue;
            for (uint8_t p = 0; p <= 1u; ++p) {
                if (seen[node_id(st, p)]) {
                    info.mm_live[guess][p][st] = 1u;
                }
            }
        }
    }
}

struct ThrExtRevCostEdge {
    uint32_t from = 0;
    thrext_int_t cost = 0;
};

static void thrext_build_sum_cutoffs(const ChildTables& T, ThrExtChildInfo& info) {
    std::vector<std::vector<ThrExtRevCostEdge>> rev(T.n_states);

    for (uint32_t st = 0; st < T.n_states; ++st) {
        if (T.is_final[st]) continue;
        for (uint32_t sym = 0; sym < T.alph; ++sym) {
            const uint32_t cell = T.idx(st, sym);
            const uint32_t b = T.off[static_cast<size_t>(cell)];
            const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

            for (uint32_t pos = b; pos < e; ++pos) {
                const auto& tr = T.edges[static_cast<size_t>(pos)];
                if (tr.to >= T.n_states) continue;

                const thrext_int_t cost = [&]() -> thrext_int_t {
                    if (info.mode == ThrExtMode::SUMPLUS) {
                        if (tr.w < weight_t(0)) {
                            QUAK_FAIL("Threshold backend for SumPlus requires non-negative child weights");
                        }
                        return thrext_scale_weight(tr.w, info.weight_scale);
                    }
                    const weight_t abs_w = (tr.w < weight_t(0)) ? -tr.w : tr.w;
                    return thrext_scale_weight(abs_w, info.weight_scale);
                }();

                rev[tr.to].push_back(ThrExtRevCostEdge{st, cost});
            }
        }
    }

    info.min_extra.assign(T.n_states, THREXT_INF);
    using DijkstraItem = std::pair<thrext_int_t, uint32_t>;
    std::priority_queue<DijkstraItem, std::vector<DijkstraItem>, std::greater<DijkstraItem>> pq;

    for (uint32_t st = 0; st < T.n_states; ++st) {
        if (T.is_final[st]) {
            info.min_extra[st] = 0u;
            pq.push({0u, st});
        }
    }

    while (!pq.empty()) {
        const auto [dist, v] = pq.top();
        pq.pop();
        if (dist != info.min_extra[v]) continue;

        for (const auto& re : rev[v]) {
            const thrext_int_t nd = thrext_sat_add_inf(dist, re.cost);
            if (nd < info.min_extra[re.from]) {
                info.min_extra[re.from] = nd;
                pq.push({nd, re.from});
            }
        }
    }

    // Seed every state that can reach a final once. This allows the reverse
    // relaxation to discover positive-gain cycles hidden behind zero-cost exits.
    info.max_extra.assign(T.n_states, 0u);
    std::deque<uint32_t> q;
    std::vector<uint8_t> in_q(T.n_states, 0u);

    for (uint32_t st = 0; st < T.n_states; ++st) {
        if (!T.live.empty() && !T.live[st]) continue;
        q.push_back(st);
        in_q[st] = 1u;
    }

    while (!q.empty()) {
        const uint32_t v = q.front();
        q.pop_front();
        in_q[v] = 0u;

        for (const auto& re : rev[v]) {
            if (!T.live.empty() && !T.live[re.from]) continue;
            const thrext_int_t cand = thrext_sat_add_cap(info.max_extra[v], re.cost, info.cap);
            if (cand > info.max_extra[re.from]) {
                info.max_extra[re.from] = cand;
                if (!in_q[re.from]) {
                    in_q[re.from] = 1u;
                    q.push_back(re.from);
                }
            }
        }
    }
}

static bool thrext_build_child_info(const ChildTables& T,
                                    value_function_t finVal,
                                    weight_t threshold,
                                    thrext_int_t weight_scale,
                                    ThrExtChildInfo& info) {
    if (!T.child || T.n_states == 0 || T.alph == 0) return false;

    info = ThrExtChildInfo{};
    info.enabled = true;
    info.mode = thrext_mode_from_fin(finVal);
    info.raw_threshold = threshold;
    info.weight_scale = weight_scale;

    if (info.mode == ThrExtMode::SUMPLUS) {
        if (threshold <= weight_t(0)) {
            info.forced = true;
            info.forced_guess = 1u;
            info.goal = 0u;
            info.cap = 0u;
        } else {
            info.goal = thrext_scale_threshold_ceil(threshold, weight_scale);
            info.cap = info.goal;
        }
        thrext_build_sum_cutoffs(T, info);
    } else if (info.mode == ThrExtMode::SUMMINUS) {
        if (threshold > weight_t(0)) {
            info.forced = true;
            info.forced_guess = 0u;
            info.goal = 0u;
            info.cap = 0u;
        } else {
            info.goal = thrext_scale_threshold_floor_nonneg(-threshold, weight_scale);
            info.cap = info.goal + 1u;
        }
        thrext_build_sum_cutoffs(T, info);
    } else {
        info.goal = 1u;
        info.cap = 1u;
        thrext_build_mm_live(T, info);
    }

    return true;
}

static inline bool thrext_is_live(const ChildTables& T,
                                  const ThrExtChildInfo& info,
                                  uint8_t guess,
                                  uint32_t st,
                                  thrext_int_t prog) {
    if (st >= T.n_states) return false;
    if (!T.live.empty() && !T.live[st]) return false;
    if (info.forced) return guess == info.forced_guess;

    switch (info.mode) {
        case ThrExtMode::MAX_F:
        case ThrExtMode::MIN_F: {
            if (prog > 1u) return false;
            if (info.mm_live[guess][prog].empty()) return false;
            return info.mm_live[guess][prog][st] != 0u;
        }
        case ThrExtMode::SUMPLUS: {
            if (guess == 1u) {
                return thrext_sat_add_cap(prog, info.max_extra[st], info.cap) >= info.goal;
            }
            if (info.min_extra[st] >= THREXT_INF) return false;
            return thrext_sat_add_inf(prog, info.min_extra[st]) < info.goal;
        }
        case ThrExtMode::SUMMINUS: {
            if (guess == 1u) {
                if (info.min_extra[st] >= THREXT_INF) return false;
                return thrext_sat_add_inf(prog, info.min_extra[st]) <= info.goal;
            }
            return thrext_sat_add_cap(prog, info.max_extra[st], info.cap) >= info.cap;
        }
    }

    return false;
}

enum class ThrExtStepStatus : uint8_t { DEAD = 0, DISCHARGED = 1, NONEMPTY = 2 };

static ThrExtStepStatus thrext_step_frontier(uint32_t child_idx,
                                             uint8_t guess,
                                             const ThrExtFrontier& in_conf,
                                             uint32_t symbol_id,
                                             ThrExtFrontier& next_conf,
                                             const std::vector<ChildTables>& child_tab,
                                             const std::vector<ThrExtChildInfo>& child_info) {
    next_conf.clear();

    if (child_idx >= child_tab.size() || child_idx >= child_info.size()) {
        return ThrExtStepStatus::DEAD;
    }

    const ChildTables& T = child_tab[child_idx];
    const ThrExtChildInfo& info = child_info[child_idx];
    if (!T.child || !info.enabled) return ThrExtStepStatus::DEAD;
    if (symbol_id >= T.alph) return ThrExtStepStatus::DEAD;
    if (in_conf.empty()) return ThrExtStepStatus::DEAD;

    next_conf.reserve(in_conf.size());

    for (const ThrExtConf& c : in_conf) {
        if (!thrext_is_live(T, info, guess, c.st, c.prog)) continue;

        const uint32_t cell = T.idx(c.st, symbol_id);
        const uint32_t b = T.off[static_cast<size_t>(cell)];
        const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

        for (uint32_t pos = b; pos < e; ++pos) {
            const auto& tr = T.edges[static_cast<size_t>(pos)];
            if (tr.to >= T.n_states) continue;

            const thrext_int_t prog2 = thrext_step_prog(info, c.prog, tr.w);
            if (T.is_final[tr.to]) {
                if (thrext_discharge_ok(info, guess, prog2)) {
                    next_conf.clear();
                    return ThrExtStepStatus::DISCHARGED;
                }
                continue;
            }

            if (!thrext_is_live(T, info, guess, tr.to, prog2)) continue;
            next_conf.push_back(ThrExtConf{tr.to, prog2});
        }
    }

    thrext_frontier_canonicalize(next_conf, info, guess);
    if (next_conf.empty()) return ThrExtStepStatus::DEAD;
    return ThrExtStepStatus::NONEMPTY;
}

static bool thrext_step_obl_bag(const ThrExtOblBag& in,
                                uint32_t symbol_id,
                                ThrExtOblBag& out,
                                bool* any_discharged,
                                const std::vector<ChildTables>& child_tab,
                                const std::vector<ThrExtChildInfo>& child_info) {
    ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_step_bag_ms : nullptr);
    mmexp_record_thr_bag(symbol_id, in, child_tab);
    out.clear();
    if (any_discharged) *any_discharged = false;
    if (in.empty()) return true;

    for (const ThrExtOblEntry& ent : in) {
        ThrExtFrontier next_conf;
        const ThrExtStepStatus st = thrext_step_frontier(ent.key.child,
                                                         ent.key.guess,
                                                         ent.key.conf,
                                                         symbol_id,
                                                         next_conf,
                                                         child_tab,
                                                         child_info);
        if (st == ThrExtStepStatus::DEAD) return false;
        if (st == ThrExtStepStatus::DISCHARGED) {
            if (any_discharged) *any_discharged = true;
            continue;
        }

        thrext_bag_add(out, ThrExtOblEntry{
            ThrExtOblKey{ent.key.child, ent.key.guess, std::move(next_conf)}
        });
    }

    thrext_bag_finalize(out);
    return true;
}

enum class ThrExtSpawnStatus : uint8_t { REJECT = 0, EMPTY = 1, NONEMPTY = 2 };

static ThrExtSpawnStatus thrext_spawn_frontier(uint32_t child_idx,
                                               uint32_t symbol_id,
                                               uint8_t guess,
                                               ThrExtFrontier& conf,
                                               const std::vector<ChildTables>& child_tab,
                                               const std::vector<ThrExtChildInfo>& child_info) {
    if (child_idx >= child_tab.size() || child_idx >= child_info.size()) {
        return ThrExtSpawnStatus::REJECT;
    }

    const ChildTables& T = child_tab[child_idx];
    const ThrExtChildInfo& info = child_info[child_idx];
    if (!T.child || !info.enabled) return ThrExtSpawnStatus::REJECT;
    if (symbol_id >= T.alph) return ThrExtSpawnStatus::REJECT;
    if (T.init >= T.n_states) return ThrExtSpawnStatus::REJECT;

    conf.clear();
    const thrext_int_t p0 = thrext_init_prog(info);

    const uint32_t cell = T.idx(T.init, symbol_id);
    const uint32_t b = T.off[static_cast<size_t>(cell)];
    const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

    for (uint32_t pos = b; pos < e; ++pos) {
        const auto& tr = T.edges[static_cast<size_t>(pos)];
        if (tr.to >= T.n_states) continue;

        const thrext_int_t prog2 = thrext_step_prog(info, p0, tr.w);
        if (T.is_final[tr.to]) {
            if (thrext_discharge_ok(info, guess, prog2)) {
                return ThrExtSpawnStatus::EMPTY;
            }
            continue;
        }

        if (!thrext_is_live(T, info, guess, tr.to, prog2)) continue;
        conf.push_back(ThrExtConf{tr.to, prog2});
    }

    thrext_frontier_canonicalize(conf, info, guess);
    if (conf.empty()) return ThrExtSpawnStatus::REJECT;
    return ThrExtSpawnStatus::NONEMPTY;
}

static ThrExtSpawnStatus thrext_spawn_obligation(uint32_t child_idx,
                                                 uint32_t symbol_id,
                                                 uint8_t guess,
                                                 ThrExtOblEntry& spawned,
                                                 const std::vector<ChildTables>& child_tab,
                                                 const std::vector<ThrExtChildInfo>& child_info) {
    ThrExtFrontier conf;
    const ThrExtSpawnStatus st = thrext_spawn_frontier(child_idx,
                                                       symbol_id,
                                                       guess,
                                                       conf,
                                                       child_tab,
                                                       child_info);
    if (st == ThrExtSpawnStatus::NONEMPTY) {
        spawned = ThrExtOblEntry{ThrExtOblKey{child_idx, guess, std::move(conf)}};
    }
    return st;
}

static Automaton* flatten_threshold_extremal_impl(NestedAutomaton* A,
                                                  value_function_t finVal,
                                                  weight_t threshold) {
    const ThrExtMode mode = thrext_mode_from_fin(finVal);
    const bool is_sum_mode = (mode == ThrExtMode::SUMPLUS || mode == ThrExtMode::SUMMINUS);
    const thrext_int_t weight_scale = is_sum_mode ? compute_weight_scale(A) : 1u;

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    MapArray<Symbol*>* new_alphabet = nullptr;
    MapArray<Weight*>* new_weights = nullptr;

    const size_t k = A->getChildrenSize();
    std::vector<ChildTables> child_tab(k);
    std::vector<ThrExtChildInfo> child_info(k);

    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* c = A->getChild(i);
        if (!thrext_child_uses_tracking(c)) continue;
        build_child_tables(c, child_tab[i]);
        thrext_build_child_info(child_tab[i], finVal, threshold, weight_scale, child_info[i]);
    }

    const size_t alph_size = A->getAlphabetSize();
    new_alphabet = new MapArray<Symbol*>(alph_size);
    for (size_t i = 0; i < alph_size; ++i) {
        Symbol* original = A->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    MapStd<weight_t, Weight*> weight_register;
    new_weights = new MapArray<Weight*>(3);

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

    MapStd<ThrExtBuchiState, State*> state_map;
    std::deque<ThrExtBuchiState> worklist;
    unsigned int state_counter = 0;

    ThrExtBuchiState init;
    init.parent_state = A->getInitial();
    init.P1.clear();
    init.P2.clear();
    init.phase = ACC_WAIT_parent;
    init.epoch_nonempty = false;

    std::ostringstream ss;
    ss << "bxt_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
    state_map[init] = init_state;
    worklist.push_back(init);

    ThrExtOblBag P1_step, P2_step;
    ThrExtOblBag P1_next, P2_next;

    while (!worklist.empty()) {
        ThrExtBuchiState current = std::move(worklist.front());
        worklist.pop_front();

        auto copy_bag = [&](ThrExtOblBag& dst, const ThrExtOblBag& src) {
            ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_bag_copy_ms : nullptr);
            mmexp_record_thr_bag_copy(src);
            dst = src;
        };

        State* current_state = nullptr;
        {
            ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
            if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
            current_state = state_map.at(current);
        }
        const acc_phase_t phase_after_current = advance_phase_thrext(current);
        const bool reset_epoch =
            (current.phase == ACC_WAIT_P2EMPTY && current.P2.empty());

        for (uint32_t symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            if (!thrext_step_obl_bag(current.P1, symbol_id, P1_step, nullptr, child_tab, child_info)) {
                continue;
            }

            bool tracked_discharged = false;
            if (!current.P2.empty()) {
                if (!thrext_step_obl_bag(current.P2, symbol_id, P2_step, &tracked_discharged, child_tab, child_info)) {
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
                    !child_tab[child_index].child ||
                    !child_info[child_index].enabled;
                const bool boundary = current.P2.empty();
                bool epoch_nonempty_to = reset_epoch ? !current.P1.empty() : current.epoch_nonempty;
                if (tracked_discharged) epoch_nonempty_to = true;
                if (!is_silent) epoch_nonempty_to = true;

                if (is_silent) {
                    if (boundary) {
                        P1_next.clear();
                        copy_bag(P2_next, P1_step);
                    } else {
                        copy_bag(P1_next, P1_step);
                        copy_bag(P2_next, P2_step);
                    }

                    ThrExtBuchiState nxt;
                    nxt.parent_state = q_prime;
                    nxt.P1 = P1_next;
                    nxt.P2 = P2_next;
                    nxt.phase = phase_after_current;
                    nxt.epoch_nonempty = epoch_nonempty_to;

                    bool has_nxt = false;
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
                        has_nxt = state_map.contains(nxt);
                    }
                    if (!has_nxt) {
                        std::ostringstream s2;
                        s2 << "bxt_" << state_counter++;
                        State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_insert_calls++;
                        state_map[nxt] = ns;
                        worklist.push_back(nxt);
                    }

                    State* to_state = nullptr;
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
                        to_state = state_map.at(nxt);
                    }
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(SILENT)),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                    continue;
                }

                for (uint8_t guess = 0u; guess <= 1u; ++guess) {
                    ThrExtOblEntry spawned;
                    mmexp_record_thr_spawn(static_cast<uint32_t>(child_index), guess, symbol_id);
                    const ThrExtSpawnStatus st = thrext_spawn_obligation(
                        static_cast<uint32_t>(child_index),
                        symbol_id,
                        guess,
                        spawned,
                        child_tab,
                        child_info
                    );
                    if (st == ThrExtSpawnStatus::REJECT) continue;

                    if (boundary) {
                        copy_bag(P2_next, P1_step);
                        P1_next.clear();
                        if (st == ThrExtSpawnStatus::NONEMPTY) {
                            thrext_bag_add(P1_next, std::move(spawned));
                        }
                    } else {
                        copy_bag(P2_next, P2_step);
                        copy_bag(P1_next, P1_step);
                        if (st == ThrExtSpawnStatus::NONEMPTY) {
                            thrext_bag_add(P1_next, std::move(spawned));
                        }
                    }
                    thrext_bag_finalize(P1_next);

                    ThrExtBuchiState nxt;
                    nxt.parent_state = q_prime;
                    nxt.P1 = P1_next;
                    nxt.P2 = P2_next;
                    nxt.phase = phase_after_current;
                    nxt.epoch_nonempty = epoch_nonempty_to;

                    bool has_nxt = false;
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
                        has_nxt = state_map.contains(nxt);
                    }
                    if (!has_nxt) {
                        std::ostringstream s2;
                        s2 << "bxt_" << state_counter++;
                        State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_insert_calls++;
                        state_map[nxt] = ns;
                        worklist.push_back(nxt);
                    }

                    State* to_state = nullptr;
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
                        to_state = state_map.at(nxt);
                    }
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
    for (const auto& [gs, st] : state_map) {
        new_states->insert(st->getId(), st);
        if (gs.phase == ACC_WAIT_P2EMPTY && gs.P2.empty() && gs.epoch_nonempty) {
            st->setFinal(true);
        }
    }

    const std::string name = "BuchiThr(" + A->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights, global_min, global_max, init_state);
}

// Track ONE child token explicitly for weight, others as background
// Returns a flattened automaton with 0/1 weights encoding threshold achievement
Automaton* NestedAutomaton::flatten_SumPlusMinus_Sup(value_function_t finite_aggregator,
                                                     weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Sup requires SumPlus or SumMinus");
    }
    return flatten_threshold_extremal_impl(this, finite_aggregator, threshold);

    // Compute scale factor to convert fractional weights to integers
    internal_weight_t weight_scale = compute_weight_scale(this);

    internal_weight_t abs_threshold;
    bool negative_threshold = (threshold <= 0);
    if (threshold > 0) {
        abs_threshold = to_internal(threshold, weight_scale);
    } else {
        // For negative threshold, use floor of |threshold| to ensure correct boundary
        // E.g., threshold=-0.5 with scale=1: abs_threshold=floor(0.5)=0
        // Child with sum -1 costs 1, budget 0-1=-1 < 0, fail (correct since -1 < -0.5)
        abs_threshold = to_internal_trunc(-threshold, weight_scale);
    }

    // Flatten child state space
    std::vector<unsigned int> cumulative_size(this->children_->size() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->children_->size() + 1; ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int children_all = cumulative_size[this->children_->size()];

    // Epoch reset vector (all ones)
    std::vector<unsigned char> track_them_all(children_all, 1);

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);
    parser->states.insert("@sink@");

    // Sink self-loops on all parent symbols
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

    // Initial: no children active, no obligations discharged yet (tracking=0), no witness
    std::vector<unsigned char> zero(children_all, 0);

    std::string global_initial;
    global_initial.reserve(64 + children_all * 2);
    global_initial.append(std::to_string(this->initial->getId()));
    global_initial.push_back('/');
    global_initial.append(bits_to_string(zero)); // activation
    global_initial.push_back('/');
    global_initial.append(bits_to_string(zero)); // tracking
    global_initial.append("/@inactive@");

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    data_supremum_t* data = new data_supremum_t();
    if (data == nullptr) QUAK_FAIL("out of memory");

    data->A = this;
    data->parser = parser;
    data->abs_threshold = abs_threshold;
    data->weight_scale = weight_scale;
    data->cumulative_size = cumulative_size.data();
    data->children_all = children_all;
    data->negative_threshold = negative_threshold;
    data->track_them_all = std::move(track_them_all);

    // Iterative DFS using explicit stack
    std::vector<pending_state_t> pending_states;
    pending_states.reserve(1024);

    pending_state_t initial_ps;
    initial_ps.global_from          = global_initial;
    initial_ps.parent_state_id_from = this->initial->getId();
    initial_ps.inactive_from        = true;
    initial_ps.activation_from      = zero;
    initial_ps.tracking_from        = zero;
    initial_ps.budget_from          = BudgetSet{};  // Empty budget set for inactive
    initial_ps.child_state_id_from  = 0;
    initial_ps.child_id_from        = 0;
    pending_states.push_back(std::move(initial_ps));

    data->pending_states = &pending_states;

    while (!pending_states.empty()) {
        pending_state_t ps = std::move(pending_states.back());
        pending_states.pop_back();

        data->global_from          = std::move(ps.global_from);
        data->parent_state_id_from = ps.parent_state_id_from;
        data->inactive_from        = ps.inactive_from;
        data->activation_from      = std::move(ps.activation_from);
        data->tracking_from        = std::move(ps.tracking_from);

        if (!ps.inactive_from) {
            data->budget_from         = std::move(ps.budget_from);
            data->child_state_id_from = ps.child_state_id_from;
            data->child_id_from       = ps.child_id_from;
        } else {
            data->budget_from         = BudgetSet{};
            data->child_state_id_from = 0;
            data->child_id_from       = 0;
        }

        explore_global_initialization_supremum(data);
    }

    delete data;

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    return unnested;
}












/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////


typedef internal_weight_t (*beyond_threshold_fn_t)(internal_weight_t, internal_weight_t);

typedef struct global_exploration_data_all {
    // constraints
    NestedAutomaton* A;
    Parser* parser;
    internal_weight_t abs_threshold;
    internal_weight_t budget_limit;  // For normalization: abs_threshold + 1 (so abs_threshold is valid)
    internal_weight_t weight_scale = 1; // scale factor for fractional weights
    unsigned int* cumulative_size;
    unsigned int children_all;
    beyond_threshold_fn_t beyond_threshold;
    bool negative_threshold = false; // For SumMinus with negative threshold

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int parent_state_id_from;
    std::vector<unsigned int> global_activation_from;   // size = children_all, values 0 or 1 (is child active?)
    std::vector<unsigned int> global_tracking_from;     // size = children_all, values 0 or 1 (must discharge this epoch?)
    std::vector<BudgetSet> global_budget_from;          // size = children_all (interval-based)
    unsigned int parent_tracking_from;

    // initialized per-symbol
    std::vector<BudgetSet> old_value_of_children_state;
    std::vector<unsigned int> old_activation;           // activation from source
    std::vector<unsigned int> old_tracked_children_state;
    Symbol* symbol;

    // computed (output accumulators)
    unsigned int parent_state_id_to;
    internal_weight_t global_edge_weight;
    std::vector<unsigned int> new_activation;           // computed activation
    std::vector<unsigned int> new_tracked_children_state;
    std::vector<BudgetSet> new_value_of_children_state;
    unsigned int parent_tracking_to;
} data_all_t;

// Weight 1 at discharge point (good guess)
internal_weight_t beyond_good_threshold (internal_weight_t value, internal_weight_t abs_threshold) {
    return (value == abs_threshold) ? 1 : 0;
}

// Inverted: reaching threshold is the losing condition
internal_weight_t beyond_bad_threshold (internal_weight_t value, internal_weight_t abs_threshold) {
    return (value == abs_threshold) ? 0 : 1;
}

void explore_global_initialization (data_all_t* data);
void explore_global_selection (unsigned int child_id, unsigned int child_state_id, data_all_t* data);
void explore_global_parent_transition (data_all_t* data);

// Sink loops with weight 0 on all symbols (installed once in caller)
void explore_global_failure (data_all_t* data) {
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

void explore_global_finalization (data_all_t* data) {
    std::vector<BudgetSet> global_budget_to = data->new_value_of_children_state;
    std::vector<unsigned int> child_activation_to = data->new_activation;
    std::vector<unsigned int> child_tracking_to = data->new_tracked_children_state;

    bool global_final = false;

    // Epoch boundary: check tracking BEFORE the step (from source state).
    // This matches Sup's approach: epoch ends when all tokens from current epoch are discharged.
    bool all_tracking_from_discharged = true;
    for (unsigned int i = 0; i < data->children_all; ++i) {
        if (data->global_tracking_from[i]) { all_tracking_from_discharged = false; break; }
    }

    if (all_tracking_from_discharged && data->parent_tracking_from == 0) {
        // Epoch boundary: promote activation to tracking (active children become tracked for next epoch)
        for (unsigned int i = 0; i < data->children_all; ++i) {
            child_tracking_to[i] = child_activation_to[i];
        }
        data->parent_tracking_to = 2;  // final pulse
        State* parent_to_state = data->A->getStates()->at(data->parent_state_id_to);
        global_final = parent_to_state->getFinal();
    }

    std::string global_to;
    global_to.reserve(64 + data->children_all * 32);
    global_to.append(std::to_string(data->parent_state_id_to));
    global_to.push_back('/');
    for (unsigned int i = 0; i < data->children_all; ++i) {
        if (i > 0) global_to.push_back(',');
        global_to.append(budgetset_to_string(global_budget_to[i]));
    }
    global_to.push_back('/');
    for (unsigned int i = 0; i < data->children_all; ++i) {
        global_to.push_back(child_activation_to[i] ? '1' : '0');
    }
    global_to.push_back('/');
    for (unsigned int i = 0; i < data->children_all; ++i) {
        global_to.push_back(child_tracking_to[i] ? '1' : '0');
    }
    global_to.append(std::to_string(data->parent_tracking_to));

    if (global_final) {
        data->parser->final_states.insert(global_to);
    }

    data->parser->edges.insert({
        { data->symbol->getName(), (weight_t)data->global_edge_weight },
        { data->global_from, global_to }
    });

    if (!data->parser->states.contains(global_to)) {
        data->parser->states.insert(global_to);

        data_all_t data_deeper{};
        data_deeper.A = data->A;
        data_deeper.parser = data->parser;
        data_deeper.abs_threshold = data->abs_threshold;
        data_deeper.budget_limit = data->budget_limit;
        data_deeper.weight_scale = data->weight_scale;
        data_deeper.cumulative_size = data->cumulative_size;
        data_deeper.children_all = data->children_all;
        data_deeper.beyond_threshold = data->beyond_threshold;
        data_deeper.negative_threshold = data->negative_threshold;

        data_deeper.global_from = global_to;
        data_deeper.parent_state_id_from = data->parent_state_id_to;
        data_deeper.global_activation_from = child_activation_to;
        data_deeper.global_tracking_from = child_tracking_to;
        data_deeper.global_budget_from = global_budget_to;
        data_deeper.parent_tracking_from = data->parent_tracking_to;

        explore_global_initialization(&data_deeper);
    }
}
// Enumerate consistent successor assignments for all child-states under current parent edge
// Precondition: old_* is complete snapshot; new_* starts as inactive/untracked
// Uses interval-based budget tracking for exact representation
void explore_global_selection(unsigned int start_child_id, unsigned int start_child_state_id, data_all_t* data) {
    struct StackFrame {
        unsigned int child_id;
        unsigned int child_state_id;
        std::vector<Edge*> edges;
        size_t edge_index;
        unsigned int ii;
        unsigned int stored_activation;
        unsigned int stored_tracking;
        BudgetSet stored_budget;
    };

    std::vector<StackFrame> stack;

    // Find next active non-final state with edges, starting from (cid, csid)
    auto find_next_choice = [&](unsigned int cid, unsigned int csid)
        -> std::tuple<unsigned int, unsigned int, std::vector<Edge*>>
    {
        while (cid < data->A->getChildrenSize()) {
            ChildAutomaton* child = data->A->getChild(cid);
            auto* states = child->getStates();

            while (csid < states->size()) {
                unsigned int i = data->cumulative_size[cid] + csid;

                // Check if budget is non-empty (active token)
                if (!data->old_value_of_children_state[i].empty() &&
                    !states->at(csid)->getFinal())
                {
                    State* child_state = states->at(csid);
                    auto* succs = child_state->getSuccessors(data->symbol->getId());
                    if (succs) {
                        return {cid, csid, std::vector<Edge*>(succs->begin(), succs->end())};
                    }
                }
                csid++;
            }
            cid++;
            csid = 0;
        }
        return {cid, csid, {}};
    };

    // Find first choice point
    auto [init_cid, init_csid, init_edges] = find_next_choice(start_child_id, start_child_state_id);

    if (init_edges.empty()) {
        explore_global_finalization(data);
        return;
    }

    stack.push_back({init_cid, init_csid, std::move(init_edges), 0, 0, 0, 0, BudgetSet{}});

    while (!stack.empty()) {
        StackFrame& frame = stack.back();

        // Restore state from previous edge iteration (if any)
        if (frame.edge_index > 0) {
            data->new_activation[frame.ii] = frame.stored_activation;
            data->new_tracked_children_state[frame.ii] = frame.stored_tracking;
            data->new_value_of_children_state[frame.ii] = frame.stored_budget;
        }

        bool pushed_new_frame = false;

        while (frame.edge_index < frame.edges.size()) {
            Edge* edge = frame.edges[frame.edge_index];
            unsigned int i = data->cumulative_size[frame.child_id] + frame.child_state_id;
            unsigned int ii = data->cumulative_size[frame.child_id] + edge->getTo()->getId();

            // Save for backtracking
            frame.ii = ii;
            frame.stored_activation = data->new_activation[ii];
            frame.stored_tracking = data->new_tracked_children_state[ii];
            frame.stored_budget = data->new_value_of_children_state[ii];

            const bool to_final = edge->getTo()->getFinal();

            // Propagate activation: if source is active and destination is not final
            if (data->old_activation[i] && !to_final) {
                data->new_activation[ii] = true;
            }
            // Propagate tracking: if source is tracked and destination is not final
            if (data->old_tracked_children_state[i] && !to_final) {
                data->new_tracked_children_state[ii] = true;
            }

            internal_weight_t abs_edge_value = (edge->getWeight()->getValue() < 0)
                ? to_internal(-(edge->getWeight()->getValue()), data->weight_scale)
                : to_internal(edge->getWeight()->getValue(), data->weight_scale);

            bool should_recurse = false;

            if (to_final) {
                // Termination: check if edge contribution satisfies budget requirement
                const BudgetSet& oldb = data->old_value_of_children_state[i];

                // For negative threshold, success means budget > 0 after consuming required
                // For positive threshold, success means required >= min_budget (edge covers need)
                bool success;
                if (data->negative_threshold) {
                    // For SumMinus: budget tracks remaining slack before exceeding |threshold|
                    // Termination edge uses full cost - no capping
                    // Success if budget - |edge_value| >= 0 (still have non-negative slack)
                    BudgetSet after = budgetset_after_edge(oldb, abs_edge_value, data->budget_limit, data->negative_threshold);
                    success = after.has_non_negative();
                } else {
                    // For SumPlus: cap edge value at threshold for termination check
                    internal_weight_t required =
                        (abs_edge_value >= data->abs_threshold) ? data->abs_threshold : abs_edge_value;
                    // Success if edge contribution can satisfy the minimum budget requirement
                    success = oldb.can_satisfy(required);
                }

                // Child terminated - clear its state regardless of success
                data->new_activation[ii] = false;
                data->new_tracked_children_state[ii] = false;
                data->new_value_of_children_state[ii] = BudgetSet{};  // Empty = inactive

                if (!success) {
                    // Child failed to meet threshold - set edge weight to 0
                    // but continue exploration (important for LimInf semantics)
                    data->global_edge_weight = 0;
                }
                should_recurse = true;
            }
            else if (!data->old_value_of_children_state[i].empty()) {
                const BudgetSet& oldb = data->old_value_of_children_state[i];

                // Compute next budget using interval subtraction
                // Use budget_limit (abs_threshold + 1) so abs_threshold stays as valid value
                BudgetSet nextb = budgetset_after_edge(oldb, abs_edge_value, data->budget_limit, data->negative_threshold);

                if (nextb.empty()) {
                    // No valid budget values after this edge
                    explore_global_failure(data);
                } else {
                    // Check if we need to intersect with existing budget at destination
                    if (data->new_value_of_children_state[ii].empty()) {
                        // First path to this state: set budget
                        data->new_value_of_children_state[ii] = std::move(nextb);
                        should_recurse = true;
                    } else {
                        // Multiple paths: intersect budgets (must be consistent)
                        BudgetSet inter = budgetset_intersect(data->new_value_of_children_state[ii], nextb, data->budget_limit);
                        if (inter.empty()) {
                            explore_global_failure(data);
                        } else {
                            data->new_value_of_children_state[ii] = std::move(inter);
                            should_recurse = true;
                        }
                    }
                }
            }

            frame.edge_index++;

            if (should_recurse) {
                auto [next_cid, next_csid, next_edges] =
                    find_next_choice(frame.child_id, frame.child_state_id + 1);

                if (next_edges.empty()) {
                    explore_global_finalization(data);
                    // Restore and continue to next edge
                    data->new_activation[ii] = frame.stored_activation;
                    data->new_tracked_children_state[ii] = frame.stored_tracking;
                    data->new_value_of_children_state[ii] = frame.stored_budget;
                } else {
                    // Push deeper frame
                    stack.push_back({next_cid, next_csid, std::move(next_edges), 0, 0, 0, 0, BudgetSet{}});
                    pushed_new_frame = true;
                    break;
                }
            } else {
                // Failure: restore and continue to next edge
                data->new_activation[ii] = frame.stored_activation;
                data->new_tracked_children_state[ii] = frame.stored_tracking;
                data->new_value_of_children_state[ii] = frame.stored_budget;
            }
        }

        if (!pushed_new_frame) {
            stack.pop_back();
        }
    }
}
void explore_global_parent_transition (data_all_t* data) {
    auto* succs = data->A->getStates()->at(data->parent_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    for (Edge* edge : *succs) {
        data->parent_state_id_to = edge->getTo()->getId();
        unsigned int child_id = (edge->getWeight()->getValue()).to_uint();  // edge weight encodes summoned child

        // Reset final pulse at start of any transition
        data->parent_tracking_to = (data->parent_tracking_from == 2u) ? 1u : data->parent_tracking_from;

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // Silent: can't end epoch, only advances parent control
            data->global_edge_weight = 1;
            explore_global_selection(0, 0, data);
        } else {
            // Non-silent: epoch has now seen observable action
            data->parent_tracking_to = 0u;

            unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
            unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

            // Spawn via OLD arrays -- selection reads OLD to determine activeness
            BudgetSet saved_old_budget = data->old_value_of_children_state[ii];
            unsigned int saved_old_activation = data->old_activation[ii];
            unsigned int saved_old_track = data->old_tracked_children_state[ii];

            // Clear NEW to avoid leftovers from prior parent edges
            BudgetSet saved_new_budget = data->new_value_of_children_state[ii];
            unsigned int saved_new_activation = data->new_activation[ii];
            unsigned int saved_new_track = data->new_tracked_children_state[ii];
            data->new_value_of_children_state[ii] = BudgetSet{};
            data->new_activation[ii] = false;
            data->new_tracked_children_state[ii] = false;

            if (saved_old_budget.empty()) {
                // Slot empty -- use full interval instead of enumeration
                // New spawns are ACTIVE but NOT TRACKED (belong to next epoch)
                // For Inf, budget must be exactly abs_threshold (no nondeterministic guessing)
                // We need to achieve EXACTLY the threshold, so start with that exact value
                BudgetSet full_budget;
                full_budget.fixed.push_back({data->abs_threshold, data->abs_threshold});
                full_budget.has_unlimited = false;

                data->old_value_of_children_state[ii] = full_budget;
                data->old_activation[ii] = true;          // Active
                data->old_tracked_children_state[ii] = false;  // NOT tracked (next epoch)
                // Edge weight = 1 if budget can achieve threshold (contains abs_threshold)
                data->global_edge_weight = full_budget.contains_value(data->abs_threshold, data->abs_threshold + 1) ? 1 : 0;
                explore_global_selection(0, 0, data);
            } else {
                // Token exists -- can't represent two tokens at same state, keep existing tracking
                data->old_activation[ii] = true;
                // Keep old tracking state (don't force to true)
                // Edge weight based on whether budget can achieve threshold
                data->global_edge_weight = saved_old_budget.contains_value(data->abs_threshold, data->abs_threshold + 1) ? 1 : 0;
                explore_global_selection(0, 0, data);
            }

            data->old_value_of_children_state[ii] = saved_old_budget;
            data->old_activation[ii] = saved_old_activation;
            data->old_tracked_children_state[ii] = saved_old_track;
            data->new_value_of_children_state[ii] = saved_new_budget;
            data->new_activation[ii] = saved_new_activation;
            data->new_tracked_children_state[ii] = saved_new_track;
        }
    }
}

void explore_global_initialization (data_all_t* data) {
    const unsigned int n = data->children_all;

    data->new_value_of_children_state.assign(n, BudgetSet{});  // Empty = inactive
    data->new_activation.assign(n, false);
    data->new_tracked_children_state.assign(n, false);

    data->old_value_of_children_state = data->global_budget_from;
    data->old_activation = data->global_activation_from;
    data->old_tracked_children_state = data->global_tracking_from;

    auto* alphabet = data->A->getStates()->at(data->parent_state_id_from)->getAlphabet();
    if (!alphabet) return;

    for (Symbol* symbol : *alphabet) {
        data->symbol = symbol;
        explore_global_parent_transition(data);
    }
}


// Returns a flattened automaton with 0/1 weights encoding threshold achievement
Automaton* NestedAutomaton::flatten_SumPlusMinus_Inf(value_function_t finite_aggregator,
                                                     weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Inf requires SumPlus or SumMinus");
    }
    return flatten_threshold_extremal_impl(this, finite_aggregator, threshold);

    // Compute scale factor to convert fractional weights to integers
    internal_weight_t weight_scale = compute_weight_scale(this);

    internal_weight_t abs_threshold;
    beyond_threshold_fn_t beyond_threshold;
    bool negative_threshold = (threshold <= 0);
    if (threshold > 0) {
        abs_threshold = to_internal(threshold, weight_scale);
        beyond_threshold = beyond_good_threshold;
    } else {
        // For negative threshold, use floor of |threshold| to ensure correct boundary
        abs_threshold = to_internal_trunc(-threshold, weight_scale);
        beyond_threshold = beyond_bad_threshold;
    }

    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->getChildrenSize() + 1; i++) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    unsigned int children_all = cumulative_size[this->getChildrenSize()];

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);

    parser->states.insert("@sink@");
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

    std::vector<BudgetSet> global_budget_initial(children_all, BudgetSet{});  // Empty = inactive
    std::vector<unsigned int> global_activation_initial(children_all, 0);
    std::vector<unsigned int> global_tracking_initial(children_all, 0);
    unsigned int parent_tracking_initial = 1;

    // State encoding: parent_id / budgets / activation / tracking / parent_tracking
    std::string global_initial;
    global_initial.reserve(64 + children_all * 32);
    global_initial.append(std::to_string(this->initial->getId()));
    global_initial.push_back('/');
    for (unsigned int i = 0; i < children_all; ++i) {
        if (i > 0) global_initial.push_back(',');
        global_initial.append(budgetset_to_string(global_budget_initial[i]));
    }
    global_initial.push_back('/');
    for (unsigned int i = 0; i < children_all; ++i) {
        global_initial.push_back('0');  // activation
    }
    global_initial.push_back('/');
    for (unsigned int i = 0; i < children_all; ++i) {
        global_initial.push_back('0');  // tracking
    }
    global_initial.push_back('1');  // parent_tracking

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    data_all_t data{};
    data.A = this;
    data.parser = parser;
    data.abs_threshold = abs_threshold;
    data.budget_limit = abs_threshold + 1;  // So abs_threshold is a valid budget value
    data.weight_scale = weight_scale;
    data.cumulative_size = cumulative_size.data();
    data.children_all = children_all;
    data.beyond_threshold = beyond_threshold;
    data.negative_threshold = negative_threshold;
    data.global_from = global_initial;
    data.parent_state_id_from = this->initial->getId();
    data.global_activation_from = global_activation_initial;
    data.global_tracking_from = global_tracking_initial;
    data.global_budget_from = global_budget_initial;
    data.parent_tracking_from = parent_tracking_initial;

    explore_global_initialization(&data);

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    return unnested;
}


/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////






static std::string vec_to_string(const std::vector<unsigned int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) s.push_back(',');
        s.append(std::to_string(v[i] & 3u));
    }
    return s;
}


// Worklist item for Min_f/Max_f under Sup/LimSup: track ONE distinguished child-token,
// and represent activation/tracking as variable-length vectors (no fixed-size bitmasks).
struct min_max_sup_work_item {
    std::string global_from;
    unsigned int parent_state_id_from;

    // 0/1 per flattened child-state
    std::vector<unsigned char> activation_from;
    std::vector<unsigned char> tracking_from;

    // Distinguished token being tracked for the (0/1) edge weight.
    // If inactive_from==true, the remaining fields are ignored.
    bool inactive_from = true;
    unsigned int witness_child_id_from = 0;
    unsigned int witness_child_state_id_from = 0;
    unsigned int witness_y_from = 0; // monotone bit: Max_f starts 0, Min_f starts 1
};

// Min_f/Max_f + Sup/LimSup flattening (single token tracking)
typedef struct global_exploration_data_min_max_supremum {
    // constraints
    NestedAutomaton* A = nullptr;
    Parser* parser = nullptr;
    weight_t threshold{};
    unsigned int* cumulative_size = nullptr;
    unsigned int children_all = 0;
    unsigned int finite_is_max = 0; // 0 = Min_f, 1 = Max_f

    // epoch reset vector (all ones)
    std::vector<unsigned char> track_them_all;

    // worklist for iterative DFS
    std::vector<min_max_sup_work_item>* worklist = nullptr;

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int parent_state_id_from = 0;
    std::vector<unsigned char> activation_from;
    std::vector<unsigned char> tracking_from;
    bool inactive_from = true;
    unsigned int witness_child_id_from = 0;
    unsigned int witness_child_state_id_from = 0;
    unsigned int witness_y_from = 0;

    // initialized per-symbol
    Symbol* symbol = nullptr;
    std::vector<unsigned char> old_activation;
    std::vector<unsigned char> old_tracking;
    std::vector<unsigned char> new_activation;
    std::vector<unsigned char> new_tracking;

    // computed (output accumulators)
    unsigned int parent_state_id_to = 0;
    weight_t global_edge_weight = 0;
    bool inactive_to = true;
    unsigned int witness_child_id_to = 0;
    unsigned int witness_child_state_id_to = 0;
    unsigned int witness_y_to = 0;
} data_min_max_supremum_t;

static void explore_global_initialization_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_parent_transition_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_child_transition_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_selection_min_max_supremum(unsigned int child_id, unsigned int child_state_id,
                                                      data_min_max_supremum_t* data);
static void explore_global_finalization_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_failure_min_max_supremum(data_min_max_supremum_t* data);

// static inline bool tracking_all_zero(const std::vector<unsigned char>& v) {
//     for (unsigned char b : v) {
//         if (b != 0) return false;
//     }
//     return true;
// }

// static std::string bits_to_string(const std::vector<unsigned char>& v) {
//     std::string s;
//     s.reserve(v.size());
//     for (unsigned char b : v) {
//         s.push_back(b ? '1' : '0');
//     }
//     return s;
// }

static inline unsigned int min_max_y_update(const weight_t& edge_value,
                                            unsigned int y_current,
                                            const data_min_max_supremum_t* data) {
    const bool pass = !(edge_value < data->threshold); // edge_value >= threshold
    if (data->finite_is_max) {
        // Max_f: y' = y OR pass
        return (y_current != 0u || pass) ? 1u : 0u;
    }
    // Min_f: y' = y AND pass
    return (y_current != 0u && pass) ? 1u : 0u;
}

static void explore_global_failure_min_max_supremum(data_min_max_supremum_t* data) {
#ifdef DEBUG
    std::cout << "FAILURE: " << data->symbol->getName() << " from " << data->global_from << " -> @sink@" << std::endl;
#endif
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

static void explore_global_finalization_min_max_supremum(data_min_max_supremum_t* data) {
    // Compute destination activation/tracking vectors.
    std::vector<unsigned char> activation_to = data->new_activation; // copy
    std::vector<unsigned char> tracking_to   = data->new_tracking;   // copy

    bool global_final = false;
    // Epoch boundary: exactly as in explore_global_finalization_supremum (Sum+/Sum-)
    if (tracking_all_zero(data->tracking_from)) {
        tracking_to = data->track_them_all; // reset obligations
        global_final = data->A->getStates()->at(data->parent_state_id_to)->getFinal();
    }

    // Encode destination state:
    //   parent_id/activation_bits/tracking_bits/[child_id/child_state/y | @inactive@]
    std::string global_to;
    global_to.reserve(64 + data->children_all * 2);
    global_to.append(std::to_string(data->parent_state_id_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(activation_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(tracking_to));

    if (data->inactive_to) {
        global_to.append("/@inactive@");
    } else {
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_child_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_child_state_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_y_to));
    }

    data->parser->edges.insert({
        { data->symbol->getName(), data->global_edge_weight },
        { data->global_from, global_to }
    });

#ifdef DEBUG
    std::cout << "SUCCESS: " << data->symbol->getName() << " : " << data->global_edge_weight.to_float()
              << ", " << data->global_from << " -> " << global_to << std::endl;
#endif

    if (global_final) {
        data->parser->final_states.insert(global_to);
    }

    // Iterative DFS: push to worklist on first discovery
    if (!data->parser->states.contains(global_to)) {
        data->parser->states.insert(global_to);
        data->worklist->push_back({
            global_to,
            data->parent_state_id_to,
            std::move(activation_to),
            std::move(tracking_to),
            data->inactive_to,
            data->witness_child_id_to,
            data->witness_child_state_id_to,
            data->witness_y_to
        });
    }
}

static void explore_global_selection_min_max_supremum(unsigned int child_id,
                                                      unsigned int child_state_id,
                                                      data_min_max_supremum_t* data) {
    // Skip the distinguished token's FROM location (handled in explore_global_child_transition_min_max_supremum)
    if (!data->inactive_from &&
        child_id == data->witness_child_id_from &&
        child_state_id == data->witness_child_state_id_from) {
        explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
        return;
    }

    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_activation[i] == 0) {
                explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
                return;
            }

            if (states->at(child_state_id)->getFinal()) {
                // Background final states terminate immediately.
                explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (succs) {
                for (Edge* edge : *succs) {
                    // If successor is final: terminate in the same symbol (do not propagate)
                    if (edge->getTo()->getFinal()) {
                        explore_global_selection_min_max_supremum(child_id + 1, 0, data);
                        continue;
                    }

                    const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();
                    const unsigned char stored_tracking = data->new_tracking[ii];
                    const unsigned char stored_activation = data->new_activation[ii];

                    if (data->old_tracking[i] == 1) {
                        data->new_tracking[ii] = 1;
                    }
                    if (data->old_activation[i] == 1) {
                        data->new_activation[ii] = 1;
                    }

                    explore_global_selection_min_max_supremum(child_id + 1, 0, data);

                    data->new_tracking[ii] = stored_tracking;
                    data->new_activation[ii] = stored_activation;
                }
            } else {
                // No successors on this symbol: treat as silent "no move" for background
                explore_global_selection_min_max_supremum(child_id + 1, 0, data);
            }
        } else {
            explore_global_selection_min_max_supremum(child_id + 1, 0, data);
        }
    } else {
        explore_global_finalization_min_max_supremum(data);
    }
}

static void explore_global_child_transition_min_max_supremum(data_min_max_supremum_t* data) {
    if (data->inactive_from) {
        data->inactive_to = true;
        explore_global_selection_min_max_supremum(0, 0, data);
        return;
    }

    ChildAutomaton* child = data->A->getChild(data->witness_child_id_from);
    State* child_state = child->getStates()->at(data->witness_child_state_id_from);

    // Should normally not happen if we enforce "terminate in same symbol", but handle robustly.
    if (child_state->getFinal()) {
        if (data->witness_y_from == 1u) {
            data->inactive_to = true;
            explore_global_selection_min_max_supremum(0, 0, data);
        } else {
            explore_global_failure_min_max_supremum(data);
        }
        return;
    }

    const unsigned int i = data->cumulative_size[data->witness_child_id_from] + data->witness_child_state_id_from;

    auto* succs = child_state->getSuccessors(data->symbol->getId());
    if (!succs) {
        explore_global_failure_min_max_supremum(data);
        return;
    }

    for (Edge* child_edge : *succs) {
        const unsigned int to_state_id = (unsigned int)child_edge->getTo()->getId();
        const unsigned int ii = data->cumulative_size[data->witness_child_id_from] + to_state_id;

        const unsigned char stored_tracking = data->new_tracking[ii];
        const unsigned char stored_activation = data->new_activation[ii];

        const unsigned int y_next = min_max_y_update(child_edge->getWeight()->getValue(), data->witness_y_from, data);

        // For Min_f, once y becomes 0 it can never recover, so the branch is dead.
        if (!data->finite_is_max && y_next == 0u) {
            explore_global_failure_min_max_supremum(data);
            continue;
        }

        if (child_edge->getTo()->getFinal()) {
            // Terminate in the same symbol; success requires y_next==1
            if (y_next == 1u) {
                data->inactive_to = true;
                explore_global_selection_min_max_supremum(0, 0, data);
            } else {
                explore_global_failure_min_max_supremum(data);
            }
            continue;
        }

        // Propagate activation/tracking for the witness token to its successor.
        if (data->old_tracking[i] == 1) {
            data->new_tracking[ii] = 1;
        }
        if (data->old_activation[i] == 1) {
            data->new_activation[ii] = 1;
        }

        data->inactive_to = false;
        data->witness_child_id_to = data->witness_child_id_from;
        data->witness_child_state_id_to = to_state_id;
        data->witness_y_to = y_next;

        explore_global_selection_min_max_supremum(0, 0, data);

        data->new_tracking[ii] = stored_tracking;
        data->new_activation[ii] = stored_activation;
    }
}

static void explore_global_parent_transition_min_max_supremum(data_min_max_supremum_t* data) {
    auto* succs = data->A->getStates()->at(data->parent_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    // Save witness context: each parent edge explores independently
    const bool saved_inactive = data->inactive_from;
    const unsigned int saved_w_child = data->witness_child_id_from;
    const unsigned int saved_w_state = data->witness_child_state_id_from;
    const unsigned int saved_w_y = data->witness_y_from;

    for (Edge* parent_edge : *succs) {
        data->inactive_from = saved_inactive;
        data->witness_child_id_from = saved_w_child;
        data->witness_child_state_id_from = saved_w_state;
        data->witness_y_from = saved_w_y;

        data->parent_state_id_to = static_cast<unsigned int>(parent_edge->getTo()->getId());
        const unsigned int child_id = static_cast<unsigned int>(parent_edge->getWeight()->getValue().to_float());

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // Silent: no spawn, weight 0
            data->global_edge_weight = 0;
            explore_global_child_transition_min_max_supremum(data);
        } else {
            const unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
            const unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

            // Mark spawned token as active in OLD arrays (so selection sees it)
            const unsigned char prev_act = data->old_activation[ii];
            data->old_activation[ii] = 1;

            // Choice 1: NOT summon as witness (spawn only as background)
            data->global_edge_weight = 0;
            explore_global_child_transition_min_max_supremum(data);

            // Choice 2: start tracking as witness (only if no witness already tracked)
            if (saved_inactive) {
                data->global_edge_weight = 1;
                data->inactive_from = false;
                data->witness_child_id_from = child_id;
                data->witness_child_state_id_from = summoned_child_state_id;
                data->witness_y_from = data->finite_is_max ? 0u : 1u;
                explore_global_child_transition_min_max_supremum(data);
            }

            data->old_activation[ii] = prev_act;
        }
    }
}

static void explore_global_initialization_min_max_supremum(data_min_max_supremum_t* data) {
    const unsigned int n = data->children_all;

    data->old_activation.resize(n);
    data->old_tracking.resize(n);
    data->new_activation.assign(n, 0);
    data->new_tracking.assign(n, 0);

    for (unsigned int i = 0; i < n; ++i) {
        data->old_activation[i] = (i < data->activation_from.size()) ? (data->activation_from[i] ? 1 : 0) : 0;
        data->old_tracking[i]   = (i < data->tracking_from.size()) ? (data->tracking_from[i] ? 1 : 0) : 0;
    }

    auto* alphabet = data->A->getStates()->at(data->parent_state_id_from)->getAlphabet();
    if (!alphabet) return;

    // Save witness context that must be restored between symbols
    const bool saved_inactive = data->inactive_from;
    const unsigned int saved_w_child = data->witness_child_id_from;
    const unsigned int saved_w_state = data->witness_child_state_id_from;
    const unsigned int saved_w_y = data->witness_y_from;

    for (Symbol* symbol : *alphabet) {
        // Restore witness context for each symbol
        data->inactive_from = saved_inactive;
        data->witness_child_id_from = saved_w_child;
        data->witness_child_state_id_from = saved_w_state;
        data->witness_y_from = saved_w_y;

        data->symbol = symbol;
        explore_global_parent_transition_min_max_supremum(data);
    }
}

// Returns a flattened automaton with 0/1 weights encoding threshold achievement
Automaton* NestedAutomaton::flatten_MinMax_Sup(value_function_t finite_aggregator,
                                               weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Sup requires Max_f or Min_f");
    }
    return flatten_threshold_extremal_impl(this, finite_aggregator, threshold);

#endif
