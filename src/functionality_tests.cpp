#include <iostream>
#include <string>
#include <iomanip>
#include "Parser.h"
#include "Automaton.h"
#include "ChildAutomaton.h"
#include "NestedAutomaton.h"
#include "Set.h"
#include "Weight.h"
#include "utility.h"

#include <queue>
#include <unordered_set>

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
    //nested->print();

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

    std::cout << "\n=== DETAILED S_ij ANALYSIS ===" << std::endl;

    // Analyze the S_ij construction
    ChildAutomaton* S_ij = child->determiniseToS_ij(child_index, j, g, bound);
    
    std::cout << "S_{" << child_index << "," << j << "} Analysis:" << std::endl;
    std::cout << "- Total states: " << S_ij->getStates()->size() << std::endl;
    std::cout << "- Final states: " << S_ij->getFinalStates()->size() << std::endl;
    std::cout << "- Alphabet size: " << S_ij->getAlphabetSize() << std::endl;
    
    S_ij->print();
    
    delete S_ij;
    delete nested;
}

void testComputeChildReturnValues(const std::string& filepath, value_function_t finVal, weight_t bound) {
    std::cout << "=== Testing Child Return Values Computation ===" << std::endl;
    
    NestedAutomaton* nested = new NestedAutomaton(filepath);
    //std::cout << "Original NestedAutomaton from " << filepath << ":\n";
    //nested->print();
    
    if (nested->getChildrenSize() == 0) {
        std::cout << "No child automata found!" << std::endl;
        delete nested;
        return;
    }
    
    for (size_t i = 0; i < nested->getChildrenSize(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        if (!child) continue;
    
        SetStd<weight_t> values = computeChildReturnValues(child, finVal, bound);
        
        std::cout << "Computed return values for Child_" << i << ": {";
        bool first = true;
        for (weight_t val : values) {
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
    
    // Step 2: Test constructMonitors with MAP-BASED structure
    MapStd<MonitorKey, ChildAutomaton*> monitors;
    SetStd<State*> Q_S, F_S;
    constructMonitors(nested, global_return_values, monitors, Q_S, F_S, finVal, bound);
    
    // Step 3: Verify correct count using MAP iteration
    std::cout << "\n--- Monitor Count Verification ---" << std::endl;
    
    // Count monitors per child using the map
    std::vector<size_t> actual_counts(nested->getChildrenSize(), 0);
    for (const auto& [key, monitor] : monitors) {
        size_t child_index = key.first;  // Extract child index from MonitorKey
        if (child_index < actual_counts.size()) {
            actual_counts[child_index]++;
        }
    }
    
    bool correct_count = true;
    size_t total_actual_monitors = monitors.size();
    
    for (size_t i = 0; i < nested->getChildrenSize(); ++i) {
        ChildAutomaton* child = nested->getChild(i);
        bool is_dummy = !child || child->getName() == "dummy" || child->getName().empty();
        
        size_t expected_count = is_dummy ? 0 : child_return_values[i].size();
        size_t actual_count = actual_counts[i];
        
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
    }
    
    std::cout << "\nTotal monitors created: " << total_actual_monitors << std::endl;
    std::cout << "Total monitors expected: " << total_expected_monitors << std::endl;
    std::cout << "Count verification: " << (correct_count && total_actual_monitors == total_expected_monitors ? "✓" : "❌") << std::endl;
    
    
    // Step 5: Cleanup
    for (const auto& [key, monitor] : monitors) {
        delete monitor;
    }
    
    delete nested;
    std::cout << "\n=== Test Completed ===" << std::endl;
}

void testAllMonitorsConstruction(const std::string& filepath, value_function_t finVal, weight_t bound = -1) {
    std::cout << "=== Testing ALL Monitors Construction ===" << std::endl;
    std::cout << "File: " << filepath << std::endl;
    std::cout << "Value function: " << (finVal == Min_f ? "Min_f" : 
                                      finVal == Max_f ? "Max_f" : 
                                      finVal == SumB ? "SumB" : "Unknown") << std::endl;
    if (finVal == SumB) std::cout << "Bound: " << bound << std::endl;
    
    try {
        // Load the nested automaton
        NestedAutomaton* nested = new NestedAutomaton(filepath);
        
        if (nested->getChildrenSize() == 0) {
            std::cout << "No child automata found!" << std::endl;
            delete nested;
            return;
        }

        // Compute global return values
        SetStd<weight_t> global_values = computeGlobalReturnValues(nested, finVal, bound);
        std::cout << "\nGlobal return values: {";
        for (weight_t val : global_values) {
            std::cout << val << " ";
        }
        std::cout << "} (count: " << global_values.size() << ")" << std::endl;

        // Construct all monitors
        MapStd<MonitorKey, ChildAutomaton*> monitors;
        SetStd<State*> Q_S, F_S;
        constructMonitors(nested, global_values, monitors, Q_S, F_S, finVal, bound);
        
        std::cout << "\nTotal monitors created: " << monitors.size() << std::endl;
        std::cout << "Total monitor states (Q_S): " << Q_S.size() << std::endl;
        std::cout << "Total final monitor states (F_S): " << F_S.size() << std::endl;

        // Individual monitor details
        std::cout << "\n--- Individual Monitor Details ---" << std::endl;
        for (const auto& [key, monitor] : monitors) {
            size_t states = monitor->getStates()->size();
            size_t final_states = monitor->getFinalStates()->size();
            
            std::cout << "Monitor S_" << key.first << "^" << key.second << ": " 
                      << states << " states, " << final_states << " final" << std::endl;
        }

        // Cleanup
        for (const auto& [key, monitor] : monitors) {
            delete monitor;
        }
        
        delete nested;
        std::cout << "\n=== Test Completed ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Test failed with unknown exception" << std::endl;
    }
}

/*
void testCompareOldVsNewReturnValues(const std::string& filepath, value_function_t finVal, weight_t bound = -1) {
    std::cout << "=== Comparing OLD vs NEW Return Values ===" << std::endl;
    std::cout << "File: " << filepath << std::endl;
    std::cout << "Function: " << (finVal == Min_f ? "Min_f" : 
                                 finVal == Max_f ? "Max_f" : 
                                 finVal == SumB ? "SumB" : "Unknown") << std::endl;
    if (finVal == SumB) std::cout << "Bound: " << bound << std::endl;
    
    try {
        NestedAutomaton* nested = new NestedAutomaton(filepath);
        
        if (nested->getChildrenSize() == 0) {
            std::cout << "No child automata found." << std::endl;
            delete nested;
            return;
        }

        std::cout << "\n--- Return Values Comparison ---" << std::endl;
        
        size_t total_old_monitors = 0;
        size_t total_new_monitors = 0;

        for (size_t i = 0; i < nested->getChildrenSize(); ++i) {
            ChildAutomaton* child = nested->getChild(i);
            if (!child) continue;

            SetStd<weight_t> old_values = oldComputeChildReturnValues(child, finVal, bound);
            SetStd<weight_t> new_values = computeChildReturnValues(child, finVal, bound);

            total_old_monitors += old_values.size();
            total_new_monitors += new_values.size();

            std::cout << "Child " << i << ": OLD={";
            for (weight_t val : old_values) std::cout << val << " ";
            std::cout << "} (" << old_values.size() << ") NEW={";
            for (weight_t val : new_values) std::cout << val << " ";
            std::cout << "} (" << new_values.size() << ")" << std::endl;
        }

        // Construct monitors with OLD method
        MapStd<MonitorKey, ChildAutomaton*> old_monitors;
        SetStd<State*> old_Q_S, old_F_S;
        
        for (size_t i = 0; i < nested->getChildrenSize(); ++i) {
            ChildAutomaton* child = nested->getChild(i);
            if (!child) continue;

            SetStd<weight_t> old_values = oldComputeChildReturnValues(child, finVal, bound);
            for (weight_t w : old_values) {
                ChildAutomaton* monitor = child->determiniseToS_ij(w, finVal, bound);
                MonitorKey key = {i, w};
                old_monitors.insert(key, monitor);
                
                for (size_t s = 0; s < monitor->getStates()->size(); ++s) {
                    old_Q_S.insert(monitor->getStates()->at(s));
                }
                for (State* s : *(monitor->getFinalStates())) {
                    old_F_S.insert(s);
                }
            }
        }

        // Construct monitors with NEW method
        SetStd<weight_t> new_global_values = computeGlobalReturnValues(nested, finVal, bound);
        MapStd<MonitorKey, ChildAutomaton*> new_monitors;
        SetStd<State*> new_Q_S, new_F_S;
        constructMonitors(nested, new_global_values, new_monitors, new_Q_S, new_F_S, finVal, bound);

        std::cout << "\n--- Monitor Statistics ---" << std::endl;
        std::cout << "Total monitors: OLD=" << old_monitors.size() 
                  << " NEW=" << new_monitors.size() << std::endl;
        std::cout << "Monitor states: OLD=" << old_Q_S.size() 
                  << " NEW=" << new_Q_S.size() << std::endl;
        std::cout << "Final states: OLD=" << old_F_S.size() 
                  << " NEW=" << new_F_S.size() << std::endl;

        if (old_monitors.size() != new_monitors.size()) {
            int diff = (int)old_monitors.size() - (int)new_monitors.size();
            std::cout << "Monitor difference: " << diff << std::endl;
        }

        if (old_Q_S.size() != new_Q_S.size()) {
            int diff = (int)old_Q_S.size() - (int)new_Q_S.size();
            std::cout << "State difference: " << diff << std::endl;
        }

        // Cleanup
        for (const auto& [key, monitor] : old_monitors) {
            delete monitor;
        }
        for (const auto& [key, monitor] : new_monitors) {
            delete monitor;
        }
        
        delete nested;
        
    } catch (const std::exception& e) {
        std::cout << "Test failed: " << e.what() << std::endl;
    }
}
*/

void testTransformToBuchi(const std::string& filepath, value_function_t finVal, weight_t bound = -1) {
    std::cout << "=== Testing transformToBuchi Function ===" << std::endl;
    std::cout << "File: " << filepath << std::endl;
    std::cout << "Value function: " << (finVal == Min_f ? "Min_f" : 
                                      finVal == Max_f ? "Max_f" : 
                                      finVal == SumB ? "SumB" : "Unknown") << std::endl;
    if (finVal == SumB) std::cout << "Bound: " << bound << std::endl;
    
    try {
        // 1. Load the nested automaton
        NestedAutomaton* nested = new NestedAutomaton(filepath);
        std::cout << "\n--- Original Nested Automaton ---" << std::endl;
        //nested->print();
        
        if (nested->getChildrenSize() == 0) {
            std::cout << "❌ No child automata found! Skipping test." << std::endl;
            delete nested;
            return;
        }

        // ===== NEW DIAGNOSTIC TESTS =====
        
        // TEST 1: Analyze Return Values
        std::cout << "\n=== DIAGNOSTIC TEST 1: Return Values Analysis ===" << std::endl;
        
        SetStd<weight_t> global_values = computeGlobalReturnValues(nested, finVal, bound);
        std::cout << "Global return values: {";
        for (weight_t val : global_values) {
            std::cout << val << " ";
        }
        std::cout << "} (count: " << global_values.size() << ")" << std::endl;
        
        // TEST 2: Analyze Monitor Sizes
        std::cout << "\n=== DIAGNOSTIC TEST 2: Monitor Size Analysis ===" << std::endl;
        
        MapStd<MonitorKey, ChildAutomaton*> monitors;
        SetStd<State*> Q_S, F_S;
        constructMonitors(nested, global_values, monitors, Q_S, F_S, finVal, bound);
        
        std::cout << "Total monitors created: " << monitors.size() << std::endl;
        std::cout << "Total monitor states (Q_S): " << Q_S.size() << std::endl;
        std::cout << "Total final monitor states (F_S): " << F_S.size() << std::endl;
        
        // Analyze individual monitor sizes
        size_t max_monitor_states = 0;
        for (const auto& [key, monitor] : monitors) {
            size_t monitor_state_count = monitor->getStates()->size();
            std::cout << "Monitor S_" << key.first << "^" << key.second 
                      << ": " << monitor_state_count << " states" << std::endl;
            max_monitor_states = std::max(max_monitor_states, monitor_state_count);
        }

        // TEST 3: Theoretical State Space Analysis
        std::cout << "\n=== DIAGNOSTIC TEST 3: Theoretical Bounds ===" << std::endl;
        
        size_t parent_states = nested->getStates()->size();
        size_t guess_values = global_values.size();
        
        // Calculate theoretical maximum P1 and P2 sizes
        size_t max_P_size = 1;
        for (size_t i = 0; i < Q_S.size(); ++i) {
            max_P_size *= 2;  // 2^|Q_S| possible subsets
            if (max_P_size > 1000000) {  // Cap to avoid overflow
                max_P_size = 1000000;
                std::cout << "P set size capped at 1M (2^" << Q_S.size() << " would be too large)" << std::endl;
                break;
            }
        }
        
        std::cout << "Parent states: " << parent_states << std::endl;
        std::cout << "Guess values: " << guess_values << std::endl;
        std::cout << "Max theoretical |P1| or |P2|: 2^" << Q_S.size() << " = " << max_P_size << std::endl;
        
        size_t theoretical_max = parent_states * guess_values * max_P_size * max_P_size;
        std::cout << "Theoretical max Büchi states: " << parent_states << " × " << guess_values 
                  << " × " << max_P_size << " × " << max_P_size << " = ";
        if (theoretical_max > 1000000000) {
            std::cout << "HUGE (>1B)" << std::endl;
        } else {
            std::cout << theoretical_max << std::endl;
        }

        // TEST 4: Monitor State Details (only if reasonable size)
        if (monitors.size() <= 12 && max_monitor_states <= 20) {
            std::cout << "\n=== DIAGNOSTIC TEST 4: Monitor Details ===" << std::endl;
            for (const auto& [key, monitor] : monitors) {
                std::cout << "\n--- Monitor S_" << key.first << "^" << key.second << " ---" << std::endl;
                monitor->print();
            }
        } else {
            std::cout << "\n=== DIAGNOSTIC TEST 4: Monitor Details (SKIPPED - too large) ===" << std::endl;
        }

        // ===== END DIAGNOSTIC TESTS =====

        // 2. Transform to Büchi (with progress monitoring)
        std::cout << "\n--- Transforming to Büchi Automaton ---" << std::endl;
        std::cout << "Starting transformation..." << std::endl;
        
        ChildAutomaton* buchi = nested->transformToBuchi(finVal, bound);
        
        // 3. Print the result (limit output if too large)
        std::cout << "\n--- Resulting Büchi Automaton ---" << std::endl;
        
        size_t buchi_state_count = buchi->getStates()->size();
        if (buchi_state_count <= 50) {
            buchi->print();
        } else {
            std::cout << "Büchi automaton too large to print (>" << buchi_state_count << " states)" << std::endl;
        }
        
        // 4. Enhanced Sanity checks
        std::cout << "\n--- Sanity Checks ---" << std::endl;
        std::cout << "✓ Büchi states: " << buchi_state_count << std::endl;
        std::cout << "✓ Büchi alphabet size: " << buchi->getAlphabetSize() << std::endl;
        std::cout << "✓ Büchi weights: " << buchi->getWeights()->size() << std::endl;
        std::cout << "✓ Final states: " << buchi->getFinalStates()->size() << std::endl;
        
        // Check initial state
        if (buchi->getInitial()) {
            std::cout << "✓ Initial state: " << buchi->getInitial()->getName() << std::endl;
        } else {
            std::cout << "❌ No initial state!" << std::endl;
        }
        
        // Check alphabet consistency
        bool alphabet_consistent = (buchi->getAlphabetSize() == nested->getAlphabetSize());
        std::cout << "✓ Alphabet consistency: " << (alphabet_consistent ? "PASS" : "FAIL") << std::endl;
        
        // Check if all states are reachable (basic connectivity test)
        bool has_edges = false;
        for (size_t i = 0; i < buchi->getStates()->size() && !has_edges; ++i) {
            State* state = buchi->getStates()->at(i);
            for (size_t sym = 0; sym < buchi->getAlphabetSize() && !has_edges; ++sym) {
                if (state->getSuccessors(sym)->size() > 0) {
                    has_edges = true;
                }
            }
        }
        std::cout << "✓ Has transitions: " << (has_edges ? "PASS" : "FAIL") << std::endl;

        // TEST 5: State Space Explosion Analysis
        std::cout << "\n=== EXPLOSION ANALYSIS ===" << std::endl;
        
        if (buchi_state_count > 100) {
            std::cout << "⚠️  STATE EXPLOSION DETECTED! (" << buchi_state_count << " states)" << std::endl;
            
            // Likely causes analysis
            if (Q_S.size() > 10) {
                std::cout << "🔍 Likely cause: Too many monitor states (" << Q_S.size() << ")" << std::endl;
                std::cout << "   → P1/P2 subsets exploding exponentially" << std::endl;
            }
            if (global_values.size() > 5) {
                std::cout << "🔍 Likely cause: Too many return values (" << global_values.size() << ")" << std::endl;
            }
            if (max_monitor_states > 5) {
                std::cout << "🔍 Likely cause: Large individual monitors (max: " << max_monitor_states << " states)" << std::endl;
            }
        } else {
            std::cout << "✓ State space reasonable (" << buchi_state_count << " states)" << std::endl;
        }
        
        // Cleanup monitors
        for (const auto& [key, monitor] : monitors) {
            delete monitor;
        }
        
        // Memory cleanup
        delete buchi;
        delete nested;
        std::cout << "\n=== Test Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Test failed with exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "❌ Test failed with unknown exception" << std::endl;
    }
}