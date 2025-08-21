#ifndef CHILD_AUTOMATON_H_
#define CHILD_AUTOMATON_H_

#include <string>
#include <memory>
#include <vector>
#include "Map.h"
#include "Set.h"
#include "Parser.h"
#include "Weight.h"
#include "State.h"
#include "Symbol.h"
#include "Word.h"
#include "Automaton.h"

// A child automaton is a Finite-word Quantitative Automata
class ChildAutomaton : public Automaton {
private:
	SetStd<State*>* final_states_;
	
	//value_function_t finval_function_;	// Fixed value function for the automaton
	// TODO: SCC-related fields

public:
	~ChildAutomaton();	
	ChildAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register);
	ChildAutomaton(
		std::string name,
		MapArray<Symbol*>* alphabet,
		MapArray<State*>* states,
		MapArray<Weight*>* weights,
		weight_t min_domain,
		weight_t max_domain,
		State* initial,
		SetStd<State*>* final_states
 	);
	ChildAutomaton(const ChildAutomaton& other);	// Copy constructor

	// Accessor for final states
    SetStd<State*>* getFinalStates() const { return final_states_; }

	void print(bool full = false, bool bv_weights = false, bool bv_only = false) const;
	void print(std::ostream& out, bool full = false, bool bv_weights = false, bool bv_only = false) const;

	// TODO: Other functions
	bool isFinal(State* s) const { 
		return final_states_ && final_states_->contains(s);
	}

	// Key Lemma: S_i,j monitor construction
	ChildAutomaton* determiniseToS_ij(weight_t j, value_function_t g, weight_t bound = -1);
};

// ---------- Free function declarations ------------
bool isMinimalDFA(const ChildAutomaton* dfa);
ChildAutomaton* hopcroftMinimizeDFA(ChildAutomaton* dfa);
bool allStatesReachable(const ChildAutomaton* dfa);

#endif /* CHILD_AUTOMATON_H */
