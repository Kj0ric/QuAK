#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <chrono>

#include "Automaton.h"
#include "ChildAutomaton.h"
#include "NestedAutomaton.h"
#include "FORKLIFT/inclusion.h"
#include "Monitor.h"

#include "functionality_tests.h"
#include "dfa_minimal_check.h"
#include "utility.h"

int main(int argc, char* argv[]) {
    // std::string filepath = "/home/ege/Desktop/QuAK-playground/samples/nested/avg_resp/avg_resp_1_1.txt";
    std::string filepath = "/home/ege/Desktop/QuAK-playground/samples/nested/test_liminf/test2.txt";
    NestedAutomaton* nested = new NestedAutomaton(filepath);
    nested->print();
    
    weight_t threshold = 1;
    const int NUM_RUNS = 1;

    using clock = std::chrono::high_resolution_clock;
    double total_ms = 0.0;
    bool flag = false;

    for (int i = 0; i < NUM_RUNS; ++i) {
        auto start = clock::now();
        // flag = nested->emptiness_monotonic_nesting_supremum(LimSup, SumPlus, threshold);
        flag = nested->emptiness_monotonic_nesting(LimInf, SumPlus, threshold);
        // flag = nested->emptiness_Avg_SumPlus(LimSupAvg, threshold);
        auto end = clock::now();

        std::chrono::duration<double, std::milli> diff = end - start;
        total_ms += diff.count();
    }

    double avg_ms = total_ms / NUM_RUNS;

    std::cout
        << "test on threshold = "
        << std::to_string(int(threshold))
        << " returns: " << (flag ? "NON-EMPTY" : "EMPTY")
        << " | average running time over " << NUM_RUNS << " runs: "
        << avg_ms/1000 << " s"
        << std::endl;

    delete nested;

    // std::string filepath = "/home/ege/Desktop/QuAK-playground/samples/nested/parent_aware/max1.txt";
    // NestedAutomaton* nested = new NestedAutomaton(filepath);
    // nested->print();

    // auto printSet = [](const std::string& label, const SetStd<weight_t>& S) {
    //     std::cerr << label << " {";
    //     bool first = true;
    //     for (const auto& v : S) {
    //         if (!first) std::cerr << ", ";
    //         first = false;
    //         std::cerr << v;
    //     }
    //     std::cerr << "}\n";
    // };

    // const size_t child_idx = 1;

    // // Min/Max do not need a bound (pass 0).
    // SetStd<weight_t> minVals = nested->debug_computeChildReturnValuesParentAware(child_idx, Min_f, weight_t(0));
    // SetStd<weight_t> maxVals = nested->debug_computeChildReturnValuesParentAware(child_idx, Max_f, weight_t(0));

    // // SumB needs a bound (pick something safely >= max |sum| you expect).
    // SetStd<weight_t> sumVals = nested->debug_computeChildReturnValuesParentAware(child_idx, SumB, weight_t(100));

    // printSet("Min_f:", minVals);
    // printSet("Max_f:", maxVals);
    // printSet("SumB :", sumVals);

    // delete nested;

    // std::string filepath = "/home/ege/Desktop/QuAK-playground/samples/pseudo_determ/oneChild.txt";
    // NestedAutomaton* nested = new NestedAutomaton(filepath);
    // auto macro_alphabet = nested->generateMacroAlphabet();
    // NestedAutomaton* det_nwa = nested->determinizeWithMacroAlphabet(macro_alphabet);
    // NestedAutomaton* sync_nwa = det_nwa->synchronizeChildren(macro_alphabet);
    // Automaton* flat_automaton = sync_nwa->flatten();

    // nested->print(true);
    // det_nwa->print(true);
    // sync_nwa->print(true);
    
    // auto start = std::chrono::high_resolution_clock::now();
    // auto topFlat = flat_automaton->getTopValue(LimInfAvg);
    // auto stop = std::chrono::high_resolution_clock::now();
    // auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    // std::cout << "Top value of flattened automaton: " << topFlat.to_string() << std::endl;
    // std::cout << "Computation time: " << duration_ms.count()/1000 << " s" << std::endl;
    
    // delete nested;
    // delete det_nwa;
    // delete sync_nwa;
    // delete flat_automaton;

    /*
    std:: size_t i = 1;              // Adjust
    weight_t j = 2;                 // Adjust

    std::vector<std::pair<value_function_t, weight_t>> tests = {
        //{Min_f, -1},    // No bound needed
        //{Max_f, -1},    // No bound needed  
        {SumB, 1}     
    };
    
    for (auto [finVal, bound] : tests) {

        //std::cout << "Testing transformToBuchi with:" << std::endl;
        std::cout << "File: " << filepath << std::endl;
        std::cout << "Value function: " << (finVal == Min_f ? "Min_f" : 
                                      finVal == Max_f ? "Max_f" : "SumB") << std::endl;
        if (finVal == SumB) std::cout << " (bound=" << bound << ")";
        std::cout << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        // Test return values computation
        //testConstructMonitors(filepath, finVal, bound);
        //testCompareOldVsNewReturnValues(filepath, finVal, bound);

        // Test monitors constructed
        //testS_ijConstruction(filepath, i, j, finVal, bound);

        testTransformToBuchi(filepath, finVal, bound);
    }
    */
    
    return 0;
}
