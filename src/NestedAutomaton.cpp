#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <iomanip>
#include <limits>
#include <algorithm>

#include "NestedAutomaton.h"
#include "Parser.h"
#include "Edge.h"
#include "utility.h"
#include "FORKLIFT/inclusion.h"

/* ----------------------- ChildAutomaton ----------------------- */
inline SetStd<State*> getStatesByNames(MapArray<State*>* states, const SetStd<std::string>& final_state_names);

ChildAutomaton::~ChildAutomaton () {
    delete final_states_;
    // Automaton destructor is called automatically
}

/* ------- CONSTRUCTORS -------- */
ChildAutomaton::ChildAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register) 
    : Automaton(name, parser, sync_register){
    
    /*
        if (parser->is_dummy_child) {
        // Clean up dynamic allocations by Automaton constructor
        delete this->alphabet;
        delete this->states;
        delete this->weights;
        delete this->initial;

        // Build dummy automaton: one state which is initial and final, no transitions
        this->states = new MapArray<State*>(1);
        State *s = new State("dummy", 0, 0, 0);
        s->automaton = this;
        this->states->insert(0,s);
        this->initial = s;
        this->alphabet = new MapArray<Symbol*>(0);
        this->weights = new MapArray<Weight*>(0);
        this->final_states_ = new SetStd<State*>();
        this->final_states_->insert(s);
        // No transitions

        return;
    }
    */

	// If not dummy child then Initialize final_states_ from parser->final_states
    final_states_ = new SetStd<State*>(getStatesByNames(states, parser->final_states));
}

// CC
ChildAutomaton::ChildAutomaton(const ChildAutomaton& other)
    : Automaton(other), // call base CC
     final_states_(new SetStd<State*>(*other.final_states_)) {}

/* ------- HELPERS -------- */
void ChildAutomaton::print(bool full, bool bv_weights, bool bv_only) const {
    print(std::cout, full, bv_weights, bv_only);
}

void ChildAutomaton::print(std::ostream& out, bool full, bool bv_weights, bool bv_only) const {
    out << "Child Automaton (" << this->getName() << "):\n";
    Automaton::print(out);

    out << "\tFinal states: ";
    SetStd<State*>* finals = getFinalStates();
    for (State* s : *finals) {
        out << s->getName() << " ";
    }
    out << std::endl;
}


/* ----------------------- NestedAutomaton ----------------------- */
NestedAutomaton::~NestedAutomaton() {
    // Clean up children_ array
    if (children_ != nullptr) {
        for (size_t i = 0; i < children_->size(); ++i) {
            delete children_->at(i);
        }
        delete children_;
    }
}

/* ------- CONSTRUCTORS -------- */
NestedAutomaton::NestedAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register) 
    : Automaton(name, parser, sync_register)
{
    // Allocate children_ array with the num of child parsers
    children_ = new MapArray<ChildAutomaton*>(parser->child_parsers.size()); 
    
    //  For each child parser, initialize ChildAutomaton objects
    for (unsigned i = 0; i < parser->child_parsers.size(); ++i) {
        Parser* child_parser = parser->child_parsers[i];
        if (child_parser) {
            auto* child = new ChildAutomaton(name + "_child" + std::to_string(i), child_parser, sync_register);
            children_->insert(i, child);
        }
    }
}

// Constructs a nested automaton from a file with the same alphabet as another automaton
NestedAutomaton::NestedAutomaton(std::string filename, Automaton* other) 
    : Automaton(filename, other)
{
    // Create a new parser from the file to get child information
    Parser* parser = new Parser(filename);
    
    // Set up sync_register if other automaton is provided
    MapStd<std::string, Symbol*> sync_register;
    if (other != nullptr) {
        for (unsigned int symbol_id = 0; symbol_id < other->getAlphabet()->size(); ++symbol_id) {
            Symbol* symbol = other->getAlphabet()->at(symbol_id);
            sync_register.insert(symbol->getName(), symbol);
        }
    }
    
    // Allocate children_ array with child parsers
    children_ = new MapArray<ChildAutomaton*>(parser->child_parsers.size()); 
    
    // Initialize ChildAutomaton objects
    for (unsigned i = 0; i < parser->child_parsers.size(); ++i) {
        Parser* child_parser = parser->child_parsers[i];
        if (child_parser) {
            auto* child = new ChildAutomaton(filename + "_child" + std::to_string(i), child_parser, sync_register);
            children_->insert(i, child);
        }
    }
    
    delete parser;
}

// Helper constructor for NestedAutomaton
NestedAutomaton::NestedAutomaton(const Automaton* parent, MapArray<ChildAutomaton*>* children)
    : Automaton(*parent), // Use the public copy constructor
      children_(children) {
    
        this->setName(parent->getName() + "_noSilent");
}

/* ---------- REMOVING SILENT TRANSITIONS ---------- */
NestedAutomaton* NestedAutomaton::removeSilentTransitions(const NestedAutomaton* A, value_function_t f) {
    // 1. Transform the parent automaton using the base class method
    Automaton* transformed_parent = Automaton::removeSilentTransitions(A, f);

    // 2. Shallow copy the children array (children remain unchanged)
    MapArray<ChildAutomaton*>* copied_children = new MapArray<ChildAutomaton*>(A->children_->size());
    for (unsigned i = 0; i < A->children_->size(); ++i) {
        if (A->children_->at(i)) {
            // Use the copy constructor for ChildAutomaton
            copied_children->insert(i, new ChildAutomaton(*A->children_->at(i)));
        }
    }

    // 3. Create new NestedAutomaton with transformed parent and copied children
    NestedAutomaton* result = new NestedAutomaton(transformed_parent, copied_children);

    delete transformed_parent;
    return result;
}
    
/* ------- HELPERS -------- */
// Returns a set of State* from states whose names are in parser->final_states
inline SetStd<State*> getStatesByNames(MapArray<State*>* states, const SetStd<std::string>& final_state_names) {
    SetStd<State*> result;
    for (auto it = states->begin(); it != states->end(); ++it) {
        State* s = *it;
        if (s && final_state_names.contains(s->getName())) {
            result.insert(s);
        }
    }
    return result;
}

void NestedAutomaton::print(bool full, bool bv_weights, bool bv_only) const {
    print(std::cout, full, bv_weights, bv_only);
}

void NestedAutomaton::print(std::ostream& out, bool full, bool bv_weights, bool bv_only) const {
    out << "(1) NESTED AUTOMATON (" << this->getName() << "):\n";
    Automaton::print(out);

    if (children_ && children_->size() > 0) {
        out << "(2) CHILD AUTOMATA:" << std::endl;
        for (unsigned i = 0; i < children_->size(); ++i) {
            ChildAutomaton* child = children_->at(i);
            if (child) {
                out << "[Child " << i << "]" << std::endl;
                child->print(out);
            }
        }
    } else {
        out << "The nested automaton (" << this->getName() << ") has no child automata." << std::endl;
    }
}


