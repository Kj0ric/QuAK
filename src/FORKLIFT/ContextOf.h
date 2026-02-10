
#ifndef CONTEXTOF_H_
#define CONTEXTOF_H_

#include "StateRelation.h"
#include "../Symbol.h"


// FRESH: inheritance from pair of maps instead of a single map
class ContextOf : protected std::pair<MapArray<StateRelation*>*,MapArray<StateRelation*>*> {
private:
	unsigned int capacity; // FRESH: new atribute
	int nb_ref = 0;
public:
	~ContextOf();
	ContextOf(unsigned int capacity);
	ContextOf(ContextOf* currentB, Symbol* symbol);

	void decreaseRef () { nb_ref = nb_ref - 1; }
	void increaseRef () { nb_ref = nb_ref + 1; }
	int getRef () { return nb_ref; }

	//void print ();

	void add (State* fromB, State* toB, unsigned int weight_id, bool acceptance); // FRESH: now parameterized with a boolean
	bool smaller_than (ContextOf* other, weight_t weight_this, weight_t weight_other);
	StateRelation* at (unsigned int weight_id, bool acceptance) const { // FRESH: now parameterized with a boolean
		if (acceptance == false) {
			return (this->first)->at(weight_id);
		}
		else {
			return (this->second)->at(weight_id);
		}
	};
	unsigned int size () const { return capacity; }; // FRESH: unsigned int size() const {return MapArray<StateRelation*>::size(); };
};

#endif /* CONTEXTOF_H_ */
