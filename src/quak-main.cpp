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
    
    std:: size_t i = 1;             // Adjust
    weight_t j = 1;                 // Adjust
    value_function_t g = Max_f;     // Adjust
    weight_t bound = 5;             // Adjust (for SumB)
    
    std::cout << "Testing determinization of B_{" << i << "}." << std::endl;
    testS_ijConstruction(
        filepath,
        i,
        j,
        g
    );


    return 0;
}
