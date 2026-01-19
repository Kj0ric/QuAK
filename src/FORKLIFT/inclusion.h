#ifndef INCLUSION_H_
#define INCLUSION_H_


#include "../Automaton.h"


bool inclusion (const Automaton* A, const Automaton* B, UltimatelyPeriodicWord** witness = nullptr);
bool membership (Automaton* A, Word* stem, Word* period, weight_t threshold);

void debug_test();
void debug_test2();
void debug_test3();


#endif /* INCLUSION_H_ */
