/**
 * test_flatten_minmax_inf.cpp
 *
 * Tests for NestedAutomaton::flatten_MinMax_Inf() (private)
 * Handles: Max/Min + Inf/LimInf combinations
 * Access via NestedAutomatonTester friend class
 */

#include "test_common.h"

static bool eval_binary_threshold_backend(Automaton* flat, value_function_t infVal) {
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}

static bool eval_regular_threshold_backend(Automaton* flat, value_function_t infVal, weight_t threshold) {
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);
    delete non_silent;
    delete flat;
    return result;
}

// ============================================================================
// Max with Inf Tests
// ============================================================================

void test_flatten_MinMax_Inf_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Inf(Max_f, 5)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_Max_threshold_0() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(0));
    verifyAutomatonBasics(flat, "flattened with threshold 0");
    printAutomatonStats(flat, "flatten_MinMax_Inf(Max_f, 0)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_Max_threshold_1() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(1));
    verifyAutomatonBasics(flat, "flattened with threshold 1");
    printAutomatonStats(flat, "flatten_MinMax_Inf(Max_f, 1)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_Max_threshold_10() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(10));
    verifyAutomatonBasics(flat, "flattened with threshold 10");
    printAutomatonStats(flat, "flatten_MinMax_Inf(Max_f, 10)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_Max_varying_thresholds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    for (int threshold = 0; threshold <= 5; ++threshold) {
        Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(threshold));
        verifyAutomatonBasics(flat, "flattened");
        std::cout << "    threshold=" << threshold << ": "
                  << flat->getStates()->size() << " states, "
                  << flat->getNbTransitions() << " transitions" << std::endl;
        delete flat;
    }

    delete nwa;
}

void test_flatten_MinMax_Inf_Max_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Inf on test_empt_2");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_Max_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Inf on test_empt_3");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_Max_compute_return() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::COMPUTE_RETURN_MAX);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Inf on compute_return_max");

    delete flat;
    delete nwa;
}

// ============================================================================
// Min with Inf Tests
// ============================================================================

void test_flatten_MinMax_Inf_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Min_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Inf(Min_f, 5)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_Min_threshold_0() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Min_f, weight_t(0));
    verifyAutomatonBasics(flat, "flattened with threshold 0");
    printAutomatonStats(flat, "flatten_MinMax_Inf(Min_f, 0)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_Min_threshold_5() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Min_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened with threshold 5");
    printAutomatonStats(flat, "flatten_MinMax_Inf(Min_f, 5)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_Min_varying_thresholds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    for (int threshold = 0; threshold <= 5; ++threshold) {
        Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Min_f, weight_t(threshold));
        verifyAutomatonBasics(flat, "flattened");
        std::cout << "    threshold=" << threshold << ": "
                  << flat->getStates()->size() << " states, "
                  << flat->getNbTransitions() << " transitions" << std::endl;
        delete flat;
    }

    delete nwa;
}

void test_flatten_MinMax_Inf_Min_compute_return() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::COMPUTE_RETURN_MIN);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Min_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Inf on compute_return_min");

    delete flat;
    delete nwa;
}

// ============================================================================
// Comparison Tests
// ============================================================================

void test_flatten_MinMax_Inf_Max_vs_Min() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    Automaton* flat_max = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(3));
    Automaton* flat_min = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Min_f, weight_t(3));

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

void test_flatten_MinMax_Inf_vs_Sup() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    // Compare Inf vs Sup variants with same finite aggregator
    Automaton* flat_inf = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(3));
    Automaton* flat_sup = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(3));

    verifyAutomatonBasics(flat_inf, "Inf flattened");
    verifyAutomatonBasics(flat_sup, "Sup flattened");

    printAutomatonStats(flat_inf, "Max_f + Inf");
    printAutomatonStats(flat_sup, "Max_f + Sup");

    // Both should have same alphabet
    TEST_ASSERT_EQ(flat_inf->getAlphabet()->size(), flat_sup->getAlphabet()->size(),
        "Alphabet sizes should match");

    delete flat_inf;
    delete flat_sup;
    delete nwa;
}

