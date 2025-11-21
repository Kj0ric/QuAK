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
#include <cmath>

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

NestedAutomaton::NestedAutomaton(std::string name,
                                 MapArray<Symbol*>* alphabet,
                                 MapArray<State*>* states,
                                 MapArray<Weight*>* weights,
                                 weight_t min_domain,
                                 weight_t max_domain,
                                 State* initial,
                                 MapArray<ChildAutomaton*>* children)
  : Automaton(name + "_parent", alphabet, states, weights, min_domain, max_domain, initial),
    children_(children) {}

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
            ChildAutomaton* monitor = child->determiniseToS_ij(i, j, finVal, bound);

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

// Helper to print a BuchiState
void printBuchiState(const BuchiState& bs) {
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

                #ifdef DEBUG
                    std::cout << "Current state: ";
                    printBuchiState(current_gs);
                    std::cout << "\nNew state: ";
                    printBuchiState(next_global);
                    std::cout << " with symbol: " << symbol->getName() << ", guess: " << guess << std::endl;
                #endif
                
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

// TODO: Check correctness if completing is indeed necessary
// Make the parent and all children complete by adding a sink state and sink-weight (value 0 by default).
// Parent sink uses parent_sink value; children sinks use child_sink value.
void completeNestedAutomata(NestedAutomaton* nwa, weight_t parent_sink_w = weight_t(0), weight_t child_sink_w = weight_t(0)) {
    if (!nwa) return;

    // Helper lambdas
    auto ensure_sink_for_automaton = [](MapArray<Symbol*>* alphabet,
                                       MapArray<State*>* states,
                                       MapArray<Weight*>* weights,
                                       State*& initial,
                                       weight_t min_domain,
                                       weight_t max_domain,
                                       weight_t sink_value,
                                       const std::string& sink_name_prefix) -> State*
    {
        if (!alphabet || !states || !weights) return nullptr;

        // Create sink weight and insert
        Weight* sink_w = new Weight(sink_value);
        weights->insert(sink_w->getId(), sink_w);

        // Create sink state
        std::ostringstream ss;
        ss << sink_name_prefix << "_sink";
        State* sink = new State(ss.str(), alphabet->size(), min_domain, max_domain);
        states->insert(sink->getId(), sink);

        // For every existing state, ensure a transition on every symbol exists
        for (size_t si = 0; si < states->size(); ++si) {
            State* s = states->at(si);
            if (!s) continue;

            for (size_t a = 0; a < alphabet->size(); ++a) {
                // check successors on symbol a
                SetStd<Edge*>* succs = s->getSuccessors(a);
                bool has = false;
                if (succs) {
                    for (Edge* e : *succs) { (void)e; has = true; break; } // if any edge exists for this symbol treat as present
                }
                if (!has) {
                    Symbol* sym = alphabet->at(a);
                    Edge* e = new Edge(sym, sink_w, s, sink);
                    s->addSuccessor(e);
                    sink->addPredecessor(e);
                }
            }
        }

        // Add self-loop on sink for every symbol
        for (size_t a = 0; a < alphabet->size(); ++a) {
            Symbol* sym = alphabet->at(a);
            Edge* e = new Edge(sym, sink_w, sink, sink);
            sink->addSuccessor(e);
            sink->addPredecessor(e);
        }

        return sink;
    };

    // Parent automaton
    MapArray<Symbol*>* parent_alpha = nwa->getAlphabet();
    MapArray<State*>* parent_states = nwa->getStates();
    MapArray<Weight*>* parent_weights = nwa->getWeights();
    State* parent_init = nwa->getInitial();
    weight_t pmin = 0; //std::numeric_limits<float>::lowest();
    weight_t pmax = 0; //std::numeric_limits<float>::max();
    State* parent_sink = ensure_sink_for_automaton(parent_alpha, parent_states, parent_weights, parent_init, pmin, pmax, parent_sink_w, nwa->getName());

    // Children automata
    for (size_t i = 0; i < nwa->getChildrenSize(); ++i) {
        ChildAutomaton* child = nwa->getChild(i);
        if (!child) continue;

        MapArray<Symbol*>* calpha = child->getAlphabet();
        MapArray<State*>* cstates = child->getStates();
        MapArray<Weight*>* cweights = child->getWeights();
        State* cinit = child->getInitial();
        weight_t cmin = child->getMinDomain();
        weight_t cmax = child->getMaxDomain();

        State* csink = ensure_sink_for_automaton(calpha, cstates, cweights, cinit, cmin, cmax, child_sink_w, child->getName());

        // If child had internal references (initial/finals) they remain pointing to existing states;
        // initial/final pointers do not need update because we kept ids mapping.
    }
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

    generateResolvers(0, 0, 0, resolver, macro_alphabet, automata_list, symbol_list);
    generateMacro(macro_alphabet, automata_list, symbol_list);

    return macro_alphabet;
}

NestedAutomaton* NestedAutomaton::determinizeWithMacroAlphabet(std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual>& macro_alphabet) {
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

            Weight* new_weight = new_weights->at(edge->getWeight()->getId());
            Edge* new_edge = new Edge(new_alphabet->at(macro_id), new_weight, new_states->at(from_state->getId()), new_states->at(to_state->getId()));
            new_states->at(from_state->getId())->addSuccessor(new_edge);
            new_states->at(to_state->getId())->addPredecessor(new_edge);
        }

        // Edges from the children's resolvers
        size_t ai = 1; // skip master at 0
        size_t ci = 1; // skip dummy at 0
        while (ai < macro->getResolver().size() && ci < children_states.size()) {
            const SetStd<Edge*>& edges = macro->getResolver()[ai];
            if (edges.size() == 0) {
                // Empty bucket (dummy child or no chosen edge set for this macro)
                // skip without consuming a child slot
                ++ai;
                continue;
            }

            // Sanity: all child tables exist at ci
            auto* alpha  = children_alphabet[ci];
            auto* wtab   = children_weights[ci];
            auto* states = children_states[ci];

            // Optional asserts (leave enabled in debug):
            // assert(alpha && wtab && states);
            // assert(macro_id < alpha->size());

            for (Edge* e : edges) {
                State* from_src = e->getFrom();
                State* to_src   = e->getTo();

                State* from = states->at(from_src->getId());
                State* to   = states->at(to_src->getId());
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

            SetStd<State*>* copied_finals = new SetStd<State*>();
            for (State* of : *(child->getFinalStates())) {
                copied_finals->insert(children_states[i]->at(of->getId()));
            }

            ChildAutomaton* new_child = new ChildAutomaton(
                child->getName(),
                children_alphabet[i],
                children_states[i],
                children_weights[i],
                child->getMinDomain(),
                child->getMaxDomain(),
                new_init,
                copied_finals
            );

            new_children->insert(i, new_child);
        }
    }

    // Construct and return the new nested automaton
    NestedAutomaton* det_nwa = new NestedAutomaton("PsuedoDet(" + this->getName() + ")", new_alphabet, new_states, new_weights, 0, this->getChildrenSize() - 1, new_initial, new_children);

    // completeNestedAutomata(det_nwa);

    return det_nwa;
}


