#include <iostream>

#include "ChildAutomaton.h"
#include "NestedAutomaton.h"
#include "Edge.h"
#include "utility.h"

// Archived: S_ij construction functions -> ChildAutomaton_oldHelpers.cpp

ChildAutomaton::~ChildAutomaton() {}

ChildAutomaton::ChildAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register)
    : Automaton(name, parser, sync_register) {
    // Mark final states using State::setFinal
    for (auto it = states->begin(); it != states->end(); ++it) {
        State* s = *it;
        if (s && parser->final_states.contains(s->getName())) {
            s->setFinal(true);
        }
    }
}

ChildAutomaton::ChildAutomaton(std::string name, MapArray<Symbol*>* alphabet, MapArray<State*>* states,
                               MapArray<Weight*>* weights, weight_t min_domain, weight_t max_domain,
                               State* initial)
    : Automaton(name, alphabet, states, weights, min_domain, max_domain, initial) {}

ChildAutomaton::ChildAutomaton(const ChildAutomaton& other)
    : Automaton(other) {}

void ChildAutomaton::print(bool full, bool bv_weights, bool bv_only) const {
    print(std::cout, full, bv_weights, bv_only);
}

void ChildAutomaton::print(std::ostream& out, bool full, bool bv_weights, bool bv_only) const {
    out << "Child Automaton (" << this->getName() << "):\n";
    Automaton::print(out);
}
