// Archived threshold-extremal entry point and split-witness probes.
//
// This file is intentionally not part of the active quak build. It keeps
// the old threshold-extremal entry point and split-witness probe programs
// together so selected scenarios can be recovered later as registered
// correctness tests. Snippets depend on historical helpers/APIs and are
// preserved for reference rather than standalone compilation.

// =============================================================================
// Archived flatten_threshold_extremal_impl entry point
// =============================================================================

static Automaton* flatten_threshold_extremal_impl(NestedAutomaton* A,
                                                  value_function_t finVal,
                                                  weight_t threshold) {
    const ThrExtMode mode = thrext_mode_from_fin(finVal);
    const bool is_sum_mode = (mode == ThrExtMode::SUMPLUS || mode == ThrExtMode::SUMMINUS);
    const thrext_int_t weight_scale = is_sum_mode ? compute_weight_scale(A) : 1u;

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    MapArray<Symbol*>* new_alphabet = nullptr;
    MapArray<Weight*>* new_weights = nullptr;

    const size_t k = A->getChildrenSize();
    std::vector<ChildTables> child_tab(k);
    std::vector<ThrExtChildInfo> child_info(k);

    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* c = A->getChild(i);
        if (!thrext_child_uses_tracking(c)) continue;
        build_child_tables(c, child_tab[i]);
        thrext_build_child_info(child_tab[i], finVal, threshold, weight_scale, child_info[i]);
    }

    const size_t alph_size = A->getAlphabetSize();
    new_alphabet = new MapArray<Symbol*>(alph_size);
    for (size_t i = 0; i < alph_size; ++i) {
        Symbol* original = A->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    MapStd<weight_t, Weight*> weight_register;
    new_weights = new MapArray<Weight*>(3);

    auto get_weight = [&](const weight_t& value) -> Weight* {
        if (!weight_register.contains(value)) {
            Weight* w = new Weight(value);
            new_weights->insert(w->getId(), w);
            weight_register.insert(value, w);
        }
        return weight_register.at(value);
    };

    get_weight(weight_t(0));
    get_weight(weight_t(1));
    get_weight(weight_t(SILENT));

    const weight_t global_min = weight_t(0);
    const weight_t global_max = weight_t(1);

    MapStd<ThrExtBuchiState, State*> state_map;
    std::deque<ThrExtBuchiState> worklist;
    unsigned int state_counter = 0;

    ThrExtBuchiState init;
    init.parent_state = A->getInitial();
    init.P1.clear();
    init.P2.clear();
    init.phase = ACC_WAIT_parent;
    init.epoch_nonempty = false;

    std::ostringstream ss;
    ss << "bxt_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
    state_map[init] = init_state;
    worklist.push_back(init);

    ThrExtOblBag P1_step, P2_step;
    ThrExtOblBag P1_next, P2_next;

    while (!worklist.empty()) {
        ThrExtBuchiState current = std::move(worklist.front());
        worklist.pop_front();

        auto copy_bag = [&](ThrExtOblBag& dst, const ThrExtOblBag& src) {
            ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_bag_copy_ms : nullptr);
            mmexp_record_thr_bag_copy(src);
            dst = src;
        };

        State* current_state = nullptr;
        {
            ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
            if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
            current_state = state_map.at(current);
        }
        const acc_phase_t phase_after_current = advance_phase_thrext(current);
        const bool reset_epoch =
            (current.phase == ACC_WAIT_P2EMPTY && current.P2.empty());

        for (uint32_t symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            if (!thrext_step_obl_bag(current.P1, symbol_id, P1_step, nullptr, child_tab, child_info)) {
                continue;
            }

            bool tracked_discharged = false;
            if (!current.P2.empty()) {
                if (!thrext_step_obl_bag(current.P2, symbol_id, P2_step, &tracked_discharged, child_tab, child_info)) {
                    continue;
                }
            } else {
                P2_step.clear();
            }

            SetStd<Edge*>* succs = current.parent_state->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* parent_edge : *succs) {
                State* q_prime = parent_edge->getTo();
                const size_t child_index = edgeWeightToChildIndex(parent_edge->getWeight()->getValue());
                const bool is_silent =
                    (child_index >= k) ||
                    !child_tab[child_index].child ||
                    !child_info[child_index].enabled;
                const bool boundary = current.P2.empty();
                bool epoch_nonempty_to = reset_epoch ? !current.P1.empty() : current.epoch_nonempty;
                if (tracked_discharged) epoch_nonempty_to = true;
                if (!is_silent) epoch_nonempty_to = true;

                if (is_silent) {
                    if (boundary) {
                        P1_next.clear();
                        copy_bag(P2_next, P1_step);
                    } else {
                        copy_bag(P1_next, P1_step);
                        copy_bag(P2_next, P2_step);
                    }

                    ThrExtBuchiState nxt;
                    nxt.parent_state = q_prime;
                    nxt.P1 = P1_next;
                    nxt.P2 = P2_next;
                    nxt.phase = phase_after_current;
                    nxt.epoch_nonempty = epoch_nonempty_to;

                    bool has_nxt = false;
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
                        has_nxt = state_map.contains(nxt);
                    }
                    if (!has_nxt) {
                        std::ostringstream s2;
                        s2 << "bxt_" << state_counter++;
                        State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_insert_calls++;
                        state_map[nxt] = ns;
                        worklist.push_back(nxt);
                    }

                    State* to_state = nullptr;
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
                        to_state = state_map.at(nxt);
                    }
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(SILENT)),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                    continue;
                }

                for (uint8_t guess = 0u; guess <= 1u; ++guess) {
                    ThrExtOblEntry spawned;
                    mmexp_record_thr_spawn(static_cast<uint32_t>(child_index), guess, symbol_id);
                    const ThrExtSpawnStatus st = thrext_spawn_obligation(
                        static_cast<uint32_t>(child_index),
                        symbol_id,
                        guess,
                        spawned,
                        child_tab,
                        child_info
                    );
                    if (st == ThrExtSpawnStatus::REJECT) continue;

                    if (boundary) {
                        copy_bag(P2_next, P1_step);
                        P1_next.clear();
                        if (st == ThrExtSpawnStatus::NONEMPTY) {
                            thrext_bag_add(P1_next, std::move(spawned));
                        }
                    } else {
                        copy_bag(P2_next, P2_step);
                        copy_bag(P1_next, P1_step);
                        if (st == ThrExtSpawnStatus::NONEMPTY) {
                            thrext_bag_add(P1_next, std::move(spawned));
                        }
                    }
                    thrext_bag_finalize(P1_next);

                    ThrExtBuchiState nxt;
                    nxt.parent_state = q_prime;
                    nxt.P1 = P1_next;
                    nxt.P2 = P2_next;
                    nxt.phase = phase_after_current;
                    nxt.epoch_nonempty = epoch_nonempty_to;

                    bool has_nxt = false;
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
                        has_nxt = state_map.contains(nxt);
                    }
                    if (!has_nxt) {
                        std::ostringstream s2;
                        s2 << "bxt_" << state_counter++;
                        State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_insert_calls++;
                        state_map[nxt] = ns;
                        worklist.push_back(nxt);
                    }

                    State* to_state = nullptr;
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
                        to_state = state_map.at(nxt);
                    }
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(static_cast<unsigned int>(guess))),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                }
            }
        }
    }

    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [gs, st] : state_map) {
        new_states->insert(st->getId(), st);
        if (gs.phase == ACC_WAIT_P2EMPTY && gs.P2.empty() && gs.epoch_nonempty) {
            st->setFinal(true);
        }
    }

    const std::string name = "BuchiThr(" + A->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights, global_min, global_max, init_state);
}