// After pseudo-determinization, synchronize all children automata wrt silent transitions in the parent.
// TODO: UPDATE AFTER MODIFYING CHILD CLASS: ensure initial states are correctly handled from call sites
NestedAutomaton* NestedAutomaton::synchronizeChildren(std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual>& macro_alphabet) {
    // ---------- accumulator caps ----------
    weight_t X = 2 * this->getStates()->size();
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (child) X = X * child->getStates()->size();
    }
    std::vector<weight_t> maxWeights(this->getChildrenSize(), weight_t(0));
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (child) maxWeights[i] = X * child->getMaxDomain();
    }

    // ---------- build synchronized children on-the-fly ----------
    MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(this->getChildrenSize());

    // Helper: take the single edge (if any) from a SetStd<Edge*>
    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e; // at most one after determinization
        return nullptr;
    };

    for (size_t ci = 0; ci < this->getChildrenSize(); ++ci) {
        Symbol::RESET();
        State::RESET();
        Weight::RESET();

        ChildAutomaton* child = this->getChild(ci);
        if (!child) { new_children->insert(ci, nullptr); continue; }

        // Reuse the encoded alphabet from determinizeWithMacroAlphabet, but with fresh Symbol objects
        MapArray<Symbol*>* src_alpha = child->getAlphabet();
        const size_t A = src_alpha->size();
        std::vector<Symbol*> calpha_vec;
        calpha_vec.reserve(A);
        for (size_t a = 0; a < A; ++a) {
            Symbol* s_src = src_alpha->at(a);
            Symbol* s_new = new Symbol(s_src->getName()); // ID assigned in creation order
            // After RESET and sequential creation, IDs match indices and s_src->getId()
            // (Optional sanity during debugging)
            // assert(s_new->getId() == s_src->getId());
            calpha_vec.push_back(s_new);
        }

        // Materialize fixed-size MapArray with identical indices/IDs
        MapArray<Symbol*>* calpha = new MapArray<Symbol*>(A);
        for (Symbol* s : calpha_vec) {
            calpha->insert(s->getId(), s); // s->getId() == its index 'a'
        }

        // ---- Use std::vector for dynamic sizing during on-the-fly construction ----
        std::vector<State*>  cstates_vec;
        std::vector<Weight*> cweights_vec;
        MapStd<weight_t, Weight*> weight_register;

        // Ensure weight 0 exists
        {
            Weight* w0 = new Weight(weight_t(0));
            cweights_vec.push_back(w0);
            weight_register.insert(weight_t(0), w0);
        }

        SetStd<State*>* cfinals = new SetStd<State*>();

        // Product key (master, child, accumulator)
        struct SyncKey {
            State* m;
            State* s;
            weight_t acc;
            bool operator==(const SyncKey& o) const { return m==o.m && s==o.s && acc==o.acc; }
        };
        struct SyncKeyHash {
            size_t operator()(const SyncKey& k) const {
                size_t h = 1469598103934665603ull;
                auto mix = [&](uint64_t x){ h ^= x; h *= 1099511628211ull; };
                mix((uint64_t)k.m);
                mix((uint64_t)k.s);
                mix((uint64_t)std::hash<float>{}(static_cast<float>(k.acc)));
                return h;
            }
        };

        std::unordered_map<SyncKey, State*, SyncKeyHash> state_map;
        std::queue<SyncKey> worklist;

        auto get_weight = [&](weight_t v) -> Weight* {
            if (weight_register.contains(v) != true) {
                Weight* w = new Weight(v);
                cweights_vec.push_back(w);
                weight_register.insert(v, w);
            }
            return weight_register.at(v);
        };

        auto get_or_make_state = [&](const SyncKey& key) -> State* {
            auto it = state_map.find(key);
            if (it != state_map.end()) return it->second;

            std::ostringstream ss; ss << "sync_" << state_map.size();
            State* ns = new State(ss.str(), A, child->getMinDomain(), child->getMaxDomain());
            cstates_vec.push_back(ns);

            if (child->isFinal(key.s)) cfinals->insert(ns);

            state_map.insert({key, ns});
            worklist.push(key);
            return ns;
        };

        // --- Seed: from all call sites (m, s0, 0) ------------------------------------
        // TODO: CHILD'S INITIAL STATE MAY BE DIFFERENT FROM CALL TARGETS
        // UPDATE AFTER MODIFYING CHILD CLASS: START EXPLORATION FROM EACH CALL SITE WITH THE CORRECT INITIAL STATE
        State* initial_state = nullptr;
        {
            State* s0 = child->getInitial();
            const size_t M = this->getStates()->size();

            for (size_t mi = 0; mi < M; ++mi) {
                State* m = this->getStates()->at(mi);
                bool callable_here = false;
                for (size_t a = 0; a < A && !callable_here; ++a) {
                    Edge* me = first_edge_or_null(m->getSuccessors(a));
                    Edge* se = first_edge_or_null(s0->getSuccessors(a));
                    if (me && se) callable_here = true;
                }
                if (callable_here) {
                    SyncKey k{ m, s0, weight_t(0) };
                    State* seed_state = get_or_make_state(k); // enqueues into worklist
                    if (!initial_state) initial_state = seed_state; // pick the first as the designated initial
                }
            }
            if (!initial_state) {
                SyncKey k{ this->getInitial(), s0, weight_t(0) };
                initial_state = get_or_make_state(k);
            }
        }

        const weight_t accCap = maxWeights[ci];

        // Explore lazily
        while (!worklist.empty()) {
            SyncKey cur = worklist.front(); worklist.pop();
            State* cur_node = state_map.at(cur);

            for (size_t a = 0; a < A; ++a) {
                // Master: after pseudo-determinization, at most one outgoing per symbol
                Edge* me = first_edge_or_null(cur.m->getSuccessors(a));
                if (!me) continue;
                const bool master_silent = (me->getWeight()->getValue() == weight_t(0));
                State* m2 = me->getTo();

                // Child: similarly, at most one outgoing per symbol
                Edge* se = first_edge_or_null(cur.s->getSuccessors(a));
                if (!se) continue;

                State* s2 = se->getTo();
                weight_t ws = se->getWeight()->getValue();

                weight_t emit, acc2;
                if (master_silent) {
                    // emit 0, accumulate child weight
                    emit = weight_t(0);
                    acc2 = cur.acc + ws;
                } else {
                    // flush: emit acc + current, then reset
                    emit = cur.acc + ws;
                    acc2 = weight_t(0);
                }

                SyncKey nxt{ m2, s2, acc2 };
                State* nxt_node = get_or_make_state(nxt);

                Weight* w = get_weight(emit);
                Edge* ne = new Edge(calpha->at(a), w, cur_node, nxt_node);
                cur_node->addSuccessor(ne);
                nxt_node->addPredecessor(ne);
            }
        }

        // ---- Materialize fixed-size MapArray from vectors ----
        size_t state_count  = cstates_vec.size();
        size_t weight_count = cweights_vec.size();

        MapArray<State*>*  cstates  = new MapArray<State*>(state_count);
        MapArray<Weight*>* cweights = new MapArray<Weight*>(weight_count);

        for (State* s : cstates_vec)   cstates->insert(s->getId(), s);
        for (Weight* w : cweights_vec) cweights->insert(w->getId(), w);

        // Build synchronized child (shares alphabet with the determinized child)
        std::string cname = child->getName() + "_sync";
        ChildAutomaton* synced = new ChildAutomaton(
            cname, calpha, cstates, cweights,
            child->getMinDomain(), child->getMaxDomain(),
            initial_state, cfinals
        );
        new_children->insert(ci, synced);
    }

    // Return a fresh NWA with the same parent and synchronized children
    NestedAutomaton* result = new NestedAutomaton(this, new_children);
    result->setName("Sync(" + this->getName() + ")");
    return result;
}

