#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>

#include "sanity_tests/test_common.h"

using FlattenFn = Automaton* (*)(NestedAutomaton*, value_function_t, weight_t);

struct BackendSpec {
    const char* name;
    FlattenFn flatten;
    bool binary_threshold;
};

static long get_peak_rss_kb() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

static value_function_t parse_inf(const std::string& raw) {
    if (raw == "Inf") return Inf;
    if (raw == "LimInf") return LimInf;
    throw std::invalid_argument("Unsupported inf aggregator: " + raw);
}

static value_function_t parse_fin(const std::string& raw) {
    if (raw == "SumPlus") return SumPlus;
    if (raw == "SumMinus") return SumMinus;
    throw std::invalid_argument("Unsupported finite aggregator: " + raw);
}

static Automaton* flatten_default(NestedAutomaton* nwa, value_function_t fin, weight_t thr) {
    return NestedAutomatonTester::flatten_SumPlusMinus_Inf(nwa, fin, thr);
}

static Automaton* flatten_cached(NestedAutomaton* nwa, value_function_t fin, weight_t thr) {
    return NestedAutomatonTester::flatten_SumPlusMinus_Inf_cached(nwa, fin, thr);
}

static Automaton* flatten_regular_wrapper(NestedAutomaton* nwa, value_function_t fin, weight_t thr) {
    return NestedAutomatonTester::flatten_regular(nwa, fin, thr);
}

static const BackendSpec& get_backend(const std::string& name) {
    static const BackendSpec backends[] = {
        {"default", flatten_default, true},
        {"cached", flatten_cached, true},
        {"regular", flatten_regular_wrapper, false},
    };

    for (const BackendSpec& backend : backends) {
        if (name == backend.name) return backend;
    }
    throw std::invalid_argument("Unsupported backend: " + name);
}

static bool parse_stats_flag(const std::string& raw) {
    if (raw == "0" || raw == "false" || raw == "off") return false;
    if (raw == "1" || raw == "true" || raw == "on") return true;
    throw std::invalid_argument("Unsupported stats flag: " + raw);
}

static void print_header() {
    std::cout
        << "backend,file,inf,fin,threshold,stats_enabled,states,transitions,result,elapsed_ms,"
        << "peak_rss_kb,rss_delta_kb,"
        << "state_map_lookup_calls,state_map_insert_calls,"
        << "spawn_calls,unique_spawn_keys,"
        << "step_bag_calls,unique_bag_step_keys,step_bag_cache_hits,"
        << "step_obl_calls,step_obl_cache_hits,"
        << "bag_add_calls,bag_add_cache_hits,"
        << "bag_copy_ops,bag_copy_entries,"
        << "frontier_observations,frontier_config_total,frontier_capacity_total,"
        << "unique_obligation_count,unique_bag_count,"
        << "time_step_bag_ms,time_state_map_ms,time_bag_copy_ms\n";
}

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--header") {
        print_header();
        return 0;
    }

    if (argc != 7) {
        std::cerr << "Usage: " << argv[0]
                  << " <backend> <nested_automaton_file> <Inf|LimInf> <SumPlus|SumMinus> <threshold> <stats:0|1>\n";
        std::cerr << "  backend: default | cached | regular\n";
        return 1;
    }

    const std::string backend_name = argv[1];
    const std::string filename = argv[2];
    const value_function_t infVal = parse_inf(argv[3]);
    const value_function_t finVal = parse_fin(argv[4]);
    const weight_t threshold = weight_t(std::stod(argv[5]));
    const bool stats_enabled = parse_stats_flag(argv[6]);

    const BackendSpec& backend = get_backend(backend_name);
    NestedAutomaton nwa(filename);

    NestedAutomatonTester::setMinMaxInfExperimentStatsEnabled(stats_enabled);
    NestedAutomatonTester::resetMinMaxInfExperimentStats();

    const long rss_before = get_peak_rss_kb();
    const auto start = std::chrono::high_resolution_clock::now();

    Automaton* flat = backend.flatten(&nwa, finVal, threshold);
    const unsigned int states = flat->getStates()->size();
    const unsigned int transitions = flat->getNbTransitions();
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result =
        non_silent->isNonEmpty_withFinal(infVal, backend.binary_threshold ? weight_t(1) : threshold);

    const auto end = std::chrono::high_resolution_clock::now();
    const long rss_after = get_peak_rss_kb();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    const MinMaxInfExperimentStats stats =
        NestedAutomatonTester::getMinMaxInfExperimentStats();
    NestedAutomatonTester::setMinMaxInfExperimentStatsEnabled(false);

    std::cout << backend.name
              << "," << filename
              << "," << argv[3]
              << "," << argv[4]
              << "," << std::fixed << std::setprecision(6) << threshold
              << "," << (stats_enabled ? 1 : 0)
              << "," << states
              << "," << transitions
              << "," << (result ? 1 : 0)
              << "," << std::fixed << std::setprecision(6) << elapsed_ms
              << "," << rss_after
              << "," << (rss_after - rss_before)
              << "," << stats.state_map_lookup_calls
              << "," << stats.state_map_insert_calls
              << "," << stats.spawn_calls
              << "," << stats.unique_spawn_keys
              << "," << stats.step_bag_calls
              << "," << stats.unique_bag_step_keys
              << "," << stats.step_bag_cache_hits
              << "," << stats.step_obl_calls
              << "," << stats.step_obl_cache_hits
              << "," << stats.bag_add_calls
              << "," << stats.bag_add_cache_hits
              << "," << stats.bag_copy_ops
              << "," << stats.bag_copy_entries
              << "," << stats.frontier_observations
              << "," << stats.frontier_config_total
              << "," << stats.frontier_capacity_total
              << "," << stats.unique_obligation_count
              << "," << stats.unique_bag_count
              << "," << std::fixed << std::setprecision(6) << stats.time_step_bag_ms
              << "," << std::fixed << std::setprecision(6) << stats.time_state_map_ms
              << "," << std::fixed << std::setprecision(6) << stats.time_bag_copy_ms
              << "\n";

    delete non_silent;
    delete flat;
    return 0;
}
