#include <cstddef>
#include <ostream>
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <queue>
#include <sstream>

#include "Automaton.h"
#include "ChildAutomaton.h"
#include "NestedAutomaton.h"
#include "Map.h"
#include "Parser.h"
#include "Edge.h"
#include "Set.h"
#include "State.h"
#include "Symbol.h"
#include "Weight.h"
#include "utility.h"
#include "FORKLIFT/inclusion.h"

/* ------------------------------ DESTRUCTOR & CONSTRUCTORS ------------------------------ */
NestedAutomaton::~NestedAutomaton() {
    // Clean up children_ array
    if (children_ != nullptr) {
        for (size_t i = 0; i < children_->size(); ++i) {
            delete children_->at(i);
        }
        delete children_;
    }
}

NestedAutomaton::NestedAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register) 
    : Automaton(name, parser, sync_register)
{
    // Allocate children_ array with the num of child parsers
    children_ = new MapArray<ChildAutomaton*>(parser->child_parsers.size()); 
    
    //  For each child parser, initialize ChildAutomaton objects
    for (unsigned i = 0; i < parser->child_parsers.size(); ++i) {
        Parser* child_parser = parser->child_parsers[i];
        if (child_parser) {
            //auto* child = new ChildAutomaton(name + "_child" + std::to_string(i), child_parser, sync_register);
            auto* child = new ChildAutomaton(std::to_string(i), child_parser, sync_register);
            children_->insert(i, child);
        }
    }
}

// Constructs a nested automaton from a file with the same alphabet as another automaton
NestedAutomaton::NestedAutomaton(std::string filename, Automaton* other) 
    : Automaton(filename, other)
{
    // Create a new parser from the file to get child information
    Parser* parser = new Parser(filename);
    
    // Set up sync_register if other automaton is provided
    MapStd<std::string, Symbol*> sync_register;
    if (other != nullptr) {
        for (unsigned int symbol_id = 0; symbol_id < other->getAlphabet()->size(); ++symbol_id) {
            Symbol* symbol = other->getAlphabet()->at(symbol_id);
            sync_register.insert(symbol->getName(), symbol);
        }
    }
    
    // Allocate children_ array with child parsers
    children_ = new MapArray<ChildAutomaton*>(parser->child_parsers.size()); 
    
    // Initialize ChildAutomaton objects
    for (unsigned i = 0; i < parser->child_parsers.size(); ++i) {
        Parser* child_parser = parser->child_parsers[i];
        if (child_parser) {
            //auto* child = new ChildAutomaton(name + "_child" + std::to_string(i), child_parser, sync_register);
            auto* child = new ChildAutomaton(std::to_string(i), child_parser, sync_register);
            children_->insert(i, child);
        }
    }
    delete parser;
}

// Helper constructor for NestedAutomaton
NestedAutomaton::NestedAutomaton(const Automaton* parent, MapArray<ChildAutomaton*>* children)
    : Automaton(*parent), // Use the public copy constructor
      children_(children) {
    
        this->setName(parent->getName() + "_noSilent");
}

/* ------------------------------ REMOVING SILENT TRANSITIONS ------------------------------ */
NestedAutomaton* NestedAutomaton::removeSilentTransitions(const NestedAutomaton* A, value_function_t f) {
    // 1. Transform the parent automaton using the base class method
    Automaton* transformed_parent = Automaton::removeSilentTransitions(A, f);

    // 2. Shallow copy the children array (children remain unchanged)
    MapArray<ChildAutomaton*>* copied_children = new MapArray<ChildAutomaton*>(A->children_->size());
    for (unsigned i = 0; i < A->children_->size(); ++i) {
        if (A->children_->at(i)) {
            // Use the copy constructor for ChildAutomaton
            copied_children->insert(i, new ChildAutomaton(*A->children_->at(i)));
        }
    }

    // 3. Create new NestedAutomaton with transformed parent and copied children
    NestedAutomaton* result = new NestedAutomaton(transformed_parent, copied_children);

    delete transformed_parent;
    return result;
}

/* ------------------------------ HELPERS ------------------------------ */
void NestedAutomaton::print(bool full, bool bv_weights, bool bv_only) const {
    print(std::cout, full, bv_weights, bv_only);
}

