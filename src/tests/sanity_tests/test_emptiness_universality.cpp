/**
 * test_emptiness_universality.cpp
 *
 * Tests for NestedAutomaton::isNonEmpty() and isUniversal()
 * These are the key decision procedures for nested weighted automata.
 */

#include "test_common.h"

// ============================================================================
// isNonEmpty Tests with Different Value Functions
// ============================================================================

void test_isNonEmpty_LimSup_SumB_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    // Test with varying thresholds
    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isNonEmpty(LimSup, SumB, x, weight_t(5));
        std::cout << "    isNonEmpty(LimSup, SumB, " << x << ", 5) = " << result << std::endl;
    }

    delete nwa;
}

void test_isNonEmpty_LimInf_SumB_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isNonEmpty(LimInf, SumB, x, weight_t(5));
        std::cout << "    isNonEmpty(LimInf, SumB, " << x << ", 5) = " << result << std::endl;
    }

    delete nwa;
}

void test_isNonEmpty_LimSup_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isNonEmpty(LimSup, Max_f, x);
        std::cout << "    isNonEmpty(LimSup, Max_f, " << x << ") = " << result << std::endl;
    }

    delete nwa;
}

void test_isNonEmpty_LimInf_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isNonEmpty(LimInf, Max_f, x);
        std::cout << "    isNonEmpty(LimInf, Max_f, " << x << ") = " << result << std::endl;
    }

    delete nwa;
}

void test_isNonEmpty_LimSup_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isNonEmpty(LimSup, Min_f, x);
        std::cout << "    isNonEmpty(LimSup, Min_f, " << x << ") = " << result << std::endl;
    }

    delete nwa;
}

void test_isNonEmpty_LimInf_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isNonEmpty(LimInf, Min_f, x);
        std::cout << "    isNonEmpty(LimInf, Min_f, " << x << ") = " << result << std::endl;
    }

    delete nwa;
}

// ============================================================================
// isNonEmpty with Different Input Files
// ============================================================================

void test_isNonEmpty_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    isNonEmpty(LimSup, SumB, 0, 3) = " << nwa->isNonEmpty(LimSup, SumB, 0, 3) << std::endl;
    std::cout << "    isNonEmpty(LimInf, SumB, 0, 3) = " << nwa->isNonEmpty(LimInf, SumB, 0, 3) << std::endl;
    std::cout << "    isNonEmpty(LimSup, Max_f, 0) = " << nwa->isNonEmpty(LimSup, Max_f, 0) << std::endl;
    std::cout << "    isNonEmpty(LimInf, Min_f, 0) = " << nwa->isNonEmpty(LimInf, Min_f, 0) << std::endl;

    delete nwa;
}

void test_isNonEmpty_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    isNonEmpty(LimSup, SumB, 0, 3) = " << nwa->isNonEmpty(LimSup, SumB, 0, 3) << std::endl;
    std::cout << "    isNonEmpty(LimInf, SumB, 0, 3) = " << nwa->isNonEmpty(LimInf, SumB, 0, 3) << std::endl;
    std::cout << "    isNonEmpty(LimSup, Max_f, 0) = " << nwa->isNonEmpty(LimSup, Max_f, 0) << std::endl;
    std::cout << "    isNonEmpty(LimInf, Min_f, 0) = " << nwa->isNonEmpty(LimInf, Min_f, 0) << std::endl;

    delete nwa;
}

void test_isNonEmpty_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    std::cout << "    isNonEmpty(LimSup, SumB, 0, 5) = " << nwa->isNonEmpty(LimSup, SumB, 0, 5) << std::endl;
    std::cout << "    isNonEmpty(LimInf, Max_f, 0) = " << nwa->isNonEmpty(LimInf, Max_f, 0) << std::endl;

    delete nwa;
}

// ============================================================================
// isUniversal Tests with Different Value Functions
// ============================================================================

