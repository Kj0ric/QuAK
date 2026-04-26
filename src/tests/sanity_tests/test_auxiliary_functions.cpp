/**
 * test_auxiliary_functions.cpp
 *
 * Tests for auxiliary NestedAutomaton functions:
 * - isDeterministicNested()
 * - isCompleteNested()
 * - isDeterministicAndCompleteNested()
 * - makeCompleteNested()
 * - removeSilentTransitions()
 * - getChild() / getChildrenSize()
 * - parser handling for final: all
 */

#include "test_common.h"

// ============================================================================
// isDeterministicNested Tests
// ============================================================================

void test_isDeterministicNested_test_empt_1() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    bool is_det = nwa->isDeterministicNested();
    std::cout << "    isDeterministicNested: " << is_det << std::endl;
    std::cout << "    isDeterministic (parent only): " << nwa->isDeterministic() << std::endl;

    delete nwa;
}

void test_isDeterministicNested_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    bool is_det = nwa->isDeterministicNested();
    std::cout << "    isDeterministicNested: " << is_det << std::endl;

    delete nwa;
}

void test_isDeterministicNested_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    bool is_det = nwa->isDeterministicNested();
    std::cout << "    isDeterministicNested: " << is_det << std::endl;

    delete nwa;
}

void test_isDeterministicNested_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    bool is_det = nwa->isDeterministicNested();
    std::cout << "    isDeterministicNested: " << is_det << std::endl;

    delete nwa;
}

void test_isDeterministicNested_after_determinization() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    std::cout << "    Before determinization: " << nwa->isDeterministicNested() << std::endl;

    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet failed");

    std::cout << "    After determinization: " << det_nwa->isDeterministicNested() << std::endl;

    delete det_nwa;
    delete nwa;
}

// ============================================================================
// isCompleteNested Tests
// ============================================================================

void test_isCompleteNested_test_empt_1() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::vector<bool> complete_flags;
    bool is_complete = nwa->isCompleteNested(&complete_flags);

    std::cout << "    isCompleteNested: " << is_complete << std::endl;
    std::cout << "    isComplete (parent only): " << nwa->isComplete() << std::endl;

    if (!complete_flags.empty()) {
        std::cout << "    Child completeness flags: ";
        for (size_t i = 0; i < complete_flags.size(); ++i) {
            std::cout << complete_flags[i] << " ";
        }
        std::cout << std::endl;
    }

    delete nwa;
}

void test_isCompleteNested_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    bool is_complete = nwa->isCompleteNested();
    std::cout << "    isCompleteNested: " << is_complete << std::endl;

    delete nwa;
}

void test_isCompleteNested_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    bool is_complete = nwa->isCompleteNested();
    std::cout << "    isCompleteNested: " << is_complete << std::endl;

    delete nwa;
}

// ============================================================================
// isDeterministicAndCompleteNested Tests
// ============================================================================

void test_isDeterministicAndCompleteNested_test_empt_1() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    bool is_det_comp = nwa->isDeterministicAndCompleteNested();
    std::cout << "    isDeterministicAndCompleteNested: " << is_det_comp << std::endl;

    delete nwa;
}

void test_isDeterministicAndCompleteNested_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    bool is_det_comp = nwa->isDeterministicAndCompleteNested();
    std::cout << "    isDeterministicAndCompleteNested: " << is_det_comp << std::endl;

    delete nwa;
}

// ============================================================================
// makeCompleteNested Tests
// ============================================================================

void test_makeCompleteNested_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    Before: isCompleteNested = " << nwa->isCompleteNested() << std::endl;
    std::cout << "    Before: " << nwa->getNbStates() << " states" << std::endl;

    NestedAutomaton* complete_nwa = nwa->makeCompleteNested();
    TEST_ASSERT_NOT_NULL(complete_nwa, "makeCompleteNested returned null");
    verifyNestedAutomatonBasics(complete_nwa, "completed");

    std::cout << "    After: isCompleteNested = " << complete_nwa->isCompleteNested() << std::endl;
    std::cout << "    After: " << complete_nwa->getNbStates() << " states" << std::endl;

    delete complete_nwa;
    delete nwa;
}

