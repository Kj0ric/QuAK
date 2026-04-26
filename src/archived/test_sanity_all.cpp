/**
 * test_sanity_all.cpp
 *
 * Combined sanity tests for all NestedAutomaton functions.
 * Includes tests from:
 * - flatten_regular
 * - flatten_SumPlusMinus_Sup/Inf
 * - flatten_MinMax_Sup/Inf
 * - flatten_Avg_SumMinus
 * - isNonEmpty / isUniversal
 * - synchronizeChildren
 * - pseudo-determinization (generateMacroAlphabet, determinizeWithMacroAlphabet)
 * - auxiliary functions (isDeterministicNested, isCompleteNested, etc.)
 */

#include "test_common.h"

// Helper to compute c_bound
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
// SECTION 1: flatten_regular Tests
// ============================================================================

void test_flatten_regular_SumB_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat = nwa->flatten_regular(SumB, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_regular_SumB_varying_bounds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    unsigned int prev_states = 0;
    for (int bound = 1; bound <= 5; ++bound) {
        Automaton* flat = nwa->flatten_regular(SumB, weight_t(bound));
        verifyAutomatonBasics(flat, "flattened");
        unsigned int curr_states = flat->getStates()->size();
        if (bound > 1) {
            TEST_ASSERT_GE(curr_states, prev_states, "State count should not decrease with larger bound");
        }
        prev_states = curr_states;
        delete flat;
    }
    delete nwa;
}

void test_flatten_regular_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::COMPUTE_RETURN_MAX);
    Automaton* flat = nwa->flatten_regular(Max_f);
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_regular_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::COMPUTE_RETURN_MIN);
    Automaton* flat = nwa->flatten_regular(Min_f);
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_regular_Max_vs_Min_same_input() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat_max = nwa->flatten_regular(Max_f);
    Automaton* flat_min = nwa->flatten_regular(Min_f);
    TEST_ASSERT_EQ(flat_max->getAlphabet()->size(), flat_min->getAlphabet()->size(), "Alphabet sizes should match");
    delete flat_max;
    delete flat_min;
    delete nwa;
}

void test_flatten_regular_consistency_multiple_calls() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat1 = nwa->flatten_regular(SumB, weight_t(3));
    unsigned int states1 = flat1->getStates()->size();
    delete flat1;
    Automaton* flat2 = nwa->flatten_regular(SumB, weight_t(3));
    unsigned int states2 = flat2->getStates()->size();
    delete flat2;
    TEST_ASSERT_EQ(states1, states2, "State counts should be consistent");
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
// SECTION 2: flatten_SumPlusMinus_Sup Tests
// ============================================================================

void test_flatten_SumPlusMinus_Sup_SumPlus_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Sup(nwa, SumPlus, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Sup_SumPlus_varying_thresholds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    for (int threshold = 0; threshold <= 5; ++threshold) {
        Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Sup(nwa, SumPlus, weight_t(threshold));
        verifyAutomatonBasics(flat, "flattened");
        delete flat;
    }
    delete nwa;
}

void test_flatten_SumPlusMinus_Sup_SumMinus_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Sup(nwa, SumMinus, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Sup_output_properties() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Sup(nwa, SumPlus, weight_t(5));
    TEST_ASSERT_NOT_NULL(flat->getInitial(), "Flattened automaton should have initial state");
    bool is_01_or_silent = hasOnly01OrSilentWeights(flat);
    TEST_ASSERT(is_01_or_silent, "Output should have only 0/1/SILENT weights");
    delete flat;
    delete nwa;
}

// ============================================================================
// SECTION 3: flatten_SumPlusMinus_Inf Tests
// ============================================================================

void test_flatten_SumPlusMinus_Inf_SumPlus_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumPlus_varying_thresholds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    for (int threshold = 0; threshold <= 5; ++threshold) {
        Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(threshold));
        verifyAutomatonBasics(flat, "flattened");
        delete flat;
    }
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_SumMinus_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumMinus, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_SumPlusMinus_Inf_vs_Sup() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat_inf = NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, SumPlus, weight_t(3));
    Automaton* flat_sup = NestedAutomatonTester::flatten_SumPlusMinus_Sup(nwa, SumPlus, weight_t(3));
    TEST_ASSERT_EQ(flat_inf->getAlphabet()->size(), flat_sup->getAlphabet()->size(), "Alphabet sizes should match");
    delete flat_inf;
    delete flat_sup;
    delete nwa;
}

