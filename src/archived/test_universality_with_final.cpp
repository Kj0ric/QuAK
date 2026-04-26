#include "test_correctness_common.h"
#include "../../FORKLIFT/inclusion.h"

#include <algorithm>
#include <vector>

namespace {

void reset_ids() {
	Symbol::RESET();
	Weight::RESET();
	State::RESET();
}

void add_edge(Symbol* symbol, Weight* weight, State* from, State* to) {
	Edge* edge = new Edge(symbol, weight, from, to);
	from->addSuccessor(edge);
	to->addPredecessor(edge);
}

Automaton* make_automaton(
	const std::string& name,
	const std::vector<Symbol*>& symbols,
	const std::vector<Weight*>& weights,
	const std::vector<State*>& states,
	State* initial
) {
	auto* alphabet = new MapArray<Symbol*>(static_cast<unsigned int>(symbols.size()));
	for (Symbol* symbol : symbols) {
		alphabet->insert(symbol->getId(), symbol);
	}

	auto* weightMap = new MapArray<Weight*>(static_cast<unsigned int>(weights.size()));
	weight_t minWeight = weights.front()->getValue();
	weight_t maxWeight = weights.front()->getValue();
	for (Weight* weight : weights) {
		weightMap->insert(weight->getId(), weight);
		minWeight = std::min(minWeight, weight->getValue());
		maxWeight = std::max(maxWeight, weight->getValue());
	}

	auto* stateMap = new MapArray<State*>(static_cast<unsigned int>(states.size()));
	for (State* state : states) {
		stateMap->insert(state->getId(), state);
	}

	return new Automaton(name, alphabet, stateMap, weightMap, minWeight, maxWeight, initial);
}

Automaton* build_partial_domain_accepting_loop() {
	reset_ids();
	auto* a = new Symbol("a");
	auto* b = new Symbol("b");
	auto* zero = new Weight(weight_t(0));
	auto* one = new Weight(weight_t(1));

	auto* q0 = new State("q0", 2, zero->getValue(), one->getValue());
	q0->setFinal(true);
	auto* dead = new State("dead", 2, zero->getValue(), one->getValue());
	dead->setFinal(false);

	add_edge(b, one, q0, q0);
	add_edge(a, zero, q0, dead);
	add_edge(a, zero, dead, dead);
	add_edge(b, zero, dead, dead);

	return make_automaton("partial_domain_accepting_loop", {a, b}, {zero, one}, {q0, dead}, q0);
}

Automaton* build_empty_domain_loop() {
	reset_ids();
	auto* a = new Symbol("a");
	auto* zero = new Weight(weight_t(0));

	auto* q0 = new State("q0", 1, zero->getValue(), zero->getValue());
	q0->setFinal(false);
	add_edge(a, zero, q0, q0);

	return make_automaton("empty_domain_loop", {a}, {zero}, {q0}, q0);
}

Automaton* build_low_accepting_loop() {
	reset_ids();
	auto* a = new Symbol("a");
	auto* zero = new Weight(weight_t(0));

	auto* q0 = new State("q0", 1, zero->getValue(), zero->getValue());
	q0->setFinal(true);
	add_edge(a, zero, q0, q0);

	return make_automaton("low_accepting_loop", {a}, {zero}, {q0}, q0);
}

Automaton* build_nondet_best_run_loop() {
	reset_ids();
	auto* a = new Symbol("a");
	auto* low = new Weight(weight_t(0));
	auto* high = new Weight(weight_t(2));

	auto* q0 = new State("q0", 1, low->getValue(), high->getValue());
	q0->setFinal(true);
	add_edge(a, low, q0, q0);
	add_edge(a, high, q0, q0);

	return make_automaton("nondet_best_run_loop", {a}, {low, high}, {q0}, q0);
}

Automaton* build_transient_final_high_nonfinal_loop() {
	reset_ids();
	auto* a = new Symbol("a");
	auto* one = new Weight(weight_t(1));

	auto* q0 = new State("q0", 1, one->getValue(), one->getValue());
	q0->setFinal(true);
	auto* q1 = new State("q1", 1, one->getValue(), one->getValue());
	q1->setFinal(false);

	add_edge(a, one, q0, q1);
	add_edge(a, one, q1, q1);

	return make_automaton("transient_final_high_nonfinal_loop", {a}, {one}, {q0, q1}, q0);
}

Automaton* build_combined_final_and_threshold_cycles() {
	reset_ids();
	auto* a = new Symbol("a");
	auto* low = new Weight(weight_t(0));
	auto* high = new Weight(weight_t(2));

	auto* q0 = new State("q0", 1, low->getValue(), high->getValue());
	q0->setFinal(true);
	auto* q1 = new State("q1", 1, low->getValue(), high->getValue());
	q1->setFinal(false);

	add_edge(a, low, q0, q0);
	add_edge(a, low, q0, q1);
	add_edge(a, high, q1, q1);
	add_edge(a, low, q1, q0);

	return make_automaton("combined_final_and_threshold_cycles", {a}, {low, high}, {q0, q1}, q0);
}

Automaton* build_low_accepting_run_with_high_rejected_run() {
	reset_ids();
	auto* a = new Symbol("a");
	auto* low = new Weight(weight_t(0));
	auto* high = new Weight(weight_t(1));

	auto* q0 = new State("q0", 1, low->getValue(), high->getValue());
	q0->setFinal(true);
	auto* q1 = new State("q1", 1, low->getValue(), high->getValue());
	q1->setFinal(false);

	add_edge(a, low, q0, q0);
	add_edge(a, high, q0, q1);
	add_edge(a, high, q1, q1);

	return make_automaton("low_accepting_run_with_high_rejected_run", {a}, {low, high}, {q0, q1}, q0);
}

} // namespace

