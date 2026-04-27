#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <chrono>
#include <functional>
#include <algorithm>

#include "../../NestedAutomaton.h"
#include "../../ChildAutomaton.h"
#include "../../Parser.h"
#include "../../Edge.h"

// Test result tracking
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    double duration_ms;
};

// Global test results
inline std::vector<TestResult> g_test_results;

// Test helper class that provides access to private NestedAutomaton methods
class NestedAutomatonTester {
public:
    // Wrapper for private flatten_SumPlusMinus_Sup
    static Automaton* flatten_SumPlusMinus_Sup(NestedAutomaton* nwa,
                                                value_function_t finite_aggregator,
                                                weight_t threshold) {
        return nwa->flatten_SumPlusMinus_Sup(finite_aggregator, threshold);
    }

    static Automaton* flatten_SumPlusMinus_Sup_witness_cached(NestedAutomaton* nwa,
                                                              value_function_t finite_aggregator,
                                                              weight_t threshold) {
        return nwa->flatten_SumPlusMinus_Sup_witness_cached(finite_aggregator, threshold);
    }

    // Wrapper for private flatten_SumPlusMinus_Inf
    static Automaton* flatten_SumPlusMinus_Inf(NestedAutomaton* nwa,
                                                value_function_t finite_aggregator,
                                                weight_t threshold) {
        return nwa->flatten_SumPlusMinus_Inf(finite_aggregator, threshold);
    }

    static Automaton* flatten_SumPlusMinus_Inf_cached(NestedAutomaton* nwa,
                                                      value_function_t finite_aggregator,
                                                      weight_t threshold) {
        return nwa->flatten_SumPlusMinus_Inf_cached(finite_aggregator, threshold);
    }

    // Wrapper for private flatten_MinMax_Sup
    static Automaton* flatten_MinMax_Sup(NestedAutomaton* nwa,
                                          value_function_t finite_aggregator,
                                          weight_t threshold) {
        return nwa->flatten_MinMax_Sup(finite_aggregator, threshold);
    }

    static Automaton* flatten_MinMax_Sup_cached(NestedAutomaton* nwa,
                                                value_function_t finite_aggregator,
                                                weight_t threshold) {
        return nwa->flatten_MinMax_Sup_cached(finite_aggregator, threshold);
    }

    static Automaton* flatten_MinMax_Sup_witness_cached(NestedAutomaton* nwa,
                                                        value_function_t finite_aggregator,
                                                        weight_t threshold) {
        return nwa->flatten_MinMax_Sup_witness_cached(finite_aggregator, threshold);
    }

    // Wrapper for private flatten_MinMax_Inf
    static Automaton* flatten_MinMax_Inf(NestedAutomaton* nwa,
                                          value_function_t finite_aggregator,
                                          weight_t threshold) {
        return nwa->flatten_MinMax_Inf(finite_aggregator, threshold);
    }

    static Automaton* flatten_MinMax_Inf_cached(NestedAutomaton* nwa,
                                                value_function_t finite_aggregator,
                                                weight_t threshold) {
        return nwa->flatten_MinMax_Inf_cached(finite_aggregator, threshold);
    }

    // Wrapper for private flatten_regular
    static Automaton* flatten_regular(NestedAutomaton* nwa,
                                       value_function_t finVal,
                                       weight_t bound = -1) {
        return nwa->flatten_regular(finVal, bound);
    }

    // Access to children
    static MapArray<ChildAutomaton*>* getChildren(NestedAutomaton* nwa) {
        return nwa->children_;
    }

    static void setMinMaxInfExperimentStatsEnabled(bool enabled) {
        NestedAutomaton::setMinMaxInfExperimentStatsEnabled(enabled);
    }

    static void resetMinMaxInfExperimentStats() {
        NestedAutomaton::resetMinMaxInfExperimentStats();
    }

    static MinMaxInfExperimentStats getMinMaxInfExperimentStats() {
        return NestedAutomaton::getMinMaxInfExperimentStats();
    }
};

// Timer utility
class Timer {
    std::chrono::high_resolution_clock::time_point start_time;
public:
    Timer() : start_time(std::chrono::high_resolution_clock::now()) {}

    double elapsed_ms() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end_time - start_time).count();
    }
};

// Test execution macro
#define RUN_TEST(test_func) do { \
    std::cout << "Running: " << #test_func << "... " << std::flush; \
    Timer timer; \
    TestResult result; \
    result.name = #test_func; \
    try { \
        test_func(); \
        result.passed = true; \
        result.message = "OK"; \
        std::cout << "PASSED"; \
    } catch (const std::exception& e) { \
        result.passed = false; \
        result.message = std::string("Exception: ") + e.what(); \
        std::cout << "FAILED: " << e.what(); \
    } catch (...) { \
        result.passed = false; \
        result.message = "Unknown exception"; \
        std::cout << "FAILED: Unknown exception"; \
    } \
    result.duration_ms = timer.elapsed_ms(); \
    std::cout << " (" << result.duration_ms << " ms)" << std::endl; \
    g_test_results.push_back(result); \
} while(0)

// Assertion macros
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + msg); \
    } \
} while(0)

#define TEST_ASSERT_NOT_NULL(ptr, msg) TEST_ASSERT((ptr) != nullptr, msg)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + msg + \
            " (expected " + std::to_string(b) + ", got " + std::to_string(a) + ")"); \
    } \
} while(0)

