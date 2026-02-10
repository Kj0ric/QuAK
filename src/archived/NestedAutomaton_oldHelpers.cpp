/**
 * NestedAutomaton_oldHelpers.cpp
 *
 * ARCHIVED CODE - NOT COMPILED
 *
 * This file contains old helper functions and flatten implementations that were
 * superseded by the obligation-based flatten_regular() function. These are kept
 * for reference and potential future use.
 *
 * The code here uses monitor-DFA-based tracking which has been replaced by
 * the more efficient obligation-based tracking in flatten_regular().
 *
 * To use any of this code, you would need to:
 * 1. Add the required type definitions back to NestedAutomaton.h
 * 2. Add the function declarations to NestedAutomaton.h
 * 3. Move the implementations to NestedAutomaton.cpp (removing the #if 0 guards)
 */

#if 0  // ENTIRE FILE IS ARCHIVED - NOT COMPILED

// =============================================================================
// SECTION 1: TYPE DEFINITIONS
// =============================================================================
// These types would need to be added back to NestedAutomaton.h if reusing this code.

using MonitorKey = std::pair<size_t, weight_t>;  // (child_index, guessed_return_value)

// BuchiState for flatten_regular_parent_trivial
// Tracks: parent state, last guessed return value, two sets of monitor states (P1, P2)
struct BuchiState {
    State* parent_state;    // Current state in the parent automaton
    weight_t last_guess;    // Last guessed return value
    SetStd<State*> P1;      // Set of active monitor states (current epoch)
    SetStd<State*> P2;      // Set of active monitor states (previous epoch)

    BuchiState() : parent_state(nullptr), last_guess(0), P1(), P2() {}
    BuchiState(State* parent, weight_t guess, const SetStd<State*>& p1, const SetStd<State*>& p2)
        : parent_state(parent), last_guess(guess), P1(p1), P2(p2) {}

    bool operator<(const BuchiState& other) const {
        if (parent_state != other.parent_state) return parent_state < other.parent_state;
        if (last_guess != other.last_guess) return last_guess < other.last_guess;
        if (P1 != other.P1) return P1 < other.P1;
        return P2 < other.P2;
    }

    bool operator==(const BuchiState& other) const {
        return parent_state == other.parent_state &&
            last_guess == other.last_guess &&
            P1 == other.P1 &&
            P2 == other.P2;
    }
};

// BuchiState_acceptance for flatten_regular_parent_acceptance
// Similar to BuchiState but uses acceptance_flag instead of last_guess
// for generalized Buchi to Buchi conversion
struct BuchiState_acceptance {
    State* parent_state;
    SetStd<State*> P1;
    SetStd<State*> P2;
    uint8_t acceptance_flag; // 0 = waiting for parent accept, 1 = waiting for P2 empty

    BuchiState_acceptance()
        : parent_state(nullptr), P1(), P2(), acceptance_flag(0) {}

    BuchiState_acceptance(State* parent,
                          const SetStd<State*>& p1,
                          const SetStd<State*>& p2,
                          uint8_t af)
        : parent_state(parent), P1(p1), P2(p2), acceptance_flag(af) {}

    bool operator<(const BuchiState_acceptance& other) const {
        if (parent_state != other.parent_state) return parent_state < other.parent_state;
        if (P1 != other.P1) return P1 < other.P1;
        if (P2 != other.P2) return P2 < other.P2;
        return acceptance_flag < other.acceptance_flag;
    }

    bool operator==(const BuchiState_acceptance& other) const {
        return parent_state == other.parent_state &&
               P1 == other.P1 &&
               P2 == other.P2 &&
               acceptance_flag == other.acceptance_flag;
    }
};


// =============================================================================
// SECTION 2: UTILITY FUNCTIONS
// =============================================================================
// These are shared helper functions used by the flatten implementations below.

