#include <iostream>
#include <string>
#include <vector>

#include "NestedAutomaton.h"

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

int main() {
    const std::string base = "samples/tests/correctness/";
    const std::vector<Scenario> scenarios = {
        {"sup.max.initial_final_child", base + "sup_initial_final_child.txt", Sup, Max_f, weight_t(1), true},
        {"limsup.max.initial_final_child", base + "sup_initial_final_child.txt", LimSup, Max_f, weight_t(1), true},
        {"sup.min.initial_final_child_bad_current_symbol",
         base + "sup_initial_final_child_min_bad_current_symbol.txt", Sup, Min_f, weight_t(1), false},
        {"limsup.min.initial_final_child_bad_current_symbol",
         base + "sup_initial_final_child_min_bad_current_symbol.txt", LimSup, Min_f, weight_t(1), false},
        {"sup.max.background_obligation_blocker",
         base + "sup_background_obligation_blocker.txt", Sup, Max_f, weight_t(1), false},
        {"limsup.max.background_obligation_blocker",
         base + "sup_background_obligation_blocker.txt", LimSup, Max_f, weight_t(1), false},
        {"sup.min.background_obligation_blocker",
         base + "sup_background_obligation_blocker.txt", Sup, Min_f, weight_t(1), false},
        {"limsup.min.background_obligation_blocker",
         base + "sup_background_obligation_blocker.txt", LimSup, Min_f, weight_t(1), false},
    };

    unsigned int mismatches = 0;
    for (const Scenario& scenario : scenarios) {
        NestedAutomaton nwa(scenario.file);
        const bool got = nwa.isNonEmpty(scenario.infVal, scenario.finVal, scenario.threshold);
        const bool ok = (got == scenario.expected);
        std::cout << (ok ? "OK   " : "DIFF ") << scenario.name
                  << " got=" << as_text(got)
                  << " expected=" << as_text(scenario.expected) << '\n';
        if (!ok) {
            ++mismatches;
        }
    }

    std::cout << "mismatches=" << mismatches << "/" << scenarios.size() << '\n';
    return 0;
}
