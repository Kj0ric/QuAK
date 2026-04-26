// Archived nested-automata flattening implementations.
//
// This file is intentionally not part of the active quak build. It preserves
// removed alternatives after cached/witness-cached flatteners became the main
// decision-query implementations. Some snippets depend on helpers from the
// historical NestedAutomaton.cpp layout and are kept for reference rather than
// standalone compilation.

// =============================================================================
// Archived old body after flatten_SumPlusMinus_Sup cached dispatch
// =============================================================================

Automaton* NestedAutomaton::flatten_SumPlusMinus_Sup(value_function_t finite_aggregator,
                                                     weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Sup requires SumPlus or SumMinus");
    }
    return this->flatten_SumPlusMinus_Sup_witness_cached(finite_aggregator, threshold);

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

// =============================================================================
// Archived old body after flatten_SumPlusMinus_Inf cached dispatch
// =============================================================================

Automaton* NestedAutomaton::flatten_SumPlusMinus_Inf(value_function_t finite_aggregator,
                                                     weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Inf requires SumPlus or SumMinus");
    }
    return this->flatten_SumPlusMinus_Inf_cached(finite_aggregator, threshold);

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

// =============================================================================
// Archived old body after flatten_MinMax_Sup cached dispatch
// =============================================================================

Automaton* NestedAutomaton::flatten_MinMax_Sup(value_function_t finite_aggregator,
                                               weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Sup requires Max_f or Min_f");
    }
    return this->flatten_MinMax_Sup_witness_cached(finite_aggregator, threshold);

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
    global_initial.append("/@inactive@");

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
        finite_is_max ? 0u : 1u
    });

    data_min_max_supremum_t data{};
    data.A = this;
    data.parser = parser;
    data.threshold = threshold;
    data.cumulative_size = cumulative_size.data();
    data.children_all = children_all;
    data.finite_is_max = finite_is_max;
    data.track_them_all = std::move(track_them_all);
    data.worklist = &worklist;

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

        explore_global_initialization_min_max_supremum(&data);
    }

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    return unnested;
}

// =============================================================================
// Archived old body after flatten_MinMax_Inf cached dispatch
// =============================================================================

Automaton* NestedAutomaton::flatten_MinMax_Inf(value_function_t finite_aggregator,
                                                      weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf requires Max_f or Min_f");
    }
    return this->flatten_MinMax_Inf_cached(finite_aggregator, threshold);

    const bool is_max = (finite_aggregator == Max_f);

    // Build cumulative_size for child state indexing
    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i <= this->getChildrenSize(); ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int total_child_states = cumulative_size[this->getChildrenSize()];

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);

    // Sink state with self-loops
    parser->states.insert("@sink@");
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

    // Initial state: parent at initial, waiting phase, no active children
    std::vector<unsigned int> init_status(total_child_states, 0);
    std::vector<bool> init_tracked(total_child_states, false);
    bool init_epoch_nonempty = false;
    std::string initial_state = encode_simple_state(this->initial->getId(), 1, init_status, init_tracked, total_child_states, init_epoch_nonempty);

    parser->states.insert(initial_state);
    parser->initial = initial_state;

    // Worklist-based exploration
    std::vector<simple_work_item> worklist;
    worklist.push_back({initial_state, (unsigned int)this->initial->getId(), 1, init_status, init_tracked, init_epoch_nonempty});

    while (!worklist.empty()) {
        simple_work_item current = std::move(worklist.back());
        worklist.pop_back();

        State* parent_state = this->getStates()->at(current.parent_state_id);

        for (Symbol* symbol : *parent_state->getAlphabet()) {
            auto* parent_succs = parent_state->getSuccessors(symbol->getId());
            if (!parent_succs) continue;

            for (Edge* parent_edge : *parent_succs) {
                unsigned int next_parent_id = parent_edge->getTo()->getId();
                unsigned int child_id = (unsigned int)parent_edge->getWeight()->getValue().to_float();

                bool is_silent = (this->getChild(child_id)->getStates()->size() == 1);

                // Start with copies of current state
                std::vector<unsigned int> new_status = current.child_status;
                std::vector<bool> new_tracked = current.tracked;
                unsigned int new_phase = current.parent_phase;

                // Handle phase transition from final_pulse
                bool reset_epoch = false;
                if (current.parent_phase == 2) {
                    new_phase = 0;  // back to active
                    reset_epoch = true;  // new epoch starts; reset productivity tracker
                }

                // Spawn new child (if non-silent)
                if (!is_silent) {
                    unsigned int spawn_state = this->getChild(child_id)->initial->getId();
                    unsigned int spawn_idx = cumulative_size[child_id] + spawn_state;

                    // For Max_f: status encodes (active, seen_high) as bits
                    //   0 = inactive, 1 = active+low, 3 = active+high
                    // For Min_f: status is just 0=inactive, 1=active
                    if (is_max) {
                        new_status[spawn_idx] |= 1u;  // set active bit, seen_high stays 0
                    } else {
                        new_status[spawn_idx] = 1;  // active
                    }
                    // New children are NOT tracked (belong to next epoch)

                    if (new_phase == 1) {  // was waiting
                        new_phase = 0;  // now active
                    }
                }

                // Process all active children - need to handle non-determinism properly
                // We'll explore all combinations of child transitions

                // Collect active children and their possible transitions
                struct ChildMove {
                    unsigned int cid;
                    unsigned int from_sid;
                    unsigned int from_idx;
                    unsigned int to_sid;
                    unsigned int to_idx;
                    weight_t edge_w;
                    bool to_final;
                };

                std::vector<std::vector<ChildMove>> all_moves;
                bool has_stuck_child = false;

                for (unsigned int cid = 0; cid < this->getChildrenSize() && !has_stuck_child; ++cid) {
                    ChildAutomaton* child = this->getChild(cid);
                    auto* child_states = child->getStates();
                    if (!child_states) continue;

                    for (unsigned int sid = 0; sid < child_states->size() && !has_stuck_child; ++sid) {
                        unsigned int idx = cumulative_size[cid] + sid;
                        if ((new_status[idx] & 1u) == 0) continue;  // not active

                        State* child_state = child_states->at(sid);

                        // Skip if child is at final state (should have terminated)
                        if (child_state->getFinal()) continue;

                        auto* child_succs = child_state->getSuccessors(symbol->getId());

                        if (!child_succs || child_succs->empty()) {
                            has_stuck_child = true;
                            break;
                        }

                        std::vector<ChildMove> moves;
                        for (Edge* ce : *child_succs) {
                            unsigned int to_sid = ce->getTo()->getId();
                            moves.push_back({
                                cid, sid, idx,
                                to_sid, cumulative_size[cid] + to_sid,
                                ce->getWeight()->getValue(),
                                ce->getTo()->getFinal()
                            });
                        }
                        all_moves.push_back(moves);
                    }
                }

                if (has_stuck_child) {
                    // Go to sink
                    parser->edges.insert({
                        { symbol->getName(), weight_t(0) },
                        { current.global_from, "@sink@" }
                    });
                    continue;
                }

                // Generate all combinations of child moves (Cartesian product)
                std::vector<std::vector<size_t>> combinations;
                combinations.push_back({});
                for (const auto& moves : all_moves) {
                    std::vector<std::vector<size_t>> new_combos;
                    for (const auto& combo : combinations) {
                        for (size_t i = 0; i < moves.size(); ++i) {
                            std::vector<size_t> new_combo = combo;
                            new_combo.push_back(i);
                            new_combos.push_back(new_combo);
                        }
                    }
                    combinations = std::move(new_combos);
                }

                // Process each combination
                for (const auto& combo : combinations) {
                    std::vector<unsigned int> result_status = new_status;
                    std::vector<bool> result_tracked = new_tracked;
                    bool any_failure = false;
                    bool result_epoch_nonempty = reset_epoch ? false : current.epoch_nonempty;
                    if (!is_silent) result_epoch_nonempty = true;  // non-silent transition = non-vacuous epoch

                    // Apply each child's move
                    for (size_t i = 0; i < combo.size() && !any_failure; ++i) {
                        const ChildMove& move = all_moves[i][combo[i]];
                        bool edge_high = (move.edge_w >= threshold);

                        // Get current seen_high status
                        bool was_high = (result_status[move.from_idx] & 2u) != 0;
                        bool is_tracked = result_tracked[move.from_idx];

                        // Clear source
                        result_status[move.from_idx] = 0;
                        result_tracked[move.from_idx] = false;

                        if (is_max) {
                            bool now_high = was_high || edge_high;

                            if (move.to_final) {
                                if (is_tracked) result_epoch_nonempty = true;
                                if (now_high) {
                                    // Success! Child terminates (already cleared)
                                } else {
                                    // Failure: reached final without high edge
                                    any_failure = true;
                                }
                            } else {
                                // Move to next state
                                result_status[move.to_idx] |= 1u;  // active
                                if (now_high) result_status[move.to_idx] |= 2u;  // seen_high
                                result_tracked[move.to_idx] = result_tracked[move.to_idx] || is_tracked;
                            }
                        } else {
                            // Min_f: reject on low edge
                            if (!edge_high) {
                                if (is_tracked) result_epoch_nonempty = true;
                                any_failure = true;
                            } else if (move.to_final) {
                                if (is_tracked) result_epoch_nonempty = true;
                                // Success! Child terminates (already cleared)
                            } else {
                                // Move to next state
                                result_status[move.to_idx] |= 1u;
                                result_tracked[move.to_idx] = result_tracked[move.to_idx] || is_tracked;
                            }
                        }
                    }

                    unsigned int result_phase = new_phase;

                    // Check for epoch boundary: no tracked children remaining active
                    bool any_tracked_active = false;
                    for (unsigned int i = 0; i < total_child_states; ++i) {
                        if (result_tracked[i] && (result_status[i] & 1u)) {
                            any_tracked_active = true;
                            break;
                        }
                    }

                    bool is_final = false;
                    if (!any_tracked_active && new_phase == 0) {
                        // Epoch boundary! All tracked children completed.
                        bool epoch_was_nonempty = result_epoch_nonempty;
                        // NOTE: do NOT reset epoch_nonempty here. It encodes whether
                        // this epoch boundary was productive, and becomes part of the
                        // state encoding. Reset happens on the NEXT step when
                        // transitioning out of phase 2 (final_pulse).

                        // Promote all active children to tracked
                        for (unsigned int i = 0; i < total_child_states; ++i) {
                            if (result_status[i] & 1u) {
                                result_tracked[i] = true;
                            }
                        }
                        result_phase = 2;  // final pulse
                        // Mark as final if epoch was non-vacuous (at least one tracked
                        // child terminated). Success/failure is encoded in the edge weight,
                        // not in finality. This ensures runs with finitely many non-silent
                        // parent transitions are not accepting (Büchi condition).
                        if (epoch_was_nonempty) {
                            is_final = this->getStates()->at(next_parent_id)->getFinal();
                        }
                    }

                    std::string next_state = encode_simple_state(next_parent_id, result_phase,
                                                                  result_status, result_tracked, total_child_states,
                                                                  result_epoch_nonempty);

                    // Emit weight 1 on success, weight 0 on failure
                    // For Inf: any 0 makes Inf=0 (reject)
                    // For LimInf: need eventually all 1s
                    parser->edges.insert({
                        { symbol->getName(), any_failure ? weight_t(0) : weight_t(1) },
                        { current.global_from, next_state }
                    });

                    if (is_final) {
                        parser->final_states.insert(next_state);
                    }

                    if (!parser->states.contains(next_state)) {
                        parser->states.insert(next_state);
                        worklist.push_back({next_state, next_parent_id, result_phase, result_status, result_tracked, result_epoch_nonempty});
                    }
                }
            }
        }
    }

    std::string newname = "unnested_simple(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    return unnested;
}

