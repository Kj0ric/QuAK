#include <iomanip>
#include <iostream>
#include <chrono>
#include <string>
#include <vector>

#include "sanity_tests/test_common.h"

#ifndef SUM_SUP_USE_WITNESS_CACHED
#define SUM_SUP_USE_WITNESS_CACHED 0
#endif

#ifndef SUM_SUP_FULL_HARNESS
#define SUM_SUP_FULL_HARNESS 0
#endif

#ifndef SUM_SUP_RUN_REGULAR_ORACLE
#define SUM_SUP_RUN_REGULAR_ORACLE 1
#endif

struct CaseSpec {
    const char* label;
    const char* path;
    bool run_sumplus;
    bool run_summinus;
    bool trusted_sumplus_oracle;
};

struct EvalResult {
    bool result = false;
    unsigned int states = 0;
    unsigned int transitions = 0;
    double ms = 0.0;
};

static const char* inf_name(value_function_t infVal) {
    switch (infVal) {
        case Sup: return "Sup";
        case LimSup: return "LimSup";
        default: return "?";
    }
}

static const char* fin_name(value_function_t finVal) {
    switch (finVal) {
        case SumPlus: return "SumPlus";
        case SumMinus: return "SumMinus";
        default: return "?";
    }
}

static bool skip_known_stress_query(const CaseSpec& c,
                                    value_function_t infVal,
                                    value_function_t finVal,
                                    weight_t threshold) {
    (void)infVal;
#if SUM_SUP_FULL_HARNESS
    return finVal == SumPlus
        && std::string(c.label) == "split_witness_issue2_limsup_false_positive"
        && threshold >= weight_t(10);
#else
    (void)c;
    (void)finVal;
    (void)threshold;
    return false;
#endif
}

static Automaton* flatten_default(NestedAutomaton* nwa,
                                  value_function_t finVal,
                                  weight_t threshold) {
    return NestedAutomatonTester::flatten_SumPlusMinus_Sup(nwa, finVal, threshold);
}

static Automaton* flatten_selected(NestedAutomaton* nwa,
                                   value_function_t finVal,
                                   weight_t threshold) {
#if SUM_SUP_USE_WITNESS_CACHED
    return NestedAutomatonTester::flatten_SumPlusMinus_Sup_witness_cached(nwa, finVal, threshold);
#else
    return NestedAutomatonTester::flatten_SumPlusMinus_Sup(nwa, finVal, threshold);
#endif
}

static EvalResult evaluate_flat(NestedAutomaton* nwa,
                                value_function_t infVal,
                                value_function_t finVal,
                                weight_t threshold,
                                Automaton* (*flatten)(NestedAutomaton*, value_function_t, weight_t)) {
    const auto start = std::chrono::steady_clock::now();
    Automaton* flat = flatten(nwa, finVal, threshold);
    EvalResult out;
    out.states = flat->getStates()->size();
    out.transitions = flat->getNbTransitions();

    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    out.result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));

    delete non_silent;
    delete flat;
    const auto end = std::chrono::steady_clock::now();
    out.ms = std::chrono::duration<double, std::milli>(end - start).count();
    return out;
}

struct OracleResult {
    bool result = false;
    double ms = 0.0;
};

static OracleResult eval_sumplus_regular_oracle(NestedAutomaton* nwa,
                                                value_function_t infVal,
                                                weight_t threshold) {
    const auto start = std::chrono::steady_clock::now();

    const weight_t bound = threshold > weight_t(0) ? threshold : weight_t(0);
    Automaton* flat = NestedAutomatonTester::flatten_regular(nwa, SumB, bound);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);

    delete non_silent;
    delete flat;
    const auto end = std::chrono::steady_clock::now();
    return OracleResult{result, std::chrono::duration<double, std::milli>(end - start).count()};
}

