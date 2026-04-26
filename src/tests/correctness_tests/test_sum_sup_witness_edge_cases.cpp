/**
 * test_sum_sup_witness_edge_cases.cpp
 *
 * Targeted hand fixtures for Sup/LimSup x SumPlus witness-cached W1/W2
 * ordering.
 */

#include "test_correctness_common.h"

#include <sstream>
#include <stdexcept>
#include <vector>

using ThresholdFlattenFn = Automaton* (*)(NestedAutomaton*, value_function_t, weight_t);

static bool eval_threshold(NestedAutomaton* nwa,
                           ThresholdFlattenFn flatten,
                           value_function_t infVal,
                           value_function_t finVal,
                           weight_t threshold) {
    Automaton* flat = flatten(nwa, finVal, threshold);
    TEST_ASSERT_NOT_NULL(flat, "threshold flatten should produce an automaton");

    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}

static bool eval_regular_sumplus_oracle(NestedAutomaton* nwa,
                                        value_function_t infVal,
                                        weight_t threshold) {
    Automaton* flat = nwa->flatten_regular(SumB, threshold);
    TEST_ASSERT_NOT_NULL(flat, "regular oracle flatten should produce an automaton");

    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);
    delete non_silent;
    delete flat;
    return result;
}

static bool eval_public_nonempty(const std::string& path,
                                 value_function_t infVal,
                                 value_function_t finVal,
                                 weight_t threshold) {
    NestedAutomaton nwa(path);
    return nwa.isNonEmpty(infVal, finVal, threshold);
}

static unsigned int symbol_id(Automaton* flat, const std::string& symbol) {
    for (unsigned int i = 0; i < flat->getAlphabet()->size(); ++i) {
        if (flat->getAlphabet()->at(i)->getName() == symbol) {
            return i;
        }
    }
    throw std::runtime_error("symbol not found in flattened automaton: " + symbol);
}

static bool contains_state(const std::vector<State*>& states, State* state) {
    for (State* existing : states) {
        if (existing == state) {
            return true;
        }
    }
    return false;
}

static std::vector<State*> step_states(Automaton* flat,
                                       const std::vector<State*>& from,
                                       const std::string& symbol,
                                       const weight_t* required_weight) {
    const unsigned int sym = symbol_id(flat, symbol);
    std::vector<State*> next;

    for (State* state : from) {
        SetStd<Edge*>* succs = state->getSuccessors(sym);
        TEST_ASSERT_NOT_NULL(succs, "raw flat state should expose successor storage");
        for (Edge* edge : *succs) {
            if (required_weight && edge->getWeight()->getValue() != *required_weight) {
                continue;
            }
            State* to = edge->getTo();
            if (!contains_state(next, to)) {
                next.push_back(to);
            }
        }
    }

    return next;
}

static bool has_successor_weight(Automaton* flat,
                                 const std::vector<State*>& from,
                                 const std::string& symbol,
                                 weight_t required_weight) {
    const unsigned int sym = symbol_id(flat, symbol);
    for (State* state : from) {
        SetStd<Edge*>* succs = state->getSuccessors(sym);
        TEST_ASSERT_NOT_NULL(succs, "raw flat state should expose successor storage");
        for (Edge* edge : *succs) {
            if (edge->getWeight()->getValue() == required_weight) {
                return true;
            }
        }
    }
    return false;
}

static void assert_sumplus_fixture_accepts(const std::string& label,
                                           const std::string& path) {
    for (value_function_t infVal : {Sup, LimSup}) {
        NestedAutomaton witness_nwa(path);
        NestedAutomaton oracle_nwa(path);

        const bool witness = eval_threshold(
            &witness_nwa,
            &NestedAutomatonTester::flatten_SumPlusMinus_Sup_witness_cached,
            infVal,
            SumPlus,
            weight_t(1));
        const bool oracle = eval_regular_sumplus_oracle(&oracle_nwa, infVal, weight_t(1));

        std::stringstream ctx;
        ctx << label << "." << infValToString(infVal);
        TEST_ASSERT_TRUE(witness, ctx.str() + ": witness-cached backend should accept");
        TEST_ASSERT_EQ(witness, oracle,
                       ctx.str() + ": witness-cached backend should match regular SumB oracle");
    }
}

static void assert_sumplus_fixture_rejects(const std::string& label,
                                           const std::string& path,
                                           weight_t threshold,
                                           bool check_regular_oracle) {
    for (value_function_t infVal : {Sup, LimSup}) {
        NestedAutomaton witness_nwa(path);

        const bool witness = eval_threshold(
            &witness_nwa,
            &NestedAutomatonTester::flatten_SumPlusMinus_Sup_witness_cached,
            infVal,
            SumPlus,
            threshold);

        std::stringstream ctx;
        ctx << label << "." << infValToString(infVal);
        TEST_ASSERT_FALSE(witness, ctx.str() + ": witness-cached backend should reject");

        if (check_regular_oracle) {
            NestedAutomaton oracle_nwa(path);
            const bool oracle = eval_regular_sumplus_oracle(&oracle_nwa, infVal, threshold);
            TEST_ASSERT_EQ(witness, oracle,
                           ctx.str() + ": witness-cached backend should match regular SumB oracle");
        }
    }
}