#define TEST_ASSERT_GT(a, b, msg) do { \
    if (!((a) > (b))) { \
        throw std::runtime_error(std::string("Assertion failed: ") + msg + \
            " (expected > " + std::to_string(b) + ", got " + std::to_string(a) + ")"); \
    } \
} while(0)

#define TEST_ASSERT_GE(a, b, msg) do { \
    if (!((a) >= (b))) { \
        throw std::runtime_error(std::string("Assertion failed: ") + msg + \
            " (expected >= " + std::to_string(b) + ", got " + std::to_string(a) + ")"); \
    } \
} while(0)

// Print test summary
inline void printTestSummary() {
    int passed = 0, failed = 0;
    double total_time = 0;

    for (const auto& r : g_test_results) {
        if (r.passed) passed++; else failed++;
        total_time += r.duration_ms;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "TEST SUMMARY" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << passed << "/" << g_test_results.size() << std::endl;
    std::cout << "Failed: " << failed << "/" << g_test_results.size() << std::endl;
    std::cout << "Total time: " << total_time << " ms" << std::endl;

    if (failed > 0) {
        std::cout << "\nFailed tests:" << std::endl;
        for (const auto& r : g_test_results) {
            if (!r.passed) {
                std::cout << "  - " << r.name << ": " << r.message << std::endl;
            }
        }
    }
    std::cout << "========================================" << std::endl;
}

// Utility to verify automaton validity
inline void verifyAutomatonBasics(const Automaton* A, const std::string& context) {
    TEST_ASSERT_NOT_NULL(A, context + ": automaton is null");
    TEST_ASSERT_NOT_NULL(A->getAlphabet(), context + ": alphabet is null");
    TEST_ASSERT_NOT_NULL(A->getStates(), context + ": states is null");
    TEST_ASSERT_NOT_NULL(A->getWeights(), context + ": weights is null");
    TEST_ASSERT_GT(A->getAlphabet()->size(), 0u, context + ": alphabet is empty");
    TEST_ASSERT_GT(A->getStates()->size(), 0u, context + ": states is empty");
}

// Utility to verify nested automaton validity
inline void verifyNestedAutomatonBasics(const NestedAutomaton* NA, const std::string& context) {
    verifyAutomatonBasics(NA, context);
    // Children can be empty for some automata, so we don't require children > 0
}

// Utility to print automaton stats
inline void printAutomatonStats(const Automaton* A, const std::string& label) {
    std::cout << "  " << label << ": "
              << A->getStates()->size() << " states, "
              << A->getNbTransitions() << " transitions, "
              << "weights [" << A->getMinDomain() << ", " << A->getMaxDomain() << "]"
              << std::endl;
}

// Utility to check if output automaton has only 0/1 weights
inline bool hasOnly01Weights(const Automaton* A) {
    for (unsigned int i = 0; i < A->getWeights()->size(); ++i) {
        weight_t w = A->getWeights()->at(i)->getValue();
        if (w != weight_t(0) && w != weight_t(1)) {
            return false;
        }
    }
    return true;
}

// Public monotone flatteners may still contain SILENT edges before silence removal.
inline bool hasOnly01OrSilentWeights(const Automaton* A) {
    for (unsigned int i = 0; i < A->getWeights()->size(); ++i) {
        weight_t w = A->getWeights()->at(i)->getValue();
        if (w != weight_t(0) && w != weight_t(1) && w != SILENT) {
            return false;
        }
    }
    return true;
}

// Sample file paths
namespace TestFiles {
    // Non-nested automata
    const std::string SAMPLE_A = "samples/A.txt";
    const std::string SAMPLE_B = "samples/B.txt";

    // Nested automata
    const std::string NESTED_1 = "samples/nested/nested1.txt";
    const std::string NESTED_SIJ = "samples/nested/nested_Sij.txt";
    const std::string NESTED_SIJ2 = "samples/nested/nested_Sij2.txt";
    const std::string NESTED_SIL1 = "samples/nested/nested_sil1.txt";
    const std::string TEST_EMPT_1 = "samples/nested/test_empt_1.txt";
    const std::string TEST_EMPT_2 = "samples/nested/test_empt_2.txt";
    const std::string TEST_EMPT_3 = "samples/nested/test_empt_3.txt";
    const std::string COMPUTE_RETURN_MIN = "samples/nested/compute_return_min.txt";
    const std::string COMPUTE_RETURN_MAX = "samples/nested/compute_return_max.txt";
    const std::string COMPUTE_RETURN_SUMB = "samples/nested/compute_return_sumB.txt";
    const std::string COMPLEX_SUMB = "samples/nested/complex_sumB.txt";
    const std::string NONDET_SUMB = "samples/nested/nondet_sumB.txt";
    const std::string SUP_MAX_CYCLE_TRUE = "samples/tests/sanity/tc11_sup_max_cycle_true.txt";
    const std::string SUP_MAX_DOOMED_FALSE = "samples/tests/sanity/tc12_sup_max_doomed_false.txt";
    const std::string MAX_MERGE_BUG_COMPLETE = "samples/tests/correctness/max_merge_bug_complete.txt";
}

#endif // TEST_COMMON_H_