// assuming the input NWA is pseudo-deterministic and children are synchronized
Automaton* NestedAutomaton::flatten() {
    using std::size_t;

    // ---------- helper: single outgoing edge after determinization ----------
    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e; // at most one after determinization
        return nullptr;
    };

    // ---------- cache children and basic metadata ----------
    const size_t C = this->getChildrenSize();
    std::vector<ChildAutomaton*> children(C, nullptr);
    for (size_t ci = 0; ci < C; ++ci) {
        children[ci] = this->getChild(ci);
    }

    // |U| = total #states over all (synchronized) children
    size_t U = 0;
    // W_abs = largest absolute weight in U, as weight_t
    weight_t W_abs = weight_t(0);

    auto weight_zero = []() { return weight_t(0); };
    auto weight_two  = []() { return weight_t(2); };

    auto weight_abs = [&](weight_t v) -> weight_t {
        // Non-positive weights (Sum-) ⇒ |v| = (v < 0 ? -v : v)
        if (v < weight_zero()) return -v;
        return v;
    };

    for (size_t ci = 0; ci < C; ++ci) {
        ChildAutomaton* child = children[ci];
        if (!child) continue;

        MapArray<State*>* cstates = child->getStates();
        U += cstates->size();

        MapArray<Weight*>* cws = child->getWeights();
        for (size_t wid = 0; wid < cws->size(); ++wid) {
            weight_t wv = cws->at(wid)->getValue();
            weight_t mag = weight_abs(wv);
            if (mag > W_abs) W_abs = mag;
        }
    }

    // ---------- X, Y, Z with overflow-safe integer arithmetic on the combinatorial part ----------
    const size_t M_states = this->getStates()->size();

    auto sat_mul = [](size_t a, size_t b) -> size_t {
        if (a == 0 || b == 0) return 0;
        const size_t maxv = std::numeric_limits<size_t>::max();
        if (a > maxv / b) return maxv;
        return a * b;
    };

    auto sat_pow = [&](size_t base, size_t exp) -> size_t {
        if (exp == 0) return 1;
        const size_t maxv = std::numeric_limits<size_t>::max();
        size_t res = 1;
        while (exp > 0) {
            if (base != 0 && res > maxv / base) return maxv;
            res *= base;
            if (res == maxv) return maxv;
            --exp;
        }
        return res;
    };

    // X = 2 * |M| * Π_i |S_i|
    size_t X_states = 2;
    X_states = sat_mul(X_states, M_states);
    for (size_t ci = 0; ci < C; ++ci) {
        ChildAutomaton* child = children[ci];
        if (!child) continue;
        X_states = sat_mul(X_states, child->getStates()->size());
    }

    // Y = X * (|U| + 2) * |U|^{2|U|}
    size_t exp   = sat_mul(2, U);          // 2|U|
    size_t U_pow = sat_pow(U, exp);        // |U|^{2|U|} (saturating)

    size_t Y = sat_mul(X_states, U + 2);
    Y = sat_mul(Y, U_pow);

    // Z = 2 * X * (|U| + 2) * |U|^{2|U|} * W_abs  (all in weight_t)
    weight_t Z = weight_zero();
    if (W_abs > weight_zero()) {
        // Conversions size_t -> weight_t must be supported (they are used elsewhere in QuAK)
        weight_t WX = weight_two() * weight_t(X_states);
        WX = WX * weight_t(U + 2);
        WX = WX * weight_t(U_pow);
        Z  = WX * W_abs;
    }

    // ---------- flattened alphabet: copy from NWA ----------
    Symbol::RESET();
    MapArray<Symbol*>* src_alpha = this->getAlphabet();
    const size_t A = src_alpha->size();

    MapArray<Symbol*>* falpha = new MapArray<Symbol*>(A);
    for (size_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(src_alpha->at(a)->getName());
        falpha->insert(s_new->getId(), s_new); // id == index
    }

    // ---------- flat states and weights ----------
    State::RESET();
    Weight::RESET();

    std::vector<State*>  fstates_vec;
    std::vector<Weight*> fweights_vec;
    fstates_vec.reserve(64);
    fweights_vec.reserve(16);

    MapStd<weight_t, Weight*> weight_register;

    weight_t flat_min = weight_zero();
    weight_t flat_max = weight_zero();
    bool flat_has_weight = false;

    auto get_weight = [&](weight_t v) -> Weight* {
        if (weight_register.contains(v) != true) {
            Weight* w = new Weight(v);
            fweights_vec.push_back(w);
            weight_register.insert(v, w);

            if (!flat_has_weight) {
                flat_min = flat_max = v;
                flat_has_weight = true;
            } else {
                if (v < flat_min) flat_min = v;
                if (v > flat_max) flat_max = v;
            }
        }
        return weight_register.at(v);
    };

    // ---------- encoding of flatten states ----------
    struct BoundedInst {
        size_t   child_index;
        State*   state;
        weight_t budget;  // remaining |weight|-budget ∈ [0, Z]
    };

    using UInst = std::pair<size_t, State*>; // (child_index, child_state)

    struct FlatKey {
        State*                   master;
        std::vector<UInst>       unbounded;
        std::vector<BoundedInst> bounded;

        bool operator==(const FlatKey& o) const {
            if (master != o.master) return false;
            if (unbounded.size() != o.unbounded.size()) return false;
            if (bounded.size()   != o.bounded.size())   return false;

            for (size_t i = 0; i < unbounded.size(); ++i) {
                if (unbounded[i].first  != o.unbounded[i].first)  return false;
                if (unbounded[i].second != o.unbounded[i].second) return false;
            }
            for (size_t i = 0; i < bounded.size(); ++i) {
                const BoundedInst& b1 = bounded[i];
                const BoundedInst& b2 = o.bounded[i];
                if (b1.child_index != b2.child_index) return false;
                if (b1.state       != b2.state)       return false;
                if (b1.budget      != b2.budget)      return false;
            }
            return true;
        }
    };

    struct FlatKeyHash {
        size_t operator()(FlatKey const& k) const {
            size_t h = 1469598103934665603ull;
            auto mix = [&](uint64_t x) {
                h ^= x;
                h *= 1099511628211ull;
            };
            mix(reinterpret_cast<uint64_t>(k.master));
            for (auto const& u : k.unbounded) {
                mix(static_cast<uint64_t>(u.first));
                mix(reinterpret_cast<uint64_t>(u.second));
            }
            for (auto const& b : k.bounded) {
                mix(static_cast<uint64_t>(b.child_index));
                mix(reinterpret_cast<uint64_t>(b.state));
                // we deliberately ignore budget in the hash to avoid depending on hash<weight_t>
            }
            return h;
        }
    };

    auto normalize_key = [](FlatKey& k) {
        auto cmpU = [](const UInst& a, const UInst& b) {
            if (a.first != b.first) return a.first < b.first;
            return a.second->getId() < b.second->getId();
        };
        std::sort(k.unbounded.begin(), k.unbounded.end(), cmpU);

        auto cmpB = [](const BoundedInst& a, const BoundedInst& b) {
            if (a.child_index != b.child_index) return a.child_index < b.child_index;
            int ida = a.state->getId();
            int idb = b.state->getId();
            if (ida != idb) return ida < idb;
            if (a.budget < b.budget) return true;
            if (a.budget > b.budget) return false;
            return false;
        };
        std::sort(k.bounded.begin(), k.bounded.end(), cmpB);
    };

    std::unordered_map<FlatKey, State*, FlatKeyHash> state_map;
    std::queue<FlatKey> worklist;

    auto get_or_make_state = [&](FlatKey key) -> State* {
        normalize_key(key);
        auto it = state_map.find(key);
        if (it != state_map.end()) return it->second;

        std::ostringstream ss;
        ss << "flat_" << state_map.size();
        State* ns = new State(ss.str(), A, 0, 0); // no children: domain [0,0]
        fstates_vec.push_back(ns);

        state_map.insert(std::make_pair(key, ns));
        worklist.push(key);
        return ns;
    };

    // ---------- initial flat state: (master_initial, no slaves) ----------
    FlatKey initKey;
    initKey.master = this->getInitial();
    State* flat_initial = get_or_make_state(initKey);

    // ---------- BFS over flatten states ----------
    while (!worklist.empty()) {
        FlatKey key = worklist.front();
        worklist.pop();

        State* from_flat = state_map.at(key);

        for (size_t a = 0; a < A; ++a) {
            // master step
            Edge* me = first_edge_or_null(key.master->getSuccessors(a));
            if (!me) continue;
            State*   m2 = me->getTo();
            weight_t wm = me->getWeight()->getValue(); // ≤ 0 under Sum-

            bool ok = true;

            std::vector<UInst>       next_unbounded;
            std::vector<BoundedInst> next_bounded;
            next_unbounded.reserve(key.unbounded.size());
            next_bounded.reserve(key.bounded.size());

            weight_t sum_unbounded = weight_zero();
            weight_t sum_bounded   = weight_zero();

            // --- existing unbounded instances ---
            for (const UInst& u : key.unbounded) {
                const size_t ci    = u.first;
                State* const s_cur = u.second;

                ChildAutomaton* child = children[ci];
                if (!child) { ok = false; break; }

                Edge* se = first_edge_or_null(s_cur->getSuccessors(a));
                if (!se) { ok = false; break; }

                State*   s2 = se->getTo();
                weight_t xu = se->getWeight()->getValue(); // ≤ 0

                sum_unbounded += xu;
                next_unbounded.emplace_back(ci, s2);
            }
            if (!ok) continue;

            // --- existing bounded instances ---
            for (const BoundedInst& b : key.bounded) {
                const size_t ci    = b.child_index;
                State* const s_cur = b.state;
                weight_t     bud   = b.budget; // ≥ 0

                ChildAutomaton* child = children[ci];
                if (!child) { ok = false; break; }

                Edge* se = first_edge_or_null(s_cur->getSuccessors(a));
                if (!se) { ok = false; break; }

                State*   s2 = se->getTo();
                weight_t z  = se->getWeight()->getValue();     // ≤ 0
                weight_t mag_z = weight_abs(z);                // |z| ≥ 0

                // strictly decreasing absolute budget
                if (bud < mag_z) { ok = false; break; }
                weight_t bud2 = bud - mag_z;

                sum_bounded += z; // actual contribution is still z (≤ 0)

                // if budget is exhausted and child is in final, drop this instance
                if (bud2 == weight_zero() && children[ci]->isFinal(s2)) {
                    continue;
                }

                BoundedInst nb;
                nb.child_index = ci;
                nb.state       = s2;
                nb.budget      = bud2;
                next_bounded.push_back(nb);
            }
            if (!ok) continue;

            // -------- (i) no new instantiation --------
            {
                FlatKey k2;
                k2.master    = m2;
                k2.unbounded = next_unbounded;
                k2.bounded   = next_bounded;

                State* to_flat = get_or_make_state(k2);
                weight_t x = wm + sum_unbounded + sum_bounded; // Sum- ⇒ x ≤ 0

                Edge* e = new Edge(falpha->at(a), get_weight(x), from_flat, to_flat);
                from_flat->addSuccessor(e);
                to_flat->addPredecessor(e);
            }

            // -------- (ii) spawn unbounded instance --------
            if (Y > 0 && next_unbounded.size() < Y && U > 0) {
                for (size_t ci = 0; ci < C; ++ci) {
                    ChildAutomaton* child = children[ci];
                    if (!child) continue;

                    State* s0 = child->getInitial();
                    Edge*  se0 = first_edge_or_null(s0->getSuccessors(a));
                    if (!se0) continue;

                    State*   s1    = se0->getTo();
                    weight_t x_new = se0->getWeight()->getValue(); // ≤ 0

                    FlatKey k2;
                    k2.master    = m2;
                    k2.unbounded = next_unbounded;
                    k2.bounded   = next_bounded;
                    k2.unbounded.emplace_back(ci, s1);

                    State* to_flat = get_or_make_state(k2);
                    weight_t x = wm + sum_unbounded + sum_bounded + x_new;

                    Edge* e = new Edge(falpha->at(a), get_weight(x), from_flat, to_flat);
                    from_flat->addSuccessor(e);
                    to_flat->addPredecessor(e);
                }
            }

            // -------- (iii) spawn bounded instance --------
            if (U > 0 && Z > weight_zero() && next_bounded.size() < U) {
                for (size_t ci = 0; ci < C; ++ci) {
                    ChildAutomaton* child = children[ci];
                    if (!child) continue;

                    State* s0 = child->getInitial();
                    Edge*  se0 = first_edge_or_null(s0->getSuccessors(a));
                    if (!se0) continue;

                    State*   s1 = se0->getTo();
                    weight_t z  = se0->getWeight()->getValue();   // ≤ 0
                    weight_t mag_z = weight_abs(z);
                    if (mag_z > Z) continue;

                    weight_t bud2 = Z - mag_z;

                    FlatKey k2;
                    k2.master    = m2;
                    k2.unbounded = next_unbounded;
                    k2.bounded   = next_bounded;

                    BoundedInst nb;
                    nb.child_index = ci;
                    nb.state       = s1;
                    nb.budget      = bud2;
                    k2.bounded.push_back(nb);

                    State* to_flat = get_or_make_state(k2);
                    weight_t x = wm + sum_unbounded + sum_bounded + z; // ≤ 0

                    Edge* e = new Edge(falpha->at(a), get_weight(x), from_flat, to_flat);
                    from_flat->addSuccessor(e);
                    to_flat->addPredecessor(e);
                }
            }
        }
    }

    // ---------- materialize states / weights ----------
    const size_t state_count  = fstates_vec.size();
    const size_t weight_count = fweights_vec.size();

    MapArray<State*>*  fstates  = new MapArray<State*>(state_count);
    MapArray<Weight*>* fweights = new MapArray<Weight*>(weight_count);

    for (State* s : fstates_vec)   fstates->insert(s->getId(), s);
    for (Weight* w : fweights_vec) fweights->insert(w->getId(), w);

    if (!flat_has_weight) {
        flat_min = flat_max = weight_zero();
    }

    // ---------- select final states (logic only, wiring into Automaton left to you) ----------
    // Final iff master is final and there are no active slave instances.
    SetStd<State*>* flat_finals = new SetStd<State*>();
    for (const auto& kv : state_map) {
        const FlatKey& k = kv.first;
        State* s = kv.second;
        if (k.unbounded.empty() && k.bounded.empty()) {
            flat_finals->insert(s);
        }
    }
    (void)flat_finals; // TODO: wire into acceptance once Automaton/NestedAutomaton exposes it

    // ---------- build and return a childless NestedAutomaton as Automaton* ----------
    std::string fname = "Flat(" + this->getName() + ")";
    MapArray<ChildAutomaton*>* no_children = new MapArray<ChildAutomaton*>(0);

    NestedAutomaton* flatNA = new NestedAutomaton(
        fname,
        falpha,
        fstates,
        fweights,
        flat_min,
        flat_max,
        flat_initial,
        no_children
    );

    return static_cast<Automaton*>(flatNA);
}