void NestedAutomaton::print(std::ostream& out, bool full, bool bv_weights, bool bv_only) const {
    out << "(1) NESTED AUTOMATON (" << this->getName() << "):\n";
    Automaton::print(out);

    if (children_ && children_->size() > 0) {
        out << "(2) CHILD AUTOMATA:" << std::endl;
        for (unsigned i = 0; i < children_->size(); ++i) {
            ChildAutomaton* child = children_->at(i);
            if (child) {
                out << "[Child " << i << "]" << std::endl;
                child->print(out);
            }
        }
    } else {
        out << "The nested automaton (" << this->getName() << ") has no child automata." << std::endl;
    }
}

std::size_t NestedAutomaton::getChildrenSize() const {
	return children_ ? children_->size() : 0;
}

ChildAutomaton* NestedAutomaton::getChild(std::size_t index) const {
	if (!children_ || index >= children_->size()) {
		return nullptr;
	}
	return children_->at(index);
}


/* ------------------------ Helper utilities ------------------------- */
// Helper function to apply SumB bounding to a weight value
weight_t applyBound(weight_t value, weight_t bound) {
    if (value > bound) {
        return bound;
    } else if (value < -bound) {
        return -bound;
    } else {
        return value;
    }
}

/* ------------------------ Büchi Transformation ----------------------- */
// Key lemma:
// Assumes: a NWA AA with regular WA children B_i
// Ensures: a silf(f)-WA A' that is equivalent to A

void computeGlobalDomains(const NestedAutomaton* nwa, weight_t& global_min, weight_t& global_max){
    global_min = std::numeric_limits<float>::max();
    global_max = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < nwa->getChildrenSize(); ++i) {
        ChildAutomaton* child = nwa->getChild(i);
        if (child == nullptr) continue;
        global_min = std::min(global_min, child->getMinDomain());
        global_max = std::max(global_max, child->getMaxDomain());
    }
}

//Helper: Efficient dominance-based state removal for MinState pairs
void removeDominatedStates(SetStd<std::pair<State*, weight_t>>& visited, State* target_state, weight_t new_value, value_function_t finVal) {
    SetStd<std::pair<State*, weight_t>> to_remove;
    
    // Collect all dominated states (same state with worse min value)
    for (const auto& visited_state : visited) {
        if (finVal == Min_f) {
            if (visited_state.first == target_state && visited_state.second >= new_value) {
                to_remove.insert(visited_state);
            }
        } else if (finVal == Max_f) {
            if (visited_state.first == target_state && visited_state.second <= new_value) {
                to_remove.insert(visited_state);
            }
        }
    }
    
    // Remove dominated entries in batch
    for (const auto& remove_state : to_remove) {
        visited.erase(remove_state);
    }
}

// Helper: Check if a state-value pair is dominated by existing visited states
bool isDominatedState(const SetStd<std::pair<State*, weight_t>>& visited, State* target_state, weight_t new_value, value_function_t finVal) {
    for (const auto& visited_state : visited) {
        if (finVal == Min_f) {
            if (visited_state.first == target_state && visited_state.second <= new_value) {
                return true; // Existing state has better (smaller) min value
            }
        } else if (finVal == Max_f) {
            if (visited_state.first == target_state && visited_state.second >= new_value) {
                return true; // Existing state has better (larger) max value
            }
        }
    }
    return false;
}

