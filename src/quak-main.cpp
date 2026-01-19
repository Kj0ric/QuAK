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

// Struct to hold test configuration
struct TestConfig {
    std::string name;
    value_function_t val_func;   // Enum for Inf, Sup, LimInf, LimSup
    value_function_t agg_func;     // Enum for Max_f, Min_f
};

int main(int argc, char* argv[]) {
	std::string myfilepathA(argv[1]);
	Automaton* A = new Automaton(myfilepathA);
	
	A->getStates()->at(2)->setFinal(true);
	A->getStates()->at(3)->setFinal(true);
	A->print();

	Word* stem = new Word(new Word(A->getAlphabet()->at(0)), A->getAlphabet()->at(0));
	Word* period = new Word(A->getAlphabet()->at(0));
	bool member = membership(A, stem, period, 0);

	if (member == true) {
		std::cout << "THE WORD " << stem->toString() << "(" << period->toString() << ") IS MEMBER with threshold 0\n";
	}
	else {
		std::cout << "THE WORD " << stem->toString() << "(" << period->toString() << ") IS NOT MEMBER with threshold 0\n";
	}


	Word* stem_ = new Word(new Word(A->getAlphabet()->at(0)), A->getAlphabet()->at(0));
	Word* period_ = new Word(A->getAlphabet()->at(1));
	bool member_ = membership(A, stem_, period_, 1);

	if (member_ == true) {
		std::cout << "THE WORD " << stem_->toString() << "(" << period_->toString() << ") IS MEMBER  with threshold 1\n";
	}
	else {
		std::cout << "THE WORD " << stem_->toString() << "(" << period_->toString() << ") IS NOT MEMBER with threshold 1\n";
	}

	
	std::string myfilepathB(argv[2]);
	Automaton* B = new Automaton(myfilepathB);
	
	B->getStates()->at(3)->setFinal(true);
	B->getStates()->at(4)->setFinal(true);
	B->getStates()->at(5)->setFinal(true);
	B->getStates()->at(6)->setFinal(true);
	B->print();

	bool included = inclusion(A, B);
	if (included == true) {
		std::cout << "L(A) IN L(B)\n";
	}
	else {
		std::cout << "L(A) NOT IN L(B)\n";
	}

	bool rev_included = inclusion(B, A);
	if (rev_included == true) {
		std::cout << "L(B) IN L(A)\n";
	}
	else {
		std::cout << "L(B) NOT IN L(A)\n";
	}


	return 0;
	




    // // std::string filepath = "/home/ege/Desktop/QuAK-playground/samples/nested/avg_resp/avg_resp_5_5.txt";
    // std::string filepath = "/home/ege/Desktop/QuAK-playground/samples/nested/test_liminf/test3.txt";
    // NestedAutomaton* nested = new NestedAutomaton(filepath);
    // nested->print();
    
    // weight_t threshold = 2;
    // const int NUM_RUNS = 1;

    // using clock = std::chrono::high_resolution_clock;
    // double total_ms = 0.0;
    // bool flag = false;

    // for (int i = 0; i < NUM_RUNS; ++i) {
    //     auto start = clock::now();
    //     // flag = nested->emptiness_monotonic_nesting_supremum(Sup, SumPlus, threshold);
    //     // flag = nested->emptiness_monotonic_nesting(LimInf, SumPlus, threshold);
    //     // flag = nested->emptiness_Avg_SumPlus(LimSupAvg, threshold);
    //     flag = nested->emptiness_monotonic_nesting_min_max(LimInf, Max_f, threshold);
    //     auto end = clock::now();

    //     std::chrono::duration<double, std::milli> diff = end - start;
    //     total_ms += diff.count();
    // }

    // double avg_ms = total_ms / NUM_RUNS;

    // std::cout
    //     << "test on threshold = "
    //     << std::to_string(int(threshold))
    //     << " returns: " << (flag ? "NON-EMPTY" : "EMPTY")
    //     << " | average running time over " << NUM_RUNS << " runs: "
    //     << avg_ms/1000 << " s"
    //     << std::endl;

    // delete nested;

    const std::string base_path = "/home/ege/Desktop/QuAK-playground/samples/nested/test_liminf/";
        const int NUM_RUNS = 1;

        // 1. Define the combinations (Order: Configs -> Files -> Thresholds)
        std::vector<TestConfig> configs = {
            {"Inf / Min",    Inf,    Min_f},
            {"Inf / Max",    Inf,    Max_f},
            {"Sup / Min",    Sup,    Min_f},
            {"Sup / Max",    Sup,    Max_f},
            {"LimInf / Min", LimInf, Min_f},
            {"LimInf / Max", LimInf, Max_f},
            {"LimSup / Min", LimSup, Min_f},
            {"LimSup / Max", LimSup, Max_f}
        };

        // 2. Define specific thresholds for each file
        std::map<std::string, std::vector<weight_t>> file_thresholds = {
            {"test1.txt", {1, 2}},      // Modify these values as needed
            {"test2.txt", {0, 1}},
            {"test3.txt", {0, 1, 2}},
            {"test4.txt", {0, 1, 2}},
            {"test5.txt", {1, 2, 3}},
            {"test6.txt", {1, 2, 3}}
        };

        using clock = std::chrono::high_resolution_clock;

        // ---------------------------------------------------------
        // OUTER LOOP: Iterate through Aggregator Combinations first
        // ---------------------------------------------------------
        for (const auto& config : configs) {
            
            std::cout << "\n==========================================" << std::endl;
            std::cout << " Running Tests for: " << config.name << std::endl;
            std::cout << "==========================================" << std::endl;
            std::cout << std::left << std::setw(12) << "File" 
                    << std::setw(10) << "Thresh" 
                    << std::setw(12) << "Result" 
                    << "Time (s)" << std::endl;
            std::cout << std::string(45, '-') << std::endl;

            // MIDDLE LOOP: Iterate through files test1 to test5
            for (int f = 1; f <= 6; ++f) {
                std::string filename = "test" + std::to_string(f) + ".txt";
                std::string fullpath = base_path + filename;
                
                // Skip if no thresholds defined for this file
                if (file_thresholds.find(filename) == file_thresholds.end()) continue;
                const auto& thresholds = file_thresholds[filename];

                NestedAutomaton* nested = nullptr;
                try {
                    nested = new NestedAutomaton(fullpath);
                } catch (const std::exception& e) {
                    std::cerr << "Error loading " << filename << ": " << e.what() << std::endl;
                    continue;
                }

                // INNER LOOP: Iterate through thresholds
                for (weight_t threshold : thresholds) {
                    double total_ms = 0.0;
                    bool flag = false;

                    for (int i = 0; i < NUM_RUNS; ++i) {
                        auto start = clock::now();
                        
                        flag = nested->emptiness_monotonic_nesting_min_max(config.val_func, config.agg_func, threshold);
                        
                        auto end = clock::now();
                        std::chrono::duration<double, std::milli> diff = end - start;
                        total_ms += diff.count();
                    }

                    double avg_s = (total_ms / NUM_RUNS) / 1000.0;

                    std::cout << std::left << std::setw(12) << filename 
                            << std::setw(10) << (int)threshold 
                            << std::setw(12) << (flag ? "NON-EMPTY" : "EMPTY") 
                            << avg_s << std::endl;
                }

                // Clean up automaton before moving to next file (or re-loading same file for next config later)
                delete nested;
            }
        }


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