void test_flatten_MinMax_Inf_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Call twice and verify consistent results
    Automaton* flat1 = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(3));
    unsigned int states1 = flat1->getStates()->size();
    unsigned int trans1 = flat1->getNbTransitions();
    delete flat1;

    Automaton* flat2 = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(3));
    unsigned int states2 = flat2->getStates()->size();
    unsigned int trans2 = flat2->getNbTransitions();
    delete flat2;

    TEST_ASSERT_EQ(states1, states2, "State counts should be consistent");
    TEST_ASSERT_EQ(trans1, trans2, "Transition counts should be consistent");

    delete nwa;
}

// ============================================================================
// Edge Cases and Other Files
// ============================================================================

void test_flatten_MinMax_Inf_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Inf on nested_Sij");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_nested_Sij2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ2);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Inf on nested_Sij2");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_output_properties() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(5));
    std::cout << std::endl;

    // Verify output properties
    TEST_ASSERT_NOT_NULL(flat->getInitial(), "Flattened automaton should have initial state");

    std::cout << "    Initial state: " << flat->getInitial()->getName() << std::endl;
    std::cout << "    Weight range: [" << flat->getMinDomain() << ", "
              << flat->getMaxDomain() << "]" << std::endl;
    std::cout << "    Is complete: " << flat->isComplete() << std::endl;

    // Raw flattened output may still contain SILENT edges before silence removal.
    bool is_01_or_silent = hasOnly01OrSilentWeights(flat);
    std::cout << "    Has only 0/1/SILENT weights: " << is_01_or_silent << std::endl;

    delete flat;
    delete nwa;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "TEST: flatten_MinMax_Inf()" << std::endl;
    std::cout << "========================================" << std::endl;

    // Max tests
    std::cout << "\n--- Max Tests ---" << std::endl;
    RUN_TEST(test_flatten_MinMax_Inf_Max_basic);
    RUN_TEST(test_flatten_MinMax_Inf_Max_threshold_0);
    RUN_TEST(test_flatten_MinMax_Inf_Max_threshold_1);
    RUN_TEST(test_flatten_MinMax_Inf_Max_threshold_10);
    RUN_TEST(test_flatten_MinMax_Inf_Max_varying_thresholds);
    RUN_TEST(test_flatten_MinMax_Inf_Max_test_empt_2);
    RUN_TEST(test_flatten_MinMax_Inf_Max_test_empt_3);
    RUN_TEST(test_flatten_MinMax_Inf_Max_compute_return);

    // Min tests
    std::cout << "\n--- Min Tests ---" << std::endl;
    RUN_TEST(test_flatten_MinMax_Inf_Min_basic);
    RUN_TEST(test_flatten_MinMax_Inf_Min_threshold_0);
    RUN_TEST(test_flatten_MinMax_Inf_Min_threshold_5);
    RUN_TEST(test_flatten_MinMax_Inf_Min_varying_thresholds);
    RUN_TEST(test_flatten_MinMax_Inf_Min_compute_return);

    // Comparison tests
    std::cout << "\n--- Comparison Tests ---" << std::endl;
    RUN_TEST(test_flatten_MinMax_Inf_Max_vs_Min);
    RUN_TEST(test_flatten_MinMax_Inf_vs_Sup);
    RUN_TEST(test_flatten_MinMax_Inf_consistency);

    // Edge cases
    std::cout << "\n--- Edge Cases ---" << std::endl;
    RUN_TEST(test_flatten_MinMax_Inf_nested_Sij);
    RUN_TEST(test_flatten_MinMax_Inf_nested_Sij2);
    RUN_TEST(test_flatten_MinMax_Inf_output_properties);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
