#include <iostream>
#include <string>

#include "tests/sanity_tests/test_common.h"

static const char* inf_name(value_function_t infVal) {
    switch (infVal) {
        case Sup: return "Sup";
        case LimSup: return "LimSup";
        case Inf: return "Inf";
        case LimInf: return "LimInf";
        default: return "?";
    }
}

static const char* fin_name(value_function_t finVal) {
    switch (finVal) {
        case Max_f: return "Max_f";
        case Min_f: return "Min_f";
        default: return "?";
    }
}

static bool eval_regular_flat(NestedAutomaton* nwa,
                              value_function_t infVal,
                              value_function_t finVal,
                              weight_t threshold) {
    Automaton* flat = NestedAutomatonTester::flatten_regular(nwa, finVal);
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
    const bool result = eval_regular_flat(&nwa, infVal, finVal, threshold);
    std::cout << path
              << " inf=" << inf_name(infVal)
              << " fin=" << fin_name(finVal)
              << " threshold=" << threshold.to_float()
              << " regular=" << result
              << '\n';
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <nested-automaton-file> [more-files...]\n";
        return 2;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string path = argv[i];
        for (value_function_t infVal : {Sup, LimSup, Inf, LimInf}) {
            run_case(path, infVal, Max_f, weight_t(1));
            run_case(path, infVal, Min_f, weight_t(1));
        }
    }
    return 0;
}
