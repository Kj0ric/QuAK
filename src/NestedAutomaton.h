#ifndef NESTED_AUTOMATON_H_
#define NESTED_AUTOMATON_H_

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

	void print(bool full = false, bool bv_weights = false, bool bv_only = false) const;
	void print(std::ostream& out, bool full = false, bool bv_weights = false, bool bv_only = false) const;

	static NestedAutomaton* removeSilentTransitions(const NestedAutomaton* A, value_function_t f);
	std::size_t getChildrenSize() const;
	ChildAutomaton* getChild(std::size_t index) const;

	// Büchi transformation
	Automaton* transformToBuchi(value_function_t finVal);
	// TODO: Decision problems
};

#endif /* NESTED_AUTOMATON_H_ */