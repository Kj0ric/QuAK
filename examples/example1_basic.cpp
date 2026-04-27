#include <iostream>
#include "NestedAutomaton.h"

int main() {
    std::cout << "=== Example 1: Basic Nested Automata Analysis ===" << std::endl;

    // simple_counter: parent loops on one state, 'a' (weight 1) spawns a child,
    // 'b' (weight 0) is silent. Child counts a's before b terminates it.
    NestedAutomaton* nwa = new NestedAutomaton("examples/simple_counter.txt");

    // Compare four finVals at two thresholds using Sup (best child value).
    // Max_f = 1 always, Min_f = 0 always, SumPlus = unbounded, SumB(3) caps at 3.
    std::cout << "isNonEmpty(Sup, finVal, threshold):" << std::endl;
    std::cout << std::endl;

    for (int t : {1, 4}) {
        weight_t threshold(t);
        std::cout << "  threshold=" << t << ":  "
                  << "Max_f=" << (nwa->isNonEmpty(Sup, Max_f, threshold) ? "T" : "F") << "  "
                  << "Min_f=" << (nwa->isNonEmpty(Sup, Min_f, threshold) ? "T" : "F") << "  "
                  << "SumPlus=" << (nwa->isNonEmpty(Sup, SumPlus, threshold) ? "T" : "F") << "  "
                  << "SumB(3)=" << (nwa->isNonEmpty(Sup, SumB, threshold, 3) ? "T" : "F")
                  << std::endl;
    }

    delete nwa;
    return 0;
}
