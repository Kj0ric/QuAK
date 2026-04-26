/**
 * test_backend_regression_cases.cpp
 *
 * Compact promoted cases from optional comparison probes. These keep semantic
 * coverage for selected threshold flattening regressions without preserving
 * every differential harness as a registered test.
 */

#include "test_correctness_common.h"

#include <sstream>
#include <vector>

using ThresholdFlattenFn = Automaton* (*)(NestedAutomaton*, value_function_t, weight_t);

namespace {

bool eval_threshold_backend(const std::string& path,
                            ThresholdFlattenFn flatten,
                            value_function_t infVal,
                            value_function_t finVal,
                            weight_t threshold) {
    NestedAutomaton nwa(path);
    Automaton* flat = flatten(&nwa, finVal, threshold);
    TEST_ASSERT_NOT_NULL(flat, "threshold backend should produce an automaton");

    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));

    delete non_silent;
    delete flat;
    return result;
}

bool eval_public_nonempty(const std::string& path,
                          value_function_t infVal,
                          value_function_t finVal,
                          weight_t threshold) {
    NestedAutomaton nwa(path);
    return nwa.isNonEmpty(infVal, finVal, threshold);
}

bool eval_regular_oracle(const std::string& path,
                         value_function_t infVal,
                         value_function_t finVal,
                         weight_t threshold) {
    NestedAutomaton nwa(path);
    Automaton* flat = nwa.flatten_regular(finVal, weight_t(-1));
    TEST_ASSERT_NOT_NULL(flat, "regular oracle should produce an automaton");

    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);

    delete non_silent;
    delete flat;
    return result;
}

bool eval_sumplus_regular_oracle(const std::string& path,
                                 value_function_t infVal,
                                 weight_t threshold) {
    NestedAutomaton nwa(path);
    const weight_t bound = threshold > weight_t(0) ? threshold : weight_t(0);
    Automaton* flat = nwa.flatten_regular(SumB, bound);
    TEST_ASSERT_NOT_NULL(flat, "SumPlus regular oracle should produce an automaton");

    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);

    delete non_silent;
    delete flat;
    return result;
}

void assert_matches(const std::string& context, bool actual, bool expected) {
    TEST_ASSERT_EQ(actual, expected, context);
}

} // namespace

void test_minmax_sup_promoted_probe_cases() {
    struct Case {
        const char* label;
        std::string path;
        value_function_t finVal;
        weight_t threshold;
        bool expected_sup;
        bool expected_limsup;
    };

    const std::vector<Case> cases = {
        {"background_blocker_max",
         CorrectnessTestFiles::SUP_BACKGROUND_OBLIGATION_BLOCKER,
         Max_f, weight_t(1), false, false},
        {"background_blocker_min",
         CorrectnessTestFiles::SUP_BACKGROUND_OBLIGATION_BLOCKER,
         Min_f, weight_t(1), false, false},
        {"background_collision_fresh_nomove",
         CorrectnessTestFiles::SUP_BACKGROUND_COLLISION_FRESH_NOMOVE,
         Max_f, weight_t(1), false, false},
        {"overlap_max_merge",
         CorrectnessTestFiles::MAX_MERGE_BUG_COMPLETE,
         Max_f, weight_t(3), false, false},
        {"overlap_min_merge",
         CorrectnessTestFiles::MAX_MERGE_BUG_COMPLETE,
         Min_f, weight_t(0.5), false, false},
        {"split_witness_issue2",
         CorrectnessTestFiles::BASE_PATH + "split_witness_issue2_limsup_false_positive.txt",
         Max_f, weight_t(1), true, false},
    };

    for (const Case& c : cases) {
        for (value_function_t infVal : {Sup, LimSup}) {
            const bool expected = (infVal == Sup) ? c.expected_sup : c.expected_limsup;
            const bool regular = eval_regular_oracle(c.path, infVal, c.finVal, c.threshold);
            const bool current = eval_threshold_backend(
                c.path, &NestedAutomatonTester::flatten_MinMax_Sup, infVal, c.finVal, c.threshold);
            const bool cached = eval_threshold_backend(
                c.path, &NestedAutomatonTester::flatten_MinMax_Sup_cached, infVal, c.finVal, c.threshold);
            const bool public_result = eval_public_nonempty(c.path, infVal, c.finVal, c.threshold);

            std::stringstream ctx;
            ctx << c.label << "." << infValToString(infVal)
                << "." << finValToString(c.finVal)
                << ".threshold=" << c.threshold;

            assert_matches(ctx.str() + ".regular", regular, expected);
            assert_matches(ctx.str() + ".current", current, regular);
            assert_matches(ctx.str() + ".cached", cached, regular);
            assert_matches(ctx.str() + ".public", public_result, regular);
        }
    }
}