// Compute the global set of all possible return values across all children
// Used for debugging - replaced by per-child computation in flatten_regular
SetStd<weight_t> NestedAutomaton::computeGlobalReturnValues(value_function_t finVal, weight_t bound) {
    SetStd<weight_t> global_values;

    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (child == nullptr) continue;

        SetStd<weight_t> child_values = this->computeChildReturnValues(child, finVal, bound);

        for (weight_t val : child_values) {
            global_values.insert(val);
        }
    }

    global_values.insert(weight_t(SILENT));
    return global_values;
}

// Construct all S_ij (monitor DFAs) and collect Q_S and F_S
// Each monitor S_ij tracks whether child i can return value j
void constructMonitors(
    const NestedAutomaton* nwa,
    const SetStd<weight_t>& global_return_values,
    MapStd<MonitorKey, ChildAutomaton*>& monitors,
    SetStd<State*>& Q_S,
    SetStd<State*>& F_S,
    value_function_t finVal,
    weight_t bound,
    std::vector<SetStd<weight_t>>& child_return_values
) {
    for (size_t i = 0; i < nwa->getChildrenSize(); ++i) {
        ChildAutomaton* child = nwa->getChild(i);
        if (child == nullptr || child->getStates()->size() < 2) continue;

        for (weight_t j : child_return_values[i]) {
            ChildAutomaton* monitor = child->determiniseToS_ij(i, j, finVal, bound);

            MonitorKey key = {i, j};
            monitors.insert(key, monitor);

            for (size_t s = 0; s < monitor->getStates()->size(); ++s) {
                State* st = monitor->getStates()->at(s);
                Q_S.insert(st);
                if (st->getFinal()) F_S.insert(st);
            }
        }
    }
}

// Find successors of states in P on symbol a, excluding final states
SetStd<State*> stepMonitors(const SetStd<State*>& P, Symbol* a, const SetStd<State*>& F_S) {
    SetStd<State*> result;

    for (State* q : P) {
        for (Edge* e : *(q->getSuccessors(a->getId()))) {
            State* q_prime = e->getTo();
            if (!F_S.contains(q_prime)) {
                result.insert(q_prime);
            }
        }
    }
    return result;
}

// Remove accepting states from P
void removeFinalStates(SetStd<State*>& P, const SetStd<State*>& F_S) {
    auto it = P.begin();
    while (it != P.end()) {
        if (F_S.contains(*it)) {
            State* to_remove = *it;
            ++it;
            P.erase(to_remove);
        } else {
            ++it;
        }
    }
}

// Debug helper to print a BuchiState
void printBuchiState([[maybe_unused]] const BuchiState& bs) {
#ifdef DEBUG
    std::cout << "<";
    if (bs.parent_state) std::cout << bs.parent_state->getName();
    else std::cout << "null";
    std::cout << ", " << bs.last_guess;
    std::cout << ", P1={";

    bool first = true;
    for (State* s : bs.P1) {
        if (!first) std::cout << ",";
        std::cout << s->getName();
        first = false;
    }
    std::cout << "}, P2={";

    first = true;
    for (State* s : bs.P2) {
        if (!first) std::cout << ",";
        std::cout << s->getName();
        first = false;
    }
    std::cout << "}>";
#endif
}


// =============================================================================
// SECTION 3: flatten_regular_parent_trivial AND HELPERS
// =============================================================================
// This implementation assumes all parent states are accepting (trivial acceptance).
// Uses BuchiState with last_guess field.
// Accepting states: those where P2 is empty.

