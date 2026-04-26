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

static bool eval_specialized_flat(NestedAutomaton* nwa,
                                  value_function_t infVal,
                                  weight_t threshold) {
    if (threshold <= weight_t(0)) {
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
    if (threshold <= weight_t(0)) {
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
        {"sup_initial_final_child",
         "src/tests/correctness_tests/inputs/sup_initial_final_child.txt"},
        {"sup_initial_final_child_min_bad_current_symbol",
         "src/tests/correctness_tests/inputs/sup_initial_final_child_min_bad_current_symbol.txt"},
        {"sup_background_obligation_blocker",
         "src/tests/correctness_tests/inputs/sup_background_obligation_blocker.txt"},
        {"sup_background_collision_fresh_nomove",
         "src/tests/correctness_tests/inputs/sup_background_collision_fresh_nomove.txt"},
        {"max_merge_bug_complete",
         "src/tests/correctness_tests/inputs/max_merge_bug_complete.txt"},
        {"split_witness_issue2_limsup_false_positive",
         "src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt"},
        {"split_witness_issue5_phase_same_step_control",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt"},
        {"split_witness_issue5_phase_async_idle",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_idle.txt"},
        {"split_witness_issue5_phase_async_active_false_negative",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt"},
    };

    const std::vector<weight_t> thresholds = {
        weight_t(-1.0f), weight_t(0.0f), weight_t(0.5f), weight_t(1.0f),
        weight_t(2.0f), weight_t(2.5f), weight_t(3.0f), weight_t(5.0f)
    };

    unsigned int mismatches = 0;

    for (const CaseSpec& c : cases) {
        NestedAutomaton nwa(c.path);
        for (value_function_t infVal : {Sup, LimSup}) {
            for (weight_t threshold : thresholds) {
                const bool specialized = eval_specialized_flat(&nwa, infVal, threshold);
                const bool end_to_end = nwa.isNonEmpty(infVal, SumPlus, threshold);
                const bool oracle = eval_regular_oracle(&nwa, infVal, threshold);

                if (specialized != end_to_end || specialized != oracle) {
                    ++mismatches;
                    std::cout << c.label
                              << ","
                              << inf_name(infVal)
                              << ","
                              << std::fixed << std::setprecision(2) << threshold.to_float()
                              << ",oracle=" << (oracle ? 1 : 0)
                              << ",specialized=" << (specialized ? 1 : 0)
                              << ",isNonEmpty=" << (end_to_end ? 1 : 0)
                              << "\n";
                }
            }
        }
    }

    std::cout << "mismatches=" << mismatches << "\n";
    return 0;
}
