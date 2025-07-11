#ifndef NESTED_AUTOMATON_H_
#define MESTED_AUTOMATON_H_

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
class ChildAutomaton {
private:
	// final states
	std::string name_;
	State* initial_;	// Multiple initial states in non-deterministic automata ??
	MapArray<Symbol*>* alphabet_;
	MapArray<State*>* states_;
	MapArray<Weight*>* weights_;

	MapArray<State*>* final_states_;
	//value_function_t finval_function_;	// Fixed value function for the automaton
	// TODO: SCC-related fields
	// TODO: Decide if domain ranges are needed

	/* Private methods */
	void build(std::string newname, Parser* parser, MapStd<std::string, Symbol*> sync_register);

public:
	// Constructors
	ChildAutomaton(std::string newname, Parser* parser, MapStd<std::string, Symbol*> sync_register);
	ChildAutomaton(
		std::string name,
		MapArray<Symbol*>* alphabet,
		MapArray<State*>* states,
		MapArray<Weight*>* weights,
		MapArray<State*>* final_states,
		State* initial,
		value_function_t
	) : name_(name), alphabet_(alphabet), states_(states),
		initial_(initial), final_states_(final_states) {}

	void appropriateStates();	// Validate and assign ownership to states
	// TODO: Methods to print the automaton to stdout

	bool isDeterministic() const;
	bool isComplete() const;

};

class NestedAutomaton {
private:
	std::string name_;
	State* initial_;
	Automaton* parent_automaton_;					// Parent Automaton
	MapArray<Symbol*>* alphabet_;
	MapArray<State*>* states_;
	MapArray<ChildAutomaton*>* child_automata_list_;	// list of Child Automata, instead of weights
	// TODO: Decide if domain ranges are needed

public:
	// Decision problems
	bool isNonEmpty(value_function_t f, weight_t x, UltimatelyPeriodicWord** witness = nullptr);
	bool isUniversal(value_function_t f, weight_t x, UltimatelyPeriodicWord** witness = nullptr);


};


#endif /* NESTED_AUTOMATON_H_ */