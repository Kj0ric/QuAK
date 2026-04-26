#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "NestedAutomaton.h"

struct CaseSpec {
    const char* label;
    const char* path;
};

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
        {"sup_initial_final_child", "src/tests/correctness_tests/inputs/sup_initial_final_child.txt"},
        {"sup_initial_final_child_min_bad_current_symbol",
         "src/tests/correctness_tests/inputs/sup_initial_final_child_min_bad_current_symbol.txt"},
        {"sup_background_obligation_blocker",
         "src/tests/correctness_tests/inputs/sup_background_obligation_blocker.txt"},
        {"sup_background_collision_fresh_nomove",
         "src/tests/correctness_tests/inputs/sup_background_collision_fresh_nomove.txt"},
        {"complete_nonterminating_background",
         "src/tests/correctness_tests/inputs/complete_nonterminating_background.txt"},
        {"max_merge_bug_complete", "src/tests/correctness_tests/inputs/max_merge_bug_complete.txt"},
        {"phase_parent_final_then_empty",
         "src/tests/correctness_tests/inputs/phase_parent_final_then_empty.txt"},
        {"split_witness_issue2_limsup_false_positive",
         "src/tests/correctness_tests/inputs/split_witness_issue2_limsup_false_positive.txt"},
        {"split_witness_issue5_phase_same_step_control",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_same_step_control.txt"},
        {"split_witness_issue5_phase_async_idle",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_idle.txt"},
        {"split_witness_issue5_phase_async_active_false_negative",
         "src/tests/correctness_tests/inputs/split_witness_issue5_phase_async_active_false_negative.txt"},
        {"baseline_det_neg", "src/tests/correctness_tests/inputs/baseline_det_neg.txt"},
        {"baseline_fractional_neg", "src/tests/correctness_tests/inputs/baseline_fractional_neg.txt"},
        {"nondet_child_binary_neg", "src/tests/correctness_tests/inputs/nondet_child_binary_neg.txt"},
        {"two_children_binary_neg", "src/tests/correctness_tests/inputs/two_children_binary_neg.txt"},
        {"scc_chain_binary_neg", "src/tests/correctness_tests/inputs/scc_chain_binary_neg.txt"},
        {"deep_nondet_binary_neg", "src/tests/correctness_tests/inputs/deep_nondet_binary_neg.txt"},
        {"three_children_varied_neg", "src/tests/correctness_tests/inputs/three_children_varied_neg.txt"},
        {"epsilon_boundary_neg", "src/tests/correctness_tests/inputs/epsilon_boundary_neg.txt"},
        {"positive_only_nondet_neg", "src/tests/correctness_tests/inputs/positive_only_nondet_neg.txt"},
        {"child_pump_loop_neg", "src/tests/correctness_tests/inputs/child_pump_loop_neg.txt"},
    };

    const std::vector<weight_t> thresholds = {
        weight_t(-1.0f), weight_t(0.0f), weight_t(0.5f), weight_t(1.0f),
        weight_t(1.5f), weight_t(2.0f), weight_t(3.0f), weight_t(4.0f),
        weight_t(5.0f), weight_t(6.0f), weight_t(8.0f), weight_t(10.0f)
    };

    unsigned int queries = 0;

    for (const CaseSpec& c : cases) {
        NestedAutomaton nwa(c.path);
        for (weight_t threshold : thresholds) {
            ++queries;
            const bool result = nwa.isNonEmpty(Sup, Max_f, threshold);
            std::cout << c.label
                      << ",Sup,Max_f,"
                      << std::fixed << std::setprecision(2) << threshold.to_float()
                      << ","
                      << (result ? 1 : 0)
                      << "\n";
        }
    }

    std::cout << "queries=" << queries << "\n";
    return 0;
}
