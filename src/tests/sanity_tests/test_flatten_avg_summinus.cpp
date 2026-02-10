/**
 * test_flatten_avg_summinus.cpp
 *
 * Tests for NestedAutomaton::flatten_Avg_SumMinus()
 * This is the specialized flattening for SumMinus + LimAvg combinations.
 * Requires synchronized input (single ultimate child).
 */

#include "test_common.h"

// Helper to compute c_bound (copied from NestedAutomaton.cpp logic)
static uint64_t compute_c_bound_for_test(const NestedAutomaton* nwa) {
    uint64_t max_child_states = 1;
    for (size_t i = 0; i < nwa->getChildrenSize(); ++i) {
        ChildAutomaton* child = nwa->getChild(i);
        if (child && child->getStates()) {
            max_child_states = std::max(max_child_states,
                static_cast<uint64_t>(child->getStates()->size()));
        }
    }
    uint64_t parent_states = nwa->getStates()->size();
    return max_child_states * parent_states;
}

// ============================================================================
// Basic Tests (with full pipeline: determinize -> synchronize -> flatten)
// ============================================================================

void test_flatten_Avg_SumMinus_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    // Synchronize children first
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have exactly 1 synchronized child");

    std::cout << "    After sync: " << sync_nwa->getNbStates() << " parent states, "
              << "child has " << sync_nwa->getChild(0)->getNbStates() << " states" << std::endl;

    // Compute c_bound and flatten
    uint64_t c_bound = compute_c_bound_for_test(nwa);
    std::cout << "    c_bound = " << c_bound << std::endl;

    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_Avg_SumMinus");

    delete flat;
    delete sync_nwa;
    delete nwa;
}

void test_flatten_Avg_SumMinus_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");

    uint64_t c_bound = compute_c_bound_for_test(nwa);
    std::cout << "    c_bound = " << c_bound << std::endl;

    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_Avg_SumMinus on test_empt_2");

    delete flat;
    delete sync_nwa;
    delete nwa;
}

void test_flatten_Avg_SumMinus_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");

    uint64_t c_bound = compute_c_bound_for_test(nwa);
    std::cout << "    c_bound = " << c_bound << std::endl;

    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_Avg_SumMinus on test_empt_3");

    delete flat;
    delete sync_nwa;
    delete nwa;
}

// ============================================================================
// c_bound Variation Tests
// ============================================================================

void test_flatten_Avg_SumMinus_cbound_1() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");

    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(1);
    verifyAutomatonBasics(flat, "flattened with c_bound=1");
    printAutomatonStats(flat, "flatten_Avg_SumMinus(c_bound=1)");

    delete flat;
    delete sync_nwa;
    delete nwa;
}

void test_flatten_Avg_SumMinus_cbound_5() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");

    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(5);
    verifyAutomatonBasics(flat, "flattened with c_bound=5");
    printAutomatonStats(flat, "flatten_Avg_SumMinus(c_bound=5)");

    delete flat;
    delete sync_nwa;
    delete nwa;
}

void test_flatten_Avg_SumMinus_cbound_10() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");

    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(10);
    verifyAutomatonBasics(flat, "flattened with c_bound=10");
    printAutomatonStats(flat, "flatten_Avg_SumMinus(c_bound=10)");

    delete flat;
    delete sync_nwa;
    delete nwa;
}

void test_flatten_Avg_SumMinus_varying_cbounds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    std::cout << std::endl;

    for (uint64_t c_bound = 1; c_bound <= 5; ++c_bound) {
        Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
        verifyAutomatonBasics(flat, "flattened");
        std::cout << "    c_bound=" << c_bound << ": "
                  << flat->getStates()->size() << " states, "
                  << flat->getNbTransitions() << " transitions" << std::endl;
        delete flat;
    }

    delete sync_nwa;
    delete nwa;
}

// ============================================================================
// Pipeline Tests (determinize -> sync -> flatten)
// ============================================================================

