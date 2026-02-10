// ARCHIVED: Old versions of Inf flattening functions before the activation/tracking separation fix.
// Date: 2026-02-03
// Issue: Epoch boundaries were not detected correctly because new spawns were immediately tracked,
//        preventing epochs from completing when every transition spawns a new child.
// Fix: Separate activation (child exists) from tracking (child must complete this epoch).
//      New spawns are active but NOT tracked. At epoch boundary, promote activation to tracking.

#if 0  // This file is for archival purposes only - do not compile

//=============================================================================
// OLD VERSION: flatten_SumPlusMinus_Inf
//=============================================================================

// Old data structure - no separate activation tracking
typedef struct global_exploration_data_all {
    // constraints
    NestedAutomaton* A;
    Parser* parser;
    internal_weight_t abs_threshold;
    internal_weight_t weight_scale = 1; // scale factor for fractional weights
    unsigned int* cumulative_size;
    unsigned int children_all;
    beyond_threshold_fn_t beyond_threshold;

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int parent_state_id_from;
    std::vector<unsigned int> global_tracking_from;     // size = children_all, values 0 or 1
    std::vector<internal_weight_t> global_budget_from;  // size = children_all
    unsigned int parent_tracking_from;

    // initialized per-symbol
    std::vector<internal_weight_t> old_value_of_children_state;
    std::vector<unsigned int> old_tracked_children_state;
    Symbol* symbol;

    // computed (output accumulators)
    unsigned int parent_state_id_to;
    internal_weight_t global_edge_weight;
    std::vector<unsigned int> new_tracked_children_state;
    std::vector<internal_weight_t> new_value_of_children_state;
    unsigned int parent_tracking_to;
} data_all_t;

void explore_global_finalization (data_all_t* data) {
    std::vector<internal_weight_t> global_budget_to = data->new_value_of_children_state;
    std::vector<unsigned int> child_tracking_to = data->new_tracked_children_state;

    bool global_final = false;

    bool all_untracked = true;
    for (unsigned int i = 0; i < data->children_all; ++i) {
        if (child_tracking_to[i]) { all_untracked = false; break; }
    }

    if (all_untracked && data->parent_tracking_to == 0) {
        for (unsigned int i = 0; i < data->children_all; ++i) {
            child_tracking_to[i] = 1;
        }
        data->parent_tracking_to = 1;
        State* parent_to_state = data->A->getStates()->at(data->parent_state_id_to);
        global_final = parent_to_state->getFinal();
    }

    std::string global_to;
    global_to.reserve(64 + data->children_all * 12);
    global_to.append(std::to_string(data->parent_state_id_to));
    global_to.push_back('/');
    for (unsigned int i = 0; i < data->children_all; ++i) {
        if (i > 0) global_to.push_back(',');
        global_to.append(std::to_string(global_budget_to[i]));
    }
    global_to.push_back('/');
    for (unsigned int i = 0; i < data->children_all; ++i) {
        global_to.push_back(child_tracking_to[i] ? '1' : '0');
    }
    global_to.push_back(data->parent_tracking_to ? '1' : '0');

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
        data_deeper.weight_scale = data->weight_scale;
        data_deeper.cumulative_size = data->cumulative_size;
        data_deeper.children_all = data->children_all;
        data_deeper.beyond_threshold = data->beyond_threshold;

        data_deeper.global_from = global_to;
        data_deeper.parent_state_id_from = data->parent_state_id_to;
        data_deeper.global_tracking_from = child_tracking_to;
        data_deeper.global_budget_from = global_budget_to;
        data_deeper.parent_tracking_from = data->parent_tracking_to;

        explore_global_initialization(&data_deeper);
    }
}

