

#include "ContextOf.h"
#include "inclusion.h"



ContextOf::~ContextOf() {
	for (unsigned int weight_id = 0; weight_id < this->size(); ++weight_id) {
		delete (this->first)->at(weight_id);
		delete (this->second)->at(weight_id);
	}
	delete this->first;
	delete this->second;
}



ContextOf::ContextOf (unsigned int capacity) {
	//	std::pair<MapArray<StateRelation*>, MapArray<StateRelation*>>(new MapArray<StateRelation*>(capacity), MapArray<StateRelation*>(capacity) ) {
	
	this->capacity = capacity;
	this->first = new MapArray<StateRelation*>(this->capacity);
	this->second = new MapArray<StateRelation*>(this->capacity);

	for (unsigned int weight_id = 0; weight_id < this->size(); ++weight_id) {
		(this->first)->insert(weight_id, new StateRelation());
		(this->second)->insert(weight_id, new StateRelation());
	}
}



ContextOf::ContextOf (ContextOf* currentB, Symbol* symbol) {
	//	std::pair<MapArray<StateRelation*>, MapArray<StateRelation*>>( MapArray<StateRelation*>(currentB->size()), MapArray<StateRelation*>(currentB->size()) ) {
	
	this->capacity = currentB->size();
	this->first = new MapArray<StateRelation*>(this->capacity);
	this->second = new MapArray<StateRelation*>(this->capacity);

	for (unsigned int weight_id = 0; weight_id < this->size(); ++weight_id) {
		(this->first)->insert(weight_id, new StateRelation());
		(this->second)->insert(weight_id, new StateRelation());
	}


	for (unsigned int weight_id = 0; weight_id < this->size(); ++weight_id) {
		for (std::pair<State*,TargetOf*> pairB : *(currentB->at(weight_id, false))) {
			for (State* fromB: *(pairB.second)) {
				for (Edge* edgeB : *(fromB->getSuccessors(symbol->getId()))) {
					// #ifdef INCLUSION_SCC_SEARCH_ACTIVE
					// if (edgeB->getFrom()->getTag() != edgeB->getTo()->getTag()) continue;
					// #endif
					auto max_weight_id = std::max(weight_id, edgeB->getWeight()->getId());
					add(pairB.first, edgeB->getTo(), max_weight_id, edgeB->getTo()->getFinal()); // HERE ACCEPTANCE
					// -- since weight are sorted
					// -- id_1 < id_2 <==>  value_1 < value_2
				}
			}
		}
		for (std::pair<State*,TargetOf*> pairB : *(currentB->at(weight_id, true))) {
			for (State* fromB: *(pairB.second)) {
				for (Edge* edgeB : *(fromB->getSuccessors(symbol->getId()))) {
				  // #ifdef INCLUSION_SCC_SEARCH_ACTIVE
				  // if (edgeB->getFrom()->getTag() != edgeB->getTo()->getTag()) continue;
				  // #endif
					auto max_weight_id = std::max(weight_id, edgeB->getWeight()->getId());
					add(pairB.first, edgeB->getTo(), max_weight_id, true); // HERE ACCEPTANCE
					// -- since weight are sorted
					// -- id_1 < id_2 <==>  value_1 < value_2
				}
			}
		}
	}
}


/*
void ContextOf::print () {
	for (unsigned int weight_id = 0; weight_id < this->size(); ++weight_id) {
		printf("weight_id %u\n", weight_id);
		printf("acceptance FALSE\n");
		(this->first)->at(weight_id)->print();
		printf("acceptance TRUE\n");
		(this->second)->at(weight_id)->print();
	}
}
*/





void ContextOf::add (State* fromB, State* toB, unsigned int weight_id, bool acceptance) {
	(this->first)->at(weight_id)->add(fromB, toB);
	if (acceptance == true) (this->second)->at(weight_id)->add(fromB, toB);
}