// =============================================================================
// Archived flatten_MinMax_Sup_split_witness alias
// =============================================================================

Automaton* NestedAutomaton::flatten_MinMax_Sup_split_witness(value_function_t finite_aggregator,
                                                             weight_t threshold) {
    return flatten_MinMax_Sup(finite_aggregator, threshold);
}

// =============================================================================
// Archived flatten_MinMax_Inf_threshold_obl implementation
// =============================================================================

Automaton* NestedAutomaton::flatten_MinMax_Inf_threshold_obl(value_function_t finite_aggregator,
                                                             weight_t threshold) {
    const bool finite_is_max = (finite_aggregator == Max_f);
    if (!finite_is_max && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf_threshold_obl requires Max_f or Min_f");
    }

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    MapArray<Symbol*>* new_alphabet = nullptr;
    MapArray<Weight*>* new_weights = nullptr;

    const size_t k = this->getChildrenSize();
    std::vector<ChildTables> child_tab(k);
    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* c = this->getChild(i);
        if (!c) continue;
        if (c->getStates()->size() < 2) continue;
        build_child_tables(c, child_tab[i]);
    }

    const MMThrLive live = build_mmthr_live(child_tab, finite_is_max, threshold);

    const size_t alph_size = this->getAlphabetSize();
    new_alphabet = new MapArray<Symbol*>(alph_size);
    for (size_t i = 0; i < alph_size; ++i) {
        Symbol* original = this->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    SetStd<weight_t> global_values;
    global_values.insert(weight_t(SILENT));
    global_values.insert(weight_t(0));
    global_values.insert(weight_t(1));
    MapStd<weight_t, Weight*> weight_register;
    new_weights = new MapArray<Weight*>(global_values.size());
    for (const weight_t& value : global_values) {
        Weight* w = new Weight(value);
        new_weights->insert(w->getId(), w);
        weight_register.insert(value, w);
    }

    const weight_t global_min = weight_t(0);
    const weight_t global_max = weight_t(1);

    MapStd<BuchiState_mmthr, State*> state_map;
    std::deque<BuchiState_mmthr> worklist;
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
    state_map[init] = init_state;
    worklist.push_back(init);

    MMThrOblBag P1_step, P2_step;
    MMThrOblBag P1_next, P2_next;

    while (!worklist.empty()) {
        BuchiState_mmthr current = std::move(worklist.front());
        worklist.pop_front();

        State* current_state = state_map[current];

        for (unsigned int symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            if (!step_mmthr_obl_bag(current.P1, symbol_id, P1_step, nullptr, child_tab, live,
                                    finite_is_max, threshold)) {
                continue;
            }

            bool tracked_discharged = false;
            if (!current.P2.empty()) {
                if (!step_mmthr_obl_bag(current.P2, symbol_id, P2_step, &tracked_discharged, child_tab, live,
                                        finite_is_max, threshold)) {
                    continue;
                }
            } else {
                P2_step.clear();
                tracked_discharged = false;
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
                if (reset_epoch) {
                    new_phase = 0u;
                }

                bool epoch_nonempty = reset_epoch ? false : current.epoch_nonempty;
                if (tracked_discharged) {
                    epoch_nonempty = true;
                }
                if (!is_silent) {
                    epoch_nonempty = true;
                    if (new_phase == 1u) {
                        new_phase = 0u;
                    }
                }

                const bool current_epoch_complete = current.P2.empty() || P2_step.empty();

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
                        if (st == MMThrSpawnStatus::NONEMPTY) {
                            mmthr_bag_add(P1_next, std::move(spawned));
                        }
                    } else {
                        P2_next = P2_step;
                        P1_next = P1_step;
                        if (st == MMThrSpawnStatus::NONEMPTY) {
                            mmthr_bag_add(P1_next, std::move(spawned));
                        }
                    }
                    mmthr_bag_finalize(P1_next);

                    BuchiState_mmthr nxt;
                    nxt.parent_state = q_prime;
                    nxt.P1 = P1_next;
                    nxt.P2 = P2_next;
                    nxt.parent_phase = (current_epoch_complete && new_phase == 0u) ? 2u : new_phase;
                    nxt.epoch_nonempty = epoch_nonempty;

                    if (!state_map.contains(nxt)) {
                        std::ostringstream s2;
                        s2 << "b_" << state_counter++;
                        State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                        state_map[nxt] = ns;
                        worklist.push_back(nxt);
                    }

                    Weight* w = weight_register.at(weight_t(static_cast<unsigned int>(guess)));
                    Edge* ne = new Edge(new_alphabet->at(symbol_id), w, current_state, state_map[nxt]);
                    current_state->addSuccessor(ne);
                    state_map[nxt]->addPredecessor(ne);
                }
            }
        }
    }

    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [gs, st] : state_map) {
        new_states->insert(st->getId(), st);
        if (gs.parent_phase == 2u && gs.epoch_nonempty && gs.parent_state->getFinal()) {
            st->setFinal(true);
        }
    }

std::string name = "BuchiMinMaxThr(" + this->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights,
                         global_min, global_max, init_state);
}

// =============================================================================
// Archived flatten_MinMax_Inf_v1/v2 implementations and helpers
// =============================================================================

// ============================================================================
// OLD VERSION: flatten_MinMax_Inf_v1
// This is the archived version of flatten_MinMax_Inf before bug fixes.
// It lacks:
//   - The at-risk check for children that can't succeed
//   - The can_succeed precomputation
//   - Fixed recursion pattern in selection case functions
// Kept for performance comparison purposes.
// ============================================================================



// Worklist item for iterative DFS (avoids stack overflow)
struct min_max_work_item_old {
    std::string global_from;
    unsigned int parent_state_id_from;
    unsigned int parent_tracking_from;
    std::vector<unsigned int> from_0_0;
    std::vector<unsigned int> from_0_1;
    std::vector<unsigned int> from_1_0;
    std::vector<unsigned int> from_1_1;
};

typedef struct global_exploration_data_min_max_old {
    // constraints
    NestedAutomaton* A;
    Parser* parser;
    weight_t threshold;
    unsigned int* cumulative_size; // prefix sums for flattening (child_id, local_state) -> global index
    unsigned int children_all;
    unsigned int inf_or_sup;       // 0 = inf-type outer, 1 = sup-type outer
    unsigned int finite_is_max;    // 0 = Min_f, 1 = Max_f

    // worklist for iterative DFS
    std::vector<min_max_work_item_old>* worklist;

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int parent_state_id_from;
    unsigned int parent_tracking_from;
    std::vector<unsigned int> from_0_0;
    std::vector<unsigned int> from_0_1;
    std::vector<unsigned int> from_1_0;
    std::vector<unsigned int> from_1_1;

    // initialized per-symbol
    Symbol* symbol = nullptr;
    std::vector<unsigned int> old_0_0, old_0_1, old_1_0, old_1_1;
    std::vector<unsigned int> new_0_0, new_0_1, new_1_0, new_1_1;

    // computed (output accumulators)
    unsigned int parent_state_id_to = 0;
    unsigned int parent_tracking_to = 0;
    weight_t global_edge_weight = 0;
} data_min_max_old_t;

static void explore_global_initialization_min_max_old(data_min_max_old_t* data);
static void explore_global_parent_transition_min_max_old(data_min_max_old_t* data);
static void explore_global_finalization_min_max_old(data_min_max_old_t* data);
static void explore_global_failure_min_max_old(data_min_max_old_t* data);

