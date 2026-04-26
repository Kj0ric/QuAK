/**
 * benchmark_oblbag.cpp
 *
 * Benchmark for comparing OblBag implementations in flatten_regular.
 * Measures time and memory. OblBag stats are printed by flatten_regular itself.
 */

#include <iostream>
#include <chrono>
#include <sys/resource.h>
#include "NestedAutomaton.h"

static long get_peak_rss_kb() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <nested_automaton_file> <bound> [finVal]" << std::endl;
        std::cerr << "  finVal: SumB (default), Max_f, Min_f" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    weight_t bound = weight_t(std::stod(argv[2]));

    value_function_t finVal = SumB;
    if (argc > 3) {
        std::string fv = argv[3];
        if (fv == "Max_f") finVal = Max_f;
        else if (fv == "Min_f") finVal = Min_f;
        else if (fv != "SumB") {
            std::cerr << "Unknown finVal: " << fv << std::endl;
            return 1;
        }
    }

    NestedAutomaton* nwa = nullptr;
    try {
        nwa = new NestedAutomaton(filename);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "=== Input ===" << std::endl;
    std::cout << "File: " << filename << std::endl;
    std::cout << "Parent states: " << nwa->getNbStates() << std::endl;
    std::cout << "Children: " << nwa->getChildrenSize() << std::endl;
    std::cout << "Alphabet: " << nwa->getAlphabet()->size() << std::endl;
    std::cout << "Bound: " << bound.to_float() << std::endl;

    long rss_before = get_peak_rss_kb();
    auto start = std::chrono::high_resolution_clock::now();

    // This will also print OblBag stats internally
    Automaton* flat = nwa->flatten_regular(finVal, bound);

    auto end = std::chrono::high_resolution_clock::now();
    long rss_after = get_peak_rss_kb();

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "=== Output ===" << std::endl;
    std::cout << "States: " << flat->getNbStates() << std::endl;
    std::cout << "Transitions: " << flat->getNbTransitions() << std::endl;
    std::cout << "=== Timing ===" << std::endl;
    std::cout << "Wall time: " << duration_ms << " ms (" << duration_us << " us)" << std::endl;
    std::cout << "Peak RSS: " << rss_after << " KB" << std::endl;
    std::cout << "RSS delta: " << (rss_after - rss_before) << " KB" << std::endl;

    delete flat;
    delete nwa;

    return 0;
}
