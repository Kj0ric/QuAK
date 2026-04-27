#ifndef TEST_CORRECTNESS_COMMON_H_
#define TEST_CORRECTNESS_COMMON_H_

#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <chrono>
#include <functional>
#include <cmath>
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

// Test helper class that provides access to private threshold-flattening methods
class NestedAutomatonTester {
public:
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

    static SetStd<weight_t> compute_child_return_values(NestedAutomaton* nwa,
                                                        size_t child_index,
                                                        value_function_t finVal,
                                                        weight_t bound) {
        return nwa->computeChildReturnValues(nwa->getChild(child_index), finVal, bound);
    }

    static NestedAutomaton* split_child_finals_for_testing(const NestedAutomaton* nwa) {
        MapArray<ChildAutomaton*>* split_children =
            new MapArray<ChildAutomaton*>(nwa->children_->size());

        for (unsigned int child_id = 0; child_id < nwa->children_->size(); ++child_id) {
            ChildAutomaton* child = nwa->children_->at(child_id);
            if (!child) {
                continue;
            }

            if (child_id == 0) {
                split_children->insert(child_id, new ChildAutomaton(*child));
                continue;
            }

            split_children->insert(child_id, split_child_finals(child));
        }

        return new NestedAutomaton(nwa, split_children);
    }

private:
    static ChildAutomaton* split_child_finals(const ChildAutomaton* child) {
        MapArray<Symbol*>* alphabet = new MapArray<Symbol*>(child->getAlphabet()->size());
        for (unsigned int i = 0; i < child->getAlphabet()->size(); ++i) {
            Symbol* symbol = new Symbol(child->getAlphabet()->at(i));
            alphabet->insert(symbol->getId(), symbol);
        }

        MapArray<Weight*>* weights = new MapArray<Weight*>(child->getWeights()->size());
        for (unsigned int i = 0; i < child->getWeights()->size(); ++i) {
            Weight* weight = new Weight(child->getWeights()->at(i));
            weights->insert(weight->getId(), weight);
        }

        unsigned int split_state_count = 0;
        for (unsigned int i = 0; i < child->getStates()->size(); ++i) {
            split_state_count += child->getStates()->at(i)->getFinal() ? 2u : 1u;
        }

        State::RESET();
        MapArray<State*>* states = new MapArray<State*>(split_state_count);
        std::vector<State*> normal_copy(child->getStates()->size(), nullptr);
        std::vector<State*> stop_copy(child->getStates()->size(), nullptr);
        std::vector<State*> cont_copy(child->getStates()->size(), nullptr);

        for (unsigned int i = 0; i < child->getStates()->size(); ++i) {
            State* old_state = child->getStates()->at(i);
            if (old_state->getFinal()) {
                State* stop = new State(old_state->getName() + "_stop",
                                        alphabet->size(),
                                        child->getMinDomain(),
                                        child->getMaxDomain());
                stop->setFinal(true);
                states->insert(stop->getId(), stop);
                stop_copy[i] = stop;

                State* cont = new State(old_state->getName() + "_cont",
                                        alphabet->size(),
                                        child->getMinDomain(),
                                        child->getMaxDomain());
                cont->setFinal(false);
                states->insert(cont->getId(), cont);
                cont_copy[i] = cont;
            } else {
                State* copy = new State(old_state->getName(),
                                        alphabet->size(),
                                        child->getMinDomain(),
                                        child->getMaxDomain());
                copy->setFinal(false);
                states->insert(copy->getId(), copy);
                normal_copy[i] = copy;
            }
        }

        State* initial = state_for_continuation(child->getInitial(), normal_copy, cont_copy);

        for (unsigned int i = 0; i < child->getStates()->size(); ++i) {
            State* old_from = child->getStates()->at(i);
            State* new_from = state_for_continuation(old_from, normal_copy, cont_copy);

            for (unsigned int symbol_id = 0; symbol_id < old_from->getAlphabet()->size(); ++symbol_id) {
                SetStd<Edge*>* successors = old_from->getSuccessors(symbol_id);
                for (Edge* old_edge : *successors) {
                    State* old_to = old_edge->getTo();
                    Weight* new_weight = weights->at(old_edge->getWeight()->getId());
                    Symbol* new_symbol = alphabet->at(old_edge->getSymbol()->getId());

                    if (old_to->getFinal()) {
                        add_edge(new_symbol, new_weight, new_from, stop_copy[old_to->getId()]);
                        add_edge(new_symbol, new_weight, new_from, cont_copy[old_to->getId()]);
                    } else {
                        add_edge(new_symbol, new_weight, new_from, normal_copy[old_to->getId()]);
                    }
                }
            }
        }

        return new ChildAutomaton(child->getName() + "_split_finals",
                                  alphabet,
                                  states,
                                  weights,
                                  child->getMinDomain(),
                                  child->getMaxDomain(),
                                  initial);
    }