// Initialize Buchi automaton components for flatten_regular_parent_trivial
State* initializeBuchi(
    const NestedAutomaton* nwa,
    MapArray<Symbol*>*& new_alphabet,
    MapArray<Weight*>*& new_weights,
    MapStd<weight_t, Weight*>& weight_register,
    SetStd<weight_t>& global_return_values,
    MapStd<BuchiState, State*>& state_map,
    BuchiState init_buchi,
    weight_t global_min,
    weight_t global_max,
    std::queue<BuchiState>& worklist,
    unsigned int& state_counter
) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    // Copy alphabet from parent
    size_t alph_size = nwa->getAlphabetSize();
    new_alphabet = new MapArray<Symbol*>(alph_size);
    for (size_t i = 0; i < alph_size; ++i) {
        Symbol* original = nwa->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    // Create weights array and weight register
    new_weights = new MapArray<Weight*>(global_return_values.size());
    weight_register.clear();

    for (weight_t value : global_return_values) {
        Weight* w = new Weight(value);
        new_weights->insert(w->getId(), w);
        weight_register.insert(value, w);
    }

    state_counter = 0;

    init_buchi.parent_state = nwa->getInitial();
    init_buchi.last_guess = weight_t(INIT_BUCHI_VALUE);
    init_buchi.P1 = SetStd<State*>();
    init_buchi.P2 = SetStd<State*>();

    std::ostringstream ss;
    ss << "b_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);

    state_map[init_buchi] = init_state;
    worklist.push(init_buchi);

    return init_state;
}

// Process a single transition in the Buchi construction
void processBuchiTransition(
    const BuchiState& current_gs,
    unsigned int symbol_id,
    MapStd<BuchiState, State*>& state_map,
    MapArray<Symbol*>* new_alphabet,
    MapArray<Weight*>* new_weights,
    MapStd<weight_t, Weight*>& weight_register,
    const MapStd<MonitorKey, ChildAutomaton*>& monitors,
    const SetStd<State*>& F_S,
    unsigned int& state_counter,
    SetStd<weight_t>& global_return_values,
    weight_t global_min,
    weight_t global_max,
    std::queue<BuchiState>& worklist,
    const std::vector<SetStd<weight_t>>& child_return_values
) {
    Symbol* symbol = new_alphabet->at(symbol_id);
    State* current_state = state_map[current_gs];

    SetStd<State*> P1next = stepMonitors(current_gs.P1, symbol, F_S);
    SetStd<State*> P2next = stepMonitors(current_gs.P2, symbol, F_S);

    for (Edge* parent_edge : *(current_gs.parent_state)->getSuccessors(symbol_id)) {
        State* q_prime = parent_edge->getTo();
        bool is_silent = (parent_edge->getWeight()->getValue() == 0);

        if (is_silent) {
            // CASE (A): Silent transition
            BuchiState next_global(q_prime, SILENT, P1next, P2next);

            if (!state_map.contains(next_global)) {
                std::ostringstream ss;
                ss << "b_" << state_counter++;
                State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                state_map[next_global] = next_state;
                worklist.push(next_global);
            }

            Weight* weight = weight_register.at(SILENT);
            Edge* new_edge = new Edge(symbol, weight, current_state, state_map[next_global]);
            current_state->addSuccessor(new_edge);
            state_map[next_global]->addPredecessor(new_edge);
        } else {
            // CASE (B/C): Call transitions
            weight_t parent_weight = parent_edge->getWeight()->getValue();
            size_t child_index = static_cast<size_t>(parent_weight.to_float());

            const SetStd<weight_t>& child_vals = child_return_values[child_index];

            for (const weight_t& guess : child_vals) {
                MonitorKey key = {child_index, guess};
                if (!monitors.contains(key)) continue;

                ChildAutomaton* monitor = monitors.at(key);
                State* monitor_init = monitor->getInitial();
                monitor_init = (*monitor_init->getSuccessors(symbol_id)->begin())->getTo();

                SetStd<State*> P1new, P2new;

                if (current_gs.P2.size() == 0) {
                    // CASE (B): P2 was empty - start new epoch
                    P1new.insert(monitor_init);
                    P2new = P1next;
                } else {
                    // CASE (C): P2 not empty - continue epoch
                    P1new = P1next;
                    P1new.insert(monitor_init);
                    removeFinalStates(P1new, F_S);
                    P2new = P2next;
                }

                BuchiState next_global(q_prime, guess, P1new, P2new);

                if (!state_map.contains(next_global)) {
                    std::ostringstream ss;
                    ss << "b_" << state_counter++;
                    State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                    state_map[next_global] = next_state;
                    worklist.push(next_global);
                }

                Weight* weight = weight_register.at(guess);
                Edge* new_edge = new Edge(symbol, weight, current_state, state_map[next_global]);
                current_state->addSuccessor(new_edge);
                state_map[next_global]->addPredecessor(new_edge);
            }
        }
    }
}

