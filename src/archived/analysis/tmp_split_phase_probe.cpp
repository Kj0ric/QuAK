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
    std::cout << "FLAT_BEGIN " << (infVal == Sup ? "Sup" : "LimSup") << "\n";
    non_silent->print();
    std::cout << "FLAT_END\n";
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <nested-automaton-file>\n";
        return 2;
    }

    const std::string file = argv[1];
    NestedAutomaton nwa(file);

    const bool sup = eval_split_flat(&nwa, Sup, Max_f, weight_t(1));
    const bool limsup = eval_split_flat(&nwa, LimSup, Max_f, weight_t(1));

    std::cout << "split_witness.sup.max.thr1=" << sup << "\n";
    std::cout << "split_witness.limsup.max.thr1=" << limsup << "\n";
    return 0;
}
