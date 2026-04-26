#include <vector>
#include <memory>
#include <cassert>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <stack>
#include <queue>
#include <unordered_map>
#include "Automaton.h"
#include "Edge.h"
#include "utility.h"
#include "FORKLIFT/inclusion.h"

// Strongly connected component DAG representation
class SCC_Dag {
public:
	State* origin;
	SetStd<SCC_Dag*>* nexts;

	SCC_Dag() : origin(nullptr), nexts(new SetStd<SCC_Dag*>) {};
	~SCC_Dag() { delete nexts; }
	void addNext (SCC_Dag* next) { this->nexts->insert(next); };
	std::string toString (std::string offset) const {
		std::string s = "\n";
		s.append(offset);
		s.append(this->origin->getName());
		offset.append("\t");
		for (SCC_Dag* subdag : *nexts) s.append(subdag->toString(offset));
		return s;
	};
};

Automaton::~Automaton () {
	for (unsigned int state_id = 0; state_id < states->size(); ++state_id) {
		for (Symbol* symbol : *(states->at(state_id)->getAlphabet())) {
			for (Edge* edge : *(states->at(state_id)->getSuccessors(symbol->getId()))) {
				delete edge;
			}
		}
	}
	for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
		delete this->alphabet->at(symbol_id);
	}
	delete alphabet;
	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		delete this->states->at(state_id);
	}
	delete states;
	for (unsigned int weight_id = 0; weight_id < this->weights->size(); ++weight_id) {
		delete this->weights->at(weight_id);
	}
	delete weights;
	if (initial != nullptr) {
		for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
			delete this->SCCs[scc_id];
		}
		delete[] this->SCCs;
		delete[] this->final_SCCs;
	}
}
// Constructors

// Construct from parsed data
Automaton::Automaton(std::string newname, Parser* parser, MapStd<std::string, Symbol*> sync_register) {
	build(newname, parser, sync_register);
}

// Construct from direct data
Automaton::Automaton (
		std::string name,
		MapArray<Symbol*>* alphabet,
		MapArray<State*>* states,
		MapArray<Weight*>* weights,
		weight_t min_domain,
		weight_t max_domain,
		State* initial
) :
		name(name),
		alphabet(alphabet),
		states(states),
		weights(weights),
		min_domain(min_domain),
		max_domain(max_domain),
		initial(initial)
{
  // assert(isComplete() && "The automaton is not complete.");
	appropriateStates();
	if (initial != nullptr) compute_SCC();
}

// Copy constructor
Automaton::Automaton(const Automaton& other) :
	name(other.name),
	min_domain(other.min_domain),
	max_domain(other.max_domain),
	nb_SCCs(0),
	final_SCCs(nullptr),
	SCCs(nullptr)
{
	State::RESET();
	Symbol::RESET();
	Weight::RESET();

	// Copy alphabet
	alphabet = new MapArray<Symbol*>(other.alphabet->size());
	for (size_t i = 0; i < other.alphabet->size(); ++i) {
		alphabet->insert(i, new Symbol(other.alphabet->at(i)));
	}

	// Copy weights
	weights = new MapArray<Weight*>(other.weights->size());
	for (size_t i = 0; i < other.weights->size(); ++i) {
		weights->insert(i, new Weight(other.weights->at(i)));
	}

	// Copy states
	states = new MapArray<State*>(other.states->size());
	for (size_t i = 0; i < other.states->size(); ++i) {
		states->insert(i, new State(other.states->at(i)));
	}

	// Set initial state
	initial = states->at(other.initial->getId());

	// Copy edges
	for (unsigned int state_id = 0; state_id < other.states->size(); ++state_id) {
		State* original_state = other.states->at(state_id);

		for (Symbol* original_symbol : *(original_state->getAlphabet())) {
			for (Edge* original_edge : *(original_state->getSuccessors(original_symbol->getId()))) {
				Symbol* new_symbol = alphabet->at(original_edge->getSymbol()->getId());
				Weight* new_weight = weights->at(original_edge->getWeight()->getId());
				State* new_from = states->at(original_edge->getFrom()->getId());
				State* new_to = states->at(original_edge->getTo()->getId());

				// Create new edge
				Edge* new_edge = new Edge(new_symbol, new_weight, new_from, new_to);
				new_from->addSuccessor(new_edge);
				new_to->addPredecessor(new_edge);
			}
		}
	}

	// Set ownership and compute SCCs
	appropriateStates();
	compute_SCC();
}


// Assign ownership of states to this automaton
void Automaton::appropriateStates() {
  for (auto *state : *states) {
    assert(state->automaton == nullptr);
    state->automaton = this;
  }
}

// Build automaton from parser data
void Automaton::build(std::string newname, Parser* parser, MapStd<std::string, Symbol*> sync_register){
	this->name = newname;

	Symbol::RESET(sync_register.size());
	State::RESET();
	Weight::RESET();

	MapStd<weight_t, Weight*> weight_register;
	MapStd<std::string, State*> state_register;
	MapStd<std::string, Symbol*> symbol_register;

	// Set domain bounds from parser
	this->min_domain = parser->min_domain;
	this->max_domain = parser->max_domain;

	this->weights = new MapArray<Weight*>(parser->weights.size());
	this->alphabet = new MapArray<Symbol*>(parser->alphabet.size());
	this->states = new MapArray<State*>(parser->states.size());

	// Create Weight objects from parser's weight values
	for (weight_t value : parser->weights) {
		Weight* weight = new Weight(value);
		this->weights->insert(weight->getId(), weight);
		weight_register.insert(weight->getValue(), weight);
	}

	// Create State objects from parser's state names
	for (const std::string &statename : parser->states) {
		State* state = new State(statename, this->alphabet->size(), this->min_domain, this->max_domain);
		this->states->insert(state->getId(), state);
		state_register.insert(state->getName(), state);

		if (parser->final_states_all || parser->final_states.contains(statename)) {
			state->setFinal(true);
		}
	}

	this->initial = state_register.at(parser->initial);
	
	// Creates Symbol objects from parser's alphabet
	for (const std::string &symbolname : parser->alphabet) {
		Symbol * symbol;
		if (sync_register.contains(symbolname))
			symbol = new Symbol(sync_register.at(symbolname));
		else
			symbol = new Symbol(symbolname);
		this->alphabet->insert(symbol->getId(), symbol);
		symbol_register.insert(symbol->getName(), symbol);
	}

	// Creates Edge objects from parser's edge tuples
	for (const auto &tuple : parser->edges) {
		Symbol* symbol = symbol_register.at(tuple.first.first);
		Weight* weight = weight_register.at(tuple.first.second);
		State* from = state_register.at(tuple.second.first);
		State* to = state_register.at(tuple.second.second);
		Edge *edge = new Edge(symbol, weight, from, to);
		from->addSuccessor(edge);
		to->addPredecessor(edge);
	}

  	appropriateStates();
	compute_SCC();

	// Recursively trim the automaton:
	// if there are unreachable states, construct a new automaton ignoring those states
	unsigned int reachable = 0;
	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		if (this->states->at(state_id)->getTag() > -1) {
			reachable++;
		}
	}
	if (reachable < this->states->size()) {
		Parser parserTrim = this->parse_trim();
		for (unsigned int state_id = 0; state_id < states->size(); ++state_id) {
			for (Symbol* symbol : *(states->at(state_id)->getAlphabet())) {
				for (Edge* edge : *(states->at(state_id)->getSuccessors(symbol->getId()))) {
					delete edge;
				}
			}
		}
		for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
			delete this->alphabet->at(symbol_id);
		}
		delete this->alphabet;
		for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
			delete this->states->at(state_id);
		}
		delete this->states;
		for (unsigned int weight_id = 0; weight_id < this->weights->size(); ++weight_id) {
			delete this->weights->at(weight_id);
		}
		delete this->weights;
		for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
			delete this->SCCs[scc_id];
		}
		delete[] this->SCCs;
		delete[] this->final_SCCs;
		build(newname, &parserTrim, sync_register);
	}
}

Parser Automaton::parse_trim() {
    Parser parser(this->getMinDomain(), this->getMaxDomain());
    for (unsigned int stateA_id = 0; stateA_id < this->getStates()->size(); ++stateA_id) {
    	if (this->getStates()->at(stateA_id)->getTag() == -1) {
			continue;
		}
		parser.states.insert(this->getStates()->at(stateA_id)->getName());
		if (this->getStates()->at(stateA_id)->getFinal()) {
			parser.final_states.insert(this->getStates()->at(stateA_id)->getName());
		}
        for (Symbol* symbol : *(this->getStates()->at(stateA_id)->getAlphabet())) {
			parser.alphabet.insert(symbol->getName());
			for (Edge* edgeA : *(this->getStates()->at(stateA_id)->getSuccessors(symbol->getId()))) {
				parser.weights.insert(edgeA->getWeight()->getValue());
				std::pair<std::pair<std::string, weight_t>,std::pair<std::string, std::string>> edge;
				edge.first.first = edgeA->getSymbol()->getName();
				edge.first.second = edgeA->getWeight()->getValue();
				edge.second.first = edgeA->getFrom()->getName();
				edge.second.second = edgeA->getTo()->getName();
				parser.edges.insert(edge);
			}
	    }
    }
    parser.initial = this->getInitial()->getName();
    return parser;
}

// Create automaton from file with optional alphabet sync
Automaton::Automaton(std::string filename, Automaton* other) {
	MapStd<std::string, Symbol*> sync_register;
	if (other != nullptr) {
		for (unsigned int symbol_id = 0; symbol_id < other->alphabet->size(); ++symbol_id) {
			other->alphabet->at(symbol_id);
			sync_register.insert(other->alphabet->at(symbol_id)->getName(), other->alphabet->at(symbol_id));
		}
	}
	Parser parser(filename, &sync_register);
	build(filename, &parser, sync_register);
}


Automaton* Automaton::from_file_sync_alphabet (std::string filename, Automaton* other) {
	MapStd<std::string, Symbol*> sync_register;
	if (other != nullptr) {
		for (unsigned int symbol_id = 0; symbol_id < other->alphabet->size(); ++symbol_id) {
			other->alphabet->at(symbol_id);
			sync_register.insert(other->alphabet->at(symbol_id)->getName(), other->alphabet->at(symbol_id));
		}
	}
	Parser parser(filename, &sync_register);
	return (new Automaton(filename, &parser, sync_register));
}



std::string aggregator_name (aggregator_t aggregator) {
	switch (aggregator) {
		case Max: return "Max";
		case Min: return "Min";
		case Plus: return "Plus";
		case Minus: return "Minus";
		case Times: return "Times";
		default: QUAK_FAIL("case aggregator_t");
	}
}

weight_t aggregator_apply (aggregator_t aggregator, weight_t x, weight_t y) {
	switch (aggregator) {
		case Max: return std::max(x, y);
		case Min: return std::min(x, y);
		case Plus: return x + y;
		case Minus: return x - y;
		case Times: return x * y;
		default: QUAK_FAIL("case aggregator_t");
	}
}


Automaton* Automaton::product(const Automaton* A, aggregator_t f, const Automaton* B) {
	MapStd<std::string, Symbol*> sync_register;
	Parser parser(aggregator_apply(f, A->min_domain, B->min_domain), aggregator_apply(f, A->max_domain, B->max_domain));

	for (unsigned int stateA_id = 0; stateA_id < A->states->size(); ++stateA_id) {
		if (A->states->at(stateA_id)->getTag() == -1) continue;
		for (unsigned int stateB_id = 0; stateB_id < B->states->size(); ++stateB_id) {
			if (B->states->at(stateB_id)->getTag() == -1) continue;

			for (Symbol* symbol : *(A->states->at(stateA_id)->getAlphabet())) {
				if (B->alphabet->size() <= symbol->getId()) continue;
				if (B->alphabet->at(symbol->getId())->getName() != symbol->getName()) QUAK_FAIL("product with unsynchronized alphabet");

				for (Edge* edgeA : *(A->states->at(stateA_id)->getSuccessors(symbol->getId()))) {
					for (Edge* edgeB : *(B->states->at(stateB_id)->getSuccessors(symbol->getId()))) {
						std::string symbolname = symbol->getName();
						weight_t weightvalue = aggregator_apply(f, edgeA->getWeight()->getValue(), edgeB->getWeight()->getValue());
						std::string fromname =  "(" + edgeA->getFrom()->getName() + "," + edgeB->getFrom()->getName() + ")";
						std::string toname =  "(" + edgeA->getTo()->getName() + "," + edgeB->getTo()->getName() + ")";

						parser.alphabet.insert(symbolname);
						parser.weights.insert(weightvalue);
						parser.states.insert(fromname);
						parser.states.insert(toname);

						if (A->initial->getId() == stateA_id && B->initial->getId() == stateB_id) {
							parser.initial = fromname;
						}

						std::pair<std::pair<std::string, weight_t>,std::pair<std::string, std::string>> edge;
						edge.first.first = symbolname;
						edge.first.second = weightvalue;
						edge.second.first = fromname;
						edge.second.second = toname;
						parser.edges.insert(edge);
					}
				}
			}
		}
	}

	std::string newname =  aggregator_name(f) + "(" + A->getName() + "," + B->getName() + ")";
	return (new Automaton(newname, &parser, sync_register));
}



Parser* parse_trim_complete(const Automaton* A, value_function_t f) {
	Parser* parser = new Parser(A->getMinDomain(), A->getMaxDomain());
	for (unsigned int stateA_id = 0; stateA_id < A->getStates()->size(); ++stateA_id) {
		if (A->getStates()->at(stateA_id)->getTag() == -1) continue;
		parser->states.insert(A->getStates()->at(stateA_id)->getName());
		if (A->getStates()->at(stateA_id)->getFinal()) {
			parser->final_states.insert(A->getStates()->at(stateA_id)->getName());
		}
		for (Symbol* symbol : *(A->getStates()->at(stateA_id)->getAlphabet())) {
			parser->alphabet.insert(symbol->getName());
			for (Edge* edgeA : *(A->getStates()->at(stateA_id)->getSuccessors(symbol->getId()))) {
				parser->weights.insert(edgeA->getWeight()->getValue());
				std::pair<std::pair<std::string, weight_t>,std::pair<std::string, std::string>> edge;
				edge.first.first = edgeA->getSymbol()->getName();
				edge.first.second = edgeA->getWeight()->getValue();
				edge.second.first = edgeA->getFrom()->getName();
				edge.second.second = edgeA->getTo()->getName();
				parser->edges.insert(edge);
			}
		}
	}

	weight_t sinkvalue;
	switch(f) {
		case Inf: case LimInf : case LimInfAvg:
			sinkvalue = A->getMaxDomain();
			break;
		case Sup: case LimSup: case LimSupAvg:
			sinkvalue = A->getMinDomain();
			break;
		default: QUAK_FAIL("case value function");
	}

	bool sinkFlag = false;

	for (unsigned int stateA_id = 0; stateA_id < A->getStates()->size(); ++stateA_id) {
		if (A->getStates()->at(stateA_id)->getTag() == -1) continue;
		for (unsigned int symbol_id = 0; symbol_id < A->getAlphabet()->size(); ++symbol_id) {
			if (parser->alphabet.contains(A->getAlphabet()->at(symbol_id)->getName()) == false) continue;
			if (A->getStates()->at(stateA_id)->getSuccessors(symbol_id)->size() > 0) continue;
			// if (A->getStates()->at(stateA_id)->getAlphabet()->contains(A->getAlphabet()->at(symbol_id)) == true) continue;
			parser->states.insert("@sink@");
			parser->weights.insert(sinkvalue);
			std::pair<std::pair<std::string, weight_t>,std::pair<std::string, std::string>> edge;
			edge.first.first = A->getAlphabet()->at(symbol_id)->getName();
			edge.first.second = sinkvalue;
			edge.second.first = A->getStates()->at(stateA_id)->getName();
			edge.second.second = "@sink@";
			parser->edges.insert(edge);
			sinkFlag = true;
		}
	}

	if (sinkFlag) {
		for (std::string symbolname: parser->alphabet) {
			std::pair<std::pair<std::string, weight_t>,std::pair<std::string, std::string>> edge;
			edge.first.first = symbolname;
			edge.first.second = sinkvalue;
			edge.second.first = "@sink@";
			edge.second.second = "@sink@";
			parser->edges.insert(edge);
		}
	}

	parser->initial = A->getInitial()->getName();
	return parser;
}

static bool has_any_final_state(const Automaton* A) {
    for (unsigned int state_id = 0; state_id < A->getStates()->size(); ++state_id) {
        if (A->getStates()->at(state_id)->getFinal()) {
            return true;
        }
    }
    return false;
}


Automaton::Automaton(const Automaton* A, value_function_t f) {
	MapStd<std::string, Symbol*> sync_register;
	Parser* parser = parse_trim_complete(A, f);
	build(A->name, parser, sync_register);
	delete parser;

	if (!has_any_final_state(A)) {
		for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
			this->states->at(state_id)->setFinal(false);
		}
		for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
			this->final_SCCs[scc_id] = false;
		}
	}
}


Automaton* Automaton::copy_trim_complete(const Automaton* A, value_function_t f) {
	MapStd<std::string, Symbol*> sync_register;
	Parser* parser = parse_trim_complete(A, f);
	Automaton* that = new Automaton(A->name, parser, sync_register);
	delete parser;

	if (!has_any_final_state(A)) {
		for (unsigned int state_id = 0; state_id < that->states->size(); ++state_id) {
			that->states->at(state_id)->setFinal(false);
		}
		for (unsigned int scc_id = 0; scc_id < that->nb_SCCs; ++scc_id) {
			that->final_SCCs[scc_id] = false;
		}
	}

  assert(that->isComplete() && "The automaton is not complete.");
	return that;
}



// SCC computation