SetStd<weight_t> computeMinMaxReturnValues(ChildAutomaton* child, value_function_t finVal) {
    SetStd<weight_t> return_values;
    
    // State representation: (current_automaton_state, accumulated_value_so_far_on_path)
    using ValueState = std::pair<State*, weight_t>;
    std::queue<ValueState> worklist;  // BFS queue
    SetStd<ValueState> visited;       // Visited (state, value) pairs
    
    // Start exploration from initial state with appropriate initial value
    weight_t initial_value;
    if (finVal == Min_f) {
        initial_value = std::numeric_limits<float>::max();     // +∞ for Min_f
    } else { // Max_f
        initial_value = std::numeric_limits<float>::lowest();  // -∞ for Max_f
    }
    
    ValueState init_state = {child->getInitial(), initial_value};
    worklist.push(init_state);
    visited.insert(init_state);
    
    while (!worklist.empty()) {
        ValueState current = worklist.front(); 
        worklist.pop();
        
        State* curr_state = current.first;
        weight_t curr_value = current.second;
        
        // Explore all outgoing transitions from current state
        for (size_t sym_id = 0; sym_id < child->getAlphabetSize(); ++sym_id) {
            SetStd<Edge*>* successors = curr_state->getSuccessors(sym_id);
            if (!successors) continue;
            
            for (Edge* edge : *successors) {
                State* next_state = edge->getTo();
                weight_t edge_weight = edge->getWeight()->getValue();
                
                // Update accumulated value based on value function
                weight_t next_value;
                if (finVal == Min_f) {
                    next_value = std::min(curr_value, edge_weight);
                } else { // Max_f
                    next_value = std::max(curr_value, edge_weight);
                }
                
                ValueState next_value_state = {next_state, next_value};
                
                // If reached a final state, record this value as returnable
                if (child->isFinal(next_state)) {
                    return_values.insert(next_value);
                    
                    #ifdef DEBUG
                    std::cout << (finVal == Min_f ? "Min" : "Max") << "_f path to final state " 
                              << next_state->getName() << ": " 
                              << (finVal == Min_f ? "min" : "max") << "_value = " << next_value << std::endl;
                    #endif
                    continue; // Don't continue exploration from final states (treat as sinks)
                }
                
                // Dominance checking: avoid redundant exploration
                // Check if we've already seen this state with a better value
                bool should_explore = true;
                for (const ValueState& visited_state : visited) {
                    if (visited_state.first == next_state) {
                        if (finVal == Min_f) {
                            // For Min_f: existing smaller value dominates
                            if (visited_state.second <= next_value) {
                                should_explore = false;
                                break;
                            }
                        } else { // Max_f
                            // For Max_f: existing larger value dominates
                            if (visited_state.second >= next_value) {
                                should_explore = false;
                                break;
                            }
                        }
                    }
                }
                
                if (should_explore) {
                    // Remove any dominated entries: same state with worse value
                    SetStd<ValueState> to_remove;
                    for (const ValueState& visited_state : visited) {
                        if (visited_state.first == next_state) {
                            if (finVal == Min_f) {
                                // Remove entries with larger (worse) min values
                                if (visited_state.second >= next_value) {
                                    to_remove.insert(visited_state);
                                }
                            } else { // Max_f
                                // Remove entries with smaller (worse) max values
                                if (visited_state.second <= next_value) {
                                    to_remove.insert(visited_state);
                                }
                            }
                        }
                    }
                    
                    // Remove dominated entries in batch
                    for (const ValueState& remove_state : to_remove) {
                        visited.erase(remove_state);
                    }
                    
                    // Add this new state-value pair for continued exploration
                    visited.insert(next_value_state);
                    worklist.push(next_value_state);
                }
            }
        }
    }
    
    return return_values;
}

