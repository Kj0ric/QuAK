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

struct CorrectnessCase {
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
                                 NestedAutomaton* nwa,
                                 value_function_t infVal,
                                 value_function_t finVal,
                                 weight_t threshold) {
    Automaton* flat = backend.flatten(nwa, finVal, threshold);
    EvalResult out;
    out.states = flat->getStates()->size();
    out.transitions = flat->getNbTransitions();
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    out.result =
        non_silent->isNonEmpty_withFinal(infVal, backend.binary_threshold ? weight_t(1) : threshold);
    delete non_silent;
    delete flat;
    return out;
}

int main() {
    const BackendSpec default_backend{"default", flatten_default, true};
    const BackendSpec cached_backend{"cached", flatten_cached, true};

    const std::vector<CorrectnessCase> cases = {
        {"baseline_det_sumplus_eq", "samples/tests/correctness/baseline_det.txt", SumPlus, weight_t(5)},
        {"baseline_det_sumplus_gt", "samples/tests/correctness/baseline_det.txt", SumPlus, weight_t(6)},
        {"baseline_fractional_sumplus", "samples/tests/correctness/baseline_fractional.txt", SumPlus, weight_t(2.5)},
        {"baseline_fractional_summinus", "samples/tests/correctness/baseline_fractional.txt", SumMinus, weight_t(-2.5)},
        {"deep_nondet_sumplus_eq", "samples/tests/correctness/deep_nondet_binary.txt", SumPlus, weight_t(8)},
        {"deep_nondet_sumplus_gt", "samples/tests/correctness/deep_nondet_binary.txt", SumPlus, weight_t(9)},
        {"positive_only_sumplus", "samples/tests/correctness/positive_only_nondet.txt", SumPlus, weight_t(4)},
        {"child_pump_loop_summinus", "samples/tests/correctness/child_pump_loop.txt", SumMinus, weight_t(-1)},
        {"phase_parent_final_then_empty_sumplus",
         "samples/tests/correctness/phase_parent_final_then_empty.txt", SumPlus, weight_t(1)},
        {"phase_parent_final_then_empty_summinus",
         "samples/tests/correctness/phase_parent_final_then_empty_summinus.txt", SumMinus, weight_t(-3)},
        {"epsilon_boundary_sumplus", "samples/tests/correctness/epsilon_boundary.txt", SumPlus, weight_t(0.5)},
        {"epsilon_boundary_summinus", "samples/tests/correctness/epsilon_boundary.txt", SumMinus, weight_t(-0.5)},
    };

    const value_function_t inf_modes[] = {Inf, LimInf};

    std::cout << "=== Sum Inf/LimInf Cached Correctness ===\n";
    std::cout << "Oracle backend: default\n";

    bool ok = true;
    for (const CorrectnessCase& c : cases) {
        NestedAutomaton nwa(c.path);
        std::cout << c.label << " [" << fin_name(c.fin) << ", threshold=" << c.threshold << "]\n";

        for (value_function_t infVal : inf_modes) {
            const EvalResult oracle = evaluate_query(default_backend, &nwa, infVal, c.fin, c.threshold);
            std::cout << "  " << inf_name(infVal)
                      << " default -> result=" << oracle.result
                      << ", states=" << oracle.states
                      << ", transitions=" << oracle.transitions
                      << "\n";

            const EvalResult got = evaluate_query(cached_backend, &nwa, infVal, c.fin, c.threshold);
            const bool match =
                got.result == oracle.result &&
                got.states == oracle.states &&
                got.transitions == oracle.transitions;
            ok = ok && match;

            std::cout << "    " << std::setw(10) << std::left << cached_backend.name
                      << " -> result=" << got.result
                      << ", states=" << got.states
                      << ", transitions=" << got.transitions
                      << (match ? "" : "  MISMATCH")
                      << "\n";
        }
    }

    return ok ? 0 : 1;
}
