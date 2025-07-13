#include <iostream>
#include <string>
#include "Parser.h"
#include "Automaton.h"
#include "NestedAutomaton.h"

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