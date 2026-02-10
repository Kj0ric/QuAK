/**
 * test_flatten_regular.cpp
 *
 * Tests for NestedAutomaton::flatten_regular()
 * Supports: SumB, Max_f, Min_f finite aggregators (for LimAvg combinations)
 */

#include "test_common.h"

// ============================================================================
// SumB Tests
// ============================================================================

void test_flatten_regular_SumB_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(SumB, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(SumB, 5)");

    delete flat;
    delete nwa;
}

void test_flatten_regular_SumB_bound_0() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = nwa->flatten_regular(SumB, weight_t(0));
    verifyAutomatonBasics(flat, "flattened with bound 0");
    printAutomatonStats(flat, "flatten_regular(SumB, 0)");

    delete flat;
    delete nwa;
}

void test_flatten_regular_SumB_bound_1() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = nwa->flatten_regular(SumB, weight_t(1));
    verifyAutomatonBasics(flat, "flattened with bound 1");
    printAutomatonStats(flat, "flatten_regular(SumB, 1)");

    delete flat;
    delete nwa;
}

void test_flatten_regular_SumB_bound_10() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = nwa->flatten_regular(SumB, weight_t(10));
    verifyAutomatonBasics(flat, "flattened with bound 10");
    printAutomatonStats(flat, "flatten_regular(SumB, 10)");

    delete flat;
    delete nwa;
}

void test_flatten_regular_SumB_varying_bounds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    unsigned int prev_states = 0;
    for (int bound = 1; bound <= 5; ++bound) {
        Automaton* flat = nwa->flatten_regular(SumB, weight_t(bound));
        verifyAutomatonBasics(flat, "flattened");

        unsigned int curr_states = flat->getStates()->size();
        std::cout << "    bound=" << bound << ": " << curr_states << " states" << std::endl;

        // State count should generally increase or stay same with larger bounds
        if (bound > 1) {
            TEST_ASSERT_GE(curr_states, prev_states,
                "State count should not decrease with larger bound");
        }
        prev_states = curr_states;

        delete flat;
    }

    delete nwa;
}

void test_flatten_regular_SumB_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(SumB, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(SumB, 3) on test_empt_2");

    delete flat;
    delete nwa;
}

void test_flatten_regular_SumB_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(SumB, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(SumB, 3) on test_empt_3");

    delete flat;
    delete nwa;
}

// ============================================================================
// Max_f Tests
// ============================================================================

void test_flatten_regular_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::COMPUTE_RETURN_MAX);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(Max_f);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(Max_f)");

    delete flat;
    delete nwa;
}

void test_flatten_regular_Max_test_empt_1() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(Max_f);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(Max_f) on test_empt_1");

    delete flat;
    delete nwa;
}

void test_flatten_regular_Max_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(Max_f);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(Max_f) on test_empt_2");

    delete flat;
    delete nwa;
}

void test_flatten_regular_Max_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(Max_f);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(Max_f) on nested_Sij");

    delete flat;
    delete nwa;
}

// ============================================================================
// Min_f Tests
// ============================================================================

void test_flatten_regular_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::COMPUTE_RETURN_MIN);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(Min_f);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(Min_f)");

    delete flat;
    delete nwa;
}

void test_flatten_regular_Min_test_empt_1() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(Min_f);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(Min_f) on test_empt_1");

    delete flat;
    delete nwa;
}

void test_flatten_regular_Min_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(Min_f);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(Min_f) on test_empt_2");

    delete flat;
    delete nwa;
}

void test_flatten_regular_Min_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(Min_f);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(Min_f) on nested_Sij");

    delete flat;
    delete nwa;
}

// ============================================================================
// Comparison Tests
// ============================================================================