// =============================================================================
// Archived src/tests/minmax_sup_split_witness_probe.cpp
// =============================================================================

#include <iostream>
#include <string>
#include <vector>

#include "sanity_tests/test_common.h"

struct Scenario {
    std::string name;
    std::string file;
    value_function_t infVal;
    value_function_t finVal;
    weight_t threshold;
    bool expected;
};

static const char* as_text(bool value) {
    return value ? "true" : "false";
}

static bool eval_split_flat(NestedAutomaton* nwa,
                            value_function_t infVal,
                            value_function_t finVal,
                            weight_t threshold) {
    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup_split_witness(nwa, finVal, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}

int main() {
    const std::string base = "src/tests/correctness_tests/inputs/";
    const std::vector<Scenario> scenarios = {
        {"sup.max.initial_final_child.thr1", base + "sup_initial_final_child.txt", Sup, Max_f, weight_t(1), true},
        {"limsup.max.initial_final_child.thr1", base + "sup_initial_final_child.txt", LimSup, Max_f, weight_t(1), true},
        {"sup.min.initial_final_child_bad_symbol.thr1",
         base + "sup_initial_final_child_min_bad_current_symbol.txt", Sup, Min_f, weight_t(1), false},
        {"limsup.min.initial_final_child_bad_symbol.thr1",
         base + "sup_initial_final_child_min_bad_current_symbol.txt", LimSup, Min_f, weight_t(1), false},
        {"sup.max.background_obligation_blocker.thr1",
         base + "sup_background_obligation_blocker.txt", Sup, Max_f, weight_t(1), false},
        {"limsup.max.background_obligation_blocker.thr1",
         base + "sup_background_obligation_blocker.txt", LimSup, Max_f, weight_t(1), false},
        {"sup.min.background_obligation_blocker.thr1",
         base + "sup_background_obligation_blocker.txt", Sup, Min_f, weight_t(1), false},
        {"limsup.min.background_obligation_blocker.thr1",
         base + "sup_background_obligation_blocker.txt", LimSup, Min_f, weight_t(1), false},
        {"sup.max.background_obligation_blocker.thr0_5",
         base + "sup_background_obligation_blocker.txt", Sup, Max_f, weight_t(0.5), false},
        {"limsup.max.background_obligation_blocker.thr0_5",
         base + "sup_background_obligation_blocker.txt", LimSup, Max_f, weight_t(0.5), false},
        {"sup.max.background_collision_fresh_nomove.thr1",
         base + "sup_background_collision_fresh_nomove.txt", Sup, Max_f, weight_t(1), false},
        {"sup.max.background_collision_fresh_nomove.thr0",
         base + "sup_background_collision_fresh_nomove.txt", Sup, Max_f, weight_t(0), false},
        {"sup.max.background_collision_fresh_nomove.thr_minus1",
         base + "sup_background_collision_fresh_nomove.txt", Sup, Max_f, weight_t(-1), false},
        {"limsup.max.background_collision_fresh_nomove.thr1",
         base + "sup_background_collision_fresh_nomove.txt", LimSup, Max_f, weight_t(1), false},
        {"limsup.max.background_collision_fresh_nomove.thr0",
         base + "sup_background_collision_fresh_nomove.txt", LimSup, Max_f, weight_t(0), false},
        {"limsup.max.background_collision_fresh_nomove.thr_minus1",
         base + "sup_background_collision_fresh_nomove.txt", LimSup, Max_f, weight_t(-1), false},
        {"sup.min.background_collision_fresh_nomove.thr1",
         base + "sup_background_collision_fresh_nomove.txt", Sup, Min_f, weight_t(1), false},
        {"sup.min.background_collision_fresh_nomove.thr0",
         base + "sup_background_collision_fresh_nomove.txt", Sup, Min_f, weight_t(0), false},
        {"sup.min.background_collision_fresh_nomove.thr_minus1",
         base + "sup_background_collision_fresh_nomove.txt", Sup, Min_f, weight_t(-1), false},
        {"limsup.min.background_collision_fresh_nomove.thr1",
         base + "sup_background_collision_fresh_nomove.txt", LimSup, Min_f, weight_t(1), false},
        {"limsup.min.background_collision_fresh_nomove.thr0",
         base + "sup_background_collision_fresh_nomove.txt", LimSup, Min_f, weight_t(0), false},
        {"limsup.min.background_collision_fresh_nomove.thr_minus1",
         base + "sup_background_collision_fresh_nomove.txt", LimSup, Min_f, weight_t(-1), false},
        {"sup.max.max_merge_bug_complete.thr3",
         base + "max_merge_bug_complete.txt", Sup, Max_f, weight_t(3), false},
        {"sup.max.max_merge_bug_complete.thr2",
         base + "max_merge_bug_complete.txt", Sup, Max_f, weight_t(2), true},
        {"sup.max.max_merge_bug_complete.thr1",
         base + "max_merge_bug_complete.txt", Sup, Max_f, weight_t(1), true},
        {"sup.max.max_merge_bug_complete.thr0",
         base + "max_merge_bug_complete.txt", Sup, Max_f, weight_t(0), true},
        {"sup.max.max_merge_bug_complete.thr_minus1",
         base + "max_merge_bug_complete.txt", Sup, Max_f, weight_t(-1), true},
        {"limsup.max.max_merge_bug_complete.thr3",
         base + "max_merge_bug_complete.txt", LimSup, Max_f, weight_t(3), false},
        {"limsup.max.max_merge_bug_complete.thr2",
         base + "max_merge_bug_complete.txt", LimSup, Max_f, weight_t(2), true},
        {"limsup.max.max_merge_bug_complete.thr1",
         base + "max_merge_bug_complete.txt", LimSup, Max_f, weight_t(1), true},
        {"limsup.max.max_merge_bug_complete.thr0",
         base + "max_merge_bug_complete.txt", LimSup, Max_f, weight_t(0), true},
        {"limsup.max.max_merge_bug_complete.thr_minus1",
         base + "max_merge_bug_complete.txt", LimSup, Max_f, weight_t(-1), true},
        {"sup.max.issue2_limsup_false_positive.thr1",
         base + "split_witness_issue2_limsup_false_positive.txt", Sup, Max_f, weight_t(1), true},
        {"limsup.max.issue2_limsup_false_positive.thr1",
         base + "split_witness_issue2_limsup_false_positive.txt", LimSup, Max_f, weight_t(1), false},
        {"sup.min.max_merge_bug_complete.thr0",
         base + "max_merge_bug_complete.txt", Sup, Min_f, weight_t(0), true},
        {"sup.min.max_merge_bug_complete.thr_minus1",
         base + "max_merge_bug_complete.txt", Sup, Min_f, weight_t(-1), true},
        {"sup.min.max_merge_bug_complete.thr0_5",
         base + "max_merge_bug_complete.txt", Sup, Min_f, weight_t(0.5), false},
        {"limsup.min.max_merge_bug_complete.thr0",
         base + "max_merge_bug_complete.txt", LimSup, Min_f, weight_t(0), true},
        {"limsup.min.max_merge_bug_complete.thr_minus1",
         base + "max_merge_bug_complete.txt", LimSup, Min_f, weight_t(-1), true},
        {"limsup.min.max_merge_bug_complete.thr0_5",
         base + "max_merge_bug_complete.txt", LimSup, Min_f, weight_t(0.5), false},
        {"sup.max.issue5_phase_same_step_control.thr1",
         base + "split_witness_issue5_phase_same_step_control.txt", Sup, Max_f, weight_t(1), true},
        {"limsup.max.issue5_phase_same_step_control.thr1",
         base + "split_witness_issue5_phase_same_step_control.txt", LimSup, Max_f, weight_t(1), true},
        {"sup.max.issue5_phase_async_idle.thr1",
         base + "split_witness_issue5_phase_async_idle.txt", Sup, Max_f, weight_t(1), true},
        {"limsup.max.issue5_phase_async_idle.thr1",
         base + "split_witness_issue5_phase_async_idle.txt", LimSup, Max_f, weight_t(1), true},
        {"sup.max.issue5_phase_async_active.thr1",
         base + "split_witness_issue5_phase_async_active_false_negative.txt", Sup, Max_f, weight_t(1), true},
        {"limsup.max.issue5_phase_async_active.thr1",
         base + "split_witness_issue5_phase_async_active_false_negative.txt", LimSup, Max_f, weight_t(1), true},
    };

    unsigned int mismatches = 0;
    for (const Scenario& scenario : scenarios) {
        NestedAutomaton nwa(scenario.file);
        const bool got = eval_split_flat(&nwa, scenario.infVal, scenario.finVal, scenario.threshold);
        const bool ok = (got == scenario.expected);
        std::cout << (ok ? "OK   " : "DIFF ") << scenario.name
                  << " got=" << as_text(got)
                  << " expected=" << as_text(scenario.expected) << '\n';
        if (!ok) {
            ++mismatches;
        }
    }

    std::cout << "mismatches=" << mismatches << "/" << scenarios.size() << '\n';
    return mismatches == 0 ? 0 : 1;
}

// =============================================================================
// Archived former minmax_sup_resource_probe_split_old_v2.cpp
// =============================================================================

#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>

#include "sanity_tests/test_common.h"

static long get_peak_rss_kb() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

static Automaton* flatten_backend(NestedAutomaton* nwa,
                                  const std::string& backend,
                                  weight_t threshold) {
    if (backend == "split_witness") {
        return NestedAutomatonTester::flatten_MinMax_Sup_split_witness(nwa, Max_f, threshold);
    }
    throw std::invalid_argument("Unsupported backend: " + backend);
}

static void print_header() {
    std::cout
        << "backend,file,threshold,states,transitions,result,elapsed_ms,peak_rss_kb,rss_delta_kb\n";
}

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--header") {
        print_header();
        return 0;
    }

    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <backend> <nested_automaton_file> <threshold>\n";
        std::cerr << "  backend: split_witness\n";
        return 1;
    }

    const std::string backend = argv[1];
    const std::string filename = argv[2];
    const weight_t threshold = weight_t(std::stod(argv[3]));

    NestedAutomaton nwa(filename);

    const long rss_before = get_peak_rss_kb();
    const auto start = std::chrono::high_resolution_clock::now();

    Automaton* flat = flatten_backend(&nwa, backend, threshold);
    const unsigned int states = flat->getStates()->size();
    const unsigned int transitions = flat->getNbTransitions();
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, Sup, false);
    const bool result = non_silent->isNonEmpty_withFinal(Sup, weight_t(1));

    const auto end = std::chrono::high_resolution_clock::now();
    const long rss_after = get_peak_rss_kb();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << backend
              << "," << filename
              << "," << std::fixed << std::setprecision(6) << threshold
              << "," << states
              << "," << transitions
              << "," << (result ? 1 : 0)
              << "," << std::fixed << std::setprecision(6) << elapsed_ms
              << "," << rss_after
              << "," << (rss_after - rss_before)
              << "\n";

    delete non_silent;
    delete flat;
    return 0;
}
