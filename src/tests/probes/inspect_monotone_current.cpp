#include <iostream>
#include <stdexcept>
#include <string>

#include "NestedAutomaton.h"

static value_function_t parse_value_function(const std::string& raw) {
    if (raw == "Inf") return Inf;
    if (raw == "Sup") return Sup;
    if (raw == "LimInf") return LimInf;
    if (raw == "LimSup") return LimSup;
    if (raw == "Max_f") return Max_f;
    if (raw == "Min_f") return Min_f;
    if (raw == "SumPlus") return SumPlus;
    if (raw == "SumMinus") return SumMinus;
    throw std::runtime_error("bad value function: " + raw);
}

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: " << argv[0] << " <file> <InfVal> <FinVal> <threshold>\n";
        return 1;
    }

    const std::string file = argv[1];
    const value_function_t infVal = parse_value_function(argv[2]);
    const value_function_t finVal = parse_value_function(argv[3]);
    const weight_t threshold(std::stod(argv[4]));

    NestedAutomaton nwa(file);

    Automaton* flat = nullptr;
    if ((finVal == Max_f || finVal == Min_f) && (infVal == Sup || infVal == LimSup)) {
        flat = nwa.flatten_MinMax_Sup(finVal, threshold);
    } else if ((finVal == Max_f || finVal == Min_f) && (infVal == Inf || infVal == LimInf)) {
        flat = nwa.flatten_MinMax_Inf(finVal, threshold);
    } else if ((finVal == SumPlus || finVal == SumMinus) && (infVal == Sup || infVal == LimSup)) {
        flat = nwa.flatten_SumPlusMinus_Sup(finVal, threshold);
    } else if ((finVal == SumPlus || finVal == SumMinus) && (infVal == Inf || infVal == LimInf)) {
        flat = nwa.flatten_SumPlusMinus_Inf(finVal, threshold);
    } else {
        throw std::runtime_error("unsupported combination");
    }

    std::cout << "=== RAW ===\n";
    flat->print();

    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    std::cout << "=== NONSILENT ===\n";
    non_silent->print();
    std::cout << "=== NONSILENT RESULT ===\n";
    std::cout << non_silent->isNonEmpty_withFinal(infVal, weight_t(1)) << "\n";

    delete non_silent;
    delete flat;
    return 0;
}