static void explore_global_selection_case_0_0_min_max_old(unsigned int child_id, unsigned int child_state_id, data_min_max_old_t* data);
static void explore_global_selection_case_0_1_min_max_old(unsigned int child_id, unsigned int child_state_id, data_min_max_old_t* data);
static void explore_global_selection_case_1_0_min_max_old(unsigned int child_id, unsigned int child_state_id, data_min_max_old_t* data);
static void explore_global_selection_case_1_1_min_max_old(unsigned int child_id, unsigned int child_state_id, data_min_max_old_t* data);

static void explore_global_selection_min_max_old(unsigned int child_id, unsigned int child_state_id, data_min_max_old_t* data) {
    explore_global_selection_case_0_0_min_max_old(child_id, child_state_id, data);
}

static void explore_global_failure_min_max_old(data_min_max_old_t* data) {
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

/*
  Status x_y:
    x = objective bit (0: want final outcome 0, 1: want final outcome 1)  -> birth guess -> edge weight
    y = current bit (monotone progress w.r.t. threshold, depends on finite aggregator):
        - Max_f: y starts 0 and can flip 0->1 when seeing edge>=threshold
        - Min_f: y starts 1 and can flip 1->0 when seeing edge<threshold
    - in *_0 cases: ok means "flip to *_1"
    - in *_1 cases: ok means "stay in *_1"
*/
static bool beyond_threshold_min_max_old(const weight_t& edge_value,
                                     bool /*guessed_weight_unused*/,
                                     bool current_is_1,
                                     const data_min_max_old_t* data) {
    const bool pass = !(edge_value < data->threshold); // edge_value >= threshold

    if (data->finite_is_max) {
        // Max_f: y' = y OR pass
        if (!current_is_1) return pass; // 0 -> 1 iff pass
        return true;                    // 1 stays 1
    } else {
        // Min_f: y' = y AND pass
        if (!current_is_1) return false; // 0 stays 0
        return pass;                     // 1 stays 1 iff pass, else flips to 0
    }
}

/*
  enforce "final-state handling in the same symbol" on the NEW arrays.
  - If a child is in a final state with status 0_0 or 1_1: it MUST terminate now -> drop it from new_*.
  - If a child is in a final state with status 0_1 or 1_0: it would be forced to terminate but cannot -> branch dies.
*/
static bool cleanup_new_on_finals_min_max_old(data_min_max_old_t* data) {
    const unsigned int active_bit = 1u;

    for (unsigned int cid = 0; cid < data->A->getChildrenSize(); ++cid) {
        ChildAutomaton* child = data->A->getChild(cid);
        auto* states = child->getStates();
        if (!states) continue;

        for (unsigned int sid = 0; sid < states->size(); ++sid) {
            if (!states->at(sid)->getFinal()) continue;

            const unsigned int idx = data->cumulative_size[cid] + sid;

            // forbidden: reached final but status cannot terminate
            if ((data->new_0_1[idx] & active_bit) || (data->new_1_0[idx] & active_bit)) {
                return false;
            }

            // allowed-terminate: drop immediately (also clears any tracked bit)
            data->new_0_0[idx] = 0u;
            data->new_1_1[idx] = 0u;
        }
    }
    return true;
}

static void explore_global_finalization_min_max_old(data_min_max_old_t* data) {
    // must handle finals in NEW arrays before packing/acceptance
    if (!cleanup_new_on_finals_min_max_old(data)) {
        explore_global_failure_min_max_old(data);
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

static void explore_global_selection_case_1_1_min_max_old(unsigned int child_id, unsigned int child_state_id, data_min_max_old_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_1_1[i] == 0u || data->old_1_1[i] == 2u) {
                explore_global_selection_case_1_1_min_max_old(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                explore_global_selection_case_1_1_min_max_old(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max_old(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_1_1 = data->new_1_1[ii];
                const unsigned int stored_1_0 = data->new_1_0[ii];

                if (beyond_threshold_min_max_old(edge->getWeight()->getValue(), true, true, data)) {
                    // stay 1_1
                    data->new_1_1[ii] = data->old_1_1[i]; // preserves 1 vs 3
                    explore_global_selection_case_1_1_min_max_old(child_id + 1, 0, data);
                    data->new_1_1[ii] = stored_1_1;
                } else {
                    // become 1_0
                    data->new_1_0[ii] = data->old_1_1[i];
                    explore_global_selection_case_1_1_min_max_old(child_id + 1, 0, data);
                    data->new_1_0[ii] = stored_1_0;
                }
            }
        } else {
            explore_global_selection_case_1_1_min_max_old(child_id + 1, 0, data);
        }
    } else {
        explore_global_finalization_min_max_old(data);
    }
}

static void explore_global_selection_case_1_0_min_max_old(unsigned int child_id, unsigned int child_state_id, data_min_max_old_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_1_0[i] == 0u || data->old_1_0[i] == 2u) {
                explore_global_selection_case_1_0_min_max_old(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                // 1_0 cannot terminate
                explore_global_failure_min_max_old(data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max_old(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_1_0 = data->new_1_0[ii];
                const unsigned int stored_1_1 = data->new_1_1[ii];

                // current bit is 0 here
                const bool ok = beyond_threshold_min_max_old(edge->getWeight()->getValue(), true, false, data);
                if (!ok) {
                    // stay 1_0
                    data->new_1_0[ii] = data->old_1_0[i];
                    explore_global_selection_case_1_0_min_max_old(child_id + 1, 0, data);
                    data->new_1_0[ii] = stored_1_0;
                } else {
                    // become 1_1
                    data->new_1_1[ii] = data->old_1_0[i];
                    explore_global_selection_case_1_0_min_max_old(child_id + 1, 0, data);
                    data->new_1_1[ii] = stored_1_1;
                }
            }
        } else {
            explore_global_selection_case_1_0_min_max_old(child_id + 1, 0, data);
        }
    } else {
        explore_global_selection_case_1_1_min_max_old(0, 0, data);
    }
}

static void explore_global_selection_case_0_1_min_max_old(unsigned int child_id, unsigned int child_state_id, data_min_max_old_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_0_1[i] == 0u || data->old_0_1[i] == 2u) {
                explore_global_selection_case_0_1_min_max_old(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                // 0_1 cannot terminate
                explore_global_failure_min_max_old(data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max_old(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_0_1 = data->new_0_1[ii];
                const unsigned int stored_0_0 = data->new_0_0[ii];

                // current bit is 1 here
                const bool ok = beyond_threshold_min_max_old(edge->getWeight()->getValue(), false, true, data);
                if (ok) {
                    // stay 0_1
                    data->new_0_1[ii] = data->old_0_1[i];
                    explore_global_selection_case_0_1_min_max_old(child_id + 1, 0, data);
                    data->new_0_1[ii] = stored_0_1;
                } else {
                    // become 0_0
                    data->new_0_0[ii] = data->old_0_1[i];
                    explore_global_selection_case_0_1_min_max_old(child_id + 1, 0, data);
                    data->new_0_0[ii] = stored_0_0;
                }
            }
        } else {
            explore_global_selection_case_0_1_min_max_old(child_id + 1, 0, data);
        }
    } else {
        explore_global_selection_case_1_0_min_max_old(0, 0, data);
    }
}

static void explore_global_selection_case_0_0_min_max_old(unsigned int child_id, unsigned int child_state_id, data_min_max_old_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_0_0[i] == 0u || data->old_0_0[i] == 2u) {
                explore_global_selection_case_0_0_min_max_old(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                // 0_0 can terminate
                explore_global_selection_case_0_0_min_max_old(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max_old(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_0_0 = data->new_0_0[ii];
                const unsigned int stored_0_1 = data->new_0_1[ii];

                // current bit is 0 here
                const bool ok = beyond_threshold_min_max_old(edge->getWeight()->getValue(), false, false, data);
                if (!ok) {
                    // stay 0_0
                    data->new_0_0[ii] = data->old_0_0[i];
                    explore_global_selection_case_0_0_min_max_old(child_id + 1, 0, data);
                    data->new_0_0[ii] = stored_0_0;
                } else {
                    // become 0_1
                    data->new_0_1[ii] = data->old_0_0[i];
                    explore_global_selection_case_0_0_min_max_old(child_id + 1, 0, data);
                    data->new_0_1[ii] = stored_0_1;
                }
            }
        } else {
            explore_global_selection_case_0_0_min_max_old(child_id + 1, 0, data);
        }
    } else {
        explore_global_selection_case_0_1_min_max_old(0, 0, data);
    }
}


static void explore_global_parent_transition_min_max_old(data_min_max_old_t* data) {
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

        // // Default: preserve parent tracking unless we take non-silent.
        // data->parent_tracking_to = data->parent_tracking_from;

        // Default: preserve tracking, but make the "final pulse" (2) last only one step.
        data->parent_tracking_to = (data->parent_tracking_from == 2u) ? 1u : data->parent_tracking_from;

        clear_new();

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // silent: identity element of the OUTER aggregator
            data->global_edge_weight = (data->inf_or_sup == 0u) ? weight_t(1) : weight_t(0);
            explore_global_selection_min_max_old(0, 0, data);
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
            explore_global_selection_min_max_old(0, 0, data);
            data->old_0_0[ii] = saved_0_0;

            // objective 1 -> 1_0 (edge weight 1)
            activate_tracked(data->old_1_0[ii]);
            data->global_edge_weight = weight_t(1);
            explore_global_selection_min_max_old(0, 0, data);
            data->old_1_0[ii] = saved_1_0;
        } else {
            // objective 0 -> 0_1 (edge weight 0)
            activate_tracked(data->old_0_1[ii]);
            data->global_edge_weight = weight_t(0);
            explore_global_selection_min_max_old(0, 0, data);
            data->old_0_1[ii] = saved_0_1;

            // objective 1 -> 1_1 (edge weight 1)
            activate_tracked(data->old_1_1[ii]);
            data->global_edge_weight = weight_t(1);
            explore_global_selection_min_max_old(0, 0, data);
            data->old_1_1[ii] = saved_1_1;
        }

        // Restore any untouched cells too
        data->old_0_0[ii] = saved_0_0;
        data->old_0_1[ii] = saved_0_1;
        data->old_1_0[ii] = saved_1_0;
        data->old_1_1[ii] = saved_1_1;
    }
}

static void explore_global_initialization_min_max_old(data_min_max_old_t* data) {
    // Ensure storage exists
    const unsigned int n = data->children_all;

    data->old_0_0.resize(n);
    data->old_0_1.resize(n);
    data->old_1_0.resize(n);
    data->old_1_1.resize(n);

    data->new_0_0.assign(n, 0u);
    data->new_0_1.assign(n, 0u);
    data->new_1_0.assign(n, 0u);
    data->new_1_1.assign(n, 0u);

    // Direct copy from vectors
    for (unsigned int i = 0; i < n; ++i) {
        data->old_0_0[i] = data->from_0_0[i];
        data->old_0_1[i] = data->from_0_1[i];
        data->old_1_0[i] = data->from_1_0[i];
        data->old_1_1[i] = data->from_1_1[i];
    }

    auto* alphabet = data->A->getStates()->at(data->parent_state_id_from)->getAlphabet();
    if (!alphabet) return;

    for (Symbol* symbol : *alphabet) {
        data->symbol = symbol;
        explore_global_parent_transition_min_max_old(data);
    }
}

// Returns a flattened automaton with 0/1 weights encoding threshold achievement
Automaton* NestedAutomaton::flatten_MinMax_Inf_v1(value_function_t finite_aggregator,
                                               weight_t threshold) {
    // inf_or_sup is always 0 (Inf-type) for this function.
    // Sup/LimSup cases use flatten_MinMax_Sup instead, which is faster.
    // (This function supports both via an infinite_aggregator parameter)
    // unsigned int inf_or_sup;
    // if (infinite_aggregator == Inf || infinite_aggregator == LimInf) {
    //     inf_or_sup = 0u;
    // } else if (infinite_aggregator == Sup || infinite_aggregator == LimSup) {
    //     inf_or_sup = 1u;
    // } else {
    //     QUAK_FAIL("bad infinite_aggregator for min_max construction");
    // }
    unsigned int inf_or_sup = 0u;

    // Decide finite_is_max
    unsigned int finite_is_max;
    if (finite_aggregator == Max_f) {
        finite_is_max = 1u;
    } else if (finite_aggregator == Min_f) {
        finite_is_max = 0u;
    } else {
        QUAK_FAIL("bad finite_aggregator for min_max construction");
    }

    // Build cumulative_size as vector
    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->getChildrenSize() + 1; ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int children_all = cumulative_size[this->getChildrenSize()];

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);

    parser->states.insert("@sink@");

    // Install sink self-loops on all symbols of the parent alphabet.
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

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

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    // Iterative DFS using worklist
    std::vector<min_max_work_item_old> worklist;
    worklist.push_back({
        global_initial,
        (unsigned int)this->initial->getId(),
        1u,
        std::vector<unsigned int>(children_all, 0u),
        std::vector<unsigned int>(children_all, 0u),
        std::vector<unsigned int>(children_all, 0u),
        std::vector<unsigned int>(children_all, 0u)
    });

    data_min_max_old_t data{};
    data.A = this;
    data.parser = parser;
    data.threshold = threshold;
    data.cumulative_size = cumulative_size.data();
    data.children_all = children_all;
    data.inf_or_sup = inf_or_sup;
    data.finite_is_max = finite_is_max;
    data.worklist = &worklist;

    while (!worklist.empty()) {
        min_max_work_item_old item = std::move(worklist.back());
        worklist.pop_back();

        data.global_from = std::move(item.global_from);
        data.parent_state_id_from = item.parent_state_id_from;
        data.parent_tracking_from = item.parent_tracking_from;
        data.from_0_0 = std::move(item.from_0_0);
        data.from_0_1 = std::move(item.from_0_1);
        data.from_1_0 = std::move(item.from_1_0);
        data.from_1_1 = std::move(item.from_1_1);

        explore_global_initialization_min_max_old(&data);
    }

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    return unnested;
}
// Worklist item for iterative DFS (avoids stack overflow)
struct min_max_work_item {
    std::string global_from;
    unsigned int parent_state_id_from;
    unsigned int parent_tracking_from;
    std::vector<unsigned int> from_0_0;
    std::vector<unsigned int> from_0_1;
    std::vector<unsigned int> from_1_0;
    std::vector<unsigned int> from_1_1;
};

