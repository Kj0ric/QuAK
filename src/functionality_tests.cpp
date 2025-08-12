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
        value_function_t finVal,
        weight_t bound
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
    
    // Step 1: Compute global and per-child return values
    SetStd<weight_t> global_return_values = computeGlobalReturnValues(nested, finVal, bound);
    
    std::cout << "Global return values: {";
    bool first = true;
    for (weight_t val : global_return_values) {
        if (!first) std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "}" << std::endl;
    
    // Compute expected monitors per child
    std::vector<SetStd<weight_t>> child_return_values(nested->getChildrenSize());
    size_t total_expected_monitors = 0;
    
    for (size_t i = 0; i < nested->getChildrenSize(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        if (child && child->getName() != "dummy" && !child->getName().empty()) {
            child_return_values[i] = computeChildReturnValues(child, finVal, bound);
            total_expected_monitors += child_return_values[i].size();
            
            std::cout << "Child " << i << " can return: {";
            bool child_first = true;
            for (weight_t val : child_return_values[i]) {
                if (!child_first) std::cout << ", ";
                std::cout << val;
                child_first = false;
            }
            std::cout << "}" << std::endl;
        } else {
            std::cout << "Child " << i << " (dummy): no monitors expected" << std::endl;
        }
    }
    
    // Step 2: Test constructMonitors
    std::vector<std::vector<ChildAutomaton*>> monitors;
    SetStd<State*> Q_S, F_S;
    
    constructMonitors(nested, global_return_values, monitors, Q_S, F_S, finVal, bound);
    
    // Step 3: Verify correct count
    std::cout << "\n--- Monitor Count Verification ---" << std::endl;
    
    bool correct_count = true;
    size_t total_actual_monitors = 0;
    
    for (size_t i = 0; i < monitors.size(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        bool is_dummy = !child || child->getName() == "dummy" || child->getName().empty();
        
        size_t expected_count = is_dummy ? 0 : child_return_values[i].size();
        size_t actual_count = monitors[i].size();
        
        std::cout << "Child " << i;
        if (is_dummy) std::cout << " (dummy)";
        std::cout << ": " << actual_count << " monitors";
        
        if (actual_count == expected_count) {
            std::cout << " ✓";
        } else {
            std::cout << " ❌ (expected " << expected_count << ")";
            correct_count = false;
        }
        std::cout << std::endl;
        
        total_actual_monitors += actual_count;
    }
    
    std::cout << "\nTotal monitors created: " << total_actual_monitors << std::endl;
    std::cout << "Total monitors expected: " << total_expected_monitors << std::endl;
    std::cout << "Count verification: " << (correct_count && total_actual_monitors == total_expected_monitors ? "✓" : "❌") << std::endl;
    
    // Step 4: Verify correct values (sample a few monitors)
    std::cout << "\n--- Monitor Value Verification ---" << std::endl;
    
    for (size_t i = 0; i < monitors.size(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        if (!child || child->getName() == "dummy" || child->getName().empty()) {
            continue;
        }
        
        std::cout << "Child " << i << " monitors:" << std::endl;
        
        // Convert child return values to sorted vector for indexing
        std::vector<weight_t> child_values_vec(child_return_values[i].begin(), child_return_values[i].end());
        std::sort(child_values_vec.begin(), child_values_vec.end());
        
        for (size_t j = 0; j < monitors[i].size(); ++j) {
            if (monitors[i][j]) {
                std::string monitor_name = monitors[i][j]->getName();
                weight_t expected_value = child_values_vec[j];
                std::cout << "  [" << j << "]: " << monitor_name 
                          << " (handles value " << expected_value << ")" << std::endl;
            } else {
                std::cout << "  [" << j << "]: NULL monitor ❌" << std::endl;
            }
        }
    }
    
    // Step 5: Cleanup
    for (size_t i = 0; i < monitors.size(); ++i) {
        for (size_t j = 0; j < monitors[i].size(); ++j) {
            delete monitors[i][j];
        }
    }
    
    delete nested;
    std::cout << "\n=== Test Completed ===" << std::endl;
}