void test_flatten_Avg_SumMinus_with_determinization() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    // Full pipeline: determinize -> synchronize -> flatten
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet failed");
    std::cout << "    After determinization: " << det_nwa->getNbStates() << " parent states"
              << std::endl;

    NestedAutomaton* sync_nwa = det_nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have 1 synchronized child");

    uint64_t c_bound = compute_c_bound_for_test(nwa);
    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "full pipeline flatten");

    delete flat;
    delete sync_nwa;
    delete det_nwa;
    delete nwa;
}

void test_flatten_Avg_SumMinus_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");

    uint64_t c_bound = compute_c_bound_for_test(nwa);
    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "flatten_Avg_SumMinus on nested_Sij");

    delete flat;
    delete sync_nwa;
    delete nwa;
}

// ============================================================================
// Verification Tests
// ============================================================================

void test_flatten_Avg_SumMinus_output_properties() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();

    uint64_t c_bound = compute_c_bound_for_test(nwa);
    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    std::cout << std::endl;

    // Verify output properties
    TEST_ASSERT_NOT_NULL(flat->getInitial(), "Flattened automaton should have initial state");

    // Check alphabet matches input
    TEST_ASSERT_EQ(flat->getAlphabet()->size(), sync_nwa->getChild(0)->getAlphabet()->size(),
        "Alphabet size should match synchronized child alphabet");

    std::cout << "    Initial state: " << flat->getInitial()->getName() << std::endl;
    std::cout << "    Weight range: [" << flat->getMinDomain() << ", "
              << flat->getMaxDomain() << "]" << std::endl;
    std::cout << "    Is complete: " << flat->isComplete() << std::endl;

    delete flat;
    delete sync_nwa;
    delete nwa;
}

void test_flatten_Avg_SumMinus_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();

    uint64_t c_bound = 5;

    // Call twice and verify consistent results
    Automaton* flat1 = sync_nwa->flatten_Avg_SumMinus(c_bound);
    unsigned int states1 = flat1->getStates()->size();
    unsigned int trans1 = flat1->getNbTransitions();
    delete flat1;

    Automaton* flat2 = sync_nwa->flatten_Avg_SumMinus(c_bound);
    unsigned int states2 = flat2->getStates()->size();
    unsigned int trans2 = flat2->getNbTransitions();
    delete flat2;

    TEST_ASSERT_EQ(states1, states2, "State count should be consistent");
    TEST_ASSERT_EQ(trans1, trans2, "Transition count should be consistent");

    delete sync_nwa;
    delete nwa;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "TEST: flatten_Avg_SumMinus()" << std::endl;
    std::cout << "========================================" << std::endl;

    // Basic tests
    std::cout << "\n--- Basic Tests ---" << std::endl;
    RUN_TEST(test_flatten_Avg_SumMinus_basic);
    RUN_TEST(test_flatten_Avg_SumMinus_test_empt_2);
    RUN_TEST(test_flatten_Avg_SumMinus_test_empt_3);

    // c_bound variation tests
    std::cout << "\n--- c_bound Variation Tests ---" << std::endl;
    RUN_TEST(test_flatten_Avg_SumMinus_cbound_1);
    RUN_TEST(test_flatten_Avg_SumMinus_cbound_5);
    RUN_TEST(test_flatten_Avg_SumMinus_cbound_10);
    RUN_TEST(test_flatten_Avg_SumMinus_varying_cbounds);

    // Pipeline tests
    std::cout << "\n--- Pipeline Tests ---" << std::endl;
    RUN_TEST(test_flatten_Avg_SumMinus_with_determinization);
    RUN_TEST(test_flatten_Avg_SumMinus_nested_Sij);

    // Verification tests
    std::cout << "\n--- Verification Tests ---" << std::endl;
    RUN_TEST(test_flatten_Avg_SumMinus_output_properties);
    RUN_TEST(test_flatten_Avg_SumMinus_consistency);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