static void assert_summinus_mixed_sign_result(weight_t threshold, bool expected) {
    for (value_function_t infVal : {Sup, LimSup}) {
        NestedAutomaton witness_nwa(CorrectnessTestFiles::SUM_SUP_SUMMINUS_MIXED_SIGN_ABS_COST);

        const bool witness = eval_threshold(
            &witness_nwa,
            &NestedAutomatonTester::flatten_SumPlusMinus_Sup_witness_cached,
            infVal,
            SumMinus,
            threshold);

        std::stringstream ctx;
        ctx << "sum_sup_summinus_mixed_sign_abs_cost."
            << infValToString(infVal)
            << ".threshold=" << threshold;
        TEST_ASSERT_EQ(witness, expected, ctx.str() + ": unexpected direct SumMinus decision");
    }
}

static void assert_direct_trivial_thresholds() {
    for (value_function_t infVal : {Sup, LimSup}) {
        NestedAutomaton sumplus_witness(CorrectnessTestFiles::SUM_SUP_WITNESS_IMMEDIATE_DISCHARGE);
        const bool witness_plus = eval_threshold(
            &sumplus_witness,
            &NestedAutomatonTester::flatten_SumPlusMinus_Sup_witness_cached,
            infVal,
            SumPlus,
            weight_t(0));

        std::stringstream plus_ctx;
        plus_ctx << "sum_sup_direct_trivial.SumPlus." << infValToString(infVal);
        TEST_ASSERT_TRUE(witness_plus, plus_ctx.str() + ": witness threshold 0 should accept");

        NestedAutomaton summinus_witness(CorrectnessTestFiles::SUM_SUP_SUMMINUS_MIXED_SIGN_ABS_COST);
        const bool witness_minus = eval_threshold(
            &summinus_witness,
            &NestedAutomatonTester::flatten_SumPlusMinus_Sup_witness_cached,
            infVal,
            SumMinus,
            weight_t(1));

        std::stringstream minus_ctx;
        minus_ctx << "sum_sup_direct_trivial.SumMinus." << infValToString(infVal);
        TEST_ASSERT_FALSE(witness_minus,
                          minus_ctx.str() + ": witness positive threshold should reject");
    }
}

static void assert_public_trivial_thresholds() {
    for (value_function_t infVal : {Sup, LimSup, LimSupAvg}) {
        std::stringstream ctx;
        ctx << "public_trivial." << infValToString(infVal);

        TEST_ASSERT_FALSE(
            eval_public_nonempty(CorrectnessTestFiles::SUM_SUP_BACKGROUND_BLOCKS_ACCEPTANCE,
                                 infVal, SumPlus, weight_t(0)),
            ctx.str() + ".background_blocker: SumPlus threshold 0 should reject");
        TEST_ASSERT_FALSE(
            eval_public_nonempty(CorrectnessTestFiles::SUM_SUP_NO_NONSILENT_AFTER_PREFIX,
                                 infVal, SumPlus, weight_t(0)),
            ctx.str() + ".no_nonsilent_suffix: SumPlus threshold 0 should reject");
        TEST_ASSERT_TRUE(
            eval_public_nonempty(CorrectnessTestFiles::SUM_SUP_WITNESS_IMMEDIATE_DISCHARGE,
                                 infVal, SumPlus, weight_t(0)),
            ctx.str() + ".immediate_discharge: SumPlus threshold 0 should accept");

        TEST_ASSERT_FALSE(
            eval_public_nonempty(CorrectnessTestFiles::SUM_SUP_BACKGROUND_BLOCKS_ACCEPTANCE,
                                 infVal, SumPlus, weight_t(-1)),
            ctx.str() + ".background_blocker: SumPlus negative threshold should reject");
        TEST_ASSERT_FALSE(
            eval_public_nonempty(CorrectnessTestFiles::SUM_SUP_NO_NONSILENT_AFTER_PREFIX,
                                 infVal, SumPlus, weight_t(-1)),
            ctx.str() + ".no_nonsilent_suffix: SumPlus negative threshold should reject");
        TEST_ASSERT_TRUE(
            eval_public_nonempty(CorrectnessTestFiles::SUM_SUP_WITNESS_IMMEDIATE_DISCHARGE,
                                 infVal, SumPlus, weight_t(-1)),
            ctx.str() + ".immediate_discharge: SumPlus negative threshold should accept");

        TEST_ASSERT_FALSE(
            eval_public_nonempty(CorrectnessTestFiles::SUM_SUP_SUMMINUS_MIXED_SIGN_ABS_COST,
                                 infVal, SumMinus, weight_t(1)),
            ctx.str() + ".summinus_positive: SumMinus positive threshold should reject");
    }
}