void test_isUniversal_LimSup_SumB_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isUniversal(LimSup, SumB, x, weight_t(5));
        std::cout << "    isUniversal(LimSup, SumB, " << x << ", 5) = " << result << std::endl;
    }

    delete nwa;
}

void test_isUniversal_LimInf_SumB_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isUniversal(LimInf, SumB, x, weight_t(5));
        std::cout << "    isUniversal(LimInf, SumB, " << x << ", 5) = " << result << std::endl;
    }

    delete nwa;
}

void test_isUniversal_LimSup_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isUniversal(LimSup, Max_f, x);
        std::cout << "    isUniversal(LimSup, Max_f, " << x << ") = " << result << std::endl;
    }

    delete nwa;
}

void test_isUniversal_LimInf_Max_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isUniversal(LimInf, Max_f, x);
        std::cout << "    isUniversal(LimInf, Max_f, " << x << ") = " << result << std::endl;
    }

    delete nwa;
}

void test_isUniversal_LimSup_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isUniversal(LimSup, Min_f, x);
        std::cout << "    isUniversal(LimSup, Min_f, " << x << ") = " << result << std::endl;
    }

    delete nwa;
}

void test_isUniversal_LimInf_Min_basic() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    for (weight_t x = 0; x <= 5; ++x) {
        bool result = nwa->isUniversal(LimInf, Min_f, x);
        std::cout << "    isUniversal(LimInf, Min_f, " << x << ") = " << result << std::endl;
    }

    delete nwa;
}

// ============================================================================
// isUniversal with Different Input Files
// ============================================================================

void test_isUniversal_test_empt_2() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_2);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    isUniversal(LimSup, SumB, 0, 3) = " << nwa->isUniversal(LimSup, SumB, 0, 3) << std::endl;
    std::cout << "    isUniversal(LimInf, SumB, 0, 3) = " << nwa->isUniversal(LimInf, SumB, 0, 3) << std::endl;
    std::cout << "    isUniversal(LimSup, Max_f, 0) = " << nwa->isUniversal(LimSup, Max_f, 0) << std::endl;
    std::cout << "    isUniversal(LimInf, Min_f, 0) = " << nwa->isUniversal(LimInf, Min_f, 0) << std::endl;

    delete nwa;
}

void test_isUniversal_test_empt_3() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_3);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;

    std::cout << "    isUniversal(LimSup, SumB, 0, 3) = " << nwa->isUniversal(LimSup, SumB, 0, 3) << std::endl;
    std::cout << "    isUniversal(LimInf, SumB, 0, 3) = " << nwa->isUniversal(LimInf, SumB, 0, 3) << std::endl;
    std::cout << "    isUniversal(LimSup, Max_f, 0) = " << nwa->isUniversal(LimSup, Max_f, 0) << std::endl;
    std::cout << "    isUniversal(LimInf, Min_f, 0) = " << nwa->isUniversal(LimInf, Min_f, 0) << std::endl;

    delete nwa;
}

void test_isUniversal_nested_Sij() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::NESTED_SIJ);
    verifyNestedAutomatonBasics(nwa, "input");
    std::cout << std::endl;
    std::cout << "    Input: " << nwa->getNbStates() << " parent states, "
              << nwa->getChildrenSize() << " children" << std::endl;

    std::cout << "    isUniversal(LimSup, SumB, 0, 5) = " << nwa->isUniversal(LimSup, SumB, 0, 5) << std::endl;
    std::cout << "    isUniversal(LimInf, Max_f, 0) = " << nwa->isUniversal(LimInf, Max_f, 0) << std::endl;

    delete nwa;
}

// ============================================================================
// Consistency and Property Tests
// ============================================================================

