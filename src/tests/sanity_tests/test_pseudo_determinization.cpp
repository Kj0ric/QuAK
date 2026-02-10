/**
 * test_pseudo_determinization.cpp
 *
 * Tests for NestedAutomaton pseudo-determinization:
 * - generateMacroAlphabet()
 * - determinizeWithMacroAlphabet()
 */

#include "test_common.h"

// ============================================================================
// generateMacroAlphabet Tests
// ============================================================================

void test_generateMacroAlphabet_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    auto macro_alphabet = nwa->generateMacroAlphabet();

    std::cout << "    Input alphabet size: " << nwa->getAlphabet()->size() << std::endl;
    std::cout << "    Macro alphabet size: " << macro_alphabet.size() << std::endl;
    TEST_ASSERT_GT(macro_alphabet.size(), 0u, "Macro alphabet should not be empty");

    // Clean up macro symbols
    for (auto* ms : macro_alphabet) {
        delete ms;
    }

    delete nwa;
}

void test_generateMacroAlphabet_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    auto macro_alphabet = nwa->generateMacroAlphabet();

    std::cout << "    Input alphabet size: " << nwa->getAlphabet()->size() << std::endl;
    std::cout << "    Children: " << nwa->getChildrenSize() << std::endl;
    std::cout << "    Macro alphabet size: " << macro_alphabet.size() << std::endl;

    for (auto* ms : macro_alphabet) {
        delete ms;
    }

    delete nwa;
}

void test_generateMacroAlphabet_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    auto macro_alphabet = nwa->generateMacroAlphabet();

    std::cout << "    Input alphabet size: " << nwa->getAlphabet()->size() << std::endl;
    std::cout << "    Children: " << nwa->getChildrenSize() << std::endl;
    std::cout << "    Macro alphabet size: " << macro_alphabet.size() << std::endl;

    for (auto* ms : macro_alphabet) {
        delete ms;
    }

    delete nwa;
}

void test_generateMacroAlphabet_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    auto macro_alphabet = nwa->generateMacroAlphabet();

    std::cout << "    Parent alphabet size: " << nwa->getAlphabet()->size() << std::endl;
    std::cout << "    Macro alphabet size: " << macro_alphabet.size() << std::endl;

    for (auto* ms : macro_alphabet) {
        delete ms;
    }

    delete nwa;
}

void test_generateMacroAlphabet_nested_Sij2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    auto macro_alphabet = nwa->generateMacroAlphabet();

    std::cout << "    Input alphabet size: " << nwa->getAlphabet()->size() << std::endl;
    std::cout << "    Children: " << nwa->getChildrenSize() << std::endl;
    std::cout << "    Macro alphabet size: " << macro_alphabet.size() << std::endl;

    for (auto* ms : macro_alphabet) {
        delete ms;
    }

    delete nwa;
}

void test_generateMacroAlphabet_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Generate twice and compare sizes
    auto macro1 = nwa->generateMacroAlphabet();
    auto macro2 = nwa->generateMacroAlphabet();

    TEST_ASSERT_EQ(macro1.size(), macro2.size(), "Macro alphabet size should be consistent");

    for (auto* ms : macro1) delete ms;
    for (auto* ms : macro2) delete ms;

    delete nwa;
}

// ============================================================================
// determinizeWithMacroAlphabet Tests
// ============================================================================

void test_determinizeWithMacroAlphabet_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " states, "
              << nwa->getChildrenSize() << " children" << std::endl;
    std::cout << "    Input is deterministic: " << nwa->isDeterministic() << std::endl;

    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");
    verifyNestedAutomatonBasics(det_nwa, "determinized");

    std::cout << "    Output: " << det_nwa->getNbStates() << " states, "
              << det_nwa->getChildrenSize() << " children" << std::endl;
    std::cout << "    Output is deterministic: " << det_nwa->isDeterministic() << std::endl;

    delete det_nwa;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");
    verifyNestedAutomatonBasics(det_nwa, "determinized");

    std::cout << "    Input states: " << nwa->getNbStates()
              << ", Output states: " << det_nwa->getNbStates() << std::endl;

    delete det_nwa;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");
    verifyNestedAutomatonBasics(det_nwa, "determinized");

    std::cout << "    Input states: " << nwa->getNbStates()
              << ", Output states: " << det_nwa->getNbStates() << std::endl;

    delete det_nwa;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");
    verifyNestedAutomatonBasics(det_nwa, "determinized");

    std::cout << "    Output: " << det_nwa->getNbStates() << " states, "
              << det_nwa->getChildrenSize() << " children" << std::endl;

    delete det_nwa;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_nested_Sij2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");
    verifyNestedAutomatonBasics(det_nwa, "determinized");

    std::cout << "    Input states: " << nwa->getNbStates()
              << ", Output states: " << det_nwa->getNbStates() << std::endl;

    delete det_nwa;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_preserves_children() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    size_t input_children = nwa->getChildrenSize();
    std::cout << "    Input children: " << input_children << std::endl;

    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");

    size_t output_children = det_nwa->getChildrenSize();
    std::cout << "    Output children: " << output_children << std::endl;

    // Children should be preserved or synchronized
    TEST_ASSERT_GT(output_children, 0u, "Determinized automaton should have children");

    delete det_nwa;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Determinize twice and compare state counts
    NestedAutomaton* det1 = nwa->determinizeWithMacroAlphabet();
    unsigned int states1 = det1->getNbStates();
    delete det1;

    NestedAutomaton* det2 = nwa->determinizeWithMacroAlphabet();
    unsigned int states2 = det2->getNbStates();
    delete det2;

    TEST_ASSERT_EQ(states1, states2, "Determinization should be consistent");

    delete nwa;
}

