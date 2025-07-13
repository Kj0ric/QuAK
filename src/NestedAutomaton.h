#ifndef NESTED_AUTOMATON_H_
#define NESTED_AUTOMATON_H_

#include <string>
#include <memory>
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
	// TODO: Decide if domain ranges are needed

	/* Private methods */
	//void build(std::string newname, Parser* parser, MapStd<std::string, Symbol*> sync_register);

public:
	~ChildAutomaton();	
	ChildAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register);

	// Accessor for final states
    SetStd<State*>* getFinalStates() const { return final_states_; }

	// TODO: Methods to print the automaton to stdout
	void print(bool full = false, bool bv_weights = false, bool bv_only = false) const;
	void print(std::ostream& out, bool full = false, bool bv_weights = false, bool bv_only = false) const;

	// TODO: Other functions
	bool isFinal(State* s) const { 
		return final_states_ && final_states_->contains(s);
	}
 	
};

class NestedAutomaton : public Automaton {
private:
	MapArray<ChildAutomaton*>* children_;	// list of Child Automata, instead of weights
	// TODO: Decide if domain ranges are needed

public:
	~NestedAutomaton();
	NestedAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register);

	// TODO: Methods to print the automaton to stdout
	void print(bool full = false, bool bv_weights = false, bool bv_only = false) const;
	void print(std::ostream& out, bool full = false, bool bv_weights = false, bool bv_only = false) const;

	// TODO: Decision problems
};


#endif /* NESTED_AUTOMATON_H_ */