#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <cassert>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <map>
#include <queue>
#include <sstream>
#include <unordered_map>

#include "Automaton.h"
#include "NestedAutomaton.h"
#include "ChildAutomaton.h"
#include "Map.h"
#include "Parser.h"
#include "Edge.h"
#include "Set.h"
#include "State.h"
#include "Weight.h"
#include "utility.h"
#include "FORKLIFT/inclusion.h"
#include "dfa_minimal_check.h"

/* ----------------------------------------------- ChildAutomaton ------------------------------------------------------ */
inline SetStd<State*> getStatesByNames(MapArray<State*>* states, const SetStd<std::string>& final_state_names);

ChildAutomaton::~ChildAutomaton () {
    delete final_states_;
    // Automaton destructor is called automatically
}

/* ------- CONSTRUCTORS -------- */
ChildAutomaton::ChildAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register) 
    : Automaton(name, parser, sync_register){

	// If not dummy child then Initialize final_states_ from parser->final_states
    final_states_ = new SetStd<State*>(getStatesByNames(states, parser->final_states));
}

ChildAutomaton::ChildAutomaton(
    std::string name,
    MapArray<Symbol*>* alphabet,
    MapArray<State*>* states,
    MapArray<Weight*>* weights,
    weight_t min_domain,
    weight_t max_domain,
    State* initial,
    SetStd<State*>* final_states
 ) : Automaton(name, alphabet, states, weights, min_domain, max_domain, initial), 
    final_states_(final_states)
 {}

// Copy constructor
ChildAutomaton::ChildAutomaton(const ChildAutomaton& other)
    : Automaton(other), // call base CC
     final_states_(new SetStd<State*>(*other.final_states_)) {}

/* --------------------------- B_i DETERMINIZATION --------------------------- */
//using Subset = SetStd<State*>;
//using Pair = std::pair<Subset, weight_t>;
using StateVector = std::vector<weight_t>;

