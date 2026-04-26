/**
 * test_flatten_sumplusminus_inf.cpp
 *
 * Tests for NestedAutomaton::flatten_SumPlusMinus_Inf() (private)
 * Handles: SumPlus/SumMinus + Inf/LimInf combinations
 * Access via NestedAutomatonTester friend class
 */

#include "test_common.h"

// ============================================================================
// SumPlus with Inf Tests
// ============================================================================

void test_flatten_SumPlusMinus_Inf_SumPlus_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf(SumPlus, 5)");

    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumPlus_threshold_0() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(0));
    verifyAutomatonBasics(flat, "flattened with threshold 0");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf(SumPlus, 0)");

    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumPlus_threshold_1() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(1));
    verifyAutomatonBasics(flat, "flattened with threshold 1");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf(SumPlus, 1)");

    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumPlus_threshold_10() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(10));
    verifyAutomatonBasics(flat, "flattened with threshold 10");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf(SumPlus, 10)");

    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumPlus_varying_thresholds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    for (int threshold = 0; threshold <= 5; ++threshold) {
        Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(threshold));
        verifyAutomatonBasics(flat, "flattened");
        std::cout << "    threshold=" << threshold << ": "
                  << flat->getStates()->size() << " states, "
                  << flat->getNbTransitions() << " transitions" << std::endl;
        delete flat;
    }

    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumPlus_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf on test_empt_2");

    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumPlus_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf on test_empt_3");

    delete flat;
    delete nwa;
}

// ============================================================================
// SumMinus with Inf Tests
// ============================================================================

void test_flatten_SumPlusMinus_Inf_SumMinus_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumMinus, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf(SumMinus, 5)");

    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumMinus_threshold_0() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumMinus, weight_t(0));
    verifyAutomatonBasics(flat, "flattened with threshold 0");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf(SumMinus, 0)");

    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumMinus_threshold_5() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumMinus, weight_t(5));
    verifyAutomatonBasics(flat, "flattened with threshold 5");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf(SumMinus, 5)");

    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumMinus_varying_thresholds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    for (int threshold = 0; threshold <= 5; ++threshold) {
        Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumMinus, weight_t(threshold));
        verifyAutomatonBasics(flat, "flattened");
        std::cout << "    threshold=" << threshold << ": "
                  << flat->getStates()->size() << " states, "
                  << flat->getNbTransitions() << " transitions" << std::endl;
        delete flat;
    }

    delete nwa;
}

// ============================================================================
// Comparison Tests
// ============================================================================

void test_flatten_SumPlusMinus_Inf_SumPlus_vs_SumMinus() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    Automaton* flat_plus = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(3));
    Automaton* flat_minus = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumMinus, weight_t(3));

    verifyAutomatonBasics(flat_plus, "SumPlus flattened");
    verifyAutomatonBasics(flat_minus, "SumMinus flattened");

    printAutomatonStats(flat_plus, "SumPlus");
    printAutomatonStats(flat_minus, "SumMinus");

    // Both should produce valid automata with same alphabet
    TEST_ASSERT_EQ(flat_plus->getAlphabet()->size(), flat_minus->getAlphabet()->size(),
        "Alphabet sizes should match");

    delete flat_plus;
    delete flat_minus;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_vs_Sup() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    // Compare Inf vs Sup variants with same finite aggregator
    Automaton* flat_inf = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(3));
    Automaton* flat_sup = NestedAutomatonTester::flatten_SumPlusMinus_Sup(nwa, SumPlus, weight_t(3));

    verifyAutomatonBasics(flat_inf, "Inf flattened");
    verifyAutomatonBasics(flat_sup, "Sup flattened");

    printAutomatonStats(flat_inf, "SumPlus + Inf");
    printAutomatonStats(flat_sup, "SumPlus + Sup");

    // Both should have same alphabet
    TEST_ASSERT_EQ(flat_inf->getAlphabet()->size(), flat_sup->getAlphabet()->size(),
        "Alphabet sizes should match");

    delete flat_inf;
    delete flat_sup;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Call twice and verify consistent results
    Automaton* flat1 = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(3));
    unsigned int states1 = flat1->getStates()->size();
    unsigned int trans1 = flat1->getNbTransitions();
    delete flat1;

    Automaton* flat2 = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(3));
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

void test_flatten_SumPlusMinus_Inf_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf on nested_Sij");

    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_nested_Sij2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ2);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_SumPlusMinus_Inf on nested_Sij2");

    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_output_properties() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(5));
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
    std::cout << "TEST: flatten_SumPlusMinus_Inf()" << std::endl;
    std::cout << "========================================" << std::endl;

    // SumPlus tests
    std::cout << "\n--- SumPlus Tests ---" << std::endl;
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumPlus_basic);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumPlus_threshold_0);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumPlus_threshold_1);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumPlus_threshold_10);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumPlus_varying_thresholds);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumPlus_test_empt_2);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumPlus_test_empt_3);

    // SumMinus tests
    std::cout << "\n--- SumMinus Tests ---" << std::endl;
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumMinus_basic);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumMinus_threshold_0);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumMinus_threshold_5);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumMinus_varying_thresholds);

    // Comparison tests
    std::cout << "\n--- Comparison Tests ---" << std::endl;
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumPlus_vs_SumMinus);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_vs_Sup);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_consistency);

    // Edge cases
    std::cout << "\n--- Edge Cases ---" << std::endl;
    RUN_TEST(test_flatten_SumPlusMinus_Inf_nested_Sij);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_nested_Sij2);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_output_properties);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