void compute_SCC_dag(State* startState, int* spot, int* low, bool* stackMem, SCC_Dag** SCCs) {
    
    struct Frame {
        State* state;
        std::vector<Edge*> successors;
        size_t succIndex;
        bool initialized;
    };
    
    auto collectSuccessors = [](State* s) {
        std::vector<Edge*> result;
        for (Symbol* symbol : *(s->getAlphabet())) {
            for (Edge* edge : *(s->getSuccessors(symbol->getId()))) {
                result.push_back(edge);
            }
        }
        return result;
    };
    
    std::vector<Frame> callStack;
    callStack.push_back({startState, {}, 0, false});
    
    while (!callStack.empty()) {
        Frame& frame = callStack.back();
        State* state = frame.state;
        
        // Phase 1: Initialize
        if (!frame.initialized) {
            if (stackMem[state->getId()] == true) {
                callStack.pop_back();
                continue;
            }
            stackMem[state->getId()] = true;
            
            if (spot[state->getId()] == low[state->getId()]) {
                SCCs[state->getTag()]->origin = state;
            }
            
            frame.successors = collectSuccessors(state);
            frame.succIndex = 0;
            frame.initialized = true;
        }
        
        // Phase 2: Process successors one at a time
        if (frame.succIndex < frame.successors.size()) {
            Edge* edge = frame.successors[frame.succIndex];
            State* child = edge->getTo();
            frame.succIndex++;
            
            // Add DAG edge (this happens regardless of whether child was visited)
            if (state->getTag() != child->getTag()) {
                SCCs[state->getTag()]->addNext(SCCs[child->getTag()]);
            }
            
            // Push child if not yet visited
            if (stackMem[child->getId()] != true) {
                callStack.push_back({child, {}, 0, false});
            }
            continue;
        }
        
        // Phase 3: All successors processed, pop frame
        callStack.pop_back();
    }
}

// // Build DAGs (SCC_Dag object) 
// void compute_SCC_dag (State* state, int* spot, int* low, bool* stackMem, SCC_Dag** SCCs) {
// 	if (stackMem[state->getId()] == true) return;
// 	stackMem[state->getId()] = true;

// 	if (spot[state->getId()] == low[state->getId()]) {
// 		SCCs[state->getTag()]->origin = state;
// 	}

// 	// printf("state: %s\n", state->getName().c_str());
// 	for (Symbol* symbol : *(state->getAlphabet())) {
// 		for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
// 			compute_SCC_dag(edge->getTo(), spot, low, stackMem, SCCs);
// 			if (state->getTag() != edge->getTo()->getTag()) {
// 				SCCs[state->getTag()]->addNext(SCCs[edge->getTo()->getTag()]);
// 			}
// 		}
// 	}
// }

void compute_SCC_tag(State* startState, int* tag, int* time, int* spot, int* low, SetList<State*>* stack, bool* stackMem) {
    
    struct Frame {
        State* state;
        std::vector<Edge*> successors;
        size_t succIndex;
        bool initialized;
    };
    
    std::vector<Frame> callStack;
    
    // Collect all successors for a state
    auto collectSuccessors = [](State* s) {
        std::vector<Edge*> result;
        for (Symbol* symbol : *(s->getAlphabet())) {
            for (Edge* edge : *(s->getSuccessors(symbol->getId()))) {
                result.push_back(edge);
            }
        }
        return result;
    };
    
    callStack.push_back({startState, {}, 0, false});
    
    while (!callStack.empty()) {
        Frame& frame = callStack.back();
        State* state = frame.state;
        
        // Phase 1: Initialize (equivalent to start of recursive function)
        if (!frame.initialized) {
            spot[state->getId()] = *time;
            low[state->getId()] = *time;
            (*time)++;
            stack->push(state);
            stackMem[state->getId()] = true;
            
            frame.successors = collectSuccessors(state);
            frame.succIndex = 0;
            frame.initialized = true;
        }
        
        // Phase 2: Process successors
        bool pushedChild = false;
        while (frame.succIndex < frame.successors.size()) {
            Edge* edge = frame.successors[frame.succIndex];
            State* child = edge->getTo();
            
            if (spot[child->getId()] == -1) {
                // Will "recurse" - but first advance index so we continue after return
                frame.succIndex++;
                callStack.push_back({child, {}, 0, false});
                pushedChild = true;
                break;
            } 
            else if (stackMem[child->getId()] == true) {
                low[state->getId()] = std::min(low[state->getId()], spot[child->getId()]);
            }
            frame.succIndex++;
        }
        
        if (pushedChild) {
            continue;  // Process the child frame
        }
        
        // Phase 3: Post-processing (after all successors handled)
        // Update parent's low value (simulates return from recursion)
        if (callStack.size() > 1) {
            Frame& parent = callStack[callStack.size() - 2];
            low[parent.state->getId()] = std::min(low[parent.state->getId()], low[state->getId()]);
        }
        
        // Check if this state is an SCC root
        if (spot[state->getId()] == low[state->getId()]) {
            while (stack->head() != state) {
                stack->head()->setTag(*tag);
                stackMem[stack->head()->getId()] = false;
                stack->pop();
            }
            state->setTag(*tag);
            (*tag)++;
            stackMem[state->getId()] = false;
            stack->pop();
        }
        
        callStack.pop_back();
    }
}

// void compute_SCC_tag (State* state, int* tag, int* time, int* spot, int* low, SetList<State*>* stack, bool* stackMem) {
// 	spot[state->getId()] = *time;
// 	low[state->getId()] = *time;
// 	(*time)++;
// 	stack->push(state);
// 	stackMem[state->getId()] = true;

// 	for (Symbol* symbol : *(state->getAlphabet())) {
// 		for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
// 			if (spot[edge->getTo()->getId()] == -1) {
// 				compute_SCC_tag(edge->getTo(), tag, time, spot, low, stack, stackMem);
// 				low[state->getId()] = std::min(low[state->getId()], low[edge->getTo()->getId()]);
// 			}
// 			else if (stackMem[edge->getTo()->getId()] == true) {
// 				low[state->getId()] = std::min(low[state->getId()], spot[edge->getTo()-> getId()]);
// 			}
// 		}
// 	}

// 	if (spot[state->getId()] == low[state->getId()]) {
// 		while (stack->head() != state) {
// 			stack->head()->setTag(*tag);
// 			stackMem[stack->head()->getId()] = false;
// 			stack->pop();
// 		}
// 		state->setTag(*tag);
// 		// printf("state %s, tag %d\n", state->getName().c_str(), *tag);
// 		(*tag)++;
// 		stackMem[state->getId()] = false;
// 		stack->pop();
// 	}
// }


// Compute Strongly Connected Components using Tarjan's algorithm
// Update this->nb_SCCs and this->SCCs
void Automaton::compute_SCC (void) {
	unsigned int size = this->states->size();
	int* spot = new int[size];	// Array to keep discovery time for each state
	int* low = new int[size];	// Array to keep lowest reachable discovery time for each state
	bool* stackMem = new bool[size];
	int time = 0;
	int tag = 0;
	SetList<State*> stack;	// DFS stack

	for (unsigned int state_id = 0; state_id < size; ++state_id) {
		spot[state_id] = -1;
		stackMem[state_id] = false;
	}

	compute_SCC_tag(initial, &tag, &time, spot, low, &stack, stackMem);
	this->nb_SCCs = tag;
	this->final_SCCs = new bool[nb_SCCs]; // CHANGE WITH ACCEPTANCE

	this->SCCs = new SCC_Dag*[nb_SCCs];
	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		this->SCCs[scc_id] = new SCC_Dag();
		this->final_SCCs[scc_id] = 0;
	}
	compute_SCC_dag(initial, spot, low, stackMem, this->SCCs);

	for (unsigned int state_id = 0; state_id < size; ++state_id) {
		if (this->states->at(state_id)->getTag() > -1) {
			this->final_SCCs[this->states->at(state_id)->getTag()] |= this->states->at(state_id)->getFinal();
		}
	}

	delete [] spot;
	delete [] low;
	delete [] stackMem;
}

// Getters
weight_t Automaton::getTopValue (value_function_t f, UltimatelyPeriodicWord** witness) const {
	weight_t *top_values = new weight_t[this->nb_SCCs];
	weight_t top = compute_Top(f, top_values, witness);
	/*for (unsigned int id = 0;id <this->nb_SCCs; id++) {
		printf("top[%u] = %s\n", id, std::to_string(top_values[id]).c_str());
	}*/
    delete[] top_values;
	return top;
}

weight_t Automaton::getBottomValue (value_function_t f, UltimatelyPeriodicWord** witness) {
	weight_t *bot_values = new weight_t[this->nb_SCCs];
	auto bot = compute_Bottom(f, bot_values, witness);
    delete[] bot_values;
    return bot;
}


MapArray<Symbol*>* Automaton::getAlphabet() const { return this->alphabet; }
MapArray<State*>* Automaton::getStates() const { return this->states; }
MapArray<Weight*>* Automaton::getWeights() const { return this->weights; }
weight_t Automaton::getMinDomain () const { return this->min_domain; }
weight_t Automaton::getMaxDomain () const { return this->max_domain; }
State* Automaton::getInitial () const { return initial; }
const std::string &Automaton::getName() const { return this->name; }
unsigned int Automaton::getAlphabetSize() const { return alphabet->size(); }

void Automaton::invert_weights() {
	for (unsigned int weight_id = 0; weight_id < this->weights->size(); ++weight_id) {
		weight_t value = this->weights->at(weight_id)->getValue();
		this->weights->at(weight_id)->setValue(-value);
	}

	unsigned int i, j;
	for (i = 0, j = this->weights->size()-1; i < j; ++i, --j) {
		Weight* weight_at_i = this->weights->at(i);
		Weight* weight_at_j = this->weights->at(j);
		this->weights->insert(i, weight_at_j);
		this->weights->insert(j, weight_at_i);
	}

	weight_t temp = this->max_domain;
	this->setMaxDomain(-this->getMinDomain());
	this->setMinDomain(-temp);
}

// Transformations
Automaton* Automaton::constantAutomaton (const Automaton* A, weight_t x) {
	State::RESET();
	Symbol::RESET();
	Weight::RESET();

	std::string newname = "Constant(" + std::to_string(x) + ")";

	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
	for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
		newalphabet->insert(symbol_id, new Symbol(A->alphabet->at(symbol_id)));
	}

	MapArray<State*>* newstates = new MapArray<State*>(1);
	newstates->insert(0, new State("unique", newalphabet->size(), x, x));
	State* newinitial = newstates->at(0);
	newinitial->setFinal(true);

	Weight* weight = new Weight(x);
	MapArray<Weight*>* newweights = new MapArray<Weight*>(1);
	newweights->insert(0, weight);

	for (unsigned int symbol_id = 0; symbol_id < newalphabet->size(); ++symbol_id) {
		State* state = newstates->at(0);
		Edge* edge = new Edge(newalphabet->at(symbol_id), weight, state, state);
		state->addSuccessor(edge);
		state->addPredecessor(edge);
	}

	return new Automaton(newname, newalphabet, newstates, newweights, x, x, newinitial);
}

namespace {

Automaton* acceptedLanguageConstantAutomaton(const Automaton* A, weight_t x) {
	State::RESET();
	Symbol::RESET();
	Weight::RESET();

	std::string newname = "AcceptedLanguageConstant(" + A->getName() + ", " + std::to_string(x) + ")";

	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->getAlphabet()->size());
	for (unsigned int symbol_id = 0; symbol_id < A->getAlphabet()->size(); ++symbol_id) {
		newalphabet->insert(symbol_id, new Symbol(A->getAlphabet()->at(symbol_id)));
	}

	MapArray<State*>* newstates = new MapArray<State*>(A->getStates()->size());
	for (unsigned int state_id = 0; state_id < A->getStates()->size(); ++state_id) {
		State* old_state = A->getStates()->at(state_id);
		State* new_state = new State(old_state->getName(), newalphabet->size(), x, x);
		new_state->setFinal(old_state->getFinal());
		newstates->insert(state_id, new_state);
	}
	State* newinitial = newstates->at(A->getInitial()->getId());

	Weight* weight = new Weight(x);
	MapArray<Weight*>* newweights = new MapArray<Weight*>(1);
	newweights->insert(0, weight);

	for (unsigned int state_id = 0; state_id < A->getStates()->size(); ++state_id) {
		State* old_from = A->getStates()->at(state_id);
		for (Symbol* symbol : *(old_from->getAlphabet())) {
			for (Edge* edge : *(old_from->getSuccessors(symbol->getId()))) {
				State* from = newstates->at(edge->getFrom()->getId());
				State* to = newstates->at(edge->getTo()->getId());
				Edge* newedge = new Edge(newalphabet->at(symbol->getId()), weight, from, to);
				from->addSuccessor(newedge);
				to->addPredecessor(newedge);
			}
		}
	}

	return new Automaton(newname, newalphabet, newstates, newweights, x, x, newinitial);
}

} // namespace

Automaton* Automaton::booleanize(const Automaton* A, weight_t x) {
	State::RESET();
	Symbol::RESET();
	Weight::RESET();

	std::string newname = "Bool(" + A->getName() + ", " + std::to_string(x) + ")";

	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
	for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
		newalphabet->insert(symbol_id, new Symbol(A->alphabet->at(symbol_id)));
	}

	MapArray<State*>* newstates = new MapArray<State*>(A->states->size());
	for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
		newstates->insert(state_id, new State(A->states->at(state_id)));
	}
	State* newinitial = newstates->at(A->initial->getId());

	MapArray<Weight*>* newweights = new MapArray<Weight*>(2);
	for (unsigned int weight_id = 0; weight_id < 2; ++weight_id) {
		newweights->insert(weight_id, new Weight(weight_id));
	}

	for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
		for (Symbol* symbol : *(A->states->at(state_id)->getAlphabet())) {
			for (Edge* edge : *(A->states->at(state_id)->getSuccessors(symbol->getId()))) {
				auto value = ((edge->getWeight()->getValue() >= x) ? 1 : 0);
				Weight* weight = newweights->at(value);
				State* from = newstates->at(edge->getFrom()->getId());
				State* to = newstates->at(edge->getTo()->getId());
				Edge* newedge = new Edge(newalphabet->at(symbol->getId()), weight, from, to);
				newstates->at(state_id)->addSuccessor(newedge);
				newstates->at(edge->getTo()->getId())->addPredecessor(newedge);
			}
		}
	}

	return new Automaton(newname, newalphabet, newstates, newweights, 0, 1, newinitial);
}

/* ---------------------------------- SIL --------------------------------- */
// Copy A, replace all weights with value SILENT by a new Weight object with replacement value
// replacement is the silent transition weight value

namespace {
static inline weight_t empty_language_bottom_value() {
    return weight_t(std::numeric_limits<weight_t::T>::lowest());
}

static inline weight_t sup_silent_replacement(const Automaton* A) {
    return A->getMinDomain() - weight_t(1);
}

static inline weight_t inf_silent_replacement(const Automaton* A) {
    return A->getMaxDomain() + weight_t(1);
}
}

Automaton* Automaton::removeSilentTransitionsHelperStandard_prefixIndependent(const Automaton* A,
                                                                              weight_t replacement,
                                                                              weight_t forced_min_domain,
                                                                              weight_t forced_max_domain) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    std::string newname = "NonSilentAccSCC(" + A->getName() + ")";

    // Copy alphabet and states
    MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
    for (unsigned int sid = 0; sid < A->alphabet->size(); ++sid) {
        newalphabet->insert(sid, new Symbol(A->alphabet->at(sid)));
    }

    MapArray<State*>* newstates = new MapArray<State*>(A->states->size());
    for (unsigned int stid = 0; stid < A->states->size(); ++stid) {
        newstates->insert(stid, new State(A->states->at(stid)));
    }
    State* newinitial = newstates->at(A->initial->getId());

    // Copy weights and add replacement weight
    const unsigned int oldW = A->weights->size();
    MapArray<Weight*>* newweights = new MapArray<Weight*>(oldW + 1);

    // Copy all original weights as-is (including SILENT).
    for (unsigned int wid = 0; wid < oldW; ++wid) {
        newweights->insert(wid, new Weight(A->weights->at(wid)->getValue()));
    }

    // One extra weight object used for "de-silencing" inside accepting SCCs.
    Weight* replacementWeight = new Weight(replacement);
    newweights->insert(oldW, replacementWeight);

    // The transformed domain must include the extremal sentinel even if no edge happens to use it.
    weight_t newmin_domain = forced_min_domain;
    weight_t newmax_domain = forced_max_domain;

    // Accepting SCC predicate
    const unsigned int nbSCC = A->nb_SCCs;

    auto is_accepting_scc_id = [&](int cid) -> bool {
        if (cid < 0) return false;
        unsigned int ucid = static_cast<unsigned int>(cid);
        if (ucid >= nbSCC) return false;
        return A->final_SCCs[ucid];
    };

    auto silent_edge_is_internal_to_accepting_scc = [&](const Edge* e) -> bool {
        if (!e) return false;
        if (!e->getWeight() || e->getWeight()->getValue() != SILENT) return false;

        const State* u = e->getFrom();
        const State* v = e->getTo();
        if (!u || !v) return false;

        int cu = u->getTag();
        int cv = v->getTag();
        if (cu < 0 || cv < 0) return false;
        if (cu != cv) return false;

        return is_accepting_scc_id(cu);
    };

    // Copy transitions, de-silence internal edges in accepting SCCs
    for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
        State* oldFrom = A->states->at(state_id);

        for (Symbol* sym : *(oldFrom->getAlphabet())) {
            SetStd<Edge*>* succs = oldFrom->getSuccessors(sym->getId());
            if (!succs) continue;

            for (Edge* edge : *succs) {
                // Choose weight:
                // - internal silent edge inside an accepting SCC: use replacementWeight
                // - otherwise: keep the copied weight by its original weight-id (may still be SILENT)
                Weight* w =
                    silent_edge_is_internal_to_accepting_scc(edge)
                        ? replacementWeight
                        : newweights->at(edge->getWeight()->getId());

                State* from = newstates->at(edge->getFrom()->getId());
                State* to   = newstates->at(edge->getTo()->getId());

                Edge* newedge = new Edge(newalphabet->at(sym->getId()), w, from, to);
                from->addSuccessor(newedge);
                to->addPredecessor(newedge);
            }
        }
    }

    return new Automaton(newname, newalphabet, newstates, newweights, newmin_domain, newmax_domain, newinitial);
}

