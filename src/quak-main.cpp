#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>

#include "Automaton.h"
#include "ChildAutomaton.h"
#include "NestedAutomaton.h"
#include "FORKLIFT/inclusion.h"
#include "Monitor.h"

#include "functionality_tests.cpp"
#include "utility.h"

int main(int argc, char* argv[]) {
    std::string filepath = argv[1];
    
    std:: size_t i = 1;             // Adjust
    weight_t j = -1;                 // Adjust

    std::vector<std::pair<value_function_t, weight_t>> tests = {
        //{Min_f, -1},    // No bound needed
        //{Max_f, -1},    // No bound needed  
        {SumB, 5}       // Small bound for testing
    };
    
    for (auto [finVal, bound] : tests) {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "Testing transformToBuchi with:" << std::endl;
        std::cout << "File: " << filepath << std::endl;
        std::cout << "Value function: " << (finVal == Min_f ? "Min_f" : 
                                      finVal == Max_f ? "Max_f" : "SumB") << std::endl;
        if (finVal == SumB) std::cout << " (bound=" << bound << ")";
        std::cout << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        // Test return values computation
        //testComputeChildReturnValues(filepath, finVal, bound);
        // Test S_ij construction for child 1, return value 1
        //testS_ijConstruction(filepath, 1, 1, finVal, bound);
        
        testTransformToBuchi(filepath, finVal, bound);
    }

    //testComputeChildReturnValues(filepath);
    //testConstructMonitors(filepath, g, bound);
    
    

    
    

    return 0;
}
