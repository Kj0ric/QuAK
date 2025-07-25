#include <iostream>
#include <string>
#include "Parser.h"
#include "Automaton.h"
#include "ChildAutomaton.h"
#include "NestedAutomaton.h"
#include "Weight.h"
#include "utility.h"

void testReadDomain() {
    std::string filename = "../samples/tests/testH1.txt";
    
    Parser* parser = new Parser(filename);    
    std::cout << "Min domain: " << parser->min_domain << " Max domain: " << parser->max_domain << std::endl;

    if (parser->min_domain == 10 && parser->max_domain == 200) {
        std::cout << "readDomain() works correctly" << std::endl;
    }
    
    delete parser;
}
// Appearently Parser::readDomain() works correctly but never called anywhere useful.

void testSilent(std::string filename) {
    Parser* parser = new Parser(filename);
    std::cout << "Parsed initial state: " << parser->initial << std::endl;

    delete parser;
}

// Simple non-nested automata test to test readNonNestedFile() in Parser.cpp
void testNonNestedRead(const std::string filepath) {
    Automaton* A = new Automaton(filepath);
    A->print();

    delete A;
}

void testNestedRead(std::string filepath) {
    Parser* parser = new Parser(filepath);
    parser->print(std::cout);

    delete parser;
}

void testNestedConstruction(const std::string& filepath) {
    Parser* parser = new Parser(filepath);
    parser->print(std::cout);
    NestedAutomaton* nested = new NestedAutomaton("nested1", parser, MapStd<std::string, Symbol*>());
    std::cout << "NestedAutomaton constructed from " << filepath << ":\n";
    nested->print(); 

    delete nested;
    delete parser;
}

void testSilentTransformationNonNested(const std::string& filepath) {
    Automaton* sil_A = new Automaton(filepath);
    std::cout << "NonNested silent Automaton constructed from " << filepath << ":\n";
    sil_A->print();

    Automaton* nonSil_A = Automaton::removeSilentTransitions(sil_A, LimInf);    // Static member function
    std::cout << "NonNested nonSilent Automaton transformed" << ":\n";
    nonSil_A->print();
}

void testSilentTransformationNested(const std::string& filepath) {
    NestedAutomaton* nested_sil_A = new NestedAutomaton(filepath);
    std::cout << "Nested silent Automaton constructed from " << filepath << ":\n";
    nested_sil_A->print();

    NestedAutomaton* nested_nonSil_A = NestedAutomaton::removeSilentTransitions(nested_sil_A, Sup);    // Static member function
    std::cout << "Nested nonSilent Automaton transformed" << ":\n";
    nested_nonSil_A->print();

    delete nested_sil_A;
    delete nested_nonSil_A;
}

void testS_ijConstruction(
        const std::string& filepath, 
        std::size_t child_index, 
        weight_t j, 
        value_function_t g, 
        weight_t bound = -1
) {
    NestedAutomaton* nested = new NestedAutomaton(filepath);
    std::cout << "Original NestedAutomaton from " << filepath << ":\n" << std::endl;
    nested->print();

    if (nested->getChildrenSize() == 0) {
        delete nested;
        QUAK_FAIL("No child automata found in the nested automaton.\n");
    }

    if (child_index < 0 || child_index >= nested->getChildrenSize()) {
        delete nested;
        QUAK_FAIL("Invalid child automaton index.\n");
    }

    ChildAutomaton* child = nested->getChild(child_index);
    if (!child) {
        delete nested;
        QUAK_FAIL("Child automaton pointer is null.\n");
    }

    std::cout << "Start determinizing..." << std::endl;
    ChildAutomaton* S_ij = child->determiniseToS_ij(j, g, bound);
    std::cout << "Determinized S_{" << j << "} automaton (child " << child_index << "):\n";
    S_ij->print();

    // Clean up
    delete S_ij;
    delete nested;
}