Automaton* Automaton::removeSilentTransitionsHelperStandard(const Automaton* A,
                                                            weight_t replacement,
                                                            weight_t forced_min_domain,
                                                            weight_t forced_max_domain) {
	State::RESET();
	Symbol::RESET();
	Weight::RESET();

	std::string newname = "NonSilent(" + A->getName() + ")";

	// We cannot change the weight list of A. Hence we need to copy all unchanged fields to the new automaton.
	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
	for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
		newalphabet->insert(symbol_id, new Symbol(A->alphabet->at(symbol_id)));
	}

	MapArray<State*>* newstates = new MapArray<State*>(A->states->size());
	for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
		newstates->insert(state_id, new State(A->states->at(state_id)));
	}
	State* newinitial = newstates->at(A->initial->getId());

	MapArray<Weight*>* newweights = new MapArray<Weight*>(A->weights->size());
	weight_t newmin_domain = forced_min_domain;
	weight_t newmax_domain = forced_max_domain;
	for (unsigned int weight_id = 0; weight_id < A->weights->size(); ++weight_id) {
		
		if (A->weights->at(weight_id)->getValue() == SILENT) {
	    		// Replace the weights with float value SILENT with new Weights objects that represent silent
	    		Weight* rep = new Weight(replacement);
    		newweights->insert(weight_id, rep);
		}
		else{
	    		newweights->insert(weight_id, new Weight(A->weights->at(weight_id)->getValue())); // If not silent, keep the weight value
		}
	}

	for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
		for (Symbol* symbol : *(A->states->at(state_id)->getAlphabet())) {
			for (Edge* edge : *(A->states->at(state_id)->getSuccessors(symbol->getId()))) {
				Weight* weight = newweights->at(edge->getWeight()->getId());	// newweights are used => weights used in state information are silent weight objects
				State* from = newstates->at(edge->getFrom()->getId());
				State* to = newstates->at(edge->getTo()->getId());
				Edge* newedge = new Edge(newalphabet->at(symbol->getId()), weight, from, to);
				newstates->at(state_id)->addSuccessor(newedge);
				newstates->at(edge->getTo()->getId())->addPredecessor(newedge);
			}
		}
	}

	return new Automaton(newname, newalphabet, newstates, newweights, newmin_domain, newmax_domain, newinitial);
}

Automaton* Automaton::removeSilentTransitionsHelperLimitAverage_prefixIndependent(const Automaton* A) {
    // Remove ONLY silent transitions that are INTERNAL to ACCEPTING SCCs.
    // All other silent transitions are kept as-is.

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    // Custom hash for tuple<uint, uint, uint>
    struct TupleHash {
        size_t operator()(const std::tuple<unsigned int, unsigned int, unsigned int>& t) const {
            auto h1 = std::hash<unsigned int>{}(std::get<0>(t));
            auto h2 = std::hash<unsigned int>{}(std::get<1>(t));
            auto h3 = std::hash<unsigned int>{}(std::get<2>(t));
            return h1 ^ (h2 * 31) ^ (h3 * 997);
        }
    };

    using EdgeKey = std::tuple<unsigned int, unsigned int, unsigned int>;
    using EdgeMap = std::unordered_map<EdgeKey, weight_t, TupleHash>;

    // Copy alphabet and states
    MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
    for (unsigned int sid = 0; sid < A->alphabet->size(); ++sid) {
        newalphabet->insert(sid, new Symbol(A->alphabet->at(sid)));
    }

    MapArray<State*>* newstates = new MapArray<State*>(A->states->size());
    for (unsigned int stid = 0; stid < A->states->size(); ++stid) {
        newstates->insert(stid, new State(A->states->at(stid)));
    }
    State* newinitial = newstates->at(A->initial->getId());

    const unsigned int n = A->states->size();
    const unsigned int nbSCC = A->nb_SCCs;

    // Helper lambdas
    auto is_accepting_scc_id = [&](int cid) -> bool {
        if (cid < 0) return false;
        unsigned int ucid = static_cast<unsigned int>(cid);
        if (ucid >= nbSCC) return false;
        return A->final_SCCs[ucid];
    };

    auto in_accepting_scc = [&](const State* s) -> bool {
        return is_accepting_scc_id(s->getTag());
    };

    auto silent_edge_is_internal_to_accepting_scc =
        [&](const State* u, const State* v, weight_t w) -> bool
    {
        if (w != SILENT) return false;
        int cu = u->getTag();
        int cv = v->getTag();
        if (cu < 0 || cv < 0) return false;
        if (cu != cv) return false;
        return is_accepting_scc_id(cu);
    };

    // Compute restricted epsilon-closure
    std::vector<std::vector<unsigned int>> silentSucc(n);

    for (unsigned int i = 0; i < n; ++i) {
        State* root = A->states->at(i);
        silentSucc[i].push_back(i);  // reflexive

        if (!in_accepting_scc(root)) continue;

        const int root_cid = root->getTag();

        std::stack<State*> st;
        st.push(root);

        // Use a local visited set (flat vector for speed)
        std::vector<bool> visited(n, false);
        visited[i] = true;

        while (!st.empty()) {
            State* u = st.top();
            st.pop();

            if (u->getTag() != root_cid) continue;

            for (Symbol* sym : *(u->getAlphabet())) {
                SetStd<Edge*>* succs = u->getSuccessors(sym->getId());
                if (!succs) continue;

                for (Edge* e : *succs) {
                    weight_t w = e->getWeight()->getValue();
                    if (w != SILENT) continue;

                    State* v = e->getTo();
                    if (!v) continue;

                    unsigned int vId = v->getId();
                    if (!silent_edge_is_internal_to_accepting_scc(u, v, w)) continue;

                    if (!visited[vId]) {
                        visited[vId] = true;
                        silentSucc[i].push_back(vId);
                        st.push(v);
                    }
                }
            }
        }
    }

    // Compute reverse closure
    std::vector<std::vector<unsigned int>> silentPred(n);
    for (unsigned int j = 0; j < n; ++j) {
        for (unsigned int reached : silentSucc[j]) {
            silentPred[reached].push_back(j);
        }
    }

    // Gather best compressed non-silent edges
    // Estimate capacity: at most O(m) entries where m = number of non-silent edges
    EdgeMap best;
    best.reserve(n * 4);  // heuristic; adjust based on typical density

    for (unsigned int sId = 0; sId < n; ++sId) {
        State* s = A->states->at(sId);

        for (Symbol* sym : *(s->getAlphabet())) {
            SetStd<Edge*>* succs = s->getSuccessors(sym->getId());
            if (!succs) continue;

            for (Edge* e : *succs) {
                weight_t w = e->getWeight()->getValue();
                if (w == SILENT) continue;  // only compress around a REAL step

                State* t = e->getTo();
                if (!t) continue;

                unsigned int tId = t->getId();
                unsigned int symId = sym->getId();

                // Cross-product: all states that can reach s via silent *
                //                all states reachable from t via silent
                for (unsigned int pId : silentPred[sId]) {
                    for (unsigned int rId : silentSucc[tId]) {
                        EdgeKey key{pId, symId, rId};
                        auto it = best.find(key);
                        if (it == best.end()) {
                            best.emplace(key, w);
                        } else if (w > it->second) {
                            it->second = w;
                        }
                    }
                }
            }
        }
    }

    // Build final transition set
    EdgeMap final_edges;
    final_edges.reserve(best.size() + n * 4);

    // 4a) Keep original edges unless they are silent AND internal to an accepting SCC
    for (unsigned int uid = 0; uid < n; ++uid) {
        State* u = A->states->at(uid);

        for (Symbol* sym : *(u->getAlphabet())) {
            SetStd<Edge*>* succs = u->getSuccessors(sym->getId());
            if (!succs) continue;

            for (Edge* e : *succs) {
                State* v = e->getTo();
                if (!v) continue;

                weight_t w = e->getWeight()->getValue();

                // Drop only silent edges that are internal to an accepting SCC
                if (silent_edge_is_internal_to_accepting_scc(u, v, w)) {
                    continue;
                }

                EdgeKey key{uid, sym->getId(), v->getId()};
                auto [it, inserted] = final_edges.try_emplace(key, w);
                if (!inserted && w > it->second) {
                    it->second = w;
                }
            }
        }
    }

    // 4b) Merge compressed edges from 'best'
    for (const auto& [key, w] : best) {
        auto [it, inserted] = final_edges.try_emplace(key, w);
        if (!inserted && w > it->second) {
            it->second = w;
        }
    }

    // Collect unique weights
    SetSorted<weight_t> weight_vals;
    // weight_vals.reserve(final_edges.size());  // upper bound
    for (const auto& [key, w] : final_edges) {
        weight_vals.insert(w);
    }

    MapArray<Weight*>* newweights = new MapArray<Weight*>(weight_vals.size());
    MapStd<weight_t, Weight*> wreg;
    for (weight_t v : weight_vals) {
        Weight* wobj = new Weight(v);
        newweights->insert(wobj->getId(), wobj);
        wreg.insert(v, wobj);
    }

    // Create new transition relation
    for (const auto& [key, wval] : final_edges) {
        auto [pId, symId, rId] = key;

        Symbol* sym  = newalphabet->at(symId);
        State*  from = newstates->at(pId);
        State*  to   = newstates->at(rId);
        Weight* w    = wreg.at(wval);

        Edge* edge = new Edge(sym, w, from, to);
        from->addSuccessor(edge);
        to->addPredecessor(edge);
    }

    // Wrap-up
    std::string newname = "NonSilentAccSCC(" + A->getName() + ")";
    return new Automaton(
        newname, newalphabet, newstates, newweights,
        A->min_domain, A->max_domain, newinitial
    );
}



Automaton* Automaton::removeSilentTransitionsHelperLimitAverage(const Automaton* A) {
	//  --------  A_fix : compress every (silent)*(nonSilent)(silent)* pattern  --------
	State::RESET();
	Symbol::RESET();
	Weight::RESET();

	// Copy alphabet and states
	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
	for (unsigned int sid = 0; sid < A->alphabet->size(); ++sid)
		newalphabet->insert(sid, new Symbol(A->alphabet->at(sid)));

	MapArray<State*>* newstates = new MapArray<State*>(A->states->size());
	for (unsigned int stid = 0; stid < A->states->size(); ++stid)
		newstates->insert(stid, new State(A->states->at(stid)));
	State* newinitial = newstates->at(A->initial->getId());

	// Compute silent-closure
	const unsigned int n = A->states->size();
	std::vector< SetStd<State*> > silentSucc(n);

	for (unsigned int i = 0; i < n; ++i) {
		State* root = A->states->at(i);
		silentSucc[i].insert(root);

		std::stack<State*> st;
		st.push(root);
		while (!st.empty()) {
			State* u = st.top(); st.pop();
			for (Symbol* sym : *(u->getAlphabet())) {
				for (Edge* e : *(u->getSuccessors(sym->getId()))) {
					if (e->getWeight()->getValue() != SILENT) continue;
					State* v = e->getTo();
					if (!silentSucc[i].contains(v)) {
						silentSucc[i].insert(v);
						st.push(v);
					}
				}
			}
		}
	}

	// Gather best compressed edges
	std::map< std::tuple<unsigned,int,unsigned>, weight_t > best;      // (p,a,r) -> max weight
	SetSorted<weight_t>                weight_vals;

	for (unsigned int pId = 0; pId < n; ++pId) {
		for (State* s : silentSucc[pId]) {
			for (Symbol* sym : *(s->getAlphabet())) {
				for (Edge* e : *(s->getSuccessors(sym->getId()))) {
					if (e->getWeight()->getValue() == SILENT) continue;
					weight_t w = e->getWeight()->getValue();
					State* t = e->getTo();
					for (State* r : silentSucc[t->getId()]) {
						auto key = std::make_tuple(pId, sym->getId(), r->getId());
						auto it  = best.find(key);
						if (it == best.end() || w > it->second) best[key] = w;
					}
				}
			}
		}
	}

	for (const auto &kv : best) weight_vals.insert(kv.second);

	// Materialize new weight objects
	MapArray<Weight*>* newweights = new MapArray<Weight*>(weight_vals.size());
	MapStd<weight_t, Weight*> wreg;
	for (weight_t v : weight_vals) {
		Weight* w = new Weight(v);
		newweights->insert(w->getId(), w);
		wreg.insert(v, w);
	}

	// Create compressed transition relation
	for (const auto &kv : best) {
		unsigned pId, symId, rId;
		std::tie(pId, symId, rId) = kv.first;
		Symbol* sym  = newalphabet->at(symId);
		State*  from = newstates->at(pId);
		State*  to   = newstates->at(rId);
		Weight* w    = wreg.at(kv.second);

		Edge* e = new Edge(sym, w, from, to);
		from->addSuccessor(e);
		to->addPredecessor(e);
	}

	// Wrap-up
	std::string newname = "NonSilent(" + A->getName() + ")";
	return new Automaton(
		newname, newalphabet, newstates, newweights,
		A->min_domain, A->max_domain, newinitial
	);
}

Automaton* Automaton::removeSilentTransitions(const Automaton* A, value_function_t f, bool withShortcuts) {
	if (f == Inf || f == LimInf) {
		const weight_t replacement = inf_silent_replacement(A);
		const weight_t forced_min_domain = A->getMinDomain();
		const weight_t forced_max_domain = replacement;
		if (f == LimInf && withShortcuts) {
			return removeSilentTransitionsHelperStandard_prefixIndependent(
			    A, replacement, forced_min_domain, forced_max_domain);
		}
		return removeSilentTransitionsHelperStandard(
		    A, replacement, forced_min_domain, forced_max_domain);
	}
	else if (f == Sup || f == LimSup) {
		const weight_t replacement = sup_silent_replacement(A);
		const weight_t forced_min_domain = replacement;
		const weight_t forced_max_domain = A->getMaxDomain();
		if (f == LimSup && withShortcuts) {
			return removeSilentTransitionsHelperStandard_prefixIndependent(
			    A, replacement, forced_min_domain, forced_max_domain);
		}
		return removeSilentTransitionsHelperStandard(
		    A, replacement, forced_min_domain, forced_max_domain);
	}
	else if (f == LimInfAvg || f == LimSupAvg) {
		if (withShortcuts) return removeSilentTransitionsHelperLimitAverage_prefixIndependent(A);
		else return removeSilentTransitionsHelperLimitAverage(A);
	}
	else {
		QUAK_FAIL("invalid value function");
	}
}




Automaton* Automaton::safetyClosure(Automaton* A, value_function_t f) {
	if (f == Sup) {
		std::unique_ptr<Automaton> AA = std::unique_ptr<Automaton>(Automaton::toLimSup(A, f));
		return safetyClosure(AA.get(), LimSup);
	}

	State::RESET();
	Symbol::RESET();
	Weight::RESET();

	std::string newname = "SafeOf(" + A->name + ")";

	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
	for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
		newalphabet->insert(symbol_id, new Symbol(A->alphabet->at(symbol_id)));
	}
	MapArray<State*>* newstates = new MapArray<State*>(A->states->size());
	for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
		newstates->insert(state_id, new State(A->states->at(state_id)));
	}
	State* newinitial = newstates->at(A->initial->getId());

	weight_t *top_values = new weight_t[A->nb_SCCs];
	A->compute_Top(f, top_values);


	SetSorted<weight_t> weight_values;
	MapStd<weight_t, Weight*> weight_register;
	for (unsigned int scc_id = 0; scc_id < A->nb_SCCs; ++scc_id) {
		weight_values.insert(top_values[scc_id]);
	}
	weight_t newmin_domain = A->min_domain;
	weight_t newmax_domain = A->max_domain;

	MapArray<Weight*>* newweights = new MapArray<Weight*>(weight_values.size());
	for (weight_t value : weight_values) {
		Weight* weight = new Weight(value);
		newweights->insert(weight->getId(), weight);
		weight_register.insert(weight->getValue(), weight);
	}

	for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
		if (A->states->at(state_id)->getTag() == -1) continue;
		for (Symbol* symbol : *(A->states->at(state_id)->getAlphabet())) {
			for (Edge* edge : *(A->states->at(state_id)->getSuccessors(symbol->getId()))) {
				State* from = newstates->at(edge->getFrom()->getId());
				State* to = newstates->at(edge->getTo()->getId());
				Weight* weight = weight_register.at(top_values[edge->getTo()->getTag()]);
				Edge* newedge = new Edge(newalphabet->at(symbol->getId()), weight, from, to);
				from->addSuccessor(newedge);
				to->addPredecessor(newedge);
			}
		}
	}

    delete[] top_values;
	return new Automaton(newname, newalphabet, newstates, newweights, newmin_domain, newmax_domain, newinitial);
}


Automaton* Automaton::livenessComponent_deterministic (const Automaton* A, value_function_t f) {
	if (A->isDeterministic() == false || f == Inf || f == LimInfAvg || f == LimSupAvg) {
		QUAK_FAIL("invalid automaton type for deterministic liveness component");
	}

	State::RESET();
	Symbol::RESET();
	Weight::RESET();

	std::string newname = "LiveOf(" + A->getName() + ")";

	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
	for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
		newalphabet->insert(symbol_id, new Symbol(A->alphabet->at(symbol_id)));
	}

	MapArray<State*>* newstates = new MapArray<State*>(A->states->size());
	for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
		newstates->insert(state_id, new State(A->states->at(state_id)));
	}
	State* newinitial = newstates->at(A->initial->getId());

	weight_t *top_values = new weight_t[A->nb_SCCs];
	A->compute_Top(f, top_values);

	SetSorted<weight_t> weight_values;
	weight_values.insert(top_values[A->initial->getTag()]);
	for (unsigned int weight_id = 0; weight_id < A->weights->size(); ++weight_id) {
		weight_values.insert(A->weights->at(weight_id)->getValue());
	}
	weight_t newmin_domain = A->min_domain;
	weight_t newmax_domain = A->max_domain;

	MapStd<weight_t, Weight*> weight_register;
	MapArray<Weight*>* newweights = new MapArray<Weight*>(weight_values.size());
	for (weight_t value : weight_values) {
		Weight* weight = new Weight(value);
		newweights->insert(weight->getId(), weight);
		weight_register.insert(weight->getValue(), weight);
	}

	for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
		for (Symbol* symbol : *(A->states->at(state_id)->getAlphabet())) {
			for (Edge* edge : *(A->states->at(state_id)->getSuccessors(symbol->getId()))) {
				State* from = newstates->at(edge->getFrom()->getId());
				State* to = newstates->at(edge->getTo()->getId());
				weight_t value;
				if (edge->getWeight()->getValue() == top_values[from->getTag()]) {
					value = top_values[A->initial->getTag()];
				}
				else {
					value = edge->getWeight()->getValue();
				}
				Weight* weight = weight_register.at(value);
				Edge* new_edge = new Edge(newalphabet->at(symbol->getId()), weight, from, to);
				from->addSuccessor(new_edge);
				to->addPredecessor(new_edge);
			}
		}
	}

    delete[] top_values;
	return new Automaton(newname, newalphabet, newstates, newweights, newmin_domain, newmax_domain, newinitial);
}


Automaton* Automaton::livenessComponent(const Automaton* A, value_function_t f) {
	if ((f == Sup || f == LimInf || f == LimSup) && A->isDeterministic()) {
		return Automaton::livenessComponent_deterministic(A, f);
	}
	
	if (f == LimInf || f == LimSup || f == LimInfAvg || f == LimSupAvg) {
		return Automaton::livenessComponent_prefixIndependent(A, f);
	}
	
	QUAK_FAIL("Cannot do safety-liveness decomposition for this type of automata.");
}