bool ContextOf::smaller_than (ContextOf* other, weight_t weight_this, weight_t weight_other) {
	if (weight_this < weight_other) return false;

	for (unsigned int x_id = 0; x_id < this->size(); ++x_id) {
		for (std::pair<State*, TargetOf*> pair : *(this->at(x_id,false))) {
			for (State* state : *(pair.second)) {
				bool flag = false;
				for (unsigned int y_id = x_id; y_id < this->size(); ++y_id) {
					if (other->at(y_id,false)->contains(pair.first) == false) continue;
					if (other->at(y_id,false)->at(pair.first)->contains(state) == false) continue;
					flag = true;
					break;
				}
				if (flag == false) return false;
			}
		}

		for (std::pair<State*, TargetOf*> pair : *(this->at(x_id,true))) {
			for (State* state : *(pair.second)) {
				bool flag = false;
				for (unsigned int y_id = x_id; y_id < this->size(); ++y_id) {
					if (other->at(y_id,true)->contains(pair.first) == false) continue;
					if (other->at(y_id,true)->at(pair.first)->contains(state) == false) continue;
					flag = true;
					break;
				}
				if (flag == false) return false;
			}
		}
	}

	return true;
}




//////////////////////////////////////////

/* FRESH PREVIOUS


ContextOf::~ContextOf() {
	for (unsigned int weight_id = 0; weight_id < this->size(); ++weight_id) {
		delete this->at(weight_id);
	}
}



ContextOf::ContextOf(unsigned int capacity) : MapArray<StateRelation*>(capacity) {
	for (unsigned int weight_id = 0; weight_id < this->size(); ++weight_id) {
		this->insert(weight_id, new StateRelation());
	}
}



ContextOf::ContextOf (ContextOf* currentB, Symbol* symbol) : MapArray<StateRelation*>(currentB->size()) {
	for (unsigned int weight_id = 0; weight_id < this->size(); ++weight_id) {
		this->insert(weight_id, new StateRelation());
	}

	for (unsigned int weight_id = 0; weight_id < this->size(); ++weight_id) {
		for (std::pair<State*,TargetOf*> pairB : *(currentB->at(weight_id))) {
			for (State* fromB: *(pairB.second)) {
				for (Edge* edgeB : *(fromB->getSuccessors(symbol->getId()))) {
				  // #ifdef INCLUSION_SCC_SEARCH_ACTIVE
				  // if (edgeB->getFrom()->getTag() != edgeB->getTo()->getTag()) continue;
				  // #endif
					auto max_weight_id = std::max(weight_id, edgeB->getWeight()->getId());
					add(pairB.first, edgeB->getTo(), max_weight_id);
					// -- since weight are sorted
					// -- id_1 < id_2 <==>  value_1 < value_2
				}
			}
		}
	}
}



//void ContextOf::print () {
//	for (unsigned int weight_id = 0; weight_id < this->size(); ++weight_id) {
//		printf("weight_id %u\n", weight_id);
//		this->at(weight_id)->print();
//	}
//}








void ContextOf::add (State* fromB, State* toB, unsigned int weight_id) {
	this->at(weight_id)->add(fromB, toB);
}



bool ContextOf::smaller_than (ContextOf* other, weight_t weight_this, weight_t weight_other) {
	if (weight_this < weight_other) return false;

	for (unsigned int x_id = 0; x_id < this->size(); ++x_id) {
		for (std::pair<State*, TargetOf*> pair : *(this->at(x_id))) {
			for (State* state : *(pair.second)) {
				bool flag = false;
				for (unsigned int y_id = x_id; y_id < this->size(); ++y_id) {
					if (other->at(y_id)->contains(pair.first) == false) continue;
					if (other->at(y_id)->at(pair.first)->contains(state) == false) continue;
					flag = true;
					break;
				}
				if (flag == false) return false;
			}
		}
	}
	return true;
}


*/