// Main flatten function for trivial parent acceptance
// Assumes all parent states are accepting
Automaton* NestedAutomaton::flatten_regular_parent_trivial(value_function_t finVal, weight_t bound) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    MapArray<Symbol*>* new_alphabet;
    MapArray<Weight*>* new_weights;
    weight_t global_min, global_max;
    BuchiState init_buchi;

    MapStd<BuchiState, State*> state_map;
    std::queue<BuchiState> worklist;
    MapStd<weight_t, Weight*> weight_register;
    unsigned int state_counter;

    // 1. Compute return values per child
    size_t k = this->getChildrenSize();
    std::vector<SetStd<weight_t>> child_return_values(k);
    SetStd<weight_t> global_return_values;
    global_return_values.insert(weight_t(SILENT));

    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (!child) continue;
        child_return_values[i] = this->computeChildReturnValuesParentAware(i, finVal, bound);
        for (const weight_t& v : child_return_values[i]) {
            global_return_values.insert(v);
        }
    }

    for (weight_t val : global_return_values) {
        if (val != SILENT) {
            global_min = std::min(global_min, val);
            global_max = std::max(global_max, val);
        }
    }

    // 2. Construct monitors
    MapStd<MonitorKey, ChildAutomaton*> monitors;
    SetStd<State*> Q_S, F_S;
    constructMonitors(this, global_return_values, monitors, Q_S, F_S, finVal, bound, child_return_values);

    // 3. Initialize Buchi
    State* init_state = initializeBuchi(this, new_alphabet, new_weights, weight_register,
        global_return_values, state_map, init_buchi, global_min, global_max, worklist, state_counter);

    // 4. BFS construction
    while (!worklist.empty()) {
        BuchiState current_gs = worklist.front();
        worklist.pop();

        for (unsigned symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            processBuchiTransition(current_gs, symbol_id, state_map, new_alphabet, new_weights,
                weight_register, monitors, F_S, state_counter, global_return_values,
                global_min, global_max, worklist, child_return_values);
        }
    }

    // 5. Create states array and mark accepting states
    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [global_state, state] : state_map) {
        new_states->insert(state->getId(), state);
        if (global_state.P2.size() == 0) {
            state->setFinal(true);
        }
    }

    // 6. Construct Buchi automaton
    std::string buchi_name = "Buchi(" + this->getName() + ")";
    Automaton* buchi = new Automaton(buchi_name, new_alphabet, new_states,
        new_weights, global_min, global_max, init_state);

    // 7. Cleanup monitors
    for (const auto& [key, monitor] : monitors) {
        delete monitor;
    }

    return buchi;
}

// Check if all parent states are final (accepting)
bool NestedAutomaton::allParentStatesFinal() const {
    for (State* q : *(this->getStates())) {
        if (!q->getFinal()) return false;
    }
    return true;
}


// =============================================================================
// SECTION 4: flatten_regular_parent_acceptance AND HELPERS
// =============================================================================
// This implementation handles non-trivial parent acceptance using
// generalized Buchi to Buchi conversion (on-the-fly).
// Uses BuchiState_acceptance with acceptance_flag for phase tracking.
// Accepting states: those in phase WAIT_P2EMPTY with P2 empty.

// Acceptance phase constants
static constexpr bool ACC_WAIT_parent_  = 0; // waiting to see parent in accepting state
static constexpr bool ACC_WAIT_P2EMPTY_ = 1; // waiting to see P2 empty