typedef struct global_exploration_data_min_max {
    // constraints
    NestedAutomaton* A;
    Parser* parser;
    weight_t threshold;
    unsigned int* cumulative_size; // prefix sums for flattening (child_id, local_state) -> global index
    unsigned int children_all;
    unsigned int inf_or_sup;       // 0 = inf-type outer, 1 = sup-type outer
    unsigned int finite_is_max;    // 0 = Min_f, 1 = Max_f

    // worklist for iterative DFS
    std::vector<min_max_work_item>* worklist;

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int parent_state_id_from;
    unsigned int parent_tracking_from;
    std::vector<unsigned int> from_0_0;
    std::vector<unsigned int> from_0_1;
    std::vector<unsigned int> from_1_0;
    std::vector<unsigned int> from_1_1;

    // initialized per-symbol
    Symbol* symbol = nullptr;
    std::vector<unsigned int> old_0_0, old_0_1, old_1_0, old_1_1;
    std::vector<unsigned int> new_0_0, new_0_1, new_1_0, new_1_1;

    // computed (output accumulators)
    unsigned int parent_state_id_to = 0;
    unsigned int parent_tracking_to = 0;
    weight_t global_edge_weight = 0;

    // precomputed: can_succeed[i] = true if a child at state i can reach final with success
    // For Max_f: can 1_0 reach final as 1_1? (need edge >= threshold on path)
    // For Min_f: can 1_1 reach final as 1_1? (need safe path with all edges >= threshold)
    std::vector<bool> can_succeed;
} data_min_max_t;

static void explore_global_initialization_min_max(data_min_max_t* data);
static void explore_global_parent_transition_min_max(data_min_max_t* data);
static void explore_global_finalization_min_max(data_min_max_t* data);
static void explore_global_failure_min_max(data_min_max_t* data);

static void explore_global_selection_case_0_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);
static void explore_global_selection_case_0_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);
static void explore_global_selection_case_1_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);
static void explore_global_selection_case_1_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);

static void explore_global_selection_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
    explore_global_selection_case_0_0_min_max(child_id, child_state_id, data);
}

