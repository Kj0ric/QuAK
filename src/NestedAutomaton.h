#ifndef NESTED_AUTOMATON_H_
#define NESTED_AUTOMATON_H_

#include <unordered_set>
#include <vector>
#include "Automaton.h"
#include "ChildAutomaton.h"

// Forward declaration for test access
class NestedAutomatonTester;

struct MinMaxInfExperimentStats {
	uint64_t state_map_lookup_calls = 0;
	uint64_t state_map_insert_calls = 0;

	uint64_t spawn_calls = 0;
	uint64_t unique_spawn_keys = 0;

	uint64_t step_bag_calls = 0;
	uint64_t unique_bag_step_keys = 0;
	uint64_t step_bag_cache_hits = 0;

	uint64_t step_obl_calls = 0;
	uint64_t step_obl_cache_hits = 0;

	uint64_t bag_add_calls = 0;
	uint64_t bag_add_cache_hits = 0;

	uint64_t bag_copy_ops = 0;
	uint64_t bag_copy_entries = 0;

	uint64_t frontier_observations = 0;
	uint64_t frontier_config_total = 0;
	uint64_t frontier_capacity_total = 0;

	uint64_t unique_obligation_count = 0;
	uint64_t unique_bag_count = 0;

	double time_step_bag_ms = 0.0;
	double time_state_map_ms = 0.0;
	double time_bag_copy_ms = 0.0;
};

class NestedAutomaton : public Automaton {
	// Allow test class to access private members
	friend class NestedAutomatonTester;

private:
	MapArray<ChildAutomaton*>* children_;

	NestedAutomaton(const Automaton* parent, MapArray<ChildAutomaton*>* children);


	bool allParentStatesFinal() const;
	SetStd<weight_t> computeChildReturnValuesParentAware(size_t child_index, value_function_t finVal, weight_t bound);
	SetStd<weight_t> computeChildReturnValues(ChildAutomaton* child, value_function_t finVal, weight_t bound);
	bool childWeightsNeedProjection(value_function_t finVal) const;
	NestedAutomaton* projectChildWeightsForAggregator(value_function_t finVal) const;
	NestedAutomaton* splitContinuableChildFinals() const;
	void validateNested() const;

	static void setMinMaxInfExperimentStatsEnabled(bool enabled);
	static void resetMinMaxInfExperimentStats();
	static MinMaxInfExperimentStats getMinMaxInfExperimentStats();

	// Ensures child 0 exists with a default trivial automaton if missing
	void ensureChild0Exists();

public:
	virtual ~NestedAutomaton();
	NestedAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register);
	NestedAutomaton(std::string filename, Automaton* other = nullptr);
	NestedAutomaton(std::string name, MapArray<Symbol*>* alphabet, MapArray<State*>* states,
	                MapArray<Weight*>* weights, weight_t min_domain, weight_t max_domain,
	                State* initial, MapArray<ChildAutomaton*>* children);

	void print(bool full = false, bool bv_weights = false, bool bv_only = false) const;
	void print(std::ostream& out, bool full = false, bool bv_weights = false, bool bv_only = false) const;

	static NestedAutomaton* removeSilentTransitions(const NestedAutomaton* A, value_function_t f);
	std::size_t getChildrenSize() const;
	ChildAutomaton* getChild(std::size_t index) const;

	std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual> generateMacroAlphabet();
	NestedAutomaton* determinizeWithMacroAlphabet();
	NestedAutomaton* synchronizeChildren();
	Automaton* flatten_Avg_SumMinus(uint64_t c_bound);
	Automaton* flatten_regular(value_function_t finVal, weight_t bound = -1);
	Automaton* flatten_SumPlusMinus_Sup(value_function_t finite_aggregator, weight_t threshold);
		Automaton* flatten_SumPlusMinus_Sup_witness_cached(value_function_t finite_aggregator, weight_t threshold);
		Automaton* flatten_SumPlusMinus_Inf(value_function_t finite_aggregator, weight_t threshold);
		Automaton* flatten_SumPlusMinus_Inf_cached(value_function_t finite_aggregator, weight_t threshold);
		Automaton* flatten_MinMax_Sup(value_function_t finite_aggregator, weight_t threshold);
		Automaton* flatten_MinMax_Sup_cached(value_function_t finite_aggregator, weight_t threshold);
		Automaton* flatten_MinMax_Sup_witness_cached(value_function_t finite_aggregator, weight_t threshold);
		Automaton* flatten_MinMax_Inf(value_function_t finite_aggregator, weight_t threshold);
		Automaton* flatten_MinMax_Inf_cached(value_function_t finite_aggregator, weight_t threshold);


	NestedAutomaton* makeCompleteNested(std::vector<bool>* complete_flags = nullptr,
	                                    weight_t parent_sink_w = weight_t(0),
	                                    weight_t child_sink_w = weight_t(0)) const;

	bool isDeterministicNested() const;
	bool isCompleteNested(std::vector<bool>* complete_flags = nullptr) const;
	bool isDeterministicAndCompleteNested() const;
	bool isNonEmpty(value_function_t infVal, value_function_t finVal, weight_t x, weight_t bound = -1);
	bool isUniversal(value_function_t infVal, value_function_t finVal, weight_t x, weight_t bound = -1);
};

const weight_t INIT_BUCHI_VALUE = 0;

weight_t applyBound(weight_t value, weight_t bound);

#endif