    static State* state_for_continuation(State* old_state,
                                         const std::vector<State*>& normal_copy,
                                         const std::vector<State*>& cont_copy) {
        return old_state->getFinal()
            ? cont_copy[old_state->getId()]
            : normal_copy[old_state->getId()];
    }

    static void add_edge(Symbol* symbol, Weight* weight, State* from, State* to) {
        Edge* edge = new Edge(symbol, weight, from, to);
        from->addSuccessor(edge);
        to->addPredecessor(edge);
    }
};

// Value function name conversion utilities
inline std::string infValToString(value_function_t f) {
    switch (f) {
        case Inf: return "Inf";
        case Sup: return "Sup";
        case LimInf: return "LimInf";
        case LimSup: return "LimSup";
        case LimInfAvg: return "LimInfAvg";
        case LimSupAvg: return "LimSupAvg";
        default: return "Unknown";
    }
}

inline std::string finValToString(value_function_t f) {
    switch (f) {
        case Max_f: return "Max_f";
        case Min_f: return "Min_f";
        case SumB: return "SumB";
        case SumPlus: return "SumPlus";
        case SumMinus: return "SumMinus";
        default: return "Unknown";
    }
}

// Weight epsilon for floating-point comparisons
constexpr float WEIGHT_EPSILON_F = 10e-5f;

inline bool weightsEqual(weight_t a, weight_t b) {
    return std::fabs(a.to_float() - b.to_float()) < WEIGHT_EPSILON_F;
}

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

#define TEST_ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + msg + " (expected true, got false)"); \
    } \
} while(0)