static void explore_global_failure_min_max(data_min_max_t* data) {
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

/*
  Status x_y:
    x = objective bit (0: want final outcome 0, 1: want final outcome 1)  -> birth guess -> edge weight
    y = current bit (monotone progress w.r.t. threshold, depends on finite aggregator):
        - Max_f: y starts 0 and can flip 0->1 when seeing edge>=threshold
        - Min_f: y starts 1 and can flip 1->0 when seeing edge<threshold
    - in *_0 cases: ok means "flip to *_1"
    - in *_1 cases: ok means "stay in *_1"
*/
static bool beyond_threshold_min_max(const weight_t& edge_value,
                                     bool /*guessed_weight_unused*/,
                                     bool current_is_1,
                                     const data_min_max_t* data) {
    const bool pass = !(edge_value < data->threshold); // edge_value >= threshold

    if (data->finite_is_max) {
        // Max_f: y' = y OR pass
        if (!current_is_1) return pass; // 0 -> 1 iff pass
        return true;                    // 1 stays 1
    } else {
        // Min_f: y' = y AND pass
        if (!current_is_1) return false; // 0 stays 0
        return pass;                     // 1 stays 1 iff pass, else flips to 0
    }
}

/*
  enforce "final-state handling in the same symbol" on the NEW arrays.
  - If a child is in a final state with status 0_0 or 1_1: it MUST terminate now -> drop it from new_*.
  - If a child is in a final state with status 0_1 or 1_0: it would be forced to terminate but cannot -> branch dies.
*/
static bool cleanup_new_on_finals_min_max(data_min_max_t* data) {
    const unsigned int active_bit = 1u;

    for (unsigned int cid = 0; cid < data->A->getChildrenSize(); ++cid) {
        ChildAutomaton* child = data->A->getChild(cid);
        auto* states = child->getStates();
        if (!states) continue;

        for (unsigned int sid = 0; sid < states->size(); ++sid) {
            if (!states->at(sid)->getFinal()) continue;

            const unsigned int idx = data->cumulative_size[cid] + sid;

            // forbidden: reached final but status cannot terminate
            if ((data->new_0_1[idx] & active_bit) || (data->new_1_0[idx] & active_bit)) {
                return false;
            }

            // allowed-terminate: drop immediately (also clears any tracked bit)
            data->new_0_0[idx] = 0u;
            data->new_1_1[idx] = 0u;
        }
    }
    return true;
}

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

    // Epoch boundary: check tracked bit (bit 1) in FROM arrays (before step).
    // This matches Sup's approach: epoch ends when all tokens from current epoch are discharged.
    bool any_tracked_from = false;
    for (unsigned int i = 0; i < data->children_all && !any_tracked_from; ++i) {
        if ((data->from_0_0[i] | data->from_0_1[i] | data->from_1_0[i] | data->from_1_1[i]) & 2u) {
            any_tracked_from = true;
        }
    }

    // if no tracked children (before step) and parent saw a non-silent since last completion:
    // emit a one-step "final pulse" (2). This forces infinitely many non-silent segments
    // to visit final states infinitely often.
    if (!any_tracked_from && data->parent_tracking_from == 0u) {
        // Epoch boundary: promote all active (bit 0) to tracked (bit 1) in TO arrays
        for (unsigned int i = 0; i < data->children_all; ++i) {
            if (to_0_0[i] & 1u) to_0_0[i] |= 2u;
            if (to_0_1[i] & 1u) to_0_1[i] |= 2u;
            if (to_1_0[i] & 1u) to_1_0[i] |= 2u;
            if (to_1_1[i] & 1u) to_1_1[i] |= 2u;
        }
        data->parent_tracking_to = 2u; // final pulse

        // Check for "doomed" tracked children. A child is doomed if it has made an
        // incorrect guess that cannot be corrected:
        // - Max_f: 0_1 is doomed (achieved threshold but guessed 0 - can't undo)
        //          1_0 is NOT doomed (might still achieve threshold)
        // - Min_f: 1_0 is doomed (failed threshold but guessed 1 - can't undo)
        //          0_1 is NOT doomed (might still fail threshold)
        // If any tracked child is doomed, this epoch cannot succeed, so don't mark final.
        bool any_doomed = false;
        for (unsigned int i = 0; i < data->children_all && !any_doomed; ++i) {
            if (data->finite_is_max) {
                // Max_f: 0_1 is doomed
                if (to_0_1[i] & 2u) any_doomed = true;
            } else {
                // Min_f: 1_0 is doomed
                if (to_1_0[i] & 2u) any_doomed = true;
            }
        }

        // Also check for "at-risk" tracked children that cannot reach final with success.
        // - For Max_f: 1_0 at non-final is at-risk if there's no path with edge >= threshold to final
        // - For Min_f: 1_1 at non-final is at-risk if there's no safe path (all edges >= threshold) to final
        // We use the precomputed can_succeed array to check this efficiently.
        // A child that can't reach final with success will inevitably fail.
        bool any_at_risk = false;
        for (unsigned int cid = 0; cid < data->A->getChildrenSize() && !any_at_risk; ++cid) {
            ChildAutomaton* child = data->A->getChild(cid);
            auto* states = child->getStates();
            if (!states) continue;

            for (unsigned int sid = 0; sid < states->size() && !any_at_risk; ++sid) {
                if (states->at(sid)->getFinal()) continue; // final state is OK

                const unsigned int idx = data->cumulative_size[cid] + sid;

                if (data->finite_is_max) {
                    // Max_f: 1_0 at non-final without path to flip = at risk
                    if ((to_1_0[idx] & 2u) && !data->can_succeed[idx]) {
                        any_at_risk = true;
                    }
                } else {
                    // Min_f: 1_1 at non-final without safe path to final = at risk
                    if ((to_1_1[idx] & 2u) && !data->can_succeed[idx]) {
                        any_at_risk = true;
                    }
                }
            }
        }

        if (!any_doomed && !any_at_risk) {
            global_final = data->A->getStates()->at(data->parent_state_id_to)->getFinal();
        }
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

static void explore_global_selection_case_1_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_1_1[i] == 0u || data->old_1_1[i] == 2u) {
                explore_global_selection_case_1_1_min_max(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                explore_global_selection_case_1_1_min_max(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_1_1 = data->new_1_1[ii];
                const unsigned int stored_1_0 = data->new_1_0[ii];

                if (beyond_threshold_min_max(edge->getWeight()->getValue(), true, true, data)) {
                    // stay 1_1
                    data->new_1_1[ii] = data->old_1_1[i]; // preserves 1 vs 3
                    explore_global_selection_case_1_1_min_max(child_id, child_state_id + 1, data);
                    data->new_1_1[ii] = stored_1_1;
                } else {
                    // become 1_0
                    data->new_1_0[ii] = data->old_1_1[i];
                    explore_global_selection_case_1_1_min_max(child_id, child_state_id + 1, data);
                    data->new_1_0[ii] = stored_1_0;
                }
            }
        } else {
            explore_global_selection_case_1_1_min_max(child_id + 1, 0, data);
        }
    } else {
        explore_global_finalization_min_max(data);
    }
}

static void explore_global_selection_case_1_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_1_0[i] == 0u || data->old_1_0[i] == 2u) {
                explore_global_selection_case_1_0_min_max(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                // 1_0 cannot terminate
                explore_global_failure_min_max(data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_1_0 = data->new_1_0[ii];
                const unsigned int stored_1_1 = data->new_1_1[ii];

                // current bit is 0 here
                const bool ok = beyond_threshold_min_max(edge->getWeight()->getValue(), true, false, data);
                if (!ok) {
                    // stay 1_0
                    data->new_1_0[ii] = data->old_1_0[i];
                    explore_global_selection_case_1_0_min_max(child_id, child_state_id + 1, data);
                    data->new_1_0[ii] = stored_1_0;
                } else {
                    // become 1_1
                    data->new_1_1[ii] = data->old_1_0[i];
                    explore_global_selection_case_1_0_min_max(child_id, child_state_id + 1, data);
                    data->new_1_1[ii] = stored_1_1;
                }
            }
        } else {
            explore_global_selection_case_1_0_min_max(child_id + 1, 0, data);
        }
    } else {
        explore_global_selection_case_1_1_min_max(0, 0, data);
    }
}

static void explore_global_selection_case_0_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_0_1[i] == 0u || data->old_0_1[i] == 2u) {
                explore_global_selection_case_0_1_min_max(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                // 0_1 cannot terminate
                explore_global_failure_min_max(data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_0_1 = data->new_0_1[ii];
                const unsigned int stored_0_0 = data->new_0_0[ii];

                // current bit is 1 here
                const bool ok = beyond_threshold_min_max(edge->getWeight()->getValue(), false, true, data);
                if (ok) {
                    // stay 0_1
                    data->new_0_1[ii] = data->old_0_1[i];
                    explore_global_selection_case_0_1_min_max(child_id, child_state_id + 1, data);
                    data->new_0_1[ii] = stored_0_1;
                } else {
                    // become 0_0
                    data->new_0_0[ii] = data->old_0_1[i];
                    explore_global_selection_case_0_1_min_max(child_id, child_state_id + 1, data);
                    data->new_0_0[ii] = stored_0_0;
                }
            }
        } else {
            explore_global_selection_case_0_1_min_max(child_id + 1, 0, data);
        }
    } else {
        explore_global_selection_case_1_0_min_max(0, 0, data);
    }
}