// Compute next acceptance phase
static inline bool advance_acc_phase(const BuchiState_acceptance& s) {
    if (s.acceptance_flag == ACC_WAIT_parent_) {
        return (s.parent_state->getFinal()) ? ACC_WAIT_P2EMPTY_ : ACC_WAIT_parent_;
    } else {
        return (s.P2.size() == 0) ? ACC_WAIT_parent_ : ACC_WAIT_P2EMPTY_;
    }
}

// Initialize Buchi for acceptance-aware version
State* initializeBuchi_acceptance(
    const NestedAutomaton* nwa,
    MapArray<Symbol*>*& new_alphabet,
    MapArray<Weight*>*& new_weights,
    MapStd<weight_t, Weight*>& weight_register,
    SetStd<weight_t>& global_return_values,
    MapStd<BuchiState_acceptance, State*>& state_map,
    BuchiState_acceptance init_buchi,
    weight_t global_min,
    weight_t global_max,
    std::queue<BuchiState_acceptance>& worklist,
    unsigned int& state_counter
) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    size_t alph_size = nwa->getAlphabetSize();
    new_alphabet = new MapArray<Symbol*>(alph_size);
    for (size_t i = 0; i < alph_size; ++i) {
        Symbol* original = nwa->getAlphabet()->at(i);
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

    state_counter = 0;

    init_buchi.parent_state = nwa->getInitial();
    init_buchi.P1 = SetStd<State*>();
    init_buchi.P2 = SetStd<State*>();
    init_buchi.acceptance_flag = ACC_WAIT_parent_;

    std::ostringstream ss;
    ss << "b_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);

    state_map[init_buchi] = init_state;
    worklist.push(init_buchi);

    return init_state;
}

// Process transition for acceptance-aware version
void processBuchiTransition_acceptance(
    const BuchiState_acceptance& current_gs,
    unsigned int symbol_id,
    MapStd<BuchiState_acceptance, State*>& state_map,
    MapArray<Symbol*>* new_alphabet,
    MapArray<Weight*>* new_weights,
    MapStd<weight_t, Weight*>& weight_register,
    const MapStd<MonitorKey, ChildAutomaton*>& monitors,
    const SetStd<State*>& F_S,
    unsigned int& state_counter,
    SetStd<weight_t>& global_return_values,
    weight_t global_min,
    weight_t global_max,
    std::queue<BuchiState_acceptance>& worklist,
    const std::vector<SetStd<weight_t>>& child_return_values
) {
    Symbol* symbol = new_alphabet->at(symbol_id);
    State* current_state = state_map[current_gs];

    uint8_t phase_after_current = advance_acc_phase(current_gs);

    SetStd<State*> P1next = stepMonitors(current_gs.P1, symbol, F_S);
    SetStd<State*> P2next = stepMonitors(current_gs.P2, symbol, F_S);

    for (Edge* parent_edge : *(current_gs.parent_state)->getSuccessors(symbol_id)) {
        State* q_prime = parent_edge->getTo();
        bool is_silent = (parent_edge->getWeight()->getValue() == 0);

        if (is_silent) {
            BuchiState_acceptance next_global(q_prime, P1next, P2next, phase_after_current);

            if (!state_map.contains(next_global)) {
                std::ostringstream ss;
                ss << "b_" << state_counter++;
                State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                state_map[next_global] = next_state;
                worklist.push(next_global);
            }

            Weight* weight = weight_register.at(SILENT);
            Edge* new_edge = new Edge(symbol, weight, current_state, state_map[next_global]);
            current_state->addSuccessor(new_edge);
            state_map[next_global]->addPredecessor(new_edge);
        } else {
            weight_t parent_weight = parent_edge->getWeight()->getValue();
            size_t child_index = static_cast<size_t>(parent_weight.to_float());

            const SetStd<weight_t>& child_vals = child_return_values[child_index];

            for (const weight_t& guess : child_vals) {
                MonitorKey key = {child_index, guess};
                if (!monitors.contains(key)) continue;

                ChildAutomaton* monitor = monitors.at(key);
                State* monitor_init = monitor->getInitial();
                monitor_init = (*monitor_init->getSuccessors(symbol_id)->begin())->getTo();

                SetStd<State*> P1new, P2new;

                if (current_gs.P2.size() == 0) {
                    P1new.insert(monitor_init);
                    P2new = P1next;
                } else {
                    P1new = P1next;
                    P1new.insert(monitor_init);
                    removeFinalStates(P1new, F_S);
                    P2new = P2next;
                }

                BuchiState_acceptance next_global(q_prime, P1new, P2new, phase_after_current);

                if (!state_map.contains(next_global)) {
                    std::ostringstream ss;
                    ss << "b_" << state_counter++;
                    State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                    state_map[next_global] = next_state;
                    worklist.push(next_global);
                }

                Weight* weight = weight_register.at(guess);
                Edge* new_edge = new Edge(symbol, weight, current_state, state_map[next_global]);
                current_state->addSuccessor(new_edge);
                state_map[next_global]->addPredecessor(new_edge);
            }
        }
    }
}

