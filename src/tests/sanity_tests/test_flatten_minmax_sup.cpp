/**
 * test_flatten_minmax_sup.cpp
 *
 * Tests for NestedAutomaton::flatten_MinMax_Sup() (private)
 * Handles: Max/Min + Sup/LimSup combinations
 * Access via NestedAutomatonTester friend class
 */

#include "test_common.h"

// ============================================================================
// Max with Sup Tests
// ============================================================================

void test_flatten_MinMax_Sup_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Sup(Max_f, 5)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Max_threshold_0() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(0));
    verifyAutomatonBasics(flat, "flattened with threshold 0");
    printAutomatonStats(flat, "flatten_MinMax_Sup(Max_f, 0)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Max_threshold_1() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(1));
    verifyAutomatonBasics(flat, "flattened with threshold 1");
    printAutomatonStats(flat, "flatten_MinMax_Sup(Max_f, 1)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Max_threshold_10() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(10));
    verifyAutomatonBasics(flat, "flattened with threshold 10");
    printAutomatonStats(flat, "flatten_MinMax_Sup(Max_f, 10)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Max_varying_thresholds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    for (int threshold = 0; threshold <= 5; ++threshold) {
        Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(threshold));
        verifyAutomatonBasics(flat, "flattened");
        std::cout << "    threshold=" << threshold << ": "
                  << flat->getStates()->size() << " states, "
                  << flat->getNbTransitions() << " transitions" << std::endl;
        delete flat;
    }

    delete nwa;
}

void test_flatten_MinMax_Sup_Max_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Sup on test_empt_2");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Max_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Sup on test_empt_3");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Max_compute_return() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::COMPUTE_RETURN_MAX);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Sup on compute_return_max");

    delete flat;
    delete nwa;
}

// ============================================================================
// Min with Sup Tests
// ============================================================================

void test_flatten_MinMax_Sup_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Min_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Sup(Min_f, 5)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Min_threshold_0() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Min_f, weight_t(0));
    verifyAutomatonBasics(flat, "flattened with threshold 0");
    printAutomatonStats(flat, "flatten_MinMax_Sup(Min_f, 0)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Min_threshold_5() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Min_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened with threshold 5");
    printAutomatonStats(flat, "flatten_MinMax_Sup(Min_f, 5)");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Min_varying_thresholds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    for (int threshold = 0; threshold <= 5; ++threshold) {
        Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Min_f, weight_t(threshold));
        verifyAutomatonBasics(flat, "flattened");
        std::cout << "    threshold=" << threshold << ": "
                  << flat->getStates()->size() << " states, "
                  << flat->getNbTransitions() << " transitions" << std::endl;
        delete flat;
    }

    delete nwa;
}

void test_flatten_MinMax_Sup_Min_compute_return() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::COMPUTE_RETURN_MIN);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Min_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Sup on compute_return_min");

    delete flat;
    delete nwa;
}

// ============================================================================
// Comparison Tests
// ============================================================================

void test_flatten_MinMax_Sup_Max_vs_Min() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    Automaton* flat_max = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(3));
    Automaton* flat_min = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Min_f, weight_t(3));

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

void test_flatten_MinMax_Sup_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Call twice and verify consistent results
    Automaton* flat1 = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(3));
    unsigned int states1 = flat1->getStates()->size();
    unsigned int trans1 = flat1->getNbTransitions();
    delete flat1;

    Automaton* flat2 = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(3));
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

void test_flatten_MinMax_Sup_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(3));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Sup on nested_Sij");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_nested_Sij2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ2);
    verifyNestedAutomatonBasics(nwa, "input");

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_MinMax_Sup on nested_Sij2");

    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_output_properties() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(5));
    std::cout << std::endl;

    // Verify output properties
    TEST_ASSERT_NOT_NULL(flat->getInitial(), "Flattened automaton should have initial state");

    std::cout << "    Initial state: " << flat->getInitial()->getName() << std::endl;
    std::cout << "    Weight range: [" << flat->getMinDomain() << ", "
              << flat->getMaxDomain() << "]" << std::endl;
    std::cout << "    Is complete: " << flat->isComplete() << std::endl;

    // Output should have 0/1 weights (Buchi-style)
    bool is_01 = hasOnly01Weights(flat);
    std::cout << "    Has only 0/1 weights: " << is_01 << std::endl;

    delete flat;
    delete nwa;
}

