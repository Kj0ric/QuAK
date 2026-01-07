#ifndef NESTED_AUTOMATON_H_
#define NESTED_AUTOMATON_H_

#include <string>
#include <memory>
#include <vector>
#include <queue>
#include "Map.h"
#include "Set.h"
#include "Parser.h"
#include "Weight.h"
#include "State.h"
#include "Symbol.h"
#include "Word.h"
#include "Automaton.h"
#include "ChildAutomaton.h"

class NestedAutomaton : public Automaton {
private:
	MapArray<ChildAutomaton*>* children_;	// list of Child Automata, instead of weights
	//size_t children_size;

	NestedAutomaton(const Automaton* parent, MapArray<ChildAutomaton*>* children);	// Helper constructor for removeSilentTransitions

	// TODO: Decide if domain ranges are needed

public:
	~NestedAutomaton();
	NestedAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register);
	NestedAutomaton(std::string filename, Automaton* other = nullptr);
    NestedAutomaton(std::string name, MapArray<Symbol*>* alphabet, MapArray<State*>* states, MapArray<Weight*>* weights, weight_t min_domain, weight_t max_domain, State* initial, MapArray<ChildAutomaton*>* children);

	void print(bool full = false, bool bv_weights = false, bool bv_only = false) const;
	void print(std::ostream& out, bool full = false, bool bv_weights = false, bool bv_only = false) const;

	static NestedAutomaton* removeSilentTransitions(const NestedAutomaton* A, value_function_t f);
	std::size_t getChildrenSize() const;
	ChildAutomaton* getChild(std::size_t index) const;

    std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual> generateMacroAlphabet();
    NestedAutomaton* determinizeWithMacroAlphabet(std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual>& macro_alphabet);
    NestedAutomaton* synchronizeChildren(std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual>& macro_alphabet);
    bool emptiness_monotonic_nesting_supremum(value_function_t infinite_aggregator, value_function_t finite_aggregator, weight_t threshold);
    bool emptiness_monotonic_nesting(value_function_t infinite_aggregator, value_function_t finite_aggregator, weight_t threshold);
    bool emptiness_Avg_SumPlus (value_function_t infinite_aggregator, weight_t threshold);
    Automaton* flatten();

	Automaton* transformToBuchi(value_function_t finVal, weight_t bound = -1);
	// TODO: Decision problems
    // public:
    // SetStd<weight_t> debug_computeChildReturnValuesParentAware(
    //     size_t child_index,
    //     value_function_t finVal,
    //     weight_t bound
    // ) const;
};

// ------------------- Type definitions ----------------------
const weight_t INIT_BUCHI_VALUE = 0;
using MonitorKey = std::pair<size_t, weight_t>;  // (i, j)

struct BuchiState {
    State* parent_state;	// Current state in the parent automaton
    weight_t last_guess;	// Last guessed return value
    SetStd<State*> P1;		// Set of active monitor states (current epoch)
	SetStd<State*> P2;		// Set of active monitor states (previous epoch)

    BuchiState() : parent_state(nullptr), last_guess(0), P1(), P2() {}
    BuchiState(State* parent, weight_t guess, const SetStd<State*>& p1, const SetStd<State*>& p2)
        : parent_state(parent), last_guess(guess), P1(p1), P2(p2) {}
    
    // Required for std::map - defines strict weak ordering
    bool operator<(const BuchiState& other) const {
        // Compare all four fields lexicographically
        if (parent_state != other.parent_state) {
            return parent_state < other.parent_state;  // Compare pointers
        }
        
        if (last_guess != other.last_guess) {
            return last_guess < other.last_guess;  // weight_t has operator<
        }
        
        if (P1 != other.P1) {
            return P1 < other.P1;  // SetStd has operator< defined
        }
        
        return P2 < other.P2;
    }
    
    bool operator==(const BuchiState& other) const {
        return parent_state == other.parent_state && 
            last_guess == other.last_guess && 
            P1 == other.P1 && 
            P2 == other.P2;
    }
};

// ----------------- Free function declarations --------------------
weight_t applyBound(weight_t value, weight_t bound);
SetStd<weight_t> computeChildReturnValues(ChildAutomaton* child, value_function_t finVal, weight_t bound = -1);

SetStd<weight_t> computeGlobalReturnValues(const NestedAutomaton* nwa, value_function_t finVal, weight_t bound = -1);
void computeGlobalDomains(const NestedAutomaton* nwa, weight_t& global_min, weight_t& global_max);

void constructMonitors(
    const NestedAutomaton* nwa,
    const SetStd<weight_t>& global_return_values,
    MapStd<MonitorKey, ChildAutomaton*>& monitors,
    SetStd<State*>& Q_S,
    SetStd<State*>& F_S,
    value_function_t finVal,
    weight_t bound = -1
);
// Monitor stepping functions
SetStd<State*> stepMonitors(const SetStd<State*>& P, Symbol* a, const SetStd<State*>& F_S);
void removeFinalStates(SetStd<State*>& P, const SetStd<State*>& F_S);

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
);

// Büchi transition processing
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
    long long int count
);

#endif /* NESTED_AUTOMATON_H_ */