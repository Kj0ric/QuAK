#include <iostream>
#include "NestedAutomaton.h"

int main() {
    // Part 1: request_response with SumPlus (response time).
    // Each child counts r's until g via SumPlus. The last request
    // before each grant always waits 1 step, so Inf/LimInf should
    // differ from Sup/LimSup at higher thresholds.
    {
        NestedAutomaton* nwa = new NestedAutomaton("examples/request_response.txt");
        std::cout << "=== request_response: isNonEmpty(infVal, SumPlus, t) ===" << std::endl;
        std::cout << "  t   Sup  LSup  LSAvg  LInf  Inf" << std::endl;
        for (int t = 0; t <= 4; t++) {
            weight_t threshold(t);
            std::cout << "  " << t << "    "
                      << (nwa->isNonEmpty(Sup, SumPlus, threshold) ? "T" : "F") << "    "
                      << (nwa->isNonEmpty(LimSup, SumPlus, threshold) ? "T" : "F") << "     "
                      << (nwa->isNonEmpty(LimSupAvg, SumPlus, threshold) ? "T" : "F") << "      "
                      << (nwa->isNonEmpty(LimInf, SumPlus, threshold) ? "T" : "F") << "     "
                      << (nwa->isNonEmpty(Inf, SumPlus, threshold) ? "T" : "F") << std::endl;
        }
        delete nwa;
    }

    // Part 2: priority_tasks with Max_f.
    // High-priority children have Max_f = 4, low-priority have Max_f = 2.
    // Sup/LimSup only need SOME child to reach the threshold,
    // LimInf/Inf need ALL children (eventually/always) to reach it.
    {
        NestedAutomaton* nwa = new NestedAutomaton("examples/priority_tasks.txt");
        std::cout << std::endl << "=== priority_tasks: isNonEmpty(infVal, Max_f, t) ===" << std::endl;
        std::cout << "  t   Sup  LSup  LInf  Inf" << std::endl;
        for (int t = 0; t <= 6; t++) {
            weight_t threshold(t);
            std::cout << "  " << t << "    "
                      << (nwa->isNonEmpty(Sup, Max_f, threshold) ? "T" : "F") << "    "
                      << (nwa->isNonEmpty(LimSup, Max_f, threshold) ? "T" : "F") << "     "
                      << (nwa->isNonEmpty(LimInf, Max_f, threshold) ? "T" : "F") << "     "
                      << (nwa->isNonEmpty(Inf, Max_f, threshold) ? "T" : "F") << std::endl;
        }
        delete nwa;
    }

    return 0;
}