// ============================================================================
// SECTION 4: flatten_MinMax_Sup Tests
// ============================================================================

void test_flatten_MinMax_Sup_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Max_varying_thresholds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    for (int threshold = 0; threshold <= 5; ++threshold) {
        Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(threshold));
        verifyAutomatonBasics(flat, "flattened");
        delete flat;
    }
    delete nwa;
}

void test_flatten_MinMax_Sup_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Min_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Sup_Max_vs_Min() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat_max = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(3));
    Automaton* flat_min = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Min_f, weight_t(3));
    TEST_ASSERT_EQ(flat_max->getAlphabet()->size(), flat_min->getAlphabet()->size(), "Alphabet sizes should match");
    delete flat_max;
    delete flat_min;
    delete nwa;
}

// ============================================================================
// SECTION 5: flatten_MinMax_Inf Tests
// ============================================================================

void test_flatten_MinMax_Inf_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_Max_varying_thresholds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    for (int threshold = 0; threshold <= 5; ++threshold) {
        Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(threshold));
        verifyAutomatonBasics(flat, "flattened");
        delete flat;
    }
    delete nwa;
}

void test_flatten_MinMax_Inf_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Min_f, weight_t(5));
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete nwa;
}

void test_flatten_MinMax_Inf_vs_Sup() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    Automaton* flat_inf = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, weight_t(3));
    Automaton* flat_sup = NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, weight_t(3));
    TEST_ASSERT_EQ(flat_inf->getAlphabet()->size(), flat_sup->getAlphabet()->size(), "Alphabet sizes should match");
    delete flat_inf;
    delete flat_sup;
    delete nwa;
}

// ============================================================================
// SECTION 6: flatten_Avg_SumMinus Tests
// ============================================================================

void test_flatten_Avg_SumMinus_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    uint64_t c_bound = compute_c_bound_for_test(nwa);
    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete sync_nwa;
    delete nwa;
}

void test_flatten_Avg_SumMinus_varying_cbounds() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    for (uint64_t c_bound = 1; c_bound <= 5; ++c_bound) {
        Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
        verifyAutomatonBasics(flat, "flattened");
        delete flat;
    }
    delete sync_nwa;
    delete nwa;
}

void test_flatten_Avg_SumMinus_with_determinization() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet failed");
    NestedAutomaton* sync_nwa = det_nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    uint64_t c_bound = compute_c_bound_for_test(nwa);
    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete sync_nwa;
    delete det_nwa;
    delete nwa;
}

// ============================================================================
// SECTION 7: isNonEmpty / isUniversal Tests
// ============================================================================

void test_isNonEmpty_LimSup_SumB_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    for (weight_t x = 0; x <= 5; ++x) {
        nwa->isNonEmpty(LimSup, SumB, x, weight_t(5));
    }
    delete nwa;
}

void test_isNonEmpty_LimSup_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    for (weight_t x = 0; x <= 5; ++x) {
        nwa->isNonEmpty(LimSup, Max_f, x);
    }
    delete nwa;
}

void test_isNonEmpty_LimInf_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    for (weight_t x = 0; x <= 5; ++x) {
        nwa->isNonEmpty(LimInf, Min_f, x);
    }
    delete nwa;
}

void test_isUniversal_LimSup_SumB_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    for (weight_t x = 0; x <= 5; ++x) {
        nwa->isUniversal(LimSup, SumB, x, weight_t(5));
    }
    delete nwa;
}

void test_isUniversal_LimSup_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    for (weight_t x = 0; x <= 5; ++x) {
        nwa->isUniversal(LimSup, Max_f, x);
    }
    delete nwa;
}

void test_emptiness_universality_relationship() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    for (weight_t x = 0; x <= 3; ++x) {
        bool univ = nwa->isUniversal(LimSup, SumB, x, weight_t(5));
        bool nonempty = nwa->isNonEmpty(LimSup, SumB, x, weight_t(5));
        if (univ) {
            TEST_ASSERT(nonempty, "Universal implies non-empty");
        }
    }
    delete nwa;
}

void test_emptiness_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    bool result1 = nwa->isNonEmpty(LimSup, SumB, weight_t(1), weight_t(5));
    bool result2 = nwa->isNonEmpty(LimSup, SumB, weight_t(1), weight_t(5));
    TEST_ASSERT_EQ(result1 ? 1 : 0, result2 ? 1 : 0, "isNonEmpty should be consistent");
    delete nwa;
}