Automaton* Automaton::livenessComponent_prefixIndependent (const Automaton* A, value_function_t f) {
	weight_t *top_values = new weight_t[A->nb_SCCs];
	SetList<Edge*>* scc_cycles[A->nb_SCCs];
	if (f == LimInf) {
		A->top_LimInf_cycles(top_values, scc_cycles);
        delete[] top_values;
	}
	else if (f == LimSup) {
		A->top_LimSup_cycles(top_values, scc_cycles);
        delete[] top_values;
	}
	else if (f == LimInfAvg || f == LimSupAvg) {
		A->top_LimAvg_cycles(top_values, scc_cycles);
        delete[] top_values;
	}
	else {
        delete[] top_values;
		QUAK_FAIL("invalid automaton type for prefix-independent liveness component computation");
	}


	//Symbol::RESET();
	//Weight::RESET();
	State::RESET(A->states->size());

	std::string newname = "LiveOf(" + A->getName() + ")";

	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
	for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
		newalphabet->insert(symbol_id, new Symbol(A->alphabet->at(symbol_id)));
	}

	unsigned int size_of_cycles = 0;
	for (unsigned int scc_id = 0; scc_id < A->nb_SCCs; ++scc_id) {
		if (scc_cycles[scc_id]->size() != 0) {
			size_of_cycles += scc_cycles[scc_id]->size();
		}
	}

	MapArray<State*>* newstates = new MapArray<State*>(A->states->size() + size_of_cycles + 1);
	for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
		newstates->insert(state_id, new State(A->states->at(state_id)));
	}
	State* newinitial = newstates->at(A->initial->getId());

	MapStd<unsigned int, State*> state_register;
	for (unsigned int scc_id = 0; scc_id < A->nb_SCCs; ++scc_id) {
		if (scc_cycles[scc_id]->size() != 0) {
			for (Edge* edge : *(scc_cycles[scc_id])) {
				std::string statename = "copy_" + edge->getFrom()->getName();
				State* state = new State(statename, A->alphabet->size(), A->min_domain, A->max_domain);
				newstates->insert(state->getId(), state);
				state_register.insert(edge->getFrom()->getId(), state);
			}
		}
	}
	State* sink_state = new State("@sink@", A->alphabet->size(), A->min_domain, A->max_domain);
	newstates->insert(sink_state->getId(), sink_state);

	weight_t newmin_domain = A->min_domain;
	weight_t newmax_domain = A->max_domain;

	MapArray<Weight*>* newweights = new MapArray<Weight*>(A->weights->size());
	for (unsigned int weight_id = 0; weight_id < A->weights->size(); ++weight_id) {
		newweights->insert(weight_id, new Weight(A->weights->at(weight_id)));
	}
	Weight* lowest = newweights->at(0);
	Weight* greatest = newweights->at(newweights->size()-1);

	for (unsigned int state_id = 0; state_id < A->states->size(); ++state_id) {
		for (Symbol* symbol : *(A->states->at(state_id)->getAlphabet())) {
			for (Edge* edge : *(A->states->at(state_id)->getSuccessors(symbol->getId()))) {
				Symbol* new_symbol = newalphabet->at(symbol->getId());
				State* new_from = newstates->at(edge->getFrom()->getId());
				State* new_to = newstates->at(edge->getTo()->getId());
				Weight* new_weight = newweights->at(edge->getWeight()->getId());
				Edge* new_edge = new Edge(new_symbol, new_weight, new_from, new_to);
				new_from->addSuccessor(new_edge);
				new_to->addPredecessor(new_edge);
			}
		}
	}

	for (unsigned int scc_id = 0; scc_id < A->nb_SCCs; ++scc_id) {
		if (scc_cycles[scc_id]->size() != 0) {
			for (Edge* edge : *(scc_cycles[scc_id])) {
				Symbol* new_symbol;
				State* new_from;
				State* new_to;
				Weight* new_weight = greatest;
				Edge* new_edge;

				//edge of the cycle
				new_symbol = newalphabet->at(edge->getSymbol()->getId());
				new_from = state_register.at(edge->getFrom()->getId());
				new_to = state_register.at(edge->getTo()->getId());
				new_edge = new Edge(new_symbol, new_weight, new_from, new_to);
				new_from->addSuccessor(new_edge);
				new_to->addPredecessor(new_edge);

				//edge to the sink
				for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
					new_symbol = newalphabet->at(symbol_id);
					new_from = state_register.at(edge->getFrom()->getId());
					new_to = sink_state;
					// new_edge = new Edge(new_symbol, new_weight, new_from, new_to);
					new_edge = new Edge(new_symbol, lowest, new_from, new_to);
					new_from->addSuccessor(new_edge);
					new_to->addPredecessor(new_edge);
				}

				//edge from automaton
				new_symbol = newalphabet->at(edge->getSymbol()->getId());
				new_from = newstates->at(edge->getFrom()->getId());
				new_to = state_register.at(edge->getTo()->getId());
				new_edge = new Edge(new_symbol, new_weight, new_from, new_to);
				new_from->addSuccessor(new_edge);
				new_to->addPredecessor(new_edge);
			}
		}
	}

	for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
		Symbol* new_symbol = newalphabet->at(symbol_id);
		State* new_from = sink_state;
		State* new_to = sink_state;
		Weight* new_weight = lowest;
		Edge* new_edge = new Edge(new_symbol, new_weight, new_from, new_to);
		new_from->addSuccessor(new_edge);
		new_to->addPredecessor(new_edge);
	}

	Automaton* that = new Automaton(newname, newalphabet, newstates, newweights, newmin_domain, newmax_domain, newinitial);
	Automaton* AA = copy_trim_complete(that, f);

	// AA->print();

	for (unsigned int scc_id = 0; scc_id < A->nb_SCCs; ++scc_id) {
		delete scc_cycles[scc_id];
	}

	delete that;
	return AA;
}



void explore_monotonically(
        std::pair<State*, Weight*> &from,
        SetStd<std::pair<State*, Weight*>> &set_of_states,
        SetStd<std::pair<Symbol*, std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>>> &set_of_edges,
        Weight* (*select_weight)(Weight*, Weight*)
) {
    // Mark the start node as discovered.
    if (!set_of_states.contains(from)) {
        set_of_states.insert(from);
    }

    // Explicit DFS stack over nodes (State*, Weight*).
    std::vector<std::pair<State*, Weight*>> st;
    st.push_back(from);

    while (!st.empty()) {
        const std::pair<State*, Weight*> cur = st.back();
        st.pop_back();

        State* s = cur.first;
        Weight* accw = cur.second;

        auto* alphabet = s->getAlphabet();
        if (!alphabet) continue;

        for (Symbol* symbol : *alphabet) {
            auto* succs = s->getSuccessors(symbol->getId());
            if (!succs) continue;

            for (Edge* edge : *succs) {
                State* t = edge->getTo();
                Weight* w = select_weight(accw, edge->getWeight());

                const std::pair<State*, Weight*> to(t, w);

                const auto pair_of_states =
                    std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>(cur, to);

                const auto newedge =
                    std::pair<Symbol*, std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>>(symbol, pair_of_states);

                set_of_edges.insert(newedge);

                // Discover and push if new.
                if (!set_of_states.contains(to)) {
                    set_of_states.insert(to);
                    st.push_back(to);
                }
            }
        }
    }
}

// void explore_monotonically (
// 		std::pair<State*, Weight*> &from,
// 		SetStd<std::pair<State*, Weight*>> &set_of_states,
// 		SetStd<std::pair<Symbol*, std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>>> &set_of_edges,
// 		Weight* (*select_weight)(Weight*, Weight*)
// ) {
// 	set_of_states.insert(from);

// 	for (Symbol* symbol : *((from.first)->getAlphabet())) {
// 		for (Edge* edge : *((from.first)->getSuccessors(symbol->getId()))) {
// 			State* state = edge->getTo();
// 			Weight* weight = select_weight(from.second, edge->getWeight());
// 			auto to = std::pair<State*, Weight*>(state, weight);
// 			auto pair_of_states = std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>(from, to);
// 			auto newedge = std::pair<Symbol*, std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>>(symbol, pair_of_states);
// 			set_of_edges.insert(newedge);
// 			if (set_of_states.contains(to) == false) {
// 				explore_monotonically(to, set_of_states, set_of_edges, select_weight);
// 			}
// 		}
// 	}
// }


void explore_Inf (
		std::pair<State*, Weight*> &from,
		SetStd<std::pair<State*, Weight*>> &set_of_states,
		SetStd<std::pair<Symbol*, std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>>> &set_of_edges
){
	Weight* (*select_weight)(Weight*, Weight*);
	select_weight = [] (Weight* x, Weight* y) -> Weight* {
		return ((x->getValue() < y->getValue()) ? x : y);
	};
	explore_monotonically(from, set_of_states, set_of_edges, select_weight);
}


void explore_Sup (
		std::pair<State*, Weight*> &from,
		SetStd<std::pair<State*, Weight*>> &set_of_states,
		SetStd<std::pair<Symbol*, std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>>> &set_of_edges
){
	Weight* (*select_weight)(Weight*, Weight*);
	select_weight = [] (Weight* x, Weight* y) -> Weight* {
		return ((x->getValue() < y->getValue()) ? y : x);
	};
	explore_monotonically(from, set_of_states, set_of_edges, select_weight);
}


void explore_LimInf (
		std::pair<State*, Weight*> &from,
		SetStd<std::pair<State*, Weight*>> &set_of_states,
		SetStd<std::pair<Symbol*, std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>>> &set_of_edges
) {
	set_of_states.insert(from);

	for (Symbol* symbol : *((from.first)->getAlphabet())) {
		for (Edge* edge : *((from.first)->getSuccessors(symbol->getId()))) {
			if (edge->getWeight()->getValue() >= from.second->getValue()) {
				State* state = edge->getTo();
				auto to1 = std::pair<State*, Weight*>(state, from.second);
				auto pair1 = std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>(from, to1);
				auto edge1 = std::pair<Symbol*, std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>>(symbol, pair1);
				set_of_edges.insert(edge1);
				if (set_of_states.contains(to1) == false) {
					explore_LimInf(to1, set_of_states, set_of_edges);
				}
				auto to2 = std::pair<State*, Weight*>(state, edge->getWeight());
				auto pair2 = std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>(from, to2);
				auto edge2 = std::pair<Symbol*, std::pair<std::pair<State*, Weight*>, std::pair<State*, Weight*>>>(symbol, pair2);
				set_of_edges.insert(edge2);
				if (set_of_states.contains(to2) == false) {
					explore_LimInf(to2, set_of_states, set_of_edges);
				}
			}
		}
	}
}


Automaton* Automaton::toLimSup (const Automaton* A, value_function_t f) {
	using std::pair;
	State::RESET();
	Symbol::RESET();
	Weight::RESET();

	int initWeightId;
	void (*explore)(
			pair<State*, Weight*> &from,
			SetStd<pair<State*, Weight*>> &set_of_states,
			SetStd<pair<Symbol*, pair<pair<State*, Weight*>, pair<State*, Weight*>>>> &set_of_edges
	);
	switch(f) {
	case Inf:
		explore = explore_Inf;
		initWeightId = A->getWeights()->size() - 1;
		break;
	case Sup:
		explore = explore_Sup;
		initWeightId = 0;
		break;
	case LimInf:
		explore = explore_LimInf;
		initWeightId = 0;
		break;
	case LimSup: case LimInfAvg: case LimSupAvg:
		QUAK_FAIL("invalid translation to LimSup");
	default:
		QUAK_FAIL("invalid value function"); }

	std::string newname = "LimSup(" + A->getName() + ")";

	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
	for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
		newalphabet->insert(symbol_id, new Symbol(A->alphabet->at(symbol_id)));
	}

	MapArray<Weight*>* newweights = new MapArray<Weight*>(A->weights->size());
	for (unsigned int weight_id = 0; weight_id < A->weights->size(); ++weight_id) {
		newweights->insert(weight_id, new Weight(A->weights->at(weight_id)));
	}

	weight_t newmin_domain = A->min_domain;
	weight_t newmax_domain = A->max_domain;

	SetStd<pair<State*, Weight*>> set_of_states;
	SetStd<pair<Symbol*, pair<pair<State*, Weight*>, pair<State*, Weight*>>>> set_of_edges;
	auto start = pair<State*, Weight*>(A->initial, A->weights->at(initWeightId));
	explore(start, set_of_states, set_of_edges);

	MapArray<State*>* newstates = new MapArray<State*>(set_of_states.size());
	MapStd<pair<State*, Weight*>, State*> state_register;
	for (const pair<State*, Weight*> &weighted_state : set_of_states) {
		std::string statename = "(" + weighted_state.first->getName() + ", " + std::to_string(weighted_state.second->getValue()) + ")";
		State* state = new State(statename, newalphabet->size(), newmin_domain, newmax_domain);
		if (weighted_state.first->getFinal()) {
			state->setFinal(true);
		}
		newstates->insert(state->getId(), state);
		state_register.insert(weighted_state, state);
	}
	State* newinitial = state_register.at(start);

	for (const auto &edgeA : set_of_edges) {
		Symbol* symbol = newalphabet->at(edgeA.first->getId());
    	Weight* weight = newweights->at(edgeA.second.second.second->getId());
		State* from = state_register.at(edgeA.second.first);
		State* to = state_register.at(edgeA.second.second);
		Edge *edge = new Edge(symbol, weight, from, to);
		from->addSuccessor(edge);
		to->addPredecessor(edge);
	}

	Automaton* that = new Automaton(newname, newalphabet, newstates, newweights, newmin_domain, newmax_domain, newinitial);
	// that->print();
	Automaton* AA = copy_trim_complete(that, LimSup);
	// AA->print();
	delete that;
	return AA;
}



std::vector<unsigned int> int2function (unsigned int id, unsigned int n, unsigned int m) {
	std::vector<unsigned int> func(n, 0);
	for (int i = n - 1; i >= 0; i--) {
		func[i] = id % m;
		id = id / m;
	}
	return func;
}

unsigned int function2int (std::vector<unsigned int> func, unsigned int n, unsigned int m) {
	unsigned int id = 0;
	unsigned int b = 1;
	for (int i = n - 1; i >= 0; i--) {
		id = id + func[i] * b;
		b = b * m;
	}
	return id;
}


Automaton* Automaton::determinizeInf (const Automaton* A) {
	State::RESET();
	Symbol::RESET();
	Weight::RESET();
	
	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(A->alphabet->size());
	for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
		newalphabet->insert(symbol_id, new Symbol(A->alphabet->at(symbol_id)));
	}

	weight_t top_value_of_A = A->getTopValue(Inf);
	Weight* top_weight_of_A = nullptr;
	MapArray<Weight*>* newweights = new MapArray<Weight*>(A->weights->size());
	for (unsigned int weight_id = 0; weight_id < A->weights->size(); ++weight_id) {
		Weight* weight = new Weight(A->weights->at(weight_id));
		newweights->insert(weight_id, weight);
		if (weight->getValue() == top_value_of_A) {
			top_weight_of_A = weight;
		}
	}

	weight_t newmin_domain = A->min_domain;
	weight_t newmax_domain = A->max_domain;

	unsigned int n = A->states->size();
	unsigned int m = A->weights->size();

	unsigned int size = 1;
	for (unsigned int state_id = 0; state_id < n; ++state_id) {
		size = size * m;
	}

	MapArray<State*>* newstates = new MapArray<State*>(size); // each state represents a function from states of A to weightIds of A
	for (unsigned int state_id = 0; state_id < size; ++state_id) {
		std::string statename = "(";
		std::vector<unsigned int> funcTemp = int2function(state_id, n, m);
		for (unsigned int i = 0; i < n - 1; i++) {
			statename += std::to_string(funcTemp[i]) + ",";
		}
		statename += std::to_string(funcTemp[n-1]) + ")";
		State* state = new State(statename, newalphabet->size(), newmin_domain, newmax_domain);
		newstates->insert(state->getId(), state);
	}

	std::vector<unsigned int> funcFrom(n, 0);
	funcFrom[A->getInitial()->getId()] = top_weight_of_A->getId();
	unsigned int initIndex = function2int(funcFrom, n, m);
	State* newinitial = newstates->at(initIndex);

	for (unsigned int state_id = 0; state_id < size; ++state_id) {
		funcFrom = int2function(state_id, n, m);

		for (unsigned int symbol_id = 0; symbol_id < A->alphabet->size(); ++symbol_id) {
			std::vector<unsigned int> funcTo(n, 0);
			for (unsigned int to_id = 0; to_id < n; to_id++) {
				for (Edge* edge : *(A->states->at(to_id)->getPredecessors(symbol_id))) {
					unsigned int from_id = edge->getFrom()->getId();
					unsigned int x = funcFrom[from_id];
					unsigned int y = edge->getWeight()->getId();
					unsigned int z = funcTo[to_id];
					funcTo[to_id] = std::max(z, std::min(x, y)); // this is fine because weights are ordered
				}
			}

			unsigned int new_to_id = function2int(funcTo, n, m);
			State* from = newstates->at(state_id);
			State* to = newstates->at(new_to_id);

			unsigned int weight_id = 0;
			for (unsigned int i = 0; i < n; i++) {
				weight_id = std::max(weight_id, funcFrom[i]);
			}
			
			Weight* w = newweights->at(weight_id);
			Edge* newedge = new Edge(newalphabet->at(symbol_id), w, from, to);
			from->addSuccessor(newedge);
			to->addPredecessor(newedge);
		}
	}


	std::string newname = "Determinized(" + A->getName() + ")";
	Automaton* that = new Automaton(newname, newalphabet, newstates, newweights, newmin_domain, newmax_domain, newinitial);
	Automaton* AA = copy_trim_complete(that, Inf);
	delete that;
	return AA;
}


// Decision procedures



bool Automaton::isDeterministic () const {
	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
			if (1 < this->states->at(state_id)->getSuccessors(symbol_id)->size()) return false;
		}
	}
	return true;
}

bool Automaton::isNonEmpty (value_function_t f, weight_t x, UltimatelyPeriodicWord** witness) {
	return (getTopValue(f, witness) >= x);
}


bool Automaton::isUniversal (value_function_t f, weight_t x, UltimatelyPeriodicWord** witness)  {
	Automaton* C = Automaton::constantAutomaton(this, x);
	bool flag = C->isIncludedIn(this, f, false, witness);
	delete C;
	return flag;
}

