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

int main(int argc, char* argv[]) {
    std::string filepath = argv[1];
    value_function_t g = SumB;      // Adjust
    weight_t bound = 5;            // Adjust (for SumB)

    std:: size_t i = 1;             // Adjust
    weight_t j = 1;                 // Adjust

    testComputeChildReturnValues(filepath, bound);
    
    std::cout << "Testing determinization of B_{" << i << "}." << std::endl;
    testS_ijConstruction(
        filepath,
        i,
        j,
        g,
        bound
    );

    //testComputeChildReturnValues(filepath);
    //testConstructMonitors(filepath, g, bound);
    /*
    std::cout << "Testing transformToBuchi with:" << std::endl;
    std::cout << "File: " << filepath << std::endl;
    std::cout << "Value function: " << (g == Min_f ? "Min_f" : 
                                      g == Max_f ? "Max_f" : "SumB") << std::endl;
    if (g == SumB) std::cout << "Bound: " << bound << std::endl;
    
    testTransformToBuchi(filepath, g, bound);
    */

    return 0;
}
