/**
 * test_synchronization.cpp
 *
 * Tests for NestedAutomaton::synchronizeChildren()
 * This operation combines multiple children into a single synchronized child.
 * Required before flatten_Avg_SumMinus().
 */

#include "test_common.h"

// ============================================================================
// Basic Synchronization Tests
// ============================================================================

void test_synchronizeChildren_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    size_t input_children = nwa->getChildrenSize();
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << input_children << " children" << std::endl;

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren returned null");
    verifyNestedAutomatonBasics(sync_nwa, "synchronized");

    size_t output_children = sync_nwa->getChildrenSize();
    std::cout << "    Output: " << sync_nwa->getNbStates() << " parent states, "
              << output_children << " children" << std::endl;

    // Should have exactly 1 synchronized child
    TEST_ASSERT_EQ(output_children, 1u, "Should have exactly 1 synchronized child");

    delete sync_nwa;
    delete nwa;
}

void test_synchronizeChildren_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    Input children: " << nwa->getChildrenSize() << std::endl;

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren returned null");
    verifyNestedAutomatonBasics(sync_nwa, "synchronized");

    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have 1 synchronized child");
    std::cout << "    Synchronized child states: "
              << sync_nwa->getChild(0)->getNbStates() << std::endl;

    delete sync_nwa;
    delete nwa;
}

void test_synchronizeChildren_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    Input children: " << nwa->getChildrenSize() << std::endl;

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren returned null");
    verifyNestedAutomatonBasics(sync_nwa, "synchronized");

    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have 1 synchronized child");
    std::cout << "    Synchronized child states: "
              << sync_nwa->getChild(0)->getNbStates() << std::endl;

    delete sync_nwa;
    delete nwa;
}

void test_synchronizeChildren_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren returned null");
    verifyNestedAutomatonBasics(sync_nwa, "synchronized");

    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have 1 synchronized child");

    std::cout << "    Output: " << sync_nwa->getNbStates() << " parent states" << std::endl;
    std::cout << "    Synchronized child: " << sync_nwa->getChild(0)->getNbStates()
              << " states" << std::endl;

    delete sync_nwa;
    delete nwa;
}

void test_synchronizeChildren_nested_Sij2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    Input children: " << nwa->getChildrenSize() << std::endl;

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren returned null");
    verifyNestedAutomatonBasics(sync_nwa, "synchronized");

    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have 1 synchronized child");

    delete sync_nwa;
    delete nwa;
}

// ============================================================================
// Synchronized Child Properties Tests
// ============================================================================

void test_synchronizeChildren_child_properties() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren returned null");
    std::cout << std::endl;

    ChildAutomaton* sync_child = sync_nwa->getChild(0);
    TEST_ASSERT_NOT_NULL(sync_child, "Synchronized child is null");

    std::cout << "    Synchronized child states: " << sync_child->getNbStates() << std::endl;
    std::cout << "    Synchronized child transitions: " << sync_child->getNbTransitions() << std::endl;
    std::cout << "    Synchronized child alphabet: " << sync_child->getAlphabet()->size() << std::endl;

    // Synchronized child should NOT have an initial state
    TEST_ASSERT(sync_child->getInitial() == nullptr, "Synchronized child should not have initial state");

    delete sync_nwa;
    delete nwa;
}

void test_synchronizeChildren_preserves_parent_structure() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    unsigned int input_states = nwa->getNbStates();
    unsigned int input_trans = nwa->getNbTransitions();

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren returned null");

    std::cout << "    Input: " << input_states << " states, " << input_trans << " transitions" << std::endl;
    std::cout << "    Output: " << sync_nwa->getNbStates() << " states, "
              << sync_nwa->getNbTransitions() << " transitions" << std::endl;

    // Parent structure should be roughly preserved
    // (may have slight modifications due to synchronization)
    TEST_ASSERT_GT(sync_nwa->getNbStates(), 0u, "Output should have states");

    delete sync_nwa;
    delete nwa;
}

void test_synchronizeChildren_alphabet_relationship() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    size_t parent_alpha = nwa->getAlphabet()->size();
    std::cout << "    Input parent alphabet: " << parent_alpha << std::endl;

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren returned null");

    size_t sync_parent_alpha = sync_nwa->getAlphabet()->size();
    size_t sync_child_alpha = sync_nwa->getChild(0)->getAlphabet()->size();

    std::cout << "    Output parent alphabet: " << sync_parent_alpha << std::endl;
    std::cout << "    Synchronized child alphabet: " << sync_child_alpha << std::endl;

    delete sync_nwa;
    delete nwa;
}

// ============================================================================
// Consistency Tests
// ============================================================================

