#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "tests/sanity_tests/test_common.h"

struct SymbolTemplate {
    const char* label;
    std::vector<std::pair<const char*, int>> edges;
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

static void write_parent(std::ofstream& out, const std::vector<std::string>& symbols) {
    out << "@PARENT\n";
    out << "final: all\n";
    for (const std::string& symbol : symbols) {
        out << symbol << " : 1, q -> q\n";
    }
    out << "\n@CHILD 0\n\n";
}

static void write_parent_toggle(std::ofstream& out) {
    out << "@PARENT\n";
    out << "final: p0\n";
    out << "a : 1, p0 -> p0\n";
    out << "b : 1, p0 -> p0\n";
    out << "c : 1, p0 -> p1\n";
    out << "d : 1, p0 -> p0\n";
    out << "a : 1, p1 -> p1\n";
    out << "b : 1, p1 -> p1\n";
    out << "c : 1, p1 -> p1\n";
    out << "d : 1, p1 -> p0\n\n";
    out << "@CHILD 0\n\n";
}

static void write_edges(std::ofstream& out,
                        const std::string& symbol,
                        const char* source,
                        const SymbolTemplate& templ) {
    for (const auto& edge : templ.edges) {
        out << symbol << " : " << edge.second << ", " << source << " -> " << edge.first << "\n";
    }
}

static bool check_case(const std::string& path,
                       const std::string& family,
                       const std::string& detail,
                       unsigned int& checked) {
    NestedAutomaton nwa(path);
    for (value_function_t infVal : {Sup, LimSup}) {
        for (value_function_t finVal : {Max_f, Min_f}) {
            for (weight_t threshold : {weight_t(0), weight_t(1)}) {
                const bool split = eval_split_flat(&nwa, infVal, finVal, threshold);
                const bool regular = eval_regular_flat(&nwa, infVal, finVal, threshold);
                ++checked;
                if (split != regular) {
                    std::cout << "MISMATCH family=" << family
                              << " detail=" << detail
                              << " inf=" << (infVal == Sup ? "Sup" : "LimSup")
                              << " fin=" << (finVal == Max_f ? "Max_f" : "Min_f")
                              << " threshold=" << threshold.to_float()
                              << " split=" << split
                              << " regular=" << regular << "\n";
                    return true;
                }
            }
        }
    }
    return false;
}

static int search_all_collocated() {
    const std::vector<SymbolTemplate> templates = {
        {"loop0", {{"s0", 0}}},
        {"loop1", {{"s0", 1}}},
        {"fin0", {{"f", 0}}},
        {"fin1", {{"f", 1}}},
        {"loop0_fin0", {{"s0", 0}, {"f", 0}}},
        {"loop0_fin1", {{"s0", 0}, {"f", 1}}},
        {"loop1_fin0", {{"s0", 1}, {"f", 0}}},
        {"loop1_fin1", {{"s0", 1}, {"f", 1}}},
    };

    const std::string path = "analysis/issue3_search_candidate.txt";
    unsigned int checked = 0;

    for (const SymbolTemplate& a : templates) {
        for (const SymbolTemplate& b : templates) {
            std::ofstream out(path);
            write_parent(out, {"a", "b"});
            out << "@CHILD 1\n";
            out << "final: f\n";
            write_edges(out, "a", "s0", a);
            write_edges(out, "b", "s0", b);
            out.close();

            const std::string detail =
                std::string("a=") + a.label + ",b=" + b.label;
            if (check_case(path, "all_collocated", detail, checked)) {
                return 0;
            }
        }
    }

    std::cout << "family=all_collocated checked=" << checked << " mismatches=0\n";
    return 0;
}

static int search_forced_collision() {
    const std::vector<SymbolTemplate> s0_c_templates = {
        {"s0_0", {{"s0", 0}}},
        {"s0_1", {{"s0", 1}}},
        {"u_0", {{"u", 0}}},
        {"u_1", {{"u", 1}}},
        {"t_0", {{"t", 0}}},
        {"t_1", {{"t", 1}}},
        {"f_0", {{"f", 0}}},
        {"f_1", {{"f", 1}}},
    };

    const std::vector<SymbolTemplate> u_c_templates = {
        {"u_0", {{"u", 0}}},
        {"u_1", {{"u", 1}}},
        {"t_0", {{"t", 0}}},
        {"t_1", {{"t", 1}}},
        {"f_0", {{"f", 0}}},
        {"f_1", {{"f", 1}}},
        {"u0_f0", {{"u", 0}, {"f", 0}}},
        {"u0_f1", {{"u", 0}, {"f", 1}}},
        {"u1_f0", {{"u", 1}, {"f", 0}}},
        {"u1_f1", {{"u", 1}, {"f", 1}}},
        {"t0_f0", {{"t", 0}, {"f", 0}}},
        {"t0_f1", {{"t", 0}, {"f", 1}}},
        {"t1_f0", {{"t", 1}, {"f", 0}}},
        {"t1_f1", {{"t", 1}, {"f", 1}}},
    };

    const std::vector<SymbolTemplate> t_c_templates = {
        {"t_0", {{"t", 0}}},
        {"t_1", {{"t", 1}}},
        {"f_0", {{"f", 0}}},
        {"f_1", {{"f", 1}}},
    };

    const std::string path = "analysis/issue3_search_candidate.txt";
    unsigned int checked = 0;

    for (const SymbolTemplate& s0_c : s0_c_templates) {
        for (const SymbolTemplate& u_c : u_c_templates) {
            for (const SymbolTemplate& t_c : t_c_templates) {
                std::ofstream out(path);
                write_parent(out, {"a", "b", "c"});
                out << "@CHILD 1\n";
                out << "final: f\n";
                out << "a : 0, s0 -> s0\n";
                out << "b : 0, s0 -> u\n";
                write_edges(out, "c", "s0", s0_c);
                out << "\n";
                out << "a : 0, u -> u\n";
                out << "b : 0, u -> u\n";
                write_edges(out, "c", "u", u_c);
                out << "\n";
                out << "a : 0, t -> t\n";
                out << "b : 0, t -> t\n";
                write_edges(out, "c", "t", t_c);
                out.close();

                const std::string detail =
                    std::string("s0.c=") + s0_c.label +
                    ",u.c=" + u_c.label +
                    ",t.c=" + t_c.label;
                if (check_case(path, "forced_collision", detail, checked)) {
                    return 0;
                }
            }
        }
    }

    std::cout << "family=forced_collision checked=" << checked << " mismatches=0\n";
    return 0;
}

static int search_broad_random() {
    const std::vector<SymbolTemplate> templates = {
        {"s0_0", {{"s0", 0}}},
        {"s0_1", {{"s0", 1}}},
        {"u_0", {{"u", 0}}},
        {"u_1", {{"u", 1}}},
        {"t_0", {{"t", 0}}},
        {"t_1", {{"t", 1}}},
        {"f_0", {{"f", 0}}},
        {"f_1", {{"f", 1}}},
        {"s0_0_f_0", {{"s0", 0}, {"f", 0}}},
        {"s0_0_f_1", {{"s0", 0}, {"f", 1}}},
        {"s0_1_f_0", {{"s0", 1}, {"f", 0}}},
        {"s0_1_f_1", {{"s0", 1}, {"f", 1}}},
        {"u_0_f_0", {{"u", 0}, {"f", 0}}},
        {"u_0_f_1", {{"u", 0}, {"f", 1}}},
        {"u_1_f_0", {{"u", 1}, {"f", 0}}},
        {"u_1_f_1", {{"u", 1}, {"f", 1}}},
        {"t_0_f_0", {{"t", 0}, {"f", 0}}},
        {"t_0_f_1", {{"t", 0}, {"f", 1}}},
        {"t_1_f_0", {{"t", 1}, {"f", 0}}},
        {"t_1_f_1", {{"t", 1}, {"f", 1}}},
    };

    std::mt19937_64 rng(0x5eed1234ULL);
    std::uniform_int_distribution<size_t> pick(0, templates.size() - 1);

    const std::string path = "analysis/issue3_search_candidate.txt";
    const unsigned int samples = 20000;
    unsigned int checked = 0;

    for (unsigned int sample = 0; sample < samples; ++sample) {
        const SymbolTemplate& s0_c = templates[pick(rng)];
        const SymbolTemplate& s0_d = templates[pick(rng)];
        const SymbolTemplate& u_a = templates[pick(rng)];
        const SymbolTemplate& u_b = templates[pick(rng)];
        const SymbolTemplate& u_c = templates[pick(rng)];
        const SymbolTemplate& u_d = templates[pick(rng)];
        const SymbolTemplate& t_a = templates[pick(rng)];
        const SymbolTemplate& t_b = templates[pick(rng)];
        const SymbolTemplate& t_c = templates[pick(rng)];
        const SymbolTemplate& t_d = templates[pick(rng)];

        std::ofstream out(path);
        write_parent(out, {"a", "b", "c", "d"});
        out << "@CHILD 1\n";
        out << "final: f\n";
        out << "a : 0, s0 -> s0\n";
        out << "b : 0, s0 -> u\n";
        write_edges(out, "c", "s0", s0_c);
        write_edges(out, "d", "s0", s0_d);
        out << "\n";
        write_edges(out, "a", "u", u_a);
        write_edges(out, "b", "u", u_b);
        write_edges(out, "c", "u", u_c);
        write_edges(out, "d", "u", u_d);
        out << "\n";
        write_edges(out, "a", "t", t_a);
        write_edges(out, "b", "t", t_b);
        write_edges(out, "c", "t", t_c);
        write_edges(out, "d", "t", t_d);
        out.close();

        const std::string detail =
            std::string("sample=") + std::to_string(sample) +
            ",s0.c=" + s0_c.label +
            ",s0.d=" + s0_d.label +
            ",u.a=" + u_a.label +
            ",u.b=" + u_b.label +
            ",u.c=" + u_c.label +
            ",u.d=" + u_d.label +
            ",t.a=" + t_a.label +
            ",t.b=" + t_b.label +
            ",t.c=" + t_c.label +
            ",t.d=" + t_d.label;
        if (check_case(path, "broad_random", detail, checked)) {
            return 0;
        }
    }

    std::cout << "family=broad_random samples=" << samples
              << " checked=" << checked << " mismatches=0\n";
    return 0;
}

static int search_broad_random_toggle_parent() {
    const std::vector<SymbolTemplate> templates = {
        {"s0_0", {{"s0", 0}}},
        {"s0_1", {{"s0", 1}}},
        {"u_0", {{"u", 0}}},
        {"u_1", {{"u", 1}}},
        {"t_0", {{"t", 0}}},
        {"t_1", {{"t", 1}}},
        {"f_0", {{"f", 0}}},
        {"f_1", {{"f", 1}}},
        {"s0_0_f_0", {{"s0", 0}, {"f", 0}}},
        {"s0_0_f_1", {{"s0", 0}, {"f", 1}}},
        {"s0_1_f_0", {{"s0", 1}, {"f", 0}}},
        {"s0_1_f_1", {{"s0", 1}, {"f", 1}}},
        {"u_0_f_0", {{"u", 0}, {"f", 0}}},
        {"u_0_f_1", {{"u", 0}, {"f", 1}}},
        {"u_1_f_0", {{"u", 1}, {"f", 0}}},
        {"u_1_f_1", {{"u", 1}, {"f", 1}}},
        {"t_0_f_0", {{"t", 0}, {"f", 0}}},
        {"t_0_f_1", {{"t", 0}, {"f", 1}}},
        {"t_1_f_0", {{"t", 1}, {"f", 0}}},
        {"t_1_f_1", {{"t", 1}, {"f", 1}}},
    };

    std::mt19937_64 rng(0x5eed5678ULL);
    std::uniform_int_distribution<size_t> pick(0, templates.size() - 1);

    const std::string path = "analysis/issue3_search_candidate.txt";
    const unsigned int samples = 10000;
    unsigned int checked = 0;

    for (unsigned int sample = 0; sample < samples; ++sample) {
        const SymbolTemplate& s0_c = templates[pick(rng)];
        const SymbolTemplate& s0_d = templates[pick(rng)];
        const SymbolTemplate& u_a = templates[pick(rng)];
        const SymbolTemplate& u_b = templates[pick(rng)];
        const SymbolTemplate& u_c = templates[pick(rng)];
        const SymbolTemplate& u_d = templates[pick(rng)];
        const SymbolTemplate& t_a = templates[pick(rng)];
        const SymbolTemplate& t_b = templates[pick(rng)];
        const SymbolTemplate& t_c = templates[pick(rng)];
        const SymbolTemplate& t_d = templates[pick(rng)];

        std::ofstream out(path);
        write_parent_toggle(out);
        out << "@CHILD 1\n";
        out << "final: f\n";
        out << "a : 0, s0 -> s0\n";
        out << "b : 0, s0 -> u\n";
        write_edges(out, "c", "s0", s0_c);
        write_edges(out, "d", "s0", s0_d);
        out << "\n";
        write_edges(out, "a", "u", u_a);
        write_edges(out, "b", "u", u_b);
        write_edges(out, "c", "u", u_c);
        write_edges(out, "d", "u", u_d);
        out << "\n";
        write_edges(out, "a", "t", t_a);
        write_edges(out, "b", "t", t_b);
        write_edges(out, "c", "t", t_c);
        write_edges(out, "d", "t", t_d);
        out.close();

        const std::string detail =
            std::string("sample=") + std::to_string(sample) +
            ",s0.c=" + s0_c.label +
            ",s0.d=" + s0_d.label +
            ",u.a=" + u_a.label +
            ",u.b=" + u_b.label +
            ",u.c=" + u_c.label +
            ",u.d=" + u_d.label +
            ",t.a=" + t_a.label +
            ",t.b=" + t_b.label +
            ",t.c=" + t_c.label +
            ",t.d=" + t_d.label;
        if (check_case(path, "broad_random_toggle_parent", detail, checked)) {
            return 0;
        }
    }

    std::cout << "family=broad_random_toggle_parent samples=" << samples
              << " checked=" << checked << " mismatches=0\n";
    return 0;
}

int main() {
    if (search_all_collocated() != 0) {
        return 1;
    }
    if (search_forced_collision() != 0) {
        return 1;
    }
    if (search_broad_random() != 0) {
        return 1;
    }
    if (search_broad_random_toggle_parent() != 0) {
        return 1;
    }
    return 0;
}
