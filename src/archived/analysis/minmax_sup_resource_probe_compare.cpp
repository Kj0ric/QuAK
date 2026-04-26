#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>

#include "tests/sanity_tests/test_common.h"

static long get_peak_rss_kb() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <label> <nested_automaton_file> <threshold>\n";
        return 1;
    }

    const std::string label = argv[1];
    const std::string filename = argv[2];
    const weight_t threshold = weight_t(std::stod(argv[3]));

    NestedAutomaton nwa(filename);

    const long rss_before = get_peak_rss_kb();
    const auto start = std::chrono::high_resolution_clock::now();

    Automaton* flat = NestedAutomatonTester::flatten_MinMax_Sup(&nwa, Max_f, threshold);
    const unsigned int states = flat->getStates()->size();
    const unsigned int transitions = flat->getNbTransitions();
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, Sup, false);
    const bool result = non_silent->isNonEmpty_withFinal(Sup, weight_t(1));

    const auto end = std::chrono::high_resolution_clock::now();
    const long rss_after = get_peak_rss_kb();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << label
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