bool Automaton::isUniversal_withFinal (value_function_t f, weight_t x, UltimatelyPeriodicWord** witness) {
	Automaton* C = acceptedLanguageConstantAutomaton(this, x);
	bool flag = C->isIncludedIn(this, f, false, witness);
	delete C;
	return flag;
}

bool Automaton::isComplete () const {
	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
			if (1 > this->states->at(state_id)->getSuccessors(symbol_id)->size()) return false;
		}
	}
	return true;
}


bool Automaton::isLimAvgConstant(UltimatelyPeriodicWord** witness) const {
	weight_t top = getTopValue(LimSupAvg); //top of LimSupAvg and LimInfAvg coincide

    weight_t dist[this->getStates()->size()];
    for (unsigned int state_id = 0; state_id < this->getStates()->size(); ++state_id) {
        dist[state_id] = 0;
    }

	for (unsigned int len = 0; len < this->getStates()->size(); len++) {
		for (unsigned int state_id = 0; state_id < this->getStates()->size(); ++state_id) {
			for (Symbol* symbol : *(this->getStates()->at(state_id)->getAlphabet())) {
				for (Edge* edge : *(this->getStates()->at(state_id)->getSuccessors(symbol->getId()))) {
					unsigned int u = edge->getFrom()->getId();
					unsigned int v = edge->getTo()->getId();
					//weights are inversed AND shifted by 'top': -(edge-top) = top-edge
					weight_t value = top - edge->getWeight()->getValue();
					weight_t du = dist[u];
					if (du + value < dist[v]) {
						dist[v] = (du + value);
					}
				}
			}
		}
	}

	State::RESET();
	Symbol::RESET();
	Weight::RESET();

	std::string newname = "Dist(" + this->getName() + ")";

	MapArray<Symbol*>* newalphabet = new MapArray<Symbol*>(this->alphabet->size());
	for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
		newalphabet->insert(symbol_id, new Symbol(this->alphabet->at(symbol_id)));
	}

	MapArray<State*>* newstates = new MapArray<State*>(this->states->size());
	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		newstates->insert(state_id, new State(this->states->at(state_id)));
	}
	State* newinitial = newstates->at(this->initial->getId());

	bool weightsSeen[2];
	weightsSeen[0] = false;
	weightsSeen[1] = false;
	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		for (Symbol* symbol : *(this->states->at(state_id)->getAlphabet())) {
			for (Edge* edge : *(this->states->at(state_id)->getSuccessors(symbol->getId()))) {
				unsigned int u = edge->getFrom()->getId();
				unsigned int v = edge->getTo()->getId();
				weight_t x = (edge->getWeight()->getValue() - dist[u] + dist[v]);
			}
		}
	}

	MapArray<Weight*>* newweights = new MapArray<Weight*>(2);
	for (unsigned int weight_id = 0; weight_id < 2; ++weight_id) {
		newweights->insert(weight_id, new Weight(weight_id));
	}

	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		for (Symbol* symbol : *(this->states->at(state_id)->getAlphabet())) {
			for (Edge* edge : *(this->states->at(state_id)->getSuccessors(symbol->getId()))) {
				unsigned int u = edge->getFrom()->getId();
				unsigned int v = edge->getTo()->getId();
				//originally: -(edge-top) + from - to = 0 (<=?)
				//inverted weights: (edge-top) - from + to = 0
				//equivalently: edge - from + to = top
				weight_t x = (edge->getWeight()->getValue() - dist[u] + dist[v]);
				auto value = ((x == top) ? 1 : 0);
				Weight* weight = newweights->at(value);
				State* from = newstates->at(edge->getFrom()->getId());
				State* to = newstates->at(edge->getTo()->getId());
				Edge* newedge = new Edge(newalphabet->at(symbol->getId()), weight, from, to);
				newstates->at(state_id)->addSuccessor(newedge);
				newstates->at(edge->getTo()->getId())->addPredecessor(newedge);
			}
		}
	}

	Automaton* Dist = new Automaton(newname, newalphabet, newstates, newweights, 0, 1, newinitial);
	// Dist->print();
	bool out = Dist->isUniversal(LimInf, 1, witness);
	delete Dist;
	return out;
}

// witness for isConstant is a lasso word whose value is strictly less than the automaton's top value
bool Automaton::isConstant (value_function_t f, UltimatelyPeriodicWord** witness) {
	if (isDeterministic() == true) {
		return (getTopValue(f) == getBottomValue(f, witness));
	}
	else if ((f == LimSupAvg || f == LimInfAvg)) {
		return isLimAvgConstant(witness);
	}
	else {
		return isUniversal(f, getTopValue(f), witness);
	}
}

bool Automaton::isEquivalentTo (const Automaton* B, value_function_t f, bool booleanized, UltimatelyPeriodicWord** witness) const {
	return this->isIncludedIn(B, f, booleanized, witness) && B->isIncludedIn(this, f, booleanized, witness);
}

bool Automaton::isIncludedIn(const Automaton* B, value_function_t f, bool booleanized, UltimatelyPeriodicWord** witness) const {
    assert(alphabetsAreCompatible(B) && "Incompatible alphabets");

    if (booleanized == true) {
        return isIncludedIn_booleanized(B, f, witness);
    }

    return isIncludedIn_antichains(B, f, witness);
}

bool Automaton::isIncludedIn_antichains(const Automaton* B, value_function_t f, UltimatelyPeriodicWord** witness) const {
	if (f == LimSupAvg || f == LimInfAvg) {
		if (B->isDeterministic()) {
			Automaton* C = Automaton::product(this, Minus, B);
			weight_t Ctop = C->getTopValue(f, witness);
			delete C;
			return (Ctop <= 0);
		}
		else {
			QUAK_FAIL("automata inclusion undecidable for nondeterministic limavg");
		}
	}
	else if (f == Inf || f == Sup || f == LimInf || f == LimSup) {
		bool flag;

		if (f == LimSup) {
			flag = inclusion(this, B, witness);
		}
		else {
			Automaton* AA = Automaton::toLimSup(this, f);
			Automaton* BB = Automaton::toLimSup(B, f);
			// AA->print();
			// BB->print();

			flag = inclusion(AA, BB, witness);
			delete AA;
			delete BB;
		}

		return flag;
	}
	else {
		QUAK_FAIL("automata inclusion type");
	}
}

bool Automaton::isIncludedIn_booleanized(const Automaton* B, value_function_t f, UltimatelyPeriodicWord** witness) const {
    // unique_ptr to keep memory in case we create new limSup automata
    std::unique_ptr<Automaton> limSupThisMem, limSupBMem;
    const Automaton *limSupThis{nullptr}, *limSupB{nullptr};

    if (f == LimSup) {
      limSupThis = this;
      limSupB = B;
    } else {
      limSupThisMem = std::unique_ptr<Automaton>(Automaton::toLimSup(this, f));
      limSupBMem = std::unique_ptr<Automaton>(Automaton::toLimSup(B, f));
      limSupThis = limSupThisMem.get();
      limSupB = limSupBMem.get();
    }

    for (auto *weight: *weights) {
        auto boolA = std::unique_ptr<Automaton>(booleanize(limSupThis, weight->getValue()));
        auto boolB = std::unique_ptr<Automaton>(booleanize(limSupB, weight->getValue()));

        if (!inclusion(boolA.get(), boolB.get(), witness)) {
          return false;
        }
    }

    return true;
}


bool Automaton::isSafe (value_function_t f, UltimatelyPeriodicWord** witness) {
	if (f == Inf) {
		return true;
	}

	Automaton* S = Automaton::safetyClosure(this, f);
	bool out;

	if ((f == LimSupAvg || f== LimInfAvg) && !this->isDeterministic()) {
		Automaton* SS = Automaton::determinizeInf(S);
		Automaton* C = Automaton::product(this, Minus, SS);
		out = C->isLimAvgConstant(witness);
		delete SS;
		delete C;
	}
	else {
		out = S->isIncludedIn(this, f, false, witness);
	}

	delete S;
	return out;
}

bool Automaton::isLive (value_function_t f, UltimatelyPeriodicWord** witness) {
	bool out;

	if (f == Inf) {
		return this->isConstant(Inf, witness);
	}

	Automaton* S = Automaton::safetyClosure(this, f);
	out = S->isConstant(f, witness);
	delete S;
	return out;
}










// Top value computation

void Automaton::top_dag (SCC_Dag* dag, bool* done, weight_t* top_values) const {
	if (done[dag->origin->getTag()] == true) return;
	done[dag->origin->getTag()] = true;

	for (SCC_Dag* subdag : *(dag->nexts)) {
		top_dag(subdag, done, top_values);
		top_values[dag->origin->getTag()] = std::max(top_values[dag->origin->getTag()],
				top_values[subdag->origin->getTag()]);
	}
}

void Automaton::top_reachably_scc_new(State* startState, bool in_scc, std::vector<bool>& spot, std::vector<weight_t>& values) const {
    
    struct Frame {
        State* state;
        std::vector<Edge*> successors;
        size_t succIndex;
        bool initialized;
    };
    
    auto collectSuccessors = [](State* s) {
        std::vector<Edge*> result;
        for (Symbol* symbol : *(s->getAlphabet())) {
            for (Edge* edge : *(s->getSuccessors(symbol->getId()))) {
                result.push_back(edge);
            }
        }
        return result;
    };
    
    std::vector<Frame> callStack;
    callStack.push_back({startState, {}, 0, false});
    
    while (!callStack.empty()) {
        Frame& frame = callStack.back();
        State* state = frame.state;
        
        // Phase 1: Initialize
        if (!frame.initialized) {
            if (spot[state->getId()]) {
                callStack.pop_back();
                continue;
            }
            spot[state->getId()] = true;
            
            frame.successors = collectSuccessors(state);
            frame.succIndex = 0;
            frame.initialized = true;
        }
        
        // Phase 2: Process successors
        bool pushedChild = false;
        while (frame.succIndex < frame.successors.size()) {
            Edge* edge = frame.successors[frame.succIndex];
            State* child = edge->getTo();
            
            if (child->getTag() == state->getTag()) {
                // Same SCC
                if (!spot[child->getId()]) {
                    // Need to recurse - don't advance index yet
                    // We'll process this edge again after child returns
                    callStack.push_back({child, {}, 0, false});
                    pushedChild = true;
                    break;
                } 
                else {
                    // Child already visited - do the value updates
                    values[state->getId()] = std::max(values[state->getId()], edge->getWeight()->getValue());
                    values[state->getId()] = std::max(values[state->getId()], values[child->getId()]);
                }
            }
            else if (!in_scc) {
                // Different SCC and not in_scc mode
                values[state->getId()] = std::max(values[state->getId()], edge->getWeight()->getValue());
            }
            frame.succIndex++;
        }
        
        if (pushedChild) {
            continue;
        }
        
        // Phase 3: All successors done, pop frame
        callStack.pop_back();
    }
}
void Automaton::top_reachably_scc(State* startState, bool in_scc, bool* spot, weight_t* values) const {
    
    struct Frame {
        State* state;
        std::vector<Edge*> successors;
        size_t succIndex;
        bool initialized;
    };
    
    auto collectSuccessors = [](State* s) {
        std::vector<Edge*> result;
        for (Symbol* symbol : *(s->getAlphabet())) {
            for (Edge* edge : *(s->getSuccessors(symbol->getId()))) {
                result.push_back(edge);
            }
        }
        return result;
    };
    
    std::vector<Frame> callStack;
    callStack.push_back({startState, {}, 0, false});
    
    while (!callStack.empty()) {
        Frame& frame = callStack.back();
        State* state = frame.state;
        
        // Phase 1: Initialize
        if (!frame.initialized) {
            if (spot[state->getId()] == true) {
                callStack.pop_back();
                continue;
            }
            spot[state->getId()] = true;
            
            frame.successors = collectSuccessors(state);
            frame.succIndex = 0;
            frame.initialized = true;
        }
        
        // Phase 2: Process successors
        bool pushedChild = false;
        while (frame.succIndex < frame.successors.size()) {
            Edge* edge = frame.successors[frame.succIndex];
            State* child = edge->getTo();
            
            if (child->getTag() == state->getTag()) {
                // Same SCC
                if (spot[child->getId()] != true) {
                    // Need to recurse - don't advance index yet
                    // We'll process this edge again after child returns
                    callStack.push_back({child, {}, 0, false});
                    pushedChild = true;
                    break;
                } 
                else {
                    // Child already visited - do the value updates
                    values[state->getId()] = std::max(values[state->getId()], edge->getWeight()->getValue());
                    values[state->getId()] = std::max(values[state->getId()], values[child->getId()]);
                }
            }
            else if (in_scc == false) {
                // Different SCC and not in_scc mode
                values[state->getId()] = std::max(values[state->getId()], edge->getWeight()->getValue());
            }
            frame.succIndex++;
        }
        
        if (pushedChild) {
            continue;
        }
        
        // Phase 3: All successors done, pop frame
        callStack.pop_back();
    }
}

// void Automaton::top_reachably_scc (State* state, bool in_scc, bool* spot, weight_t* values) const {
// 	if (spot[state->getId()] == true) return;
// 	spot[state->getId()] = true;
// 	for (Symbol* symbol : *(state->getAlphabet())) {
// 		for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
// 			if (edge->getTo()->getTag() == state->getTag()) {
// 				top_reachably_scc(edge->getTo(), in_scc, spot, values);
// 				values[state->getId()] = std::max(values[state->getId()], edge->getWeight()->getValue());
// 				values[state->getId()] = std::max(values[state->getId()], values[edge->getTo()->getId()]);
// 			}
// 			else if (in_scc == false) {
// 				values[state->getId()] = std::max(values[state->getId()], edge->getWeight()->getValue());
// 			}
// 		}
// 	}
// }


weight_t Automaton::top_reachably (bool in_scc, weight_t* values, weight_t* top_values) const {
	bool spot[this->states->size()];
	bool done[this->nb_SCCs];

	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		values[state_id] = this->min_domain;
		spot[state_id] = false;
	}

	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		top_reachably_scc(this->SCCs[scc_id]->origin, in_scc, spot, values);
		done[scc_id] = false;
		top_values[scc_id] = values[this->SCCs[scc_id]->origin->getId()];
	}

	top_dag(this->SCCs[this->initial->getTag()], done, top_values);

	return top_values[this->initial->getTag()];
}


weight_t Automaton::top_Sup (weight_t* top_values) const {
	weight_t values[this->states->size()];
	return top_reachably(false, values, top_values);
}


weight_t Automaton::top_LimSup (weight_t* top_values) const {
	weight_t values[this->states->size()];
	return top_reachably(true, values, top_values);
}


void Automaton::top_safety_scc_recursive (Edge* edge, SetStd<Edge*>* done_edge, bool in_scc, int* done_symbol, weight_t* values, weight_t** value_symbol, int** counters) const {
	if (done_edge->contains(edge) == true) return;
	done_edge->insert(edge);

	unsigned int state_id = edge->getFrom()->getId();
	unsigned int symbol_id = edge->getSymbol()->getId();

	//printf("edge = %s\n", edge->toString().c_str());

	counters[state_id][symbol_id]--;
	if (counters[state_id][symbol_id] == 0) {
		/*printf("state %s done for %s\n",
				edge->getFrom()->getName().c_str(),
				edge->getSymbol()->getName().c_str()
		);*/

		for (Edge* succ : *(edge->getFrom()->getSuccessors(symbol_id))) {
			if (in_scc == false || succ->getFrom()->getTag() == succ->getTo()->getTag()) {
				weight_t tmp = std::min(succ->getWeight()->getValue(), values[succ->getTo()->getId()]);
				value_symbol[state_id][symbol_id] = std::max(value_symbol[state_id][symbol_id], tmp);
			}
		}
		done_symbol[state_id]--;

		/*printf("value_symbol[%s][%s] = %f\n",
				edge->getFrom()->getName().c_str(),
				edge->getSymbol()->getName().c_str(),
				value_symbol[state_id][symbol_id]
		);*/


		if (done_symbol[state_id] == 0) {
			//printf("state %s done\n", edge->getFrom()->toString().c_str());

			values[state_id] = value_symbol[state_id][symbol_id];
			for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
				values[state_id] = std::max(values[state_id], value_symbol[state_id][symbol_id]);
			}

			/*printf("values[%s] = %f\n",
					edge->getFrom()->getName().c_str(),
					values[state_id]
			);*/

			for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
				for (Edge* pred : *(edge->getFrom()->getPredecessors(symbol_id))) {
					if (in_scc == false || pred->getFrom()->getTag() == pred->getTo()->getTag()) {
						top_safety_scc_recursive(pred, done_edge, in_scc, done_symbol, values, value_symbol, counters);
					}
				}
			}
		}
	}
};


void Automaton::top_safety_scc (weight_t* values, bool in_scc) const {
	//O(x)
	MapArray<SetList<Edge*>*> edges(this->weights->size());
	for (unsigned int weight_id = 0; weight_id < this->weights->size(); ++weight_id) {
		edges.insert(weight_id, new SetList<Edge*>);
	}

	//O(m+n)
	int done_symbol[this->states->size()];
	int* counters[this->states->size()];
	weight_t* value_symbol[this->states->size()];
	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		counters[state_id] = new int[this->alphabet->size()];
		value_symbol[state_id] = new weight_t[this->alphabet->size()];
		done_symbol[state_id] = 0;
		for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
			value_symbol[state_id][symbol_id] = this->min_domain;
			counters[state_id][symbol_id] = 0;

			bool flag = false;
			for (Edge* edge : *(this->states->at(state_id)->getSuccessors(symbol_id))) {
				if (in_scc == false || edge->getFrom()->getTag() == edge->getTo()->getTag()){
					edges.at(edge->getWeight()->getId())->push(edge);
					counters[state_id][symbol_id]++;
					flag = true;
				}
			}
			if (flag == true) done_symbol[state_id]++;
		}
		if (done_symbol[state_id] == 0) {
			values[state_id] = this->min_domain;
		}
		else {
			values[state_id] = this->max_domain;
		}
	}

	//O((x+m)) because 'top_safety_scc_recursive' is called 2m times overall
	SetStd<Edge*> done_edge;
	for (unsigned int weight_id = 0; weight_id < this->weights->size(); ++weight_id) {
		while (edges.at(weight_id)->size() > 0) {
			Edge* edge = edges.at(weight_id)->head();
			edges.at(weight_id)->pop();
			top_safety_scc_recursive(edge, &done_edge, in_scc, done_symbol, values, value_symbol, counters);
		}
	}

	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		delete[] counters[state_id];
		delete[] value_symbol[state_id];
	}

	for (unsigned int weight_id = 0; weight_id < this->weights->size(); ++weight_id) {
		delete edges.at(weight_id);
	}


	/*for (unsigned int id=0; id < this->states->size(); ++id) {
		printf("values[%u]=%s\n", id, std::to_string(values[id]).c_str());
	}*/
}