void explore_global_selection(unsigned int start_child_id, unsigned int start_child_state_id, data_all_t* data) {
    struct StackFrame {
        unsigned int child_id;
        unsigned int child_state_id;
        std::vector<Edge*> edges;
        size_t edge_index;
        unsigned int ii;
        unsigned int stored_tracking;
        internal_weight_t stored_budget;
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

                if (data->old_value_of_children_state[i] != data->abs_threshold + 1 &&
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

    stack.push_back({init_cid, init_csid, std::move(init_edges), 0, 0, 0, 0});

    while (!stack.empty()) {
        StackFrame& frame = stack.back();

        // Restore state from previous edge iteration (if any)
        if (frame.edge_index > 0) {
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
            frame.stored_tracking = data->new_tracked_children_state[ii];
            frame.stored_budget = data->new_value_of_children_state[ii];

            const bool to_final = edge->getTo()->getFinal();

            if (data->old_tracked_children_state[i] && !to_final) {
                data->new_tracked_children_state[ii] = true;
            }

            internal_weight_t abs_edge_value = (edge->getWeight()->getValue() < 0)
                ? to_internal(-(edge->getWeight()->getValue()), data->weight_scale)
                : to_internal(edge->getWeight()->getValue(), data->weight_scale);

            bool should_recurse = false;

            if (to_final) {
                internal_weight_t oldb = data->old_value_of_children_state[i];
                internal_weight_t required =
                    (abs_edge_value >= data->abs_threshold) ? data->abs_threshold : abs_edge_value;

                if (oldb != required) {
                    explore_global_failure(data);
                } else {
                    data->new_tracked_children_state[ii] = false;
                    data->new_value_of_children_state[ii] = data->abs_threshold + 1;
                    should_recurse = true;
                }
            }
            else if (data->old_value_of_children_state[i] <= data->abs_threshold) {
                internal_weight_t oldb = data->old_value_of_children_state[i];

                if (oldb < abs_edge_value) {
                    explore_global_failure(data);
                } else {
                    internal_weight_t nextb = oldb - abs_edge_value;

                    if (data->new_value_of_children_state[ii] == data->abs_threshold + 1) {
                        data->new_value_of_children_state[ii] = nextb;
                        should_recurse = true;
                    } else if (data->new_value_of_children_state[ii] != nextb) {
                        explore_global_failure(data);
                    } else {
                        should_recurse = true;
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
                    data->new_tracked_children_state[ii] = frame.stored_tracking;
                    data->new_value_of_children_state[ii] = frame.stored_budget;
                } else {
                    // Push deeper frame
                    stack.push_back({next_cid, next_csid, std::move(next_edges), 0, 0, 0, 0});
                    pushed_new_frame = true;
                    break;
                }
            } else {
                // Failure: restore and continue to next edge
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

        data->parent_tracking_to = data->parent_tracking_from;

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // Silent: can't end epoch, only advances parent control
            data->global_edge_weight = 1;
            explore_global_selection(0, 0, data);
        } else {
            // Non-silent: epoch has now seen observable action
            data->parent_tracking_to = 0;

            unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
            unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

            // Spawn via OLD arrays -- selection reads OLD to determine activeness
            internal_weight_t saved_old_budget = data->old_value_of_children_state[ii];
            unsigned int saved_old_track = data->old_tracked_children_state[ii];

            // Clear NEW to avoid leftovers from prior parent edges
            internal_weight_t saved_new_budget = data->new_value_of_children_state[ii];
            unsigned int saved_new_track = data->new_tracked_children_state[ii];
            data->new_value_of_children_state[ii] = data->abs_threshold + 1;
            data->new_tracked_children_state[ii] = false;

            if (saved_old_budget == data->abs_threshold + 1) {
                // Slot empty -- nondeterministically guess initial budget
                // Save entire new arrays to restore between iterations (avoid cross-contamination)
                std::vector<internal_weight_t> saved_new_budgets_all = data->new_value_of_children_state;
                std::vector<unsigned int> saved_new_tracks_all = data->new_tracked_children_state;
                for (internal_weight_t abs_weight = 0; abs_weight <= data->abs_threshold; ++abs_weight) {
                    data->old_value_of_children_state[ii] = abs_weight;
                    data->old_tracked_children_state[ii] = true;
                    data->global_edge_weight = data->beyond_threshold(abs_weight, data->abs_threshold);
                    explore_global_selection(0, 0, data);
                    // Restore new arrays for next iteration
                    data->new_value_of_children_state = saved_new_budgets_all;
                    data->new_tracked_children_state = saved_new_tracks_all;
                }
            } else {
                // Token exists -- can't represent two tokens at same state, just ensure tracking
                data->old_tracked_children_state[ii] = true;
                data->global_edge_weight = data->beyond_threshold(saved_old_budget, data->abs_threshold);
                explore_global_selection(0, 0, data);
            }

            data->old_value_of_children_state[ii] = saved_old_budget;
            data->old_tracked_children_state[ii] = saved_old_track;
            data->new_value_of_children_state[ii] = saved_new_budget;
            data->new_tracked_children_state[ii] = saved_new_track;
        }
    }
}

void explore_global_initialization (data_all_t* data) {
    const unsigned int n = data->children_all;

    data->new_value_of_children_state.assign(n, data->abs_threshold + 1);
    data->new_tracked_children_state.assign(n, false);

    data->old_value_of_children_state = data->global_budget_from;
    data->old_tracked_children_state = data->global_tracking_from;

    auto* alphabet = data->A->getStates()->at(data->parent_state_id_from)->getAlphabet();
    if (!alphabet) return;

    for (Symbol* symbol : *alphabet) {
        data->symbol = symbol;
        explore_global_parent_transition(data);
    }
}

// Main flatten function (relevant parts)
Automaton* NestedAutomaton::flatten_SumPlusMinus_Inf(value_function_t finite_aggregator,
                                                     weight_t threshold) {
    // ... (weight_scale, abs_threshold, beyond_threshold setup) ...

    std::vector<internal_weight_t> global_budget_initial(children_all, abs_threshold + 1);
    std::vector<unsigned int> global_tracking_initial(children_all, 0);
    unsigned int parent_tracking_initial = 1;

    std::string global_initial;
    global_initial.reserve(64 + children_all * 12);
    global_initial.append(std::to_string(this->initial->getId()));
    global_initial.push_back('/');
    for (unsigned int i = 0; i < children_all; ++i) {
        if (i > 0) global_initial.push_back(',');
        global_initial.append(std::to_string(global_budget_initial[i]));
    }
    global_initial.push_back('/');
    for (unsigned int i = 0; i < children_all; ++i) {
        global_initial.push_back('0');
    }
    global_initial.push_back('1');

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    data_all_t data{};
    data.A = this;
    data.parser = parser;
    data.abs_threshold = abs_threshold;
    data.weight_scale = weight_scale;
    data.cumulative_size = cumulative_size.data();
    data.children_all = children_all;
    data.beyond_threshold = beyond_threshold;
    data.global_from = global_initial;
    data.parent_state_id_from = this->initial->getId();
    data.global_tracking_from = global_tracking_initial;
    data.global_budget_from = global_budget_initial;
    data.parent_tracking_from = parent_tracking_initial;

    explore_global_initialization(&data);

    // ... (rest) ...
}

//=============================================================================
// OLD VERSION: flatten_MinMax_Inf
//=============================================================================

static void explore_global_finalization_min_max(data_min_max_t* data) {
    // must handle finals in NEW arrays before packing/acceptance
    if (!cleanup_new_on_finals_min_max(data)) {
        explore_global_failure_min_max(data);
        return;
    }

    // Build vectors from NEW arrays
    std::vector<unsigned int> to_0_0(data->children_all);
    std::vector<unsigned int> to_0_1(data->children_all);
    std::vector<unsigned int> to_1_0(data->children_all);
    std::vector<unsigned int> to_1_1(data->children_all);
    for (unsigned int i = 0; i < data->children_all; ++i) {
        to_0_0[i] = data->new_0_0[i] & 3u;
        to_0_1[i] = data->new_0_1[i] & 3u;
        to_1_0[i] = data->new_1_0[i] & 3u;
        to_1_1[i] = data->new_1_1[i] & 3u;
    }

    bool global_final = false;

    // Check if any child is tracked (bit 1 set in any of the four arrays)
    bool any_tracked = false;
    for (unsigned int i = 0; i < data->children_all && !any_tracked; ++i) {
        if ((to_0_0[i] | to_0_1[i] | to_1_0[i] | to_1_1[i]) & 2u) {
            any_tracked = true;
        }
    }

    // if no tracked children and parent saw a non-silent since last completion:
    // emit a one-step "final pulse" (2). This forces infinitely many non-silent segments
    // to visit final states infinitely often.
    if (!any_tracked && data->parent_tracking_to == 0u) {
        data->parent_tracking_to = 2u; // final pulse
        global_final = data->A->getStates()->at(data->parent_state_id_to)->getFinal();
    }

    // Build destination state string.
    std::string global_to;
    global_to.reserve(96 + data->children_all * 8);
    global_to.append(std::to_string(data->parent_state_id_to));
    global_to.push_back('/');
    global_to.append(std::to_string(data->parent_tracking_to));
    global_to.push_back('/');
    global_to.append(vec_to_string(to_0_0));
    global_to.push_back('/');
    global_to.append(vec_to_string(to_0_1));
    global_to.push_back('/');
    global_to.append(vec_to_string(to_1_0));
    global_to.push_back('/');
    global_to.append(vec_to_string(to_1_1));

    if (global_final) {
        data->parser->final_states.insert(global_to);
    }

    data->parser->edges.insert({
        { data->symbol->getName(), data->global_edge_weight },
        { data->global_from, global_to }
    });

    // Iterative DFS: push to worklist instead of recursive call
    if (!data->parser->states.contains(global_to)) {
        data->parser->states.insert(global_to);
        data->worklist->push_back({
            global_to,
            data->parent_state_id_to,
            data->parent_tracking_to,
            std::move(to_0_0),
            std::move(to_0_1),
            std::move(to_1_0),
            std::move(to_1_1)
        });
    }
}

static void explore_global_parent_transition_min_max(data_min_max_t* data) {
    auto* succs = data->A->getStates()->at(data->parent_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    auto clear_new = [&]() {
        std::fill(data->new_0_0.begin(), data->new_0_0.end(), 0u);
        std::fill(data->new_0_1.begin(), data->new_0_1.end(), 0u);
        std::fill(data->new_1_0.begin(), data->new_1_0.end(), 0u);
        std::fill(data->new_1_1.begin(), data->new_1_1.end(), 0u);
    };

    auto activate_tracked = [](unsigned int& cell) {
        cell = 3u; // force active+tracked
    };

    for (Edge* edge : *succs) {
        data->parent_state_id_to = (unsigned int)edge->getTo()->getId();

        const unsigned int child_id =
            (unsigned int)edge->getWeight()->getValue().to_uint();

        // Default: preserve tracking, but make the "final pulse" (2) last only one step.
        data->parent_tracking_to = (data->parent_tracking_from == 2u) ? 1u : data->parent_tracking_from;

        clear_new();

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // silent: identity element of the OUTER aggregator
            data->global_edge_weight = (data->inf_or_sup == 0u) ? weight_t(1) : weight_t(0);
            explore_global_selection_min_max(0, 0, data);
            continue;
        }

        // non-silent
        data->parent_tracking_to = 0u;

        const unsigned int summoned_child_state_id = (unsigned int)data->A->getChild(child_id)->initial->getId();
        const unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

        // Save old cells we might mutate for spawning.
        const unsigned int saved_0_0 = data->old_0_0[ii];
        const unsigned int saved_0_1 = data->old_0_1[ii];
        const unsigned int saved_1_0 = data->old_1_0[ii];
        const unsigned int saved_1_1 = data->old_1_1[ii];

        // Birth status depends on FINITE aggregator:
        //   Max_f: current starts 0  -> *_0
        //   Min_f: current starts 1  -> *_1
        if (data->finite_is_max) {
            // objective 0 -> 0_0 (edge weight 0)
            activate_tracked(data->old_0_0[ii]);
            data->global_edge_weight = weight_t(0);
            explore_global_selection_min_max(0, 0, data);
            data->old_0_0[ii] = saved_0_0;

            // objective 1 -> 1_0 (edge weight 1)
            activate_tracked(data->old_1_0[ii]);
            data->global_edge_weight = weight_t(1);
            explore_global_selection_min_max(0, 0, data);
            data->old_1_0[ii] = saved_1_0;
        } else {
            // objective 0 -> 0_1 (edge weight 0)
            activate_tracked(data->old_0_1[ii]);
            data->global_edge_weight = weight_t(0);
            explore_global_selection_min_max(0, 0, data);
            data->old_0_1[ii] = saved_0_1;

            // objective 1 -> 1_1 (edge weight 1)
            activate_tracked(data->old_1_1[ii]);
            data->global_edge_weight = weight_t(1);
            explore_global_selection_min_max(0, 0, data);
            data->old_1_1[ii] = saved_1_1;
        }

        // Restore any untouched cells too
        data->old_0_0[ii] = saved_0_0;
        data->old_0_1[ii] = saved_0_1;
        data->old_1_0[ii] = saved_1_0;
        data->old_1_1[ii] = saved_1_1;
    }
}

// Main flatten function initial state setup (relevant parts)
Automaton* NestedAutomaton::flatten_MinMax_Inf(value_function_t finite_aggregator,
                                               weight_t threshold) {
    // ... (setup) ...

    // Helper to create zero vector string
    std::string zero_vec_str = vec_to_string(std::vector<unsigned int>(children_all, 0u));

    // GLOBAL INITIAL
    // encoding: parent_state_id/parent_tracking/from_0_0/from_0_1/from_1_0/from_1_1
    std::string global_initial;
    global_initial.reserve(96 + children_all * 8);
    global_initial.append(std::to_string(this->initial->getId()));
    global_initial.push_back('/');
    global_initial.append(std::to_string(1u));   // parent_tracking initially "waiting"
    global_initial.push_back('/');
    global_initial.append(zero_vec_str);
    global_initial.push_back('/');
    global_initial.append(zero_vec_str);
    global_initial.push_back('/');
    global_initial.append(zero_vec_str);
    global_initial.push_back('/');
    global_initial.append(zero_vec_str);

    // ... (rest) ...
}

#endif // archival only
