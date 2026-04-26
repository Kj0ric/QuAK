#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "NestedAutomaton.h"

class NestedAutomatonTester {
public:
    static bool childWeightsNeedProjection(const NestedAutomaton* nwa, value_function_t finVal) {
        return nwa->childWeightsNeedProjection(finVal);
    }

    static NestedAutomaton* projectChildWeightsForAggregator(const NestedAutomaton* nwa,
                                                             value_function_t finVal) {
        return nwa->projectChildWeightsForAggregator(finVal);
    }

    static Automaton* flatten_regular(NestedAutomaton* nwa,
                                      value_function_t finVal,
                                      weight_t bound = -1) {
        return nwa->flatten_regular(finVal, bound);
    }

    static Automaton* flatten_SumPlusMinus_Sup(NestedAutomaton* nwa,
                                               value_function_t finVal,
                                               weight_t threshold) {
        return nwa->flatten_SumPlusMinus_Sup(finVal, threshold);
    }
};

struct CaseSpec {
    const char* label;
    const char* path;
};

static const char* inf_name(value_function_t infVal) {
    switch (infVal) {
        case Sup: return "Sup";
        case LimSup: return "LimSup";
        default: return "?";
    }
}

static bool is_sumplus_trivial(weight_t threshold) {
    return threshold <= weight_t(0);
}

static bool eval_specialized_flat(NestedAutomaton* nwa,
                                  value_function_t infVal,
                                  weight_t threshold) {
    if (is_sumplus_trivial(threshold)) {
        return true;
    }

    NestedAutomaton* projected = nullptr;
    NestedAutomaton* work = nwa;
    if (NestedAutomatonTester::childWeightsNeedProjection(nwa, SumPlus)) {
        projected = NestedAutomatonTester::projectChildWeightsForAggregator(nwa, SumPlus);
        work = projected;
    }

    Automaton* flat = NestedAutomatonTester::flatten_SumPlusMinus_Sup(work, SumPlus, threshold);
    const bool result = flat->isNonEmpty_withFinal(infVal, weight_t(1));
    delete flat;
    delete projected;
    return result;
}

static bool eval_regular_oracle(NestedAutomaton* nwa,
                                value_function_t infVal,
                                weight_t threshold) {
    if (is_sumplus_trivial(threshold)) {
        return true;
    }

    NestedAutomaton* projected = nullptr;
    NestedAutomaton* work = nwa;
    if (NestedAutomatonTester::childWeightsNeedProjection(nwa, SumPlus)) {
        projected = NestedAutomatonTester::projectChildWeightsForAggregator(nwa, SumPlus);
        work = projected;
    }

    Automaton* flat = NestedAutomatonTester::flatten_regular(work, SumB, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);
    delete non_silent;
    delete flat;
    delete projected;
    return result;
}

int main() {
    const std::vector<CaseSpec> cases = {
        {"baseline_det", "src/tests/correctness_tests/inputs/baseline_det.txt"},
        {"baseline_fractional", "src/tests/correctness_tests/inputs/baseline_fractional.txt"},
        {"nondet_child_binary", "src/tests/correctness_tests/inputs/nondet_child_binary.txt"},
        {"two_children_binary", "src/tests/correctness_tests/inputs/two_children_binary.txt"},
        {"scc_chain_binary", "src/tests/correctness_tests/inputs/scc_chain_binary.txt"},
        {"deep_nondet_binary", "src/tests/correctness_tests/inputs/deep_nondet_binary.txt"},
        {"three_children_varied", "src/tests/correctness_tests/inputs/three_children_varied.txt"},
        {"epsilon_boundary", "src/tests/correctness_tests/inputs/epsilon_boundary.txt"},
        {"positive_only_nondet", "src/tests/correctness_tests/inputs/positive_only_nondet.txt"},
        {"child_pump_loop", "src/tests/correctness_tests/inputs/child_pump_loop.txt"},
        {"sup_background_obligation_blocker",
         "src/tests/correctness_tests/inputs/sup_background_obligation_blocker.txt"},
        {"sup_background_collision_fresh_nomove",
         "src/tests/correctness_tests/inputs/sup_background_collision_fresh_nomove.txt"},
        {"phase_parent_final_then_empty",
         "src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt"},
        {"split_witness_issue5_phase_async_active_false_negative",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt"},
    };

    const std::vector<weight_t> thresholds = {
        weight_t(-1.0f), weight_t(0.0f), weight_t(0.5f), weight_t(1.0f),
        weight_t(2.5f), weight_t(5.0f), weight_t(8.0f), weight_t(10.0f)
    };

    unsigned int total_queries = 0;
    unsigned int oracle_queries = 0;
    unsigned int mismatches = 0;

    for (const CaseSpec& c : cases) {
        NestedAutomaton nwa(c.path);
        const bool needs_projection =
            NestedAutomatonTester::childWeightsNeedProjection(&nwa, SumPlus);
        std::cout << "CASE " << c.label
                  << " projection=" << (needs_projection ? 1 : 0)
                  << "\n";

        for (value_function_t infVal : {Sup, LimSup}) {
            std::cout << "  INF " << inf_name(infVal) << "\n";
            for (weight_t threshold : thresholds) {
                ++total_queries;

                const bool specialized = eval_specialized_flat(&nwa, infVal, threshold);
                const bool end_to_end = nwa.isNonEmpty(infVal, SumPlus, threshold);

                if (specialized != end_to_end) {
                    ++mismatches;
                    std::cout << "MISMATCH specialized_vs_isNonEmpty "
                              << c.label
                              << " inf=" << inf_name(infVal)
                              << " threshold=" << std::fixed << std::setprecision(2)
                              << threshold.to_float()
                              << " projection=" << (needs_projection ? 1 : 0)
                              << " specialized=" << specialized
                              << " isNonEmpty=" << end_to_end
                              << "\n";
                }

                if (is_sumplus_trivial(threshold)) {
                    continue;
                }

                ++oracle_queries;
                const bool oracle = eval_regular_oracle(&nwa, infVal, threshold);
                if (specialized != oracle || end_to_end != oracle) {
                    ++mismatches;
                    std::cout << "MISMATCH oracle "
                              << c.label
                              << " inf=" << inf_name(infVal)
                              << " threshold=" << std::fixed << std::setprecision(2)
                              << threshold.to_float()
                              << " projection=" << (needs_projection ? 1 : 0)
                              << " oracle=" << oracle
                              << " specialized=" << specialized
                              << " isNonEmpty=" << end_to_end
                              << "\n";
                }
            }
        }
    }

    std::cout << "queries=" << total_queries
              << " oracle_queries=" << oracle_queries
              << " mismatches=" << mismatches
              << "\n";
    return mismatches == 0 ? 0 : 1;
}