/*
weight_t Automaton::top_safety (bool in_scc, weight_t* values, weight_t* top_values) const {
	bool done[this->nb_SCCs];
	top_safety_scc(values, in_scc);

	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		done[scc_id] = false;
		top_values[scc_id] = values[this->SCCs[scc_id]->origin->getId()];
	}
	top_dag(this->SCCs[this->initial->getTag()], done, top_values);
	return top_values[this->initial->getTag()];
}
*/

weight_t Automaton::top_Inf (weight_t* top_values) const {
	weight_t values[this->states->size()];
	// top_safety_scc(values, true);
	top_safety_scc(values, false);

	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		top_values[scc_id] = values[this->SCCs[scc_id]->origin->getId()];
	}
	
	return values[this->initial->getId()];
}


// weight_t Automaton::top_Inf (weight_t* top_values) const {
// 	weight_t values[this->states->size()];
// 	bool done[this->nb_SCCs];
// 	top_safety_scc(values, true);

// 	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
// 		done[scc_id] = false;
// 		int x = this->SCCs[scc_id]->origin->getId();
// 		top_values[scc_id] = values[this->SCCs[scc_id]->origin->getId()];
// 	}

// 	top_dag(this->SCCs[this->initial->getTag()], done, top_values);
// 	return top_values[this->initial->getTag()];
// }


weight_t Automaton::top_LimInf (weight_t* top_values) const {
	weight_t values[this->states->size()];
	bool done[this->nb_SCCs];
	// top_safety_scc(values, false);
	top_safety_scc(values, true);

	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		done[scc_id] = false;
		top_values[scc_id] = this->min_domain;
	}

	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		if (this->states->at(state_id)->getTag() > -1) {
			top_values[this->states->at(state_id)->getTag()] = std::max(
							top_values[this->states->at(state_id)->getTag()],
							values[state_id]
					);
		}
	}

	top_dag(this->SCCs[this->initial->getTag()], done, top_values);
	return top_values[this->initial->getTag()];
}



weight_t Automaton::top_LimAvg(weight_t* top_values) const {
    unsigned int size = this->states->size();
    if (size == 0) return this->min_domain; // avoids 0-sized matrix edge cases

    weight_t infinity = std::max(weight_t(1), -(weight_t(size) * this->min_domain) + 1);

    // HEAP allocation instead of stack VLA:
    std::unique_ptr<weight_t[]> distance(new weight_t[(size + 1) * size]);

    auto D = [&](unsigned int len, unsigned int state_id) -> weight_t& {
        return distance[len * size + state_id];
    };

    // O(n)
    for (unsigned int length = 0; length <= size; ++length) {
        for (unsigned int state_id = 0; state_id < size; ++state_id) {
            D(length, state_id) = infinity;
        }
    }

    // O(n)
    auto initialize_distances = [] (SCC_Dag* dag, weight_t* dist_row, auto &rec) -> void {
        dist_row[dag->origin->getId()] = 0;
        for (auto iter = dag->nexts->begin(); iter != dag->nexts->end(); ++iter) {
            rec(*iter, dist_row, rec);
        }
    };
    initialize_distances(this->SCCs[this->initial->getTag()], distance.get() + 0 * size, initialize_distances);

    // O(n.m)
    for (unsigned int len = 1; len <= size; ++len) {
        for (unsigned int state_id = 0; state_id < size; ++state_id) {
            for (Symbol* symbol : *(states->at(state_id)->getAlphabet())) {
                for (Edge* edge : *(states->at(state_id)->getSuccessors(symbol->getId()))) {
                    if (edge->getFrom()->getTag() == edge->getTo()->getTag()) {
                        auto from = edge->getFrom()->getId();
                        auto to   = edge->getTo()->getId();
                        if (D(len - 1, from) != infinity) {
                            weight_t value = D(len - 1, from) - edge->getWeight()->getValue();
                            if (D(len, to) == infinity) D(len, to) = value;
                            else D(len, to) = std::min(value, D(len, to));
                        }
                    }
                }
            }
        }
    }

    // Also avoid stack VLA here:
    std::unique_ptr<bool[]> done(new bool[this->nb_SCCs]);

    for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
        done[scc_id] = false;
        top_values[scc_id] = this->min_domain;
    }

    for (unsigned int state_id = 0; state_id < size; ++state_id) {
        weight_t min_lenght_avg = this->max_domain;
        bool len_flag = false;

        if (D(size, state_id) != infinity) {
            for (unsigned int lenght = 0; lenght < size; ++lenght) {
                if (D(lenght, state_id) != infinity) {
                    weight_t avg =
                        (D(lenght, state_id) - D(size, state_id) + 0.0) /
                        weight_t(size - lenght + 0.0);
                    min_lenght_avg = std::min(min_lenght_avg, avg);
                    len_flag = true;
                }
            }
        }

        if (len_flag) {
            top_values[this->states->at(state_id)->getTag()] =
                std::max(top_values[this->states->at(state_id)->getTag()], min_lenght_avg);
        }
    }

    top_dag(this->SCCs[this->initial->getTag()], done.get(), top_values);
    return top_values[this->initial->getTag()];
}

// weight_t Automaton::top_LimAvg (weight_t* top_values) const {
// 	unsigned int size = this->states->size();
// 	weight_t distance[size + 1][size];
// 	weight_t infinity = std::max(weight_t(1), -(weight_t(size)*this->min_domain) + 1); // TODO

// 	// O(n)
// 	for (unsigned int length = 0; length <= size; ++length) {
// 		for (unsigned int state_id = 0; state_id < size; ++state_id) {
// 			distance[length][state_id] = infinity;
// 		}
// 	}


// 	//O(n)
// 	auto initialize_distances = [] (SCC_Dag* dag, weight_t* distance, auto &rec) -> void {
// 		distance[dag->origin->getId()] = 0;
// 		for (auto iter = dag->nexts->begin(); iter != dag->nexts->end(); ++iter) {
// 			rec(*iter, distance, rec);
// 		}
// 	};
// 	initialize_distances(this->SCCs[this->initial->getTag()], distance[0], initialize_distances);


// 	// O(n.m)
// 	for (unsigned int len = 1; len <= size; ++len) {
// 		for (unsigned int state_id = 0; state_id < size; ++state_id)	{
// 			for (Symbol* symbol : *(states->at(state_id)->getAlphabet())) {
// 				for (Edge* edge : *(states->at(state_id)->getSuccessors(symbol->getId()))) {
// 					if (edge->getFrom()->getTag() == edge->getTo()->getTag()) {
// 						if (distance[len-1][edge->getFrom()->getId()] != infinity) {
// 							weight_t value = distance[len-1][edge->getFrom()->getId()] - edge->getWeight()->getValue();
// 							if (distance[len][edge->getTo()->getId()] == infinity) {
// 								distance[len][edge->getTo()->getId()] = value;
// 							}
// 							else {
// 								distance[len][edge->getTo()->getId()] =
// 										std::min(value, distance[len][edge->getTo()->getId()]);
// 							}
// 						}
// 					}
// 				}
// 			}
// 		}
// 	}

// 	//O(n.m)
// 	bool done[this->nb_SCCs];
// 	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
// 		done[scc_id] = false;
// 		top_values[scc_id] = this->min_domain;
// 	}

// 	for (unsigned int state_id = 0; state_id < size; ++state_id) {
// 		weight_t min_lenght_avg = this->max_domain;
// 		bool len_flag = false;
// 		if (distance[size][state_id] != infinity) { // => id has an ongoing edge (inside its SCC)
// 			for (unsigned int lenght = 0; lenght < size; ++lenght) { // hence the nested loop is call at most O(m) times
// 				if (distance[lenght][state_id] != infinity) {
// 					weight_t avg = (distance[lenght][state_id] - distance[size][state_id] + 0.0) / weight_t(size - lenght + 0.0);
// 					min_lenght_avg = std::min(min_lenght_avg, avg);
// 					len_flag = true;
// 				}
// 			}
// 		}
// 		if (len_flag) {
// 			top_values[this->states->at(state_id)->getTag()]
// 				  = std::max(top_values[this->states->at(state_id)->getTag()], min_lenght_avg);
// 		}
// 	}

// 	top_dag(this->SCCs[this->initial->getTag()], done, top_values);
// 	return top_values[this->initial->getTag()];
// }



weight_t Automaton::top_LimAvg_cycles (weight_t* top_values, SetList<Edge*>** scc_cycles, UltimatelyPeriodicWord** witness) const {
	unsigned int size = this->states->size();
	Edge* back_distance[size + 1][size];
	weight_t distance[size + 1][size];
	weight_t infinity = std::max(weight_t(1), -(this->min_domain*weight_t(size)) + 1);

	// O(n)
	for (unsigned int length = 0; length <= size; ++length) {
		for (unsigned int state_id = 0; state_id < size; ++state_id) {
			distance[length][state_id] = infinity;
			back_distance[length][state_id] = nullptr;
		}
	}


	//O(n)
	auto initialize_distances = [] (SCC_Dag* dag, weight_t* distance, auto &rec) -> void {
		distance[dag->origin->getId()] = 0;
		for (auto iter = dag->nexts->begin(); iter != dag->nexts->end(); ++iter) {
			rec(*iter, distance, rec);
		}
	};
	initialize_distances(this->SCCs[this->initial->getTag()], distance[0], initialize_distances);


	// O(n.m)
	for (unsigned int len = 1; len <= size; ++len) {
		for (unsigned int state_id = 0; state_id < size; ++state_id)	{
			for (Symbol* symbol : *(states->at(state_id)->getAlphabet())) {
				for (Edge* edge : *(states->at(state_id)->getSuccessors(symbol->getId()))) {
					if (edge->getFrom()->getTag() == edge->getTo()->getTag()) {
						if (distance[len-1][edge->getFrom()->getId()] != infinity) {
							weight_t value = distance[len-1][edge->getFrom()->getId()] - edge->getWeight()->getValue();
							if (distance[len][edge->getTo()->getId()] == infinity) {
								distance[len][edge->getTo()->getId()] = value;
								back_distance[len][edge->getTo()->getId()] = edge;
							}
							else {
								// distance[len][edge->getTo()->getId()] =
								// 		std::min(value, distance[len][edge->getTo()->getId()]);
								if (value < distance[len][edge->getTo()->getId()]) {
									distance[len][edge->getTo()->getId()] = value;
									back_distance[len][edge->getTo()->getId()] = edge;
								}
							}

							// weight_t old_value = distance[len][edge->getTo()->getId()];
							// weight_t new_value = distance[len-1][edge->getFrom()->getId()] - edge->getWeight()->getValue();
							// if (old_value == infinity || new_value < old_value) {
							// 	distance[len][edge->getTo()->getId()] = new_value;
							// 	back_distance[len][edge->getTo()->getId()] = edge;
							// }
						}
					}
				}
			}
		}
	}

	//O(n.m)
	bool done[this->nb_SCCs];
	State* scc_back[this->nb_SCCs];
	weight_t scc_values[this->nb_SCCs];
	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		done[scc_id] = false;
		top_values[scc_id] = this->min_domain;
		scc_values[scc_id] = this->min_domain;
		scc_back[scc_id] = nullptr;
		scc_cycles[scc_id] = new SetList<Edge*>();
	}

	for (unsigned int state_id = 0; state_id < size; ++state_id) {
		weight_t min_lenght_avg = this->max_domain;
		bool len_flag = false;
		if (distance[size][state_id] != infinity) { // => id has an ongoing edge (inside its SCC)
			for (unsigned int length = 0; length < size; ++length) { // hence the nested loop is call at most O(m) times
				if (distance[length][state_id] != infinity) {
					weight_t avg = (distance[length][state_id] - distance[size][state_id] + 0.0) / (size - length + 0.0);
					min_lenght_avg = std::min(min_lenght_avg, avg);
					len_flag = true;
				}
			}
		}
		if (len_flag && top_values[this->states->at(state_id)->getTag()] <= min_lenght_avg) {
			top_values[this->states->at(state_id)->getTag()] = min_lenght_avg;
			scc_values[this->states->at(state_id)->getTag()] = min_lenght_avg;
			scc_back[this->states->at(state_id)->getTag()] = this->states->at(state_id);
		}
	}

	top_dag(this->SCCs[this->initial->getTag()], done, top_values);

	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		if (scc_values[scc_id] == top_values[scc_id]){
			
			bool spot[this->states->size()];
			
			for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
				spot[state_id] = false;
			}
			
			State* seek_state = scc_back[scc_id];
			int length = this->states->size();
			while(seek_state != nullptr && spot[seek_state->getId()] == false) {
				spot[seek_state->getId()] = true;
				seek_state = back_distance[length][seek_state->getId()]->getFrom();
				length--;
			}

			if (seek_state != nullptr) {
				State* state = scc_back[scc_id];
				length = this->states->size();
				while (state != seek_state) {
					state = back_distance[length][state->getId()]->getFrom();
					length--;
				}
				do {
					scc_cycles[scc_id]->push(back_distance[length][state->getId()]);
					state = back_distance[length][state->getId()]->getFrom();
					length--;
				} while (state != seek_state);
			}
		}
	}

	if (witness != nullptr) {
		constructWitness(LimInfAvg, witness, scc_values, top_values, scc_cycles, nullptr, nullptr);
	}

	return top_values[this->initial->getTag()];
}





bool Automaton::top_cycles_explore (State* target, State* state, bool* spot, weight_t (*filter)(weight_t,weight_t), weight_t* top_values, SetList<Edge*>** scc_cycles) const {
	if (spot[state->getId()] == true) return false;
	if (state == target) return true;
	
	spot[state->getId()] = true;
	for (Symbol* symbol : *(state->getAlphabet())) {
		for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
			if (edge->getFrom()->getTag() == edge->getTo()->getTag()) {
				weight_t value = filter(edge->getWeight()->getValue(), top_values[state->getTag()]);
				if (value == top_values[state->getTag()]) {
					if (top_cycles_explore(target, edge->getTo(), spot, filter, top_values, scc_cycles) == true) {
						scc_cycles[state->getTag()]->push(edge);
						return true;
					}
				}
			}
		}
	}
	return false;
}
void Automaton::top_cycles (weight_t (*filter)(weight_t,weight_t), weight_t* scc_values, weight_t* top_values, SetList<Edge*>** scc_cycles) const {
	bool spot[this->states->size()];
	
	for (unsigned int scc_id = 0 ; scc_id < this->nb_SCCs; ++scc_id) {
		scc_cycles[scc_id] = new SetList<Edge*>();
	}
	
	for (unsigned int state_id = 0 ; state_id < this->states->size(); ++state_id) {
		State* state = this->states->at(state_id);
		
		if (state->getTag() == -1) continue; // state not reachable
		if (scc_values[state->getId()] != top_values[state->getTag()]) continue; // top not held by SCC
		if (scc_cycles[state->getTag()]->size() != 0) continue; // cycle already computed
		
		for (Symbol* symbol : *(state->getAlphabet())) {
			for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
				if (edge->getFrom()->getTag() == edge->getTo()->getTag() && edge->getWeight()->getValue() == top_values[edge->getTo()->getTag()]) {
					for (unsigned int state_id = 0 ; state_id < this->states->size(); ++state_id) {
						spot[state_id] = false;
					}
					if (top_cycles_explore(edge->getFrom(), edge->getTo(), spot, filter, top_values, scc_cycles) == true) {
						scc_cycles[edge->getFrom()->getTag()]->push(edge);
					}
				}
			}
		}
	}

	return;
}


weight_t Automaton::top_LimInf_cycles (weight_t* top_values, SetList<Edge*>** scc_cycles, UltimatelyPeriodicWord** witness) const {
	weight_t (*filter)(weight_t,weight_t) = [] (weight_t x, weight_t y) -> weight_t {
		return std::min(x, y);
	};
	weight_t scc_values[this->states->size()];

	bool done[this->nb_SCCs];
	top_safety_scc(scc_values, true);

	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		done[scc_id] = false;
		top_values[scc_id] = this->min_domain;
	}

	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
		top_values[this->states->at(state_id)->getTag()] = std::max(
				top_values[this->states->at(state_id)->getTag()],
				scc_values[state_id]
		);
	}

	top_dag(this->SCCs[this->initial->getTag()], done, top_values);
	top_cycles(filter, scc_values, top_values, scc_cycles);

	if (witness != nullptr) {
		constructWitness(LimInf, witness, scc_values, top_values, scc_cycles, nullptr, nullptr);
		for (unsigned int scc_id = 0 ; scc_id < this->nb_SCCs; ++scc_id) {
			delete scc_cycles[scc_id];
		}
	}

	return top_values[this->initial->getTag()];
}


weight_t Automaton::top_LimSup_cycles (weight_t* top_values, SetList<Edge*>** scc_cycles, UltimatelyPeriodicWord** witness) const {
	weight_t (*filter)(weight_t,weight_t) = [] (weight_t x, weight_t y) -> weight_t {
		return std::max(x, y);
	};
	weight_t scc_values[this->states->size()];
	
	weight_t top = top_reachably(true, scc_values, top_values);
    top_cycles(filter, scc_values, top_values, scc_cycles);

    if (witness != nullptr) {
		constructWitness(LimSup, witness, scc_values, top_values, scc_cycles, nullptr, nullptr);
		for (unsigned int scc_id = 0 ; scc_id < this->nb_SCCs; ++scc_id) {
			delete scc_cycles[scc_id];
		}
	}

	return top;
}

bool Automaton::top_Sup_witness_explore (weight_t top, State* state, bool* spot, SetList<Edge*>* path) const {
	if (spot[state->getId()] == true) return false;
	
	spot[state->getId()] = true;
	for (Symbol* symbol : *(state->getAlphabet())) {
		for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
			if (edge->getWeight()->getValue() == top) {
				path->push(edge);
				return true;
			}
			else {
				weight_t value = std::max(edge->getWeight()->getValue(), top);
				if (value == top) {
					if (top_Sup_witness_explore(top, edge->getTo(), spot, path) == true) {
						path->push(edge);
						return true;
					}
				}
			}
		}
	}

	return false;
}

void Automaton::top_Sup_witness (weight_t top, SetList<Edge*>* path) const {
	bool spot[this->states->size()];
	
	for (unsigned int state_id = 0 ; state_id < this->states->size(); ++state_id) {
		spot[state_id] = false;
	}
	
	top_Sup_witness_explore(top, this->initial, spot, path);
}