// ============================================================================
// SECTION 8: synchronizeChildren Tests
// ============================================================================

void test_synchronizeChildren_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren returned null");
    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have exactly 1 synchronized child");
    delete sync_nwa;
    delete nwa;
}

void test_synchronizeChildren_child_properties() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren returned null");
    ChildAutomaton* sync_child = sync_nwa->getChild(0);
    TEST_ASSERT_NOT_NULL(sync_child, "Synchronized child is null");
    TEST_ASSERT(sync_child->getInitial() == nullptr, "Synchronized child should not have initial state");
    delete sync_nwa;
    delete nwa;
}

void test_synchronizeChildren_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
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
    NestedAutomaton* sync1 = nwa->synchronizeChildren();
    TEST_ASSERT_EQ(sync1->getChildrenSize(), 1u, "Should have 1 child after first sync");
    NestedAutomaton* sync2 = sync1->synchronizeChildren();
    TEST_ASSERT_EQ(sync2->getChildrenSize(), 1u, "Should still have 1 child");
    delete sync2;
    delete sync1;
    delete nwa;
}

// ============================================================================
// SECTION 9: Pseudo-Determinization Tests
// ============================================================================

void test_generateMacroAlphabet_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    auto macro_alphabet = nwa->generateMacroAlphabet();
    TEST_ASSERT_GT(macro_alphabet.size(), 0u, "Macro alphabet should not be empty");
    for (auto* ms : macro_alphabet) {
        delete ms;
    }
    delete nwa;
}

void test_generateMacroAlphabet_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    auto macro1 = nwa->generateMacroAlphabet();
    auto macro2 = nwa->generateMacroAlphabet();
    TEST_ASSERT_EQ(macro1.size(), macro2.size(), "Macro alphabet size should be consistent");
    for (auto* ms : macro1) delete ms;
    for (auto* ms : macro2) delete ms;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");
    verifyNestedAutomatonBasics(det_nwa, "determinized");
    delete det_nwa;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_preserves_children() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");
    TEST_ASSERT_GT(det_nwa->getChildrenSize(), 0u, "Determinized automaton should have children");
    delete det_nwa;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* det1 = nwa->determinizeWithMacroAlphabet();
    unsigned int states1 = det1->getNbStates();
    delete det1;
    NestedAutomaton* det2 = nwa->determinizeWithMacroAlphabet();
    unsigned int states2 = det2->getNbStates();
    delete det2;
    TEST_ASSERT_EQ(states1, states2, "Determinization should be consistent");
    delete nwa;
}

void test_determinizeWithMacroAlphabet_output_determinism() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");
    // Output should be deterministic
    det_nwa->isDeterministic();  // Just verify the call works
    delete det_nwa;
    delete nwa;
}

void test_determinize_then_flatten() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");
    Automaton* flat = det_nwa->flatten_regular(Max_f);
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete det_nwa;
    delete nwa;
}

// ============================================================================
// SECTION 10: Auxiliary Functions Tests
// ============================================================================

void test_isDeterministicNested_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    nwa->isDeterministicNested();
    nwa->isDeterministic();
    delete nwa;
}

void test_isDeterministicNested_after_determinization() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet failed");
    det_nwa->isDeterministicNested();
    delete det_nwa;
    delete nwa;
}

void test_isCompleteNested_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::vector<bool> complete_flags;
    nwa->isCompleteNested(&complete_flags);
    nwa->isComplete();
    delete nwa;
}

void test_isDeterministicAndCompleteNested_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    nwa->isDeterministicAndCompleteNested();
    delete nwa;
}

void test_makeCompleteNested_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* complete_nwa = nwa->makeCompleteNested();
    TEST_ASSERT_NOT_NULL(complete_nwa, "makeCompleteNested returned null");
    verifyNestedAutomatonBasics(complete_nwa, "completed");
    delete complete_nwa;
    delete nwa;
}

void test_makeCompleteNested_idempotent() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* complete1 = nwa->makeCompleteNested();
    TEST_ASSERT_NOT_NULL(complete1, "First makeCompleteNested failed");
    unsigned int states1 = complete1->getNbStates();
    NestedAutomaton* complete2 = complete1->makeCompleteNested();
    TEST_ASSERT_NOT_NULL(complete2, "Second makeCompleteNested failed");
    unsigned int states2 = complete2->getNbStates();
    TEST_ASSERT_EQ(states1, states2, "makeCompleteNested should be idempotent");
    delete complete2;
    delete complete1;
    delete nwa;
}

