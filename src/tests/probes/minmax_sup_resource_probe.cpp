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
    if (backend == "default") {
        return NestedAutomatonTester::flatten_MinMax_Sup(nwa, Max_f, threshold);
    }
    if (backend == "cached_mmthr") {
        return NestedAutomatonTester::flatten_MinMax_Sup_cached(nwa, Max_f, threshold);
    }
    if (backend == "witness_cached") {
        return NestedAutomatonTester::flatten_MinMax_Sup_witness_cached(nwa, Max_f, threshold);
    }
    if (backend == "regular") {
        return NestedAutomatonTester::flatten_regular(nwa, Max_f, threshold);
    }
    throw std::invalid_argument("Unsupported backend: " + backend);
}

static bool uses_binary_threshold(const std::string& backend) {
    if (backend == "default") {
        return true;
    }
    if (backend == "cached_mmthr") {
        return true;
    }
    if (backend == "witness_cached") {
        return true;
    }
    if (backend == "regular") {
        return false;
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
        std::cerr << "  backend: default | cached_mmthr | witness_cached | regular\n";
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
    const bool result = non_silent->isNonEmpty_withFinal(
        Sup,
        uses_binary_threshold(backend) ? weight_t(1) : threshold
    );

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