weight_t Automaton::top_Sup_path (weight_t* top_values, SetList<Edge*>* path, UltimatelyPeriodicWord** witness) const {
	weight_t top = top_Sup(top_values);
	top_Sup_witness (top, path);
	constructWitness(Sup, witness, nullptr, top_values, nullptr, path, nullptr);
	return top;
}


int Automaton::top_Inf_witness_explore_post (weight_t top, State* state, bool* spot, bool* spot_back, SetList<Edge*>* witness_path ,SetList<Edge*>* witness_loop) const {
	if (spot[state->getId()] == true) {
		spot_back[state->getId()] = true;
		return 2;
	}
	
	spot[state->getId()] = true;
	for (Symbol* symbol : *(state->getAlphabet())) {
		for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
			weight_t value = std::min(edge->getWeight()->getValue(), top);
			if (value == top) {
				int tmp = top_Inf_witness_explore_post(top, edge->getTo(), spot, spot_back, witness_path, witness_loop);
				if (tmp == 1) {
					witness_path->push(edge);
					return 1;
				}
				if (tmp == 2) {
					if (spot_back[state->getId()] == true) {
						witness_loop->push(edge);
						return 1;
					}
					else {
						spot_back[state->getId()] = true;
						witness_loop->push(edge);
						return 2;
					}
				}
			}
		}
	}
	
	spot[state->getId()] = false;
	return 0;
}

bool Automaton::top_Inf_witness_post (State* init, weight_t top, SetList<Edge*>* witness_path, SetList<Edge*>* witness_loop) const {
	bool spot[this->states->size()];
	bool spot_back[this->states->size()];	
	
	for (unsigned int state_id = 0 ; state_id < this->states->size(); ++state_id) {
		spot[state_id] = false;
		spot_back[state_id] = false;
	}
	
	return (top_Inf_witness_explore_post (top, init, spot, spot_back, witness_path, witness_loop) != 0);
}

bool Automaton::top_Inf_witness_explore_pre (weight_t top, State* state, bool* spot, SetList<Edge*>* witness_path, SetList<Edge*>* witness_loop) const {
	if (spot[state->getId()] == true) return false;
	
	spot[state->getId()] = true;
	for (Symbol* symbol : *(state->getAlphabet())) {
		for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
			if (edge->getWeight()->getValue() == top) {
				if (top_Inf_witness_post (edge->getTo(), top, witness_path, witness_loop) == true) {
					witness_path->push(edge);
					return true;
				}
			}
			else {
				weight_t value = std::min(edge->getWeight()->getValue(), top);
				if (value == top) {
					if (top_Inf_witness_explore_pre(top, edge->getTo(), spot, witness_path, witness_loop) == true) {
						witness_path->push(edge);
						return true;
					}
				}
			}
		}
	}
	
	return false;
}

void Automaton::top_Inf_witness (weight_t top, SetList<Edge*>* witness_path, SetList<Edge*>* witness_loop) const {
	bool spot[this->states->size()];
	
	for (unsigned int state_id = 0 ; state_id < this->states->size(); ++state_id) {
		spot[state_id] = false;
	}
	
	top_Inf_witness_explore_pre (top, this->initial, spot, witness_path, witness_loop);
}

weight_t Automaton::top_Inf_path (weight_t* top_values, SetList<Edge*>* witness_path, SetList<Edge*>* witness_loop, UltimatelyPeriodicWord** witness) const {
	weight_t top = top_Inf(top_values);
	top_Inf_witness(top, witness_path, witness_loop);
	constructWitness(Inf, witness, nullptr, top_values, nullptr, witness_path, witness_loop);
	return top;
}

void Automaton::constructWitness(value_function_t f, UltimatelyPeriodicWord** witness, const weight_t* scc_values, const weight_t* top_values, SetList<Edge*>** scc_cycles, SetList<Edge*>* path, SetList<Edge*>* loop) const {
    (*witness) = new UltimatelyPeriodicWord{new Word(), new Word()};

	if (f == Sup) {
        for (Edge* edge : *path) {
            (*witness)->prefix->push_back(edge->getSymbol());
        }
		(*witness)->cycle->push_back(this->alphabet->at(0));
    }
	else if (f == Inf) {
		if (path->head()->getFrom()->getId() == path->back()->getTo()->getId()) {
			for (Edge* edge : *path) {
            	(*witness)->cycle->push_back(edge->getSymbol());
			}
        }
		else {
			for (Edge* edge : *path) {
				(*witness)->prefix->push_back(edge->getSymbol());
			}
			for (Edge* edge : *loop) {
				(*witness)->cycle->push_back(edge->getSymbol());
			}
		}
	}
	else { // prefix-independent value functions
		unsigned int top_state_id = 0;
		if (f == LimInf || f == LimSup) {
			for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
				if (scc_values[state_id] > scc_values[top_state_id]) {
					top_state_id = state_id;
				}
			}
		}
		else if (f == LimInfAvg || f == LimSupAvg) {
			int temp = 0;
			for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
				if (scc_values[scc_id] > scc_values[temp]) {
					temp = scc_id;
				}
			}

			for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
				if (this->states->at(state_id)->getTag() == temp) {
					top_state_id = state_id;
					break;
				}
			}
		}

		// construct a path to the start state of the top scc's cycle
		std::vector<Edge*> path_to_top_scc;
		State* target = (*scc_cycles[states->at(top_state_id)->getTag()]->begin())->getFrom();

		std::vector<State*> queue;
		std::vector<Edge*> predecessor_edge(this->states->size(), nullptr);
		std::vector<bool> visited(this->states->size(), false);

		queue.push_back(this->initial);
		visited[this->initial->getId()] = true;
		
		bool found = false;
		for (size_t i = 0; i < queue.size() && !found; i++) {
			State* current = queue[i];
			
			for (Symbol* symbol : *(current->getAlphabet())) {
				for (Edge* edge : *(current->getSuccessors(symbol->getId()))) {
					State* next = edge->getTo();
						
					// only consider states that lead to the top scc
					if (top_values[next->getTag()] == top_values[states->at(top_state_id)->getTag()]) {
						if (!visited[next->getId()]) {
							visited[next->getId()] = true;
							queue.push_back(next);
							predecessor_edge[next->getId()] = edge;

							if (next == target) {
								found = true;
								break;
							}
						}
					}
				}
				if (found) break;
			}
		}

		// reconstruct the path
		if (found) {
			State* current = target;
			while (current != this->initial) {
				Edge* edge = predecessor_edge[current->getId()];
				path_to_top_scc.insert(path_to_top_scc.begin(), edge);
				current = edge->getFrom();
			}
		}

		if (!scc_cycles[states->at(top_state_id)->getTag()]->empty()) {
			for (Edge* edge : path_to_top_scc) {
				(*witness)->prefix->push_back(edge->getSymbol());
			}
			for (Edge* edge : *scc_cycles[states->at(top_state_id)->getTag()]) {
				(*witness)->cycle->push_back(edge->getSymbol());
			}		
		}
	}
}


weight_t Automaton::compute_Top (value_function_t f, weight_t* top_values, UltimatelyPeriodicWord** witness) const {
	if (witness == nullptr) {
		switch (f) {
			case Inf:
				return top_Inf(top_values);
			case Sup:
				return top_Sup(top_values);
			case LimInf:
				return top_LimInf(top_values);
			case LimSup:
				return top_LimSup(top_values);
			case LimInfAvg: case LimSupAvg:
				return top_LimAvg(top_values);
			default:
				QUAK_FAIL("automata top (without witness)");
		}
	}
	else {
		SetList<Edge*>* scc_cycles[this->nb_SCCs];
		SetList<Edge*>* path = new SetList<Edge*>();
		SetList<Edge*>* loop = new SetList<Edge*>();
        weight_t result;

        switch (f) {
            case Inf:
				result = top_Inf_path(top_values, path, loop, witness);
                break;
            case Sup:
                result = top_Sup_path(top_values, path, witness);
                break;
            case LimInf:
                result = top_LimInf_cycles(top_values, scc_cycles, witness);
                break;
            case LimSup:
                result = top_LimSup_cycles(top_values, scc_cycles, witness);
                break;
            case LimInfAvg: case LimSupAvg:
                result = top_LimAvg_cycles(top_values, scc_cycles, witness);
                break;
            default:
                QUAK_FAIL("automata top (with witness)");
        }

        delete path;
		delete loop;
        return result;
	}
}

value_function_t getValueFunctionDual (value_function_t f) {
	if (f == Inf) {
		return Sup;
	}
	else if (f == Sup) {
		return Inf;
	}
	else if (f == LimInf) {
		return LimSup;
	}
	else if (f == LimSup) {
		return LimInf;
	}
	else if (f == LimInfAvg) {
		return LimSupAvg;
	}
	else if (f == LimSupAvg) {
		return LimInfAvg;
	}
	else {
		std::cerr << "Unknown value function dual" << "\n";
    	abort();
	}
}

weight_t Automaton::compute_Bottom (value_function_t f, weight_t* bot_values, UltimatelyPeriodicWord** witness) {
	if (this->isDeterministic()) {
		invert_weights();
		value_function_t f_dual = getValueFunctionDual(f);
		weight_t bot = compute_Top(f_dual, bot_values, witness);
		bot = -bot;
		for (unsigned int i = 0; i < this->nb_SCCs; i++) {
			bot_values[i] = -bot_values[i];
		}
		invert_weights();
		return bot;
	}
	else {
		if (f == Inf || f == Sup || f == LimInf || f == LimSup) {
			bool found = false;
			unsigned int weight_id = weights->size();
			weight_t x;

			while (!found && weight_id > 0) {
				weight_id--;
				x = this->weights->at(weight_id)->getValue();
				found = this->isUniversal(f, x, witness);
			}

			return x;
		}
		else {
			QUAK_FAIL("automata bot");
		}
	}
}


void Automaton::setMaxDomain (weight_t x) {
	this->max_domain = x;
}


void Automaton::setMinDomain (weight_t x) {
	this->min_domain = x;
}



bool Automaton::alphabetsAreCompatible(const Automaton *B) const {
  if (alphabet->size() != B->alphabet->size()) {
    return false;
  }

  // same symbols must be at same indices
  for (auto *symbol1 : *alphabet) {
    auto *symbol2 = B->alphabet->at(symbol1->getId());
    if (symbol1->getName() != symbol2->getName()) {
      return false;
    }
  }

  return true;
}


weight_t Automaton::computeValue(value_function_t f, UltimatelyPeriodicWord* w) {
	if (f == Sup) {
		SetStd<State*> current_states;
		current_states.insert(this->initial);
		weight_t value = this->min_domain;

		for (unsigned int i = 0; i < w->prefix->getLength(); i++) {
			Symbol* symbol = w->prefix->at(i);
			SetStd<std::pair<State*, weight_t>> next_states_weights;
			
			for (State* state : current_states) {
				for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
					next_states_weights.insert(std::make_pair(edge->getTo(), edge->getWeight()->getValue()));
					value = std::max(value, edge->getWeight()->getValue());
				}
			}

			current_states.clear();
			for (const auto& pair : next_states_weights) {
				current_states.insert(pair.first);
			}
		}

		for (State* start_state : current_states) {
			SetStd<std::pair<State*, weight_t>> current_pairs;
			current_pairs.insert(std::make_pair(start_state, this->min_domain));
			
			for (unsigned int i = 0; i < w->cycle->getLength(); i++) {
				Symbol* symbol = w->cycle->at(i);
				SetStd<std::pair<State*, weight_t>> next_pairs;
				
				for (const auto& pair : current_pairs) {
					for (Edge* edge : *(pair.first->getSuccessors(symbol->getId()))) {
						value = std::max(value, edge->getWeight()->getValue());
						next_pairs.insert(std::make_pair(edge->getTo(), edge->getWeight()->getValue()));
					}
				}
				current_pairs = next_pairs;
			}
		}

		return value;
	}
	else if (f == Inf) {
		SetStd<State*> current_states;
		current_states.insert(this->initial);
		std::vector<weight_t> path_mins;
		
		SetStd<std::pair<State*, weight_t>> current_pairs;
		current_pairs.insert(std::make_pair(this->initial, this->max_domain));
		
		for (unsigned int i = 0; i < w->prefix->getLength(); i++) {
			Symbol* symbol = w->prefix->at(i);
			SetStd<std::pair<State*, weight_t>> next_pairs;
			
			for (const auto& pair : current_pairs) {
				for (Edge* edge : *(pair.first->getSuccessors(symbol->getId()))) {
					weight_t min_value = std::min(pair.second, edge->getWeight()->getValue());
					next_pairs.insert(std::make_pair(edge->getTo(), min_value));
				}
			}
			current_pairs = next_pairs;
		}
		
		weight_t best_value = this->min_domain;
		for (const auto& start_pair : current_pairs) {
			SetStd<std::pair<State*, weight_t>> cycle_pairs;
			cycle_pairs.insert(start_pair);
			
			for (unsigned int i = 0; i < w->cycle->getLength(); i++) {
				Symbol* symbol = w->cycle->at(i);
				SetStd<std::pair<State*, weight_t>> next_pairs;
				
				for (const auto& pair : cycle_pairs) {
					for (Edge* edge : *(pair.first->getSuccessors(symbol->getId()))) {
						weight_t min_value = std::min(pair.second, edge->getWeight()->getValue());
						next_pairs.insert(std::make_pair(edge->getTo(), min_value));
					}
				}
				cycle_pairs = next_pairs;
			}
			
			// Only consider values where we return to the start state
			for (const auto& pair : cycle_pairs) {
				if (pair.first == start_pair.first) {  // Check if we've returned to the starting state
					best_value = std::max(best_value, pair.second);
				}
			}
		}
		
		return best_value;
	}
	else if (f == LimSup) {
		SetStd<State*> current_states;
		current_states.insert(this->initial);
		
		for (unsigned int i = 0; i < w->prefix->getLength(); i++) {
			Symbol* symbol = w->prefix->at(i);
			SetStd<State*> next_states;
			
			for (State* state : current_states) {
				for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
					next_states.insert(edge->getTo());
				}
			}
			current_states = next_states;
		}

		weight_t value = this->min_domain;
    
		for (State* start_state : current_states) {
			SetStd<std::pair<State*, weight_t>> current_pairs;
			current_pairs.insert(std::make_pair(start_state, this->min_domain));
			
			for (unsigned int i = 0; i < w->cycle->getLength(); i++) {
				Symbol* symbol = w->cycle->at(i);
				SetStd<std::pair<State*, weight_t>> next_pairs;
				
				for (const auto& pair : current_pairs) {
					for (Edge* edge : *(pair.first->getSuccessors(symbol->getId()))) {
						next_pairs.insert(std::make_pair(
							edge->getTo(),
							std::max(pair.second, edge->getWeight()->getValue())
						));
					}
				}
				current_pairs = next_pairs;
			}
			
			for (const auto& pair : current_pairs) {
				if (pair.first == start_state) {
					value = std::max(value, pair.second);
				}
			}
		}
		
		return value;
	}
	else if (f == LimInf) {
		SetStd<State*> current_states;
		current_states.insert(this->initial);

		for (unsigned int i = 0; i < w->prefix->getLength(); i++) {
			Symbol* symbol = w->prefix->at(i);
			SetStd<State*> next_states;
			
			for (State* state : current_states) {
				for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
					next_states.insert(edge->getTo());
				}
			}
			current_states = next_states;
		}

		weight_t value = this->min_domain;

		for (State* start_state : current_states) {
			SetStd<std::pair<State*, weight_t>> current_pairs;
			current_pairs.insert(std::make_pair(start_state, this->max_domain));
			
			for (unsigned int i = 0; i < w->cycle->getLength(); i++) {
				Symbol* symbol = w->cycle->at(i);
				SetStd<std::pair<State*, weight_t>> next_pairs;
				
				for (const auto& pair : current_pairs) {
					for (Edge* edge : *(pair.first->getSuccessors(symbol->getId()))) {
						next_pairs.insert(std::make_pair(
							edge->getTo(),
							std::min(pair.second, edge->getWeight()->getValue())
						));
					}
				}
				current_pairs = next_pairs;
			}
			
			for (const auto& pair : current_pairs) {
				if (pair.first == start_state) {
					value = std::max(value, pair.second);
				}
			}
		}

		return value;
	}
	else if (f == LimInfAvg || f == LimSupAvg) {
		SetStd<State*> current_states;
		current_states.insert(this->initial);
		if (w->prefix != nullptr) {
			for (unsigned int i = 0; i < w->prefix->getLength(); i++) {
				Symbol* symbol = w->prefix->at(i);
				SetStd<State*> next_states;
				
				for (State* state : current_states) {
					for (Edge* edge : *(state->getSuccessors(symbol->getId()))) {
						next_states.insert(edge->getTo());
					}
				}
				current_states = next_states;
			}
		}

		weight_t value = this->min_domain;

		for (State* start_state : current_states) {
			SetStd<std::pair<State*, weight_t>> current_pairs;
			current_pairs.insert(std::make_pair(start_state, 0));
			
			for (unsigned int i = 0; i < w->cycle->getLength(); i++) {
				Symbol* symbol = w->cycle->at(i);
				SetStd<std::pair<State*, weight_t>> next_pairs;
				
				for (const auto& pair : current_pairs) {
					for (Edge* edge : *(pair.first->getSuccessors(symbol->getId()))) {
						next_pairs.insert(std::make_pair(
							edge->getTo(),
							pair.second + edge->getWeight()->getValue()
						));
					}
				}
				current_pairs = next_pairs;
			}
			
			for (const auto& pair : current_pairs) {
				if (pair.first == start_state) {
					weight_t avg = pair.second / (weight_t)w->cycle->getLength();
					value = std::max(value, avg);
				}
			}
		}

		return value;
	}
	else {
		QUAK_FAIL("compute value of lasso word");
	}
}

// String representations


void Automaton::print(bool full, bool bv_weights, bool bv_only) const {
	print(std::cout, full, bv_weights, bv_only);

}

