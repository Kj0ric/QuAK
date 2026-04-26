#include <iostream>
#include <vector>

#include "NestedAutomaton.h"

static bool eval_regular_oracle(NestedAutomaton* nwa,
                                value_function_t infVal,
                                value_function_t finVal,
                                weight_t threshold) {
    Automaton* flat = nwa->flatten_regular(finVal, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);
    delete non_silent;
    delete flat;
    return result;
}

int main() {
    const char* path =
        "src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt";
    NestedAutomaton nwa(path);

    for (weight_t threshold : {weight_t(0.5f), weight_t(1.0f)}) {
        const bool sup_main = nwa.isNonEmpty(Sup, Max_f, threshold);
        const bool limsup_main = nwa.isNonEmpty(LimSup, Max_f, threshold);
        const bool sup_regular = eval_regular_oracle(&nwa, Sup, Max_f, threshold);
        const bool limsup_regular = eval_regular_oracle(&nwa, LimSup, Max_f, threshold);

        std::cout << "threshold=" << threshold.to_float()
                  << " sup_main=" << sup_main
                  << " limsup_main=" << limsup_main
                  << " sup_regular=" << sup_regular
                  << " limsup_regular=" << limsup_regular
                  << "\n";
    }

    return 0;
}
