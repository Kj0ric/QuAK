/**
 * test_split_final_differential.cpp
 *
 * Randomized differential tests for the final-state continuation contract.
 * Each generated automaton is compared against a stop/continue split-final
 * normalization at the decision level.
 */

#include "test_correctness_common.h"

#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>

using ThresholdFlattenFn = Automaton* (*)(NestedAutomaton*, value_function_t, weight_t);

namespace {

constexpr uint32_t RNG_SEED = 0x5f17a1u;
constexpr int NUM_CASES = 1000;
constexpr int REAL_CHILDREN = 3;
constexpr weight_t SUMB_BOUND = 5;

const int CHILD_WEIGHT_VALUES[] = {0, 1, 2, 3};

void add_edge(Symbol* symbol, Weight* weight, State* from, State* to) {
    Edge* edge = new Edge(symbol, weight, from, to);
    from->addSuccessor(edge);
    to->addPredecessor(edge);
}

int rand_int(std::mt19937& rng, int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

bool rand_chance(std::mt19937& rng, int numerator, int denominator) {
    return rand_int(rng, 1, denominator) <= numerator;
}

MapArray<Symbol*>* make_parent_alphabet(unsigned int alphabet_size) {
    Symbol::RESET();
    MapArray<Symbol*>* alphabet = new MapArray<Symbol*>(alphabet_size);
    const char* names[] = {"a", "b", "c"};
    for (unsigned int i = 0; i < alphabet_size; ++i) {
        Symbol* symbol = new Symbol(names[i]);
        alphabet->insert(symbol->getId(), symbol);
    }
    return alphabet;
}

MapArray<Weight*>* make_parent_weights() {
    Weight::RESET();
    MapArray<Weight*>* weights = new MapArray<Weight*>(REAL_CHILDREN + 1);
    for (int child = 0; child <= REAL_CHILDREN; ++child) {
        Weight* weight = new Weight(weight_t(child));
        weights->insert(weight->getId(), weight);
    }
    return weights;
}

MapArray<Weight*>* make_child_weights(std::map<int, Weight*>& by_value) {
    Weight::RESET();
    MapArray<Weight*>* weights =
        new MapArray<Weight*>(sizeof(CHILD_WEIGHT_VALUES) / sizeof(CHILD_WEIGHT_VALUES[0]));
    for (int value : CHILD_WEIGHT_VALUES) {
        Weight* weight = new Weight(weight_t(value));
        weights->insert(weight->getId(), weight);
        by_value[value] = weight;
    }
    return weights;
}

MapArray<Symbol*>* copy_alphabet_for_child(const MapArray<Symbol*>* parent_alphabet) {
    MapArray<Symbol*>* alphabet = new MapArray<Symbol*>(parent_alphabet->size());
    for (unsigned int i = 0; i < parent_alphabet->size(); ++i) {
        Symbol* symbol = new Symbol(parent_alphabet->at(i));
        alphabet->insert(symbol->getId(), symbol);
    }
    return alphabet;
}

ChildAutomaton* make_child(const std::string& name,
                           const MapArray<Symbol*>* parent_alphabet,
                           int child_kind,
                           std::mt19937& rng) {
    MapArray<Symbol*>* alphabet = copy_alphabet_for_child(parent_alphabet);

    std::map<int, Weight*> weight_by_value;
    MapArray<Weight*>* weights = make_child_weights(weight_by_value);

    const unsigned int state_count = static_cast<unsigned int>(3 + rand_int(rng, 0, 1));
    State::RESET();
    MapArray<State*>* states = new MapArray<State*>(state_count);
    for (unsigned int i = 0; i < state_count; ++i) {
        State* state = new State("s" + std::to_string(i),
                                 alphabet->size(),
                                 weight_t(0),
                                 weight_t(3));
        state->setFinal(i == 1 || i == 2 || (i > 2 && rand_chance(rng, 1, 3)));
        states->insert(state->getId(), state);
    }

    int first_weight = 0;
    int second_weight = 1;
    if (child_kind == 2) {
        first_weight = 1;
        second_weight = 0;
    } else if (child_kind == 3) {
        first_weight = 2;
        second_weight = 3;
    }

    add_edge(alphabet->at(0), weight_by_value[first_weight], states->at(0), states->at(1));
    add_edge(alphabet->at(1), weight_by_value[second_weight], states->at(1), states->at(2));

    const int extra_edges = 0;
    const int weight_count = static_cast<int>(sizeof(CHILD_WEIGHT_VALUES) / sizeof(CHILD_WEIGHT_VALUES[0]));
    for (int i = 0; i < extra_edges; ++i) {
        State* from = states->at(static_cast<unsigned int>(rand_int(rng, 0, state_count - 1)));
        State* to = states->at(static_cast<unsigned int>(rand_int(rng, 0, state_count - 1)));
        Symbol* symbol = alphabet->at(static_cast<unsigned int>(rand_int(rng, 0, alphabet->size() - 1)));
        int weight_value = CHILD_WEIGHT_VALUES[rand_int(rng, 0, weight_count - 1)];
        add_edge(symbol, weight_by_value[weight_value], from, to);
    }

    return new ChildAutomaton(name,
                              alphabet,
                              states,
                              weights,
                              weight_t(0),
                              weight_t(3),
                              states->at(0));
}

NestedAutomaton* make_case(int case_index, std::mt19937& rng) {
    const unsigned int alphabet_size = 2;
    MapArray<Symbol*>* alphabet = make_parent_alphabet(alphabet_size);

    MapArray<Weight*>* parent_weights = make_parent_weights();

    const unsigned int parent_state_count = static_cast<unsigned int>(rand_int(rng, 2, 3));
    State::RESET();
    MapArray<State*>* parent_states = new MapArray<State*>(parent_state_count);
    for (unsigned int i = 0; i < parent_state_count; ++i) {
        State* state = new State("p" + std::to_string(i),
                                 alphabet->size(),
                                 weight_t(0),
                                 weight_t(REAL_CHILDREN));
        state->setFinal(i == 0 || (i > 1 && rand_chance(rng, 1, 4)));
        parent_states->insert(state->getId(), state);
    }

    for (int child = 1; child <= REAL_CHILDREN; ++child) {
        add_edge(alphabet->at(0), parent_weights->at(static_cast<unsigned int>(child)),
                 parent_states->at(0), parent_states->at(1));
    }
    add_edge(alphabet->at(1), parent_weights->at(0), parent_states->at(1), parent_states->at(0));

    const int extra_parent_edges = 0;
    for (int i = 0; i < extra_parent_edges; ++i) {
        State* from = parent_states->at(static_cast<unsigned int>(rand_int(rng, 0, parent_state_count - 1)));
        State* to = parent_states->at(static_cast<unsigned int>(rand_int(rng, 0, parent_state_count - 1)));
        Symbol* symbol = alphabet->at(static_cast<unsigned int>(rand_int(rng, 0, alphabet_size - 1)));
        Weight* weight = parent_weights->at(static_cast<unsigned int>(rand_int(rng, 0, REAL_CHILDREN)));
        add_edge(symbol, weight, from, to);
    }

    MapArray<ChildAutomaton*>* children = new MapArray<ChildAutomaton*>(REAL_CHILDREN + 1);
    for (int child = 1; child <= REAL_CHILDREN; ++child) {
        children->insert(static_cast<unsigned int>(child),
                         make_child("child" + std::to_string(child),
                                    alphabet,
                                    child,
                                    rng));
    }

    return new NestedAutomaton("split_final_random_" + std::to_string(case_index),
                               alphabet,
                               parent_states,
                               parent_weights,
                               weight_t(0),
                               weight_t(REAL_CHILDREN),
                               parent_states->at(0),
                               children);
}

bool evaluate_regular(NestedAutomaton* nwa,
                      value_function_t infVal,
                      value_function_t finVal,
                      weight_t threshold) {
    const weight_t bound = (finVal == SumB) ? SUMB_BOUND : weight_t(-1);
    Automaton* flat = nwa->flatten_regular(finVal, bound);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, threshold);
    delete non_silent;
    delete flat;
    return result;
}

bool evaluate_threshold(NestedAutomaton* nwa,
                        ThresholdFlattenFn flatten,
                        value_function_t infVal,
                        value_function_t finVal,
                        weight_t threshold) {
    Automaton* flat = flatten(nwa, finVal, threshold);
    Automaton* non_silent = Automaton::removeSilentTransitions(flat, infVal, false);
    const bool result = non_silent->isNonEmpty_withFinal(infVal, weight_t(1));
    delete non_silent;
    delete flat;
    return result;
}

void require_same(bool original,
                  bool split,
                  int case_index,
                  const std::string& backend,
                  value_function_t infVal,
                  value_function_t finVal,
                  weight_t threshold) {
    if (original == split) {
        return;
    }

    std::stringstream err;
    err << "split-final mismatch"
        << " seed=" << RNG_SEED
        << " case=" << case_index
        << " backend=" << backend
        << " inf=" << infValToString(infVal)
        << " fin=" << finValToString(finVal)
        << " threshold=" << threshold
        << " original=" << original
        << " split=" << split;
    throw std::runtime_error(err.str());
}

struct Coverage {
    int true_results = 0;
    int false_results = 0;