void test_flatten_regular_Max_vs_Min_same_input() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    Automaton* flat_max = nwa->flatten_regular(Max_f);
    Automaton* flat_min = nwa->flatten_regular(Min_f);

    verifyAutomatonBasics(flat_max, "Max flattened");
    verifyAutomatonBasics(flat_min, "Min flattened");

    printAutomatonStats(flat_max, "Max_f");
    printAutomatonStats(flat_min, "Min_f");

    // Both should produce valid automata with same alphabet
    TEST_ASSERT_EQ(flat_max->getAlphabet()->size(), flat_min->getAlphabet()->size(),
        "Alphabet sizes should match");

    delete flat_max;
    delete flat_min;
    delete nwa;
}

void test_flatten_regular_consistency_multiple_calls() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Call flatten multiple times - should produce consistent results
    Automaton* flat1 = nwa->flatten_regular(SumB, weight_t(3));
    unsigned int states1 = flat1->getStates()->size();
    unsigned int trans1 = flat1->getNbTransitions();
    delete flat1;

    Automaton* flat2 = nwa->flatten_regular(SumB, weight_t(3));
    unsigned int states2 = flat2->getStates()->size();
    unsigned int trans2 = flat2->getNbTransitions();
    delete flat2;

    TEST_ASSERT_EQ(states1, states2, "State counts should be consistent");
    TEST_ASSERT_EQ(trans1, trans2, "Transition counts should be consistent");

    delete nwa;
}

// ============================================================================
// Edge Cases
// ============================================================================

void test_flatten_regular_nested_Sij2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ2);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = nwa->flatten_regular(SumB, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(SumB, 5) on nested_Sij2");

    delete flat;
    delete nwa;
}

void test_flatten_regular_complex_sumB() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::COMPLEX_SUMB);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    Automaton* flat = nwa->flatten_regular(SumB, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(SumB, 3) on complex_sumB");

    delete flat;
    delete nwa;
}

void test_flatten_regular_nondet_sumB() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NONDET_SUMB);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;
    std::cout << "    Is deterministic: " << nwa->isDeterministic() << std::endl;

    Automaton* flat = nwa->flatten_regular(SumB, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_regular(SumB, 3) on nondet_sumB");

    delete flat;
    delete nwa;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "TEST: flatten_regular()" << std::endl;
    std::cout << "========================================" << std::endl;

    // SumB tests
    std::cout << "\n--- SumB Tests ---" << std::endl;
    RUN_TEST(test_flatten_regular_SumB_basic);
    RUN_TEST(test_flatten_regular_SumB_bound_0);
    RUN_TEST(test_flatten_regular_SumB_bound_1);
    RUN_TEST(test_flatten_regular_SumB_bound_10);
    RUN_TEST(test_flatten_regular_SumB_varying_bounds);
    RUN_TEST(test_flatten_regular_SumB_test_empt_2);
    RUN_TEST(test_flatten_regular_SumB_test_empt_3);

    // Max_f tests
    std::cout << "\n--- Max_f Tests ---" << std::endl;
    RUN_TEST(test_flatten_regular_Max_basic);
    RUN_TEST(test_flatten_regular_Max_test_empt_1);
    RUN_TEST(test_flatten_regular_Max_test_empt_2);
    RUN_TEST(test_flatten_regular_Max_nested_Sij);

    // Min_f tests
    std::cout << "\n--- Min_f Tests ---" << std::endl;
    RUN_TEST(test_flatten_regular_Min_basic);
    RUN_TEST(test_flatten_regular_Min_test_empt_1);
    RUN_TEST(test_flatten_regular_Min_test_empt_2);
    RUN_TEST(test_flatten_regular_Min_nested_Sij);

    // Comparison tests
    std::cout << "\n--- Comparison Tests ---" << std::endl;
    RUN_TEST(test_flatten_regular_Max_vs_Min_same_input);
    RUN_TEST(test_flatten_regular_consistency_multiple_calls);

    // Edge cases
    std::cout << "\n--- Edge Cases ---" << std::endl;
    RUN_TEST(test_flatten_regular_nested_Sij2);
    RUN_TEST(test_flatten_regular_complex_sumB);
    RUN_TEST(test_flatten_regular_nondet_sumB);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
