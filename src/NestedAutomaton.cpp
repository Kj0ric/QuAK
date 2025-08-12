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

//namespace {
    const weight_t INIT_VALUE = 0;          // Does not matter what it is

    // Define global state for Büchi automaton
    struct GlobalState {
        State* master;
        weight_t last_guess;
        SetStd<State*> P1, P2;
        // bool operator==
    };

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

// Helper: Compute all possible return values for a single child automaton
SetStd<weight_t> computeChildReturnValues(ChildAutomaton* child, value_function_t finVal, weight_t bound = -1) {
    SetStd<weight_t> return_values;
    
    if (finVal == Min_f || finVal == Max_f) {
        // For MIN/MAX: return values are exactly the weights in the automaton
        for (size_t w = 0; w < child->getWeights()->size(); ++w) {
            return_values.insert(child->getWeights()->at(w)->getValue());
        }
    }
    else if (finVal == SumB) {
        // For SUM_bounded: need BFS to compute all reachable sums
        if (bound < 0) {
            QUAK_FAIL("SumB requires a non-negative bound");
        }
        
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
                    weight_t next_bound_hit = bound_hit;  // Inherit bound hit status
                    
                    if (raw_sum > bound && bound_hit == weight_t(0)) {
                        // First time hitting upper bound
                        next_sum = bound;
                        next_bound_hit = bound;  // Remember we hit +bound
                    } else if (raw_sum < -bound && bound_hit == weight_t(0)) {
                        // First time hitting lower bound  
                        next_sum = -bound;
                        next_bound_hit = -bound;  // Remember we hit -bound
                    } else if (bound_hit != weight_t(0)) {
                        // Already exceeded bounds before -> continue with bounded value
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
    }
    else {
        QUAK_FAIL("Unsupported value function for child automaton");
    }
    return return_values;
}

    // Compute the global set of all possible return values across all children
    SetStd<weight_t> computeGlobalReturnValues(const NestedAutomaton* nwa, value_function_t finVal, weight_t bound = -1) {
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
        
        return global_values;
    }

    // Construct all S_ij (monitors) and collect Q_S and F_S
    void constructMonitors(
        const NestedAutomaton* nwa,
        const SetStd<weight_t>& global_return_values,
        std::vector<std::vector<ChildAutomaton*>>& monitors,
        SetStd<State*>& Q_S,
        SetStd<State*>& F_S,
        value_function_t finVal
    ) {
        size_t k = nwa->getChildrenSize();
        monitors.resize(k);

        for (size_t i = 0; i < k; ++i) {
            ChildAutomaton* child = nwa->getChild(i);
            if (child == nullptr) continue;

            // Create monitors for possible return values

            for (weight_t j : global_return_values) {
                ChildAutomaton* monitor = child->determiniseToS_ij(j, finVal);
                monitors[i].push_back(monitor);
                
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
    #if 0
    // Helper: Initialize büchi automaton components
    void initializeBuchiAutomaton(
        const NestedAutomaton* nwa,
        MapArray<Symbol*>*& new_alphabet,
        MapArray<Weight*>*& new_weights,
        MapStd<GlobalState, State*> state_map,
        State*& initial_state,
        weight_t global_min,
        weight_t global_max,
        std::queue<GlobalState>& worklist,
        unsigned int& state_counter
    ) {
        // Copy alphabet from master
        size_t alph_size = nwa->getAlphabetSize();
        new_alphabet = new MapArray<Symbol*>(alph_size);
        for (size_t i = 0; i < alph_size; ++i) {
            Symbol* original = nwa->getAlphabet()->at(i);
            Symbol* copy = new Symbol(original->getName());
            new_alphabet->insert(i, copy);
        }

        // Create weights: silent + all possible guesses

        
        // Create initial Büchi state
        GlobalState init;
        init.master = nwa->getInitial();
        init.last_guess = weight_t(INIT_VALUE); 
        init.P1 = SetStd<State*>();
        init.P2 = SetStd<State*>();

        std::ostringstream ss;
        ss << "q_" << state_counter++;
        initial_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);


        // Map the state to the tuple
        state_map[init] = initial_state;
        worklist.push(init);
    }

    void processBuchiTransition(
        const GlobalState& current_gs,
        unsigned int symbol_id,
        MapStd<GlobalState, State*> state_map,
        MapArray<Symbol*>* new_alphabet,
        MapArray<Weight*>* new_weights,
        const SetStd<State*>& F_S,
        unsigned int& state_counter,
        weight_t global_min,
        weight_t global_max,
        std::queue<GlobalState>& worklist


        // TODO:

    ) {
        Symbol* symbol = new_alphabet->at(symbol_id);
        State* currrent_state = state_map[current_gs];

        for (Edge* master_edge : *(current_gs.master)->getSuccessors(symbol_id)) {
            State* q_prime = master_edge->getTo();

            // Advance all active monitors
            SetStd<State*> P1next = stepMonitors(current_gs.P1, symbol, F_S);
            SetStd<State*> P2next = stepMonitors(current_gs.P2, symbol, F_S);

            // TODO: Implement SILENT constant
            bool is_silent = (master_edge->getWeight()->getValue() == SILENT);

            if (is_silent) {
                // Case A: Silent transition
                // Create new GS
                GlobalState next_global;
                next_global.master = q_prime;
                next_global.last_guess = SILENT_WEIGHT;
                next_global.P1 = P1next;
                next_global.P2 = P2next;

                // Create new state if not seen before
                if (state_map.contains(next_global) != true) {
                    std::ostringstream ss;
                    ss << "q_" << state_counter++;
                    State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                    state_map[next_global] = next_state;
                    worklist.push(next_global);
                }

                // Add edge with SILENT weight
                Weight* weight = SILENT_WEIGHT;



            }
            else {
                // Case B and C
            }


        }
    }
//}

// Assumes:
//      - a NWA AA <A_mas; f; B_1, ... B_k>
//      - finite-word deterministic automata S_ij for each i and j
//      - input alphabet is the same for parent and children
// Ensures: Outputs a büchi automaton A' such that L(A') = L(A)
Automaton* NestedAutomaton::transformToBuchi(value_function_t finVal, weight_t bound) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    // Initialize containers for the G.Büchi automaton
    MapArray<Symbol*>* new_alphabet;
    MapArray<Weight*>* new_weights;
    MapArray<State*>* new_states;
    weight_t global_min, global_max;
    GlobalState* init;
    
    // Helper containers
    MapStd<GlobalState, State *> state_map;
    std::queue<GlobalState> worklist;
    unsigned int state_counter;
    
    // 1. Compute global min/max domain for all children
    SetStd<weight_t> global_return_values = computeGlobalReturnValues(this, finVal, bound);

    // 2. Construct all S_ij and collect Q_S and F_S
    std::vector<std::vector<ChildAutomaton*>> monitors;
    SetStd<State*> Q_S, F_S;
    constructMonitors(this, global_return_values, monitors, Q_S, F_S, finVal);

    // 3. Initialize
    initializeBuchiAutomaton(this, new_alphabet, new_weights, state_map, initial, global_min, global_max, worklist, state_counter);

    // 4. Build the product automaton on-the-fly
    while (worklist.empty() != true)  {
        GlobalState current_gs = worklist.front(); worklist.pop();

        // for each symbol start transition from current
        for (unsigned symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            processBuchiTransition(
                // TODO:
            );
        }
    
    }

    // Construct and return the product automaton
    std::string buchi_name = "Buchi(" + this->getName() + ")";
    Automaton* buchi = new Automaton(
        // TODO:
    );

    return buchi;
}// Add this to src/functionality_tests.cpp

void testConstructMonitors(const std::string& filepath, value_function_t finVal, weight_t bound = -1) {
    std::cout << "=== Testing constructMonitors Function ===" << std::endl;
    std::cout << "File: " << filepath << std::endl;
    std::cout << "Value function: " << (finVal == Min_f ? "Min_f" : 
                                      finVal == Max_f ? "Max_f" : 
                                      finVal == SumB ? "SumB" : "Unknown") << std::endl;
    if (finVal == SumB) std::cout << "Bound: " << bound << std::endl;
    
    NestedAutomaton* nested = new NestedAutomaton(filepath);
    
    if (nested->getChildrenSize() == 0) {
        std::cout << "No child automata found!" << std::endl;
        delete nested;
        return;
    }
    
    std::cout << "Number of children: " << nested->getChildrenSize() << std::endl;
    
    // Step 1: Compute global return values
    SetStd<weight_t> global_return_values = computeGlobalReturnValues(nested, finVal, bound);
    
    std::cout << "\nGlobal return values: {";
    bool first = true;
    for (weight_t val : global_return_values) {
        if (!first) std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "}" << std::endl;
    std::cout << "Total unique return values: " << global_return_values.size() << std::endl;
    
    // Step 2: Test constructMonitors
    std::vector<std::vector<ChildAutomaton*>> monitors;
    SetStd<State*> Q_S, F_S;
    
    std::cout << "\n--- Calling constructMonitors ---" << std::endl;
    constructMonitors(nested, global_return_values, monitors, Q_S, F_S, finVal);
    
    // Step 3: Verify results
    std::cout << "\n--- Monitor Construction Results ---" << std::endl;
    std::cout << "monitors.size() = " << monitors.size() << " (should equal " << nested->getChildrenSize() << ")" << std::endl;
    
    size_t total_monitors = 0;
    for (size_t i = 0; i < monitors.size(); ++i) {
        std::cout << "Child " << i << " has " << monitors[i].size() << " monitors (should equal " 
                  << global_return_values.size() << ")" << std::endl;
        total_monitors += monitors[i].size();
        
        // Test each monitor for this child
        for (size_t j = 0; j < monitors[i].size(); ++j) {
            ChildAutomaton* monitor = monitors[i][j];
            if (monitor) {
                std::cout << "  Monitor[" << i << "][" << j << "]: " 
                          << monitor->getStates()->size() << " states, "
                          << monitor->getFinalStates()->size() << " final states" << std::endl;
                          
                // Verify monitor is deterministic (requirement for S_ij)
                if (!monitor->isDeterministic()) {
                    std::cout << "    WARNING: Monitor is not deterministic!" << std::endl;
                }
                
                // Check if monitor has reasonable structure
                if (monitor->getStates()->size() == 0) {
                    std::cout << "    WARNING: Monitor has no states!" << std::endl;
                }
            } else {
                std::cout << "  Monitor[" << i << "][" << j << "]: NULL pointer!" << std::endl;
            }
        }
    }
    
    std::cout << "\nTotal monitors created: " << total_monitors << std::endl;
    std::cout << "Expected total: " << nested->getChildrenSize() * global_return_values.size() << std::endl;
    
    // Step 4: Test Q_S and F_S collections
    std::cout << "\n--- Global State Collections ---" << std::endl;
    std::cout << "|Q_S| = " << Q_S.size() << " (global monitor states)" << std::endl;
    std::cout << "|F_S| = " << F_S.size() << " (global accepting states)" << std::endl;
    
    // Verify F_S is subset of Q_S
    bool f_s_subset_q_s = true;
    for (State* s : F_S) {
        if (!Q_S.contains(s)) {
            f_s_subset_q_s = false;
            break;
        }
    }
    std::cout << "F_S ⊆ Q_S: " << (f_s_subset_q_s ? "✓" : "✗") << std::endl;
    
    // Step 5: Test state disjointness (states should have unique names)
    std::cout << "\n--- Testing State Disjointness ---" << std::endl;
    SetStd<std::string> state_names;
    bool disjoint = true;
    
    for (State* s : Q_S) {
        if (state_names.contains(s->getName())) {
            std::cout << "WARNING: Duplicate state name found: " << s->getName() << std::endl;
            disjoint = false;
        } else {
            state_names.insert(s->getName());
        }
    }
    std::cout << "State names are unique: " << (disjoint ? "✓" : "✗") << std::endl;
    
    // Step 6: Sample monitor details (show first few)
    std::cout << "\n--- Sample Monitor Details ---" << std::endl;
    size_t samples_shown = 0;
    const size_t max_samples = 3;
    
    for (size_t i = 0; i < monitors.size() && samples_shown < max_samples; ++i) {
        if (monitors[i].size() > 0 && monitors[i][0] != nullptr) {
            ChildAutomaton* sample_monitor = monitors[i][0];
            std::cout << "Sample Monitor[" << i << "][0]:" << std::endl;
            std::cout << "  Name: " << sample_monitor->getName() << std::endl;
            std::cout << "  Alphabet size: " << sample_monitor->getAlphabetSize() << std::endl;
            std::cout << "  States: " << sample_monitor->getStates()->size() << std::endl;
            std::cout << "  Final states: " << sample_monitor->getFinalStates()->size() << std::endl;
            std::cout << "  Deterministic: " << (sample_monitor->isDeterministic() ? "Yes" : "No") << std::endl;
            samples_shown++;
        }
    }
    
    // Step 7: Cleanup verification
    std::cout << "\n--- Memory Management Test ---" << std::endl;
    std::cout << "Cleaning up " << total_monitors << " monitors..." << std::endl;
    
    // Clean up monitors
    for (size_t i = 0; i < monitors.size(); ++i) {
        for (size_t j = 0; j < monitors[i].size(); ++j) {
            delete monitors[i][j];
        }
    }
    std::cout << "Monitor cleanup completed." << std::endl;
    
    delete nested;
    std::cout << "\n=== constructMonitors Test Completed ===" << std::endl;
}
#endif