void Automaton::print(std::ostream& out, bool full, bool bv_weights, bool bv_only) const {
	out << "automaton (" << this->name << "):\n";
	out << "\talphabet (" << this->alphabet->size() << "):";
	out << this->alphabet->toString(Symbol::toString) << "\n";
	out << "\tweights (" << this->weights->size() << "):";
	out << weights->toString(Weight::toString) << "\n";
	out << "\t\tMIN = " << std::to_string(min_domain) << "\n";
	out << "\t\tMAX = " << std::to_string(max_domain) << "\n";
	out << "\tstates (" << this->states->size() << "):";
	out << states->toString(State::toString) << "\n";
	// out << "\t\tINITIAL = " << initial->getName() << "\n";
	out << "\t\tINITIAL = ";
	if (initial != nullptr) out << initial->getName() << "\n";
	else out << "(none)\n";

	
	out << "\t\tFINAL = ";
	bool first_final = true;
	unsigned int final_count = 0;
	for (unsigned int state_id = 0; state_id < states->size(); ++state_id) {
		if (states->at(state_id)->getFinal()) {
			if (!first_final) out << ", ";
			out << states->at(state_id)->getName();
			first_final = false;
			final_count++;
		}
	}
	if (final_count == 0) {
		out << "(none)";
	}
	out << "\n";

	if (initial != nullptr) {
		out << "\tSCCs (" << this->nb_SCCs << "):";
		out << this->SCCs[this->initial->getTag()]->toString("\t\t") << "\n";
	}

	unsigned int nb_edge = 0;
	for (unsigned int state_id = 0; state_id < states->size(); ++state_id) {
		for (Symbol* symbol : *(states->at(state_id)->getAlphabet())) {
			nb_edge += states->at(state_id)->getSuccessors(symbol->getId())->size();
		}
	}
	out << "\tedges (" << nb_edge << "):\n";
	for (unsigned int state_id = 0; state_id < states->size(); ++state_id) {
		for (Symbol* symbol : *(states->at(state_id)->getAlphabet())) {
            auto *state = states->at(state_id);
            for (auto *edge : *state->getSuccessors(symbol->getId())) {
                out << "\t\t" << edge->getSymbol()->toString() << " : ";
                if (bv_only) {
                    out << "0x" << std::hex << edge->getWeight()->getValue().to_bv();
                } else {
                    if (full) {
                      out << std::setprecision(std::numeric_limits<weight_t::T>::max_digits10)
                                << std::fixed;
                    }
                    out << edge->getWeight()->getValue();	// CHANGE!!: DELETED *
                    if (bv_weights) {
                        out << " (" << "0x" << std::hex << edge->getWeight()->getValue().to_bv() << ")";
                    }
                }
                out << ", " << edge->getFrom()->getName() << " -> "
                          << edge->getTo()->getName() << "\n";
            }
		}
	}
	out << "\n";
}


void Automaton::write(std::ostream& out) const {
	unsigned int initID = this->initial->getId();
	for (Symbol* symbol : *(states->at(initID)->getAlphabet())) {
		auto *state = states->at(initID);
		for (auto *edge : *state->getSuccessors(symbol->getId())) {
			out << edge->getSymbol()->toString() << " : ";
			out << "0x" << std::hex << edge->getWeight()->getValue().to_bv();
			//out << *edge->getWeight()->getValue();
			out << ", " << edge->getFrom()->getName() << " -> " << edge->getTo()->getName() << "\n";
		}
	}
	for (unsigned int state_id = 0; state_id < states->size(); ++state_id) {
		if (state_id == initID) continue;
		for (Symbol* symbol : *(states->at(state_id)->getAlphabet())) {
            auto *state = states->at(state_id);
            for (auto *edge : *state->getSuccessors(symbol->getId())) {
                out << edge->getSymbol()->toString() << " : ";
                out << "0x" << std::hex << edge->getWeight()->getValue().to_bv();
                //out << *edge->getWeight()->getValue();
                out << ", " << edge->getFrom()->getName() << " -> " << edge->getTo()->getName() << "\n";
            }
		}
	}
	out << "\n";
}



//...................................................//





weight_t Automaton::top_Sup_with_final () const {
	Automaton* A = Automaton::toLimSup(this, Sup);
	weight_t top = A->top_LimSup_with_final();
	delete A;
	return top;
}



weight_t Automaton::top_Inf_with_final () const {
	Automaton* A = Automaton::toLimSup(this, Inf);
	weight_t top = A->top_LimSup_with_final();
	// A->print();
	delete A;
	return top;
}


weight_t Automaton::top_LimSup_with_final() const {
	std::vector<weight_t> values(this->states->size(), empty_language_bottom_value());
	std::vector<bool> spot(this->states->size(), false);

	weight_t top = empty_language_bottom_value();
	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		if (final_SCCs[scc_id]) {
			top_reachably_scc_new(this->SCCs[scc_id]->origin, true, spot, values);
			top = std::max(top, values[this->SCCs[scc_id]->origin->getId()]);
		}
	}

	return top;
}
// weight_t Automaton::top_LimSup_with_final () const {
// 	weight_t values[this->states->size()];
// 	bool spot[this->states->size()];
	
// 	for (unsigned int state_id = 0; state_id < this->states->size(); ++state_id) {
// 		values[state_id] = this->min_domain;
// 		spot[state_id] = false;
// 	}

// 	weight_t top = this->min_domain;
// 	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
// 		if (final_SCCs[scc_id] == true) {
// 			top_reachably_scc(this->SCCs[scc_id]->origin, true, spot, values);
// 			top = std::max(top, values[this->SCCs[scc_id]->origin->getId()]);
// 		}
// 	}

// 	return top;
// }




weight_t Automaton::top_LimInf_with_final () const {
	Automaton* A = Automaton::toLimSup(this, LimInf);
	// A->print();
	weight_t top = A->top_LimSup_with_final();
	delete A;
	return top;
}



weight_t Automaton::top_LimAvg_with_final () const {
	unsigned int size = this->states->size();
	// weight_t distance[size + 1][size];
	std::vector<std::vector<weight_t>> distance(size + 1, std::vector<weight_t>(size));
	weight_t infinity = std::max(weight_t(1), -(weight_t(size)*this->min_domain) + 1); // TODO

	// O(n)
	for (unsigned int length = 0; length <= size; ++length) {
		for (unsigned int state_id = 0; state_id < size; ++state_id) {
			distance[length][state_id] = infinity;
		}
	}


	//O(n)
	// auto initialize_distances = [] (SCC_Dag* dag, weight_t* distance, auto &rec) -> void {
	auto initialize_distances = [] (SCC_Dag* dag, std::vector<weight_t> &distance, auto &rec) -> void {
		distance[dag->origin->getId()] = 0;
		for (auto iter = dag->nexts->begin(); iter != dag->nexts->end(); ++iter) {
			rec(*iter, distance, rec);
		}
	};
	initialize_distances(this->SCCs[this->initial->getTag()], distance[0], initialize_distances);


	// O(n.m)
	for (unsigned int len = 1; len <= size; ++len) {
		for (unsigned int state_id = 0; state_id < size; ++state_id)	{
			for (Symbol* symbol : *(states->at(state_id)->getAlphabet())) {
				for (Edge* edge : *(states->at(state_id)->getSuccessors(symbol->getId()))) {
					if (edge->getFrom()->getTag() == edge->getTo()->getTag()) {
						if (distance[len-1][edge->getFrom()->getId()] != infinity) {
							weight_t value = distance[len-1][edge->getFrom()->getId()] - edge->getWeight()->getValue();
							if (distance[len][edge->getTo()->getId()] == infinity) {
								distance[len][edge->getTo()->getId()] = value;
							}
							else {
								distance[len][edge->getTo()->getId()] =
										std::min(value, distance[len][edge->getTo()->getId()]);
							}
						}
					}
				}
			}
		}
	}

	//O(n.m)
	weight_t top_scc[this->nb_SCCs];
	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		top_scc[scc_id] = this->min_domain;
	}

	for (unsigned int state_id = 0; state_id < size; ++state_id) {
		weight_t min_lenght_avg = this->max_domain;
		bool len_flag = false;
		if (distance[size][state_id] != infinity) { // => id has an ongoing edge (inside its SCC)
			for (unsigned int lenght = 0; lenght < size; ++lenght) { // hence the nested loop is call at most O(m) times
				if (distance[lenght][state_id] != infinity) {
					weight_t avg = (distance[lenght][state_id] - distance[size][state_id] + 0.0) / weight_t(size - lenght + 0.0);
					min_lenght_avg = std::min(min_lenght_avg, avg);
					len_flag = true;
				}
			}
		}
		if (len_flag) {
			top_scc[this->states->at(state_id)->getTag()] = std::max(top_scc[this->states->at(state_id)->getTag()], min_lenght_avg);
		}
	}

	weight_t top = this->min_domain;
	for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
		if (final_SCCs[scc_id] == true) {
			top = std::max(top, top_scc[scc_id]);
		}
	}

	return top;
}


weight_t Automaton::compute_top_with_final(value_function_t f) const {
	switch (f) {
		case Inf:
			return top_Inf_with_final();
		case Sup:
			return top_Sup_with_final();
		case LimInf:
			return top_LimInf_with_final();
		case LimSup:
			return top_LimSup_with_final();
		case LimInfAvg: case LimSupAvg:
			return top_LimAvg_with_final();
		default:
			QUAK_FAIL("automata top (with final)");
	}
}



//...................................................//

namespace {

	// Local adjacency type inside an SCC
	struct LocalEdge {
		int to;        // local index
		weight_t w;    // edge weight
	};
	
	/**
	 * Compute maximum mean cycle weight in a single SCC.
	 *
	 * @param A          The automaton (for access to states, edges, weights).
	 * @param scc_states Global state IDs belonging to this SCC.
	 * @param scc_id     ID (tag) of the SCC (as stored in State::getTag()).
	 *
	 * Preconditions:
	 *   - scc_states is non-empty.
	 *   - All states in scc_states have getTag() == scc_id.
	 */
	weight_t max_mean_cycle_on_scc(
		const Automaton* A,
		const std::vector<unsigned int>& scc_states,
		unsigned int scc_id,
		std::vector<int>& global_to_local,
		std::vector<unsigned int>& touched)
	{
		using std::vector;
		const unsigned int n = static_cast<unsigned int>(scc_states.size());
		if (n == 0) {
			// No states → no cycle, return lowest possible value.
			return A->min_domain;
		}
	
		// Map global state_id -> local index [0 .. n-1]
		touched.clear();
		touched.reserve(n);

		for (unsigned int i = 0; i < n; ++i) {
			const unsigned int sid = scc_states[i];
			global_to_local[sid] = static_cast<int>(i);
			touched.push_back(sid);
		}
	
		// Build adjacency list of this SCC only.
		vector<vector<LocalEdge>> adj(n);
		bool has_cycle_edge = false;
	
		for (unsigned int i = 0; i < n; ++i) {
			unsigned int sid = scc_states[i];
			State* s = A->states->at(sid);
	
			for (Symbol* sym : *s->getAlphabet()) {
				auto* succs = s->getSuccessors(sym->getId());
				if (!succs) continue;
	
				for (Edge* e : *succs) {
					State* to = e->getTo();
					if (to->getTag() != scc_id) continue;  // outside SCC
	
					int j = global_to_local[to->getId()];
					if (j < 0) continue;                   // safety
	
					adj[i].push_back(LocalEdge{ j, e->getWeight()->getValue() });
	
					// If there is at least one edge in this SCC, there is a cycle
					// as soon as n > 1 or there is a self-loop.
					if (n > 1 || j == static_cast<int>(i)) {
						has_cycle_edge = true;
					}
				}
			}
		}
	
		if (!has_cycle_edge) {
			// Single state without self-loop, or something degenerate:
			// cannot form a cycle with length ≥ 1.
			// Use float's lowest to ensure a proper sentinel value
			return weight_t(std::numeric_limits<float>::lowest());
		}
	
		const unsigned int N = n;
		const int src = 0;   // Choose any vertex in the SCC as source.
	
		// H_n[v] and its validity flag
		vector<weight_t> Hn(N);
		vector<char> Hn_valid(N, 0);
	
		// DP arrays (two layers) and validity bits.
		vector<weight_t> dp_prev(N), dp_curr(N);
		vector<char> valid_prev(N, 0), valid_curr(N, 0);
	
		// Pass 1: compute H_n[v] for all v
	
		std::fill(valid_prev.begin(), valid_prev.end(), 0);
		valid_prev[src] = 1;
		dp_prev[src] = weight_t(0); // assuming weight_t(0) is valid
	
		for (unsigned int step = 1; step <= N; ++step) {
			std::fill(valid_curr.begin(), valid_curr.end(), 0);
	
			for (unsigned int u = 0; u < N; ++u) {
				if (!valid_prev[u]) continue;
	
				const weight_t base = dp_prev[u];
				for (const LocalEdge& e : adj[u]) {
					unsigned int v = static_cast<unsigned int>(e.to);
					weight_t cand = base + e.w;
	
					if (!valid_curr[v]) {
						dp_curr[v] = cand;
						valid_curr[v] = 1;
					}
					else if (cand > dp_curr[v]) {
						dp_curr[v] = cand;
					}
				}
			}
	
			if (step == N) {
				// Store H_n and validity.
				Hn = dp_curr;
				Hn_valid.assign(valid_curr.begin(), valid_curr.end());
			}
	
			dp_prev.swap(dp_curr);
			valid_prev.swap(valid_curr);
		}
	
		// Pass 2: recompute H_k[v], accumulate min_k (H_n - H_k)/(n-k)
	
		vector<weight_t> min_ratio(N);   // per-vertex min over k
		vector<char> ratio_defined(N, 0);
	
		std::fill(valid_prev.begin(), valid_prev.end(), 0);
		valid_prev[src] = 1;
		dp_prev[src] = weight_t(0);
	
		// k = 0 case: H_0[src] = 0, others invalid.
		{
			const unsigned int k = 0;
			for (unsigned int v = 0; v < N; ++v) {
				if (!Hn_valid[v] || !valid_prev[v]) continue;
	
				weight_t num = Hn[v] - dp_prev[v];            // H_n(v) - H_0(v)
				weight_t den = weight_t(N - k);               // n - 0 = n
				weight_t frac = num / den;
	
				min_ratio[v] = frac;
				ratio_defined[v] = 1;
			}
		}
	
		for (unsigned int k = 1; k < N; ++k) {
			std::fill(valid_curr.begin(), valid_curr.end(), 0);
	
			for (unsigned int u = 0; u < N; ++u) {
				if (!valid_prev[u]) continue;
				const weight_t base = dp_prev[u];
	
				for (const LocalEdge& e : adj[u]) {
					unsigned int v = static_cast<unsigned int>(e.to);
					weight_t cand = base + e.w;
	
					if (!valid_curr[v]) {
						dp_curr[v] = cand;
						valid_curr[v] = 1;
					}
					else if (cand > dp_curr[v]) {
						dp_curr[v] = cand;
					}
				}
			}
	
			// Now dp_curr holds H_k[v]
			for (unsigned int v = 0; v < N; ++v) {
				if (!Hn_valid[v] || !valid_curr[v]) continue;
	
				weight_t num = Hn[v] - dp_curr[v];
				weight_t den = weight_t(N - k);
				weight_t frac = num / den;
	
				if (!ratio_defined[v]) {
					min_ratio[v] = frac;
					ratio_defined[v] = 1;
				}
				else if (frac < min_ratio[v]) {
					min_ratio[v] = frac;
				}
			}
	
			dp_prev.swap(dp_curr);
			valid_prev.swap(valid_curr);
		}
	
		// mu_max = max_v min_ratio[v]
		bool any = false;
		weight_t best = A->min_domain; // lower bound
	
		for (unsigned int v = 0; v < N; ++v) {
			if (!ratio_defined[v]) continue;
			if (!any || min_ratio[v] > best) {
				best = min_ratio[v];
				any = true;
			}
		}

		for (unsigned int sid : touched) {
			global_to_local[sid] = -1;
		}
		touched.clear();
	
		if (!any) {
			// Shouldn't happen for an SCC with at least one cycle, but be safe.
			// Use float's lowest to ensure a proper sentinel value
			return weight_t(std::numeric_limits<float>::lowest());
		}
		return best;
	}
	
	} // namespace
	


bool Automaton::emptiness_LimAvg_with_final(weight_t threshold) const {
    const unsigned int n = this->states->size();

    // Collect states per SCC
    std::vector<std::vector<unsigned int>> states_in_scc(this->nb_SCCs);
    for (unsigned int sid = 0; sid < n; ++sid) {
        // if (!state_reachable[sid]) continue;  // prune unreachable early
        int tag = this->states->at(sid)->getTag();
        if (tag > -1) states_in_scc[tag].push_back(sid);
    }

    // For each reachable, final SCC: compute max mean cycle and compare
    for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
        if (!this->final_SCCs[scc_id]) continue;  // SCC not accepting
        // if (!scc_reachable[scc_id]) continue;     // SCC not reachable
        const auto& vec = states_in_scc[scc_id];
        if (vec.empty()) continue;                // no reachable state in this SCC

        // weight_t mu_scc = max_mean_cycle_on_scc(this, vec, scc_id);
		std::vector<int> global_to_local(n, -1);
		std::vector<unsigned int> touched;
		weight_t mu_scc = max_mean_cycle_on_scc(this, vec, scc_id, global_to_local, touched);

		if (mu_scc == weight_t(std::numeric_limits<float>::lowest())) continue;

        if (mu_scc >= threshold) {
            // Witness found: this accepting SCC supports a word with value
            // at least the threshold.
            return true;   // non-empty at threshold
        }
    }

    return false; // no accepting SCC with mean >= threshold
}

bool Automaton::isNonEmpty_withFinal(value_function_t f, weight_t threshold) const {
    if (f == LimInfAvg || f == LimSupAvg) {
        return this->emptiness_LimAvg_with_final(threshold);
    } else {
        weight_t top = this->compute_top_with_final(f);
        return (top >= threshold);
    }
}

unsigned int Automaton::getNbSCCs() const {
    return this->nb_SCCs;;
}


unsigned int Automaton::getNbAcceptingSCCs() const {
    unsigned int count = 0;
    // Iterate through all SCCs using the pre-computed count
    for (unsigned int scc_id = 0; scc_id < this->nb_SCCs; ++scc_id) {
        // final_SCCs[i] is true if the SCC contains at least one final state
        if (this->final_SCCs[scc_id]) {
            count++;
        }
    }
    return count;
}

unsigned int Automaton::getNbStates() const {
    return this->states ? this->states->size() : 0;
}

unsigned int Automaton::getNbTransitions() const {
    unsigned int m = 0;
    if (!this->states) return 0;

    for (unsigned int i = 0; i < this->states->size(); ++i) {
        State* st = this->states->at(i);
        if (!st) continue;

        for (Symbol* sym : *(st->getAlphabet())) {
            auto* succ = st->getSuccessors(sym->getId());
            if (succ) m += succ->size();
        }
    }
    return m;
}