static void explore_global_selection_case_0_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_0_0[i] == 0u || data->old_0_0[i] == 2u) {
                explore_global_selection_case_0_0_min_max(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                // 0_0 can terminate
                explore_global_selection_case_0_0_min_max(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_0_0 = data->new_0_0[ii];
                const unsigned int stored_0_1 = data->new_0_1[ii];

                // current bit is 0 here
                const bool ok = beyond_threshold_min_max(edge->getWeight()->getValue(), false, false, data);
                if (!ok) {
                    // stay 0_0
                    data->new_0_0[ii] = data->old_0_0[i];
                    explore_global_selection_case_0_0_min_max(child_id, child_state_id + 1, data);
                    data->new_0_0[ii] = stored_0_0;
                } else {
                    // become 0_1
                    data->new_0_1[ii] = data->old_0_0[i];
                    explore_global_selection_case_0_0_min_max(child_id, child_state_id + 1, data);
                    data->new_0_1[ii] = stored_0_1;
                }
            }
        } else {
            explore_global_selection_case_0_0_min_max(child_id + 1, 0, data);
        }
    } else {
        explore_global_selection_case_0_1_min_max(0, 0, data);
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

    // New spawns are active but NOT tracked (belong to next epoch, not current)
    auto activate_untracked = [](unsigned int& cell) {
        cell = 1u; // active only, not tracked
    };

    for (Edge* edge : *succs) {
        data->parent_state_id_to = (unsigned int)edge->getTo()->getId();

        const unsigned int child_id =
            (unsigned int)edge->getWeight()->getValue().to_uint();

        // // Default: preserve parent tracking unless we take non-silent.
        // data->parent_tracking_to = data->parent_tracking_from;

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
        // New spawns are active but NOT tracked (belong to next epoch)
        if (data->finite_is_max) {
            // objective 0 -> 0_0 (edge weight 0)
            activate_untracked(data->old_0_0[ii]);
            data->global_edge_weight = weight_t(0);
            explore_global_selection_min_max(0, 0, data);
            data->old_0_0[ii] = saved_0_0;

            // objective 1 -> 1_0 (edge weight 1)
            activate_untracked(data->old_1_0[ii]);
            data->global_edge_weight = weight_t(1);
            explore_global_selection_min_max(0, 0, data);
            data->old_1_0[ii] = saved_1_0;
        } else {
            // objective 0 -> 0_1 (edge weight 0)
            activate_untracked(data->old_0_1[ii]);
            data->global_edge_weight = weight_t(0);
            explore_global_selection_min_max(0, 0, data);
            data->old_0_1[ii] = saved_0_1;

            // objective 1 -> 1_1 (edge weight 1)
            activate_untracked(data->old_1_1[ii]);
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

static void explore_global_initialization_min_max(data_min_max_t* data) {
    // Ensure storage exists
    const unsigned int n = data->children_all;

    data->old_0_0.resize(n);
    data->old_0_1.resize(n);
    data->old_1_0.resize(n);
    data->old_1_1.resize(n);

    data->new_0_0.assign(n, 0u);
    data->new_0_1.assign(n, 0u);
    data->new_1_0.assign(n, 0u);
    data->new_1_1.assign(n, 0u);

    // Direct copy from vectors
    for (unsigned int i = 0; i < n; ++i) {
        data->old_0_0[i] = data->from_0_0[i];
        data->old_0_1[i] = data->from_0_1[i];
        data->old_1_0[i] = data->from_1_0[i];
        data->old_1_1[i] = data->from_1_1[i];
    }

    auto* alphabet = data->A->getStates()->at(data->parent_state_id_from)->getAlphabet();
    if (!alphabet) return;

    for (Symbol* symbol : *alphabet) {
        data->symbol = symbol;
        explore_global_parent_transition_min_max(data);
    }
}

// Helper: compute which child states can succeed for the at-risk check
// For Max_f: can 1_0 at state s eventually reach a final as 1_1?
//   (need at least one edge >= threshold on some path to final)
// For Min_f: can 1_1 at state s eventually reach a final as 1_1?
//   (need ALL edges >= threshold on some path to final)
static std::vector<bool> compute_can_succeed_from(ChildAutomaton* child, weight_t threshold, bool finite_is_max) {
    auto* states = child->getStates();
    if (!states) return {};

    const unsigned int n = states->size();
    std::vector<bool> can_succeed(n, false);

    if (finite_is_max) {
        // Max_f: can 1_0 reach final as 1_1?
        // Need path with at least one edge >= threshold to a final.
        // Backward computation: can_succeed[s] = true if
        //   - s has edge >= threshold to t where t can reach final, OR
        //   - s has edge to t where can_succeed[t]

        // First compute can_reach_final
        std::vector<bool> can_reach_final(n, false);
        std::vector<bool> visited(n, false);
        std::queue<unsigned int> q;
        for (unsigned int i = 0; i < n; ++i) {
            if (states->at(i)->getFinal()) {
                can_reach_final[i] = true;
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
                            can_reach_final[pred] = true;
                            visited[pred] = true;
                            q.push(pred);
                            break;
                        }
                    }
                    if (visited[pred]) break;
                }
            }
        }

        // Now compute can_succeed for Max_f
        std::fill(visited.begin(), visited.end(), false);
        for (unsigned int i = 0; i < n; ++i) {
            State* state = states->at(i);
            for (Symbol* sym : *state->getAlphabet()) {
                auto* succs = state->getSuccessors(sym->getId());
                if (!succs) continue;
                for (Edge* e : *succs) {
                    unsigned int succ = (unsigned int)e->getTo()->getId();
                    if (e->getWeight()->getValue() >= threshold && can_reach_final[succ]) {
                        can_succeed[i] = true;
                        if (!visited[i]) {
                            visited[i] = true;
                            q.push(i);
                        }
                    }
                }
            }
        }
        // Propagate backward
        while (!q.empty()) {
            unsigned int cur = q.front();
            q.pop();
            for (unsigned int pred = 0; pred < n; ++pred) {
                if (can_succeed[pred]) continue;
                State* pred_state = states->at(pred);
                for (Symbol* sym : *pred_state->getAlphabet()) {
                    auto* succs = pred_state->getSuccessors(sym->getId());
                    if (!succs) continue;
                    for (Edge* e : *succs) {
                        if ((unsigned int)e->getTo()->getId() == cur) {
                            can_succeed[pred] = true;
                            if (!visited[pred]) {
                                visited[pred] = true;
                                q.push(pred);
                            }
                        }
                    }
                }
            }
        }
    } else {
        // Min_f: can 1_1 reach final as 1_1?
        // Need path where ALL edges >= threshold to a final.
        // Backward computation: can_succeed[s] = true if
        //   - s is final, OR
        //   - s has edge >= threshold to t where can_succeed[t]

        std::vector<bool> visited(n, false);
        std::queue<unsigned int> q;
        for (unsigned int i = 0; i < n; ++i) {
            if (states->at(i)->getFinal()) {
                can_succeed[i] = true;
                visited[i] = true;
                q.push(i);
            }
        }
        // Backward fixpoint: only propagate through edges >= threshold
        while (!q.empty()) {
            unsigned int cur = q.front();
            q.pop();
            for (unsigned int pred = 0; pred < n; ++pred) {
                if (can_succeed[pred]) continue;
                State* pred_state = states->at(pred);
                for (Symbol* sym : *pred_state->getAlphabet()) {
                    auto* succs = pred_state->getSuccessors(sym->getId());
                    if (!succs) continue;
                    for (Edge* e : *succs) {
                        // Only propagate through safe edges (>= threshold)
                        if ((unsigned int)e->getTo()->getId() == cur &&
                            e->getWeight()->getValue() >= threshold) {
                            can_succeed[pred] = true;
                            if (!visited[pred]) {
                                visited[pred] = true;
                                q.push(pred);
                            }
                        }
                    }
                }
            }
        }
    }

    return can_succeed;
}

// Returns a flattened automaton with 0/1 weights encoding threshold achievement
Automaton* NestedAutomaton::flatten_MinMax_Inf_v2(value_function_t finite_aggregator,
                                               weight_t threshold) {
    // inf_or_sup is always 0 (Inf-type) for this function.
    // Sup/LimSup cases use flatten_MinMax_Sup instead, which is faster.
    // (This function supports both via an infinite_aggregator parameter)
    // unsigned int inf_or_sup;
    // if (infinite_aggregator == Inf || infinite_aggregator == LimInf) {
    //     inf_or_sup = 0u;
    // } else if (infinite_aggregator == Sup || infinite_aggregator == LimSup) {
    //     inf_or_sup = 1u;
    // } else {
    //     QUAK_FAIL("bad infinite_aggregator for min_max construction");
    // }
    unsigned int inf_or_sup = 0u;

    // Decide finite_is_max
    unsigned int finite_is_max;
    if (finite_aggregator == Max_f) {
        finite_is_max = 1u;
    } else if (finite_aggregator == Min_f) {
        finite_is_max = 0u;
    } else {
        QUAK_FAIL("bad finite_aggregator for min_max construction");
    }

    // Build cumulative_size as vector
    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->getChildrenSize() + 1; ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int children_all = cumulative_size[this->getChildrenSize()];

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);

    parser->states.insert("@sink@");

    // Install sink self-loops on all symbols of the parent alphabet.
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

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

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    // Iterative DFS using worklist
    std::vector<min_max_work_item> worklist;
    worklist.push_back({
        global_initial,
        (unsigned int)this->initial->getId(),
        1u,
        std::vector<unsigned int>(children_all, 0u),
        std::vector<unsigned int>(children_all, 0u),
        std::vector<unsigned int>(children_all, 0u),
        std::vector<unsigned int>(children_all, 0u)
    });

    data_min_max_t data{};
    data.A = this;
    data.parser = parser;
    data.threshold = threshold;
    data.cumulative_size = cumulative_size.data();
    data.children_all = children_all;
    data.inf_or_sup = inf_or_sup;
    data.finite_is_max = finite_is_max;
    data.worklist = &worklist;

    // Precompute which child states can reach finals with status flip
    data.can_succeed.resize(children_all, false);
    for (unsigned int cid = 0; cid < this->getChildrenSize(); ++cid) {
        ChildAutomaton* child = this->getChild(cid);
        std::vector<bool> child_can_succeed = compute_can_succeed_from(child, threshold, finite_is_max);
        for (unsigned int sid = 0; sid < child_can_succeed.size(); ++sid) {
            data.can_succeed[cumulative_size[cid] + sid] = child_can_succeed[sid];
        }
    }

    while (!worklist.empty()) {
        min_max_work_item item = std::move(worklist.back());
        worklist.pop_back();

        data.global_from = std::move(item.global_from);
        data.parent_state_id_from = item.parent_state_id_from;
        data.parent_tracking_from = item.parent_tracking_from;
        data.from_0_0 = std::move(item.from_0_0);
        data.from_0_1 = std::move(item.from_0_1);
        data.from_1_0 = std::move(item.from_1_0);
        data.from_1_1 = std::move(item.from_1_1);

        explore_global_initialization_min_max(&data);
    }

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    return unnested;
}

// =============================================================================
// Archived simplified flatten_MinMax_Inf implementation and helpers
// =============================================================================

// ============================================================================
// SIMPLIFIED VERSION: flatten_MinMax_Inf
//
// Key insight: For Inf >= threshold, ALL children must succeed. So:
// - We don't need to "guess" the outcome (always bet on success)
// - For Max_f: track 1 bit per child (has it seen edge >= threshold?)
// - For Min_f: no per-child state needed (reject immediately on bad edge)
//
// This avoids the 2x branching at spawn and reduces status from 4 to 1-2.
// ============================================================================

// Work item for simplified construction
struct simple_work_item {
    std::string global_from;
    unsigned int parent_state_id;
    unsigned int parent_phase;  // 0=active, 1=waiting, 2=final_pulse
    std::vector<unsigned int> child_status;  // per child state: bit 0 = active, bit 1 = seen_high (Max_f only)
    std::vector<bool> tracked;  // is this child tracked for current epoch?
    bool epoch_nonempty;  // true if any tracked child completed in current epoch
};

// Helper to encode state for simple version
static std::string encode_simple_state(unsigned int parent_id, unsigned int phase,
                                        const std::vector<unsigned int>& status,
                                        const std::vector<bool>& tracked,
                                        unsigned int total,
                                        bool epoch_nonempty) {
    std::string s;
    s.reserve(64 + total * 4);
    s += std::to_string(parent_id) + "/" + std::to_string(phase) + "/";
    for (unsigned int i = 0; i < total; ++i) {
        s += std::to_string(status[i]);
        if (i < total - 1) s += ",";
    }
    s += "/";
    for (unsigned int i = 0; i < total; ++i) {
        s += (tracked[i] ? "1" : "0");
    }
    s += (epoch_nonempty ? "E" : "e");
    return s;
}