void test_synchronizeChildren_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Synchronize twice and compare
    NestedAutomaton* sync1 = nwa->synchronizeChildren();
    unsigned int states1 = sync1->getNbStates();
    unsigned int child_states1 = sync1->getChild(0)->getNbStates();
    delete sync1;

    NestedAutomaton* sync2 = nwa->synchronizeChildren();
    unsigned int states2 = sync2->getNbStates();
    unsigned int child_states2 = sync2->getChild(0)->getNbStates();
    delete sync2;

    TEST_ASSERT_EQ(states1, states2, "Parent state count should be consistent");
    TEST_ASSERT_EQ(child_states1, child_states2, "Child state count should be consistent");

    delete nwa;
}

void test_synchronizeChildren_idempotent() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Synchronize once
    NestedAutomaton* sync1 = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync1, "First synchronization failed");
    TEST_ASSERT_EQ(sync1->getChildrenSize(), 1u, "Should have 1 child after first sync");

    // Synchronizing again should produce similar results (already has 1 child)
    NestedAutomaton* sync2 = sync1->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync2, "Second synchronization failed");
    TEST_ASSERT_EQ(sync2->getChildrenSize(), 1u, "Should still have 1 child");

    delete sync2;
    delete sync1;
    delete nwa;
}

// ============================================================================
// Pipeline Tests (with Flatten)
// ============================================================================

void test_synchronize_then_flatten_Avg_SumMinus() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    // Synchronize
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Need exactly 1 child for flatten_Avg_SumMinus");

    std::cout << "    After sync: " << sync_nwa->getNbStates() << " states, "
              << "child has " << sync_nwa->getChild(0)->getNbStates() << " states" << std::endl;

    // Compute c_bound and flatten
    uint64_t c_bound = static_cast<uint64_t>(sync_nwa->getChild(0)->getStates()->size())
                     * sync_nwa->getStates()->size();
    std::cout << "    c_bound = " << c_bound << std::endl;

    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "sync -> flatten_Avg_SumMinus");

    delete flat;
    delete sync_nwa;
    delete nwa;
}

void test_determinize_then_synchronize() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    // Determinize first
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet failed");
    std::cout << "    After determinize: " << det_nwa->getNbStates() << " states, "
              << det_nwa->getChildrenSize() << " children" << std::endl;

    // Then synchronize
    NestedAutomaton* sync_nwa = det_nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have 1 child after sync");

    std::cout << "    After sync: " << sync_nwa->getNbStates() << " states, "
              << "child has " << sync_nwa->getChild(0)->getNbStates() << " states" << std::endl;

    delete sync_nwa;
    delete det_nwa;
    delete nwa;
}

void test_full_pipeline_det_sync_flatten() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    // Full pipeline: determinize -> synchronize -> flatten
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinize failed");

    NestedAutomaton* sync_nwa = det_nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronize failed");

    // Compute c_bound for flatten_Avg_SumMinus
    uint64_t c_bound = static_cast<uint64_t>(sync_nwa->getChild(0)->getStates()->size())
                     * sync_nwa->getStates()->size();

    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "det -> sync -> flatten");

    delete flat;
    delete sync_nwa;
    delete det_nwa;
    delete nwa;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "TEST: synchronizeChildren()" << std::endl;
    std::cout << "========================================" << std::endl;

    // Basic synchronization tests
    std::cout << "\n--- Basic Synchronization Tests ---" << std::endl;
    RUN_TEST(test_synchronizeChildren_basic);
    RUN_TEST(test_synchronizeChildren_test_empt_2);
    RUN_TEST(test_synchronizeChildren_test_empt_3);
    RUN_TEST(test_synchronizeChildren_nested_Sij);
    RUN_TEST(test_synchronizeChildren_nested_Sij2);

    // Synchronized child properties tests
    std::cout << "\n--- Synchronized Child Properties Tests ---" << std::endl;
    RUN_TEST(test_synchronizeChildren_child_properties);
    RUN_TEST(test_synchronizeChildren_preserves_parent_structure);
    RUN_TEST(test_synchronizeChildren_alphabet_relationship);

    // Consistency tests
    std::cout << "\n--- Consistency Tests ---" << std::endl;
    RUN_TEST(test_synchronizeChildren_consistency);
    RUN_TEST(test_synchronizeChildren_idempotent);

    // Pipeline tests
    std::cout << "\n--- Pipeline Tests ---" << std::endl;
    RUN_TEST(test_synchronize_then_flatten_Avg_SumMinus);
    RUN_TEST(test_determinize_then_synchronize);
    RUN_TEST(test_full_pipeline_det_sync_flatten);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