void test_partial_domain_ignores_rejected_words() {
	Automaton* A = build_partial_domain_accepting_loop();

	TEST_ASSERT_TRUE(
		A->isUniversal_withFinal(LimSup, weight_t(1)),
		"accepted-domain universality should ignore rejected a^omega"
	);
	TEST_ASSERT_FALSE(
		A->isUniversal(LimSup, weight_t(1)),
		"ordinary universality should still quantify over all words"
	);
	TEST_ASSERT_FALSE(
		A->isUniversal_withFinal(LimSup, weight_t(2)),
		"accepted b^omega has LimSup value 1, not 2"
	);

	delete A;
}

void test_empty_domain_is_vacuously_universal() {
	Automaton* A = build_empty_domain_loop();

	TEST_ASSERT_TRUE(
		A->isUniversal_withFinal(LimSup, weight_t(100)),
		"accepted-domain universality should be vacuous for an empty language"
	);

	delete A;
}

void test_low_accepting_loop_fails_above_value() {
	Automaton* A = build_low_accepting_loop();

	TEST_ASSERT_TRUE(
		A->isUniversal_withFinal(LimSup, weight_t(0)),
		"final loop with value 0 should satisfy threshold 0"
	);
	TEST_ASSERT_FALSE(
		A->isUniversal_withFinal(LimSup, weight_t(1)),
		"final loop with value 0 should not satisfy threshold 1"
	);

	delete A;
}

void test_nondeterministic_best_accepted_word_value_semantics() {
	Automaton* A = build_nondet_best_run_loop();

	TEST_ASSERT_TRUE(
		A->isUniversal_withFinal(LimSup, weight_t(2)),
		"accepted word can realize value 2 via the high self-loop"
	);
	TEST_ASSERT_FALSE(
		A->isUniversal_withFinal(LimSup, weight_t(3)),
		"no accepted word has value at least threshold 3"
	);

	delete A;
}

void test_forklift_membership_rejects_transient_final_before_high_loop() {
	Automaton* A = build_transient_final_high_nonfinal_loop();
	Word stem;
	Word period(A->getAlphabet()->at(0));

	TEST_ASSERT_FALSE(
		membership(A, &stem, &period, weight_t(1)),
		"high loop reached after a transient final is not an accepting lasso"
	);

	delete A;
}

void test_forklift_membership_combines_final_and_threshold_cycles() {
	Automaton* A = build_combined_final_and_threshold_cycles();
	Word stem;
	Word period(A->getAlphabet()->at(0));

	TEST_ASSERT_TRUE(
		membership(A, &stem, &period, weight_t(2)),
		"same product SCC may combine a final cycle and a separate threshold cycle"
	);

	TEST_ASSERT_TRUE(
		A->isUniversal_withFinal(LimSup, weight_t(2)),
		"accepted-domain universality should also use the combined high accepting cycle"
	);

	delete A;
}

void test_universality_with_final_rejects_high_nonaccepting_run() {
	Automaton* A = build_low_accepting_run_with_high_rejected_run();

	TEST_ASSERT_FALSE(
		A->isUniversal_withFinal(LimSup, weight_t(1)),
		"rejected high path must not witness the target value"
	);

	delete A;
}

int main() {
	std::cout << "========================================" << std::endl;
	std::cout << "CORRECTNESS TESTS: universality with final" << std::endl;
	std::cout << "========================================" << std::endl;

	RUN_TEST(test_partial_domain_ignores_rejected_words);
	RUN_TEST(test_empty_domain_is_vacuously_universal);
	RUN_TEST(test_low_accepting_loop_fails_above_value);
	RUN_TEST(test_nondeterministic_best_accepted_word_value_semantics);
	RUN_TEST(test_forklift_membership_rejects_transient_final_before_high_loop);
	RUN_TEST(test_forklift_membership_combines_final_and_threshold_cycles);
	RUN_TEST(test_universality_with_final_rejects_high_nonaccepting_run);

	printTestSummary();

	return g_test_results.empty() ? 0 :
	       (std::all_of(g_test_results.begin(), g_test_results.end(),
	                    [](const TestResult& r) { return r.passed; }) ? 0 : 1);
}