void test_removeSilentTransitions_LimInf() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIL1);
    NestedAutomaton* no_silent = NestedAutomaton::removeSilentTransitions(nwa, LimInf);
    TEST_ASSERT_NOT_NULL(no_silent, "removeSilentTransitions returned null");
    verifyNestedAutomatonBasics(no_silent, "without silent");
    delete no_silent;
    delete nwa;
}

void test_removeSilentTransitions_LimSup() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIL1);
    NestedAutomaton* no_silent = NestedAutomaton::removeSilentTransitions(nwa, LimSup);
    TEST_ASSERT_NOT_NULL(no_silent, "removeSilentTransitions returned null");
    verifyNestedAutomatonBasics(no_silent, "without silent");
    delete no_silent;
    delete nwa;
}

void test_getChild_getChildrenSize_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    size_t num_children = nwa->getChildrenSize();
    for (size_t i = 0; i < num_children; ++i) {
        nwa->getChild(i);  // Just verify the call doesn't crash
    }
    delete nwa;
}

void test_getChild_after_synchronize() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have 1 child after sync");
    ChildAutomaton* sync_child = sync_nwa->getChild(0);
    TEST_ASSERT_NOT_NULL(sync_child, "Synchronized child is null");
    delete sync_nwa;
    delete nwa;
}

void test_print_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    // Just verify print doesn't crash (output suppressed)
    std::ostringstream oss;
    std::streambuf* old = std::cout.rdbuf(oss.rdbuf());
    nwa->print(false);
    std::cout.rdbuf(old);
    delete nwa;
}

// ============================================================================
// SECTION 11: Pipeline Integration Tests
// ============================================================================

void test_full_pipeline_det_sync_flatten() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinize failed");
    NestedAutomaton* sync_nwa = det_nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronize failed");
    uint64_t c_bound = static_cast<uint64_t>(sync_nwa->getChild(0)->getStates()->size())
                     * sync_nwa->getStates()->size();
    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete sync_nwa;
    delete det_nwa;
    delete nwa;
}

void test_determinize_then_synchronize() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet failed");
    NestedAutomaton* sync_nwa = det_nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Should have 1 child after sync");
    delete sync_nwa;
    delete det_nwa;
    delete nwa;
}

