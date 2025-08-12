#include <iostream>
#include <string>
#include "Parser.h"
#include "Automaton.h"
#include "ChildAutomaton.h"
#include "NestedAutomaton.h"
#include "Weight.h"
#include "utility.h"

extern SetStd<weight_t> computeChildReturnValues(ChildAutomaton* child, value_function_t finVal, weight_t bound = -1);
extern SetStd<weight_t> computeGlobalReturnValues(const NestedAutomaton* nwa, value_function_t finVal, weight_t bound = -1);
extern void constructMonitors(
        const NestedAutomaton* nwa,
        const SetStd<weight_t>& global_return_values,
        std::vector<std::vector<ChildAutomaton*>>& monitors,
        SetStd<State*>& Q_S,
        SetStd<State*>& F_S,
        value_function_t finVal
    );

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

void testComputeChildReturnValues(const std::string& filepath) {
    std::cout << "=== Testing Child Return Values Computation ===" << std::endl;
    
    NestedAutomaton* nested = new NestedAutomaton(filepath);
    //std::cout << "Original NestedAutomaton from " << filepath << ":\n";
    //nested->print();
    
    if (nested->getChildrenSize() == 0) {
        std::cout << "No child automata found!" << std::endl;
        delete nested;
        return;
    }
    
    std::cout << "\n=== Testing Different Value Functions ===" << std::endl;
    
    // Test MIN function
    std::cout << "\n--- Testing MIN Function ---" << std::endl;
    for (size_t i = 0; i < nested->getChildrenSize(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        if (!child) continue;
    
        SetStd<weight_t> values_min = computeChildReturnValues(child, Min_f);
        
        std::cout << "Computed MIN return values for Child_" << i << ": {";
        bool first = true;
        for (weight_t val : values_min) {
            if (!first) std::cout << ", ";
            std::cout << val;
            first = false;
        }
        std::cout << "}" << std::endl;

    }
    
    // Test MAX function  
    std::cout << "--- Testing MAX Function ---" << std::endl;
    for (size_t i = 0; i < nested->getChildrenSize(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        if (!child) continue;
        
        SetStd<weight_t> values_max = computeChildReturnValues(child, Max_f);
        
        std::cout << "Computed MAX return values for Child_" << i << ": {";
        bool first = true;
        for (weight_t val : values_max) {
            if (!first) std::cout << ", ";
            std::cout << val;
            first = false;
        }
        std::cout << "}" << std::endl;

    }
    
    // Test SumB function
    std::cout << "--- Testing SumB Function (bound=10) ---" << std::endl;
    weight_t bound = 10;
    for (size_t i = 0; i < nested->getChildrenSize(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        if (!child) continue;
        
        //std::cout << "Child " << i << " (SumB with bound=" << bound << "):" << std::endl;
        //child->print(); // Print the automaton structure
        
        SetStd<weight_t> sumB_values = computeChildReturnValues(child, SumB, bound);
        
        std::cout << "Computed SumB return values for Child_" << i << ": {";
        bool first = true;
        for (weight_t val : sumB_values) {
            if (!first) std::cout << ", ";
            std::cout << val;
            first = false;
        }
        std::cout << "}" << std::endl;
    }
    
    delete nested;
}

void testConstructMonitors(const std::string& filepath, value_function_t finVal, weight_t bound = -1) {
    std::cout << "=== Testing constructMonitors Function ===" << std::endl;
    std::cout << "File: " << filepath << std::endl;
    std::cout << "Value function: " << (finVal == Min_f ? "Min_f" : 
                                      finVal == Max_f ? "Max_f" : 
                                      finVal == SumB ? "SumB" : "Unknown") << std::endl;
    if (finVal == SumB) std::cout << "Bound: " << bound << std::endl;
    
    NestedAutomaton* nested = new NestedAutomaton(filepath);
    
    if (nested->getChildrenSize() == 0) {
        std::cout << "No child automata found!" << std::endl;
        delete nested;
        return;
    }
    
    // Count actual (non-dummy) children
    size_t actual_children = 0;
    for (size_t i = 0; i < nested->getChildrenSize(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        if (child && !child->getName().empty() && child->getName() != "dummy") {
            actual_children++;
        }
    }
    
    std::cout << "Total children: " << nested->getChildrenSize() << std::endl;
    std::cout << "Actual (non-dummy) children: " << actual_children << std::endl;
    
    // Step 1: Get expected return values
    SetStd<weight_t> global_return_values = computeGlobalReturnValues(nested, finVal, bound);
    
    std::cout << "Expected return values: {";
    bool first = true;
    for (weight_t val : global_return_values) {
        if (!first) std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "}" << std::endl;
    
    // Step 2: Test constructMonitors
    std::vector<std::vector<ChildAutomaton*>> monitors;
    SetStd<State*> Q_S, F_S;
    
    constructMonitors(nested, global_return_values, monitors, Q_S, F_S, finVal);
    
    // Step 3: Verify correct amount
    std::cout << "\n--- Results ---" << std::endl;
    std::cout << "monitors.size(): " << monitors.size() << std::endl;
    
    bool correct_structure = true;
    size_t total_monitors = 0;
    
    for (size_t i = 0; i < monitors.size(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        
        // Check if this is the dummy child (index 0 or has dummy characteristics)
        bool is_dummy = (i == 0) || (child && (child->getName().empty() || child->getName() == "dummy"));
        
        std::cout << "Child " << i;
        if (is_dummy) {
            std::cout << " (dummy)";
        }
        std::cout << ": " << monitors[i].size() << " monitors";
        
        if (is_dummy) {
            // Dummy child should have 0 monitors
            if (monitors[i].size() != 0) {
                std::cout << " ❌ (dummy child should have 0 monitors)";
                correct_structure = false;
            } else {
                std::cout << " ✓ (correctly empty)";
            }
        } else {
            // Real child should have monitors for each return value
            if (monitors[i].size() != global_return_values.size()) {
                std::cout << " ❌ (expected " << global_return_values.size() << ")";
                correct_structure = false;
            } else {
                std::cout << " ✓";
            }
            total_monitors += monitors[i].size();
        }
        std::cout << std::endl;
    }
    
    std::cout << "\nCorrect structure: " << (correct_structure ? "✓" : "❌") << std::endl;
    std::cout << "Total monitors (excluding dummy): " << total_monitors << std::endl;
    std::cout << "Expected total: " << actual_children * global_return_values.size() << std::endl;
    
    // Step 4: Verify weights handled (skip dummy child)
    std::cout << "\n--- Monitor Weight Verification ---" << std::endl;
    std::vector<weight_t> return_values_vec(global_return_values.begin(), global_return_values.end());
    std::sort(return_values_vec.begin(), return_values_vec.end());
    
    for (size_t i = 0; i < monitors.size(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        bool is_dummy = (i == 0) || (child && (child->getName().empty() || child->getName() == "dummy"));
        
        if (is_dummy) {
            std::cout << "Child " << i << " (dummy): skipped" << std::endl;
            continue;
        }
        
        std::cout << "Child " << i << " monitors:" << std::endl;
        for (size_t j = 0; j < monitors[i].size(); ++j) {
            if (monitors[i][j]) {
                std::string name = monitors[i][j]->getName();
                weight_t expected_weight = return_values_vec[j];
                std::cout << "  [" << j << "]: " << name << " (should handle weight " << expected_weight << ")" << std::endl;
            } else {
                std::cout << "  [" << j << "]: NULL monitor ❌" << std::endl;
            }
        }
    }
    
    // Step 5: Quick cleanup
    for (size_t i = 0; i < monitors.size(); ++i) {
        for (size_t j = 0; j < monitors[i].size(); ++j) {
            delete monitors[i][j];
        }
    }
    
    delete nested;
    std::cout << "\n=== Test Completed ===" << std::endl;
}