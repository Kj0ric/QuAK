#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "sanity_tests/test_common.h"

using FlattenFn = Automaton* (*)(NestedAutomaton*, value_function_t, weight_t);

struct BackendSpec {
    const char* name;
    FlattenFn flatten;
    bool binary_threshold;
};

struct EvalResult {
    bool result = false;
    unsigned int states = 0;
    unsigned int transitions = 0;
};

struct StressCase {
    const char* label;
    const char* path;
    value_function_t fin;
    weight_t threshold;
};

static const char* inf_name(value_function_t infVal) {
    switch (infVal) {
        case Inf: return "Inf";
        case LimInf: return "LimInf";
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

static Automaton* flatten_default(NestedAutomaton* nwa, value_function_t fin, weight_t thr) {
    return NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, fin, thr);
}

static Automaton* flatten_cached(NestedAutomaton* nwa, value_function_t fin, weight_t thr) {
    return NestedAutomatonTester::flatten_SumPlusMinus_Inf_cached(nwa, fin, thr);
}

static EvalResult evaluate_query(const BackendSpec& backend,
                                 const StressCase& c,
                                 value_function_t infVal,
                                 bool stats_enabled) {
    NestedAutomatonTester::setMinMaxInfExperimentStatsEnabled(stats_enabled);
    NestedAutomatonTester::resetMinMaxInfExperimentStats();

    NestedAutomaton nwa(c.path);
    Automaton* flat = backend.flatten(&nwa, c.fin, c.threshold);

    EvalResult out;
    out.states = flat->getStates()->size();
    out.transitions = flat->getNbTransitions();

    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    out.result =
        non_silent->isNonEmpty_withFinal(infVal, backend.binary_threshold ? weight_t(1) : c.threshold);

    delete non_silent;
    delete flat;

    NestedAutomatonTester::setMinMaxInfExperimentStatsEnabled(false);
    return out;
}

static void require_match(const StressCase& c,
                          value_function_t infVal,
                          int iteration,
                          const EvalResult& oracle,
                          const EvalResult& got) {
    if (oracle.result == got.result &&
        oracle.states == got.states &&
        oracle.transitions == got.transitions) {
        return;
    }

    std::ostringstream msg;
    msg << "Mismatch at iteration " << iteration
        << " for " << c.label
        << " [" << inf_name(infVal)
        << ", " << fin_name(c.fin)
        << ", threshold=" << c.threshold << "]"
        << ": default=(result=" << oracle.result
        << ", states=" << oracle.states
        << ", transitions=" << oracle.transitions
        << ") cached=(result=" << got.result
        << ", states=" << got.states
        << ", transitions=" << got.transitions
        << ")";
    throw std::runtime_error(msg.str());
}

static std::vector<StressCase> build_cases(const std::string& profile) {
    std::vector<StressCase> cases = {
        {"baseline_det_sumplus_eq", "samples/tests/correctness/baseline_det.txt", SumPlus, weight_t(5)},
        {"baseline_det_sumplus_gt", "samples/tests/correctness/baseline_det.txt", SumPlus, weight_t(6)},
        {"baseline_fractional_sumplus", "samples/tests/correctness/baseline_fractional.txt", SumPlus, weight_t(2.5)},
        {"baseline_fractional_summinus", "samples/tests/correctness/baseline_fractional.txt", SumMinus, weight_t(-2.5)},
        {"deep_nondet_sumplus_eq", "samples/tests/correctness/deep_nondet_binary.txt", SumPlus, weight_t(8)},
        {"deep_nondet_sumplus_gt", "samples/tests/correctness/deep_nondet_binary.txt", SumPlus, weight_t(9)},
        {"positive_only_sumplus", "samples/tests/correctness/positive_only_nondet.txt", SumPlus, weight_t(4)},
        {"child_pump_loop_summinus_eq", "samples/tests/correctness/child_pump_loop.txt", SumMinus, weight_t(-1)},
        {"child_pump_loop_summinus_heavy", "samples/tests/correctness/child_pump_loop.txt", SumMinus, weight_t(-20)},
        {"epsilon_boundary_sumplus", "samples/tests/correctness/epsilon_boundary.txt", SumPlus, weight_t(0.5)},
        {"epsilon_boundary_summinus", "samples/tests/correctness/epsilon_boundary.txt", SumMinus, weight_t(-0.5)},
        {"response_time_2_n5_k5", "samples/generated_response_time_2/response_n5_k5.txt", SumPlus, weight_t(5)},
    };

    if (profile == "extended") {
        cases.push_back({"response_time_1_n8_k8", "samples/generated_response_time_1/response_n8_k8.txt", SumPlus, weight_t(8)});
        cases.push_back({"response_time_3_n8_k8", "samples/generated_response_time_3/response_n8_k8.txt", SumPlus, weight_t(8)});
    } else if (profile != "core") {
        throw std::invalid_argument("Unsupported profile: " + profile + " (expected core or extended)");
    }

    return cases;
}

int main(int argc, char* argv[]) {
    int iterations = 50;
    std::string profile = "core";

    if (argc >= 2) iterations = std::stoi(argv[1]);
    if (argc >= 3) profile = argv[2];
    if (argc > 3) {
        std::cerr << "Usage: " << argv[0] << " [iterations] [core|extended]\n";
        return 1;
    }
    if (iterations <= 0) {
        std::cerr << "Iterations must be positive.\n";
        return 1;
    }

    const BackendSpec default_backend{"default", flatten_default, true};
    const BackendSpec cached_backend{"cached", flatten_cached, true};
    const value_function_t inf_modes[] = {Inf, LimInf};
    const std::vector<StressCase> cases = build_cases(profile);

    std::size_t checked = 0;
    std::cout << "=== Sum Cached Stability Stress ===\n";
    std::cout << "iterations=" << iterations
              << ", profile=" << profile
              << ", cases=" << cases.size()
              << ", inf_modes=2\n";

    for (int iter = 1; iter <= iterations; ++iter) {
        const bool reverse_order = (iter % 2 == 0);
        for (std::size_t case_idx = 0; case_idx < cases.size(); ++case_idx) {
            const StressCase& c = cases[case_idx];
            for (int inf_idx = 0; inf_idx < 2; ++inf_idx) {
                const value_function_t infVal = inf_modes[inf_idx];
                const bool stats_enabled = ((iter + static_cast<int>(case_idx) + inf_idx) % 3 == 0);

                EvalResult oracle;
                EvalResult got;

                if (!reverse_order) {
                    oracle = evaluate_query(default_backend, c, infVal, stats_enabled);
                    got = evaluate_query(cached_backend, c, infVal, stats_enabled);
                } else {
                    got = evaluate_query(cached_backend, c, infVal, stats_enabled);
                    oracle = evaluate_query(default_backend, c, infVal, stats_enabled);
                }

                require_match(c, infVal, iter, oracle, got);
                ++checked;
            }
        }

        if (iter == 1 || iter == iterations || iter % 10 == 0) {
            std::cout << "completed iteration " << std::setw(4) << iter
                      << " / " << iterations
                      << "  checks=" << checked
                      << "\n";
        }
    }

    std::cout << "PASS total_checks=" << checked << "\n";
    return 0;
}
