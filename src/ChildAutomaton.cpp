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
// Helper struct for hashing a pair of SetStd<State*> and int
struct SubsetValuePairHash{
    // Define a custom hash functor. This allows the use of struct as a function
    std::size_t operator()(const std::pair<SetStd<State*>, weight_t>& pair) const {
        std::size_t h1 = 0;
        for (const State* s : pair.first) {
            h1 ^= std::hash<unsigned int>()(s->getId());
        }
        std::size_t h2 = std::hash<float>()(pair.second.to_float());
        return h1 ^(h2 << 1);
    }
};

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
        result   = std::min(state_value, transit_value);
    } else if (finVal == SumB) {
        result = state_value + transit_value;
        if (state_value == bound || result >= bound) {
            result = bound;
        } else if (state_value == -bound || result <= -bound) {
            result = -bound;
        }
        // If none satisfies above then result = state_value + transit_value
    } else {
        QUAK_FAIL("transitionFunction: Non-regular value function for child automaton B_i\n");
    }
	return result;
}

// Helper to compare Pairs
using Subset = SetStd<State*>;
using Pair = std::pair<Subset, weight_t>;
// Helper: Initialize S_ij
void initializeDFA(
	MapArray<Symbol*>*& dfa_alphabet, 
	MapArray<Weight*>*& dfa_weights, 
	MapStd<Pair, State*>& state_map_DFA, 
	std::queue<Pair>& worklist, 
	State* &initial_dfa, 
	unsigned int& state_counter, 
	const ChildAutomaton* B_i, 
	weight_t initial_value
) {
    dfa_alphabet = new MapArray<Symbol*>(B_i->getAlphabet()->size());
    for (size_t i = 0; i < B_i->getAlphabet()->size(); ++i) {
        Symbol* orig = B_i->getAlphabet()->at(i);
        dfa_alphabet->insert(i, new Symbol(*orig));
    }

    // Initialize weightss
	dfa_weights = new MapArray<Weight*>(2);
	dfa_weights->insert(0, new Weight(weight_t(0)));
	dfa_weights->insert(1, new Weight(weight_t(1)));

	Subset initial_subset;
	initial_subset.insert(B_i->getInitial());
	Pair initial_pair = {initial_subset, initial_value};

	std::ostringstream ss;
    ss << "d_" << state_counter++;
    initial_dfa = new State(ss.str(), dfa_alphabet->size(), 0, 1);
    initial_dfa->setDFAValue(initial_value);

    state_map_DFA.insert(initial_pair, initial_dfa);
    //state_map_DFA[initial_pair] = initial_dfa;  
    worklist.push(initial_pair);        // Push initial pair to start subset construction

    #ifdef DEBUG
        std::cout << "Initial DFA state: " << ss.str() << " for subset {";
        for (State* s : initial_subset) std::cout << s->getName() << " ";
        std::cout << "} with value: " << initial_value << std::endl;
    #endif
}

bool hasFinalIntersection(const SetStd<State*>& subset, const SetStd<State*>* finals) {
    for (State* s : subset) {
        if (finals->contains(s)) return true;
    }
    return false;
}

// Helper: Check if a DFA state is accepting
bool isAcceptingState(const SetStd<State*>& subset, weight_t value, weight_t j, const SetStd<State*>* finals) {
	return (value == j) && hasFinalIntersection(subset, finals);
}