SetStd<weight_t> computeSumBReturnValues(ChildAutomaton* child, weight_t bound) {
    SetStd<weight_t> return_values;

    // BFS to explore all possible sums
    // State: (automaton_state, accumulated_sum, bound_value_hit)
    // bound_value_hit: 0 = never exceeded, +bound = hit upper bound, -bound = hit lower bound
    using SumState = std::tuple<State*, weight_t, weight_t>;
    std::queue<SumState> worklist;
    SetStd<SumState> visited;
    
    // Start from initial state with sum 0, no bound hit
    SumState init_state = {child->getInitial(), weight_t(0), weight_t(0)};
    worklist.push(init_state);
    visited.insert(init_state);
    
    // Check if initial state is final
    if (child->isFinal(child->getInitial())) {
        return_values.insert(weight_t(0));
    }
    
    while (!worklist.empty()) {
        SumState current = worklist.front(); 
        worklist.pop();
        
        State* curr_state = std::get<0>(current);
        weight_t curr_sum = std::get<1>(current);
        weight_t bound_hit = std::get<2>(current);  // 0, +bound, or -bound
        
        // If this is a final state, record the return value
        if (child->isFinal(curr_state)) {
            if (bound_hit != weight_t(0)) {
                // Path exceeded bounds at some point -> return the bound value that was hit
                return_values.insert(bound_hit);
            } else {
                // Never exceeded bounds -> return actual sum
                return_values.insert(curr_sum);
            }
            continue;   // Final states are treated as sinks
        }
        
        // Explore all outgoing transitions
        for (size_t sym_id = 0; sym_id < child->getAlphabetSize(); ++sym_id) {
            for (Edge* edge : *(curr_state->getSuccessors(sym_id))) {
                State* next_state = edge->getTo();
                weight_t edge_weight = edge->getWeight()->getValue();
                weight_t raw_sum = curr_sum + edge_weight;
                
                // Determine next state values
                weight_t next_sum;
                weight_t next_bound_hit = bound_hit; 
                
                if (raw_sum > bound && bound_hit == weight_t(0)) {
                    // First time hitting upper bound
                    next_sum = bound;
                    next_bound_hit = bound;  // Remember we hit +bound
                } else if (raw_sum < -bound && bound_hit == weight_t(0)) {
                    // First time hitting lower bound  
                    next_sum = -bound;
                    next_bound_hit = -bound;  // Remember we hit -bound
                } else if (bound_hit != weight_t(0)) {
                    // Already exceeded bounds before, so continue with bounded value
                    next_sum = applyBound(raw_sum, bound);
                    // next_bound_hit stays the same (already hit bound)
                } else {
                    // Normal case: within bounds
                    next_sum = raw_sum;
                    // next_bound_hit stays 0
                }
                
                SumState next_sum_state = {next_state, next_sum, next_bound_hit};
                
                // Continue exploration if not visited
                if (!visited.contains(next_sum_state)) {
                    visited.insert(next_sum_state);
                    
                    if (child->isFinal(next_state)) {
                        // Final state: return bound value if ever exceeded
                        if (next_bound_hit != weight_t(0)) {
                            return_values.insert(next_bound_hit);  // Return the bound that was hit
                        } else {
                            return_values.insert(next_sum); 
                        }
                    } else {
                        // Non-final: continue BFS
                        worklist.push(next_sum_state);
                    }
                }
            }
        }
    }

    return return_values;
}

// Helper: Compute all possible return values for a single child automaton
SetStd<weight_t> computeChildReturnValues(ChildAutomaton* child, value_function_t finVal, weight_t bound) {
    SetStd<weight_t> return_values;
    
    if (!child) {
        return return_values; // Empty set for null child
    }

    if (finVal == Min_f || finVal == Max_f) {
        return_values = computeMinMaxReturnValues(child, finVal);
    } 
    else if (finVal == SumB) {
        if (bound < 0) {
            QUAK_FAIL("SumB requires a non-negative bound");
        }
        return_values = computeSumBReturnValues(child, bound);
    }
    else {
        QUAK_FAIL("Unsupported value function for child automaton");
    }
    
    #ifdef DEBUG
    std::cout << "Child " << child->getName() << " (";
    switch(finVal) {
        case Min_f: std::cout << "Min_f"; break;
        case Max_f: std::cout << "Max_f"; break;
        case SumB: std::cout << "SumB"; break;
        default: std::cout << "Unknown"; break;
    }
    std::cout << ") can return values: {";
    for (weight_t val : return_values) {
        std::cout << val << " ";
    }
    std::cout << "} (count: " << return_values.size() << ")" << std::endl;
    #endif
    
    return return_values;
}

