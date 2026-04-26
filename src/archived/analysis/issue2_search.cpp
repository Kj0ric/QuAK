#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "tests/sanity_tests/test_common.h"

struct Choice {
    const char* to;
    int weight;
};

static bool eval_split_flat(NestedAutomaton* nwa,
                            value_function_t infVal,
                            value_function_t finVal,
                            weight_t threshold) {
    Automaton* flat =
        NestedAutomatonTester::flatten_MinMax_Sup_split_witness(nwa, finVal, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}

static bool eval_regular_flat(NestedAutomaton* nwa,
                              value_function_t infVal,
                              value_function_t finVal,
                              weight_t threshold) {
    Automaton* flat = NestedAutomatonTester::flatten_regular(nwa, finVal, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);
    delete non_silent;
    delete flat;
    return result;
}

static void write_case(const std::string& path,
                       const Choice& s0_c,
                       const Choice& s1_b,
                       const Choice& s1_c,
                       const Choice& s2_c) {
    std::ofstream out(path);
    out << "@PARENT\n";
    out << "final: all\n";
    out << "a : 1, q -> q\n";
    out << "b : 1, q -> q\n";
    out << "c : 1, q -> q\n\n";
    out << "@CHILD 0\n\n";
    out << "@CHILD 1\n";
    out << "final: f\n";
    out << "a : 0, s0 -> s0\n";
    out << "b : 0, s0 -> s1\n";
    out << "c : " << s0_c.weight << ", s0 -> " << s0_c.to << "\n\n";
    out << "a : 0, s1 -> s2\n";
    out << "b : " << s1_b.weight << ", s1 -> " << s1_b.to << "\n";
    out << "c : " << s1_c.weight << ", s1 -> " << s1_c.to << "\n\n";
    out << "a : 0, s2 -> s2\n";
    out << "b : 0, s2 -> s2\n";
    out << "c : " << s2_c.weight << ", s2 -> " << s2_c.to << "\n";
}

int main() {
    const std::vector<Choice> choices = {
        {"s0", 0}, {"s0", 1},
        {"s1", 0}, {"s1", 1},
        {"s2", 0}, {"s2", 1},
        {"f", 0},  {"f", 1},
    };

    const std::string path = "analysis/issue2_search_candidate.txt";
    unsigned int checked = 0;

    for (const Choice& s0_c : choices) {
        for (const Choice& s1_b : choices) {
            for (const Choice& s1_c : choices) {
                for (const Choice& s2_c : choices) {
                    write_case(path, s0_c, s1_b, s1_c, s2_c);
                    NestedAutomaton nwa(path);

                    for (value_function_t infVal : {Sup, LimSup}) {
                        const bool split = eval_split_flat(&nwa, infVal, Max_f, weight_t(1));
                        const bool regular = eval_regular_flat(&nwa, infVal, Max_f, weight_t(1));
                        ++checked;
                        if (split != regular) {
                            std::cout << "MISMATCH inf="
                                      << (infVal == Sup ? "Sup" : "LimSup")
                                      << " split=" << split
                                      << " regular=" << regular << "\n";
                            std::cout << "s0_c=" << s0_c.to << "/" << s0_c.weight
                                      << " s1_b=" << s1_b.to << "/" << s1_b.weight
                                      << " s1_c=" << s1_c.to << "/" << s1_c.weight
                                      << " s2_c=" << s2_c.to << "/" << s2_c.weight << "\n";
                            return 0;
                        }
                    }
                }
            }
        }
    }

    std::cout << "checked=" << checked << " mismatches=0\n";
    return 0;
}