void test_makeCompleteNested_with_flags() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    std::vector<bool> complete_flags;
    nwa->isCompleteNested(&complete_flags);
    std::cout << "    Initial complete flags: ";
    for (bool f : complete_flags) std::cout << f << " ";
    std::cout << std::endl;

    NestedAutomaton* complete_nwa = nwa->makeCompleteNested(&complete_flags);
    TEST_ASSERT_NOT_NULL(complete_nwa, "makeCompleteNested returned null");

    std::cout << "    After makeCompleteNested: isCompleteNested = "
              << complete_nwa->isCompleteNested() << std::endl;

    delete complete_nwa;
    delete nwa;
}

void test_makeCompleteNested_with_custom_weights() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    // Make complete with custom sink weights
    NestedAutomaton* complete_nwa = nwa->makeCompleteNested(nullptr, weight_t(-1), weight_t(-1));
    TEST_ASSERT_NOT_NULL(complete_nwa, "makeCompleteNested returned null");
    verifyNestedAutomatonBasics(complete_nwa, "completed with custom weights");

    std::cout << "    Completed with parent_sink_w=-1, child_sink_w=-1" << std::endl;
    std::cout << "    States: " << complete_nwa->getNbStates() << std::endl;

    delete complete_nwa;
    delete nwa;
}

void test_makeCompleteNested_idempotent() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Make complete once
    NestedAutomaton* complete1 = nwa->makeCompleteNested();
    TEST_ASSERT_NOT_NULL(complete1, "First makeCompleteNested failed");

    unsigned int states1 = complete1->getNbStates();

    // Make complete again
    NestedAutomaton* complete2 = complete1->makeCompleteNested();
    TEST_ASSERT_NOT_NULL(complete2, "Second makeCompleteNested failed");

    unsigned int states2 = complete2->getNbStates();

    // Should be idempotent (no additional states added)
    TEST_ASSERT_EQ(states1, states2, "makeCompleteNested should be idempotent");

    delete complete2;
    delete complete1;
    delete nwa;
}

// ============================================================================
// removeSilentTransitions Tests
// ============================================================================

void test_removeSilentTransitions_LimInf() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIL1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    Input: " << nwa->getNbStates() << " states, "
              << nwa->getNbTransitions() << " transitions" << std::endl;

    NestedAutomaton* no_silent = NestedAutomaton::removeSilentTransitions(nwa, LimInf);
    TEST_ASSERT_NOT_NULL(no_silent, "removeSilentTransitions returned null");
    verifyNestedAutomatonBasics(no_silent, "without silent");

    std::cout << "    Output: " << no_silent->getNbStates() << " states, "
              << no_silent->getNbTransitions() << " transitions" << std::endl;

    delete no_silent;
    delete nwa;
}

void test_removeSilentTransitions_LimSup() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIL1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    NestedAutomaton* no_silent = NestedAutomaton::removeSilentTransitions(nwa, LimSup);
    TEST_ASSERT_NOT_NULL(no_silent, "removeSilentTransitions returned null");
    verifyNestedAutomatonBasics(no_silent, "without silent");

    std::cout << "    After removeSilentTransitions(LimSup): "
              << no_silent->getNbStates() << " states" << std::endl;

    delete no_silent;
    delete nwa;
}

// ============================================================================
// getChild / getChildrenSize Tests
// ============================================================================

void test_getChild_getChildrenSize_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    size_t num_children = nwa->getChildrenSize();
    std::cout << "    Number of children: " << num_children << std::endl;

    for (size_t i = 0; i < num_children; ++i) {
        ChildAutomaton* child = nwa->getChild(i);
        if (child) {
            std::cout << "    Child " << i << ": " << child->getNbStates() << " states, "
                      << child->getNbTransitions() << " transitions" << std::endl;
        } else {
            std::cout << "    Child " << i << ": null" << std::endl;
        }
    }

    delete nwa;
}

void test_getChild_valid_indices() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    size_t num_children = nwa->getChildrenSize();

    // All valid indices should return non-null children (for this file)
    for (size_t i = 0; i < num_children; ++i) {
        ChildAutomaton* child = nwa->getChild(i);
        // Note: Some files may have null children, so we just verify the call doesn't crash
    }

    delete nwa;
}