void test_isNonEmpty_Sup_Max_cycle_true() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::SUP_MAX_CYCLE_TRUE);
    verifyNestedAutomatonBasics(nwa, "input");

    TEST_ASSERT(nwa->isNonEmpty(Sup, Max_f, weight_t(1)),
        "Sup/Max should reach threshold 1 on repeated s-a-t-u cycles");
    TEST_ASSERT(!nwa->isNonEmpty(Sup, Max_f, weight_t(2)),
        "Sup/Max should not reach threshold 2 in tc11");

    delete nwa;
}

void test_isNonEmpty_LimSup_Max_cycle_true() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::SUP_MAX_CYCLE_TRUE);
    verifyNestedAutomatonBasics(nwa, "input");

    TEST_ASSERT(nwa->isNonEmpty(LimSup, Max_f, weight_t(1)),
        "LimSup/Max should see infinitely many threshold-1 child returns in tc11");
    TEST_ASSERT(!nwa->isNonEmpty(LimSup, Max_f, weight_t(2)),
        "LimSup/Max should not reach threshold 2 in tc11");

    delete nwa;
}

static State* findStateByName(const Automaton* automaton, const std::string& name) {
    for (unsigned int i = 0; i < automaton->getStates()->size(); ++i) {
        State* state = automaton->getStates()->at(i);
        if (state->getName() == name) {
            return state;
        }
    }
    return nullptr;
}

void test_flatten_MinMax_Sup_doomed_state_redirects_to_sink() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::SUP_MAX_DOOMED_FALSE);

    // Structural: verify that the doomed-state pruning path ran.
    // @sink@ must exist in the flattened automaton and at least one state
    // must redirect to it, confirming tracked-rej states are correctly pruned.
    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(1));
    verifyAutomatonBasics(flat, "flattened doomed-overlap case");

    State* sink = findStateByName(flat, "@sink@");
    TEST_ASSERT_NOT_NULL(sink, "Expected flattened automaton to contain @sink@ state");

    bool found_redirect = false;
    for (size_t sid = 0; sid < flat->getStates()->size() && !found_redirect; ++sid) {
        State* state = flat->getStates()->at(sid);
        if (state == sink) continue;
        for (size_t a = 0; a < flat->getAlphabet()->size() && !found_redirect; ++a) {
            SetStd<Edge*>* succs = state->getSuccessors(a);
            if (!succs) continue;
            for (Edge* e : *succs) {
                if (e->getTo() == sink) { found_redirect = true; break; }
            }
        }
    }
    TEST_ASSERT(found_redirect, "At least one state should redirect to @sink@ in the doomed-overlap flattening");

    delete flat;
    delete nwa;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "TEST: flatten_MinMax_Sup()" << std::endl;
    std::cout << "========================================" << std::endl;

    // Max tests
    std::cout << "\n--- Max Tests ---" << std::endl;
    RUN_TEST(test_flatten_MinMax_Sup_Max_basic);
    RUN_TEST(test_flatten_MinMax_Sup_Max_threshold_0);
    RUN_TEST(test_flatten_MinMax_Sup_Max_threshold_1);
    RUN_TEST(test_flatten_MinMax_Sup_Max_threshold_10);
    RUN_TEST(test_flatten_MinMax_Sup_Max_varying_thresholds);
    RUN_TEST(test_flatten_MinMax_Sup_Max_test_empt_2);
    RUN_TEST(test_flatten_MinMax_Sup_Max_test_empt_3);
    RUN_TEST(test_flatten_MinMax_Sup_Max_compute_return);

    // Min tests
    std::cout << "\n--- Min Tests ---" << std::endl;
    RUN_TEST(test_flatten_MinMax_Sup_Min_basic);
    RUN_TEST(test_flatten_MinMax_Sup_Min_threshold_0);
    RUN_TEST(test_flatten_MinMax_Sup_Min_threshold_5);
    RUN_TEST(test_flatten_MinMax_Sup_Min_varying_thresholds);
    RUN_TEST(test_flatten_MinMax_Sup_Min_compute_return);

    // Comparison tests
    std::cout << "\n--- Comparison Tests ---" << std::endl;
    RUN_TEST(test_flatten_MinMax_Sup_Max_vs_Min);
    RUN_TEST(test_flatten_MinMax_Sup_consistency);

    // Edge cases
    std::cout << "\n--- Edge Cases ---" << std::endl;
    RUN_TEST(test_flatten_MinMax_Sup_nested_Sij);
    RUN_TEST(test_flatten_MinMax_Sup_nested_Sij2);
    RUN_TEST(test_flatten_MinMax_Sup_output_properties);
    RUN_TEST(test_isNonEmpty_Sup_Max_cycle_true);
    RUN_TEST(test_isNonEmpty_LimSup_Max_cycle_true);
    RUN_TEST(test_flatten_MinMax_Sup_doomed_state_redirects_to_sink);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