    void observe(bool value) {
        if (value) {
            ++true_results;
        } else {
            ++false_results;
        }
    }
};

void compare_regular_case(NestedAutomaton* original,
                          NestedAutomaton* split,
                          int case_index,
                          Coverage& coverage) {
    const value_function_t inf_modes[] = {Inf, Sup, LimInf, LimSup};
    struct Case {
        const char* label;
        value_function_t fin;
        weight_t threshold;
    };
    const Case cases[] = {
        {"regular_max_high", Max_f, weight_t(1)},
        {"regular_max_unreachable", Max_f, weight_t(4)},
        {"regular_min_high", Min_f, weight_t(1)},
        {"regular_sumb", SumB, weight_t(1)},
    };

    for (value_function_t infVal : inf_modes) {
        for (const Case& c : cases) {
            const bool original_result = evaluate_regular(original, infVal, c.fin, c.threshold);
            const bool split_result = evaluate_regular(split, infVal, c.fin, c.threshold);
            coverage.observe(original_result);
            require_same(original_result, split_result, case_index, c.label, infVal, c.fin, c.threshold);
        }
    }
}

void compare_threshold_case(NestedAutomaton* original,
                            NestedAutomaton* split,
                            int case_index,
                            Coverage& coverage,
                            const std::string& label,
                            ThresholdFlattenFn flatten,
                            const value_function_t* inf_modes,
                            size_t inf_count,
                            const value_function_t* fin_modes,
                            size_t fin_count,
                            const weight_t* thresholds,
                            size_t threshold_count) {
    for (size_t i = 0; i < inf_count; ++i) {
        for (size_t f = 0; f < fin_count; ++f) {
            for (size_t t = 0; t < threshold_count; ++t) {
                const bool original_result = evaluate_threshold(
                    original, flatten, inf_modes[i], fin_modes[f], thresholds[t]);
                const bool split_result = evaluate_threshold(
                    split, flatten, inf_modes[i], fin_modes[f], thresholds[t]);
                coverage.observe(original_result);
                require_same(original_result, split_result, case_index, label,
                             inf_modes[i], fin_modes[f], thresholds[t]);
            }
        }
    }
}

void compare_case(NestedAutomaton* original,
                  NestedAutomaton* split,
                  int case_index,
                  Coverage& coverage) {
    compare_regular_case(original, split, case_index, coverage);

    const value_function_t sup_modes[] = {Sup, LimSup};
    const value_function_t inf_modes[] = {Inf, LimInf};
    const value_function_t minmax_fins[] = {Max_f, Min_f};
    const value_function_t sum_fins[] = {SumPlus, SumMinus};
    const weight_t minmax_thresholds[] = {weight_t(1), weight_t(4)};
    const weight_t sum_thresholds[] = {weight_t(1), weight_t(5)};

    compare_threshold_case(original, split, case_index, coverage,
                           "threshold_minmax_sup",
                           &NestedAutomatonTester::flatten_MinMax_Sup,
                           sup_modes, 2, minmax_fins, 2, minmax_thresholds, 2);
    compare_threshold_case(original, split, case_index, coverage,
                           "threshold_minmax_inf",
                           &NestedAutomatonTester::flatten_MinMax_Inf,
                           inf_modes, 2, minmax_fins, 2, minmax_thresholds, 2);
    compare_threshold_case(original, split, case_index, coverage,
                           "threshold_sum_sup",
                           &NestedAutomatonTester::flatten_SumPlusMinus_Sup,
                           sup_modes, 2, sum_fins, 2, sum_thresholds, 2);
    compare_threshold_case(original, split, case_index, coverage,
                           "threshold_sum_inf",
                           &NestedAutomatonTester::flatten_SumPlusMinus_Inf,
                           inf_modes, 2, sum_fins, 2, sum_thresholds, 2);

    compare_threshold_case(original, split, case_index, coverage,
                           "cached_minmax_sup",
                           &NestedAutomatonTester::flatten_MinMax_Sup_cached,
                           sup_modes, 2, minmax_fins, 2, minmax_thresholds, 2);
    compare_threshold_case(original, split, case_index, coverage,
                           "cached_minmax_witness_sup",
                           &NestedAutomatonTester::flatten_MinMax_Sup_witness_cached,
                           sup_modes, 2, minmax_fins, 2, minmax_thresholds, 2);
    compare_threshold_case(original, split, case_index, coverage,
                           "cached_minmax_inf",
                           &NestedAutomatonTester::flatten_MinMax_Inf_cached,
                           inf_modes, 2, minmax_fins, 2, minmax_thresholds, 2);
    compare_threshold_case(original, split, case_index, coverage,
                           "cached_sum_inf",
                           &NestedAutomatonTester::flatten_SumPlusMinus_Inf_cached,
                           inf_modes, 2, sum_fins, 2, sum_thresholds, 2);
}

} // namespace

void test_randomized_split_final_differential() {
    std::mt19937 rng(RNG_SEED);
    Coverage coverage;

    for (int case_index = 0; case_index < NUM_CASES; ++case_index) {
        std::unique_ptr<NestedAutomaton> original(make_case(case_index, rng));
        std::unique_ptr<NestedAutomaton> split(
            NestedAutomatonTester::split_child_finals_for_testing(original.get()));

        compare_case(original.get(), split.get(), case_index, coverage);
    }

    TEST_ASSERT(coverage.true_results > 0,
                "randomized split-final suite should exercise accepting decisions");
    TEST_ASSERT(coverage.false_results > 0,
                "randomized split-final suite should exercise rejecting decisions");

    std::cout << "    cases=" << NUM_CASES
              << ", true decisions=" << coverage.true_results
              << ", false decisions=" << coverage.false_results
              << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "CORRECTNESS TESTS: split-final differential" << std::endl;
    std::cout << "========================================" << std::endl;

    RUN_TEST(test_randomized_split_final_differential);

    printTestSummary();

    return g_test_results.empty() ? 0 :
           (std::all_of(g_test_results.begin(), g_test_results.end(),
                        [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