void test_synchronize_then_flatten_Avg_SumMinus() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    NestedAutomaton* sync_nwa = nwa->synchronizeChildren();
    TEST_ASSERT_NOT_NULL(sync_nwa, "synchronizeChildren failed");
    TEST_ASSERT_EQ(sync_nwa->getChildrenSize(), 1u, "Need exactly 1 child for flatten_Avg_SumMinus");
    uint64_t c_bound = static_cast<uint64_t>(sync_nwa->getChild(0)->getStates()->size())
                     * sync_nwa->getStates()->size();
    Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    verifyAutomatonBasics(flat, "flattened");
    delete flat;
    delete sync_nwa;
    delete nwa;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "COMBINED SANITY TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    // Section 1: flatten_regular
    std::cout << "\n--- flatten_regular Tests ---" << std::endl;
    RUN_TEST(test_flatten_regular_SumB_basic);
    RUN_TEST(test_flatten_regular_SumB_varying_bounds);
    RUN_TEST(test_flatten_regular_Max_basic);
    RUN_TEST(test_flatten_regular_Min_basic);
    RUN_TEST(test_flatten_regular_Max_vs_Min_same_input);
    RUN_TEST(test_flatten_regular_consistency_multiple_calls);
    RUN_TEST(test_parser_final_all_marks_all_parent_states_final);

    // Section 2: flatten_SumPlusMinus_Sup
    std::cout << "\n--- flatten_SumPlusMinus_Sup Tests ---" << std::endl;
    RUN_TEST(test_flatten_SumPlusMinus_Sup_SumPlus_basic);
    RUN_TEST(test_flatten_SumPlusMinus_Sup_SumPlus_varying_thresholds);
    RUN_TEST(test_flatten_SumPlusMinus_Sup_SumMinus_basic);
    RUN_TEST(test_flatten_SumPlusMinus_Sup_output_properties);

    // Section 3: flatten_SumPlusMinus_Inf
    std::cout << "\n--- flatten_SumPlusMinus_Inf Tests ---" << std::endl;
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumPlus_basic);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumPlus_varying_thresholds);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_SumMinus_basic);
    RUN_TEST(test_flatten_SumPlusMinus_Inf_vs_Sup);

    // Section 4: flatten_MinMax_Sup
    std::cout << "\n--- flatten_MinMax_Sup Tests ---" << std::endl;
    RUN_TEST(test_flatten_MinMax_Sup_Max_basic);
    RUN_TEST(test_flatten_MinMax_Sup_Max_varying_thresholds);
    RUN_TEST(test_flatten_MinMax_Sup_Min_basic);
    RUN_TEST(test_flatten_MinMax_Sup_Max_vs_Min);

    // Section 5: flatten_MinMax_Inf
    std::cout << "\n--- flatten_MinMax_Inf Tests ---" << std::endl;
    RUN_TEST(test_flatten_MinMax_Inf_Max_basic);
    RUN_TEST(test_flatten_MinMax_Inf_Max_varying_thresholds);
    RUN_TEST(test_flatten_MinMax_Inf_Min_basic);
    RUN_TEST(test_flatten_MinMax_Inf_vs_Sup);

    // Section 6: flatten_Avg_SumMinus
    std::cout << "\n--- flatten_Avg_SumMinus Tests ---" << std::endl;
    RUN_TEST(test_flatten_Avg_SumMinus_basic);
    RUN_TEST(test_flatten_Avg_SumMinus_varying_cbounds);
    RUN_TEST(test_flatten_Avg_SumMinus_with_determinization);

    // Section 7: isNonEmpty / isUniversal
    std::cout << "\n--- isNonEmpty / isUniversal Tests ---" << std::endl;
    RUN_TEST(test_isNonEmpty_LimSup_SumB_basic);
    RUN_TEST(test_isNonEmpty_LimSup_Max_basic);
    RUN_TEST(test_isNonEmpty_LimInf_Min_basic);
    RUN_TEST(test_isUniversal_LimSup_SumB_basic);
    RUN_TEST(test_isUniversal_LimSup_Max_basic);
    RUN_TEST(test_emptiness_universality_relationship);
    RUN_TEST(test_emptiness_consistency);

    // Section 8: synchronizeChildren
    std::cout << "\n--- synchronizeChildren Tests ---" << std::endl;
    RUN_TEST(test_synchronizeChildren_basic);
    RUN_TEST(test_synchronizeChildren_child_properties);
    RUN_TEST(test_synchronizeChildren_consistency);
    RUN_TEST(test_synchronizeChildren_idempotent);

    // Section 9: Pseudo-Determinization
    std::cout << "\n--- Pseudo-Determinization Tests ---" << std::endl;
    RUN_TEST(test_generateMacroAlphabet_basic);
    RUN_TEST(test_generateMacroAlphabet_consistency);
    RUN_TEST(test_determinizeWithMacroAlphabet_basic);
    RUN_TEST(test_determinizeWithMacroAlphabet_preserves_children);
    RUN_TEST(test_determinizeWithMacroAlphabet_consistency);
    RUN_TEST(test_determinizeWithMacroAlphabet_output_determinism);
    RUN_TEST(test_determinize_then_flatten);

    // Section 10: Auxiliary Functions
    std::cout << "\n--- Auxiliary Functions Tests ---" << std::endl;
    RUN_TEST(test_isDeterministicNested_basic);
    RUN_TEST(test_isDeterministicNested_after_determinization);
    RUN_TEST(test_isCompleteNested_basic);
    RUN_TEST(test_isDeterministicAndCompleteNested_basic);
    RUN_TEST(test_makeCompleteNested_basic);
    RUN_TEST(test_makeCompleteNested_idempotent);
    RUN_TEST(test_removeSilentTransitions_LimInf);
    RUN_TEST(test_removeSilentTransitions_LimSup);
    RUN_TEST(test_getChild_getChildrenSize_basic);
    RUN_TEST(test_getChild_after_synchronize);
    RUN_TEST(test_print_basic);

    // Section 11: Pipeline Integration
    std::cout << "\n--- Pipeline Integration Tests ---" << std::endl;
    RUN_TEST(test_full_pipeline_det_sync_flatten);
    RUN_TEST(test_determinize_then_synchronize);
    RUN_TEST(test_synchronize_then_flatten_Avg_SumMinus);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