SetStd<weight_t> oldComputeChildReturnValues(ChildAutomaton* child, value_function_t finVal, weight_t bound) {
    SetStd<weight_t> return_values;
    
    if (finVal == Min_f || finVal == Max_f) {
        for (size_t i = 0; i < child->getWeights()->size(); ++i) {
            return_values.insert(child->getWeights()->at(i)->getValue());
        }
    }
    else if (finVal == SumB) {
        if (bound < 0) {
            QUAK_FAIL("SumB requires a non-negative bound");
        }
        
        using SumState = std::tuple<State*, weight_t, weight_t>;
        std::queue<SumState> worklist;
        SetStd<SumState> visited;
        
        SumState init_state = {child->getInitial(), weight_t(0), weight_t(0)};
        worklist.push(init_state);
        visited.insert(init_state);
        
        if (child->isFinal(child->getInitial())) {
            return_values.insert(weight_t(0));
        }
        
        while (!worklist.empty()) {
            SumState current = worklist.front(); 
            worklist.pop();
            
            State* curr_state = std::get<0>(current);
            weight_t curr_sum = std::get<1>(current);
            weight_t bound_hit = std::get<2>(current);
            
            if (child->isFinal(curr_state)) {
                if (bound_hit != weight_t(0)) {
                    return_values.insert(bound_hit);
                } else {
                    return_values.insert(curr_sum);
                }
                continue;
            }
            
            for (size_t sym_id = 0; sym_id < child->getAlphabetSize(); ++sym_id) {
                SetStd<Edge*>* successors = curr_state->getSuccessors(sym_id);
                if (!successors) continue;
                
                for (Edge* edge : *successors) {
                    State* next_state = edge->getTo();
                    weight_t edge_weight = edge->getWeight()->getValue();
                    weight_t raw_sum = curr_sum + edge_weight;
                    
                    weight_t next_sum;
                    weight_t next_bound_hit = bound_hit;
                    
                    if (raw_sum > bound && bound_hit == weight_t(0)) {
                        next_sum = bound;
                        next_bound_hit = bound;
                    } else if (raw_sum < -bound && bound_hit == weight_t(0)) {
                        next_sum = -bound;
                        next_bound_hit = -bound;
                    } else if (bound_hit != weight_t(0)) {
                        next_sum = applyBound(raw_sum, bound);
                    } else {
                        next_sum = raw_sum;
                    }
                    
                    SumState next_sum_state = {next_state, next_sum, next_bound_hit};
                    
                    if (!visited.contains(next_sum_state)) {
                        visited.insert(next_sum_state);
                        
                        if (child->isFinal(next_state)) {
                            if (next_bound_hit != weight_t(0)) {
                                return_values.insert(next_bound_hit);
                            } else {
                                return_values.insert(next_sum);
                            }
                        } else {
                            worklist.push(next_sum_state);
                        }
                    }
                }
            }
        }
    }
    else {
        QUAK_FAIL("Unsupported value function for child automaton");
    }
    
    #ifdef DEBUG
    std::cout << "Child " << child->getName() << " (" << 
        (finVal == Min_f ? "Min_f" : finVal == Max_f ? "Max_f" : "SumB") << 
        ") can return values: {";
    for (weight_t val : return_values) {
        std::cout << val << " ";
    }
    std::cout << "}" << std::endl;
    #endif
    
    return return_values;
}

// Compute the global set of all possible return values across all children
SetStd<weight_t> computeGlobalReturnValues(const NestedAutomaton* nwa, value_function_t finVal, weight_t bound) {
    SetStd<weight_t> global_values;
    
    for (size_t i = 0; i < nwa->getChildrenSize(); ++i) {
        ChildAutomaton* child = nwa->getChild(i);
        if (child == nullptr) continue;
        
        SetStd<weight_t> child_values = computeChildReturnValues(child, finVal, bound);
        
        // Union with global set
        for (weight_t val : child_values) {
            global_values.insert(val);
        }
    }
    
    // Add silent value as well
    global_values.insert(weight_t(SILENT));

    return global_values;
}

using MonitorKey = std::pair<size_t, weight_t>;  // (i, j)
// Construct all S_ij (monitors) and collect Q_S and F_S
void constructMonitors(
    const NestedAutomaton* nwa,
    const SetStd<weight_t>& global_return_values,
    MapStd<MonitorKey, ChildAutomaton*>& monitors,
    SetStd<State*>& Q_S,
    SetStd<State*>& F_S,
    value_function_t finVal,
    weight_t bound
) {
    for (size_t i = 0; i < nwa->getChildrenSize(); ++i) {
        ChildAutomaton* child = nwa->getChild(i);
        if (child == nullptr) continue;

        // Only create monitors for possible return values by the child i
        SetStd<weight_t> child_values = computeChildReturnValues(child, finVal, bound);
        for (weight_t j : child_values) {
            ChildAutomaton* monitor = child->determiniseToS_ij(j, finVal, bound);

            MonitorKey key = {i,j};
            monitors.insert(key, monitor);
            
            // Collect Q_S and F_S
            for (size_t s = 0; s < monitor->getStates()->size(); ++s) {
                Q_S.insert(monitor->getStates()->at(s));
            }
            for (State* s : *(monitor->getFinalStates())) {
                F_S.insert(s);
            }
        }
    }
}