void test_getChild_after_synchronize() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    std::cout << "    Before sync children: " << nwa->getChildrenSize() << std::endl;

    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");

    std::cout << "    After sync children: " << sync_nwa->getChildrenSize() << std::endl;
    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have 1 child after sync");

    ChildAutomaton* sync_child = sync_nwa->getChild(0);
    TEST_ASSERT_NOT_NULL(sync_child, "Synchronized child is null");

    std::cout << "    Synchronized child: " << sync_child->getNbStates() << " states" << std::endl;

    delete sync_nwa;
    delete nwa;
}

void test_parser_final_all_marks_all_parent_states_final() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");

    unsigned int parent_final_count = 0;
    for (unsigned int state_id = 0; state_id < nwa->getStates()->size(); ++state_id) {
        if (nwa->getStates()->at(state_id)->getFinal()) {
            ++parent_final_count;
        }
    }
    TEST_ASSERT_EQ(parent_final_count, nwa->getStates()->size(),
                   "final: all should mark every parent state final");

    ChildAutomaton* child = nwa->getChild(1);
    TEST_ASSERT_NOT_NULL(child, "child 1 should exist");
    unsigned int child_final_count = 0;
    for (unsigned int state_id = 0; state_id < child->getStates()->size(); ++state_id) {
        if (child->getStates()->at(state_id)->getFinal()) {
            ++child_final_count;
        }
    }
    TEST_ASSERT_EQ(child_final_count, 1u,
                   "explicit child final lists should still mark only the listed states");

    delete nwa;
}

// ============================================================================
// print() Tests
// ============================================================================

void test_print_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    === Print output (brief) ===" << std::endl;
    nwa->print(false);  // Brief output
    std::cout << "    === End print output ===" << std::endl;

    delete nwa;
}

void test_print_full() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    === Print output (full) ===" << std::endl;
    nwa->print(true);  // Full output
    std::cout << "    === End print output ===" << std::endl;

    delete nwa;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "TEST: Auxiliary Functions" << std::endl;
    std::cout << "========================================" << std::endl;

    // isDeterministicNested tests
    std::cout << "\n--- isDeterministicNested Tests ---" << std::endl;
    RUN_TEST(test_isDeterministicNested_test_empt_1);
    RUN_TEST(test_isDeterministicNested_test_empt_2);
    RUN_TEST(test_isDeterministicNested_test_empt_3);
    RUN_TEST(test_isDeterministicNested_nested_Sij);
    RUN_TEST(test_isDeterministicNested_after_determinization);

    // isCompleteNested tests
    std::cout << "\n--- isCompleteNested Tests ---" << std::endl;
    RUN_TEST(test_isCompleteNested_test_empt_1);
    RUN_TEST(test_isCompleteNested_test_empt_2);
    RUN_TEST(test_isCompleteNested_nested_Sij);

    // isDeterministicAndCompleteNested tests
    std::cout << "\n--- isDeterministicAndCompleteNested Tests ---" << std::endl;
    RUN_TEST(test_isDeterministicAndCompleteNested_test_empt_1);
    RUN_TEST(test_isDeterministicAndCompleteNested_test_empt_2);

    // makeCompleteNested tests
    std::cout << "\n--- makeCompleteNested Tests ---" << std::endl;
    RUN_TEST(test_makeCompleteNested_basic);
    RUN_TEST(test_makeCompleteNested_with_flags);
    RUN_TEST(test_makeCompleteNested_with_custom_weights);
    RUN_TEST(test_makeCompleteNested_idempotent);

    // removeSilentTransitions tests
    std::cout << "\n--- removeSilentTransitions Tests ---" << std::endl;
    RUN_TEST(test_removeSilentTransitions_LimInf);
    RUN_TEST(test_removeSilentTransitions_LimSup);

    // getChild / getChildrenSize tests
    std::cout << "\n--- getChild/getChildrenSize Tests ---" << std::endl;
    RUN_TEST(test_getChild_getChildrenSize_basic);
    RUN_TEST(test_getChild_valid_indices);
    RUN_TEST(test_getChild_after_synchronize);
    RUN_TEST(test_parser_final_all_marks_all_parent_states_final);

    // print tests
    std::cout << "\n--- print() Tests ---" << std::endl;
    RUN_TEST(test_print_basic);
    RUN_TEST(test_print_full);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
