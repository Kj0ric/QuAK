
#ifndef STATE_H_
#define STATE_H_

#include <string>
#include "Set.h"
#include "Map.h"
#include "Edge.h"
#include "Weight.h"

class Automaton;
class ChildAutomaton;
class NestedAutomaton;

class State{
private:
	const unsigned int my_id;
	std::string name;
  	Automaton *automaton{nullptr};		// States have an owner automaton 
	int my_scc;				// An ID for the SCC that this state belongs
	bool final;		//CHANGED WITH ACCEPTANCE
	weight_t min_weight;
	weight_t max_weight;
	MapArray<SetStd<Edge*>*>* successors;
	MapArray<SetStd<Edge*>*>* predecessors;

	weight_t dfa_value;	// Value field for S_ij DFAs
	//bool is_initial = false;

  	// Friendship is not inherited
	friend class Automaton;	
  	friend class ChildAutomaton;
  	friend class NestedAutomaton;

public:
	static void RESET();
	static void RESET(unsigned int n);
	~State();
	State (std::string name, unsigned int alphabet_size, weight_t automaton_min_weight, weight_t automaton_max_weight);
	State (State* state); 

	std::string getName() const;
	unsigned int getId() const;
	weight_t getMaxWeightValue() const;
	weight_t getMinWeightValue() const;

	int getTag() const;
	void setTag(int tag);

	bool getFinal() const;
	void setFinal(bool final);

	MapArray<Symbol*> *getAlphabet () const;

	SetStd<Edge*>* getSuccessors(unsigned int symbol_id) const;
	void addSuccessor (Edge* edge);

	SetStd<Edge*>* getPredecessors(unsigned int symbol_id) const;
	void addPredecessor (Edge* edge);

	static std::string toString (State *state);
	std::string toString() const;
	//std::string toStringOnlyName() const; // Why not calling get Name?

	// dfa_value methods
	void setDFAValue(weight_t v) { dfa_value = v; }
	weight_t getDFAValue() const { return dfa_value; }

	//void setInitial() { is_initial = true; }
	//bool checkInitial() { return is_initial; }
};

#endif /* STATE_H_ */
