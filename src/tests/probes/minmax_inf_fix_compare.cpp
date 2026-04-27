#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "sanity_tests/test_common.h"

using FlattenFn = Automaton* (*)(NestedAutomaton*, value_function_t, weight_t);

struct BackendSpec {
    const char* name;
    FlattenFn flatten;
    bool binary_threshold;
};

struct CorrectnessCase {
    const char* label;
    const char* path;
    value_function_t fin;
    weight_t threshold;
    bool expected_inf;
    bool expected_liminf;
};

struct PerfCase {
    const char* label;
    const char* path;
    value_function_t fin;
    weight_t threshold;
    int iterations;
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
        case Max_f: return "Max_f";
        case Min_f: return "Min_f";
        default: return "?";
    }
}

static Automaton* flatten_default(NestedAutomaton* nwa, value_function_t fin, weight_t thr) {
    return NestedAutomatonTester::flatten_MinMax_Inf(nwa, fin, thr);
}

static Automaton* flatten_cached(NestedAutomaton* nwa, value_function_t fin, weight_t thr) {
    return NestedAutomatonTester::flatten_MinMax_Inf_cached(nwa, fin, thr);
}

static Automaton* flatten_regular_wrapper(NestedAutomaton* nwa, value_function_t fin, weight_t thr) {
    return NestedAutomatonTester::flatten_regular(nwa, fin, thr);
}

static bool evaluate_query(const BackendSpec& backend,
                           NestedAutomaton* nwa,
                           value_function_t infVal,
                           value_function_t finVal,
                           weight_t threshold) {
    Automaton* flat = backend.flatten(nwa, finVal, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    bool result = non_silent->isNonEmpty_withFinal(infVal, backend.binary_threshold ? weight_t(1) : threshold);
    delete non_silent;
    delete flat;
    return result;
}

static void run_correctness() {
    const std::vector<BackendSpec> backends = {
        {"default", flatten_default, true},
        {"cached", flatten_cached, true},
    };

    const std::vector<CorrectnessCase> cases = {
        {"overlap_max_bug", TestFiles::MAX_MERGE_BUG_COMPLETE.c_str(), Max_f, weight_t(1), false, false},
        {"test_empt_2_impossible", TestFiles::TEST_EMPT_2.c_str(), Max_f, weight_t(3), false, false},
        {"baseline_det_max_eq", "samples/tests/correctness/baseline_det.txt", Max_f, weight_t(5), true, true},
        {"baseline_det_max_gt", "samples/tests/correctness/baseline_det.txt", Max_f, weight_t(6), false, false},
        {"baseline_det_min_eq", "samples/tests/correctness/baseline_det.txt", Min_f, weight_t(3), true, true},
        {"baseline_det_min_gt", "samples/tests/correctness/baseline_det.txt", Min_f, weight_t(4), false, false},
        {"scc_chain_inf_vs_liminf", "samples/tests/correctness/scc_chain_binary.txt", Max_f, weight_t(4), false, true},
        {"deep_nondet_eq", "samples/tests/correctness/deep_nondet_binary.txt", Max_f, weight_t(8), true, true},
        {"deep_nondet_gt", "samples/tests/correctness/deep_nondet_binary.txt", Max_f, weight_t(9), false, false},
        {"nondet_child_binary_eq", "samples/tests/correctness/nondet_child_binary.txt", Max_f, weight_t(7), true, true},
        {"nondet_child_binary_gt", "samples/tests/correctness/nondet_child_binary.txt", Max_f, weight_t(8), false, false},
        {"phase_parent_final_then_empty_max",
         "samples/tests/correctness/phase_parent_final_then_empty.txt", Max_f, weight_t(1), true, true},
        {"phase_parent_final_then_empty_min",
         "samples/tests/correctness/phase_parent_final_then_empty.txt", Min_f, weight_t(1), true, true},
    };

    std::cout << "=== Correctness ===\n";
    for (const CorrectnessCase& c : cases) {
        NestedAutomaton nwa(c.path);
        std::cout << c.label << " [" << fin_name(c.fin) << ", threshold=" << c.threshold << "]"
                  << " expected=(" << c.expected_inf << "," << c.expected_liminf << ")\n";

        for (const BackendSpec& backend : backends) {
            const bool got_inf = evaluate_query(backend, &nwa, Inf, c.fin, c.threshold);
            const bool got_liminf = evaluate_query(backend, &nwa, LimInf, c.fin, c.threshold);
            const bool ok = (got_inf == c.expected_inf) && (got_liminf == c.expected_liminf);

            std::cout << "  " << std::setw(14) << std::left << backend.name
                      << " -> (" << got_inf << "," << got_liminf << ")"
                      << (ok ? "" : "  MISMATCH") << "\n";
        }
    }
}

static void run_performance() {
    const std::vector<BackendSpec> backends = {
        {"default", flatten_default, true},
        {"cached", flatten_cached, true},
    };

    const std::vector<PerfCase> cases = {
        {"overlap_max_bug", TestFiles::MAX_MERGE_BUG_COMPLETE.c_str(), Max_f, weight_t(1), 200},
        {"nested_sij2", TestFiles::NESTED_SIJ2.c_str(), Max_f, weight_t(5), 200},
        {"scc_chain_binary", "samples/tests/correctness/scc_chain_binary.txt", Max_f, weight_t(1), 100},
        {"nondet_child_binary", "samples/tests/correctness/nondet_child_binary.txt", Max_f, weight_t(1), 100},
        {"deep_nondet_binary", "samples/tests/correctness/deep_nondet_binary.txt", Max_f, weight_t(1), 50},
        {"baseline_det_min", "samples/tests/correctness/baseline_det.txt", Min_f, weight_t(3), 200},
    };

    std::cout << "\n=== Performance (flatten + Inf check) ===\n";
    for (const PerfCase& c : cases) {
        NestedAutomaton nwa(c.path);
        std::cout << c.label << " [" << fin_name(c.fin) << ", threshold=" << c.threshold
                  << ", iterations=" << c.iterations << "]\n";

        for (const BackendSpec& backend : backends) {
            double total_ms = 0.0;
            unsigned int states = 0;
            unsigned int transitions = 0;
            bool result = false;

            for (int i = 0; i < c.iterations; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                Automaton* flat = backend.flatten(&nwa, c.fin, c.threshold);
                if (i == 0) {
                    states = flat->getStates()->size();
                    transitions = flat->getNbTransitions();
                }
                Automaton* non_silent = Automaton::removeSilentTransitions(flat, Inf, false);
                result = non_silent->isNonEmpty_withFinal(Inf, weight_t(1));
                auto end = std::chrono::high_resolution_clock::now();
                total_ms += std::chrono::duration<double, std::milli>(end - start).count();
                delete non_silent;
                delete flat;
            }

            std::cout << "  " << std::setw(14) << std::left << backend.name
                      << " -> avg_ms=" << (total_ms / c.iterations)
                      << ", states=" << states
                      << ", transitions=" << transitions
                      << ", result=" << result << "\n";
        }
    }
}

int main() {
    run_correctness();
    run_performance();
    return 0;
}
