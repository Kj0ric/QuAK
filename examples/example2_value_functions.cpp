/**
 * Example 2: Value Function Combinations
 *
 * Demonstrates how different (infVal, finVal) combinations produce
 * contrasting TRUE/FALSE results on the same nested automaton.
 *
 * Automaton story: A system alternates between high-priority (h) and
 * low-priority (l) tasks. The child automaton measures each task's cost.
 * h-tasks produce high weights (3, 2); l-tasks produce low weights (0).
 *
 * Build:
 *   cmake --build build --target example2_value_functions -j
 *
 * Run from project root:
 *   ./build/example2_value_functions
 */

#include <iostream>
#include "NestedAutomaton.h"

int main() {
    std::cout << "=== Example 2: Value Function Combinations ===" << std::endl;
    std::cout << std::endl;

    std::string filepath = "examples/priority_tasks.txt";
    NestedAutomaton* nwa = new NestedAutomaton(filepath);

    std::cout << "Loaded: " << filepath << std::endl;
    std::cout << "System alternates h/l tasks; child measures cost per task." << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Comparison 1: Varying finVal (threshold=4, infVal=Sup)
    // =========================================================================
    // Sup only needs one accepted word to hit the threshold.
    // Question: which finVal can reach 4?
    std::cout << "--- Comparison 1: Varying finVal (Sup, threshold=4) ---" << std::endl;
    std::cout << std::endl;

    weight_t t1 = 4;

    bool supMax = nwa->isNonEmpty(Sup, Max_f, t1);
    std::cout << "  Sup + Max_f  >= 4 : " << (supMax ? "TRUE" : "FALSE") << std::endl;

    bool supSum = nwa->isNonEmpty(Sup, SumPlus, t1);
    std::cout << "  Sup + SumPlus >= 4 : " << (supSum ? "TRUE" : "FALSE") << std::endl;

    bool supSumB = nwa->isNonEmpty(Sup, SumB, t1, 3);
    std::cout << "  Sup + SumB(3) >= 4 : " << (supSumB ? "TRUE" : "FALSE") << std::endl;

    // =========================================================================
    // Comparison 2: Non-emptiness vs Universality (Sup + Max_f)
    // =========================================================================
    // Non-emptiness = "can we achieve this?" vs Universality = "is it guaranteed?"
    std::cout << "--- Comparison 2: Non-emptiness vs Universality (Sup + Max_f) ---" << std::endl;
    std::cout << std::endl;

    bool ne3 = nwa->isNonEmpty(Sup, Max_f, weight_t(3));
    std::cout << "  isNonEmpty(Sup, Max_f, 3)  : " << (ne3 ? "TRUE" : "FALSE") << std::endl;

    bool u3 = nwa->isUniversal(Sup, Max_f, weight_t(3));
    std::cout << "  isUniversal(Sup, Max_f, 3) : " << (u3 ? "TRUE" : "FALSE") << std::endl;

    bool ne1 = nwa->isNonEmpty(Sup, Max_f, weight_t(1));
    std::cout << "  isNonEmpty(Sup, Max_f, 1)  : " << (ne1 ? "TRUE" : "FALSE") << std::endl;

    bool u1 = nwa->isUniversal(Sup, Max_f, weight_t(1));
    std::cout << "  isUniversal(Sup, Max_f, 1) : " << (u1 ? "TRUE" : "FALSE") << std::endl;

    delete nwa;

    std::cout << std::endl;
    std::cout << "Example 2 complete." << std::endl;
    return 0;
}