// Hash function for StateVector TODO: Check this
struct StateVectorHash {
    // Define a custom hash functor. This allows the use of struct as a function
    std::size_t operator()(const StateVector& vec) const {
        std::size_t seed = vec.size();
        for (auto& val : vec) {
            seed ^= std::hash<float>()(val.to_float()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

// Helper to print the state vector for debugging
void printVector(const StateVector& vec) {
    std::cout << "<";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) std::cout << ", ";
        if (vec[i] == weight_t(std::numeric_limits<float>::lowest())) {
            std::cout << "-inf";
        } else if (vec[i] == weight_t(std::numeric_limits<float>::max())){
            std::cout << "+inf";
        } else {
            std::cout << vec[i];
        }
    }
    std::cout << ">";
}

// Solve non-determinism in S_ij transition weights with MAX value function
weight_t aggregateWeights(MapStd<State*, std::vector<weight_t>> *weightMap) {
    weight_t result = std::numeric_limits<float>::lowest();
    for (auto& [t, weights] : *weightMap) { // MapStd stores key,value as pairs under the hood
        for (weight_t w : weights) {
            result = std::max(result, w);
        }
    }
	return result;
}

// Determines the new accumulated weight value for the next state of S_ij
weight_t transitionFunction(weight_t state_value, weight_t transit_value, value_function_t finVal, weight_t bound) {
    weight_t result;
    if (finVal == Max_f) {
        result = std::max(state_value, transit_value);  
    } else if (finVal == Min_f) {
        result = std::min(state_value, transit_value);
    } else if (finVal == SumB) {
        // Handle bound exceeded cases
        if (state_value == bound + 1 || state_value == -bound - 1) {
            return state_value; // Once exceeded stay exceeded
        }

        result = state_value + transit_value;

        // Check bounds with placeholder values
        if (result > bound) {
            result = bound + 1;     // Means positive bound exceeded
        } else if (result < -bound) {
            result = -bound - 1;    // Means negative bound exceeded
        }
    } else {
        QUAK_FAIL("transitionFunction: Non-regular value function for child automaton B_i\n");
    }
	return result;
}

// Helper: Initialize S_ij
void initializeDFA(
	MapArray<Symbol*>*& dfa_alphabet, 
	MapArray<Weight*>*& dfa_weights, 
	MapStd<StateVector, State*>& state_map_DFA,
	std::queue<StateVector>& worklist, 
	State* &initial_dfa, 
	unsigned int& state_counter, 
	const ChildAutomaton* B_i, 
    value_function_t finVal
) {
    dfa_alphabet = new MapArray<Symbol*>(B_i->getAlphabet()->size());
    for (size_t i = 0; i < B_i->getAlphabet()->size(); ++i) {
        Symbol* orig = B_i->getAlphabet()->at(i);
        dfa_alphabet->insert(i, new Symbol(*orig));
    }

    // Initialize weights
	dfa_weights = new MapArray<Weight*>(2);
	dfa_weights->insert(0, new Weight(weight_t(0)));
	dfa_weights->insert(1, new Weight(weight_t(1)));

    // Create initial vector: 0 for initial state of B_i -\inf for other states
    StateVector initial_vector(B_i->getStates()->size());
    weight_t initial_value, unreachable_value;

    if (finVal == Min_f) {
        initial_value = weight_t(std::numeric_limits<float>::max());        // +\inf
        unreachable_value = weight_t(std::numeric_limits<float>::max());    // +\inf
    } else if (finVal == Max_f) {
        initial_value = weight_t(std::numeric_limits<float>::lowest());     // -\inf
        unreachable_value = weight_t(std::numeric_limits<float>::lowest()); // -\inf
    } else if (finVal == SumB) {
        initial_value = weight_t(0);                                        // 0
        unreachable_value = weight_t(std::numeric_limits<float>::lowest()); // -\inf
    } else {
        QUAK_FAIL("Unsupported value function in initializeDFA");
    }
    
    for (size_t k = 0; k < initial_vector.size(); ++k) {
        if (k == B_i->getInitial()->getId()) {
            initial_vector[k] = initial_value;
        } else {
            initial_vector[k] = unreachable_value;
        }
    }

    // Create initial DFA state
    std::ostringstream ss;
    ss << "d_" << state_counter++;
    initial_dfa = new State(ss.str(), dfa_alphabet->size(), 0, 1);
    
    state_map_DFA.insert(initial_vector, initial_dfa);
    worklist.push(initial_vector);

    #ifdef DEBUG
        std::cout << "Initial DFA state: " << ss.str() << " with vector: <";
        for (weight_t v : initial_vector){
            std::cout << v << ", ";
        } 
        std::cout << ">" << std::endl;
    #endif
}

bool hasFinalIntersection(const SetStd<State*>& subset, const SetStd<State*>* finals) {
    for (State* s : subset) {
        if (finals->contains(s)) return true;
    }
    return false;
}

// Helper: Check if a DFA state is accepting
bool isAcceptingVector(
    const StateVector& vector,
    weight_t j,
    const SetStd<State*>* finals,
    const ChildAutomaton* B_i,
    weight_t bound
) {
    // Check if any final state component has value j or (B+1/-B-1 for bound case)
    for (State* final_state : * finals) {
        size_t k = final_state->getId();
        weight_t component_value = vector[k];

        if (component_value == j) {
            return true;
        }

        // For SumB: also check bound exceeded cases
        if (j == bound && (component_value == bound + 1 || component_value == -bound - 1)) {
            return true;
        }
    }
    return false;
}

// Helper: Process a single transition for the subset construction
void processTransition(
	const StateVector& current_vector,
	unsigned symbol_id,
	MapArray<Symbol*>* dfa_alphabet,
	MapArray<Weight*>* dfa_weights,
	MapStd<StateVector, State*>& state_map_DFA, 
    std::queue<StateVector>& worklist,
    unsigned int& state_counter,
	weight_t j,
    value_function_t finVal,
    weight_t bound,
    const SetStd<State*>* finals,
    const ChildAutomaton* B_i
) {
	
	State* from_state = state_map_DFA[current_vector];
	Symbol* symbol = dfa_alphabet->at(symbol_id);

    #ifdef DEBUG
        std::cout << "\n--- Processing transition from state " << from_state->getName() 
                << " with symbol " << symbol->getName() << " ---" << std::endl;
        std::cout << "Current vector: ";
        printVector(current_vector);
        std::cout << std::endl;
    #endif

    // Compute next vector
    StateVector next_vector(B_i->getStates()->size());
    weight_t unreachable_value;

    if (finVal == Min_f) {
        unreachable_value = weight_t(std::numeric_limits<float>::max()); // +\inf
    } else if (finVal == Max_f) {
        unreachable_value = weight_t(std::numeric_limits<float>::lowest()); // -\inf
    } else { // SumB
        unreachable_value = weight_t(std::numeric_limits<float>::lowest()); // -\inf
    }

    // Initialize the vector
    for (size_t k = 0; k < next_vector.size(); ++k) {
        next_vector[k] = unreachable_value;
    }

    // Compute max value achievable ending in each state k in B_i
    for (size_t k = 0; k < current_vector.size(); ++k) {
        weight_t from_value = current_vector[k];

        bool should_skip;
        if (finVal == Min_f) {
            // Skip if this component has +inf AND it's not the initial state
            should_skip = (from_value == weight_t(std::numeric_limits<float>::max())) && 
                        (k != B_i->getInitial()->getId());
        } else if (finVal == Max_f) {
            // Skip if this component has -inf AND it's not the initial state  
            should_skip = (from_value == weight_t(std::numeric_limits<float>::lowest())) &&
                        (k != B_i->getInitial()->getId());
        } else { // SumB
            // Skip if this component is -inf (truly unreachable)
            should_skip = (from_value == weight_t(std::numeric_limits<float>::lowest()));
        }

        /*
        #ifdef DEBUG
        std::cout << "  Component " << k << " (state " << B_i->getStates()->at(k)->getName() 
                  << "): value=" << from_value << ", skip=" << should_skip << std::endl;
        #endif
        */
        
        if (should_skip) continue;
        
        State* from_bi_state = B_i->getStates()->at(k);

        // Explore all transitions from bi_state
        for (Edge* edge : *(from_bi_state->getSuccessors(symbol_id))) {
            State* to_bi_state = edge->getTo();
            size_t to_k = to_bi_state->getId();
            weight_t edge_weight = edge->getWeight()->getValue();

            weight_t new_value = transitionFunction(from_value, edge_weight, finVal, bound);

            #ifdef DEBUG
                std::cout << "    Transition: " << from_bi_state->getName() 
                        << " --" << symbol->getName() << "/" << edge_weight 
                        << "--> " << to_bi_state->getName() 
                        << " gives new_value=" << new_value << std::endl;
            #endif
            
            // Update the vector based on finVal
            if (finVal == Min_f) {
                weight_t old_value = next_vector[to_k];
                next_vector[to_k] = std::min(next_vector[to_k], new_value);

                #ifdef DEBUG
                    std::cout << "    Updated component " << to_k << ": " 
                            << old_value << " -> " << next_vector[to_k] << std::endl;
                #endif

            } else {    // for Max_f and SumB
                next_vector[to_k] = std::max(next_vector[to_k], new_value);
            }
        }
    }

    #ifdef DEBUG
        std::cout << "Next vector: ";
        printVector(next_vector);
        std::cout << std::endl;
    #endif

    // Create new DFA state if not seen before
    if (!state_map_DFA.contains(next_vector)) {
        std::ostringstream ss;
        ss << "d_" << state_counter++;

        State* next_state = new State(ss.str(), dfa_alphabet->size(), 0, 1);
        state_map_DFA.insert(next_vector, next_state);
        worklist.push(next_vector);     // Explore this new state

        #ifdef DEBUG
            std::cout << "Created DFA state: " << ss.str() << " with vector: ";
            printVector(next_vector);  
            std::cout << std::endl;
        #endif
    }

	// Create edge (weight 1 if accepting, 0 otherwise)
    // Accepting only if one of the accepting state components of the vector has the value j
    bool accepting = isAcceptingVector(next_vector, j, finals, B_i, bound);
    Weight* weight = dfa_weights->at(accepting ? 1: 0);

	// Set weight (1 if accepting, 0 otherwise)
	// Next state is accepting only if hasFinalIntersection == true and next_value == j

	State* to_state = state_map_DFA[next_vector];
	Edge* edge = new Edge(symbol, weight, from_state, to_state);
	from_state->addSuccessor(edge);
	to_state->addPredecessor(edge);

    #ifdef DEBUG
    std::cout << "Edge: " << from_state->getName() << " --" << symbol->getName()
            << "/" << weight->getValue() << "--> " << to_state->getName() << std::endl;
    #endif
}

// Helper: Collect DFA states and final states
void collectDFAStatesAndFinals(
    const MapStd<StateVector, State*>& state_map_DFA,  
    MapArray<State*>*& dfa_states,
    SetStd<State*>*& dfa_final_states,
    weight_t j,
    const SetStd<State*>* finals,
    const ChildAutomaton* B_i,  
    weight_t bound
) {
    dfa_states = new MapArray<State*>(state_map_DFA.size());
    dfa_final_states = new SetStd<State*>();
    
    unsigned idx = 0;
    for (const auto& [vector, current_dfa_state] : state_map_DFA) {
        // Add to set of states
        dfa_states->insert(idx++, current_dfa_state);

        if (isAcceptingVector(vector, j, finals, B_i, bound)) {
            dfa_final_states->insert(current_dfa_state);
        }
    }
}

// Create S_ij boolean finite-word automaton from a child automaton B_i 
// S_ij recognizes the words on which B_i returns the value j 
// Must provide a bound if finVal = SumB
ChildAutomaton* ChildAutomaton::determiniseToS_ij(weight_t j, value_function_t finVal, weight_t bound) {
    // Reset IDs
    State::RESET();
    Symbol::RESET();
    Weight::RESET();
    
    // 1. Initialize
    MapArray<Symbol*>* dfa_alphabet;
    MapArray<Weight*>* dfa_weights;
    MapStd<StateVector, State*> state_map_DFA;
    std::queue<StateVector> worklist;  // Queue for BFS

    State* initial_dfa;
    unsigned int state_counter = 0;
    
    initializeDFA(dfa_alphabet, dfa_weights, state_map_DFA, worklist, initial_dfa, state_counter, this, finVal);

    // 2. Subset construction using BFS
    while(!worklist.empty()) {
        StateVector current_pair = worklist.front(); worklist.pop();
        for (unsigned symbol_id = 0; symbol_id < dfa_alphabet->size(); ++ symbol_id) {
            processTransition(
                current_pair, symbol_id, dfa_alphabet, dfa_weights, 
                state_map_DFA, worklist, state_counter, j, finVal, bound, this->final_states_, this
            );
        }
    }
    
    // 3. Collect S_ij states and final states
    MapArray<State*>* dfa_states;
    SetStd<State*>* dfa_final_states;
    collectDFAStatesAndFinals(state_map_DFA, dfa_states, dfa_final_states, j, this->final_states_, this, bound);

    // 4. Construct and return the DFA as a ChildAutomaton
    std::ostringstream dfa_name;
    dfa_name << "S_{" << this->getName() << "," << j << "}";
    ChildAutomaton* s_ij = new ChildAutomaton(
        dfa_name.str(),
        dfa_alphabet,
        dfa_states,
        dfa_weights,
        0,
        1,
        initial_dfa,
        dfa_final_states
    );
    #ifdef DEBUG
    if (!allStatesReachable(s_ij)) {
        abort("All states are not reachable for this S_i,j.\n");
    }
    #endif

    ChildAutomaton* minimized = hopcroftMinimizeDFA(s_ij);
    if (minimized != s_ij) {
        delete s_ij;
    }
    return minimized;
}

/* ------------------------- Other HELPERS ------------------------- */
// Returns a set of State* from states whose names are in parser->final_states
inline SetStd<State*> getStatesByNames(MapArray<State*>* states, const SetStd<std::string>& final_state_names) {
    SetStd<State*> result;
    for (auto it = states->begin(); it != states->end(); ++it) {
        State* s = *it;
        if (s && final_state_names.contains(s->getName())) {
            result.insert(s);
        }
    }
    return result;
}

void ChildAutomaton::print(bool full, bool bv_weights, bool bv_only) const {
    print(std::cout, full, bv_weights, bv_only);
}

void ChildAutomaton::print(std::ostream& out, bool full, bool bv_weights, bool bv_only) const {
    out << "Child Automaton (" << this->getName() << "):\n";
    Automaton::print(out);

    out << "\tFinal states: ";
    SetStd<State*>* finals = getFinalStates();
    for (State* s : *finals) {
        out << s->getName() << " ";
    }
    out << std::endl;
}

ChildAutomaton* hopcroftMinimizeDFA(ChildAutomaton* dfa) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    using Block = std::set<State*>;
    using Partition = std::vector<Block>;

    auto* states = dfa->getStates();
    auto* alphabet = dfa->getAlphabet();
    auto* finals = dfa->getFinalStates();
    State* initial = dfa->getInitial();

    // 1. Initial partition: accepting vs non-accepting
    Block accepting, non_accepting;
    for (auto it = states->begin(); it != states->end(); ++it) {
        State* s = *it;
        if (finals->contains(s)) accepting.insert(s);
        else non_accepting.insert(s);
    }
    Partition partition;
    if (!accepting.empty()) partition.push_back(accepting);
    if (!non_accepting.empty()) partition.push_back(non_accepting);

    // Helper: find block index for a state
    auto find_block_index = [&](State* s, const Partition& part) -> int {
        for (size_t i = 0; i < part.size(); ++i)
            if (part[i].count(s)) return static_cast<int>(i);
        return -1;
    };

    // 2. Iterative refinement
    bool changed = true;
    while (changed) {
        changed = false;
        Partition new_partition;
        for (const Block& block : partition) {
            if (block.size() <= 1) {
                new_partition.push_back(block);
                continue;
            }
            // Group by transition signature
            std::map<std::vector<int>, Block> signature_groups;
            for (State* s : block) {
                std::vector<int> signature;
                for (size_t a = 0; a < alphabet->size(); ++a) {
                    Symbol* sym = alphabet->at(a);
                    SetStd<Edge*>* succ = s->getSuccessors(sym->getId());
                    State* dest = nullptr;
                    if (succ && succ->size() == 1) {
                        dest = (*(succ->begin()))->getTo();
                    }
                    signature.push_back(find_block_index(dest, partition));
                }
                signature_groups[signature].insert(s);
            }
            if (signature_groups.size() > 1) changed = true;
            for (auto& [_, group] : signature_groups)
                new_partition.push_back(group);
        }
        partition = std::move(new_partition);
    }

    // If already minimal, return original DFA
    if (partition.size() == states->size()) return dfa;

    // 3. Build minimized DFA
    auto* min_alphabet = new MapArray<Symbol*>(alphabet->size());
    for (size_t i = 0; i < alphabet->size(); ++i)
        min_alphabet->insert(i, new Symbol(*alphabet->at(i)));

    auto* min_weights = new MapArray<Weight*>(dfa->getWeights()->size());
    for (size_t i = 0; i < dfa->getWeights()->size(); ++i)
        min_weights->insert(i, new Weight(*dfa->getWeights()->at(i)));

    auto* min_states = new MapArray<State*>(partition.size());
    auto* min_finals = new SetStd<State*>();
    std::map<State*, int> state_to_block;
    std::vector<State*> block_representatives(partition.size(), nullptr);

    // Create new states and map old states to blocks
    for (size_t i = 0; i < partition.size(); ++i) {
        std::ostringstream nm;
        nm << "q" << i;
        State* new_state = new State(nm.str(), alphabet->size(), 0, 1);
        min_states->insert(i, new_state);
        block_representatives[i] = new_state;
        for (State* s : partition[i]) state_to_block[s] = static_cast<int>(i);
        // If any state in block is final, mark as final
        for (State* s : partition[i]) {
            if (finals->contains(s)) {
                min_finals->insert(new_state);
                break;
            }
        }
    }

    // Set initial state
    State* min_initial = block_representatives[state_to_block[initial]];

    // Add transitions
    for (size_t i = 0; i < partition.size(); ++i) {
        State* from = block_representatives[i];
        State* rep = *(partition[i].begin());
        for (size_t a = 0; a < alphabet->size(); ++a) {
            Symbol* sym = min_alphabet->at(a);
            SetStd<Edge*>* succ = rep->getSuccessors(sym->getId());
            if (succ && succ->size() == 1) {
                State* dest = (*(succ->begin()))->getTo();
                int dest_block = state_to_block[dest];
                State* to = block_representatives[dest_block];
                Weight* w = min_weights->at((*(succ->begin()))->getWeight()->getId());
                Edge* edge = new Edge(sym, w, from, to);
                from->addSuccessor(edge);
                to->addPredecessor(edge);
            }
        }
    }

    // Construct minimized DFA
    ChildAutomaton* min_dfa = new ChildAutomaton(
        dfa->getName() + "_min",
        min_alphabet,
        min_states,
        min_weights,
        0, 1,
        min_initial,
        min_finals
    );

    return min_dfa;
}

/**
 * Helper function: Check if all states are reachable from initial state
 */
bool allStatesReachable(const ChildAutomaton* dfa) {
    if (!dfa || !dfa->getStates() || dfa->getStates()->size() == 0) {
        return true;
    }
    
    MapArray<State*>* states = dfa->getStates();
    MapArray<Symbol*>* alphabet = dfa->getAlphabet();
    State* initial = dfa->getInitial();
    
    if (!initial) {
        return false;
    }
    
    // BFS to find all reachable states
    SetStd<State*> reachable;
    std::queue<State*> worklist;
    
    reachable.insert(initial);
    worklist.push(initial);
    
    while (!worklist.empty()) {
        State* current = worklist.front();
        worklist.pop();
        
        if (!alphabet) continue;
        
        for (Symbol* symbol : *alphabet) {
            if (!symbol) continue;
            
            auto* successors = current->getSuccessors(symbol->getId());
            if (!successors) continue;
            
            for (Edge* edge : *successors) {
                if (!edge || !edge->getTo()) continue;
                
                State* successor = edge->getTo();
                
                if (!reachable.contains(successor)) {
                    reachable.insert(successor);
                    worklist.push(successor);
                }
            }
        }
    }
    
    bool all_reachable = (reachable.size() == states->size());
    
    #ifdef DEBUG
    std::cout << "Reachability check for '" << dfa->getName() << "': " 
              << reachable.size() << "/" << states->size() << " states reachable" << std::endl;
    
    if (!all_reachable) {
        std::cout << "Unreachable states:" << std::endl;
        for (size_t i = 0; i < states->size(); ++i) {
            State* state = states->at(i);
            if (!reachable.contains(state)) {
                std::cout << "  - " << state->getName() << std::endl;
            }
        }
    }
    #endif
    
    return all_reachable;
}