void test_minmax_sup_regular_oracle_differentials() {
    const std::string basic_path = "samples/nested/test_empt_2.txt";
    const std::vector<weight_t> basic_thresholds = {weight_t(0), weight_t(1), weight_t(3)};

    for (value_function_t infVal : {Sup, LimSup}) {
        for (value_function_t finVal : {Max_f, Min_f}) {
            for (weight_t threshold : basic_thresholds) {
                const bool cached = eval_threshold_backend(
                    basic_path,
                    &NestedAutomatonTester::flatten_MinMax_Sup_cached,
                    infVal,
                    finVal,
                    threshold);
                const bool regular = eval_regular_oracle(basic_path, infVal, finVal, threshold);

                std::stringstream ctx;
                ctx << "basic_cached." << infValToString(infVal)
                    << "." << finValToString(finVal)
                    << ".threshold=" << threshold;
                assert_matches(ctx.str(), cached, regular);
            }
        }
    }

    for (value_function_t infVal : {Sup, LimSup}) {
        const bool cached = eval_threshold_backend(
            CorrectnessTestFiles::MAX_MERGE_BUG_COMPLETE,
            &NestedAutomatonTester::flatten_MinMax_Sup_cached,
            infVal,
            Max_f,
            weight_t(1));
        const bool regular = eval_regular_oracle(
            CorrectnessTestFiles::MAX_MERGE_BUG_COMPLETE,
            infVal,
            Max_f,
            weight_t(1));

        std::stringstream ctx;
        ctx << "overlap_cached." << infValToString(infVal)
            << ".Max_f.threshold=1";
        assert_matches(ctx.str(), cached, regular);
    }

    struct Scenario {
        const char* label;
        std::string path;
        value_function_t infVal;
        value_function_t finVal;
        weight_t threshold;
    };

    const std::vector<Scenario> scenarios = {
        {"background_obligation_blocker.max.thr1",
         CorrectnessTestFiles::SUP_BACKGROUND_OBLIGATION_BLOCKER,
         Sup, Max_f, weight_t(1)},
        {"background_obligation_blocker.limsup.max.thr1",
         CorrectnessTestFiles::SUP_BACKGROUND_OBLIGATION_BLOCKER,
         LimSup, Max_f, weight_t(1)},
        {"background_obligation_blocker.sup.min.thr1",
         CorrectnessTestFiles::SUP_BACKGROUND_OBLIGATION_BLOCKER,
         Sup, Min_f, weight_t(1)},
        {"background_obligation_blocker.limsup.min.thr1",
         CorrectnessTestFiles::SUP_BACKGROUND_OBLIGATION_BLOCKER,
         LimSup, Min_f, weight_t(1)},
        {"background_obligation_blocker.max.thr0_5",
         CorrectnessTestFiles::SUP_BACKGROUND_OBLIGATION_BLOCKER,
         Sup, Max_f, weight_t(0.5)},
        {"initial_final_child.sup.max.thr1",
         CorrectnessTestFiles::SUP_INITIAL_FINAL_CHILD,
         Sup, Max_f, weight_t(1)},
        {"initial_final_child.limsup.max.thr1",
         CorrectnessTestFiles::SUP_INITIAL_FINAL_CHILD,
         LimSup, Max_f, weight_t(1)},
        {"initial_final_child_bad_symbol.sup.min.thr1",
         CorrectnessTestFiles::SUP_INITIAL_FINAL_CHILD_MIN_BAD_CURRENT_SYMBOL,
         Sup, Min_f, weight_t(1)},
        {"initial_final_child_bad_symbol.limsup.min.thr1",
         CorrectnessTestFiles::SUP_INITIAL_FINAL_CHILD_MIN_BAD_CURRENT_SYMBOL,
         LimSup, Min_f, weight_t(1)},
        {"max_merge_bug_complete.sup.max.thr1",
         CorrectnessTestFiles::MAX_MERGE_BUG_COMPLETE,
         Sup, Max_f, weight_t(1)},
        {"max_merge_bug_complete.limsup.max.thr1",
         CorrectnessTestFiles::MAX_MERGE_BUG_COMPLETE,
         LimSup, Max_f, weight_t(1)},
        {"max_merge_bug_complete.sup.min.thr0",
         CorrectnessTestFiles::MAX_MERGE_BUG_COMPLETE,
         Sup, Min_f, weight_t(0)},
        {"max_merge_bug_complete.limsup.min.thr0",
         CorrectnessTestFiles::MAX_MERGE_BUG_COMPLETE,
         LimSup, Min_f, weight_t(0)},
        {"phase_same_step_control.sup.max.thr1",
         CorrectnessTestFiles::BASE_PATH + "split_witness_issue5_phase_same_step_control.txt",
         Sup, Max_f, weight_t(1)},
        {"phase_same_step_control.limsup.max.thr1",
         CorrectnessTestFiles::BASE_PATH + "split_witness_issue5_phase_same_step_control.txt",
         LimSup, Max_f, weight_t(1)},
        {"phase_async_active.sup.max.thr1",
         CorrectnessTestFiles::BASE_PATH + "split_witness_issue5_phase_async_active_false_negative.txt",
         Sup, Max_f, weight_t(1)},
        {"phase_async_active.limsup.max.thr1",
         CorrectnessTestFiles::BASE_PATH + "split_witness_issue5_phase_async_active_false_negative.txt",
         LimSup, Max_f, weight_t(1)},
        {"nonterminating_background.sup.max.thr1",
         CorrectnessTestFiles::BASE_PATH + "complete_nonterminating_background.txt",
         Sup, Max_f, weight_t(1)},
        {"nonterminating_background.limsup.max.thr1",
         CorrectnessTestFiles::BASE_PATH + "complete_nonterminating_background.txt",
         LimSup, Max_f, weight_t(1)},
    };

    for (const Scenario& scenario : scenarios) {
        const bool witness = eval_threshold_backend(
            scenario.path,
            &NestedAutomatonTester::flatten_MinMax_Sup_witness_cached,
            scenario.infVal,
            scenario.finVal,
            scenario.threshold);
        const bool regular = eval_regular_oracle(
            scenario.path,
            scenario.infVal,
            scenario.finVal,
            scenario.threshold);

        std::stringstream ctx;
        ctx << scenario.label << "." << infValToString(scenario.infVal)
            << "." << finValToString(scenario.finVal)
            << ".threshold=" << scenario.threshold;
        assert_matches(ctx.str(), witness, regular);
    }
}

