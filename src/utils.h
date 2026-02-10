#ifndef QUAK_EXPERIMENTS_UTILS_H_
#define QUAK_EXPERIMENTS_UTILS_H_

#include "Automaton.h"

// Functions for infinite word value functions (Inf, Sup, LimInf, LimSup, LimInfAvg, LimSupAvg)
value_function_t getValueFunction(const char *str);
const char *valueFunctionToStr(value_function_t v);

// Functions for finite word aggregators (Max_f, Min_f, SumB, SumPlus, SumMinus, Avg)
value_function_t getFiniteAggregator(const char *str);
const char *finiteAggregatorToStr(value_function_t v);

#endif // !QUAK_EXPERIMENTS_UTILS_H_
