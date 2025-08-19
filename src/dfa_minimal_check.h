#ifndef DFA_MIN_CHECK_H_
#define DFA_MIN_CHECK_H_

#include "Automaton.h"
#include "ChildAutomaton.h"
#include "NestedAutomaton.h"
#include "Weight.h"
#include "Set.h"
#include <string>

// DFA minimality analysis functions
bool allStatesReachable(const ChildAutomaton* dfa);
bool allStatesDistinguishable(const ChildAutomaton* dfa);
bool isMinimalDFA(const ChildAutomaton* dfa);
SetStd<SetStd<State*>> getEquivalenceClasses(const ChildAutomaton* dfa);
void analyzeMonitorMinimality(const std::string& filepath, value_function_t finVal, weight_t bound = -1);

#endif /* DFA_MIN_CHECK_H_ */