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

#include "functionality_tests.cpp"

int main(int argc, char* argv[]) {
    //debug_test3();
    std::string filename = argv[1];

    testSilent(filename);
    return 0;
}
