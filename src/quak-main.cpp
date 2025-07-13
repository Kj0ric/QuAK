#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include "FORKLIFT/inclusion.h"
#include "Automaton.h"
#include "Monitor.h"
#include "NestedAutomaton.h"

#include "functionality_tests.cpp"

int main(int argc, char* argv[]) {
    std::string filepath = argv[1];
    testNestedConstruction(filepath);

    return 0;
}