void test_sum_sup_witness_promoted_probe_cases() {
    struct Case {
        const char* label;
        std::string path;
    };

    const std::vector<Case> cases = {
        {"issue5_same_step_control",
         CorrectnessTestFiles::BASE_PATH + "split_witness_issue5_phase_same_step_control.txt"},
        {"issue5_async_idle",
         CorrectnessTestFiles::BASE_PATH + "split_witness_issue5_phase_async_idle.txt"},
        {"issue5_async_active_false_negative",
         CorrectnessTestFiles::BASE_PATH + "split_witness_issue5_phase_async_active_false_negative.txt"},
        {"issue2_limsup_false_positive",
         CorrectnessTestFiles::BASE_PATH + "split_witness_issue2_limsup_false_positive.txt"},
        {"phase_parent_final_then_empty",
         CorrectnessTestFiles::BASE_PATH + "phase_parent_final_then_empty.txt"},
    };

    for (const Case& c : cases) {
        for (value_function_t infVal : {Sup, LimSup}) {
            const weight_t threshold(1);
            const bool current = eval_threshold_backend(
                c.path, &NestedAutomatonTester::flatten_SumPlusMinus_Sup,
                infVal, SumPlus, threshold);
            const bool oracle = eval_sumplus_regular_oracle(c.path, infVal, threshold);
            const bool public_result = eval_public_nonempty(c.path, infVal, SumPlus, threshold);

            std::stringstream ctx;
            ctx << c.label << "." << infValToString(infVal)
                << ".SumPlus.threshold=" << threshold;

            assert_matches(ctx.str() + ".oracle", oracle, current);
            assert_matches(ctx.str() + ".public", public_result, oracle);
        }
    }
}

void test_minmax_inf_cached_promoted_probe_cases() {
    struct Case {
        const char* label;
        std::string path;
        value_function_t finVal;
        weight_t threshold;
        bool expected_inf;
        bool expected_liminf;
    };

    const std::vector<Case> cases = {
        {"overlap_max_bug",
         CorrectnessTestFiles::MAX_MERGE_BUG_COMPLETE,
         Max_f, weight_t(1), false, false},
        {"baseline_det_max_eq",
         CorrectnessTestFiles::BASELINE_DET,
         Max_f, weight_t(5), true, true},
        {"baseline_det_max_gt",
         CorrectnessTestFiles::BASELINE_DET,
         Max_f, weight_t(6), false, false},
        {"scc_chain_inf_vs_liminf",
         CorrectnessTestFiles::SCC_CHAIN_BINARY,
         Max_f, weight_t(4), false, true},
        {"phase_parent_final_then_empty_max",
         CorrectnessTestFiles::BASE_PATH + "phase_parent_final_then_empty.txt",
         Max_f, weight_t(1), true, true},
        {"phase_parent_final_then_empty_min",
         CorrectnessTestFiles::BASE_PATH + "phase_parent_final_then_empty.txt",
         Min_f, weight_t(1), true, true},
    };

    for (const Case& c : cases) {
        for (value_function_t infVal : {Inf, LimInf}) {
            const bool expected = (infVal == Inf) ? c.expected_inf : c.expected_liminf;
            const bool current = eval_threshold_backend(
                c.path, &NestedAutomatonTester::flatten_MinMax_Inf,
                infVal, c.finVal, c.threshold);
            const bool public_result = eval_public_nonempty(c.path, infVal, c.finVal, c.threshold);

            std::stringstream ctx;
            ctx << c.label << "." << infValToString(infVal)
                << "." << finValToString(c.finVal)
                << ".threshold=" << c.threshold;

            assert_matches(ctx.str() + ".current", current, expected);
            assert_matches(ctx.str() + ".public", public_result, expected);
        }
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "CORRECTNESS TESTS: promoted backend regressions" << std::endl;
    std::cout << "========================================" << std::endl;

    RUN_TEST(test_minmax_sup_promoted_probe_cases);
    RUN_TEST(test_minmax_sup_regular_oracle_differentials);
    RUN_TEST(test_sum_sup_witness_promoted_probe_cases);
    RUN_TEST(test_minmax_inf_cached_promoted_probe_cases);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