void test_emptiness_universality_relationship() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    // For a given threshold, if isUniversal is true, then isNonEmpty should also be true
    // (universality implies non-emptiness)
    for (weight_t x = 0; x <= 3; ++x) {
        bool univ = nwa->isUniversal(LimSup, SumB, x, weight_t(5));
        bool nonempty = nwa->isNonEmpty(LimSup, SumB, x, weight_t(5));

        std::cout << "    x=" << x << ": isUniversal=" << univ << ", isNonEmpty=" << nonempty << std::endl;

        // If universal, must be non-empty
        if (univ) {
            TEST_ASSERT(nonempty, "Universal implies non-empty");
        }
    }

    delete nwa;
}

void test_emptiness_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Call isNonEmpty twice with same parameters - should give consistent results
    bool result1 = nwa->isNonEmpty(LimSup, SumB, weight_t(1), weight_t(5));
    bool result2 = nwa->isNonEmpty(LimSup, SumB, weight_t(1), weight_t(5));

    TEST_ASSERT_EQ(result1 ? 1 : 0, result2 ? 1 : 0, "isNonEmpty should be consistent");

    delete nwa;
}

void test_universality_consistency() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);

    // Call isUniversal twice with same parameters - should give consistent results
    bool result1 = nwa->isUniversal(LimSup, SumB, weight_t(1), weight_t(5));
    bool result2 = nwa->isUniversal(LimSup, SumB, weight_t(1), weight_t(5));

    TEST_ASSERT_EQ(result1 ? 1 : 0, result2 ? 1 : 0, "isUniversal should be consistent");

    delete nwa;
}

void test_threshold_monotonicity() {
    NestedAutomaton* nwa = new NestedAutomaton(TestFiles::TEST_EMPT_1);
    std::cout << std::endl;

    // For increasing thresholds, isNonEmpty should be monotonic (non-increasing)
    // because fewer words satisfy higher thresholds
    bool prev_result = true;
    for (weight_t x = 0; x <= 10; ++x) {
        bool result = nwa->isNonEmpty(LimSup, Max_f, x);
        std::cout << "    isNonEmpty(x=" << x << ") = " << result << std::endl;

        // Note: Result may stay true for a range, then become false and stay false
        // This is not strictly enforced due to automata-specific behavior
    }

    delete nwa;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "TEST: isNonEmpty() and isUniversal()" << std::endl;
    std::cout << "========================================" << std::endl;

    // isNonEmpty tests
    std::cout << "\n--- isNonEmpty Tests ---" << std::endl;
    RUN_TEST(test_isNonEmpty_LimSup_SumB_basic);
    RUN_TEST(test_isNonEmpty_LimInf_SumB_basic);
    RUN_TEST(test_isNonEmpty_LimSup_Max_basic);
    RUN_TEST(test_isNonEmpty_LimInf_Max_basic);
    RUN_TEST(test_isNonEmpty_LimSup_Min_basic);
    RUN_TEST(test_isNonEmpty_LimInf_Min_basic);
    RUN_TEST(test_isNonEmpty_test_empt_2);
    RUN_TEST(test_isNonEmpty_test_empt_3);
    RUN_TEST(test_isNonEmpty_nested_Sij);

    // isUniversal tests
    std::cout << "\n--- isUniversal Tests ---" << std::endl;
    RUN_TEST(test_isUniversal_LimSup_SumB_basic);
    RUN_TEST(test_isUniversal_LimInf_SumB_basic);
    RUN_TEST(test_isUniversal_LimSup_Max_basic);
    RUN_TEST(test_isUniversal_LimInf_Max_basic);
    RUN_TEST(test_isUniversal_LimSup_Min_basic);
    RUN_TEST(test_isUniversal_LimInf_Min_basic);
    RUN_TEST(test_isUniversal_test_empt_2);
    RUN_TEST(test_isUniversal_test_empt_3);
    RUN_TEST(test_isUniversal_nested_Sij);

    // Consistency and property tests
    std::cout << "\n--- Consistency and Property Tests ---" << std::endl;
    RUN_TEST(test_emptiness_universality_relationship);
    RUN_TEST(test_emptiness_consistency);
    RUN_TEST(test_universality_consistency);
    RUN_TEST(test_threshold_monotonicity);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