int main() {
    std::cout << std::unitbuf;

#if SUM_SUP_USE_WITNESS_CACHED
    const char* selected_name = "witness_cached";
#else
    const char* selected_name = "default";
#endif

    const std::vector<CaseSpec> cases = {
        {"baseline_det", "samples/tests/correctness/baseline_det.txt", true, true, true},
        {"baseline_fractional", "samples/tests/correctness/baseline_fractional.txt", true, true, true},
        {"deep_nondet_binary", "samples/tests/correctness/deep_nondet_binary.txt", true, false, true},
        {"positive_only_nondet", "samples/tests/correctness/positive_only_nondet.txt", true, false, true},
        {"child_pump_loop", "samples/tests/correctness/child_pump_loop.txt", false, true, false},
        {"epsilon_boundary", "samples/tests/correctness/epsilon_boundary.txt", true, true, true},
        {"sup_initial_final_child", "samples/tests/correctness/sup_initial_final_child.txt", true, false, true},
        {"split_witness_issue5_phase_same_step_control",
         "samples/tests/correctness/split_witness_issue5_phase_same_step_control.txt", true, false, true},
        {"split_witness_issue5_phase_async_idle",
         "samples/tests/correctness/split_witness_issue5_phase_async_idle.txt", true, false, true},
        {"split_witness_issue5_phase_async_active_false_negative",
         "samples/tests/correctness/split_witness_issue5_phase_async_active_false_negative.txt", true, false, true},
        {"split_witness_issue2_limsup_false_positive",
         "samples/tests/correctness/split_witness_issue2_limsup_false_positive.txt", true, false, true},
        {"phase_parent_final_then_empty",
         "samples/tests/correctness/phase_parent_final_then_empty.txt", true, false, true},
        {"threshold_extremal_sumplus_wrong_final_positive",
         "samples/tests/correctness/threshold_extremal_sumplus_wrong_final_positive.txt", true, false, true},
        {"sum_sup_w1_discharge_on_reset",
         "samples/tests/correctness/sum_sup_w1_discharge_on_reset.txt", true, false, true},
        {"sum_sup_witness_immediate_discharge",
         "samples/tests/correctness/sum_sup_witness_immediate_discharge.txt", true, false, true},
        {"sum_sup_w2_discharge_and_respawn",
         "samples/tests/correctness/sum_sup_w2_discharge_and_respawn.txt", true, false, true},
        {"sum_sup_background_blocks_acceptance",
         "samples/tests/correctness/sum_sup_background_blocks_acceptance.txt", true, false, true},
        {"sum_sup_no_nonsilent_after_prefix",
         "samples/tests/correctness/sum_sup_no_nonsilent_after_prefix.txt", true, false, true},
        {"sum_sup_summinus_mixed_sign_abs_cost",
         "samples/tests/correctness/sum_sup_summinus_mixed_sign_abs_cost.txt", false, true, false},
        {"baseline_det_neg", "samples/tests/correctness/baseline_det_neg.txt", false, true, false},
        {"baseline_fractional_neg", "samples/tests/correctness/baseline_fractional_neg.txt", false, true, false},
        {"child_pump_loop_neg", "samples/tests/correctness/child_pump_loop_neg.txt", false, true, false},
        {"epsilon_boundary_neg", "samples/tests/correctness/epsilon_boundary_neg.txt", false, true, false},
        {"phase_parent_final_then_empty_summinus",
         "samples/tests/correctness/phase_parent_final_then_empty_summinus.txt", false, true, false},
        {"threshold_extremal_summinus_wrong_final_low_guess",
         "samples/tests/correctness/threshold_extremal_summinus_wrong_final_low_guess.txt", false, true, false},
    };

#if SUM_SUP_FULL_HARNESS
    const std::vector<weight_t> sumplus_thresholds = {
        weight_t(0), weight_t(0.5), weight_t(1), weight_t(1.5), weight_t(2),
        weight_t(3), weight_t(4), weight_t(5), weight_t(8), weight_t(10)
    };
    const std::vector<weight_t> summinus_thresholds = {
        weight_t(1), weight_t(0), weight_t(-0.5), weight_t(-1), weight_t(-1.5),
        weight_t(-2), weight_t(-3), weight_t(-5), weight_t(-8), weight_t(-10)
    };
#else
    const std::vector<weight_t> sumplus_thresholds = {
        weight_t(0), weight_t(0.5), weight_t(1), weight_t(2)
    };
    const std::vector<weight_t> summinus_thresholds = {
        weight_t(1), weight_t(0), weight_t(-1), weight_t(-3)
    };
#endif

    unsigned int expected_queries = 0;
    unsigned int expected_oracle_queries = 0;
    unsigned int expected_skipped_queries = 0;
    for (const CaseSpec& c : cases) {
        for (value_function_t finVal : {SumPlus, SumMinus}) {
            if (finVal == SumPlus && !c.run_sumplus) continue;
            if (finVal == SumMinus && !c.run_summinus) continue;

            const std::vector<weight_t>& thresholds =
                (finVal == SumPlus) ? sumplus_thresholds : summinus_thresholds;

            for (value_function_t infVal : {Sup, LimSup}) {
                for (weight_t threshold : thresholds) {
                    if (skip_known_stress_query(c, infVal, finVal, threshold)) {
                        ++expected_skipped_queries;
                        continue;
                    }
                    ++expected_queries;
#if SUM_SUP_RUN_REGULAR_ORACLE
                    if (finVal == SumPlus && c.trusted_sumplus_oracle) {
                        ++expected_oracle_queries;
                    }
#endif
                }
            }
        }
    }

    unsigned int queries = 0;
    unsigned int oracle_queries = 0;
    unsigned int skipped_queries = 0;
    unsigned int mismatches = 0;

    std::cout << "=== Sum Sup/LimSup Witness-Cached Compare ===\n";
    std::cout << "selected_backend=" << selected_name << "\n";
    std::cout << "full_harness=" << SUM_SUP_FULL_HARNESS << "\n";
    std::cout << "regular_oracle=" << SUM_SUP_RUN_REGULAR_ORACLE << "\n";
    std::cout << "expected_queries=" << expected_queries
              << " expected_oracle_queries=" << expected_oracle_queries
              << " expected_skipped_queries=" << expected_skipped_queries << "\n";

    for (const CaseSpec& c : cases) {
        for (value_function_t finVal : {SumPlus, SumMinus}) {
            if (finVal == SumPlus && !c.run_sumplus) continue;
            if (finVal == SumMinus && !c.run_summinus) continue;

            const std::vector<weight_t>& thresholds =
                (finVal == SumPlus) ? sumplus_thresholds : summinus_thresholds;

            for (value_function_t infVal : {Sup, LimSup}) {
                for (weight_t threshold : thresholds) {
                    if (skip_known_stress_query(c, infVal, finVal, threshold)) {
                        ++skipped_queries;
                        std::cout << "SKIP " << skipped_queries << "/"
                                  << expected_skipped_queries << " "
                                  << c.label << " "
                                  << inf_name(infVal) << " "
                                  << fin_name(finVal)
                                  << " threshold=" << std::fixed << std::setprecision(2)
                                  << threshold.to_float()
                                  << " reason=default_threshold_extremal_stress\n";
                        continue;
                    }

                    const unsigned int query_no = queries + 1u;
                    std::cout << "QUERY " << query_no << "/" << expected_queries
                              << " " << c.label
                              << " " << inf_name(infVal)
                              << " " << fin_name(finVal)
                              << " threshold=" << std::fixed << std::setprecision(2)
                              << threshold.to_float() << "\n";

                    NestedAutomaton nwa(c.path);
                    std::cout << "  default start\n";
                    const EvalResult default_result =
                        evaluate_flat(&nwa, infVal, finVal, threshold, flatten_default);
                    std::cout << "  default done result=" << default_result.result
                              << " states=" << default_result.states
                              << " transitions=" << default_result.transitions
                              << " ms=" << std::fixed << std::setprecision(3)
                              << default_result.ms << "\n";

                    EvalResult selected = default_result;
#if SUM_SUP_USE_WITNESS_CACHED
                    std::cout << "  " << selected_name << " start\n";
                    selected = evaluate_flat(&nwa, infVal, finVal, threshold, flatten_selected);
                    std::cout << "  " << selected_name << " done result=" << selected.result
                              << " states=" << selected.states
                              << " transitions=" << selected.transitions
                              << " ms=" << std::fixed << std::setprecision(3)
                              << selected.ms << "\n";
#else
                    std::cout << "  selected=default skipped_duplicate_run\n";
#endif
                    ++queries;

                    if (default_result.result != selected.result) {
                        ++mismatches;
                        std::cout << "MISMATCH "
                                  << c.label << " "
                                  << inf_name(infVal) << " "
                                  << fin_name(finVal)
                                  << " threshold=" << std::fixed << std::setprecision(2)
                                  << threshold.to_float()
                                  << " default=" << default_result.result
                                  << " " << selected_name << "=" << selected.result
                                  << " default_states=" << default_result.states
                                  << " selected_states=" << selected.states
                                  << " default_transitions=" << default_result.transitions
                                  << " selected_transitions=" << selected.transitions
                                  << "\n";
                    }

#if SUM_SUP_RUN_REGULAR_ORACLE
                    if (finVal == SumPlus && c.trusted_sumplus_oracle) {
                        ++oracle_queries;
                        NestedAutomaton oracle_nwa(c.path);
                        std::cout << "  oracle " << oracle_queries << "/"
                                  << expected_oracle_queries << " start\n";
                        const OracleResult oracle =
                            eval_sumplus_regular_oracle(&oracle_nwa, infVal, threshold);
                        std::cout << "  oracle done result=" << oracle.result
                                  << " ms=" << std::fixed << std::setprecision(3)
                                  << oracle.ms << "\n";
                        if (oracle.result != selected.result) {
                            ++mismatches;
                            std::cout << "ORACLE_MISMATCH "
                                      << c.label << " "
                                      << inf_name(infVal) << " "
                                      << fin_name(finVal)
                                      << " threshold=" << std::fixed << std::setprecision(2)
                                      << threshold.to_float()
                                      << " oracle=" << oracle.result
                                      << " " << selected_name << "=" << selected.result
                                      << " selected_states=" << selected.states
                                      << " selected_transitions=" << selected.transitions
                                      << "\n";
                        }
                    }
#endif
                }
            }
        }
    }

    std::cout << "queries=" << queries
              << " oracle_queries=" << oracle_queries
              << " skipped_queries=" << skipped_queries
              << " mismatches=" << mismatches << "\n";

    return mismatches == 0 ? 0 : 1;
}
