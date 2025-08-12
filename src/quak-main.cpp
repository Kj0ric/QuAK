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
    weight_t bound = 20;             // Adjust (for SumB)

    /*
    std:: size_t i = 1;             // Adjust
    weight_t j = 3;                 // Adjust
    
    
    std::cout << "Testing determinization of B_{" << i << "}." << std::endl;
    testS_ijConstruction(
        filepath,
        i,
        j,
        g,
        bound
    );
    */

    //testComputeChildReturnValues(filepath);
    testConstructMonitors(filepath, g, bound);

    return 0;
}