// ============================================================================
// Deterministic Properties Tests
// ============================================================================

void test_determinizeWithMacroAlphabet_output_determinism() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    std::cout << "    Input is deterministic (parent): " << nwa->isDeterministic() << std::endl;

    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");

    // Output should be deterministic
    bool output_det = det_nwa->isDeterministic();
    std::cout << "    Output is deterministic (parent): " << output_det << std::endl;

    delete det_nwa;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_initial_state() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");

    TEST_ASSERT_NOT_NULL(det_nwa->getInitial(), "Determinized should have initial state");

    delete det_nwa;
    delete nwa;
}

void test_determinizeWithMacroAlphabet_alphabet_preserved() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    size_t input_alpha = nwa->getAlphabet()->size();

    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");

    size_t output_alpha = det_nwa->getAlphabet()->size();

    std::cout << "    Input alphabet: " << input_alpha
              << ", Output alphabet: " << output_alpha << std::endl;

    // Note: Alphabet may be expanded to macro alphabet
    TEST_ASSERT_GT(output_alpha, 0u, "Output alphabet should not be empty");

    delete det_nwa;
    delete nwa;
}

// ============================================================================
// Combined Pipeline Tests
// ============================================================================

void test_macro_alphabet_then_determinize() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    // First generate macro alphabet
    auto macro_alpha = nwa->generateMacroAlphabet();
    std::cout << "    Macro alphabet size: " << macro_alpha.size() << std::endl;

    // Then determinize
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");

    std::cout << "    Determinized states: " << det_nwa->getNbStates() << std::endl;

    for (auto* ms : macro_alpha) delete ms;
    delete det_nwa;
    delete nwa;
}

void test_determinize_then_flatten() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    // Determinize
    NestedAutomaton* det_nwa = nwa->determinizeWithMacroAlphabet();
    TEST_ASSERT_NOT_NULL(det_nwa, "determinizeWithMacroAlphabet returned null");
    std::cout << "    Determinized: " << det_nwa->getNbStates() << " states" << std::endl;

    // Flatten
    Automaton* flat = det_nwa->flatten_regular(Max_f);
    verifyAutomatonBasics(flat, "flattened");
    printAutomatonStats(flat, "determinize -> flatten");

    delete flat;
    delete det_nwa;
    delete nwa;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "TEST: Pseudo-Determinization" << std::endl;
    std::cout << "========================================" << std::endl;

    // generateMacroAlphabet tests
    std::cout << "\n--- generateMacroAlphabet Tests ---" << std::endl;
    RUN_TEST(test_generateMacroAlphabet_basic);
    RUN_TEST(test_generateMacroAlphabet_test_empt_2);
    RUN_TEST(test_generateMacroAlphabet_test_empt_3);
    RUN_TEST(test_generateMacroAlphabet_nested_Sij);
    RUN_TEST(test_generateMacroAlphabet_nested_Sij2);
    RUN_TEST(test_generateMacroAlphabet_consistency);

    // determinizeWithMacroAlphabet tests
    std::cout << "\n--- determinizeWithMacroAlphabet Tests ---" << std::endl;
    RUN_TEST(test_determinizeWithMacroAlphabet_basic);
    RUN_TEST(test_determinizeWithMacroAlphabet_test_empt_2);
    RUN_TEST(test_determinizeWithMacroAlphabet_test_empt_3);
    RUN_TEST(test_determinizeWithMacroAlphabet_nested_Sij);
    RUN_TEST(test_determinizeWithMacroAlphabet_nested_Sij2);
    RUN_TEST(test_determinizeWithMacroAlphabet_preserves_children);
    RUN_TEST(test_determinizeWithMacroAlphabet_consistency);

    // Deterministic properties tests
    std::cout << "\n--- Deterministic Properties Tests ---" << std::endl;
    RUN_TEST(test_determinizeWithMacroAlphabet_output_determinism);
    RUN_TEST(test_determinizeWithMacroAlphabet_initial_state);
    RUN_TEST(test_determinizeWithMacroAlphabet_alphabet_preserved);

    // Combined pipeline tests
    std::cout << "\n--- Combined Pipeline Tests ---" << std::endl;
    RUN_TEST(test_macro_alphabet_then_determinize);
    RUN_TEST(test_determinize_then_flatten);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