Automaton* NestedAutomaton::flatten_MinMax_Inf(value_function_t finite_aggregator,
                                                      weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf requires Max_f or Min_f");
    }
    return this->flatten_MinMax_Inf_cached(finite_aggregator, threshold);

    const bool is_max = (finite_aggregator == Max_f);

    // Build cumulative_size for child state indexing
    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i <= this->getChildrenSize(); ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int total_child_states = cumulative_size[this->getChildrenSize()];

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);

    // Sink state with self-loops
    parser->states.insert("@sink@");
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

    // Initial state: parent at initial, waiting phase, no active children
    std::vector<unsigned int> init_status(total_child_states, 0);
    std::vector<bool> init_tracked(total_child_states, false);
    bool init_epoch_nonempty = false;
    std::string initial_state = encode_simple_state(this->initial->getId(), 1, init_status, init_tracked, total_child_states, init_epoch_nonempty);

    parser->states.insert(initial_state);
    parser->initial = initial_state;

    // Worklist-based exploration
    std::vector<simple_work_item> worklist;
    worklist.push_back({initial_state, (unsigned int)this->initial->getId(), 1, init_status, init_tracked, init_epoch_nonempty});

    while (!worklist.empty()) {
        simple_work_item current = std::move(worklist.back());
        worklist.pop_back();

        State* parent_state = this->getStates()->at(current.parent_state_id);

        for (Symbol* symbol : *parent_state->getAlphabet()) {
            auto* parent_succs = parent_state->getSuccessors(symbol->getId());
            if (!parent_succs) continue;

            for (Edge* parent_edge : *parent_succs) {
                unsigned int next_parent_id = parent_edge->getTo()->getId();
                unsigned int child_id = (unsigned int)parent_edge->getWeight()->getValue().to_float();

                bool is_silent = (this->getChild(child_id)->getStates()->size() == 1);

                // Start with copies of current state
                std::vector<unsigned int> new_status = current.child_status;
                std::vector<bool> new_tracked = current.tracked;
                unsigned int new_phase = current.parent_phase;

                // Handle phase transition from final_pulse
                bool reset_epoch = false;
                if (current.parent_phase == 2) {
                    new_phase = 0;  // back to active
                    reset_epoch = true;  // new epoch starts; reset productivity tracker
                }

                // Spawn new child (if non-silent)
                if (!is_silent) {
                    unsigned int spawn_state = this->getChild(child_id)->initial->getId();
                    unsigned int spawn_idx = cumulative_size[child_id] + spawn_state;

                    // For Max_f: status encodes (active, seen_high) as bits
                    //   0 = inactive, 1 = active+low, 3 = active+high
                    // For Min_f: status is just 0=inactive, 1=active
                    if (is_max) {
                        new_status[spawn_idx] |= 1u;  // set active bit, seen_high stays 0
                    } else {
                        new_status[spawn_idx] = 1;  // active
                    }
                    // New children are NOT tracked (belong to next epoch)

                    if (new_phase == 1) {  // was waiting
                        new_phase = 0;  // now active
                    }
                }

                // Process all active children - need to handle non-determinism properly
                // We'll explore all combinations of child transitions

                // Collect active children and their possible transitions
                struct ChildMove {
                    unsigned int cid;
                    unsigned int from_sid;
                    unsigned int from_idx;
                    unsigned int to_sid;
                    unsigned int to_idx;
                    weight_t edge_w;
                    bool to_final;
                };

                std::vector<std::vector<ChildMove>> all_moves;
                bool has_stuck_child = false;

                for (unsigned int cid = 0; cid < this->getChildrenSize() && !has_stuck_child; ++cid) {
                    ChildAutomaton* child = this->getChild(cid);
                    auto* child_states = child->getStates();
                    if (!child_states) continue;

                    for (unsigned int sid = 0; sid < child_states->size() && !has_stuck_child; ++sid) {
                        unsigned int idx = cumulative_size[cid] + sid;
                        if ((new_status[idx] & 1u) == 0) continue;  // not active

                        State* child_state = child_states->at(sid);

                        // Skip if child is at final state (should have terminated)
                        if (child_state->getFinal()) continue;

                        auto* child_succs = child_state->getSuccessors(symbol->getId());

                        if (!child_succs || child_succs->empty()) {
                            has_stuck_child = true;
                            break;
                        }

                        std::vector<ChildMove> moves;
                        for (Edge* ce : *child_succs) {
                            unsigned int to_sid = ce->getTo()->getId();
                            moves.push_back({
                                cid, sid, idx,
                                to_sid, cumulative_size[cid] + to_sid,
                                ce->getWeight()->getValue(),
                                ce->getTo()->getFinal()
                            });
                        }
                        all_moves.push_back(moves);
                    }
                }

                if (has_stuck_child) {
                    // Go to sink
                    parser->edges.insert({
                        { symbol->getName(), weight_t(0) },
                        { current.global_from, "@sink@" }
                    });
                    continue;
                }

                // Generate all combinations of child moves (Cartesian product)
                std::vector<std::vector<size_t>> combinations;
                combinations.push_back({});
                for (const auto& moves : all_moves) {
                    std::vector<std::vector<size_t>> new_combos;
                    for (const auto& combo : combinations) {
                        for (size_t i = 0; i < moves.size(); ++i) {
                            std::vector<size_t> new_combo = combo;
                            new_combo.push_back(i);
                            new_combos.push_back(new_combo);
                        }
                    }
                    combinations = std::move(new_combos);
                }

                // Process each combination
                for (const auto& combo : combinations) {
                    std::vector<unsigned int> result_status = new_status;
                    std::vector<bool> result_tracked = new_tracked;
                    bool any_failure = false;
                    bool result_epoch_nonempty = reset_epoch ? false : current.epoch_nonempty;
                    if (!is_silent) result_epoch_nonempty = true;  // non-silent transition = non-vacuous epoch

                    // Apply each child's move
                    for (size_t i = 0; i < combo.size() && !any_failure; ++i) {
                        const ChildMove& move = all_moves[i][combo[i]];
                        bool edge_high = (move.edge_w >= threshold);

                        // Get current seen_high status
                        bool was_high = (result_status[move.from_idx] & 2u) != 0;
                        bool is_tracked = result_tracked[move.from_idx];

                        // Clear source
                        result_status[move.from_idx] = 0;
                        result_tracked[move.from_idx] = false;

                        if (is_max) {
                            bool now_high = was_high || edge_high;

                            if (move.to_final) {
                                if (is_tracked) result_epoch_nonempty = true;
                                if (now_high) {
                                    // Success! Child terminates (already cleared)
                                } else {
                                    // Failure: reached final without high edge
                                    any_failure = true;
                                }
                            } else {
                                // Move to next state
                                result_status[move.to_idx] |= 1u;  // active
                                if (now_high) result_status[move.to_idx] |= 2u;  // seen_high
                                result_tracked[move.to_idx] = result_tracked[move.to_idx] || is_tracked;
                            }
                        } else {
                            // Min_f: reject on low edge
                            if (!edge_high) {
                                if (is_tracked) result_epoch_nonempty = true;
                                any_failure = true;
                            } else if (move.to_final) {
                                if (is_tracked) result_epoch_nonempty = true;
                                // Success! Child terminates (already cleared)
                            } else {
                                // Move to next state
                                result_status[move.to_idx] |= 1u;
                                result_tracked[move.to_idx] = result_tracked[move.to_idx] || is_tracked;
                            }
                        }
                    }

                    unsigned int result_phase = new_phase;

                    // Check for epoch boundary: no tracked children remaining active
                    bool any_tracked_active = false;
                    for (unsigned int i = 0; i < total_child_states; ++i) {
                        if (result_tracked[i] && (result_status[i] & 1u)) {
                            any_tracked_active = true;
                            break;
                        }
                    }

                    bool is_final = false;
                    if (!any_tracked_active && new_phase == 0) {
                        // Epoch boundary! All tracked children completed.
                        bool epoch_was_nonempty = result_epoch_nonempty;
                        // NOTE: do NOT reset epoch_nonempty here. It encodes whether
                        // this epoch boundary was productive, and becomes part of the
                        // state encoding. Reset happens on the NEXT step when
                        // transitioning out of phase 2 (final_pulse).

                        // Promote all active children to tracked
                        for (unsigned int i = 0; i < total_child_states; ++i) {
                            if (result_status[i] & 1u) {
                                result_tracked[i] = true;
                            }
                        }
                        result_phase = 2;  // final pulse
                        // Mark as final if epoch was non-vacuous (at least one tracked
                        // child terminated). Success/failure is encoded in the edge weight,
                        // not in finality. This ensures runs with finitely many non-silent
                        // parent transitions are not accepting (Büchi condition).
                        if (epoch_was_nonempty) {
                            is_final = this->getStates()->at(next_parent_id)->getFinal();
                        }
                    }

                    std::string next_state = encode_simple_state(next_parent_id, result_phase,
                                                                  result_status, result_tracked, total_child_states,
                                                                  result_epoch_nonempty);

                    // Emit weight 1 on success, weight 0 on failure
                    // For Inf: any 0 makes Inf=0 (reject)
                    // For LimInf: need eventually all 1s
                    parser->edges.insert({
                        { symbol->getName(), any_failure ? weight_t(0) : weight_t(1) },
                        { current.global_from, next_state }
                    });

                    if (is_final) {
                        parser->final_states.insert(next_state);
                    }

                    if (!parser->states.contains(next_state)) {
                        parser->states.insert(next_state);
                        worklist.push_back({next_state, next_parent_id, result_phase, result_status, result_tracked, result_epoch_nonempty});
                    }
                }
            }
        }
    }

    std::string newname = "unnested_simple(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    return unnested;
}

// =============================================================================
// Archived flatten_MinMax_Inf_masked implementation and helpers
// =============================================================================