// Main flatten function with parent acceptance handling
Automaton* NestedAutomaton::flatten_regular_parent_acceptance(value_function_t finVal, weight_t bound) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    MapArray<Symbol*>* new_alphabet = nullptr;
    MapArray<Weight*>* new_weights = nullptr;
    weight_t global_min, global_max;

    BuchiState_acceptance init_buchi;
    MapStd<BuchiState_acceptance, State*> state_map;
    std::queue<BuchiState_acceptance> worklist;
    MapStd<weight_t, Weight*> weight_register;
    unsigned int state_counter = 0;

    // 1. Compute return values
    size_t k = this->getChildrenSize();
    std::vector<SetStd<weight_t>> child_return_values(k);
    SetStd<weight_t> global_return_values;
    global_return_values.insert(weight_t(SILENT));

    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (!child) continue;
        child_return_values[i] = this->computeChildReturnValuesParentAware(i, finVal, bound);
        for (const weight_t& v : child_return_values[i]) {
            global_return_values.insert(v);
        }
    }

    bool have_non_silent = false;
    for (weight_t val : global_return_values) {
        if (val == SILENT) continue;
        if (!have_non_silent) {
            global_min = val;
            global_max = val;
            have_non_silent = true;
        } else {
            global_min = std::min(global_min, val);
            global_max = std::max(global_max, val);
        }
    }
    if (!have_non_silent) {
        global_min = weight_t(0);
        global_max = weight_t(0);
    }

    // 2. Construct monitors
    MapStd<MonitorKey, ChildAutomaton*> monitors;
    SetStd<State*> Q_S, F_S;
    constructMonitors(this, global_return_values, monitors, Q_S, F_S, finVal, bound, child_return_values);

    // 3. Initialize
    State* init_state = initializeBuchi_acceptance(this, new_alphabet, new_weights, weight_register,
        global_return_values, state_map, init_buchi, global_min, global_max, worklist, state_counter);

    // 4. BFS construction
    while (!worklist.empty()) {
        BuchiState_acceptance current_gs = worklist.front();
        worklist.pop();

        for (unsigned symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            processBuchiTransition_acceptance(current_gs, symbol_id, state_map, new_alphabet, new_weights,
                weight_register, monitors, F_S, state_counter, global_return_values,
                global_min, global_max, worklist, child_return_values);
        }
    }

    // 5. Create states and mark accepting
    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [global_state, state] : state_map) {
        new_states->insert(state->getId(), state);
        if (global_state.acceptance_flag == ACC_WAIT_P2EMPTY_ && global_state.P2.size() == 0) {
            state->setFinal(true);
        }
    }

    // 6. Construct automaton
    std::string buchi_name = "Buchi(" + this->getName() + ")";
    Automaton* buchi = new Automaton(buchi_name, new_alphabet, new_states,
        new_weights, global_min, global_max, init_state);

    // 7. Cleanup
    for (const auto& [key, monitor] : monitors) {
        delete monitor;
    }

    return buchi;
}

#endif  // ENTIRE FILE IS ARCHIVED
