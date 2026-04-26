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

static bool eval_specialized_flat(NestedAutomaton* nwa,
                                  value_function_t infVal,
                                  weight_t threshold) {
    if (threshold <= weight_t(0)) return true;

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
    if (threshold <= weight_t(0)) return true;

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

struct Query {
    const char* label;
    const char* path;
    value_function_t infVal;
    weight_t threshold;
};

static const char* inf_name(value_function_t infVal) {
    return infVal == Sup ? "Sup" : "LimSup";
}

int main() {
    const std::vector<Query> queries = {
        {"sup_initial_final_child",
         "src/tests/correctness_tests/inputs/sup_initial_final_child.txt",
         Sup, weight_t(0.5f)},
        {"sup_initial_final_child",
         "src/tests/correctness_tests/inputs/sup_initial_final_child.txt",
         Sup, weight_t(1.0f)},
        {"sup_initial_final_child",
         "src/tests/correctness_tests/inputs/sup_initial_final_child.txt",
         LimSup, weight_t(0.5f)},
        {"sup_initial_final_child",
         "src/tests/correctness_tests/inputs/sup_initial_final_child.txt",
         LimSup, weight_t(1.0f)},
        {"split_witness_issue2_limsup_false_positive",
         "src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt",
         LimSup, weight_t(0.5f)},
        {"split_witness_issue2_limsup_false_positive",
         "src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt",
         LimSup, weight_t(1.0f)},
        {"split_witness_issue5_phase_same_step_control",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt",
         Sup, weight_t(0.5f)},
        {"split_witness_issue5_phase_same_step_control",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt",
         Sup, weight_t(1.0f)},
        {"split_witness_issue5_phase_same_step_control",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt",
         LimSup, weight_t(0.5f)},
        {"split_witness_issue5_phase_same_step_control",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt",
         LimSup, weight_t(1.0f)},
        {"split_witness_issue5_phase_async_active_false_negative",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt",
         Sup, weight_t(0.5f)},
        {"split_witness_issue5_phase_async_active_false_negative",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt",
         Sup, weight_t(1.0f)},
        {"split_witness_issue5_phase_async_active_false_negative",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt",
         LimSup, weight_t(0.5f)},
        {"split_witness_issue5_phase_async_active_false_negative",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt",
         LimSup, weight_t(1.0f)},
    };

    for (const Query& q : queries) {
        NestedAutomaton nwa(q.path);
        const bool specialized = eval_specialized_flat(&nwa, q.infVal, q.threshold);
        const bool oracle = eval_regular_oracle(&nwa, q.infVal, q.threshold);
        const bool end_to_end = nwa.isNonEmpty(q.infVal, SumPlus, q.threshold);
        std::cout << q.label
                  << "," << inf_name(q.infVal)
                  << "," << q.threshold.to_float()
                  << ",oracle=" << (oracle ? 1 : 0)
                  << ",specialized=" << (specialized ? 1 : 0)
                  << ",isNonEmpty=" << (end_to_end ? 1 : 0)
                  << "\n";
    }
    return 0;
}