// ============================================================================
// MASKED FIX: flatten_MinMax_Inf with coexistence classes per child state
//
// This keeps the current phase machine and parser-based construction, but fixes
// the Max_f overlap bug by tracking distinct untracked/tracked and low/high
// classes per flattened child state.
// ============================================================================

struct masked_simple_work_item {
    std::string global_from;
    unsigned int parent_state_id;
    unsigned int parent_phase;  // 0 = active, 1 = waiting, 2 = final_pulse
    std::vector<unsigned int> child_mask;
    bool epoch_nonempty;
};

enum : unsigned int {
    MASKED_MIN_U  = 1u << 0,
    MASKED_MIN_T  = 1u << 1,

    MASKED_MAX_UL = 1u << 0,
    MASKED_MAX_UH = 1u << 1,
    MASKED_MAX_TL = 1u << 2,
    MASKED_MAX_TH = 1u << 3,
};

static inline bool masked_mask_has_active(unsigned int mask, bool is_max) {
    if (is_max) {
        return (mask & (MASKED_MAX_UL | MASKED_MAX_UH | MASKED_MAX_TL | MASKED_MAX_TH)) != 0u;
    }
    return (mask & (MASKED_MIN_U | MASKED_MIN_T)) != 0u;
}

static inline bool masked_mask_has_tracked(unsigned int mask, bool is_max) {
    if (is_max) {
        return (mask & (MASKED_MAX_TL | MASKED_MAX_TH)) != 0u;
    }
    return (mask & MASKED_MIN_T) != 0u;
}

static inline unsigned int masked_spawn_mask(bool is_max) {
    return is_max ? MASKED_MAX_UL : MASKED_MIN_U;
}

static inline unsigned int masked_promote_mask(unsigned int mask, bool is_max) {
    if (is_max) {
        unsigned int promoted = 0u;
        if (mask & MASKED_MAX_UL) promoted |= MASKED_MAX_TL;
        if (mask & MASKED_MAX_UH) promoted |= MASKED_MAX_TH;
        if (mask & MASKED_MAX_TL) promoted |= MASKED_MAX_TL;
        if (mask & MASKED_MAX_TH) promoted |= MASKED_MAX_TH;
        return promoted;
    }

    if ((mask & (MASKED_MIN_U | MASKED_MIN_T)) != 0u) {
        return MASKED_MIN_T;
    }
    return 0u;
}

static std::string encode_masked_simple_state(unsigned int parent_id,
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

Automaton* NestedAutomaton::flatten_MinMax_Inf_masked(value_function_t finite_aggregator,
                                                      weight_t threshold) {
    const bool is_max = (finite_aggregator == Max_f);
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf_masked requires Max_f or Min_f");
    }

    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i <= this->getChildrenSize(); ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int total_child_states = cumulative_size[this->getChildrenSize()];

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

    std::vector<unsigned int> init_mask(total_child_states, 0u);
    const bool init_epoch_nonempty = false;
    std::string initial_state = encode_masked_simple_state(
        (unsigned int)this->initial->getId(),
        1u,
        init_mask,
        total_child_states,
        init_epoch_nonempty
    );
    parser->states.insert(initial_state);
    parser->initial = initial_state;

    std::vector<masked_simple_work_item> worklist;
    worklist.push_back({
        initial_state,
        (unsigned int)this->initial->getId(),
        1u,
        init_mask,
        init_epoch_nonempty
    });

    while (!worklist.empty()) {
        masked_simple_work_item current = std::move(worklist.back());
        worklist.pop_back();

        State* parent_state = this->getStates()->at(current.parent_state_id);
        for (Symbol* symbol : *parent_state->getAlphabet()) {
            auto* parent_succs = parent_state->getSuccessors(symbol->getId());
            if (!parent_succs) continue;

            for (Edge* parent_edge : *parent_succs) {
                const unsigned int next_parent_id = (unsigned int)parent_edge->getTo()->getId();
                const unsigned int child_id = (unsigned int)parent_edge->getWeight()->getValue().to_float();
                const bool is_silent = (this->getChild(child_id)->getStates()->size() == 1);

                std::vector<unsigned int> spawned_mask = current.child_mask;
                unsigned int new_phase = current.parent_phase;
                const bool reset_epoch = (current.parent_phase == 2u);
                if (reset_epoch) {
                    new_phase = 0u;
                }

                if (!is_silent) {
                    const unsigned int spawn_state = (unsigned int)this->getChild(child_id)->initial->getId();
                    const unsigned int spawn_idx = cumulative_size[child_id] + spawn_state;
                    spawned_mask[spawn_idx] |= masked_spawn_mask(is_max);

                    if (new_phase == 1u) {
                        new_phase = 0u;
                    }
                }

                struct MaskedChildMove {
                    unsigned int to_idx;
                    weight_t edge_w;
                    bool to_final;
                };

                struct MaskedClassBucket {
                    bool tracked;
                    bool high_seen;
                    std::vector<MaskedChildMove> moves;
                };

                const std::vector<unsigned int> snapshot_mask = spawned_mask;
                std::vector<MaskedClassBucket> buckets;
                bool has_stuck_child = false;

                auto push_bucket = [&](unsigned int cid,
                                       unsigned int sid,
                                       bool tracked_class,
                                       bool high_seen_class) {
                    ChildAutomaton* child = this->getChild(cid);
                    State* child_state = child->getStates()->at(sid);
                    if (child_state->getFinal()) {
                        return;
                    }

                    auto* child_succs = child_state->getSuccessors(symbol->getId());
                    if (!child_succs || child_succs->empty()) {
                        has_stuck_child = true;
                        return;
                    }

                    MaskedClassBucket bucket;
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
                        if (!masked_mask_has_active(mask, is_max)) continue;

                        if (is_max) {
                            if (mask & MASKED_MAX_UL) push_bucket(cid, sid, false, false);
                            if (mask & MASKED_MAX_UH) push_bucket(cid, sid, false, true);
                            if (mask & MASKED_MAX_TL) push_bucket(cid, sid, true, false);
                            if (mask & MASKED_MAX_TH) push_bucket(cid, sid, true, true);
                        } else {
                            if (mask & MASKED_MIN_U) push_bucket(cid, sid, false, false);
                            if (mask & MASKED_MIN_T) push_bucket(cid, sid, true, false);
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

                std::vector<unsigned int> next_mask(total_child_states, 0u);
                bool base_epoch_nonempty = reset_epoch ? false : current.epoch_nonempty;
                if (!is_silent) {
                    base_epoch_nonempty = true;
                }

                std::function<void(size_t, bool, bool)> explore_bucket;
                explore_bucket = [&](size_t bucket_idx, bool any_failure, bool epoch_nonempty) {
                    if (bucket_idx == buckets.size()) {
                        std::vector<unsigned int> result_mask = next_mask;
                        unsigned int result_phase = new_phase;
                        bool is_final = false;

                        bool any_tracked_active = false;
                        for (unsigned int i = 0; i < total_child_states; ++i) {
                            if (masked_mask_has_tracked(result_mask[i], is_max) &&
                                masked_mask_has_active(result_mask[i], is_max)) {
                                any_tracked_active = true;
                                break;
                            }
                        }

                        if (!any_tracked_active && new_phase == 0u) {
                            for (unsigned int i = 0; i < total_child_states; ++i) {
                                result_mask[i] = masked_promote_mask(result_mask[i], is_max);
                            }
                            result_phase = 2u;
                            if (epoch_nonempty) {
                                is_final = this->getStates()->at(next_parent_id)->getFinal();
                            }
                        }

                        const std::string next_state = encode_masked_simple_state(
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

                    const MaskedClassBucket& bucket = buckets[bucket_idx];
                    for (const MaskedChildMove& move : bucket.moves) {
                        if (is_max) {
                            const bool edge_high = (move.edge_w >= threshold);
                            const bool now_high = bucket.high_seen || edge_high;

                            if (move.to_final) {
                                const bool fail_here = !now_high;
                                const bool epoch_nonempty_here = epoch_nonempty || bucket.tracked;
                                explore_bucket(bucket_idx + 1,
                                               any_failure || fail_here,
                                               epoch_nonempty_here);
                            } else {
                                const unsigned int add_mask =
                                    now_high
                                        ? (bucket.tracked ? MASKED_MAX_TH : MASKED_MAX_UH)
                                        : (bucket.tracked ? MASKED_MAX_TL : MASKED_MAX_UL);
                                const unsigned int saved = next_mask[move.to_idx];
                                next_mask[move.to_idx] |= add_mask;
                                explore_bucket(bucket_idx + 1, any_failure, epoch_nonempty);
                                next_mask[move.to_idx] = saved;
                            }
                        } else {
                            const bool edge_high = (move.edge_w >= threshold);
                            if (!edge_high) {
                                const bool epoch_nonempty_here = epoch_nonempty || bucket.tracked;
                                explore_bucket(bucket_idx + 1, true, epoch_nonempty_here);
                            } else if (move.to_final) {
                                const bool epoch_nonempty_here = epoch_nonempty || bucket.tracked;
                                explore_bucket(bucket_idx + 1, any_failure, epoch_nonempty_here);
                            } else {
                                const unsigned int add_mask = bucket.tracked ? MASKED_MIN_T : MASKED_MIN_U;
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

    std::string newname = "unnested_masked(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;
    return unnested;
}

// ============================================================================
// THRESHOLD-OBLIGATION FIX: Min_f/Max_f + Inf/LimInf
//

// =============================================================================
// Archived old SumPlusMinus Sup/LimSup helper machinery
// =============================================================================

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

// =============================================================================
// Archived old SumPlusMinus Inf/LimInf helper machinery
// =============================================================================

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

// =============================================================================
// Archived old MinMax Sup/LimSup single-token helper machinery
// =============================================================================

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

