#include <iostream>
#include <cstring>
#include "Automaton.h"

value_function_t getValueFunction(const char *str) {
#define CMP(S) ((strncmp(str, (S), sizeof(S))) == 0)
    if (CMP("Inf")) { return Inf; }
    if (CMP("Sup")) { return Sup; }
    if (CMP("LimInf")) { return LimInf; }
    if (CMP("LimSup")) { return LimSup; }
    if (CMP("LimInfAvg")) { return LimInfAvg; }
    if (CMP("LimSupAvg")) { return LimSupAvg; }
    if (CMP("Avg")) { return Avg; }
#undef CMP

    std::cerr << "Unknown value function: " << str << "\n";
    abort();
}

const char *valueFunctionToStr(value_function_t v) {
  switch(v) {
    case Inf: return "Inf";
    case Sup: return "Sup";
    case LimInf: return "LimInf";
    case LimSup: return "LimSup";
    case LimInfAvg: return "LimInfAvg";
    case LimSupAvg: return "LimSupAvg";
    case Avg: return "Avg";
    default: break;
  }
  abort();
}

value_function_t getFiniteAggregator(const char *str) {
#define CMP(S) ((strncmp(str, (S), sizeof(S))) == 0)
    if (CMP("Max_f")) { return Max_f; }
    if (CMP("Min_f")) { return Min_f; }
    if (CMP("SumB")) { return SumB; }
    if (CMP("SumPlus")) { return SumPlus; }
    if (CMP("SumMinus")) { return SumMinus; }
    if (CMP("Avg")) { return Avg; }
#undef CMP

    std::cerr << "Unknown finite aggregator: " << str << "\n";
    std::cerr << "Valid aggregators: Max_f, Min_f, SumB, SumPlus, SumMinus, Avg\n";
    abort();
}

const char *finiteAggregatorToStr(value_function_t v) {
  switch(v) {
    case Max_f: return "Max_f";
    case Min_f: return "Min_f";
    case SumB: return "SumB";
    case SumPlus: return "SumPlus";
    case SumMinus: return "SumMinus";
    case Avg: return "Avg";
    default: break;
  }
  abort();
}

