#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "sanity_tests/test_common.h"

#ifndef MINMAX_SUP_USE_CACHED_MMTHR
#define MINMAX_SUP_USE_CACHED_MMTHR 0
#endif

#ifndef MINMAX_SUP_USE_WITNESS_CACHED
#define MINMAX_SUP_USE_WITNESS_CACHED 0
#endif

#if MINMAX_SUP_USE_WITNESS_CACHED
#define MINMAX_SUP_BACKEND_MODE 3
#elif MINMAX_SUP_USE_CACHED_MMTHR
#define MINMAX_SUP_BACKEND_MODE 2
#else
#define MINMAX_SUP_BACKEND_MODE 0
#endif

struct CaseSpec {
    const char* label;
    const char* path;
    bool regular_oracle_trusted;
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
        case Max_f: return "Max_f";
        case Min_f: return "Min_f";
        default: return "?";
    }
}

static bool eval_specialized_flat(NestedAutomaton* nwa,
                                  value_function_t infVal,
                                  value_function_t finVal,
                                  weight_t threshold) {
#if MINMAX_SUP_BACKEND_MODE == 3
    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup_witness_cached(nwa, finVal, threshold);
#elif MINMAX_SUP_BACKEND_MODE == 2
    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup_cached(nwa, finVal, threshold);
#else
    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(nwa, finVal, threshold);
#endif
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}

static bool eval_regular_oracle(NestedAutomaton* nwa,
                                value_function_t infVal,
                                value_function_t finVal,
                                weight_t threshold) {
    Automaton* flat = NestedAutomatonTester::flatten_regular(nwa, finVal);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);
    delete non_silent;
    delete flat;
    return result;
}

int main() {
    const std::vector<CaseSpec> cases = {
        {"baseline_det", "samples/tests/correctness/baseline_det.txt", true},
        {"baseline_fractional", "samples/tests/correctness/baseline_fractional.txt", true},
        {"nondet_child_binary", "samples/tests/correctness/nondet_child_binary.txt", true},
        {"two_children_binary", "samples/tests/correctness/two_children_binary.txt", true},
        {"scc_chain_binary", "samples/tests/correctness/scc_chain_binary.txt", true},
        {"deep_nondet_binary", "samples/tests/correctness/deep_nondet_binary.txt", true},
        {"three_children_varied", "samples/tests/correctness/three_children_varied.txt", true},
        {"epsilon_boundary", "samples/tests/correctness/epsilon_boundary.txt", true},
        {"positive_only_nondet", "samples/tests/correctness/positive_only_nondet.txt", true},
        {"child_pump_loop", "samples/tests/correctness/child_pump_loop.txt", true},
        {"sup_initial_final_child", "samples/tests/correctness/sup_initial_final_child.txt", true},
        {"sup_initial_final_child_min_bad_current_symbol",
         "samples/tests/correctness/sup_initial_final_child_min_bad_current_symbol.txt", true},
        {"sup_background_obligation_blocker",
         "samples/tests/correctness/sup_background_obligation_blocker.txt", true},
        {"sup_background_collision_fresh_nomove",
         "samples/tests/correctness/sup_background_collision_fresh_nomove.txt", true},
        {"complete_nonterminating_background",
         "samples/tests/correctness/complete_nonterminating_background.txt", true},
        {"max_merge_bug_complete", "samples/tests/correctness/max_merge_bug_complete.txt", true},
        {"phase_parent_final_then_empty",
         "samples/tests/correctness/phase_parent_final_then_empty.txt", true},
        {"split_witness_issue2_limsup_false_positive",
         "samples/tests/correctness/split_witness_issue2_limsup_false_positive.txt", true},
        {"baseline_det_neg", "samples/tests/correctness/baseline_det_neg.txt", true},
        {"baseline_fractional_neg", "samples/tests/correctness/baseline_fractional_neg.txt", true},
        {"nondet_child_binary_neg", "samples/tests/correctness/nondet_child_binary_neg.txt", true},
        {"two_children_binary_neg", "samples/tests/correctness/two_children_binary_neg.txt", true},
        {"scc_chain_binary_neg", "samples/tests/correctness/scc_chain_binary_neg.txt", true},
        {"deep_nondet_binary_neg", "samples/tests/correctness/deep_nondet_binary_neg.txt", true},
        {"three_children_varied_neg", "samples/tests/correctness/three_children_varied_neg.txt", true},
        {"epsilon_boundary_neg", "samples/tests/correctness/epsilon_boundary_neg.txt", true},
        {"positive_only_nondet_neg", "samples/tests/correctness/positive_only_nondet_neg.txt", true},
        {"child_pump_loop_neg", "samples/tests/correctness/child_pump_loop_neg.txt", true},
    };

    const std::vector<weight_t> thresholds = {
        weight_t(-1.0), weight_t(0.0), weight_t(0.5), weight_t(1.0), weight_t(1.5),
        weight_t(2.0), weight_t(3.0), weight_t(4.0), weight_t(5.0), weight_t(6.0),
        weight_t(8.0), weight_t(10.0)
    };

    unsigned int total_queries = 0;
    unsigned int oracle_queries = 0;
    unsigned int mismatches = 0;

    for (const CaseSpec& c : cases) {
        NestedAutomaton nwa(c.path);
        for (value_function_t infVal : {Sup, LimSup}) {
            for (value_function_t finVal : {Max_f, Min_f}) {
                for (weight_t threshold : thresholds) {
                    const bool flat_result = eval_specialized_flat(&nwa, infVal, finVal, threshold);
                    ++total_queries;
#if MINMAX_SUP_BACKEND_MODE == 0
                    const bool end_to_end = nwa.isNonEmpty(infVal, finVal, threshold);
                    if (flat_result != end_to_end) {
                        ++mismatches;
                        std::cout << "MISMATCH "
                                  << c.label << " "
                                  << inf_name(infVal) << " "
                                  << fin_name(finVal)
                                  << " threshold=" << std::fixed << std::setprecision(2)
                                  << threshold.to_float()
                                  << " flat=" << flat_result
                                  << " isNonEmpty=" << end_to_end << "\n";
                        continue;
                    }
#endif

                    if (!c.regular_oracle_trusted) {
                        continue;
                    }

                    ++oracle_queries;
                    const bool oracle = eval_regular_oracle(&nwa, infVal, finVal, threshold);
                    if (oracle != flat_result) {
                        ++mismatches;
                        std::cout << "MISMATCH "
                                  << c.label << " "
                                  << inf_name(infVal) << " "
                                  << fin_name(finVal)
                                  << " threshold=" << std::fixed << std::setprecision(2)
                                  << threshold.to_float()
                                  << " oracle=" << oracle
                                  << " flat=" << flat_result;
#if MINMAX_SUP_BACKEND_MODE == 0
                        std::cout << " isNonEmpty=" << end_to_end;
#endif
                        std::cout << "\n";
                    }
                }
            }
        }
    }

    std::cout << "queries=" << total_queries
              << " oracle_queries=" << oracle_queries
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
