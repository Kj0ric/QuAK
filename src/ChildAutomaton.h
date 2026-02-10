#ifndef CHILD_AUTOMATON_H_
#define CHILD_AUTOMATON_H_

#include "Automaton.h"

// Finite-word quantitative automaton (child of a nested automaton)
class ChildAutomaton : public Automaton {
public:
	~ChildAutomaton();
	ChildAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register);
	ChildAutomaton(std::string name, MapArray<Symbol*>* alphabet, MapArray<State*>* states,
	               MapArray<Weight*>* weights, weight_t min_domain, weight_t max_domain,
	               State* initial);
	ChildAutomaton(const ChildAutomaton& other);

	void print(bool full = false, bool bv_weights = false, bool bv_only = false) const;
	void print(std::ostream& out, bool full = false, bool bv_weights = false, bool bv_only = false) const;

	// Archived: determiniseToS_ij, hopcroftMinimizeDFA, allStatesReachable -> ChildAutomaton_oldHelpers.cpp
};

#endif