// Find the sucessors of the states in P1 or P2. Exclude if final state
SetStd<State*> stepMonitors(const SetStd<State*>& P, Symbol* a, const SetStd<State*>& F_S) {
    SetStd<State*> result;

    for (State* q : P) {
        // Since monitors are DFA, there's either 1 or 0 transitions
        for (Edge* e : *(q->getSuccessors(a->getId()))){
            State* q_prime = e->getTo();

            // Add only non-accepting states
            if (F_S.contains(q_prime) != true) {
                result.insert(q_prime);
            }
        }    
    }
    return result;
}

// Extra function to remove the accepting states in P
void removeFinalStates(SetStd<State*>&P, const SetStd<State*>& F_S) {
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

// Helper: Initialize büchi automaton components
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
    
    // Copy alphabet from master
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

    // Initialize state counter to name the states of Buchi
    state_counter = 0;
    
    // Fill in Büchi state
    init_buchi.parent_state = nwa->getInitial();
    init_buchi.last_guess = weight_t(INIT_BUCHI_VALUE); 
    init_buchi.P1 = SetStd<State*>();
    init_buchi.P2 = SetStd<State*>();

    std::ostringstream ss;
    ss << "b_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);

    // Map the state to the tuple
    state_map[init_buchi] = init_state;
    // Add init Büchi state to start exploration
    worklist.push(init_buchi);

    return init_state;
}

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
    std::queue<BuchiState>& worklist
) {
    Symbol* symbol = new_alphabet->at(symbol_id);   // Get symbol from id
    State* current_state = state_map[current_gs];        // Get the State object current_state is mapped to

    for (Edge* parent_edge : *(current_gs.parent_state)->getSuccessors(symbol_id)) {
        State* q_prime = parent_edge->getTo();

        // Advance all active monitors
        SetStd<State*> P1next = stepMonitors(current_gs.P1, symbol, F_S);
        SetStd<State*> P2next = stepMonitors(current_gs.P2, symbol, F_S);

        bool is_silent = (parent_edge->getWeight()->getValue() == SILENT);

        if (is_silent) {
            // --------  CASE (A): Silent transition  --------
            // add transition: ( (q,j,P₁,P₂) ─a/⊥→ ( q', ⊥, P₁next, P₂next ) )

            // Create new GS
            BuchiState next_global(q_prime, SILENT, P1next, P2next);

            // Create new state if not seen before
            if (state_map.contains(next_global) != true) {
                std::ostringstream ss;
                ss << "b_" << state_counter++;
                State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                state_map[next_global] = next_state;
                worklist.push(next_global);
            }

            // Create edge with silent weight (use last_guess as weight)
            // Edge object stores a pointer to the weight object in weights array
            Weight* weight = weight_register.at(SILENT);    // TODO: Test if correct
            Edge* new_edge = new Edge(symbol, weight, current_state, state_map[next_global]);
            current_state->addSuccessor(new_edge);
            state_map[next_global]->addPredecessor(new_edge);
        }
        else {
            // --------  CASE (B/C): Call transitions  --------  
            // Extract child automaton index from parent edge TODO: Check if correct
            weight_t parent_weight = parent_edge->getWeight()->getValue();
            size_t child_index = static_cast<size_t>(parent_weight.to_float()); 

            for (weight_t guess : global_return_values) {
                if (guess == SILENT) continue; // SILENT doesn't call any child

                // Let child_index = i and guess = j
                // Look up monitor given (i,j)s
                MonitorKey key = {child_index, guess};
                if (!monitors.contains(key)) {
                    continue; // Skip if this child can't return guess value TODO: check if good idea
                }
                ChildAutomaton* monitor = monitors.at(key);
                // Get the initial state of monitor
                State* monitor_init = monitor->getInitial();

                // Check if P2 is empty
                SetStd<State*> P1new, P2new;
                
                if (current_gs.P2.size() == 0) {
                    // CASE (B): start a fresh epoch
                    P1new.insert(monitor_init);
                    P2new = P1next;  // TODO: is std::move unnecessary?
                } else {
                    // CASE (C): overlapping call
                    P1new = P1next;
                    P1new.insert(monitor_init);
                    removeFinalStates(P1new, F_S);
                    P2new = P2next;
                }

                // Create new state q' and add it predecessor and successor
                BuchiState next_global(q_prime, guess, P1new, P2new);

                // Create new state if not seen before
                if (state_map.contains(next_global) != true) {
                    std::ostringstream ss;
                    //std::cout << "State " << state_counter << " is created." << std::endl;
                    ss << "b_" << state_counter++;
                    State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                    
                    state_map[next_global] = next_state;
                    worklist.push(next_global);
                }

                // Add transition ( (q,j,P₁,P₂) ─a/guess→  (q', guess, P₁new, P₂new) )
                Weight* weight = weight_register.at(guess);
                Edge* new_edge = new Edge(symbol, weight, current_state, state_map[next_global]);
                current_state->addSuccessor(new_edge);
                state_map[next_global]->addPredecessor(new_edge);
                
            }
        }
    }
}
//}

