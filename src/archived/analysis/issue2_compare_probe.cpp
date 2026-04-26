#include <iostream>
#include <string>

#include "tests/sanity_tests/test_common.h"

static bool eval_split_flat(NestedAutomaton* nwa,
                            value_function_t infVal,
                            value_function_t finVal,
                            weight_t threshold) {
    Automaton* flat =
        NestedAutomatonTester::flatten_MinMax_Sup_split_witness(nwa, finVal, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}

static bool eval_regular_flat(NestedAutomaton* nwa,
                              value_function_t infVal,
                              value_function_t finVal,
                              weight_t threshold) {
    Automaton* flat = NestedAutomatonTester::flatten_regular(nwa, finVal, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);
    delete non_silent;
    delete flat;
    return result;
}

static void run_case(const std::string& path,
                     value_function_t infVal,
                     value_function_t finVal,
                     weight_t threshold) {
    NestedAutomaton nwa(path);
    const bool split = eval_split_flat(&nwa, infVal, finVal, threshold);
    const bool regular = eval_regular_flat(&nwa, infVal, finVal, threshold);
    std::cout << path
              << " inf=" << (infVal == Sup ? "Sup" : "LimSup")
              << " fin=" << (finVal == Max_f ? "Max_f" : "Min_f")
              << " threshold=" << threshold.to_float()
              << " split=" << split
              << " regular=" << regular
              << '\n';
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <nested-automaton-file> [more-files...]\n";
        return 2;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string path = argv[i];
        run_case(path, Sup, Max_f, weight_t(1));
        run_case(path, LimSup, Max_f, weight_t(1));
    }
    return 0;
}
