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

static value_function_t parse_inf(const std::string& raw) {
    if (raw == "Inf") return Inf;
    if (raw == "LimInf") return LimInf;
    throw std::invalid_argument("Unsupported inf aggregator: " + raw);
}

static Automaton* flatten_backend(NestedAutomaton* nwa,
                                  const std::string& backend,
                                  weight_t threshold) {
    if (backend == "default") {
        return nwa->flatten_MinMax_Inf(Max_f, threshold);
    }
    if (backend == "cached") {
        return NestedAutomatonTester::flatten_MinMax_Inf_cached(nwa, Max_f, threshold);
    }
    throw std::invalid_argument("Unsupported backend: " + backend);
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <backend> <nested_automaton_file> <Inf|LimInf> <threshold>\n";
        std::cerr << "  backend: default | cached\n";
        return 1;
    }

    const std::string backend = argv[1];
    const std::string filename = argv[2];
    const value_function_t infVal = parse_inf(argv[3]);
    const weight_t threshold = weight_t(std::stod(argv[4]));

    NestedAutomaton nwa(filename);

    const long rss_before = get_peak_rss_kb();
    const auto start = std::chrono::high_resolution_clock::now();

    Automaton* flat = flatten_backend(&nwa, backend, threshold);
    const unsigned int states = flat->getStates()->size();
    const unsigned int transitions = flat->getNbTransitions();
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));

    const auto end = std::chrono::high_resolution_clock::now();
    const long rss_after = get_peak_rss_kb();

    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "backend=" << backend
              << " file=" << filename
              << " inf=" << argv[3]
              << " threshold=" << threshold
              << " states=" << states
              << " transitions=" << transitions
              << " result=" << (result ? 1 : 0)
              << " elapsed_ms=" << std::fixed << std::setprecision(6) << elapsed_ms
              << " peak_rss_kb=" << rss_after
              << " rss_delta_kb=" << (rss_after - rss_before)
              << "\n";

    delete non_silent;
    delete flat;
    return 0;
}
