#ifndef QUAK_EXPERIMENTS_UTILS_H_
#define QUAK_EXPERIMENTS_UTILS_H_

#include "Automaton.h"

value_function_t getValueFunction(const char *str);
const char *valueFunctionToStr(value_function_t v);

// Finite aggregator parsing for nested automata
value_function_t getFiniteAggregator(const char *str);
const char *finiteAggregatorToStr(value_function_t v);

#endif // !QUAK_EXPERIMENTS_UTILS_H_
