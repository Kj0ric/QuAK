// Reference file for the three Sup-Max nonemptiness implementations compared in:
//   results/minmax_sup_resource_small_n4_t120.csv
//
// This file is not part of the build. It collects the exact benchmark entry
// points in one place and points to the underlying source blocks that implement
// each backend.
//
// Compared backends:
//   1. `threshold_extremal`
//      - benchmark entry used here: `NestedAutomatonTester::flatten_MinMax_Sup`
//      - public wrapper: `src/NestedAutomaton.cpp:5928-5933`
//      - actual implementation core: `flatten_threshold_extremal_impl(...)`
//        in `src/NestedAutomaton.cpp:4615-4868`
//
//   2. `regular`
//      - benchmark entry used here: `NestedAutomatonTester::flatten_regular`
//      - implementation: `src/NestedAutomaton.cpp:1523-1654` and the remainder
//        of `NestedAutomaton::flatten_regular(...)`
//
//   3. `split_witness`
//      - benchmark entry used here: `NestedAutomatonTester::flatten_MinMax_Sup_split_witness`
//      - implementation: `src/NestedAutomaton_OLD_V2.cpp:5712-...`
//      - extracted standalone reference: `backend_split_sup_extract.cpp`
//
// The benchmark probe files that used these entries are:
//   - `src/tests/probes/minmax_sup_resource_probe.cpp`
//   - archived old-v2 probe code in
//     `src/archived/minmax_sup_threshold_extremal_split_witness_archive.cpp`

#include "sanity_tests/test_common.h"

static Automaton* compared_threshold_extremal(NestedAutomaton* nwa, weight_t threshold) {
    return NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, threshold);
}

static Automaton* compared_regular(NestedAutomaton* nwa, weight_t threshold) {
    return NestedAutomatonTester::flatten_regular(nwa, Max_f, threshold);
}

static Automaton* compared_split_witness(NestedAutomaton* nwa, weight_t threshold) {
    return NestedAutomatonTester::flatten_MinMax_Sup_split_witness(nwa, Max_f, threshold);
}

static bool eval_threshold_extremal_nonempty(NestedAutomaton* nwa, weight_t threshold) {
    Automaton* flat = compared_threshold_extremal(nwa, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, Sup, false);
    const bool result = non_silent->isNonEmpty_withFinal(Sup, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}

static bool eval_regular_nonempty(NestedAutomaton* nwa, weight_t threshold) {
    Automaton* flat = compared_regular(nwa, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, Sup, false);
    const bool result = non_silent->isNonEmpty_withFinal(Sup, threshold);
    delete non_silent;
    delete flat;
    return result;
}

static bool eval_split_witness_nonempty(NestedAutomaton* nwa, weight_t threshold) {
    Automaton* flat = compared_split_witness(nwa, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, Sup, false);
    const bool result = non_silent->isNonEmpty_withFinal(Sup, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}