// Assumes:
//      - a NWA AA <A_mas; f; B_1, ... B_k>
//      - finite-word deterministic automata S_ij for each i and j
//      - input alphabet is the same for parent and children
//      - single finVal function for all child automata
// Ensures: Outputs a büchi automaton A' such that L(A') = L(A)
ChildAutomaton* NestedAutomaton::transformToBuchi(value_function_t finVal, weight_t bound) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    // Initialize containers for Büchi automaton
    MapArray<Symbol*>* new_alphabet;
    MapArray<Weight*>* new_weights;
    weight_t global_min, global_max;
    BuchiState init_buchi;
    
    // Helper containers
    MapStd<BuchiState, State*> state_map;
    std::queue<BuchiState> worklist;
    MapStd<weight_t, Weight*> weight_register;
    unsigned int state_counter;
    
    // 1. Compute global return values for all children
    SetStd<weight_t> global_return_values = computeGlobalReturnValues(this, finVal, bound);
    computeGlobalDomains(this, global_min, global_max);

    // 2. Construct all S_ij and collect Q_S and F_S
    MapStd<MonitorKey, ChildAutomaton*> monitors;
    SetStd<State*> Q_S, F_S;
    constructMonitors(this, global_return_values, monitors, Q_S, F_S, finVal, bound);

    // 3. Initialize
    State* init_state = initializeBuchi(this, new_alphabet, new_weights, weight_register, global_return_values, state_map, init_buchi, global_min, global_max, worklist, state_counter);

    // 4. Build the product automaton on-the-fly
    while (worklist.empty() != true)  {
        BuchiState current_gs = worklist.front(); worklist.pop();

        // for each symbol start transition from current
        for (unsigned symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            processBuchiTransition(current_gs, symbol_id, state_map, 
                new_alphabet, new_weights, weight_register, monitors, F_S, state_counter, global_return_values, global_min, global_max, worklist);
        }
    }
    // 5. Create state and accepting state arrays
    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    SetStd<State*>* accepting_states = new SetStd<State*>();

    for (const auto& [global_state, state] : state_map) {
        new_states->insert(state->getId(), state);  // TODO: Check if state->getId is correct

        // Mark as accepting if P2 is empty
        if (global_state.P2.size() == 0) {
            accepting_states->insert(state);
        }
    }
    
    // 6. Construct and return the product automaton
    std::string buchi_name = "Buchi(" + this->getName() + ")";
    ChildAutomaton* buchi = new ChildAutomaton(
        buchi_name, new_alphabet, new_states, 
        new_weights, global_min, global_max, 
        init_state, accepting_states
    );
    
    // 7. Cleanup
    for (const auto& [key, monitor] : monitors) {
        delete monitor;
    }

    return buchi;
}