static void assert_initial_witness_edge_exists(const std::string& path,
                                               const std::string& symbol) {
    NestedAutomaton nwa(path);
    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Sup_witness_cached(
        &nwa,
        SumPlus,
        weight_t(1));

    std::vector<State*> frontier{flat->getInitial()};
    TEST_ASSERT_TRUE(has_successor_weight(flat, frontier, symbol, weight_t(1)),
                     "immediate witness discharge should emit an initial weight-1 edge");
    delete flat;
}

static void assert_w2_discharge_allows_respawn() {
    NestedAutomaton nwa(CorrectnessTestFiles::SUM_SUP_W2_DISCHARGE_AND_RESPAWN);
    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Sup_witness_cached(
        &nwa,
        SumPlus,
        weight_t(1));

    std::vector<State*> frontier{flat->getInitial()};
    const weight_t one(1);
    frontier = step_states(flat, frontier, "c", &one);
    TEST_ASSERT_TRUE(!frontier.empty(),
                     "prefix c should have a witness branch with weight 1");
    frontier = step_states(flat, frontier, "d", nullptr);
    TEST_ASSERT_TRUE(!frontier.empty(),
                     "prefix c d should reach an active-W2 frontier");
    TEST_ASSERT_TRUE(
        has_successor_weight(flat, frontier, "a", weight_t(1)),
        "active W2 discharge should permit same-symbol witness respawn");

    delete flat;
}

void test_w1_discharge_on_reset_keeps_epoch_nonempty() {
    assert_sumplus_fixture_accepts(
        "sum_sup_w1_discharge_on_reset",
        CorrectnessTestFiles::SUM_SUP_W1_DISCHARGE_ON_RESET);
}

void test_immediate_witness_discharge_leaves_no_stale_slot() {
    assert_sumplus_fixture_accepts(
        "sum_sup_witness_immediate_discharge",
        CorrectnessTestFiles::SUM_SUP_WITNESS_IMMEDIATE_DISCHARGE);
    assert_initial_witness_edge_exists(
        CorrectnessTestFiles::SUM_SUP_WITNESS_IMMEDIATE_DISCHARGE,
        "a");
}

void test_w2_discharge_allows_same_symbol_respawn() {
    assert_sumplus_fixture_accepts(
        "sum_sup_w2_discharge_and_respawn",
        CorrectnessTestFiles::SUM_SUP_W2_DISCHARGE_AND_RESPAWN);
    assert_w2_discharge_allows_respawn();
}

void test_background_child_blocks_acceptance() {
    assert_sumplus_fixture_rejects(
        "sum_sup_background_blocks_acceptance",
        CorrectnessTestFiles::SUM_SUP_BACKGROUND_BLOCKS_ACCEPTANCE,
        weight_t(1),
        true);
}

void test_no_nonsilent_after_prefix_rejects() {
    assert_sumplus_fixture_rejects(
        "sum_sup_no_nonsilent_after_prefix",
        CorrectnessTestFiles::SUM_SUP_NO_NONSILENT_AFTER_PREFIX,
        weight_t(1),
        true);
}

void test_summinus_mixed_sign_uses_absolute_cost() {
    assert_summinus_mixed_sign_result(weight_t(-3), true);
    assert_summinus_mixed_sign_result(weight_t(-2), false);
}

void test_direct_trivial_thresholds() {
    assert_direct_trivial_thresholds();
}

void test_public_trivial_thresholds_preserve_acceptance() {
    assert_public_trivial_thresholds();
}

int main() {
    std::cout << "Running Sum Sup/LimSup witness-cached W1/W2 edge cases..."
              << std::endl;

    RUN_TEST(test_w1_discharge_on_reset_keeps_epoch_nonempty);
    RUN_TEST(test_immediate_witness_discharge_leaves_no_stale_slot);
    RUN_TEST(test_w2_discharge_allows_same_symbol_respawn);
    RUN_TEST(test_background_child_blocks_acceptance);
    RUN_TEST(test_no_nonsilent_after_prefix_rejects);
    RUN_TEST(test_summinus_mixed_sign_uses_absolute_cost);
    RUN_TEST(test_direct_trivial_thresholds);
    RUN_TEST(test_public_trivial_thresholds_preserve_acceptance);

    printTestSummary();
    for (const TestResult& result : g_test_results) {
        if (!result.passed) {
            return 1;
        }
    }
    return 0;
}