#define TEST_ASSERT_FALSE(cond, msg) do { \
    if (cond) { \
        throw std::runtime_error(std::string("Assertion failed: ") + msg + " (expected false, got true)"); \
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

// Utility to verify nested automaton validity
inline void verifyNestedAutomatonBasics(const NestedAutomaton* NA, const std::string& context) {
    TEST_ASSERT_NOT_NULL(NA, context + ": automaton is null");
    TEST_ASSERT_NOT_NULL(NA->getAlphabet(), context + ": alphabet is null");
    TEST_ASSERT_NOT_NULL(NA->getStates(), context + ": states is null");
    TEST_ASSERT_NOT_NULL(NA->getWeights(), context + ": weights is null");
}

// Input file paths for correctness tests
namespace CorrectnessTestFiles {
    const std::string BASE_PATH = "samples/tests/correctness/";

    const std::string BASELINE_DET = BASE_PATH + "baseline_det.txt";
    const std::string BASELINE_FRACTIONAL = BASE_PATH + "baseline_fractional.txt";
    const std::string NONDET_CHILD_BINARY = BASE_PATH + "nondet_child_binary.txt";
    const std::string TWO_CHILDREN_BINARY = BASE_PATH + "two_children_binary.txt";
    const std::string SCC_CHAIN_BINARY = BASE_PATH + "scc_chain_binary.txt";
    const std::string DEEP_NONDET_BINARY = BASE_PATH + "deep_nondet_binary.txt";
    const std::string THREE_CHILDREN_VARIED = BASE_PATH + "three_children_varied.txt";
    const std::string EPSILON_BOUNDARY = BASE_PATH + "epsilon_boundary.txt";
    const std::string POSITIVE_ONLY_NONDET = BASE_PATH + "positive_only_nondet.txt";
    const std::string CHILD_PUMP_LOOP = BASE_PATH + "child_pump_loop.txt";
    const std::string SUP_INITIAL_FINAL_CHILD = BASE_PATH + "sup_initial_final_child.txt";
    const std::string SUP_INITIAL_FINAL_CHILD_MIN_BAD_CURRENT_SYMBOL =
        BASE_PATH + "sup_initial_final_child_min_bad_current_symbol.txt";
    const std::string SUP_BACKGROUND_OBLIGATION_BLOCKER = BASE_PATH + "sup_background_obligation_blocker.txt";
    const std::string SUP_BACKGROUND_COLLISION_FRESH_NOMOVE =
        BASE_PATH + "sup_background_collision_fresh_nomove.txt";
    const std::string MAX_MERGE_BUG_COMPLETE = BASE_PATH + "max_merge_bug_complete.txt";
    const std::string REGULAR_FINAL_CONTINUATION_MAX_POSITIVE =
        BASE_PATH + "regular_final_continuation_max_positive.txt";
    const std::string REGULAR_FINAL_CONTINUATION_MAX_NEGATIVE =
        BASE_PATH + "regular_final_continuation_max_negative.txt";
    const std::string REGULAR_FINAL_CONTINUATION_MAX_MULTIFINAL =
        BASE_PATH + "regular_final_continuation_max_multifinal.txt";
    const std::string REGULAR_FINAL_CONTINUATION_MIN_WEIGHTSET =
        BASE_PATH + "regular_final_continuation_min_weightset.txt";
    const std::string REGULAR_FINAL_CONTINUATION_SUMB_POSITIVE =
        BASE_PATH + "regular_final_continuation_sumb_positive.txt";
    const std::string THRESHOLD_EXTREMAL_MAX_WRONG_FINAL_POSITIVE =
        BASE_PATH + "threshold_extremal_max_wrong_final_positive.txt";
    const std::string THRESHOLD_EXTREMAL_MIN_WRONG_FINAL_LOW_GUESS =
        BASE_PATH + "threshold_extremal_min_wrong_final_low_guess.txt";
    const std::string THRESHOLD_EXTREMAL_SUMPLUS_WRONG_FINAL_POSITIVE =
        BASE_PATH + "threshold_extremal_sumplus_wrong_final_positive.txt";
    const std::string THRESHOLD_EXTREMAL_SUMMINUS_WRONG_FINAL_LOW_GUESS =
        BASE_PATH + "threshold_extremal_summinus_wrong_final_low_guess.txt";
    const std::string SUM_SUP_W1_DISCHARGE_ON_RESET =
        BASE_PATH + "sum_sup_w1_discharge_on_reset.txt";
    const std::string SUM_SUP_WITNESS_IMMEDIATE_DISCHARGE =
        BASE_PATH + "sum_sup_witness_immediate_discharge.txt";
    const std::string SUM_SUP_W2_DISCHARGE_AND_RESPAWN =
        BASE_PATH + "sum_sup_w2_discharge_and_respawn.txt";
    const std::string SUM_SUP_BACKGROUND_BLOCKS_ACCEPTANCE =
        BASE_PATH + "sum_sup_background_blocks_acceptance.txt";
    const std::string SUM_SUP_NO_NONSILENT_AFTER_PREFIX =
        BASE_PATH + "sum_sup_no_nonsilent_after_prefix.txt";
    const std::string SUM_SUP_SUMMINUS_MIXED_SIGN_ABS_COST =
        BASE_PATH + "sum_sup_summinus_mixed_sign_abs_cost.txt";

    // Negated automata for SumMinus tests (all child weights negated)
    const std::string BASELINE_DET_NEG = BASE_PATH + "baseline_det_neg.txt";
    const std::string BASELINE_FRACTIONAL_NEG = BASE_PATH + "baseline_fractional_neg.txt";
    const std::string NONDET_CHILD_BINARY_NEG = BASE_PATH + "nondet_child_binary_neg.txt";
    const std::string TWO_CHILDREN_BINARY_NEG = BASE_PATH + "two_children_binary_neg.txt";
    const std::string SCC_CHAIN_BINARY_NEG = BASE_PATH + "scc_chain_binary_neg.txt";
    const std::string DEEP_NONDET_BINARY_NEG = BASE_PATH + "deep_nondet_binary_neg.txt";
    const std::string THREE_CHILDREN_VARIED_NEG = BASE_PATH + "three_children_varied_neg.txt";
    const std::string EPSILON_BOUNDARY_NEG = BASE_PATH + "epsilon_boundary_neg.txt";
    const std::string POSITIVE_ONLY_NONDET_NEG = BASE_PATH + "positive_only_nondet_neg.txt";
    const std::string CHILD_PUMP_LOOP_NEG = BASE_PATH + "child_pump_loop_neg.txt";

    // LimAvg adversarial tests
    const std::string LIMAVG_SUMPLUS_DIAMOND = BASE_PATH + "limavg_adversarial_sumplus_diamond.txt";
    const std::string LIMAVG_SUMPLUS = BASE_PATH + "limavg_adversarial_sumplus.txt";
    const std::string LIMAVG_SUMPLUS_UNARY = BASE_PATH + "limavg_adversarial_sumplus_unary.txt";
    const std::string LIMAVG_SUMPLUS_UNBOUNDED = BASE_PATH + "limavg_adversarial_sumplus_unbounded.txt";
    const std::string LIMAVG_SUMMINUS_UNARY = BASE_PATH + "limavg_adversarial_summinus_unary.txt";
    const std::string LIMAVG_SUMMINUS_UNBOUNDED = BASE_PATH + "limavg_adversarial_summinus_unbounded.txt";
    const std::string LIMAVG_SUMMINUS_DIAMOND = BASE_PATH + "limavg_adversarial_summinus_diamond.txt";
    const std::string LIMAVG_SUMMINUS_FINAL_CONTINUATION_SIGNED =
        BASE_PATH + "limavg_summinus_final_continuation_signed.txt";
    const std::string LIMAVG_MAX = BASE_PATH + "limavg_adversarial_max.txt";
    const std::string LIMAVG_MIN = BASE_PATH + "limavg_adversarial_min.txt";
    const std::string LIMAVG_SUMB = BASE_PATH + "limavg_adversarial_sumb.txt";
}

// List of all infVal functions to test
inline const std::vector<value_function_t> INF_VAL_FUNCTIONS = {
    Inf, Sup, LimInf, LimSup, LimInfAvg, LimSupAvg
};

// List of all finVal functions to test
inline const std::vector<value_function_t> FIN_VAL_FUNCTIONS = {
    Max_f, Min_f, SumB, SumPlus, SumMinus
};

// Default SumB bound for tests
constexpr weight_t DEFAULT_SUMB_BOUND = 10;

// Expected value structure for a single automaton
struct ExpectedValues {
    std::string automaton_name;
    std::string file_path;

    // Child values for each finVal (indexed by finVal enum)
    // For non-deterministic children, we store BEST and WORST achievable values
    struct FinValResult {
        weight_t best_value;   // Maximum achievable child value (for NonEmpty)
        weight_t worst_value;  // Minimum achievable child value (for Universal)
    };

    // Expected results for each (infVal, finVal) combination
    // The key insight: for deterministic automata where all runs have same sequence,
    // NonEmpty threshold = Universal threshold = the constant value
    // For non-deterministic: NonEmpty uses best possible word, Universal uses worst possible

    // Values indexed by [finVal]
    FinValResult fin_val_results[5]; // Max_f, Min_f, SumB, SumPlus, SumMinus

    // For each infVal, the best and worst aggregated values
    // This depends on the parent structure and which child values are achievable
    struct InfFinValResult {
        weight_t nonempty_threshold;  // max X such that isNonEmpty(infVal, finVal, X) = true
        weight_t universal_threshold; // max X such that isUniversal(infVal, finVal, X) = true
    };

    // Results indexed by [infVal][finVal]
    InfFinValResult results[6][5];
};

// Helper to get finVal index
inline int finValIndex(value_function_t f) {
    switch (f) {
        case Max_f: return 0;
        case Min_f: return 1;
        case SumB: return 2;
        case SumPlus: return 3;
        case SumMinus: return 4;
        default: return -1;
    }
}

// Helper to get infVal index
inline int infValIndex(value_function_t f) {
    switch (f) {
        case Inf: return 0;
        case Sup: return 1;
        case LimInf: return 2;
        case LimSup: return 3;
        case LimInfAvg: return 4;
        case LimSupAvg: return 5;
        default: return -1;
    }
}

#endif // TEST_CORRECTNESS_COMMON_H_