// Helper: Process a single transition for the subset construction
void processTransition(
	const Pair& current_pair,
	unsigned symbol_id,
	MapArray<Symbol*>* dfa_alphabet,
	MapArray<Weight*>* dfa_weights,
	MapStd<Pair, State*>& state_map_DFA, 
    std::queue<Pair>& worklist,
    unsigned int& state_counter,
	weight_t j,
    value_function_t finVal,
    weight_t bound,
    const SetStd<State*>* finals
) {
	const Subset& current_subset = current_pair.first;
	weight_t current_value = current_pair.second;
	State* from_state = state_map_DFA[current_pair];

	Symbol* symbol = dfa_alphabet->at(symbol_id);
	Subset next_subset;
	auto* weights_to_T = new MapStd<State*, std::vector<weight_t>>();

	// Compute subset of successors
	for (State* s : current_subset) {
		for (Edge* e : *(s->getSuccessors(symbol_id))) {
			State* t = e->getTo();
			next_subset.insert(t);		// Construct the next state of S_ij
			(*weights_to_T)[t].push_back(e->getWeight()->getValue());
		}
	}

    // Next_subset is empty, it will never reach to an accepting state, thus DFA must die
    if (next_subset.size() == 0) {
        // Somehow create a sink state
        // When an invalid input is given, in all cases must create an edge to this sink state
    }

	// Calculate new accumulated value for the next state of S_ij
	weight_t aggregated_weight = aggregateWeights(weights_to_T);
	weight_t next_value = transitionFunction(current_value, aggregated_weight, finVal, bound);
	Pair next_pair = {next_subset, next_value};

	// If next_pair is NOT visited, create new S_ij state out of it
	if (state_map_DFA.contains(next_pair) != true) {
		std::ostringstream ss;
		ss << "d_" << state_counter++;

		State* next_state = new State(ss.str(), dfa_alphabet->size(), 0, 1);
		next_state->setDFAValue(next_value);
		state_map_DFA[next_pair] = next_state;		// Add to the map
		worklist.push(next_pair);				// Add to the queue to explore later

        #ifdef DEBUG
        std::cout << "Created DFA state: " << ss.str() << " for subset {";
        for (State* s : next_subset) std::cout << s->getName() << " ";
        std::cout << "} with value: " << next_value << std::endl;
        #endif
	}

	// Set weight (1 if accepting, 0 otherwise)
	// Next state is accepting only if hasFinalIntersection == true and next_value == j
	bool accepting = isAcceptingState(next_subset, next_value, j, finals);
	Weight* weight = dfa_weights->at(accepting ? 1 : 0);

	State* to_state = state_map_DFA[next_pair];
	Edge* edge = new Edge(symbol, weight, from_state, to_state);
	from_state->addSuccessor(edge);
	to_state->addPredecessor(edge);

    #ifdef DEBUG
    std::cout << "Edge: " << from_state->getName() << " --" << symbol->getName()
            << "/" << weight->getValue() << "--> " << to_state->getName() << std::endl;
    #endif

	delete weights_to_T;
}

// Helper: Collect DFA states and final states
void collectDFAStatesAndFinals(
	const MapStd<Pair, State*>& state_map_DFA, 
	MapArray<State*>*& dfa_states,
	SetStd<State*>*& dfa_final_states,
	weight_t j,
	const SetStd<State*>* finals
) {
	dfa_states = new MapArray<State*>(state_map_DFA.size());
    dfa_final_states = new SetStd<State*>();
    
	unsigned idx = 0;
    // entry = (Pair, State*) where Pair = (Subset, weight_t)
    for (const auto& [pair, current_dfa_state] : state_map_DFA) {
        // Add to set of states
        dfa_states->insert(idx++, current_dfa_state);

        // Add to set of accepting states if
        //   - intersection of the dfa state and B_i accepting state set is not empty and
        //   - value is j
		const SetStd<State*>& subset = pair.first;
        weight_t state_value = pair.second;
        if (isAcceptingState(subset, state_value, j, finals)) {
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
    MapStd<Pair, State*> state_map_DFA;

	std::queue<Pair> worklist;  // Queue for BFS
	State* initial_dfa;
	unsigned int state_counter = 0;
	
    weight_t initial_value = 0;
    if (finVal == Min_f) 
        { initial_value = weight_t(std::numeric_limits<float>::max()); }
    else if (finVal == Max_f) 
        { initial_value = weight_t(std::numeric_limits<float>::lowest()); }
    else ;   // finval is SumB, then initial_value is 0
    
	initializeDFA(dfa_alphabet, dfa_weights, state_map_DFA, worklist, initial_dfa, state_counter, this, initial_value);

    // 2. Subset construction using BFS
    while(!worklist.empty()) {
        Pair current_pair = worklist.front(); worklist.pop();
		for (unsigned symbol_id = 0; symbol_id < dfa_alphabet->size(); ++ symbol_id) {
			processTransition(
				current_pair, symbol_id, dfa_alphabet, dfa_weights, 
				state_map_DFA, worklist, state_counter, j, finVal, bound, this->final_states_
			);
		}
	}
    
    #ifdef DEBUG
    std::cout << "All DFA states constructed:" << std::endl;
    for (const auto& [pair, state] : state_map_DFA) {
        std::cout << state->getName() << ": subset {";
        for (State* s : pair.first) std::cout << s->getName() << " ";
        std::cout << "} value: " << pair.second << std::endl;
    }
    #endif
    
    // 3. Collect S_ij states and final states
	MapArray<State*>* dfa_states;
	SetStd<State*>* dfa_final_states;
	collectDFAStatesAndFinals(state_map_DFA, dfa_states, dfa_final_states, j, this->final_states_);

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

    return s_ij;
}

/* ------------------------- Key Lemma cont'd ------------------------- */
// Helper for taking disjoint union of states of S_ij's
SetStd<State*> disjointUnionCloned(const std::vector<SetStd<State*>>& sets) {
    SetStd<State*> result;
    for (const auto& sset : sets) {
        for (State* s : sset) {
            result.insert(new State(*s));
        }
    }
    return result;
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
