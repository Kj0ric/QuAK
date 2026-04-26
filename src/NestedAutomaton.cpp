#include <cassert>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <queue>
#include <sstream>
#include <cmath>
#include <functional>
#include <chrono>
#include <atomic>
#include <set>

#include "NestedAutomaton.h"
#include "Edge.h"
#include "utility.h"
#include "FORKLIFT/inclusion.h"

NestedAutomaton::~NestedAutomaton() {
    if (children_ != nullptr) {
        for (size_t i = 0; i < children_->size(); ++i) {
            delete children_->at(i);
        }
        delete children_;
    }
}

// Helper to ensure child 0 exists with a default trivial automaton
// Creates a single state (initial and final) with empty alphabet
void NestedAutomaton::ensureChild0Exists() {
    if (children_ == nullptr || children_->size() == 0) {
        return;  // No children array, nothing to fix
    }

    // Check if child 0 already exists
    if (children_->at(0) != nullptr) {
        return;  // Child 0 exists, nothing to do
    }

    // Match parser behavior for @CHILD 0: empty alphabet
    MapArray<Symbol*>* child_alphabet = new MapArray<Symbol*>(0);

    // Match parser behavior: single "dummy" state that is both initial and final
    // Reset state counter so the dummy state gets ID 0 (matching its array index)
    State::RESET();
    MapArray<State*>* child_states = new MapArray<State*>(1);
    State* dummy_state = new State("dummy", 0, weight_t(0), weight_t(0));
    dummy_state->setFinal(true);
    child_states->insert(dummy_state->getId(), dummy_state);

    // No transitions (empty weights)
    MapArray<Weight*>* child_weights = new MapArray<Weight*>(0);

    // Create the dummy child automaton matching parser behavior
    ChildAutomaton* default_child = new ChildAutomaton(
        "child0_default",
        child_alphabet,
        child_states,
        child_weights,
        weight_t(0),  // min_domain
        weight_t(0),  // max_domain
        dummy_state   // initial state
    );

    // Insert at position 0
    children_->insert(0, default_child);
}

NestedAutomaton::NestedAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register)
    : Automaton(name, parser, sync_register)
{
    children_ = new MapArray<ChildAutomaton*>(parser->child_parsers.size());
    for (unsigned i = 0; i < parser->child_parsers.size(); ++i) {
        Parser* child_parser = parser->child_parsers[i];
        if (child_parser) {
            auto* child = new ChildAutomaton(std::to_string(i), child_parser, sync_register);
            children_->insert(i, child);
        }
    }
    ensureChild0Exists();
}

NestedAutomaton::NestedAutomaton(std::string filename, Automaton* other)
    : Automaton(filename, other)
{
    Parser* parser = new Parser(filename);

    MapStd<std::string, Symbol*> sync_register;
    if (other != nullptr) {
        for (unsigned int symbol_id = 0; symbol_id < other->getAlphabet()->size(); ++symbol_id) {
            Symbol* symbol = other->getAlphabet()->at(symbol_id);
            sync_register.insert(symbol->getName(), symbol);
        }
    }

    children_ = new MapArray<ChildAutomaton*>(parser->child_parsers.size());
    for (unsigned i = 0; i < parser->child_parsers.size(); ++i) {
        Parser* child_parser = parser->child_parsers[i];
        if (child_parser) {
            auto* child = new ChildAutomaton(std::to_string(i), child_parser, sync_register);
            children_->insert(i, child);
        }
    }
    delete parser;
    ensureChild0Exists();
}

NestedAutomaton::NestedAutomaton(const Automaton* parent, MapArray<ChildAutomaton*>* children)
    : Automaton(*parent),
      children_(children) {
    this->setName(parent->getName() + "_noSilent");
    ensureChild0Exists();
}

NestedAutomaton::NestedAutomaton(std::string name,
                                 MapArray<Symbol*>* alphabet,
                                 MapArray<State*>* states,
                                 MapArray<Weight*>* weights,
                                 weight_t min_domain,
                                 weight_t max_domain,
                                 State* initial,
                                 MapArray<ChildAutomaton*>* children)
  : Automaton(name + "_parent", alphabet, states, weights, min_domain, max_domain, initial),
    children_(children) {
    ensureChild0Exists();
}

NestedAutomaton* NestedAutomaton::removeSilentTransitions(const NestedAutomaton* A, value_function_t f) {
    Automaton* transformed_parent = Automaton::removeSilentTransitions(A, f);

    MapArray<ChildAutomaton*>* copied_children = new MapArray<ChildAutomaton*>(A->children_->size());
    for (unsigned i = 0; i < A->children_->size(); ++i) {
        if (A->children_->at(i)) {
            copied_children->insert(i, new ChildAutomaton(*A->children_->at(i)));
        }
    }

    NestedAutomaton* result = new NestedAutomaton(transformed_parent, copied_children);
    delete transformed_parent;
    return result;
}

static bool child_state_has_outgoing(const ChildAutomaton* child, State* st) {
    if (!child || !st || !child->getAlphabet()) return false;

    for (unsigned int symbol_id = 0; symbol_id < child->getAlphabet()->size(); ++symbol_id) {
        SetStd<Edge*>* succs = st->getSuccessors(symbol_id);
        if (succs && !succs->empty()) return true;
    }

    return false;
}

static bool child_has_splittable_final(const ChildAutomaton* child) {
    if (!child || !child->getStates()) return false;

    for (unsigned int sid = 0; sid < child->getStates()->size(); ++sid) {
        State* st = child->getStates()->at(sid);
        if (st && st->getFinal() && child_state_has_outgoing(child, st)) {
            return true;
        }
    }

    return false;
}

static void add_copied_edge(Symbol* symbol, Weight* weight, State* from, State* to) {
    Edge* edge = new Edge(symbol, weight, from, to);
    from->addSuccessor(edge);
    to->addPredecessor(edge);
}

static ChildAutomaton* copy_child_with_continuable_final_split(const ChildAutomaton* child) {
    if (!child) return nullptr;

    MapArray<Symbol*>* alphabet = new MapArray<Symbol*>(child->getAlphabet()->size());
    Symbol::RESET();
    for (unsigned int i = 0; i < child->getAlphabet()->size(); ++i) {
        Symbol* symbol = new Symbol(child->getAlphabet()->at(i));
        alphabet->insert(symbol->getId(), symbol);
    }

    MapArray<Weight*>* weights = new MapArray<Weight*>(child->getWeights()->size());
    Weight::RESET();
    for (unsigned int i = 0; i < child->getWeights()->size(); ++i) {
        Weight* weight = new Weight(child->getWeights()->at(i));
        weights->insert(weight->getId(), weight);
    }

    const unsigned int old_state_count = child->getStates()->size();
    std::vector<uint8_t> splittable(old_state_count, 0u);
    unsigned int new_state_count = 0;

    for (unsigned int sid = 0; sid < old_state_count; ++sid) {
        State* old_state = child->getStates()->at(sid);
        const bool split = old_state && old_state->getFinal() &&
                           child_state_has_outgoing(child, old_state);
        splittable[sid] = split ? 1u : 0u;
        new_state_count += split ? 2u : 1u;
    }

    MapArray<State*>* states = new MapArray<State*>(new_state_count);
    std::vector<State*> normal_copy(old_state_count, nullptr);
    std::vector<State*> stop_copy(old_state_count, nullptr);
    std::vector<State*> cont_copy(old_state_count, nullptr);

    State::RESET();
    for (unsigned int sid = 0; sid < old_state_count; ++sid) {
        State* old_state = child->getStates()->at(sid);
        if (!old_state) continue;

        if (splittable[sid]) {
            State* stop = new State(old_state->getName() + "_stop",
                                    alphabet->size(),
                                    child->getMinDomain(),
                                    child->getMaxDomain());
            stop->setFinal(true);
            states->insert(stop->getId(), stop);
            stop_copy[sid] = stop;

            State* cont = new State(old_state->getName() + "_cont",
                                    alphabet->size(),
                                    child->getMinDomain(),
                                    child->getMaxDomain());
            cont->setFinal(false);
            states->insert(cont->getId(), cont);
            cont_copy[sid] = cont;
        } else {
            State* copy = new State(old_state->getName(),
                                    alphabet->size(),
                                    child->getMinDomain(),
                                    child->getMaxDomain());
            copy->setFinal(old_state->getFinal());
            states->insert(copy->getId(), copy);
            normal_copy[sid] = copy;
        }
    }

    auto source_for_old_state = [&](State* old_from) -> State* {
        const unsigned int sid = old_from->getId();
        return splittable[sid] ? cont_copy[sid] : normal_copy[sid];
    };

    auto targets_for_old_target = [&](State* old_to) -> std::vector<State*> {
        const unsigned int sid = old_to->getId();
        if (splittable[sid]) {
            return {stop_copy[sid], cont_copy[sid]};
        }
        return {normal_copy[sid]};
    };

    State* initial = source_for_old_state(child->getInitial());

    for (unsigned int sid = 0; sid < old_state_count; ++sid) {
        State* old_from = child->getStates()->at(sid);
        if (!old_from) continue;

        State* new_from = source_for_old_state(old_from);
        if (!new_from) continue;

        for (unsigned int symbol_id = 0; symbol_id < child->getAlphabet()->size(); ++symbol_id) {
            SetStd<Edge*>* succs = old_from->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* old_edge : *succs) {
                Symbol* new_symbol = alphabet->at(old_edge->getSymbol()->getId());
                Weight* new_weight = weights->at(old_edge->getWeight()->getId());

                for (State* new_to : targets_for_old_target(old_edge->getTo())) {
                    if (new_to) {
                        add_copied_edge(new_symbol, new_weight, new_from, new_to);
                    }
                }
            }
        }
    }

    return new ChildAutomaton(child->getName() + "_split_continuable_finals",
                              alphabet,
                              states,
                              weights,
                              child->getMinDomain(),
                              child->getMaxDomain(),
                              initial);
}

NestedAutomaton* NestedAutomaton::splitContinuableChildFinals() const {
    if (!children_) return nullptr;

    std::vector<uint8_t> needs_split(children_->size(), 0u);
    bool any_split = false;

    for (unsigned int child_id = 1; child_id < children_->size(); ++child_id) {
        ChildAutomaton* child = children_->at(child_id);
        if (child_has_splittable_final(child)) {
            needs_split[child_id] = 1u;
            any_split = true;
        }
    }

    if (!any_split) return nullptr;

    MapArray<ChildAutomaton*>* copied_children =
        new MapArray<ChildAutomaton*>(children_->size());

    for (unsigned int child_id = 0; child_id < children_->size(); ++child_id) {
        ChildAutomaton* child = children_->at(child_id);
        if (!child) continue;

        ChildAutomaton* copied = needs_split[child_id]
            ? copy_child_with_continuable_final_split(child)
            : new ChildAutomaton(*child);
        copied_children->insert(child_id, copied);
    }

    NestedAutomaton* result = new NestedAutomaton(this, copied_children);
    result->setName(this->name + "_splitContinuableFinals");
    return result;
}

void NestedAutomaton::print(bool full, bool bv_weights, bool bv_only) const {
    print(std::cout, full, bv_weights, bv_only);
}

void NestedAutomaton::print(std::ostream& out, bool full, bool bv_weights, bool bv_only) const {
    out << "(1) NESTED AUTOMATON (" << this->getName() << "):\n";
    Automaton::print(out);

    if (children_ && children_->size() > 0) {
        out << "(2) CHILD AUTOMATA:" << std::endl;
        for (unsigned i = 0; i < children_->size(); ++i) {
            ChildAutomaton* child = children_->at(i);
            if (child) {
                out << "[Child " << i << "]" << std::endl;
                child->print(out);
            }
        }
    } else {
        out << "The nested automaton (" << this->getName() << ") has no child automata." << std::endl;
    }
}

std::size_t NestedAutomaton::getChildrenSize() const {
	return children_ ? children_->size() : 0;
}

ChildAutomaton* NestedAutomaton::getChild(std::size_t index) const {
	if (!children_ || index >= children_->size()) {
		return nullptr;
	}
	return children_->at(index);
}




weight_t applyBound(weight_t value, weight_t bound) {
    if (value > bound) {
        return bound;
    } else if (value < -bound) {
        return -bound;
    } else {
        return value;
    }
}

static inline size_t edgeWeightToChildIndex(const weight_t& w) {
    float f = w.to_float();
    if (f <= 0.0f) return 0;
    return static_cast<size_t>(f);
}

static std::vector<bool> computeParentGoodMask(const NestedAutomaton* nwa) {
    MapArray<State*>* states = nwa->getStates();
    const size_t n = states->size();
    const size_t A = nwa->getAlphabetSize();
    const unsigned int nbSCC = nwa->nb_SCCs;

    std::vector<bool> good(n, false);

    // Identify "proper accepting SCCs" = SCC with a final state AND a directed cycle
    std::vector<int> proper_accepting_scc(nbSCC, -1);

    for (size_t sid = 0; sid < n; ++sid) {
        State* s = states->at(sid);
        int cid = s->getTag();

        if (proper_accepting_scc[cid] > -1) continue;
        if (!nwa->final_SCCs[cid]) continue;

        for (size_t a = 0; a < A; ++a) {
            SetStd<Edge*>* succs = s->getSuccessors(a);
            for (Edge* e : *succs) {
                int tid_i = e->getTo()->getTag();
                if (tid_i == cid) {
                    proper_accepting_scc[cid] = 1;
                    break;
                }
            }
            if (proper_accepting_scc[cid] > 0) break;
        }

        if (proper_accepting_scc[cid] < 0) {
            proper_accepting_scc[cid] = 0;
        }
    }

    // If there is no proper accepting SCC, nothing is "good"
    bool any_acc = false;
    for (unsigned int cid = 0; cid < nbSCC; ++cid) {
        if (proper_accepting_scc[cid] > 0) { any_acc = true; break; }
    }
    if (!any_acc) return good;

    // Build reverse SCC DAG
    std::vector<std::vector<int>> radj_scc(nbSCC);
    radj_scc.reserve(nbSCC);

    for (size_t sid = 0; sid < n; ++sid) {
        State* s = states->at(sid);
        int cs = s->getTag();

        for (size_t a = 0; a < A; ++a) {
            SetStd<Edge*>* succs = s->getSuccessors(a);

            for (Edge* e : *succs) {
                int tid_i = e->getTo()->getId();
                int ct = states->at(tid_i)->getTag();

                if (ct != cs) {
                    radj_scc[ct].push_back(cs);
                }
            }
        }
    }

    // Mark SCCs that can reach a proper accepting SCC (reverse BFS on SCC DAG)
    std::vector<unsigned char> can_reach_acc_scc(nbSCC, 0);
    std::queue<int> q;

    for (unsigned int cid = 0; cid < nbSCC; ++cid) {
        if (proper_accepting_scc[cid] <= 0) continue;
        can_reach_acc_scc[cid] = 1;
        q.push(static_cast<int>(cid));
    }

    while (!q.empty()) {
        int cur = q.front(); q.pop();
        const auto& preds = radj_scc[static_cast<size_t>(cur)];
        for (int p : preds) {
            if (!can_reach_acc_scc[p]) {
                can_reach_acc_scc[p] = 1;
                q.push(p);
            }
        }
    }

    // Lift SCC predicate back to states
    for (size_t sid = 0; sid < n; ++sid) {
        int cid = states->at(sid)->getTag();
        if (can_reach_acc_scc[cid]) {
            good[sid] = 1;
        }
    }

    return good;
}

static SetStd<weight_t> computeMinMaxReturnValuesParentAware(const NestedAutomaton* nwa, size_t child_index, value_function_t finVal) {
    SetStd<weight_t> return_values;
    ChildAutomaton* child = nwa->getChild(child_index);

    // Treat size==1 children as "dummy/silent"
    if (child->getStates()->size() <= 1) return return_values;

    MapArray<State*>* mstates = nwa->getStates();

    const size_t M = mstates->size();
    const size_t A = nwa->getAlphabetSize();

    std::vector<bool> good = computeParentGoodMask(nwa);

    State* cinit = child->getInitial();

    using ProdState = std::tuple<State*, State*, weight_t>; // (parent_state, child_state, current_value)
    std::queue<ProdState> worklist;
    SetStd<ProdState> visited;

    auto push_state = [&](State* parent_state, State* child_state, weight_t value) {
        ProdState ws = { parent_state, child_state, value };
        if (!visited.contains(ws)) {
            visited.insert(ws);
            worklist.push(ws);
        }
    };

    auto record_if_final = [&](State* child_state, weight_t value) {
        if (child_state->getFinal()) {
            return_values.insert(value);
        }
    };

    // Seed: pick a call edge p -a-> q that calls child_index, and make the child consume 'a' immediately
    for (size_t pid = 0; pid < M; ++pid) {
        State* p = mstates->at(pid);

        for (size_t a = 0; a < A; ++a) {
            SetStd<Edge*>* msuccs = p->getSuccessors(a);

            for (Edge* me : *msuccs) {
                size_t idx = edgeWeightToChildIndex(me->getWeight()->getValue());
                if (idx != child_index) continue;

                int qid_i = me->getTo()->getId();
                if (!good[qid_i]) continue; // must stay in extendable-to-acceptance region
                State* m_after = me->getTo();

                SetStd<Edge*>* cs0 = cinit->getSuccessors(a);

                for (Edge* ce0 : *cs0) {
                    State* c1 = ce0->getTo();
                    weight_t w0 = ce0->getWeight()->getValue();

                    record_if_final(c1, w0);
                    push_state(m_after, c1, w0);
                }
            }
        }
    }

    // BFS on synchronized parent x child (parent must remain in good)
    while (!worklist.empty()) {
        ProdState cur = worklist.front(); worklist.pop();

        State* mcur = std::get<0>(cur);
        State* ccur = std::get<1>(cur);
        weight_t val = std::get<2>(cur);

        for (size_t a = 0; a < A; ++a) {
            SetStd<Edge*>* msuccs = mcur->getSuccessors(a);
            SetStd<Edge*>* csuccs = ccur->getSuccessors(a);

            for (Edge* me : *msuccs) {
                int mid_i = me->getTo()->getId();
                size_t mid = static_cast<size_t>(mid_i);
                if (!good[mid_i]) continue;

                State* m2 = me->getTo();
                for (Edge* ce : *csuccs) {
                    State* c2 = ce->getTo();
                    weight_t w = ce->getWeight()->getValue();

                    weight_t next_val = (finVal == Min_f) ? std::min(val, w) : std::max(val, w);
                    record_if_final(c2, next_val);
                    push_state(m2, c2, next_val);
                }
            }
        }
    }

    return return_values;
}

static SetStd<weight_t> computeSumBReturnValuesParentAware(const NestedAutomaton* nwa, size_t child_index, weight_t bound) {
    SetStd<weight_t> return_values;
    ChildAutomaton* child = nwa->getChild(child_index);

    // Treat size==1 children as "dummy/silent"
    if (child->getStates()->size() <= 1) return return_values;

    if (bound < 0) QUAK_FAIL("SumB requires a non-negative bound");

    MapArray<State*>* mstates = nwa->getStates();

    const size_t M = mstates->size();
    const size_t A = nwa->getAlphabetSize();

    std::vector<bool> good = computeParentGoodMask(nwa);

    State* cinit = child->getInitial();

    using ProdState = std::tuple<State*, State*, weight_t, weight_t>; // (parent_state, child_state, sum, hit)
    std::queue<ProdState> worklist;
    SetStd<ProdState> visited;

    auto push_state = [&](State* parent_state, State* child_state, weight_t sum, weight_t hit) {
        ProdState ws = { parent_state, child_state, sum, hit };
        if (!visited.contains(ws)) {
            visited.insert(ws);
            worklist.push(ws);
        }
    };

    auto record_if_final = [&](State* child_state, weight_t sum, weight_t hit) {
        if (child_state->getFinal()) {
            return_values.insert(hit != weight_t(0) ? hit : sum);
        }
    };

    // Seed: pick a call edge p -a-> q that calls child_index, and make the child consume 'a' immediately
    for (size_t pid = 0; pid < M; ++pid) {
        State* p = mstates->at(pid);

        for (size_t a = 0; a < A; ++a) {
            SetStd<Edge*>* msuccs = p->getSuccessors(a);

            for (Edge* me : *msuccs) {
                size_t idx = edgeWeightToChildIndex(me->getWeight()->getValue());
                if (idx != child_index) continue;

                int qid_i = me->getTo()->getId();
                if (!good[qid_i]) continue; // must stay in extendable-to-acceptance region
                State* m_after = me->getTo();

                SetStd<Edge*>* cs0 = cinit->getSuccessors(a);

                for (Edge* ce0 : *cs0) {
                    State* c1 = ce0->getTo();
                    weight_t w0 = ce0->getWeight()->getValue();

                    weight_t sum1;
                    weight_t hit1 = weight_t(0);

                    if (w0 > bound)       { sum1 = bound;  hit1 = bound; }
                    else if (w0 < -bound) { sum1 = -bound; hit1 = -bound; }
                    else                  { sum1 = w0; }

                    record_if_final(c1, sum1, hit1);
                    push_state(m_after, c1, sum1, hit1);
                }
            }
        }
    }

    // BFS on synchronized parent x child (parent must remain in good)
    while (!worklist.empty()) {
        ProdState cur = worklist.front(); worklist.pop();

        State* mcur = std::get<0>(cur);
        State* ccur = std::get<1>(cur);
        weight_t sum = std::get<2>(cur);
        weight_t hit = std::get<3>(cur);

        for (size_t a = 0; a < A; ++a) {
            SetStd<Edge*>* msuccs = mcur->getSuccessors(a);
            SetStd<Edge*>* csuccs = ccur->getSuccessors(a);

            for (Edge* me : *msuccs) {
                int mid_i = me->getTo()->getId();
                if (!good[mid_i]) continue;

                State* m2 = me->getTo();
                for (Edge* ce : *csuccs) {
                    State* c2 = ce->getTo();
                    weight_t w = ce->getWeight()->getValue();
                    weight_t raw = sum + w;

                    weight_t next_sum;
                    weight_t next_hit = hit;

                    if (raw > bound && hit == weight_t(0)) {
                        next_sum = bound;
                        next_hit = bound;
                    } else if (raw < -bound && hit == weight_t(0)) {
                        next_sum = -bound;
                        next_hit = -bound;
                    } else if (hit != weight_t(0)) {
                        next_sum = applyBound(raw, bound);
                    } else {
                        next_sum = raw;
                    }

                    record_if_final(c2, next_sum, next_hit);
                    push_state(m2, c2, next_sum, next_hit);
                }
            }
        }
    }

    return return_values;
}

// Convenience wrapper used from flatten_regular:
SetStd<weight_t> NestedAutomaton::computeChildReturnValuesParentAware(size_t child_index, value_function_t finVal, weight_t bound) {
    SetStd<weight_t> return_values;

    ChildAutomaton* child = this->getChild(child_index);
    if (!child) return return_values;

    if (finVal == Min_f || finVal == Max_f) {
        return computeMinMaxReturnValuesParentAware(this, child_index, finVal);
    }
    if (finVal == SumB) {
        return computeSumBReturnValuesParentAware(this, child_index, bound);
    }

    QUAK_FAIL("Unsupported value function for child automaton");
    return return_values;
}

//Helper: Efficient dominance-based state removal for MinState pairs
void removeDominatedStates(SetStd<std::pair<State*, weight_t>>& visited, State* target_state, weight_t new_value, value_function_t finVal) {
    SetStd<std::pair<State*, weight_t>> to_remove;

    // Collect all dominated states (same state with worse min value)
    for (const auto& visited_state : visited) {
        if (finVal == Min_f) {
            if (visited_state.first == target_state && visited_state.second >= new_value) {
                to_remove.insert(visited_state);
            }
        } else if (finVal == Max_f) {
            if (visited_state.first == target_state && visited_state.second <= new_value) {
                to_remove.insert(visited_state);
            }
        }
    }

    // Remove dominated entries in batch
    for (const auto& remove_state : to_remove) {
        visited.erase(remove_state);
    }
}

// Helper: Check if a state-value pair is dominated by existing visited states
bool isDominatedState(const SetStd<std::pair<State*, weight_t>>& visited, State* target_state, weight_t new_value, value_function_t finVal) {
    for (const auto& visited_state : visited) {
        if (finVal == Min_f) {
            if (visited_state.first == target_state && visited_state.second <= new_value) {
                return true; // Existing state has better (smaller) min value
            }
        } else if (finVal == Max_f) {
            if (visited_state.first == target_state && visited_state.second >= new_value) {
                return true; // Existing state has better (larger) max value
            }
        }
    }
    return false;
}

static SetStd<weight_t> computeMinMaxReturnValues(ChildAutomaton* child, value_function_t finVal) {
    SetStd<weight_t> return_values;

    if (!child) return return_values;
    if (!(finVal == Min_f || finVal == Max_f)) return return_values;

    // Treat size<=1 children as "dummy/silent"
    if (!child->getStates() || child->getStates()->size() <= 1) return return_values;

    State* init = child->getInitial();
    if (!init) return return_values;

    const size_t A = child->getAlphabetSize();

    // We must compute ALL possible return values, so we track (state, current_value).
    using ChildValState = std::pair<State*, weight_t>;
    std::queue<ChildValState> worklist;
    SetStd<ChildValState> visited;

    // Seed by consuming the FIRST input letter immediately:
    // for any symbol a and any edge init -a/w-> q, start at (q, w).
    for (size_t a = 0; a < A; ++a) {
        SetStd<Edge*>* succs = init->getSuccessors(a);
        if (!succs) continue;

        for (Edge* e0 : *succs) {
            if (!e0 || !e0->getTo() || !e0->getWeight()) continue;

            State* q = e0->getTo();
            weight_t w0 = e0->getWeight()->getValue();

            if (q->getFinal()) {
                return_values.insert(w0);   // return after consuming exactly one letter
            }

            ChildValState seed = { q, w0 };
            if (visited.contains(seed)) continue;
            visited.insert(seed);
            worklist.push(seed);
        }
    }

    // BFS/graph exploration on (child-state x current min/max value)
    while (!worklist.empty()) {
        ChildValState cur = worklist.front();
        worklist.pop();

        State* s = cur.first;
        weight_t val = cur.second;

        for (size_t a = 0; a < A; ++a) {
            SetStd<Edge*>* succs = s->getSuccessors(a);
            if (!succs) continue;

            for (Edge* e : *succs) {
                if (!e || !e->getTo() || !e->getWeight()) continue;

                State* t = e->getTo();
                weight_t w = e->getWeight()->getValue();

                weight_t next_val = (finVal == Min_f) ? std::min(val, w) : std::max(val, w);
                ChildValState nxt = { t, next_val };

                if (t->getFinal()) {
                    return_values.insert(next_val);
                }

                if (visited.contains(nxt)) continue;
                visited.insert(nxt);
                worklist.push(nxt);
            }
        }
    }

    return return_values;
}

static SetStd<weight_t> computeSumBReturnValues(ChildAutomaton* child, weight_t bound) {
    SetStd<weight_t> return_values;

    if (!child) return return_values;

    // Treat size<=1 children as "dummy/silent"
    if (!child->getStates() || child->getStates()->size() <= 1) return return_values;

    State* init = child->getInitial();
    if (!init) return return_values;

    const size_t A = child->getAlphabetSize();

    // State: (automaton_state, accumulated_sum, bound_value_hit)
    // bound_value_hit: 0 = never exceeded, +bound = hit upper bound first, -bound = hit lower bound first
    using SumState = std::tuple<State*, weight_t, weight_t>;
    std::queue<SumState> worklist;
    SetStd<SumState> visited;

    auto record_return = [&](weight_t sum, weight_t bound_hit) {
        if (bound_hit != weight_t(0)) return_values.insert(bound_hit);
        else return_values.insert(sum);
    };

    auto push_or_record = [&](State* st, weight_t sum, weight_t bound_hit) {
        if (st->getFinal()) {
            record_return(sum, bound_hit);
        }

        SumState s = {st, sum, bound_hit};
        if (visited.contains(s)) return;
        visited.insert(s);
        worklist.push(s);
    };

    auto step = [&](weight_t curr_sum, weight_t bound_hit, weight_t edge_w,
                    weight_t& next_sum, weight_t& next_bound_hit) {
        weight_t raw_sum = curr_sum + edge_w;

        next_bound_hit = bound_hit;

        if (bound_hit == weight_t(0)) {
            // First time we might cross a bound
            if (raw_sum > bound) {
                next_sum = bound;
                next_bound_hit = bound;
            } else if (raw_sum < -bound) {
                next_sum = -bound;
                next_bound_hit = -bound;
            } else {
                next_sum = raw_sum;
                // next_bound_hit stays 0
            }
        } else {
            // Already hit a bound earlier; keep bounded accumulator
            next_sum = applyBound(raw_sum, bound);
            // next_bound_hit stays the same
        }
    };

    // Seed by consuming ONE symbol immediately from init
    for (size_t sym_id = 0; sym_id < A; ++sym_id) {
        SetStd<Edge*>* succs = init->getSuccessors(sym_id);
        if (!succs) continue;

        for (Edge* e0 : *succs) {
            if (!e0 || !e0->getTo() || !e0->getWeight()) continue;

            State* s1 = e0->getTo();
            weight_t w0 = e0->getWeight()->getValue();

            weight_t next_sum, next_hit;
            step(weight_t(0), weight_t(0), w0, next_sum, next_hit);

            push_or_record(s1, next_sum, next_hit);
        }
    }

    // Continue exploration
    while (!worklist.empty()) {
        SumState cur = worklist.front();
        worklist.pop();

        State* curr_state = std::get<0>(cur);
        weight_t curr_sum = std::get<1>(cur);
        weight_t bound_hit = std::get<2>(cur);

        for (size_t sym_id = 0; sym_id < A; ++sym_id) {
            SetStd<Edge*>* succs = curr_state->getSuccessors(sym_id);
            if (!succs) continue;

            for (Edge* edge : *succs) {
                if (!edge || !edge->getTo() || !edge->getWeight()) continue;

                State* next_state = edge->getTo();
                weight_t edge_weight = edge->getWeight()->getValue();

                weight_t next_sum, next_hit;
                step(curr_sum, bound_hit, edge_weight, next_sum, next_hit);

                push_or_record(next_state, next_sum, next_hit);
            }
        }
    }

    return return_values;
}

// Helper: Compute all possible return values for a single child automaton
SetStd<weight_t> NestedAutomaton::computeChildReturnValues(ChildAutomaton* child, value_function_t finVal, weight_t bound) {
    SetStd<weight_t> return_values;

    if (!child) {
        return return_values; // Empty set for null child
    }

    if (finVal == Min_f || finVal == Max_f) {
        return_values = computeMinMaxReturnValues(child, finVal);
    }
    else if (finVal == SumB) {
        if (bound < 0) {
            QUAK_FAIL("SumB requires a non-negative bound");
        }
        return_values = computeSumBReturnValues(child, bound);
    }
    else {
        QUAK_FAIL("Unsupported value function for child automaton");
    }

    #ifdef DEBUG
        std::cout << "Child " << child->getName() << " (";
        switch(finVal) {
            case Min_f: std::cout << "Min_f"; break;
            case Max_f: std::cout << "Max_f"; break;
            case SumB: std::cout << "SumB"; break;
            default: std::cout << "Unknown"; break;
        }
        std::cout << ") can return values: {";
        for (weight_t val : return_values) {
            std::cout << val << " ";
        }
        std::cout << "} (count: " << return_values.size() << ")" << std::endl;
    #endif

    return return_values;
}



// Obligation-based (regular) flattening

weight_t transitionFunction(weight_t state_value,
                            weight_t transit_value,
                            value_function_t finVal,
                            weight_t bound) {
    weight_t result;
    if (finVal == Max_f) {
        result = std::max(state_value, transit_value);
    } else if (finVal == Min_f) {
        result = std::min(state_value, transit_value);
    } else if (finVal == SumB) {
        // Handle bound exceeded cases
        if (state_value == bound + 1 || state_value == -bound - 1) {
            return state_value; // Once exceeded stay exceeded
        }

        result = state_value + transit_value;

        // Check bounds with placeholder values
        if (result > bound) {
            result = bound + 1;     // Means positive bound exceeded
        } else if (result < -bound) {
            result = -bound - 1;    // Means negative bound exceeded
        }
    } else {
        QUAK_FAIL("transitionFunction: Non-regular value function\n");
    }
    return result;
}

// Configuration set for obligation tracking
struct ConfPair {
    uint32_t st;
    weight_t acc;

    bool operator==(const ConfPair& o) const { return st == o.st && acc == o.acc; }
    bool operator<(const ConfPair& o) const {
        if (st != o.st) return st < o.st;
        return acc < o.acc;
    }
};

using ConfSet = std::vector<ConfPair>;

static inline void conf_canonicalize(ConfSet& cs) {
    if (cs.empty()) return;
    std::sort(cs.begin(), cs.end());
    cs.erase(std::unique(cs.begin(), cs.end()), cs.end());
}

// Obligation key: (child, guessed return value, child configuration-set)
struct OblKey {
    uint32_t child;
    weight_t  guess;
    ConfSet   conf;   // canonical: sorted, unique (ConfPair order)

    bool operator==(const OblKey& o) const {
        return child == o.child && guess == o.guess && conf == o.conf;
    }
    bool operator<(const OblKey& o) const {
        if (child != o.child) return child < o.child;
        if (guess != o.guess) return guess < o.guess;
        return conf < o.conf;
    }
};

struct OblEntry {
    OblKey key;

    bool operator==(const OblEntry& o) const { return key == o.key; }
    bool operator<(const OblEntry& o)  const { return key <  o.key; }
};

// Compile-time switch between three OblBag approaches:
//
// 1. USE_SET_OBLBAG: uses std::set (auto-sorted, auto-unique)
//    - Slowest: 25-50% slower than vector approaches
//    - Uses 15-18% more memory due to tree node overhead
//    - Keep for reference/debugging only
//
// 2. USE_INCREMENTAL_BAG (default): vector with sorted insertion on each add
//    - Best overall: ~3-7% faster than batch on large instances
//    - Same memory usage as batch
//
// 3. Neither defined: vector with batch sort+unique at finalize
//    - Competitive but slightly slower than incremental
//    - Good fallback option
//
// Benchmark results (189K output states):
//   INCREMENTAL: 2781ms, 292 MB
//   BATCH:       2989ms, 292 MB
//   SET:         3980ms, 346 MB

// OblBag implementation selection:
// - USE_INCREMENTAL_BAG (default): vector with sorted insertion on each add
// - USE_SET_OBLBAG: uses std::set (slower, ~25-50% overhead)
// - Neither: vector with batch sort+unique at finalize
#define USE_INCREMENTAL_BAG
// #define USE_SET_OBLBAG

// Enable OBLBAG_PERF_DEBUG to collect and print performance statistics
// #define OBLBAG_PERF_DEBUG

#ifdef OBLBAG_PERF_DEBUG
// Performance statistics for OblBag operations (debug only)
struct OblBagStats {
    std::atomic<uint64_t> canonicalize_calls{0};
    std::atomic<uint64_t> canonicalize_total_ns{0};
    std::atomic<uint64_t> add_one_calls{0};
    std::atomic<uint64_t> add_one_total_ns{0};
    std::atomic<uint64_t> total_elements_processed{0};

    void reset() {
        canonicalize_calls = 0;
        canonicalize_total_ns = 0;
        add_one_calls = 0;
        add_one_total_ns = 0;
        total_elements_processed = 0;
    }

    void print(std::ostream& out = std::cout) const {
        out << "=== OblBag Performance Stats ===\n";
#ifdef USE_SET_OBLBAG
        out << "Approach: SET (std::set with auto-sorted insert)\n";
#elif defined(USE_INCREMENTAL_BAG)
        out << "Approach: INCREMENTAL (vector with insert-sorted on each add)\n";
#else
        out << "Approach: BATCH (vector with push_back + sort_unique at end)\n";
#endif
        out << "bag_add calls: " << add_one_calls << "\n";
        out << "bag_add total time: " << (add_one_total_ns / 1000000.0) << " ms\n";
        if (add_one_calls > 0) {
            out << "bag_add avg time: " << (add_one_total_ns / add_one_calls) << " ns\n";
        }
        out << "bag_finalize calls: " << canonicalize_calls << "\n";
        out << "bag_finalize total time: " << (canonicalize_total_ns / 1000000.0) << " ms\n";
        if (canonicalize_calls > 0) {
            out << "bag_finalize avg time: " << (canonicalize_total_ns / canonicalize_calls) << " ns\n";
        }
        out << "total elements processed: " << total_elements_processed << "\n";
        out << "================================\n";
    }
};

OblBagStats g_oblbag_stats;
#endif // OBLBAG_PERF_DEBUG

#ifdef USE_SET_OBLBAG

// Set-based approach: std::set maintains sorted unique elements
using OblBag = std::set<OblEntry>;

static inline void bag_add(OblBag& bag, OblEntry&& e) {
#ifdef OBLBAG_PERF_DEBUG
    auto start = std::chrono::high_resolution_clock::now();
#endif
    bag.insert(std::move(e));
#ifdef OBLBAG_PERF_DEBUG
    auto end = std::chrono::high_resolution_clock::now();
    g_oblbag_stats.add_one_calls++;
    g_oblbag_stats.add_one_total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
#endif
}

static inline void bag_finalize(OblBag& bag) {
    // No-op: set is always sorted and unique
#ifdef OBLBAG_PERF_DEBUG
    g_oblbag_stats.canonicalize_calls++;
    g_oblbag_stats.total_elements_processed += bag.size();
#endif
    (void)bag;
}

#else

using OblBag = std::vector<OblEntry>;

// Core operations for vector
static inline void bag_sort_unique(OblBag& bag) {
    if (!bag.empty()) {
        std::sort(bag.begin(), bag.end());
        bag.erase(std::unique(bag.begin(), bag.end()), bag.end());
    }
}

static inline void bag_insert_sorted(OblBag& bag, OblEntry&& e) {
    auto it = std::lower_bound(bag.begin(), bag.end(), e);
    if (it == bag.end() || !(it->key == e.key)) {
        bag.insert(it, std::move(e));
    }
}

#ifdef USE_INCREMENTAL_BAG

// Incremental approach: maintain sorted order on each add
static inline void bag_add(OblBag& bag, OblEntry&& e) {
#ifdef OBLBAG_PERF_DEBUG
    auto start = std::chrono::high_resolution_clock::now();
#endif
    bag_insert_sorted(bag, std::move(e));
#ifdef OBLBAG_PERF_DEBUG
    auto end = std::chrono::high_resolution_clock::now();
    g_oblbag_stats.add_one_calls++;
    g_oblbag_stats.add_one_total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
#endif
}

static inline void bag_finalize(OblBag& bag) {
    // No-op: already sorted and unique
#ifdef OBLBAG_PERF_DEBUG
    g_oblbag_stats.canonicalize_calls++;
    g_oblbag_stats.total_elements_processed += bag.size();
#endif
    (void)bag;
}

#else

// Batch approach: accumulate then sort+unique at the end
static inline void bag_add(OblBag& bag, OblEntry&& e) {
#ifdef OBLBAG_PERF_DEBUG
    auto start = std::chrono::high_resolution_clock::now();
#endif
    bag.push_back(std::move(e));
#ifdef OBLBAG_PERF_DEBUG
    auto end = std::chrono::high_resolution_clock::now();
    g_oblbag_stats.add_one_calls++;
    g_oblbag_stats.add_one_total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
#endif
}

static inline void bag_finalize(OblBag& bag) {
#ifdef OBLBAG_PERF_DEBUG
    auto start = std::chrono::high_resolution_clock::now();
#endif
    bag_sort_unique(bag);
#ifdef OBLBAG_PERF_DEBUG
    auto end = std::chrono::high_resolution_clock::now();
    g_oblbag_stats.canonicalize_calls++;
    g_oblbag_stats.canonicalize_total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    g_oblbag_stats.total_elements_processed += bag.size();
#endif
}

#endif // USE_INCREMENTAL_BAG
#endif // USE_SET_OBLBAG

// Legacy function names for compatibility
static inline void bag_canonicalize(OblBag& bag) { bag_finalize(bag); }
static inline void bag_add_one_sorted(OblBag& bag, OblEntry&& e) { bag_add(bag, std::move(e)); }
static inline void bag_push_back(OblBag& bag, OblEntry&& e) { bag_add(bag, std::move(e)); }

enum acc_phase_t : uint8_t { ACC_WAIT_parent = 0, ACC_WAIT_P2EMPTY = 1 };

struct ChildTables {
    ChildAutomaton* child = nullptr;
    uint32_t n_states = 0;
    uint32_t alph = 0;
    uint32_t init = 0;

    struct Trans {
        uint32_t to;
        weight_t w;
    };

    // CSR-like storage for (st, a) -> edges[ off[idx] .. off[idx+1] )
    std::vector<uint32_t> off;   // size = n_states*alph + 1
    std::vector<Trans> edges;

    std::vector<uint8_t> is_final;
    std::vector<uint8_t> live;

    inline uint32_t idx(uint32_t st, uint32_t a) const { return st * alph + a; }
};

static bool build_child_tables(ChildAutomaton* c, ChildTables& out) {
    if (!c) return false;
    out.child = c;
    out.n_states = (uint32_t)c->getStates()->size();
    out.alph = (uint32_t)c->getAlphabet()->size();
    out.init = (uint32_t)c->getInitial()->getId();

    out.is_final.assign(out.n_states, 0);
    for (uint32_t s = 0; s < out.n_states; ++s) {
        State* st = c->getStates()->at(s);
        out.is_final[s] = st->getFinal() ? 1 : 0;
    }

    const size_t cells = (size_t)out.n_states * (size_t)out.alph;
    out.off.assign(cells + 1, 0);

    for (uint32_t s = 0; s < out.n_states; ++s) {
        State* from = c->getStates()->at(s);
        for (uint32_t a = 0; a < out.alph; ++a) {
            SetStd<Edge*>* succs = from->getSuccessors(a);
            out.off[(size_t)out.idx(s, a) + 1] = succs ? (uint32_t)succs->size() : 0;
        }
    }

    for (size_t i = 1; i < out.off.size(); ++i) out.off[i] += out.off[i - 1];
    out.edges.resize(out.off.back());

    std::vector<uint32_t> cur = out.off;
    for (uint32_t s = 0; s < out.n_states; ++s) {
        State* from = c->getStates()->at(s);
        for (uint32_t a = 0; a < out.alph; ++a) {
            SetStd<Edge*>* succs = from->getSuccessors(a);
            if (!succs) continue;
            const uint32_t id = out.idx(s, a);
            for (Edge* e : *succs) {
                if (!e) continue;
                const uint32_t t = (uint32_t)e->getTo()->getId();
                const uint32_t pos = cur[(size_t)id]++;
                out.edges[(size_t)pos] = ChildTables::Trans{t, e->getWeight()->getValue()};
            }
        }
    }

    // Compute liveness via reverse BFS from finals (using State predecessors)
    out.live.assign(out.n_states, 0);
    std::deque<uint32_t> q;
    for (uint32_t s = 0; s < out.n_states; ++s) {
        if (out.is_final[s]) {
            out.live[s] = 1;
            q.push_back(s);
        }
    }
    while (!q.empty()) {
        uint32_t v = q.front(); q.pop_front();
        State* v_state = c->getStates()->at(v);
        for (uint32_t sym = 0; sym < out.alph; ++sym) {
            SetStd<Edge*>* preds = v_state->getPredecessors(sym);
            if (!preds) continue;
            for (Edge* e : *preds) {
                uint32_t u = (uint32_t)e->getFrom()->getId();
                if (!out.live[u]) {
                    out.live[u] = 1;
                    q.push_back(u);
                }
            }
        }
    }

    return true;
}

static inline weight_t acc0_for_finVal(value_function_t finVal) {
    if (finVal == Max_f) {
        return std::numeric_limits<float>::lowest();
    }
    else if (finVal == Min_f) {
        return std::numeric_limits<float>::infinity();
    }
    return weight_t(0);
}

// Check if accumulated value matches guessed return value (with SumB saturation)
static inline bool discharge_ok_finite(value_function_t finVal,
                                      const weight_t& acc,
                                      const weight_t& guess,
                                      const weight_t& bound) {
    if (acc == guess) return true;

    if (finVal == SumB) {
        // Handle positive saturation: guess = +bound, acc overflowed to bound+1
        if (guess == bound && acc == bound + 1) return true;
        // Handle negative saturation: guess = -bound, acc overflowed to -bound-1
        if (guess == -bound && acc == -bound - 1) return true;
        return false;
    }

    return false;
}

// Target-aware liveness: can obligation reach a matching final?
struct LiveKey {
    uint32_t child;
    weight_t guess;

    bool operator<(const LiveKey& o) const {
        if (child != o.child) return child < o.child;
        return guess < o.guess;
    }
};

struct TargetAwareLive {
    // per child: finite domain of accumulator values (closure under transitionFunction)
    std::vector<std::vector<weight_t>> acc_dom;
    std::vector<MapStd<weight_t, uint32_t>> acc_id;
    std::vector<uint32_t> dom_sz;

    // per (child, guess): live table of size n_states * dom_sz[child]
    MapStd<LiveKey, std::vector<uint8_t>> live_tab;

    uint32_t acc_index(uint32_t child, const weight_t& acc) const {
        if (child >= acc_id.size()) return UINT32_MAX;
        const auto& mp = acc_id[child];
        if (!mp.contains(acc)) return UINT32_MAX;
        return mp.at(acc);
    }

    bool is_live(uint32_t child, const weight_t& guess,
                 uint32_t st, const weight_t& acc) const {
        LiveKey k{child, guess};
        if (!live_tab.contains(k)) return true; // if not built, fall back to "no pruning"
        if (child >= dom_sz.size()) return false;
        const uint32_t aidx = acc_index(child, acc);
        if (aidx == UINT32_MAX) return false;
        const auto& vec = live_tab.at(k);
        const uint32_t idx = st * dom_sz[child] + aidx;
        if (idx >= vec.size()) return false;
        return vec[idx] != 0;
    }

    void build(const std::vector<ChildTables>& child_tab,
               const std::vector<SetStd<weight_t>>& child_return_values,
               value_function_t finVal,
               const weight_t& bound) {
        const size_t K = child_tab.size();
        acc_dom.assign(K, {});
        acc_id.assign(K, {});
        dom_sz.assign(K, 0);
        live_tab.clear();

        for (uint32_t i = 0; i < (uint32_t)K; ++i) {
            const ChildTables& T = child_tab[i];
            if (!T.child || T.n_states == 0 || T.alph == 0) continue;
            if (child_return_values[i].empty()) continue;

            // 1) collect edge weights
            SetStd<weight_t> W;
            for (const auto& tr : T.edges) W.insert(tr.w);

            // 2) build finite accumulator domain by closure
            SetStd<weight_t> dom;
            std::deque<weight_t> q;
            const weight_t init_acc = acc0_for_finVal(finVal);
            dom.insert(init_acc);
            q.push_back(init_acc);

            while (!q.empty()) {
                const weight_t acc = q.front(); q.pop_front();
                for (const weight_t& w : W) {
                    const weight_t acc2 = transitionFunction(acc, w, finVal, bound);
                    // auto ins = dom.insert(acc2);
                    // if (ins.second) q.push_back(acc2);
                    if (!dom.contains(acc2)) {
                        dom.insert(acc2);
                        q.push_back(acc2);
                    }
                }
            }

            acc_dom[i].reserve(dom.size());
            uint32_t idx = 0;
            for (const weight_t& a : dom) {
                acc_dom[i].push_back(a);
                acc_id[i].insert(a, idx++);
            }
            dom_sz[i] = (uint32_t)acc_dom[i].size();

            const uint32_t N = T.n_states;
            const uint32_t A = dom_sz[i];
            const uint32_t P = N * A;

            std::vector<std::vector<uint32_t>> rev(P);

            auto node_id = [&](uint32_t st, uint32_t aidx) -> uint32_t {
                return st * A + aidx;
            };

            // Final child states are continuation states too. Reaching a final
            // state is a termination opportunity, not mandatory termination.
            for (uint32_t st = 0; st < N; ++st) {
                for (uint32_t aidx = 0; aidx < A; ++aidx) {
                    const weight_t acc = acc_dom[i][aidx];

                    for (uint32_t sym = 0; sym < T.alph; ++sym) {
                        const uint32_t cell = T.idx(st, sym);
                        const uint32_t b = T.off[(size_t)cell];
                        const uint32_t e = T.off[(size_t)cell + 1];

                        for (uint32_t p = b; p < e; ++p) {
                            const auto& tr = T.edges[(size_t)p];
                            const uint32_t to = tr.to;

                            if (to >= N) continue;
                            if (!T.live[to]) continue; // coarse pruning still helpful

                            const weight_t acc2 = transitionFunction(acc, tr.w, finVal, bound);

                            const uint32_t aidx2 = acc_index(i, acc2);
                            if (aidx2 == UINT32_MAX) continue;

                            const uint32_t u = node_id(st, aidx);
                            const uint32_t v = node_id(to, aidx2);
                            rev[v].push_back(u);
                        }
                    }
                }
            }

            // 4) for each guess: seed nodes that can discharge in ONE step to a matching final,
            //         then reverse-BFS using rev[]
            for (const weight_t& guess : child_return_values[i]) {
                std::vector<uint8_t> live(P, 0);
                std::deque<uint32_t> qq;

                for (uint32_t st = 0; st < N; ++st) {
                    for (uint32_t aidx = 0; aidx < A; ++aidx) {
                        const weight_t acc = acc_dom[i][aidx];
                        bool seed = false;

                        // check if there exists an outgoing edge into a FINAL that matches this guess
                        for (uint32_t sym = 0; sym < T.alph && !seed; ++sym) {
                            const uint32_t cell = T.idx(st, sym);
                            const uint32_t b = T.off[(size_t)cell];
                            const uint32_t e = T.off[(size_t)cell + 1];
                            for (uint32_t p = b; p < e; ++p) {
                                const auto& tr = T.edges[(size_t)p];
                                const uint32_t to = tr.to;
                                if (to >= N) continue;
                                if (!T.is_final[to]) continue;

                                const weight_t acc2 = transitionFunction(acc, tr.w, finVal, bound);
                                if (discharge_ok_finite(finVal, acc2, guess, bound)) {
                                    seed = true;
                                    break;
                                }
                            }
                        }

                        if (seed) {
                            const uint32_t u = node_id(st, aidx);
                            if (!live[u]) {
                                live[u] = 1;
                                qq.push_back(u);
                            }
                        }
                    }
                }

                // reverse BFS
                while (!qq.empty()) {
                    uint32_t v = qq.front(); qq.pop_front();
                    for (uint32_t u : rev[v]) {
                        if (!live[u]) {
                            live[u] = 1;
                            qq.push_back(u);
                        }
                    }
                }

                live_tab.insert(LiveKey{i, guess}, std::move(live));
            }
        }
    }
};


// Step obligations forward on symbol
static bool step_obl_bag_finite(value_function_t finVal,
                               const OblBag& in,
                               uint32_t symbol_id,
                               OblBag& out,
                               const std::vector<ChildTables>& child_tab,
                               const TargetAwareLive& live,
                               const weight_t& bound) {
    out.clear();
    if (in.empty()) return true;

    for (const OblEntry& ent : in) {
        const uint32_t i = ent.key.child;
        if (i >= child_tab.size()) return false;
        const ChildTables& T = child_tab[i];
        if (!T.child) return false;
        if (symbol_id >= T.alph) return false;

        bool discharged = false;
        ConfSet next_conf;
        next_conf.reserve(ent.key.conf.size()); // heuristic

        for (const ConfPair& c : ent.key.conf) {
            const uint32_t st = c.st;
            if (!live.is_live(i, ent.key.guess, st, c.acc)) continue;
            if (st >= T.n_states) continue;

            const uint32_t cell = T.idx(st, symbol_id);
            const uint32_t b = T.off[(size_t)cell];
            const uint32_t e = T.off[(size_t)cell + 1];

            for (uint32_t p = b; p < e; ++p) {
                const auto& tr = T.edges[(size_t)p];
                const uint32_t to = tr.to;
                const weight_t acc2 = transitionFunction(c.acc, tr.w, finVal, bound);

                if (T.is_final[to] &&
                    discharge_ok_finite(finVal, acc2, ent.key.guess, bound)) {
                    discharged = true;
                    break; // existential: choose this branch and return now
                }

                if (!live.is_live(i, ent.key.guess, to, acc2)) continue; // target-aware pruning
                next_conf.push_back(ConfPair{to, acc2});
            }

            if (discharged) break;
        }

        if (discharged) continue; // this obligation discharged

        conf_canonicalize(next_conf);
        if (next_conf.empty()) return false; // no branch can continue to a valid return

        // set semantics: no multiplicities
        bag_push_back(out, OblEntry{OblKey{i, ent.key.guess, std::move(next_conf)}});
    }

    bag_canonicalize(out);
    return true;
}


// Spawn new obligations when encountering child calls
enum class SpawnStatus : uint8_t { REJECT = 0, EMPTY = 1, NONEMPTY = 2 };

static SpawnStatus spawn_obligation_finite(value_function_t finVal,
                                          uint32_t child_idx,
                                          uint32_t symbol_id,
                                          const weight_t& guess,
                                          OblEntry& spawned,
                                          const std::vector<ChildTables>& child_tab,
                                          const TargetAwareLive& live,
                                          const weight_t& bound) {
    if (child_idx >= child_tab.size()) return SpawnStatus::REJECT;
    const ChildTables& T = child_tab[child_idx];
    if (!T.child) return SpawnStatus::REJECT;
    if (symbol_id >= T.alph) return SpawnStatus::REJECT;

    const uint32_t init = T.init;
    if (init >= T.n_states) return SpawnStatus::REJECT;

    const uint32_t cell = T.idx(init, symbol_id);
    const uint32_t b = T.off[(size_t)cell];
    const uint32_t e = T.off[(size_t)cell + 1];

    ConfSet conf;
    for (uint32_t p = b; p < e; ++p) {
        const auto& tr = T.edges[(size_t)p];
        const uint32_t to = tr.to;
        const weight_t acc2 = transitionFunction(acc0_for_finVal(finVal), tr.w, finVal, bound);

        if (T.is_final[to] &&
            discharge_ok_finite(finVal, acc2, guess, bound)) {
            return SpawnStatus::EMPTY; // choose this edge and terminate immediately
        }

        if (!live.is_live(child_idx, guess, to, acc2)) continue;
        conf.push_back(ConfPair{to, acc2});
    }

    conf_canonicalize(conf);
    if (conf.empty()) return SpawnStatus::REJECT;

    // set semantics: no multiplicities
    spawned = OblEntry{OblKey{child_idx, guess, std::move(conf)}};
    return SpawnStatus::NONEMPTY;
}

// Buchi automaton state for flattening
struct BuchiState_obl {
    State* parent_state;
    weight_t last_guess;
    OblBag P1;
    OblBag P2;
    acc_phase_t phase;
    bool epoch_has_nonsilent;
    bool P1_has_nonsilent;

    BuchiState_obl()
        : parent_state(nullptr), last_guess(0), P1(), P2(), phase(ACC_WAIT_parent),
          epoch_has_nonsilent(false), P1_has_nonsilent(false) {}

    BuchiState_obl(State* p, weight_t g, const OblBag& p1, const OblBag& p2,
                   acc_phase_t ph, bool epoch_seen, bool p1_seen)
        : parent_state(p), last_guess(g), P1(p1), P2(p2), phase(ph),
          epoch_has_nonsilent(epoch_seen), P1_has_nonsilent(p1_seen) {}

    bool operator==(const BuchiState_obl& o) const {
        return parent_state == o.parent_state
            && last_guess == o.last_guess
            && P1 == o.P1
            && P2 == o.P2
            && phase == o.phase
            && epoch_has_nonsilent == o.epoch_has_nonsilent
            && P1_has_nonsilent == o.P1_has_nonsilent;
    }

    bool operator<(const BuchiState_obl& o) const {
        if (parent_state != o.parent_state) return parent_state < o.parent_state;
        if (last_guess   != o.last_guess)   return last_guess   < o.last_guess;
        if (P1 != o.P1) return P1 < o.P1;     // lexicographic
        if (P2 != o.P2) return P2 < o.P2;
        if (phase != o.phase) return phase < o.phase;
        if (epoch_has_nonsilent != o.epoch_has_nonsilent) {
            return epoch_has_nonsilent < o.epoch_has_nonsilent;
        }
        return P1_has_nonsilent < o.P1_has_nonsilent;
    }
};

static inline acc_phase_t advance_phase(const BuchiState_obl& s) {
    if (s.phase == ACC_WAIT_parent) {
        return (s.parent_state->getFinal()) ? ACC_WAIT_P2EMPTY : ACC_WAIT_parent;
    } else {
        return (s.P2.empty()) ? ACC_WAIT_parent : ACC_WAIT_P2EMPTY;
    }
}

Automaton* NestedAutomaton::flatten_regular(value_function_t finVal, weight_t bound) {
    if (!(finVal == SumB || finVal == Max_f || finVal == Min_f)) {
        QUAK_FAIL("flatten_regular: finVal must be SumB / Max_f / Min_f");
    }

#ifdef OBLBAG_PERF_DEBUG
    // Reset OblBag performance stats for this run
    g_oblbag_stats.reset();
#endif

#ifdef DEBUG
    std::cout << "parent_states=" << this->getStates()->size() << std::endl;
#endif

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    MapArray<Symbol*>* new_alphabet = nullptr;
    MapArray<Weight*>* new_weights  = nullptr;

    weight_t global_min = weight_t(0), global_max = weight_t(0);

    MapStd<BuchiState_obl, State*> state_map;
    std::deque<BuchiState_obl> worklist;
    MapStd<weight_t, Weight*> weight_register;
    unsigned int state_counter = 0;

#ifdef DEBUG
    size_t max_p1_size = 0;
    size_t max_p2_size = 0;
#endif

    // 1) Return values per child
    const size_t k = this->getChildrenSize();
    std::vector<SetStd<weight_t>> child_return_values(k);

    SetStd<weight_t> global_return_values;
    global_return_values.insert(weight_t(SILENT));

    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (!child) {
#ifdef DEBUG
            std::cout << "child[" << i << "] (null)" << std::endl;
#endif
            continue;
        }
        child_return_values[i] = this->computeChildReturnValuesParentAware(i, finVal, bound);
        for (const weight_t& v : child_return_values[i]) global_return_values.insert(v);

#ifdef DEBUG
        std::cout << "child[" << i << "] states=" << child->getStates()->size()
                  << " det=" << (child->isDeterministic() ? 1 : 0)
                  << " return_values={";
        bool first_v = true;
        for (const auto& val : child_return_values[i]) {
            if (!first_v) std::cout << ",";
            std::cout << val;
            first_v = false;
        }
        std::cout << "}" << std::endl;
#endif
    }

    bool have_non_silent = false;
    for (weight_t val : global_return_values) {
        if (val == weight_t(SILENT)) continue;
        if (!have_non_silent) { global_min = val; global_max = val; have_non_silent = true; }
        else { global_min = std::min(global_min, val); global_max = std::max(global_max, val); }
    }
    if (!have_non_silent) { global_min = weight_t(0); global_max = weight_t(0); }

    // 2) Build child tables
    std::vector<ChildTables> child_tab(k);
    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* c = this->getChild(i);
        if (!c) continue;
        if (c->getStates()->size() < 2) continue;
        build_child_tables(c, child_tab[i]);
    }
    TargetAwareLive live;
    live.build(child_tab, child_return_values, finVal, bound);


    // 3) Copy alphabet + build weight objects
    {
        const size_t alph_size = this->getAlphabetSize();
        new_alphabet = new MapArray<Symbol*>(alph_size);
        for (size_t i = 0; i < alph_size; ++i) {
            Symbol* original = this->getAlphabet()->at(i);
            Symbol* copy = new Symbol(original->getName());
            new_alphabet->insert(i, copy);
        }

        new_weights = new MapArray<Weight*>(global_return_values.size());
        weight_register.clear();
        for (weight_t value : global_return_values) {
            Weight* w = new Weight(value);
            new_weights->insert(w->getId(), w);
            weight_register.insert(value, w);
        }
    }

    // 4) Initial Buchi state
    BuchiState_obl init;
    init.parent_state = this->getInitial();
    init.last_guess   = weight_t(INIT_BUCHI_VALUE);
    init.P1.clear();
    init.P2.clear();
    init.phase = ACC_WAIT_parent;
    init.epoch_has_nonsilent = false;
    init.P1_has_nonsilent = false;

    std::ostringstream ss;
    ss << "b_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);

    state_map[init] = init_state;
    worklist.push_back(init);

    // 5) BFS construction
    OblBag P1_step, P2_step;
    OblBag P1_next, P2_next;
    bool epoch_seen_next, P1_seen_next;

    while (!worklist.empty()) {
        BuchiState_obl current = std::move(worklist.front());
        worklist.pop_front();

#ifdef DEBUG
        if (current.P1.size() > max_p1_size) max_p1_size = current.P1.size();
        if (current.P2.size() > max_p2_size) max_p2_size = current.P2.size();
#endif

        State* current_state = state_map[current];
        const acc_phase_t phase_after_current = advance_phase(current);
        const bool current_is_checkpoint =
            current.phase == ACC_WAIT_P2EMPTY && current.P2.empty();
        const bool epoch_seen_base =
            current_is_checkpoint ? current.P1_has_nonsilent : current.epoch_has_nonsilent;

        for (unsigned symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            if (!step_obl_bag_finite(finVal, current.P1, symbol_id, P1_step, child_tab, live, bound)) {
                continue;
            }

            if (!current.P2.empty()) {
                if (!step_obl_bag_finite(finVal, current.P2, symbol_id, P2_step, child_tab, live, bound)) {
                    continue;
                }
            } else {
                P2_step.clear();
            }

            SetStd<Edge*>* succs = current.parent_state->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* parent_edge : *succs) {
                State* q_prime = parent_edge->getTo();
                const weight_t pw = parent_edge->getWeight()->getValue();
                const bool is_silent = (pw == weight_t(0));

                if (is_silent) {
                    if (current.P2.empty()) {
                        P1_next.clear();
                        P2_next = P1_step;
                        P1_seen_next = false;
                    } else {
                        P1_next = P1_step;
                        P2_next = P2_step;
                        P1_seen_next = current.P1_has_nonsilent;
                    }
                    epoch_seen_next = epoch_seen_base;

                    BuchiState_obl nxt(q_prime, weight_t(SILENT), P1_next, P2_next,
                                        phase_after_current, epoch_seen_next, P1_seen_next);

                    if (!state_map.contains(nxt)) {
                        std::ostringstream s2;
                        s2 << "b_" << state_counter++;
                        State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                        state_map[nxt] = ns;
                        worklist.push_back(nxt);
                    }

                    Weight* w = weight_register.at(weight_t(SILENT));
                    Edge* ne = new Edge(new_alphabet->at(symbol_id), w, current_state, state_map[nxt]);
                    current_state->addSuccessor(ne);
                    state_map[nxt]->addPredecessor(ne);

                } else {
                    const long long cid_ll = (long long)std::llround(pw.to_float());
                    if (cid_ll < 0) continue;
                    const size_t child_index = (size_t)cid_ll;
                    if (child_index >= k) continue;

                    const SetStd<weight_t>& guesses = child_return_values[child_index];
                    const bool boundary = current.P2.empty();

                    for (const weight_t& guess : guesses) {
                        OblEntry spawned;
                        const SpawnStatus st = spawn_obligation_finite(
                            finVal, (uint32_t)child_index, (uint32_t)symbol_id, guess, spawned, child_tab, live, bound
                        );
                        if (st == SpawnStatus::REJECT) continue;

                        if (boundary) {
                            P2_next = P1_step;
                            if (st == SpawnStatus::EMPTY) {
                                P1_next.clear();
                            } else {
                                P1_next.clear();
                                bag_push_back(P1_next, std::move(spawned));
                                bag_canonicalize(P1_next);
                            }
                            P1_seen_next = true;
                        } else {
                            P2_next = P2_step;
                            P1_next = P1_step;
                            if (st == SpawnStatus::NONEMPTY) {
                                bag_add_one_sorted(P1_next, std::move(spawned));
                            }
                            P1_seen_next = true;
                        }
                        epoch_seen_next = true;

                        BuchiState_obl nxt(q_prime, guess, P1_next, P2_next,
                                            phase_after_current, epoch_seen_next, P1_seen_next);

                        if (!state_map.contains(nxt)) {
                            std::ostringstream s2;
                            s2 << "b_" << state_counter++;
                            State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                            state_map[nxt] = ns;
                            worklist.push_back(nxt);
                        }

                        Weight* w = weight_register.at(guess);
                        Edge* ne = new Edge(new_alphabet->at(symbol_id), w, current_state, state_map[nxt]);
                        current_state->addSuccessor(ne);
                        state_map[nxt]->addPredecessor(ne);
                    }
                }
            }
        }
    }

    // 6) Build automaton + acceptance
    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [gs, st] : state_map) {
        new_states->insert(st->getId(), st);
        if (gs.phase == ACC_WAIT_P2EMPTY && gs.P2.empty() && gs.epoch_has_nonsilent) {
            st->setFinal(true);
        }
    }

#ifdef DEBUG
    std::cout << "(maxP1, maxP2) = (" << max_p1_size << ", " << max_p2_size << ")" << std::endl;
#endif

#ifdef OBLBAG_PERF_DEBUG
    // Print OblBag performance stats
    g_oblbag_stats.print();
#endif

    std::string name = "BuchiObl(" + this->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights, global_min, global_max, init_state);
}

static bool hasReachableAcceptingSccWithNonSilentEdge(Automaton* flat) {
    if (!flat || !flat->getStates() || !flat->getInitial()) return false;

    MapArray<State*>* states = flat->getStates();
    const size_t n = states->size();
    const size_t alph = flat->getAlphabetSize();
    const unsigned int init_id = flat->getInitial()->getId();
    if (init_id >= n) return false;

    std::vector<uint8_t> reachable(n, 0u);
    std::queue<State*> q;
    reachable[init_id] = 1u;
    q.push(flat->getInitial());

    while (!q.empty()) {
        State* s = q.front();
        q.pop();
        if (!s) continue;

        for (size_t a = 0; a < alph; ++a) {
            SetStd<Edge*>* succs = s->getSuccessors(a);
            if (!succs) continue;

            for (Edge* e : *succs) {
                if (!e || !e->getTo()) continue;
                const unsigned int tid = e->getTo()->getId();
                if (tid >= n || reachable[tid]) continue;
                reachable[tid] = 1u;
                q.push(e->getTo());
            }
        }
    }

    const unsigned int nbScc = flat->getNbSCCs();
    std::vector<uint8_t> scc_has_final(nbScc, 0u);
    std::vector<uint8_t> scc_has_nonsilent_edge(nbScc, 0u);

    for (size_t sid = 0; sid < n; ++sid) {
        if (!reachable[sid]) continue;
        State* s = states->at(sid);
        if (!s) continue;

        const int cid_i = s->getTag();
        if (cid_i < 0) continue;
        const unsigned int cid = static_cast<unsigned int>(cid_i);
        if (cid >= nbScc) continue;

        if (s->getFinal()) {
            scc_has_final[cid] = 1u;
        }

        for (size_t a = 0; a < alph; ++a) {
            SetStd<Edge*>* succs = s->getSuccessors(a);
            if (!succs) continue;

            for (Edge* e : *succs) {
                if (!e || !e->getTo() || !e->getWeight()) continue;

                const int to_cid_i = e->getTo()->getTag();
                if (to_cid_i < 0) continue;
                if (static_cast<unsigned int>(to_cid_i) != cid) continue;

                if (e->getWeight()->getValue() != weight_t(SILENT)) {
                    scc_has_nonsilent_edge[cid] = 1u;
                }
            }
        }
    }

    for (unsigned int cid = 0; cid < nbScc; ++cid) {
        if (scc_has_final[cid] && scc_has_nonsilent_edge[cid]) {
            return true;
        }
    }
    return false;
}

static bool hasAcceptingNonSilentNestedRun(NestedAutomaton* nwa) {
    if (!nwa) return false;

    // Boolean nested-language check: SumB(0) projects terminating real child
    // calls to value 0 while silent parent transitions remain SILENT. The SCC
    // check runs on the obligation automaton, so pending child calls are still
    // enforced.
    Automaton* flat = nwa->flatten_regular(SumB, weight_t(0));
    const bool ok = hasReachableAcceptingSccWithNonSilentEdge(flat);
    delete flat;
    return ok;
}

NestedAutomaton* NestedAutomaton::makeCompleteNested(std::vector<bool>* complete_flags, weight_t parent_sink_w, weight_t child_sink_w) const {
    // determine which automata need completion
    std::vector<bool> local_flags;
    std::vector<bool>* flags = complete_flags;
    if (flags == nullptr) {
        this->isCompleteNested(&local_flags);
        flags = &local_flags;
    }

    bool parent_complete = (*flags)[0];

    // copy alphabet
    Symbol::RESET();
    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(this->getAlphabet()->size());
    for (size_t i = 0; i < this->getAlphabet()->size(); ++i) {
        new_alphabet->insert(i, new Symbol(this->getAlphabet()->at(i)->getName()));
    }

    // copy weights, add sink weight if needed
    Weight::RESET();
    unsigned int parent_sink_weight_id = -1;
    for (size_t i = 0; i < this->getWeights()->size(); ++i) {
        if (this->getWeights()->at(i)->getValue() == parent_sink_w) {
            parent_sink_weight_id = i;
            break;
        }
    }
    size_t num_weights = this->getWeights()->size() + ((parent_sink_weight_id > -1) ? 1 : 0);
    MapArray<Weight*>* new_weights = new MapArray<Weight*>(num_weights);
    for (size_t i = 0; i < this->getWeights()->size(); ++i) {
        new_weights->insert(i, new Weight(this->getWeights()->at(i)->getValue()));

    }
    Weight* parent_sink_weight = nullptr;
    if (!parent_complete) {
        if (parent_sink_weight_id == -1) {
            parent_sink_weight = new Weight(parent_sink_w);
            new_weights->insert(this->getWeights()->size(), parent_sink_weight);
        } else {
            parent_sink_weight = new_weights->at(parent_sink_weight_id);
        }
    }

    // Copy states, add sink state if needed
    State::RESET();
    size_t num_states = this->getStates()->size() + (parent_complete ? 0 : 1);
    MapArray<State*>* new_states = new MapArray<State*>(num_states);
    for (size_t i = 0; i < this->getStates()->size(); ++i) {
        State* os = this->getStates()->at(i);
        State* ns = new State(os->getName(), new_alphabet->size(), 0, this->getChildrenSize() - 1);
        ns->setFinal(os->getFinal());
        new_states->insert(i, ns);
    }
    State* parent_sink = nullptr;
    if (!parent_complete) {
        parent_sink = new State("@sink@", new_alphabet->size(), 0, this->getChildrenSize() - 1);
        parent_sink->setFinal(false);
        new_states->insert(this->getStates()->size(), parent_sink);
    }

    State* new_initial = new_states->at(this->getInitial()->getId());

    // Copy existing parent transitions
    for (size_t sid = 0; sid < this->getStates()->size(); ++sid) {
        State* old_state = this->getStates()->at(sid);
        State* new_from = new_states->at(sid);
        for (size_t a = 0; a < this->getAlphabet()->size(); ++a) {
            SetStd<Edge*>* succs = old_state->getSuccessors(a);
            if (succs) {
                for (Edge* e : *succs) {
                    State* new_to = new_states->at(e->getTo()->getId());
                    Weight* new_w = new_weights->at(e->getWeight()->getId());
                    Edge* new_edge = new Edge(new_alphabet->at(a), new_w, new_from, new_to);
                    new_from->addSuccessor(new_edge);
                    new_to->addPredecessor(new_edge);
                }
            }
        }
    }

    // Add missing transitions to sink (including self-loops on sink)
    if (!parent_complete) {
        for (size_t sid = 0; sid < new_states->size(); ++sid) {
            State* state = new_states->at(sid);
            for (size_t a = 0; a < new_alphabet->size(); ++a) {
                SetStd<Edge*>* succs = state->getSuccessors(a);
                if (!succs || succs->size() == 0) {
                    Edge* sink_edge = new Edge(new_alphabet->at(a), parent_sink_weight, state, parent_sink);
                    state->addSuccessor(sink_edge);
                    parent_sink->addPredecessor(sink_edge);
                }
            }
        }
    }

    // Handle children automata
    MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(this->getChildrenSize());

    for (size_t ci = 0; ci < this->getChildrenSize(); ++ci) {
        ChildAutomaton* child = this->getChild(ci);
        if (!child) continue;

        bool child_complete = (*flags)[ci + 1];

        // Copy child alphabet
        Symbol::RESET();
        MapArray<Symbol*>* child_alpha = new MapArray<Symbol*>(child->getAlphabet()->size());
        for (size_t i = 0; i < child->getAlphabet()->size(); ++i) {
            child_alpha->insert(i, new Symbol(child->getAlphabet()->at(i)->getName()));
        }

        // Copy child weights, add sink weight if needed
        Weight::RESET();
        size_t child_num_weights = child->getWeights()->size() + (child_complete ? 0 : 1);
        MapArray<Weight*>* child_weights = new MapArray<Weight*>(child_num_weights);
        unsigned int child_sink_weight_id = -1;
        for (size_t i = 0; i < child->getWeights()->size(); ++i) {
            child_weights->insert(i, new Weight(child->getWeights()->at(i)->getValue()));
            if (child->getWeights()->at(i)->getValue() == child_sink_w) {
                child_sink_weight_id = i;
            }
        }
        Weight* child_sink_weight = nullptr;
        if (!child_complete) {
            if (child_sink_weight_id == -1) {
                child_sink_weight = new Weight(child_sink_w);
                child_weights->insert(child->getWeights()->size(), child_sink_weight);
            } else {
                child_sink_weight = child_weights->at(child_sink_weight_id);
            }
        }

        // Copy child states, add sink if needed
        State::RESET();
        size_t child_num_states = child->getStates()->size() + (child_complete ? 0 : 1);
        MapArray<State*>* child_states = new MapArray<State*>(child_num_states);
        for (size_t i = 0; i < child->getStates()->size(); ++i) {
            State* os = child->getStates()->at(i);
            State* ns = new State(os->getName(), child_alpha->size(), child->getMinDomain(), child->getMaxDomain());
            ns->setFinal(os->getFinal());
            child_states->insert(i, ns);
        }
        State* child_sink = nullptr;
        if (!child_complete) {
            child_sink = new State("@sink@", child_alpha->size(), child->getMinDomain(), child->getMaxDomain());
            child_sink->setFinal(false);
            child_states->insert(child->getStates()->size(), child_sink);
        }

        // Copy existing child transitions
        for (size_t sid = 0; sid < child->getStates()->size(); ++sid) {
            State* old_state = child->getStates()->at(sid);
            State* new_from = child_states->at(sid);
            for (size_t a = 0; a < child->getAlphabet()->size(); ++a) {
                SetStd<Edge*>* succs = old_state->getSuccessors(a);
                if (succs) {
                    for (Edge* e : *succs) {
                        State* new_to = child_states->at(e->getTo()->getId());
                        Weight* new_w = child_weights->at(e->getWeight()->getId());
                        Edge* new_edge = new Edge(child_alpha->at(a), new_w, new_from, new_to);
                        new_from->addSuccessor(new_edge);
                        new_to->addPredecessor(new_edge);
                    }
                }
            }
        }

        // Add missing transitions to child sink
        if (!child_complete) {
            for (size_t sid = 0; sid < child_states->size(); ++sid) {
                State* state = child_states->at(sid);
                if (state->getFinal()) continue; // no outgoing transitions from final states
                for (size_t a = 0; a < child_alpha->size(); ++a) {
                    SetStd<Edge*>* succs = state->getSuccessors(a);
                    if (!succs || succs->size() == 0) {
                        Edge* sink_edge = new Edge(child_alpha->at(a), child_sink_weight, state, child_sink);
                        state->addSuccessor(sink_edge);
                        child_sink->addPredecessor(sink_edge);
                    }
                }
            }
        }

        State* child_new_init = child_states->at(child->getInitial()->getId());

        ChildAutomaton* new_child = new ChildAutomaton(
            child->getName(),
            child_alpha,
            child_states,
            child_weights,
            child->getMinDomain(),
            child->getMaxDomain(),
            child_new_init
        );
        new_children->insert(ci, new_child);
    }

    return new NestedAutomaton(
        "Complete(" + this->getName() + ")",
        new_alphabet,
        new_states,
        new_weights,
        0,
        this->getChildrenSize() - 1,
        new_initial,
        new_children
    );
}


std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual> NestedAutomaton::generateMacroAlphabet() {
    // Prepare automata list
    std::vector<Automaton*> automata_list;
    automata_list.push_back(const_cast<NestedAutomaton*>(this));
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (child) {
            automata_list.push_back(child);
        }
    }

    // Prepare symbol list
    std::vector<Symbol*> symbol_list;
    for (unsigned int symbol_id = 0; symbol_id < this->getAlphabetSize(); ++symbol_id) {
        symbol_list.push_back(this->getAlphabet()->at(symbol_id));
    }

    // Initialize resolver and alphabet containers
    std::vector<SetStd<Edge*>> resolver(automata_list.size());
    std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual > macro_alphabet;

    // generateResolvers(0, 0, 0, resolver, macro_alphabet, automata_list, symbol_list); // this is already done inside generateMacro
    generateMacro(macro_alphabet, automata_list, symbol_list);

    return macro_alphabet;
}

NestedAutomaton* NestedAutomaton::determinizeWithMacroAlphabet() {
    // Generate macro alphabet
    std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual> macro_alphabet = this->generateMacroAlphabet();

    // Initialize the nested automaton children
    MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(this->getChildrenSize());

    // To get a deterministic ordering from the unordered_set, copy into a vector
    std::vector<MacroSymbol*> macro_list;
    macro_list.reserve(macro_alphabet.size());
    for (MacroSymbol* m : macro_alphabet) {
        macro_list.push_back(m);
    }

    // Build a concrete Symbol alphabet corresponding to the macro_alphabet
    // IDs of new alphabet correspond to indices in macro_list
    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(macro_alphabet.size());
    Symbol::RESET();
    size_t idx = 0;
    for (MacroSymbol* m : macro_list) {
        new_alphabet->insert(idx, new Symbol("a" + std::to_string(idx)));
        ++idx;
    }

    std::vector<MapArray<Symbol*>*> children_alphabet(this->getChildrenSize(), nullptr);
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        Symbol::RESET();
        ChildAutomaton* child = this->getChild(i);
        if (child) {
            MapArray<Symbol*>* child_alpha = new MapArray<Symbol*>(macro_alphabet.size());
            for (size_t mid = 0; mid < macro_list.size(); ++mid) {
                child_alpha->insert(mid, new Symbol("a" + std::to_string(mid)));
            }
            children_alphabet[i] = child_alpha;
        }
    }

    // States stay the same
    State::RESET();
    MapArray<State*>* new_states = new MapArray<State*>(this->getStates()->size());
    for (size_t i = 0; i < this->getStates()->size(); ++i) {
        State* state = new State(this->getStates()->at(i)->getName(), new_alphabet->size(), 0, this->getChildrenSize() - 1);
        state->setFinal(this->getStates()->at(i)->getFinal());
        new_states->insert(i, state);
    }
    State* new_initial = new_states->at(this->getInitial()->getId());

    std::vector<MapArray<State*>*> children_states(this->getChildrenSize(), nullptr);
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        State::RESET();
        ChildAutomaton* child = this->getChild(i);
        if (child) {
            MapArray<State*>* copied_states = new MapArray<State*>(child->getStates()->size());
            for (size_t sid = 0; sid < child->getStates()->size(); ++sid) {
                State* os = child->getStates()->at(sid);
                State* ns = new State(os->getName(), new_alphabet->size(), child->getMinDomain(), child->getMaxDomain());
                ns->setFinal(os->getFinal());
                copied_states->insert(sid, ns);
            }
            children_states[i] = copied_states;
        }
    }

    // Weights stay the same
    Weight::RESET();
    MapArray<Weight*>* new_weights = new MapArray<Weight*>(this->getWeights()->size());
    for (size_t i = 0; i < this->getWeights()->size(); ++i) {
        Weight* weight = new Weight(this->getWeights()->at(i)->getValue());
        new_weights->insert(i, weight);
    }

    std::vector<MapArray<Weight*>*> children_weights(this->getChildrenSize(), nullptr);
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        Weight::RESET();
        ChildAutomaton* child = this->getChild(i);
        if (child) {
            MapArray<Weight*>* copied_weights = new MapArray<Weight*>(child->getWeights()->size());
            for (size_t wid = 0; wid < child->getWeights()->size(); ++wid) {
                Weight* ow = child->getWeights()->at(wid);
                Weight* nw = new Weight(ow->getValue());
                copied_weights->insert(wid, nw);
            }
            children_weights[i] = copied_weights;
        }
    }

    // Build transitions based on macro symbols: for each resolver in each macro symbol, add corresponding edges
    for (size_t macro_id = 0; macro_id < macro_list.size(); ++macro_id) {
        MacroSymbol* macro = macro_list[macro_id];

        // Edges from the parent's resolver
        SetStd<Edge*> edges = macro->getResolver()[0];
        for (Edge* edge : edges) {
            State* from_state = edge->getFrom();
            State* to_state = edge->getTo();

            // Skip edges with invalid state references
            if (from_state->getId() >= new_states->size() || to_state->getId() >= new_states->size()) {
                continue;
            }
            if (edge->getWeight()->getId() >= new_weights->size()) {
                continue;
            }

            Weight* new_weight = new_weights->at(edge->getWeight()->getId());
            Edge* new_edge = new Edge(new_alphabet->at(macro_id), new_weight, new_states->at(from_state->getId()), new_states->at(to_state->getId()));
            new_states->at(from_state->getId())->addSuccessor(new_edge);
            new_states->at(to_state->getId())->addPredecessor(new_edge);
        }

        // Edges from the children's resolvers
        size_t ai = 1; // skip parent at 0
        size_t ci = 1; // skip dummy at 0
        while (ai < macro->getResolver().size() && ci < children_states.size()) {
            const SetStd<Edge*>& edges = macro->getResolver()[ai];
            if (edges.size() == 0) {
                ++ai;
                continue;
            }

            auto* alpha  = children_alphabet[ci];
            auto* wtab   = children_weights[ci];
            auto* states = children_states[ci];

            for (Edge* e : edges) {
                State* from_src = e->getFrom();
                State* to_src   = e->getTo();

                // Skip edges from children with < 2 states (dummy children)
                if (states == nullptr || states->size() < 2) {
                    continue;
                }
                if (from_src->getId() >= states->size() || to_src->getId() >= states->size()) {
                    continue;  // Skip invalid state references
                }

                State* from = states->at(from_src->getId());
                State* to   = states->at(to_src->getId());
                if (e->getWeight()->getId() >= wtab->size()) {
                    continue;  // Skip invalid weight references
                }
                auto* w     = wtab->at(e->getWeight()->getId());

                Edge* new_e = new Edge(alpha->at(macro_id), w, from, to);
                from->addSuccessor(new_e);
                to->addPredecessor(new_e);
            }

            // We consumed one non-empty resolver bucket for this child
            ++ai;
            ++ci;
        }
    }

    // // print all transitions for debugging
    // std::cout << "Determinized Nested Automaton Transitions:" << std::endl;
    // for (size_t sid = 0; sid < new_states->size(); ++sid) {
    //     State* s = new_states->at(sid);
    //     for (size_t a = 0; a < new_alphabet->size(); ++a) {
    //         SetStd<Edge*>* succs = s->getSuccessors(a);
    //         if (succs) {
    //             for (Edge* e : *succs) {
    //                 std::cout << "From state " << s->getName() << " to state " << e->getTo()->getName() << " on symbol " << new_alphabet->at(a)->getName() << " with weight " << e->getWeight()->getValue() << std::endl;
    //             }
    //         }
    //     }
    // }
    // // print all children transitions for debugging
    // for (size_t ci = 0; ci < this->getChildrenSize(); ++ci) {
    //     ChildAutomaton* child = this->getChild(ci);
    //     if (child) {
    //         std::cout << "Child Automaton " << child->getName() << " Transitions:" << std::endl;
    //         MapArray<State*>* cstates = children_states[ci];
    //         for (size_t sid = 0; sid < cstates->size(); ++sid) {
    //             State* s = cstates->at(sid);
    //             for (size_t a = 0; a < children_alphabet[ci]->size(); ++a) {
    //                 SetStd<Edge*>* succs = s->getSuccessors(a);
    //                 if (succs) {
    //                     for (Edge* e : *succs) {
    //                         std::cout << "From state " << s->getName() << " to state " << e->getTo()->getName() << " on symbol " << children_alphabet[ci]->at(a)->getName() << " with weight " << e->getWeight()->getValue() << std::endl;
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // }

    // Construct children automata
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (child) {
            State* new_init = children_states[i]->at(child->getInitial()->getId());

            ChildAutomaton* new_child = new ChildAutomaton(
                child->getName(),
                children_alphabet[i],
                children_states[i],
                children_weights[i],
                child->getMinDomain(),
                child->getMaxDomain(),
                new_init
            );

            new_children->insert(i, new_child);
        }
    }

    // Construct and return the new nested automaton
    NestedAutomaton* det_nwa = new NestedAutomaton("PsuedoDet(" + this->getName() + ")", new_alphabet, new_states, new_weights, 0, this->getChildrenSize() - 1, new_initial, new_children);
    // det_nwa->print();
    // NOTE: det_nwa is not complete by default, but this is not necessary for the (limavg,summinus)-emptiness pipeline
    // NestedAutomaton* complete_det_nwa = det_nwa->makeCompleteNested();

    for (MacroSymbol* m : macro_alphabet) delete m;
    macro_alphabet.clear();

    // delete det_nwa;
    // return complete_det_nwa;

    return det_nwa;
}

// Synchronization + Ultimate child construction
// NOTE: Output has no marked initial state because it has many disconnected components, so its SCCs are not procesed 
NestedAutomaton* NestedAutomaton::synchronizeChildren() {
    // UTILITY LAMBDAS
    auto sat_mul_u64 = [](uint64_t a, uint64_t b) -> uint64_t {
        if (a == 0 || b == 0) return 0;
        if (a > std::numeric_limits<uint64_t>::max() / b) {
            QUAK_FAIL("Overflow in sat_mul_u64 during synchronizeChildren");
        }
        return a * b;
    };

    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e;
        return nullptr;
    };

    // Reads the payload on a parent edge in the *input* automaton (child-id encoding).
    // Returns <=0 for silent, >0 for called child-id.
    auto old_parent_called_child_id = [](const Edge* me) -> long long {
        if (!me || !me->getWeight()) return 0;
        // The project historically used to_float() for decoding.
        // We round to nearest integer to be robust to "1.0" style encodings.
        const double d = me->getWeight()->getValue().to_float();
        return static_cast<long long>(std::llround(d));
    };

    // COMPUTE CONFIGURATION BOUND X = 2 * conf(this)
    // Compute |Q_slv| = sum of states across all child automata
    uint64_t Qslv_size = 0;
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* c = this->getChild(i);
        Qslv_size += static_cast<uint64_t>(c->getStates()->size());
    }

    // Check for overflow in 2^|Q_slv|
    if (Qslv_size >= 64) {
        QUAK_FAIL("Overflow: |Q_slv| >= 64, configuration bound too large");
    }
    const uint64_t two_power_Qslv = 1ULL << Qslv_size;

    // conf(A) = |Q_m| * 2^|Q_slv|
    const uint64_t nm = static_cast<uint64_t>(this->getStates()->size());
    const uint64_t conf = sat_mul_u64(nm, two_power_Qslv);

    const uint64_t X_u64 = sat_mul_u64(2, conf);

    // BASIC SIZES, ALPHABET SOURCE
    const size_t M = this->getStates()->size();

    // Use any (non-null) child's alphabet as the canonical alphabet.
    // (Assumes all automata use the same alphabet IDs.)
    ChildAutomaton* alpha_src_child = nullptr;
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        if (this->getChild(i) && this->getChild(i)->getNbStates() > 1 && this->getChild(i)->getAlphabet()) {
            alpha_src_child = this->getChild(i);
            break;
        }
    }
    if (!alpha_src_child) QUAK_FAIL("synchronizeChildren: no child alphabet available");

    MapArray<Symbol*>* src_alpha = alpha_src_child->getAlphabet();
    const size_t A = src_alpha->size();

    // BUILD THE SYNCHRONIZED ULTIMATE child U_sync (single child)
    Symbol::RESET();
    State::RESET();
    Weight::RESET();

    // Copy alphabet for U_sync
    MapArray<Symbol*>* calpha = new MapArray<Symbol*>(A);
    for (size_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(src_alpha->at(a));
        calpha->insert(s_new->getId(), s_new);
    }

    // Compute weight statistics over all real children (id > 0)
    bool firstW = true;
    weight_t minW = weight_t(0), maxW = weight_t(0), maxAbsW = weight_t(0);

    for (size_t ci = 1; ci < this->getChildrenSize(); ++ci) {
        ChildAutomaton* child = this->getChild(ci);
        if (!child) continue;
        MapArray<Weight*>* ws = child->getWeights();
        if (!ws) continue;

        for (size_t wi = 0; wi < ws->size(); ++wi) {
            Weight* w = ws->at(wi);
            if (!w) continue;
            const weight_t v = w->getValue();

            if (firstW) { minW = maxW = v; firstW = false; }
            else {
                if (v < minW) minW = v;
                if (v > maxW) maxW = v;
            }

            const weight_t av = (v < weight_t(0)) ? -v : v;
            if (av > maxAbsW) maxAbsW = av;
        }
    }

    // If there were no real weights found, treat as all 0.
    if (firstW) { minW = maxW = maxAbsW = weight_t(0); }

    // Accumulator bound cap = X * maxAbsW
    const weight_t cap = (maxAbsW == weight_t(0))
        ? weight_t(0)
        : weight_t(static_cast<double>(X_u64)) * maxAbsW;

    weight_t accMin = -cap;
    weight_t accMax =  cap;

    // For SumMinus (all weights <= 0), we can tighten to [ -cap, 0 ].
    if (cap == weight_t(0)) {
        accMin = accMax = weight_t(0);
    } else if (maxW <= weight_t(0)) {
        accMin = -cap;
        accMax = weight_t(0);
    } else if (minW >= weight_t(0)) {
        accMin = weight_t(0);
        accMax = cap;
    }

    // Output weights are either 0 (silent parent step) or acc + child_weight (non-silent)
    const weight_t outMinBound = std::min(weight_t(0), accMin + minW);
    const weight_t outMaxBound = std::max(weight_t(0), accMax + maxW);

    // Weight register for U_sync outputs
    std::vector<Weight*> cweights_vec;
    MapStd<weight_t, Weight*> weight_register;

    auto get_weight = [&](weight_t v) -> Weight* {
        if (!weight_register.contains(v)) {
            Weight* w = new Weight(v);
            cweights_vec.push_back(w);
            weight_register.insert(v, w);
        }
        return weight_register.at(v);
    };
    // get_weight(weight_t(0)); // ensure 0 exists TODO: CHECK

    // State space of U_sync, explored on-demand
    struct SyncKey {
        uint32_t parent_id;       // parent state at the *same time step*
        uint32_t child_index;     // which original child component (ultimate-child selector)
        uint32_t child_state_id;  // local state in that child
        weight_t accumulator;     // buffered sum waiting to flush
        bool pending_accept;      // child reached accept on silent parent step, waiting to flush

        bool operator==(const SyncKey& o) const {
            return parent_id == o.parent_id
                && child_index == o.child_index
                && child_state_id == o.child_state_id
                && accumulator == o.accumulator
                && pending_accept == o.pending_accept;
        }
    };

    struct SyncKeyHash {
        size_t operator()(const SyncKey& k) const noexcept {
            size_t h = 1469598103934665603ull;
            auto mix64 = [&](uint64_t x) { h ^= x; h *= 1099511628211ull; };

            mix64(static_cast<uint64_t>(k.parent_id));
            mix64(static_cast<uint64_t>(k.child_index));
            mix64(static_cast<uint64_t>(k.child_state_id));
            mix64(static_cast<uint64_t>(std::hash<weight_t>{}(k.accumulator)));
            mix64(static_cast<uint64_t>(k.pending_accept ? 1 : 0));
            return h;
        }
    };

    std::unordered_map<SyncKey, State*, SyncKeyHash> state_map;
    std::queue<SyncKey> worklist;

    std::vector<State*> cstates_vec;

    auto child_is_final = [&](uint32_t child_index, uint32_t local_sid) -> bool {
        ChildAutomaton* c = this->getChild(child_index);
        if (!c) return false;
        State* s = c->getStates()->at(local_sid);
        return s && s->getFinal();
    };

    auto get_or_make_state = [&](const SyncKey& key) -> State* {
        auto it = state_map.find(key);
        if (it != state_map.end()) return it->second;

        std::ostringstream ss;
        ss << "U_sync_m" << key.parent_id
           << "_c" << key.child_index
           << "_s" << key.child_state_id
           << "_a" << key.accumulator
           << (key.pending_accept ? "_P" : "");

        State* ns = new State(ss.str(), A, outMinBound, outMaxBound);
        cstates_vec.push_back(ns);

        // FINAL iff underlying local state is accepting AND we are not pending (flushed).
        if (child_is_final(key.child_index, key.child_state_id) && !key.pending_accept) {
            ns->setFinal(true);
        }

        state_map.insert({key, ns});
        worklist.push(key);
        return ns;
    };

    // Seed initial states of U_sync for every *calling* parent transition.
    // Also compute the new parent payload table: -1 for silent, else init_state_id in U_sync.
    std::vector<std::vector<long long>> new_parent_payload(
        M, std::vector<long long>(A, -1LL)
    );

    // We remember at least one created "real" seed state to use as the formal initial of U_sync.
    State* some_seed_initial = nullptr;

    for (uint32_t mid = 0; mid < static_cast<uint32_t>(M); ++mid) {
        State* m_state = this->getStates()->at(mid);

        for (uint32_t a = 0; a < static_cast<uint32_t>(A); ++a) {
            Edge* me = first_edge_or_null(m_state->getSuccessors(a));
            if (!me) continue;

            const long long cid = old_parent_called_child_id(me);

            if (cid <= 0) {
                // silent in the input -> silent in the output, encoded as -1
                new_parent_payload[mid][a] = -1;
                continue;
            }

            const uint32_t child_index = static_cast<uint32_t>(cid);
            // if (child_index >= this->getChildrenSize()) QUAK_FAIL("synchronizeChildren: parent calls out-of-range child id");

            ChildAutomaton* child = this->getChild(child_index);
            // if (!child) QUAK_FAIL("synchronizeChildren: null child called by parent");

            const uint32_t s0 = static_cast<uint32_t>(child->getInitial()->getId());

            // The spawn state must encode the parent state at the call site (mid).
            SyncKey start{mid, child_index, s0, weight_t(0), false};
            State* start_state = get_or_make_state(start);

            new_parent_payload[mid][a] = static_cast<long long>(start_state->getId());

            if (!some_seed_initial) some_seed_initial = start_state;
        }
    }

    // // If the parent never calls any child, build a harmless 1-state U_sync.
    // if (!some_seed_initial) {
    //     // Create a single non-final state with 0 self-loops.
    //     SyncKey dead{0, 1, 0, weight_t(0), false}; // child_index/child_state_id unused
    //     State* dead_state = new State("U_sync_dead", A, outMinBound, outMaxBound);
    //     cstates_vec.push_back(dead_state);
    //     some_seed_initial = dead_state;

    //     for (uint32_t a = 0; a < static_cast<uint32_t>(A); ++a) {
    //         Weight* w0 = get_weight(weight_t(0));
    //         Edge* e = new Edge(calpha->at(a), w0, dead_state, dead_state);
    //         dead_state->addSuccessor(e);
    //         dead_state->addPredecessor(e);
    //     }

    //     // No worklist exploration needed.
    //     worklist = std::queue<SyncKey>();
    //     state_map.clear();
    // }

    // BFS exploration of U_sync transitions
    while (!worklist.empty()) {
        SyncKey cur = worklist.front();
        worklist.pop();

        // Terminality: final iff (accepting && !pending)
        if (child_is_final(cur.child_index, cur.child_state_id) && !cur.pending_accept) {
            continue;
        }

        State* cur_node = state_map.at(cur);

        State* m_state = this->getStates()->at(cur.parent_id);
        ChildAutomaton* child = this->getChild(cur.child_index);
        if (!child) QUAK_FAIL("synchronizeChildren: null child during exploration");

        State* s_state = child->getStates()->at(cur.child_state_id);

        for (uint32_t a = 0; a < static_cast<uint32_t>(A); ++a) {
            Edge* parent_edge = first_edge_or_null(m_state->getSuccessors(a));
            if (!parent_edge) continue;

            const long long cid = old_parent_called_child_id(parent_edge);
            const bool parent_is_silent = (cid <= 0);

            const uint32_t next_parent_id = static_cast<uint32_t>(parent_edge->getTo()->getId());

            // child step
            Edge* child_edge = nullptr;
            uint32_t next_child_state_id = cur.child_state_id;
            weight_t child_w = weight_t(0);

            if (!cur.pending_accept) {
                child_edge = first_edge_or_null(s_state->getSuccessors(a));
                if (!child_edge) continue;

                next_child_state_id = static_cast<uint32_t>(child_edge->getTo()->getId());
                child_w = child_edge->getWeight()->getValue();
            }
            // else: frozen in accepting state; no step; child_w = 0; next_child_state_id stays

            if (parent_is_silent) {
                // Emit 0, accumulate child_w, possibly set pending_accept
                const weight_t new_acc = cur.accumulator + child_w;

                if (new_acc < accMin || new_acc > accMax) {
                    // out of allowed accumulator range
                    continue;
                }

                bool new_pending = cur.pending_accept;
                if (!cur.pending_accept && child_edge && child_edge->getTo()->getFinal()) {
                    new_pending = true;
                }

                SyncKey nxt{next_parent_id, cur.child_index, next_child_state_id, new_acc, new_pending};
                State* nxt_node = get_or_make_state(nxt);

                Weight* w0 = get_weight(weight_t(0));
                Edge* e = new Edge(calpha->at(a), w0, cur_node, nxt_node);
                cur_node->addSuccessor(e);
                nxt_node->addPredecessor(e);
            } else {
                // Emit acc + child_w, reset accumulator, clear pending
                const weight_t emit = cur.accumulator + child_w;
                Weight* w_out = get_weight(emit);

                SyncKey nxt{next_parent_id, cur.child_index, next_child_state_id, weight_t(0), false};
                State* nxt_node = get_or_make_state(nxt);

                Edge* e = new Edge(calpha->at(a), w_out, cur_node, nxt_node);
                cur_node->addSuccessor(e);
                nxt_node->addPredecessor(e);
            }
        }
    }

    // finalize U_sync weights min/max
    weight_t realMin = weight_t(0), realMax = weight_t(0);
    bool firstOut = true;
    for (Weight* w : cweights_vec) {
        const weight_t v = w->getValue();
        if (firstOut) { realMin = realMax = v; firstOut = false; }
        else {
            if (v < realMin) realMin = v;
            if (v > realMax) realMax = v;
        }
    }
    if (firstOut) { realMin = realMax = weight_t(0); }

    MapArray<State*>*  cstates  = new MapArray<State*>(cstates_vec.size());
    MapArray<Weight*>* cweights = new MapArray<Weight*>(cweights_vec.size());
    for (State* st : cstates_vec)   cstates->insert(st->getId(), st);
    for (Weight* wt : cweights_vec) cweights->insert(wt->getId(), wt);

    ChildAutomaton* ultimate_synced = new ChildAutomaton(
        "U_sync(" + this->getName() + ")",
        calpha, cstates, cweights,
        realMin, realMax,
        nullptr
    );

    MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(1);
    new_children->insert(0, ultimate_synced);

    // REBUILD THE PARENT WITH NEW PAYLOADS
    // We must rebuild because we need weights -1 / initStateId, and we cannot mutate
    // the old weight list safely.

    Symbol::RESET();
    State::RESET();
    Weight::RESET();

    // Copy parent alphabet
    MapArray<Symbol*>* malpha = new MapArray<Symbol*>(A);
    for (size_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(src_alpha->at(a));
        malpha->insert(s_new->getId(), s_new);
    }

    // Copy parent states
    MapArray<State*>* mstates = new MapArray<State*>(M);
    for (uint32_t sid = 0; sid < static_cast<uint32_t>(M); ++sid) {
        State* ns = new State(this->getStates()->at(sid));
        mstates->insert(ns->getId(), ns);
    }
    State* minitial = mstates->at(this->getInitial()->getId());

    // Copy finals
    for (uint32_t sid = 0; sid < static_cast<uint32_t>(M); ++sid) {
        State* os = this->getStates()->at(sid);
        if (os->getFinal()) {
            State* ns = mstates->at(sid);
            ns->setFinal(true);
        }
    }

    // Parent weights are just payload integers (-1 or U_sync state id).
    std::vector<Weight*> mweights_vec;
    MapStd<long long, Weight*> mweight_reg;

    auto get_mweight = [&](long long payload) -> Weight* {
        if (!mweight_reg.contains(payload)) {
            Weight* w = new Weight(weight_t(static_cast<double>(payload)));
            mweights_vec.push_back(w);
            mweight_reg.insert(payload, w);
        }
        return mweight_reg.at(payload);
    };

    // Create edges
    for (uint32_t sid = 0; sid < static_cast<uint32_t>(M); ++sid) {
        State* os = this->getStates()->at(sid);
        State* nf = mstates->at(sid);

        for (uint32_t a = 0; a < static_cast<uint32_t>(A); ++a) {
            Edge* oe = first_edge_or_null(os->getSuccessors(a));
            if (!oe) continue;

            State* nt = mstates->at(static_cast<uint32_t>(oe->getTo()->getId()));

            const long long payload = new_parent_payload[sid][a]; // -1 or U_sync init state id
            Weight* nw = get_mweight(payload);

            Edge* ne = new Edge(malpha->at(a), nw, nf, nt);
            nf->addSuccessor(ne);
            nt->addPredecessor(ne);
        }
    }

    // Finalize parent weight array and min/max (purely informational)
    MapArray<Weight*>* mweights = new MapArray<Weight*>(mweights_vec.size());
    weight_t pMin = weight_t(0), pMax = weight_t(0);
    bool firstP = true;
    for (Weight* w : mweights_vec) {
        mweights->insert(w->getId(), w);
        const weight_t v = w->getValue();
        if (firstP) { pMin = pMax = v; firstP = false; }
        else {
            if (v < pMin) pMin = v;
            if (v > pMax) pMax = v;
        }
    }
    if (firstP) { pMin = pMax = weight_t(0); }

    // BUILD RESULT NESTED AUTOMATON
    NestedAutomaton* result = new NestedAutomaton(
        "Sync(" + this->getName() + ")",
        malpha, mstates, mweights,
        pMin, pMax,
        minitial,
        new_children
    );
    // result->print();

    return result;
}

// c_bound = 2 * (|Q_slv| + 2) * conf(A) * |Q_slv|^{2*|Q_slv|}
static uint64_t compute_c_bound(const NestedAutomaton* A_det_pre_sync) {
    if (!A_det_pre_sync) QUAK_FAIL("compute_c_bound: null automaton");

    auto sat_mul_u64 = [](uint64_t a, uint64_t b) -> uint64_t {
        if (a == 0 || b == 0) return 0;
        if (a > std::numeric_limits<uint64_t>::max() / b) {
            QUAK_FAIL("Overflow in sat_mul_u64 during compute_c_bound");
        }
        return a * b;
    };

    auto sat_pow_u64 = [&](uint64_t base, uint64_t exp) -> uint64_t {
        uint64_t r = 1;
        while (exp > 0) {
            if (exp & 1ULL) r = sat_mul_u64(r, base);   // will QUAK_FAIL on overflow
            exp >>= 1ULL;
            if (exp) base = sat_mul_u64(base, base);    // will QUAK_FAIL on overflow
        }
        return r;
    };

    auto sat_pow2_u64 = [&](uint64_t exp) -> uint64_t {
        // Shifting by >= 64 is undefined for uint64_t.
        if (exp >= 64ULL) {
            QUAK_FAIL("Overflow in sat_pow2_u64 during compute_c_bound");
        }
        return (1ULL << exp);
    };

    // |Q_m|
    const uint64_t Qm = static_cast<uint64_t>(A_det_pre_sync->getStates()->size());

    // |Q_slv| = sum of sizes of child state sets (disjoint union).
    uint64_t Qslv = 0;
    for (size_t i = 1; i < A_det_pre_sync->getChildrenSize(); ++i) {
        ChildAutomaton* c = A_det_pre_sync->getChild(i);
        if (!c) continue;
        const uint64_t nc = static_cast<uint64_t>(c->getStates()->size());
        if (Qslv > std::numeric_limits<uint64_t>::max() - nc) {
            Qslv = std::numeric_limits<uint64_t>::max();
            break;
        }
        Qslv += nc;
    }

    if (Qslv == 0) {
        return 0; // no real children, multiplicity bound is irrelevant
    }

    // upper bound on number of configurations:
    // conf(A) <= |Q_m| * 2^{|Q_slv|}
    const uint64_t conf_upper = sat_mul_u64(Qm, sat_pow2_u64(Qslv));

    // N = (|Q_slv| + 2) * conf(A) * |Q_slv|^{2|Q_slv|}
    const uint64_t pow_part = sat_pow_u64(Qslv, sat_mul_u64(2ULL, Qslv));
    uint64_t N = sat_mul_u64(Qslv + 2ULL, conf_upper);
    N = sat_mul_u64(N, pow_part);

    // c_bound = 2 * N
    return sat_mul_u64(2ULL, N);
}

// Assumptions:
//  - "this" NWA is (pseudo-)deterministic: <= 1 edge per letter from any state in parent and in the unique child.
//  - "this" has only one child -- the synchronized ultimate child U_sync.
//  - parent edge weight payload: <0 means SILENT, >=0 means spawn from that U_sync state-id.
Automaton* NestedAutomaton::flatten_Avg_SumMinus(uint64_t c_bound) {
    // helpers
    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e;
        return nullptr;
    };

    auto parent_payload_ll = [](const Edge* me) -> long long {
        if (!me || !me->getWeight()) return -1;
        const double d = me->getWeight()->getValue().to_float();
        return static_cast<long long>(std::llround(d));
    };

    // sanity
    if (this->getChildrenSize() != 1) {
        QUAK_FAIL("flatten_Avg_SumMinus: expected exactly one child (ultimate synchronized child)");
    }
    ChildAutomaton* U = this->getChild(0);
    if (!U) QUAK_FAIL("flatten_Avg_SumMinus: null ultimate child");

    MapArray<State*>* mstates_src = this->getStates();
    const uint32_t M = static_cast<uint32_t>(mstates_src->size());

    MapArray<State*>* ustates_src = U->getStates();
    const uint32_t UQ = static_cast<uint32_t>(ustates_src->size());

    MapArray<Symbol*>* alpha_src = U->getAlphabet();
    if (!alpha_src) QUAK_FAIL("flatten_Avg_SumMinus: null alphabet in ultimate child");
    const uint32_t A = static_cast<uint32_t>(alpha_src->size());

    // precompute parent step table
    struct MStep {
        bool exists = false;
        uint32_t to = 0;
        long long payload = -1; // <0 silent, >=0 spawn-id
    };
    std::vector<std::vector<MStep>> mstep(M, std::vector<MStep>(A));

    for (uint32_t ms = 0; ms < M; ++ms) {
        State* s = mstates_src->at(ms);
        for (uint32_t a = 0; a < A; ++a) {
            Edge* e = first_edge_or_null(s->getSuccessors(a));
            if (!e) continue;
            mstep[ms][a].exists = true;
            mstep[ms][a].to = static_cast<uint32_t>(e->getTo()->getId());
            mstep[ms][a].payload = parent_payload_ll(e);
        }
    }

    // precompute child step table
    struct UStep {
        bool exists = false;
        uint32_t to = 0;
        weight_t w = weight_t(0);
        bool to_final = false;
    };
    std::vector<std::vector<UStep>> ustep(UQ, std::vector<UStep>(A));

    for (uint32_t us = 0; us < UQ; ++us) {
        State* s = ustates_src->at(us);
        const bool s_final = s && s->getFinal();
        // finals have no outgoing
        if (s_final) continue;

        for (uint32_t a = 0; a < A; ++a) {
            Edge* e = first_edge_or_null(s->getSuccessors(a));
            if (!e) continue;
            ustep[us][a].exists = true;
            ustep[us][a].to = static_cast<uint32_t>(e->getTo()->getId());
            ustep[us][a].w = e->getWeight()->getValue();
            ustep[us][a].to_final = (e->getTo() && e->getTo()->getFinal());
        }
    }

    // compute weight bounds for State constructor
    weight_t uMin = weight_t(0), uMax = weight_t(0);
    bool firstW = true;
    if (U->getWeights()) {
        for (size_t i = 0; i < U->getWeights()->size(); ++i) {
            Weight* w = U->getWeights()->at(i);
            if (!w) continue;
            const weight_t v = w->getValue();
            if (firstW) { uMin = uMax = v; firstW = false; }
            else {
                if (v < uMin) uMin = v;
                if (v > uMax) uMax = v;
            }
        }
    }
    if (firstW) { uMin = uMax = weight_t(0); }

    // Worst-case per-step sum <= (total active + 1 spawn) * maxAbs.
    weight_t maxAbs = (uMax < weight_t(0)) ? -uMin : std::max(uMax, (uMin < weight_t(0) ? -uMin : uMin));
    const double factor = static_cast<double>(c_bound) * static_cast<double>(UQ) + 1.0;
    weight_t outAbs = weight_t(factor) * maxAbs;

    weight_t outMinBound = (uMin >= weight_t(0)) ? weight_t(0) : -outAbs;
    weight_t outMaxBound = (uMax <= weight_t(0)) ? weight_t(0) : outAbs;

    // build new alphabet
    Symbol::RESET();
    State::RESET();
    Weight::RESET();

    MapArray<Symbol*>* falpha = new MapArray<Symbol*>(A);
    for (uint32_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(alpha_src->at(a));
        falpha->insert(s_new->getId(), s_new);
    }

    // weight register
    std::vector<Weight*> fweights_vec;
    MapStd<weight_t, Weight*> wreg;

    auto get_w = [&](const weight_t& v) -> Weight* {
        if (!wreg.contains(v)) {
            Weight* w = new Weight(v);
            fweights_vec.push_back(w);
            wreg.insert(v, w);
        }
        return wreg.at(v);
    };

    Weight* w_silent = get_w(SILENT); // ensure SILENT exists

    // key type: (parent_id, sparse multiset)
    // mult stored as [sid0,cnt0,sid1,cnt1,...] sorted by sid, only cnt>0.
    struct Key {
        uint32_t mid;
        std::vector<uint32_t> nz;

        bool operator==(const Key& o) const {
            return mid == o.mid && nz == o.nz;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& k) const noexcept {
            size_t h = 1469598103934665603ull;
            auto mix = [&](uint64_t x) {
                h ^= static_cast<size_t>(x);
                h *= 1099511628211ull;
            };
            mix(k.mid);
            for (uint32_t x : k.nz) mix(x);
            return h;
        }
    };

    auto encode_name = [&](const Key& k) -> std::string {
        std::ostringstream ss;
        ss << "F_m" << k.mid << "_";
        for (size_t i = 0; i + 1 < k.nz.size(); i += 2) {
            ss << k.nz[i] << ":" << k.nz[i+1] << ",";
        }
        return ss.str();
    };

    // state table + BFS
    std::unordered_map<Key, State*, KeyHash> stmap;
    std::queue<Key> work;

    std::vector<State*> fstates_vec;
    SetStd<State*>* ffinals = new SetStd<State*>();

    auto parent_is_final = [&](uint32_t mid) -> bool {
        State* ms = mstates_src->at(mid);
        return ms && ms->getFinal();
    };

    auto get_or_make = [&](const Key& k) -> State* {
        auto it = stmap.find(k);
        if (it != stmap.end()) return it->second;

        State* ns = new State(encode_name(k), A, outMinBound, outMaxBound);
        fstates_vec.push_back(ns);

        if (parent_is_final(k.mid)) {
            ns->setFinal(true);
            ffinals->insert(ns);
        }

        stmap.insert({k, ns});
        work.push(k);
        return ns;
    };

    // initial: (parent_initial, empty multiset)
    Key init;
    init.mid = static_cast<uint32_t>(this->getInitial()->getId());
    init.nz.clear();

    State* finitial = get_or_make(init);

    // Transition computation: apply letter a to sparse multiset, with optional spawn.
    auto step_multiset = [&](const std::vector<uint32_t>& nz_in,
                             bool do_spawn, uint32_t spawn_sid,
                             uint32_t a,
                             weight_t& sum_out,
                             std::vector<uint32_t>& nz_out) -> bool
    {
        sum_out = weight_t(0);

        // accumulate counts by current sid
        // (use unordered_map because nz_in is sparse)
        std::unordered_map<uint32_t, uint64_t> cur;
        cur.reserve(nz_in.size() / 2 + 2);

        for (size_t i = 0; i + 1 < nz_in.size(); i += 2) {
            cur[nz_in[i]] += static_cast<uint64_t>(nz_in[i + 1]);
        }
        if (do_spawn) {
            if (spawn_sid >= UQ) return false;
            cur[spawn_sid] += 1ull;
        }

        // advance each active bucket
        std::unordered_map<uint32_t, uint64_t> nxt;
        nxt.reserve(cur.size() + 4);

        for (const auto& kv : cur) {
            const uint32_t sid = kv.first;
            const uint64_t cnt = kv.second;
            if (cnt == 0) continue;

            if (sid >= UQ) return false;
            if (!ustep[sid][a].exists) return false;

            const UStep& st = ustep[sid][a];

            // sum += cnt * st.w
            sum_out = sum_out + (weight_t(static_cast<double>(cnt)) * st.w);

            // drop instances that terminate (reach final)
            if (st.to_final) continue;

            uint64_t& ref = nxt[st.to];
            ref += cnt;

            // enforce per-state cap <= c_bound
            if (ref > c_bound) return false;
        }

        // build canonical nz_out sorted by sid
        std::vector<std::pair<uint32_t, uint64_t>> tmp;
        tmp.reserve(nxt.size());
        for (const auto& kv : nxt) {
            if (kv.second == 0) continue;
            tmp.push_back({kv.first, kv.second});
        }
        std::sort(tmp.begin(), tmp.end(),
                  [](auto& x, auto& y){ return x.first < y.first; });

        nz_out.clear();
        nz_out.reserve(tmp.size() * 2);
        for (auto& p : tmp) {
            nz_out.push_back(p.first);
            nz_out.push_back(static_cast<uint32_t>(p.second));
        }

        return true;
    };

    // BFS build edges
    while (!work.empty()) {
        Key cur = work.front();
        work.pop();

        State* from = stmap.at(cur);

        for (uint32_t a = 0; a < A; ++a) {
            const MStep& ms = mstep[cur.mid][a];
            if (!ms.exists) continue;

            const bool parent_silent = (ms.payload < 0);
            const bool do_spawn = (!parent_silent);
            const uint32_t spawn_sid = do_spawn ? static_cast<uint32_t>(ms.payload) : 0;

            weight_t sumw;
            std::vector<uint32_t> nz_next;

            if (!step_multiset(cur.nz, do_spawn, spawn_sid, a, sumw, nz_next)) {
                continue; // reject this letter from this state
            }

            if (parent_silent && sumw != weight_t(0)) {
                QUAK_FAIL("U_sync violates synchronized silent transitions: nonzero sum on silent parent step");
            }

            Key nxtK;
            nxtK.mid = ms.to;
            nxtK.nz = std::move(nz_next);

            State* to = get_or_make(nxtK);

            Weight* ew = parent_silent ? w_silent : get_w(sumw);

            Edge* e = new Edge(falpha->at(a), ew, from, to);
            from->addSuccessor(e);
            to->addPredecessor(e);
        }
    }

    // finalize arrays and build Automaton
    // weights min/max
    weight_t fMin = weight_t(0), fMax = weight_t(0);
    bool firstOut = true;
    for (Weight* w : fweights_vec) {
        if (!w) continue;
        const weight_t v = w->getValue();
        if (firstOut) { fMin = fMax = v; firstOut = false; }
        else {
            if (v < fMin) fMin = v;
            if (v > fMax) fMax = v;
        }
    }
    if (firstOut) { fMin = fMax = weight_t(0); }

    MapArray<State*>*  fstates  = new MapArray<State*>(fstates_vec.size());
    MapArray<Weight*>* fweights = new MapArray<Weight*>(fweights_vec.size());

    for (State* st : fstates_vec)   fstates->insert(st->getId(), st);
    for (Weight* wt : fweights_vec) fweights->insert(wt->getId(), wt);

    delete ffinals;

    Automaton* flat = new Automaton(
        "Flat(" + this->getName() + ")",
        falpha, fstates, fweights,
        fMin, fMax,
        finitial
    );

    return flat;
}

// Shared exact scaling helper used by cached Sum/threshold backends.
typedef uint64_t internal_weight_t;

static internal_weight_t compute_weight_scale(NestedAutomaton* nwa) {
    internal_weight_t scale = 1;

    auto process_weight = [&scale](float w) {
        // Find decimal places needed for this weight
        float abs_w = (w < 0) ? -w : w;
        internal_weight_t test_scale = 1;
        for (int i = 0; i < 6; ++i) {  // Up to 6 decimal places
            float scaled = abs_w * test_scale;
            float rounded = (float)(int64_t)(scaled + 0.5f);
            if (std::abs(scaled - rounded) < 1e-6f) break;
            test_scale *= 10;
        }
        if (test_scale > scale) scale = test_scale;
    };

    // Process all child weights
    for (unsigned int c = 0; c < nwa->getChildrenSize(); ++c) {
        ChildAutomaton* child = nwa->getChild(c);
        for (State* s : *child->getStates()) {
            auto* alphabet = s->getAlphabet();
            if (!alphabet) continue;
            for (Symbol* sym : *alphabet) {
                auto* succs = s->getSuccessors(sym->getId());
                if (!succs) continue;
                for (Edge* e : *succs) {
                    process_weight(e->getWeight()->getValue().to_float());
                }
            }
        }
    }

    return scale;
}


// ============================================================================
// SHARED THRESHOLD-OBLIGATION BACKEND FOR MONOTONE/EXTREMAL CASES
//
// This generalizes the Min/Max Inf/LimInf threshold-obligation idea to
//   {Sup, LimSup, Inf, LimInf} x {Min_f, Max_f, SumPlus, SumMinus}
// while keeping the original file's proven epoch/final-pulse discipline.
// ============================================================================

using thrext_int_t = uint64_t;
static constexpr thrext_int_t THREXT_INF = std::numeric_limits<thrext_int_t>::max() / 4ull;

enum class ThrExtMode : uint8_t { MAX_F = 0, MIN_F = 1, SUMPLUS = 2, SUMMINUS = 3 };

struct ThrExtConf {
    uint32_t st = 0;
    thrext_int_t prog = 0;

    bool operator==(const ThrExtConf& o) const {
        return st == o.st && prog == o.prog;
    }
    bool operator<(const ThrExtConf& o) const {
        if (st != o.st) return st < o.st;
        return prog < o.prog;
    }
};

using ThrExtFrontier = std::vector<ThrExtConf>;

struct ThrExtOblKey {
    uint32_t child = 0;
    uint8_t guess = 0; // 0 => return < threshold, 1 => return >= threshold
    ThrExtFrontier conf;

    bool operator==(const ThrExtOblKey& o) const {
        return child == o.child && guess == o.guess && conf == o.conf;
    }
    bool operator<(const ThrExtOblKey& o) const {
        if (child != o.child) return child < o.child;
        if (guess != o.guess) return guess < o.guess;
        return conf < o.conf;
    }
};

struct ThrExtOblEntry {
    ThrExtOblKey key;

    bool operator==(const ThrExtOblEntry& o) const { return key == o.key; }
    bool operator<(const ThrExtOblEntry& o) const { return key < o.key; }
};

using ThrExtOblBag = std::vector<ThrExtOblEntry>;

namespace {

struct ThrExtBagStepKey {
    uint32_t symbol = 0;
    ThrExtOblBag bag;

    bool operator<(const ThrExtBagStepKey& o) const {
        if (symbol != o.symbol) return symbol < o.symbol;
        return bag < o.bag;
    }
};

struct MinMaxInfExperimentContext {
    bool enabled = false;
    MinMaxInfExperimentStats stats;
    std::set<ThrExtOblKey> unique_obls;
    std::set<ThrExtOblBag> unique_bags;
    std::set<ThrExtBagStepKey> unique_bag_steps;
    std::set<std::tuple<uint32_t, uint8_t, uint32_t>> unique_spawn_keys;

    void reset_preserving_enabled() {
        const bool keep_enabled = enabled;
        enabled = false;
        stats = MinMaxInfExperimentStats{};
        unique_obls.clear();
        unique_bags.clear();
        unique_bag_steps.clear();
        unique_spawn_keys.clear();
        enabled = keep_enabled;
    }
};

static MinMaxInfExperimentContext g_minmax_inf_experiment;

class ScopedStatsTimer {
public:
    explicit ScopedStatsTimer(double* acc)
        : acc_(acc)
        , start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedStatsTimer() {
        if (!acc_) return;
        const auto end = std::chrono::high_resolution_clock::now();
        *acc_ += std::chrono::duration<double, std::milli>(end - start_).count();
    }

private:
    double* acc_;
    std::chrono::high_resolution_clock::time_point start_;
};

static inline bool mmexp_enabled() {
    return g_minmax_inf_experiment.enabled;
}

static void mmexp_record_thr_bag(uint32_t symbol_id,
                                 const ThrExtOblBag& bag,
                                 const std::vector<ChildTables>& child_tab) {
    if (!mmexp_enabled()) return;

    auto& ctx = g_minmax_inf_experiment;
    ctx.stats.step_bag_calls++;
    ctx.unique_bag_steps.insert(ThrExtBagStepKey{symbol_id, bag});
    ctx.unique_bags.insert(bag);

    for (const ThrExtOblEntry& ent : bag) {
        ctx.unique_obls.insert(ent.key);
        ctx.stats.frontier_observations++;
        ctx.stats.frontier_config_total += ent.key.conf.size();
        if (ent.key.child < child_tab.size()) {
            ctx.stats.frontier_capacity_total += child_tab[ent.key.child].n_states;
        }
    }

    ctx.stats.unique_obligation_count = ctx.unique_obls.size();
    ctx.stats.unique_bag_count = ctx.unique_bags.size();
    ctx.stats.unique_bag_step_keys = ctx.unique_bag_steps.size();
}

static inline void mmexp_record_thr_spawn(uint32_t child, uint8_t guess, uint32_t symbol_id) {
    if (!mmexp_enabled()) return;
    auto& ctx = g_minmax_inf_experiment;
    ctx.stats.spawn_calls++;
    ctx.unique_spawn_keys.insert(std::make_tuple(child, guess, symbol_id));
    ctx.stats.unique_spawn_keys = ctx.unique_spawn_keys.size();
}

static inline void mmexp_record_thr_bag_copy(const ThrExtOblBag& src) {
    if (!mmexp_enabled()) return;
    auto& ctx = g_minmax_inf_experiment;
    ctx.stats.bag_copy_ops++;
    ctx.stats.bag_copy_entries += src.size();
}

} // namespace

void NestedAutomaton::setMinMaxInfExperimentStatsEnabled(bool enabled) {
    g_minmax_inf_experiment.enabled = enabled;
}

void NestedAutomaton::resetMinMaxInfExperimentStats() {
    g_minmax_inf_experiment.reset_preserving_enabled();
}

MinMaxInfExperimentStats NestedAutomaton::getMinMaxInfExperimentStats() {
    return g_minmax_inf_experiment.stats;
}

static inline void thrext_bag_add(ThrExtOblBag& bag, ThrExtOblEntry&& e) {
    auto it = std::lower_bound(bag.begin(), bag.end(), e);
    if (it == bag.end() || !(it->key == e.key)) {
        bag.insert(it, std::move(e));
    }
}

static inline void thrext_bag_finalize(ThrExtOblBag& bag) {
    (void)bag;
}

struct ThrExtBuchiState {
    State* parent_state = nullptr;
    ThrExtOblBag P1;
    ThrExtOblBag P2;
    acc_phase_t phase = ACC_WAIT_parent;
    bool epoch_nonempty = false;

    bool operator==(const ThrExtBuchiState& o) const {
        return parent_state == o.parent_state
            && P1 == o.P1
            && P2 == o.P2
            && phase == o.phase
            && epoch_nonempty == o.epoch_nonempty;
    }

    bool operator<(const ThrExtBuchiState& o) const {
        if (parent_state != o.parent_state) return parent_state < o.parent_state;
        if (P1 != o.P1) return P1 < o.P1;
        if (P2 != o.P2) return P2 < o.P2;
        if (phase != o.phase) return phase < o.phase;
        return epoch_nonempty < o.epoch_nonempty;
    }
};

static inline acc_phase_t advance_phase_thrext(const ThrExtBuchiState& s) {
    if (s.phase == ACC_WAIT_parent) {
        return (s.parent_state->getFinal()) ? ACC_WAIT_P2EMPTY : ACC_WAIT_parent;
    }
    return (s.P2.empty()) ? ACC_WAIT_parent : ACC_WAIT_P2EMPTY;
}

struct ThrExtChildInfo {
    bool enabled = false;
    ThrExtMode mode = ThrExtMode::MAX_F;
    weight_t raw_threshold = weight_t(0);
    thrext_int_t weight_scale = 1;
    thrext_int_t goal = 0;
    thrext_int_t cap = 0;
    bool forced = false;
    uint8_t forced_guess = 0;

    std::vector<uint8_t> mm_live[2][2];
    std::vector<thrext_int_t> min_extra;
    std::vector<thrext_int_t> max_extra;
};

static inline thrext_int_t thrext_sat_add_cap(thrext_int_t a, thrext_int_t b, thrext_int_t cap) {
    if (a >= cap || b >= cap) return cap;
    return (a > cap - b) ? cap : (a + b);
}

static inline thrext_int_t thrext_sat_add_inf(thrext_int_t a, thrext_int_t b) {
    if (a >= THREXT_INF || b >= THREXT_INF) return THREXT_INF;
    return (a > THREXT_INF - b) ? THREXT_INF : (a + b);
}

static inline thrext_int_t thrext_scale_weight(weight_t w, thrext_int_t scale) {
    const double scaled = static_cast<double>(w.to_float()) * static_cast<double>(scale);
    return static_cast<thrext_int_t>(std::llround(scaled));
}

static inline thrext_int_t thrext_scale_threshold_ceil(weight_t w, thrext_int_t scale) {
    const double scaled = static_cast<double>(w.to_float()) * static_cast<double>(scale);
    return static_cast<thrext_int_t>(std::ceil(scaled - 1e-9));
}

static inline thrext_int_t thrext_scale_threshold_floor_nonneg(weight_t w, thrext_int_t scale) {
    const double scaled = static_cast<double>(w.to_float()) * static_cast<double>(scale);
    return static_cast<thrext_int_t>(std::floor(scaled + 1e-9));
}

static inline ThrExtMode thrext_mode_from_fin(value_function_t finVal) {
    if (finVal == Max_f) return ThrExtMode::MAX_F;
    if (finVal == Min_f) return ThrExtMode::MIN_F;
    if (finVal == SumPlus) return ThrExtMode::SUMPLUS;
    if (finVal == SumMinus) return ThrExtMode::SUMMINUS;
    QUAK_FAIL("Unsupported shared threshold backend mode");
    return ThrExtMode::MAX_F;
}

static inline bool thrext_child_uses_tracking(ChildAutomaton* c) {
    return c && c->getStates()->size() >= 2;
}

static inline bool thrext_prefers_larger(const ThrExtChildInfo& info, uint8_t guess) {
    switch (info.mode) {
        case ThrExtMode::MAX_F:
        case ThrExtMode::MIN_F:
        case ThrExtMode::SUMPLUS:
            return guess == 1u;
        case ThrExtMode::SUMMINUS:
            return guess == 0u;
    }
    return false;
}

static inline thrext_int_t thrext_init_prog(const ThrExtChildInfo& info) {
    switch (info.mode) {
        case ThrExtMode::MAX_F: return 0u;
        case ThrExtMode::MIN_F: return 1u;
        case ThrExtMode::SUMPLUS: return 0u;
        case ThrExtMode::SUMMINUS: return 0u;
    }
    return 0u;
}

static inline thrext_int_t thrext_step_prog(const ThrExtChildInfo& info,
                                            thrext_int_t prog,
                                            const weight_t& edge_w) {
    switch (info.mode) {
        case ThrExtMode::MAX_F: {
            const bool high = !(edge_w < info.raw_threshold);
            return (prog != 0u || high) ? 1u : 0u;
        }
        case ThrExtMode::MIN_F: {
            const bool high = !(edge_w < info.raw_threshold);
            return (prog != 0u && high) ? 1u : 0u;
        }
        case ThrExtMode::SUMPLUS: {
            if (edge_w < weight_t(0)) {
                QUAK_FAIL("Threshold backend for SumPlus requires non-negative child weights");
            }
            const thrext_int_t cost = thrext_scale_weight(edge_w, info.weight_scale);
            return thrext_sat_add_cap(prog, cost, info.cap);
        }
        case ThrExtMode::SUMMINUS: {
            const weight_t abs_w = (edge_w < weight_t(0)) ? -edge_w : edge_w;
            const thrext_int_t cost = thrext_scale_weight(abs_w, info.weight_scale);
            return thrext_sat_add_cap(prog, cost, info.cap);
        }
    }
    return prog;
}

static inline bool thrext_discharge_ok(const ThrExtChildInfo& info,
                                       uint8_t guess,
                                       thrext_int_t prog) {
    if (info.forced) {
        return guess == info.forced_guess;
    }

    switch (info.mode) {
        case ThrExtMode::MAX_F:
        case ThrExtMode::MIN_F:
            return prog == static_cast<thrext_int_t>(guess);
        case ThrExtMode::SUMPLUS:
            return (guess == 1u) ? (prog >= info.goal) : (prog < info.goal);
        case ThrExtMode::SUMMINUS:
            return (guess == 1u) ? (prog <= info.goal) : (prog >= info.cap);
    }
    return false;
}

static void thrext_frontier_canonicalize(ThrExtFrontier& fr,
                                         const ThrExtChildInfo& info,
                                         uint8_t guess) {
    if (fr.empty()) return;

    std::sort(fr.begin(), fr.end(), [](const ThrExtConf& a, const ThrExtConf& b) {
        if (a.st != b.st) return a.st < b.st;
        return a.prog < b.prog;
    });

    ThrExtFrontier out;
    out.reserve(fr.size());

    const bool prefer_larger = thrext_prefers_larger(info, guess);
    size_t i = 0;
    while (i < fr.size()) {
        size_t j = i + 1;
        thrext_int_t best = fr[i].prog;
        while (j < fr.size() && fr[j].st == fr[i].st) {
            if (prefer_larger) {
                if (fr[j].prog > best) best = fr[j].prog;
            } else {
                if (fr[j].prog < best) best = fr[j].prog;
            }
            ++j;
        }
        out.push_back(ThrExtConf{fr[i].st, best});
        i = j;
    }

    fr = std::move(out);
}

static void thrext_build_mm_live(const ChildTables& T, ThrExtChildInfo& info) {
    const uint32_t prod_sz = T.n_states * 2u;
    std::vector<std::vector<uint32_t>> rev(prod_sz);

    auto node_id = [](uint32_t st, uint8_t p) -> uint32_t {
        return (st << 1u) | static_cast<uint32_t>(p);
    };

    // A child final state is a chance to stop now, not a mandatory sink.
    // If a tracker keeps (state, prog), the child already chose to continue.
    for (uint32_t st = 0; st < T.n_states; ++st) {
        for (uint8_t p = 0; p <= 1u; ++p) {
            for (uint32_t sym = 0; sym < T.alph; ++sym) {
                const uint32_t cell = T.idx(st, sym);
                const uint32_t b = T.off[static_cast<size_t>(cell)];
                const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                for (uint32_t pos = b; pos < e; ++pos) {
                    const auto& tr = T.edges[static_cast<size_t>(pos)];
                    if (tr.to >= T.n_states) continue;

                    const uint8_t p2 = static_cast<uint8_t>(thrext_step_prog(info, p, tr.w));
                    rev[node_id(tr.to, p2)].push_back(node_id(st, p));
                }
            }
        }
    }

    for (uint8_t guess = 0; guess <= 1u; ++guess) {
        std::vector<uint8_t> seen(prod_sz, 0u);
        std::deque<uint32_t> q;

        for (uint32_t st = 0; st < T.n_states; ++st) {
            for (uint8_t p = 0; p <= 1u; ++p) {
                bool seed = false;

                for (uint32_t sym = 0; sym < T.alph && !seed; ++sym) {
                    const uint32_t cell = T.idx(st, sym);
                    const uint32_t b = T.off[static_cast<size_t>(cell)];
                    const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                    for (uint32_t pos = b; pos < e; ++pos) {
                        const auto& tr = T.edges[static_cast<size_t>(pos)];
                        if (tr.to >= T.n_states) continue;
                        if (!T.is_final[tr.to]) continue;

                        const uint8_t p2 = static_cast<uint8_t>(thrext_step_prog(info, p, tr.w));
                        if (thrext_discharge_ok(info, guess, p2)) {
                            seed = true;
                            break;
                        }
                    }
                }

                if (seed) {
                    const uint32_t u = node_id(st, p);
                    if (!seen[u]) {
                        seen[u] = 1u;
                        q.push_back(u);
                    }
                }
            }
        }

        while (!q.empty()) {
            const uint32_t v = q.front();
            q.pop_front();

            for (uint32_t u : rev[v]) {
                if (!seen[u]) {
                    seen[u] = 1u;
                    q.push_back(u);
                }
            }
        }

        for (uint8_t p = 0; p <= 1u; ++p) {
            info.mm_live[guess][p].assign(T.n_states, 0u);
        }

        for (uint32_t st = 0; st < T.n_states; ++st) {
            for (uint8_t p = 0; p <= 1u; ++p) {
                if (seen[node_id(st, p)]) {
                    info.mm_live[guess][p][st] = 1u;
                }
            }
        }
    }
}

struct ThrExtRevCostEdge {
    uint32_t from = 0;
    thrext_int_t cost = 0;
};

static void thrext_build_sum_cutoffs(const ChildTables& T, ThrExtChildInfo& info) {
    std::vector<std::vector<ThrExtRevCostEdge>> rev(T.n_states);

    for (uint32_t st = 0; st < T.n_states; ++st) {
        for (uint32_t sym = 0; sym < T.alph; ++sym) {
            const uint32_t cell = T.idx(st, sym);
            const uint32_t b = T.off[static_cast<size_t>(cell)];
            const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

            for (uint32_t pos = b; pos < e; ++pos) {
                const auto& tr = T.edges[static_cast<size_t>(pos)];
                if (tr.to >= T.n_states) continue;

                const thrext_int_t cost = [&]() -> thrext_int_t {
                    if (info.mode == ThrExtMode::SUMPLUS) {
                        if (tr.w < weight_t(0)) {
                            QUAK_FAIL("Threshold backend for SumPlus requires non-negative child weights");
                        }
                        return thrext_scale_weight(tr.w, info.weight_scale);
                    }
                    const weight_t abs_w = (tr.w < weight_t(0)) ? -tr.w : tr.w;
                    return thrext_scale_weight(abs_w, info.weight_scale);
                }();

                rev[tr.to].push_back(ThrExtRevCostEdge{st, cost});
            }
        }
    }

    info.min_extra.assign(T.n_states, THREXT_INF);
    using DijkstraItem = std::pair<thrext_int_t, uint32_t>;
    std::priority_queue<DijkstraItem, std::vector<DijkstraItem>, std::greater<DijkstraItem>> pq;

    for (uint32_t st = 0; st < T.n_states; ++st) {
        if (T.is_final[st]) {
            info.min_extra[st] = 0u;
            pq.push({0u, st});
        }
    }

    while (!pq.empty()) {
        const auto [dist, v] = pq.top();
        pq.pop();
        if (dist != info.min_extra[v]) continue;

        for (const auto& re : rev[v]) {
            const thrext_int_t nd = thrext_sat_add_inf(dist, re.cost);
            if (nd < info.min_extra[re.from]) {
                info.min_extra[re.from] = nd;
                pq.push({nd, re.from});
            }
        }
    }

    // Seed every state that can reach a final once. This allows the reverse
    // relaxation to discover positive-gain cycles hidden behind zero-cost exits.
    info.max_extra.assign(T.n_states, 0u);
    std::deque<uint32_t> q;
    std::vector<uint8_t> in_q(T.n_states, 0u);

    for (uint32_t st = 0; st < T.n_states; ++st) {
        if (!T.live.empty() && !T.live[st]) continue;
        q.push_back(st);
        in_q[st] = 1u;
    }

    while (!q.empty()) {
        const uint32_t v = q.front();
        q.pop_front();
        in_q[v] = 0u;

        for (const auto& re : rev[v]) {
            if (!T.live.empty() && !T.live[re.from]) continue;
            const thrext_int_t cand = thrext_sat_add_cap(info.max_extra[v], re.cost, info.cap);
            if (cand > info.max_extra[re.from]) {
                info.max_extra[re.from] = cand;
                if (!in_q[re.from]) {
                    in_q[re.from] = 1u;
                    q.push_back(re.from);
                }
            }
        }
    }
}

static bool thrext_build_child_info(const ChildTables& T,
                                    value_function_t finVal,
                                    weight_t threshold,
                                    thrext_int_t weight_scale,
                                    ThrExtChildInfo& info) {
    if (!T.child || T.n_states == 0 || T.alph == 0) return false;

    info = ThrExtChildInfo{};
    info.enabled = true;
    info.mode = thrext_mode_from_fin(finVal);
    info.raw_threshold = threshold;
    info.weight_scale = weight_scale;

    if (info.mode == ThrExtMode::SUMPLUS) {
        if (threshold <= weight_t(0)) {
            info.forced = true;
            info.forced_guess = 1u;
            info.goal = 0u;
            info.cap = 0u;
        } else {
            info.goal = thrext_scale_threshold_ceil(threshold, weight_scale);
            info.cap = info.goal;
        }
        thrext_build_sum_cutoffs(T, info);
    } else if (info.mode == ThrExtMode::SUMMINUS) {
        if (threshold > weight_t(0)) {
            info.forced = true;
            info.forced_guess = 0u;
            info.goal = 0u;
            info.cap = 0u;
        } else {
            info.goal = thrext_scale_threshold_floor_nonneg(-threshold, weight_scale);
            info.cap = info.goal + 1u;
        }
        thrext_build_sum_cutoffs(T, info);
    } else {
        info.goal = 1u;
        info.cap = 1u;
        thrext_build_mm_live(T, info);
    }

    return true;
}

static inline bool thrext_is_live(const ChildTables& T,
                                  const ThrExtChildInfo& info,
                                  uint8_t guess,
                                  uint32_t st,
                                  thrext_int_t prog) {
    if (st >= T.n_states) return false;
    if (!T.live.empty() && !T.live[st]) return false;
    if (info.forced) return guess == info.forced_guess;

    switch (info.mode) {
        case ThrExtMode::MAX_F:
        case ThrExtMode::MIN_F: {
            if (prog > 1u) return false;
            if (info.mm_live[guess][prog].empty()) return false;
            return info.mm_live[guess][prog][st] != 0u;
        }
        case ThrExtMode::SUMPLUS: {
            if (guess == 1u) {
                return thrext_sat_add_cap(prog, info.max_extra[st], info.cap) >= info.goal;
            }
            if (info.min_extra[st] >= THREXT_INF) return false;
            return thrext_sat_add_inf(prog, info.min_extra[st]) < info.goal;
        }
        case ThrExtMode::SUMMINUS: {
            if (guess == 1u) {
                if (info.min_extra[st] >= THREXT_INF) return false;
                return thrext_sat_add_inf(prog, info.min_extra[st]) <= info.goal;
            }
            return thrext_sat_add_cap(prog, info.max_extra[st], info.cap) >= info.cap;
        }
    }

    return false;
}

enum class ThrExtStepStatus : uint8_t { DEAD = 0, DISCHARGED = 1, NONEMPTY = 2 };

static ThrExtStepStatus thrext_step_frontier(uint32_t child_idx,
                                             uint8_t guess,
                                             const ThrExtFrontier& in_conf,
                                             uint32_t symbol_id,
                                             ThrExtFrontier& next_conf,
                                             const std::vector<ChildTables>& child_tab,
                                             const std::vector<ThrExtChildInfo>& child_info) {
    next_conf.clear();

    if (child_idx >= child_tab.size() || child_idx >= child_info.size()) {
        return ThrExtStepStatus::DEAD;
    }

    const ChildTables& T = child_tab[child_idx];
    const ThrExtChildInfo& info = child_info[child_idx];
    if (!T.child || !info.enabled) return ThrExtStepStatus::DEAD;
    if (symbol_id >= T.alph) return ThrExtStepStatus::DEAD;
    if (in_conf.empty()) return ThrExtStepStatus::DEAD;

    next_conf.reserve(in_conf.size());

    for (const ThrExtConf& c : in_conf) {
        if (!thrext_is_live(T, info, guess, c.st, c.prog)) continue;

        const uint32_t cell = T.idx(c.st, symbol_id);
        const uint32_t b = T.off[static_cast<size_t>(cell)];
        const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

        for (uint32_t pos = b; pos < e; ++pos) {
            const auto& tr = T.edges[static_cast<size_t>(pos)];
            if (tr.to >= T.n_states) continue;

            const thrext_int_t prog2 = thrext_step_prog(info, c.prog, tr.w);
            if (T.is_final[tr.to] &&
                thrext_discharge_ok(info, guess, prog2)) {
                next_conf.clear();
                return ThrExtStepStatus::DISCHARGED;
            }

            if (!thrext_is_live(T, info, guess, tr.to, prog2)) continue;
            next_conf.push_back(ThrExtConf{tr.to, prog2});
        }
    }

    thrext_frontier_canonicalize(next_conf, info, guess);
    if (next_conf.empty()) return ThrExtStepStatus::DEAD;
    return ThrExtStepStatus::NONEMPTY;
}

static bool thrext_step_obl_bag(const ThrExtOblBag& in,
                                uint32_t symbol_id,
                                ThrExtOblBag& out,
                                bool* any_discharged,
                                const std::vector<ChildTables>& child_tab,
                                const std::vector<ThrExtChildInfo>& child_info) {
    ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_step_bag_ms : nullptr);
    mmexp_record_thr_bag(symbol_id, in, child_tab);
    out.clear();
    if (any_discharged) *any_discharged = false;
    if (in.empty()) return true;

    for (const ThrExtOblEntry& ent : in) {
        ThrExtFrontier next_conf;
        const ThrExtStepStatus st = thrext_step_frontier(ent.key.child,
                                                         ent.key.guess,
                                                         ent.key.conf,
                                                         symbol_id,
                                                         next_conf,
                                                         child_tab,
                                                         child_info);
        if (st == ThrExtStepStatus::DEAD) return false;
        if (st == ThrExtStepStatus::DISCHARGED) {
            if (any_discharged) *any_discharged = true;
            continue;
        }

        thrext_bag_add(out, ThrExtOblEntry{
            ThrExtOblKey{ent.key.child, ent.key.guess, std::move(next_conf)}
        });
    }

    thrext_bag_finalize(out);
    return true;
}

enum class ThrExtSpawnStatus : uint8_t { REJECT = 0, EMPTY = 1, NONEMPTY = 2 };

static ThrExtSpawnStatus thrext_spawn_frontier(uint32_t child_idx,
                                               uint32_t symbol_id,
                                               uint8_t guess,
                                               ThrExtFrontier& conf,
                                               const std::vector<ChildTables>& child_tab,
                                               const std::vector<ThrExtChildInfo>& child_info) {
    if (child_idx >= child_tab.size() || child_idx >= child_info.size()) {
        return ThrExtSpawnStatus::REJECT;
    }

    const ChildTables& T = child_tab[child_idx];
    const ThrExtChildInfo& info = child_info[child_idx];
    if (!T.child || !info.enabled) return ThrExtSpawnStatus::REJECT;
    if (symbol_id >= T.alph) return ThrExtSpawnStatus::REJECT;
    if (T.init >= T.n_states) return ThrExtSpawnStatus::REJECT;

    conf.clear();
    const thrext_int_t p0 = thrext_init_prog(info);

    const uint32_t cell = T.idx(T.init, symbol_id);
    const uint32_t b = T.off[static_cast<size_t>(cell)];
    const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

    for (uint32_t pos = b; pos < e; ++pos) {
        const auto& tr = T.edges[static_cast<size_t>(pos)];
        if (tr.to >= T.n_states) continue;

        const thrext_int_t prog2 = thrext_step_prog(info, p0, tr.w);
        if (T.is_final[tr.to] &&
            thrext_discharge_ok(info, guess, prog2)) {
            return ThrExtSpawnStatus::EMPTY;
        }

        if (!thrext_is_live(T, info, guess, tr.to, prog2)) continue;
        conf.push_back(ThrExtConf{tr.to, prog2});
    }

    thrext_frontier_canonicalize(conf, info, guess);
    if (conf.empty()) return ThrExtSpawnStatus::REJECT;
    return ThrExtSpawnStatus::NONEMPTY;
}

static ThrExtSpawnStatus thrext_spawn_obligation(uint32_t child_idx,
                                                 uint32_t symbol_id,
                                                 uint8_t guess,
                                                 ThrExtOblEntry& spawned,
                                                 const std::vector<ChildTables>& child_tab,
                                                 const std::vector<ThrExtChildInfo>& child_info) {
    ThrExtFrontier conf;
    const ThrExtSpawnStatus st = thrext_spawn_frontier(child_idx,
                                                       symbol_id,
                                                       guess,
                                                       conf,
                                                       child_tab,
                                                       child_info);
    if (st == ThrExtSpawnStatus::NONEMPTY) {
        spawned = ThrExtOblEntry{ThrExtOblKey{child_idx, guess, std::move(conf)}};
    }
    return st;
}

// Track ONE child token explicitly for weight, others as background
// Returns a flattened automaton with 0/1 weights encoding threshold achievement
Automaton* NestedAutomaton::flatten_SumPlusMinus_Sup(value_function_t finite_aggregator,
                                                     weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Sup requires SumPlus or SumMinus");
    }
    return this->flatten_SumPlusMinus_Sup_witness_cached(finite_aggregator, threshold);
}

/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////


// Returns a flattened automaton with 0/1 weights encoding threshold achievement
Automaton* NestedAutomaton::flatten_SumPlusMinus_Inf(value_function_t finite_aggregator,
                                                     weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Inf requires SumPlus or SumMinus");
    }
    return this->flatten_SumPlusMinus_Inf_cached(finite_aggregator, threshold);
}

/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////






// Returns a flattened automaton with 0/1 weights encoding threshold achievement
Automaton* NestedAutomaton::flatten_MinMax_Sup(value_function_t finite_aggregator,
                                               weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Sup requires Max_f or Min_f");
    }
    return this->flatten_MinMax_Sup_witness_cached(finite_aggregator, threshold);
}

Automaton* NestedAutomaton::flatten_MinMax_Inf(value_function_t finite_aggregator,
                                                      weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf requires Max_f or Min_f");
    }
    return this->flatten_MinMax_Inf_cached(finite_aggregator, threshold);
}

// This reuses the OblBag phase machine but tracks only the boolean threshold
// outcome of each child invocation.
// ============================================================================

struct MMThrFrontier {
    std::vector<uint64_t> y0;
    std::vector<uint64_t> y1;

    bool operator==(const MMThrFrontier& o) const {
        return y0 == o.y0 && y1 == o.y1;
    }

    bool operator<(const MMThrFrontier& o) const {
        if (y0 != o.y0) return y0 < o.y0;
        return y1 < o.y1;
    }
};

struct MMThrOblKey {
    uint32_t child = 0;
    uint8_t guess = 0;
    MMThrFrontier fr;

    bool operator==(const MMThrOblKey& o) const {
        return child == o.child && guess == o.guess && fr == o.fr;
    }

    bool operator<(const MMThrOblKey& o) const {
        if (child != o.child) return child < o.child;
        if (guess != o.guess) return guess < o.guess;
        return fr < o.fr;
    }
};

struct MMThrOblEntry {
    MMThrOblKey key;

    bool operator==(const MMThrOblEntry& o) const { return key == o.key; }
    bool operator<(const MMThrOblEntry& o) const { return key < o.key; }
};

using MMThrOblBag = std::vector<MMThrOblEntry>;

static inline size_t mmthr_word_count(uint32_t n_states) {
    return (static_cast<size_t>(n_states) + 63u) >> 6u;
}

static inline void mmthr_bits_zero(std::vector<uint64_t>& bits, size_t words) {
    bits.assign(words, 0ull);
}

static inline bool mmthr_bits_empty(const std::vector<uint64_t>& bits) {
    for (uint64_t w : bits) {
        if (w != 0ull) return false;
    }
    return true;
}

static inline void mmthr_bits_set(std::vector<uint64_t>& bits, uint32_t idx) {
    bits[idx >> 6u] |= (1ull << (idx & 63u));
}

static inline bool mmthr_bits_test(const std::vector<uint64_t>& bits, uint32_t idx) {
    return ((bits[idx >> 6u] >> (idx & 63u)) & 1ull) != 0ull;
}

template <typename Fn>
static inline void mmthr_for_each_set_bit(const std::vector<uint64_t>& bits, Fn&& fn) {
    for (size_t wi = 0; wi < bits.size(); ++wi) {
        uint64_t word = bits[wi];
        while (word != 0ull) {
            const unsigned bit = static_cast<unsigned>(__builtin_ctzll(word));
            fn(static_cast<uint32_t>((wi << 6u) + bit));
            word &= (word - 1ull);
        }
    }
}

static inline void mmthr_frontier_zero(MMThrFrontier& fr, size_t words) {
    mmthr_bits_zero(fr.y0, words);
    mmthr_bits_zero(fr.y1, words);
}

static inline bool mmthr_frontier_empty(const MMThrFrontier& fr) {
    return mmthr_bits_empty(fr.y0) && mmthr_bits_empty(fr.y1);
}

static inline void mmthr_frontier_canonicalize(MMThrFrontier& fr,
                                               bool finite_is_max,
                                               uint8_t guess) {
    if (finite_is_max && guess == 1u) {
        const size_t words = std::min(fr.y0.size(), fr.y1.size());
        for (size_t i = 0; i < words; ++i) {
            fr.y0[i] &= ~fr.y1[i];
        }
    } else if (!finite_is_max && guess == 0u) {
        const size_t words = std::min(fr.y0.size(), fr.y1.size());
        for (size_t i = 0; i < words; ++i) {
            fr.y1[i] &= ~fr.y0[i];
        }
    }
}

static inline void mmthr_bag_add(MMThrOblBag& bag, MMThrOblEntry&& e) {
    auto it = std::lower_bound(bag.begin(), bag.end(), e);
    if (it == bag.end() || !(it->key == e.key)) {
        bag.insert(it, std::move(e));
    }
}

static inline void mmthr_bag_finalize(MMThrOblBag& bag) {
    (void)bag;
}

static inline uint8_t mmthr_y_update(bool finite_is_max,
                                     uint8_t y,
                                     const weight_t& edge_w,
                                     const weight_t& threshold) {
    const bool high = !(edge_w < threshold);
    if (finite_is_max) {
        return static_cast<uint8_t>((y != 0u || high) ? 1u : 0u);
    }
    return static_cast<uint8_t>((y != 0u && high) ? 1u : 0u);
}

struct MMThrLivePerChild {
    uint32_t n_states = 0;
    uint32_t n_words = 0;
    std::vector<uint64_t> live[2][2];
};

struct MMThrLive {
    std::vector<MMThrLivePerChild> per_child;

    bool is_live(uint32_t child, uint8_t guess, uint32_t st, uint8_t y) const {
        if (child >= per_child.size()) return false;
        if (guess > 1u || y > 1u) return false;
        const MMThrLivePerChild& C = per_child[child];
        if (st >= C.n_states) return false;
        if (C.live[guess][y].empty()) return false;
        return mmthr_bits_test(C.live[guess][y], st);
    }
};

static MMThrLive build_mmthr_live(const std::vector<ChildTables>& child_tab,
                                  bool finite_is_max,
                                  const weight_t& threshold) {
    MMThrLive out;
    out.per_child.resize(child_tab.size());

    for (uint32_t cid = 0; cid < static_cast<uint32_t>(child_tab.size()); ++cid) {
        const ChildTables& T = child_tab[cid];
        if (!T.child || T.n_states == 0 || T.alph == 0) continue;

        MMThrLivePerChild& C = out.per_child[cid];
        C.n_states = T.n_states;
        C.n_words = static_cast<uint32_t>(mmthr_word_count(T.n_states));

        for (unsigned int guess = 0; guess < 2; ++guess) {
            for (unsigned int y = 0; y < 2; ++y) {
                C.live[guess][y].assign(C.n_words, 0ull);
            }
        }

        const uint32_t prod_sz = T.n_states * 2u;
        std::vector<std::vector<uint32_t>> rev(prod_sz);

        auto node_id = [](uint32_t st, uint8_t y) -> uint32_t {
            return (st << 1u) | static_cast<uint32_t>(y);
        };

        // Final child states are continuation states too. Reaching a final
        // state is a chance to terminate now, not a mandatory sink. Liveness
        // is seeded only by one future edge into a matching final.
        for (uint32_t st = 0; st < T.n_states; ++st) {
            for (uint8_t y = 0; y <= 1u; ++y) {
                for (uint32_t sym = 0; sym < T.alph; ++sym) {
                    const uint32_t cell = T.idx(st, sym);
                    const uint32_t b = T.off[static_cast<size_t>(cell)];
                    const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                    for (uint32_t p = b; p < e; ++p) {
                        const auto& tr = T.edges[static_cast<size_t>(p)];
                        if (tr.to >= T.n_states) continue;

                        const uint8_t y2 = mmthr_y_update(finite_is_max, y, tr.w, threshold);
                        rev[node_id(tr.to, y2)].push_back(node_id(st, y));
                    }
                }
            }
        }

        for (uint8_t guess = 0; guess <= 1u; ++guess) {
            std::vector<uint8_t> seen(prod_sz, 0u);
            std::deque<uint32_t> q;

            for (uint32_t st = 0; st < T.n_states; ++st) {
                for (uint8_t y = 0; y <= 1u; ++y) {
                    bool seed = false;

                    for (uint32_t sym = 0; sym < T.alph && !seed; ++sym) {
                        const uint32_t cell = T.idx(st, sym);
                        const uint32_t b = T.off[static_cast<size_t>(cell)];
                        const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                        for (uint32_t p = b; p < e; ++p) {
                            const auto& tr = T.edges[static_cast<size_t>(p)];
                            if (tr.to >= T.n_states) continue;
                            if (!T.is_final[tr.to]) continue;

                            const uint8_t y2 = mmthr_y_update(finite_is_max, y, tr.w, threshold);
                            if (y2 == guess) {
                                seed = true;
                                break;
                            }
                        }
                    }

                    if (seed) {
                        const uint32_t u = node_id(st, y);
                        if (!seen[u]) {
                            seen[u] = 1u;
                            q.push_back(u);
                        }
                    }
                }
            }

            while (!q.empty()) {
                const uint32_t v = q.front();
                q.pop_front();
                for (uint32_t u : rev[v]) {
                    if (!seen[u]) {
                        seen[u] = 1u;
                        q.push_back(u);
                    }
                }
            }

            for (uint32_t st = 0; st < T.n_states; ++st) {
                for (uint8_t y = 0; y <= 1u; ++y) {
                    if (seen[node_id(st, y)]) {
                        mmthr_bits_set(C.live[guess][y], st);
                    }
                }
            }
        }
    }

    return out;
}

enum class MMThrSpawnStatus : uint8_t { REJECT = 0, EMPTY = 1, NONEMPTY = 2 };

static bool step_mmthr_obl_bag(const MMThrOblBag& in,
                               uint32_t symbol_id,
                               MMThrOblBag& out,
                               bool* any_discharged,
                               const std::vector<ChildTables>& child_tab,
                               const MMThrLive& live,
                               bool finite_is_max,
                               const weight_t& threshold) {
    out.clear();
    if (any_discharged) *any_discharged = false;
    if (in.empty()) return true;

    for (const MMThrOblEntry& ent : in) {
        const uint32_t cid = ent.key.child;
        if (cid >= child_tab.size()) return false;

        const ChildTables& T = child_tab[cid];
        if (!T.child) return false;
        if (symbol_id >= T.alph) return false;

        MMThrFrontier next;
        mmthr_frontier_zero(next, mmthr_word_count(T.n_states));
        bool discharged = false;

        auto step_one_class = [&](const std::vector<uint64_t>& bits, uint8_t y) {
            mmthr_for_each_set_bit(bits, [&](uint32_t st) {
                if (discharged) return;
                if (st >= T.n_states) return;
                if (!live.is_live(cid, ent.key.guess, st, y)) return;

                const uint32_t cell = T.idx(st, symbol_id);
                const uint32_t b = T.off[static_cast<size_t>(cell)];
                const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                for (uint32_t p = b; p < e; ++p) {
                    const auto& tr = T.edges[static_cast<size_t>(p)];
                    if (tr.to >= T.n_states) continue;

                    const uint8_t y2 = mmthr_y_update(finite_is_max, y, tr.w, threshold);
                    if (T.is_final[tr.to] && y2 == ent.key.guess) {
                        discharged = true;
                        return;
                    }

                    if (!live.is_live(cid, ent.key.guess, tr.to, y2)) continue;
                    if (y2 == 0u) mmthr_bits_set(next.y0, tr.to);
                    else          mmthr_bits_set(next.y1, tr.to);
                }
            });
        };

        step_one_class(ent.key.fr.y0, 0u);
        if (!discharged) {
            step_one_class(ent.key.fr.y1, 1u);
        }

        if (discharged) {
            if (any_discharged) *any_discharged = true;
            continue;
        }

        mmthr_frontier_canonicalize(next, finite_is_max, ent.key.guess);
        if (mmthr_frontier_empty(next)) {
            return false;
        }

        mmthr_bag_add(out, MMThrOblEntry{MMThrOblKey{cid, ent.key.guess, std::move(next)}});
    }

    mmthr_bag_finalize(out);
    return true;
}

static MMThrSpawnStatus spawn_mmthr_obligation(uint32_t child_idx,
                                               uint32_t symbol_id,
                                               uint8_t guess,
                                               MMThrOblEntry& spawned,
                                               const std::vector<ChildTables>& child_tab,
                                               const MMThrLive& live,
                                               bool finite_is_max,
                                               const weight_t& threshold) {
    if (child_idx >= child_tab.size()) return MMThrSpawnStatus::REJECT;

    const ChildTables& T = child_tab[child_idx];
    if (!T.child) return MMThrSpawnStatus::REJECT;
    if (symbol_id >= T.alph) return MMThrSpawnStatus::REJECT;
    if (T.init >= T.n_states) return MMThrSpawnStatus::REJECT;

    const uint8_t init_y = finite_is_max ? 0u : 1u;

    MMThrFrontier fr;
    mmthr_frontier_zero(fr, mmthr_word_count(T.n_states));

    const uint32_t cell = T.idx(T.init, symbol_id);
    const uint32_t b = T.off[static_cast<size_t>(cell)];
    const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

    for (uint32_t p = b; p < e; ++p) {
        const auto& tr = T.edges[static_cast<size_t>(p)];
        if (tr.to >= T.n_states) continue;

        const uint8_t y2 = mmthr_y_update(finite_is_max, init_y, tr.w, threshold);
        if (T.is_final[tr.to] && y2 == guess) {
            return MMThrSpawnStatus::EMPTY;
        }

        if (!live.is_live(child_idx, guess, tr.to, y2)) continue;
        if (y2 == 0u) mmthr_bits_set(fr.y0, tr.to);
        else          mmthr_bits_set(fr.y1, tr.to);
    }

    mmthr_frontier_canonicalize(fr, finite_is_max, guess);
    if (mmthr_frontier_empty(fr)) {
        return MMThrSpawnStatus::REJECT;
    }

    spawned = MMThrOblEntry{MMThrOblKey{child_idx, guess, std::move(fr)}};
    return MMThrSpawnStatus::NONEMPTY;
}

struct BuchiState_mmthr {
    State* parent_state = nullptr;
    MMThrOblBag P1;
    MMThrOblBag P2;
    unsigned int parent_phase = 1u;  // 0 = active, 1 = waiting, 2 = final_pulse
    bool epoch_nonempty = false;

    bool operator==(const BuchiState_mmthr& o) const {
        return parent_state == o.parent_state
            && P1 == o.P1
            && P2 == o.P2
            && parent_phase == o.parent_phase
            && epoch_nonempty == o.epoch_nonempty;
    }

    bool operator<(const BuchiState_mmthr& o) const {
        if (parent_state != o.parent_state) return parent_state < o.parent_state;
        if (P1 != o.P1) return P1 < o.P1;
        if (P2 != o.P2) return P2 < o.P2;
        if (parent_phase != o.parent_phase) return parent_phase < o.parent_phase;
        return epoch_nonempty < o.epoch_nonempty;
    }
};

namespace {

static inline uint64_t mmsup_cached_mix64(uint64_t x) {
    x ^= (x >> 33);
    x *= 0xff51afd7ed558ccdULL;
    x ^= (x >> 33);
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= (x >> 33);
    return x;
}

static inline void mmsup_cached_hash_combine(uint64_t& h, uint64_t x) {
    h ^= mmsup_cached_mix64(x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

class TermCachedBuilder {
public:
    using OblId = uint32_t;
    using BagId = uint32_t;

    static constexpr OblId OBL_UNKNOWN    = 0xFFFFFFFFu;
    static constexpr OblId OBL_DEAD       = 0xFFFFFFFEu;
    static constexpr OblId OBL_DISCHARGED = 0xFFFFFFFDu;

    static constexpr BagId BAG_UNKNOWN = 0xFFFFFFFFu;
    static constexpr BagId BAG_DEAD    = 0xFFFFFFFEu;

    explicit TermCachedBuilder(NestedAutomaton* A_)
        : A(A_)
        , alph_size(static_cast<uint32_t>(A_->getAlphabetSize()))
        , k(static_cast<uint32_t>(A_->getChildrenSize()))
        , child_tab(k)
        , spawn(static_cast<size_t>(k) * static_cast<size_t>(alph_size), OBL_DEAD) {

        for (uint32_t i = 0; i < k; ++i) {
            ChildAutomaton* c = A->getChild(i);
            if (!c) continue;
            if (c->getStates()->size() < 2) continue;
            build_child_tables(c, child_tab[i]);
        }

        bags.push_back(Bag{});
        bags[0].step_next.assign(alph_size, 0u);
        bags[0].step_any_discharged.assign(alph_size, 0u);

        precompute_spawns();
    }

    inline bool child_enabled(uint32_t child) const {
        return child < k && child_tab[child].child != nullptr;
    }

    inline OblId spawn_code(uint32_t child, uint32_t sym) const {
        return spawn[static_cast<size_t>(child) * static_cast<size_t>(alph_size) +
                     static_cast<size_t>(sym)];
    }

    struct BagStep {
        bool ok = true;
        BagId next = 0u;
        bool any_discharged = false;
    };

    BagId bag_add_obl(BagId base, OblId add) {
        if (add == OBL_DEAD || add == OBL_DISCHARGED || add == OBL_UNKNOWN) return base;
        if (base >= bags.size()) return base;

        const uint64_t key = (static_cast<uint64_t>(base) << 32) | static_cast<uint64_t>(add);
        auto it = bag_add_cache.find(key);
        if (it != bag_add_cache.end()) return it->second;

        const std::vector<OblId>& v = bags[base].obls;
        if (std::binary_search(v.begin(), v.end(), add)) {
            bag_add_cache.emplace(key, base);
            return base;
        }

        std::vector<OblId> out;
        out.reserve(v.size() + 1u);
        bool inserted = false;
        for (OblId x : v) {
            if (!inserted && add < x) {
                out.push_back(add);
                inserted = true;
            }
            out.push_back(x);
        }
        if (!inserted) out.push_back(add);

        const BagId res = intern_bag(std::move(out));
        bag_add_cache.emplace(key, res);
        return res;
    }

    BagStep step_bag(BagId bid, uint32_t sym) {
        if (bid == 0u) return BagStep{true, 0u, false};
        if (bid >= bags.size()) return BagStep{false, 0u, false};

        Bag& B = bags[bid];
        const BagId cached = B.step_next[sym];
        if (cached != BAG_UNKNOWN) {
            if (cached == BAG_DEAD) return BagStep{false, 0u, false};
            return BagStep{true, cached, B.step_any_discharged[sym] != 0u};
        }

        std::vector<OblId> out;
        out.reserve(B.obls.size());
        bool any_d = false;

        for (OblId oid : B.obls) {
            const OblId r = step_obl(oid, sym);
            if (r == OBL_DEAD) {
                B.step_next[sym] = BAG_DEAD;
                B.step_any_discharged[sym] = 0u;
                return BagStep{false, 0u, false};
            }
            if (r == OBL_DISCHARGED) {
                any_d = true;
                continue;
            }
            out.push_back(r);
        }

        const BagId next = intern_bag(std::move(out));
        bags[bid].step_next[sym] = next;
        bags[bid].step_any_discharged[sym] = any_d ? 1u : 0u;
        return BagStep{true, next, any_d};
    }

    uint32_t alph_size = 0;
    uint32_t k = 0;

private:
    struct Obl {
        uint32_t child = 0;
        std::vector<uint64_t> bits;
        std::vector<OblId> step_cache;
    };

    struct Bag {
        std::vector<OblId> obls;
        std::vector<BagId> step_next;
        std::vector<uint8_t> step_any_discharged;
    };

    NestedAutomaton* A = nullptr;
    std::vector<ChildTables> child_tab;
    std::vector<OblId> spawn;

    std::vector<Obl> obls;
    std::unordered_map<uint64_t, std::vector<OblId>> obl_buckets;

    std::vector<Bag> bags;
    std::unordered_map<uint64_t, std::vector<BagId>> bag_buckets;
    std::unordered_map<uint64_t, BagId> bag_add_cache;

    inline size_t frontier_words(uint32_t child) const {
        return mmthr_word_count(child_tab[child].n_states);
    }

    inline OblId& spawn_code_ref(uint32_t child, uint32_t sym) {
        return spawn[static_cast<size_t>(child) * static_cast<size_t>(alph_size) +
                     static_cast<size_t>(sym)];
    }

    OblId intern_obl(uint32_t child, std::vector<uint64_t>&& bits) {
        if (child >= k || !child_enabled(child)) return OBL_DEAD;
        if (mmthr_bits_empty(bits)) return OBL_DEAD;

        uint64_t h = 0;
        mmsup_cached_hash_combine(h, child);
        mmsup_cached_hash_combine(h, static_cast<uint64_t>(bits.size()));
        for (uint64_t w : bits) mmsup_cached_hash_combine(h, w);

        auto& bucket = obl_buckets[h];
        for (OblId id : bucket) {
            const Obl& O = obls[id];
            if (O.child == child && O.bits == bits) {
                return id;
            }
        }

        const OblId id = static_cast<OblId>(obls.size());
        Obl O;
        O.child = child;
        O.bits = std::move(bits);
        O.step_cache.assign(alph_size, OBL_UNKNOWN);
        obls.push_back(std::move(O));
        bucket.push_back(id);
        return id;
    }

    BagId intern_bag(std::vector<OblId>&& ids) {
        if (ids.empty()) return 0u;

        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

        uint64_t h = 0;
        mmsup_cached_hash_combine(h, static_cast<uint64_t>(ids.size()));
        for (OblId id : ids) mmsup_cached_hash_combine(h, id);

        auto& bucket = bag_buckets[h];
        for (BagId bid : bucket) {
            if (bags[bid].obls == ids) return bid;
        }

        const BagId bid = static_cast<BagId>(bags.size());
        Bag B;
        B.obls = std::move(ids);
        B.step_next.assign(alph_size, BAG_UNKNOWN);
        B.step_any_discharged.assign(alph_size, 0u);
        bags.push_back(std::move(B));
        bucket.push_back(bid);
        return bid;
    }

    OblId spawn_term_obligation(uint32_t child, uint32_t sym) {
        if (!child_enabled(child)) return OBL_DEAD;

        const ChildTables& T = child_tab[child];
        if (sym >= T.alph || T.init >= T.n_states) return OBL_DEAD;

        std::vector<uint64_t> bits;
        mmthr_bits_zero(bits, frontier_words(child));

        const uint32_t cell = T.idx(T.init, sym);
        const uint32_t b = T.off[static_cast<size_t>(cell)];
        const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

        for (uint32_t p = b; p < e; ++p) {
            const auto& tr = T.edges[static_cast<size_t>(p)];
            if (tr.to >= T.n_states) continue;
            if (T.is_final[tr.to]) return OBL_DISCHARGED;
            if (!T.live[tr.to]) continue;
            mmthr_bits_set(bits, tr.to);
        }

        if (mmthr_bits_empty(bits)) return OBL_DEAD;
        return intern_obl(child, std::move(bits));
    }

    void precompute_spawns() {
        for (uint32_t child = 0; child < k; ++child) {
            if (!child_enabled(child)) continue;
            for (uint32_t sym = 0; sym < alph_size; ++sym) {
                spawn_code_ref(child, sym) = spawn_term_obligation(child, sym);
            }
        }
    }

    OblId step_obl(OblId id, uint32_t sym) {
        if (id >= obls.size()) return OBL_DEAD;

        Obl& O = obls[id];
        const OblId cached = O.step_cache[sym];
        if (cached != OBL_UNKNOWN) return cached;

        if (!child_enabled(O.child)) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        const ChildTables& T = child_tab[O.child];
        if (sym >= T.alph) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        std::vector<uint64_t> next;
        mmthr_bits_zero(next, frontier_words(O.child));
        bool discharged = false;

        mmthr_for_each_set_bit(O.bits, [&](uint32_t st) {
            if (discharged) return;
            if (st >= T.n_states) return;
            if (!T.live[st]) return;

            const uint32_t cell = T.idx(st, sym);
            const uint32_t b = T.off[static_cast<size_t>(cell)];
            const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

            for (uint32_t p = b; p < e; ++p) {
                const auto& tr = T.edges[static_cast<size_t>(p)];
                if (tr.to >= T.n_states) continue;
                if (T.is_final[tr.to]) {
                    discharged = true;
                    return;
                }
                if (!T.live[tr.to]) continue;
                mmthr_bits_set(next, tr.to);
            }
        });

        if (discharged) {
            O.step_cache[sym] = OBL_DISCHARGED;
            return OBL_DISCHARGED;
        }

        if (mmthr_bits_empty(next)) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        const OblId next_id = intern_obl(O.child, std::move(next));
        obls[id].step_cache[sym] = next_id;
        return next_id;
    }
};

class MMSupCachedBuilder {
public:
    using OblId = uint32_t;
    using BagId = uint32_t;

    static constexpr OblId OBL_UNKNOWN    = 0xFFFFFFFFu;
    static constexpr OblId OBL_DEAD       = 0xFFFFFFFEu;
    static constexpr OblId OBL_DISCHARGED = 0xFFFFFFFDu;

    static constexpr BagId BAG_UNKNOWN = 0xFFFFFFFFu;
    static constexpr BagId BAG_DEAD    = 0xFFFFFFFEu;

    explicit MMSupCachedBuilder(NestedAutomaton* A_,
                                bool finite_is_max_,
                                const weight_t& threshold_)
        : A(A_)
        , finite_is_max(finite_is_max_)
        , threshold(threshold_)
        , alph_size(static_cast<uint32_t>(A_->getAlphabetSize()))
        , k(static_cast<uint32_t>(A_->getChildrenSize()))
        , child_tab(k)
        , spawn(static_cast<size_t>(k) * 2u * static_cast<size_t>(alph_size), OBL_DEAD) {

        for (uint32_t i = 0; i < k; ++i) {
            ChildAutomaton* c = A->getChild(i);
            if (!c) continue;
            if (c->getStates()->size() < 2) continue;
            build_child_tables(c, child_tab[i]);
        }

        live = build_mmthr_live(child_tab, finite_is_max, threshold);

        bags.push_back(Bag{});
        bags[0].step_next.assign(alph_size, 0u);
        bags[0].step_any_discharged.assign(alph_size, 0u);

        precompute_spawns();
    }

    inline bool child_enabled(uint32_t child) const {
        return child < k && child_tab[child].child != nullptr;
    }

    inline OblId spawn_code(uint32_t child, uint8_t guess, uint32_t sym) const {
        return spawn[(static_cast<size_t>(child) * 2u + static_cast<size_t>(guess)) *
                         static_cast<size_t>(alph_size) +
                     static_cast<size_t>(sym)];
    }

    struct BagStep {
        bool ok = true;
        BagId next = 0u;
        bool any_discharged = false;
    };

    BagId bag_add_obl(BagId base, OblId add) {
        if (add == OBL_DEAD || add == OBL_DISCHARGED || add == OBL_UNKNOWN) return base;
        if (base >= bags.size()) return base;

        const uint64_t key = (static_cast<uint64_t>(base) << 32) | static_cast<uint64_t>(add);
        auto it = bag_add_cache.find(key);
        if (it != bag_add_cache.end()) return it->second;

        const std::vector<OblId>& v = bags[base].obls;
        if (std::binary_search(v.begin(), v.end(), add)) {
            bag_add_cache.emplace(key, base);
            return base;
        }

        std::vector<OblId> out;
        out.reserve(v.size() + 1u);
        bool inserted = false;
        for (OblId x : v) {
            if (!inserted && add < x) {
                out.push_back(add);
                inserted = true;
            }
            out.push_back(x);
        }
        if (!inserted) out.push_back(add);

        const BagId res = intern_bag(std::move(out));
        bag_add_cache.emplace(key, res);
        return res;
    }

    BagStep step_bag(BagId bid, uint32_t sym) {
        if (bid == 0u) return BagStep{true, 0u, false};
        if (bid >= bags.size()) return BagStep{false, 0u, false};

        Bag& B = bags[bid];
        const BagId cached = B.step_next[sym];
        if (cached != BAG_UNKNOWN) {
            if (cached == BAG_DEAD) return BagStep{false, 0u, false};
            return BagStep{true, cached, B.step_any_discharged[sym] != 0u};
        }

        std::vector<OblId> out;
        out.reserve(B.obls.size());
        bool any_d = false;

        for (OblId oid : B.obls) {
            const OblId r = step_obl(oid, sym);
            if (r == OBL_DEAD) {
                B.step_next[sym] = BAG_DEAD;
                B.step_any_discharged[sym] = 0u;
                return BagStep{false, 0u, false};
            }
            if (r == OBL_DISCHARGED) {
                any_d = true;
                continue;
            }
            out.push_back(r);
        }

        const BagId next = intern_bag(std::move(out));
        bags[bid].step_next[sym] = next;
        bags[bid].step_any_discharged[sym] = any_d ? 1u : 0u;
        return BagStep{true, next, any_d};
    }

    uint32_t alph_size = 0;
    uint32_t k = 0;

    struct OblStep {
        bool ok = true;
        bool discharged = false;
        OblId next = OBL_DEAD;
    };

    OblStep step_obl_public(OblId id, uint32_t sym) {
        if (id == OBL_DEAD || id == OBL_UNKNOWN) {
            return OblStep{false, false, OBL_DEAD};
        }
        if (id == OBL_DISCHARGED) {
            return OblStep{true, true, OBL_DISCHARGED};
        }

        const OblId r = step_obl(id, sym);
        if (r == OBL_DEAD) {
            return OblStep{false, false, OBL_DEAD};
        }
        if (r == OBL_DISCHARGED) {
            return OblStep{true, true, OBL_DISCHARGED};
        }
        return OblStep{true, false, r};
    }

private:
    struct Obl {
        uint32_t child = 0;
        uint8_t guess = 0;
        MMThrFrontier fr;
        std::vector<OblId> step_cache;
    };

    struct Bag {
        std::vector<OblId> obls;
        std::vector<BagId> step_next;
        std::vector<uint8_t> step_any_discharged;
    };

    NestedAutomaton* A = nullptr;
    bool finite_is_max = false;
    weight_t threshold = 0;

    std::vector<ChildTables> child_tab;
    MMThrLive live;

    std::vector<Obl> obls;
    std::unordered_map<uint64_t, std::vector<OblId>> obl_buckets;

    std::vector<Bag> bags;
    std::unordered_map<uint64_t, std::vector<BagId>> bag_buckets;
    std::unordered_map<uint64_t, BagId> bag_add_cache;

    std::vector<OblId> spawn;

    inline size_t frontier_words(uint32_t child) const {
        return mmthr_word_count(child_tab[child].n_states);
    }

    inline OblId& spawn_code_ref(uint32_t child, uint8_t guess, uint32_t sym) {
        return spawn[(static_cast<size_t>(child) * 2u + static_cast<size_t>(guess)) *
                         static_cast<size_t>(alph_size) +
                     static_cast<size_t>(sym)];
    }

    OblId intern_obl(uint32_t child, uint8_t guess, MMThrFrontier&& fr) {
        if (child >= k || !child_enabled(child)) return OBL_DEAD;

        mmthr_frontier_canonicalize(fr, finite_is_max, guess);
        if (mmthr_frontier_empty(fr)) return OBL_DEAD;

        uint64_t h = 0;
        mmsup_cached_hash_combine(h, child);
        mmsup_cached_hash_combine(h, guess);
        mmsup_cached_hash_combine(h, static_cast<uint64_t>(fr.y0.size()));
        for (uint64_t w : fr.y0) mmsup_cached_hash_combine(h, w);
        mmsup_cached_hash_combine(h, static_cast<uint64_t>(fr.y1.size()));
        for (uint64_t w : fr.y1) mmsup_cached_hash_combine(h, w);

        auto& bucket = obl_buckets[h];
        for (OblId id : bucket) {
            const Obl& O = obls[id];
            if (O.child == child && O.guess == guess && O.fr == fr) {
                return id;
            }
        }

        const OblId id = static_cast<OblId>(obls.size());
        Obl O;
        O.child = child;
        O.guess = guess;
        O.fr = std::move(fr);
        O.step_cache.assign(alph_size, OBL_UNKNOWN);
        obls.push_back(std::move(O));
        bucket.push_back(id);
        return id;
    }

    BagId intern_bag(std::vector<OblId>&& ids) {
        if (ids.empty()) return 0u;

        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

        uint64_t h = 0;
        mmsup_cached_hash_combine(h, static_cast<uint64_t>(ids.size()));
        for (OblId id : ids) mmsup_cached_hash_combine(h, id);

        auto& bucket = bag_buckets[h];
        for (BagId bid : bucket) {
            if (bags[bid].obls == ids) return bid;
        }

        const BagId bid = static_cast<BagId>(bags.size());
        Bag B;
        B.obls = std::move(ids);
        B.step_next.assign(alph_size, BAG_UNKNOWN);
        B.step_any_discharged.assign(alph_size, 0u);
        bags.push_back(std::move(B));
        bucket.push_back(bid);
        return bid;
    }

    void precompute_spawns() {
        for (uint32_t child = 0; child < k; ++child) {
            if (!child_enabled(child)) continue;

            for (uint8_t guess = 0u; guess <= 1u; ++guess) {
                for (uint32_t sym = 0; sym < alph_size; ++sym) {
                    MMThrOblEntry spawned;
                    const MMThrSpawnStatus st = spawn_mmthr_obligation(
                        child, sym, guess, spawned, child_tab, live, finite_is_max, threshold);

                    if (st == MMThrSpawnStatus::REJECT) {
                        spawn_code_ref(child, guess, sym) = OBL_DEAD;
                    } else if (st == MMThrSpawnStatus::EMPTY) {
                        spawn_code_ref(child, guess, sym) = OBL_DISCHARGED;
                    } else {
                        spawn_code_ref(child, guess, sym) =
                            intern_obl(child, guess, std::move(spawned.key.fr));
                    }
                }
            }
        }
    }

    OblId step_obl(OblId id, uint32_t sym) {
        if (id >= obls.size()) return OBL_DEAD;

        Obl& O = obls[id];
        const OblId cached = O.step_cache[sym];
        if (cached != OBL_UNKNOWN) return cached;

        if (!child_enabled(O.child)) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        const ChildTables& T = child_tab[O.child];
        if (sym >= T.alph) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        MMThrFrontier next;
        mmthr_frontier_zero(next, frontier_words(O.child));
        bool discharged = false;

        auto step_one_class = [&](const std::vector<uint64_t>& bits, uint8_t y) {
            mmthr_for_each_set_bit(bits, [&](uint32_t st) {
                if (discharged) return;
                if (st >= T.n_states) return;
                if (!live.is_live(O.child, O.guess, st, y)) return;

                const uint32_t cell = T.idx(st, sym);
                const uint32_t b = T.off[static_cast<size_t>(cell)];
                const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                for (uint32_t p = b; p < e; ++p) {
                    const auto& tr = T.edges[static_cast<size_t>(p)];
                    if (tr.to >= T.n_states) continue;

                    const uint8_t y2 = mmthr_y_update(finite_is_max, y, tr.w, threshold);
                    if (T.is_final[tr.to] && y2 == O.guess) {
                        discharged = true;
                        return;
                    }

                    if (!live.is_live(O.child, O.guess, tr.to, y2)) continue;
                    if (y2 == 0u) mmthr_bits_set(next.y0, tr.to);
                    else          mmthr_bits_set(next.y1, tr.to);
                }
            });
        };

        step_one_class(O.fr.y0, 0u);
        if (!discharged) step_one_class(O.fr.y1, 1u);

        if (discharged) {
            O.step_cache[sym] = OBL_DISCHARGED;
            return OBL_DISCHARGED;
        }

        mmthr_frontier_canonicalize(next, finite_is_max, O.guess);
        if (mmthr_frontier_empty(next)) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        const OblId next_id = intern_obl(O.child, O.guess, std::move(next));
        obls[id].step_cache[sym] = next_id;
        return next_id;
    }
};

static Automaton* flatten_MinMax_Sup_witness_cached_impl(NestedAutomaton* A,
                                                         value_function_t finite_aggregator,
                                                         weight_t threshold) {
    const bool finite_is_max = (finite_aggregator == Max_f);
    if (!finite_is_max && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Sup_witness_cached requires Max_f or Min_f");
    }

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    TermCachedBuilder term(A);
    MMSupCachedBuilder witness(A, finite_is_max, threshold);

    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(A->getAlphabetSize());
    for (size_t i = 0; i < A->getAlphabetSize(); ++i) {
        Symbol* original = A->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    MapStd<weight_t, Weight*> weight_register;
    MapArray<Weight*>* new_weights = new MapArray<Weight*>(3);
    auto get_weight = [&](const weight_t& value) -> Weight* {
        if (!weight_register.contains(value)) {
            Weight* w = new Weight(value);
            new_weights->insert(w->getId(), w);
            weight_register.insert(value, w);
        }
        return weight_register.at(value);
    };
    get_weight(weight_t(0));
    get_weight(weight_t(1));
    get_weight(weight_t(SILENT));

    const weight_t global_min = weight_t(0);
    const weight_t global_max = weight_t(1);
    static constexpr MMSupCachedBuilder::OblId NO_WITNESS = 0xFFFFFFFCu;

    struct Key {
        uint32_t parent = 0;
        TermCachedBuilder::BagId B1 = 0u;
        TermCachedBuilder::BagId B2 = 0u;
        MMSupCachedBuilder::OblId W1 = NO_WITNESS;
        MMSupCachedBuilder::OblId W2 = NO_WITNESS;
        acc_phase_t phase = ACC_WAIT_parent;
        bool epoch_nonempty = false;

        bool operator==(const Key& o) const {
            return parent == o.parent
                && B1 == o.B1
                && B2 == o.B2
                && W1 == o.W1
                && W2 == o.W2
                && phase == o.phase
                && epoch_nonempty == o.epoch_nonempty;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& key) const noexcept {
            uint64_t h = 0;
            mmsup_cached_hash_combine(h, key.parent);
            mmsup_cached_hash_combine(h, key.B1);
            mmsup_cached_hash_combine(h, key.B2);
            mmsup_cached_hash_combine(h, key.W1);
            mmsup_cached_hash_combine(h, key.W2);
            mmsup_cached_hash_combine(h, static_cast<uint8_t>(key.phase));
            mmsup_cached_hash_combine(h, static_cast<uint8_t>(key.epoch_nonempty ? 1u : 0u));
            return static_cast<size_t>(mmsup_cached_mix64(h));
        }
    };

    std::unordered_map<Key, State*, KeyHash> state_map;
    state_map.reserve(4096);
    std::deque<Key> worklist;
    unsigned int state_counter = 0;

    auto get_or_create = [&](const Key& key) -> State* {
        auto it = state_map.find(key);
        if (it != state_map.end()) return it->second;

        std::ostringstream name;
        name << "bmmsupwitcache_" << state_counter++;
        State* ns = new State(name.str(), new_alphabet->size(), global_min, global_max);
        state_map.emplace(key, ns);
        worklist.push_back(key);
        return ns;
    };

    Key init;
    init.parent = static_cast<uint32_t>(A->getInitial()->getId());

    State* init_state = get_or_create(init);

    struct WitStep {
        bool ok = true;
        MMSupCachedBuilder::OblId next = NO_WITNESS;
        bool discharged = false;
    };

    auto step_witness = [&](MMSupCachedBuilder::OblId w, uint32_t sym) -> WitStep {
        if (w == NO_WITNESS) {
            return WitStep{true, NO_WITNESS, false};
        }

        const auto r = witness.step_obl_public(w, sym);
        if (!r.ok) {
            return WitStep{false, NO_WITNESS, false};
        }
        if (r.discharged) {
            return WitStep{true, NO_WITNESS, true};
        }
        return WitStep{true, r.next, false};
    };

    while (!worklist.empty()) {
        const Key current = worklist.front();
        worklist.pop_front();

        State* current_state = state_map.find(current)->second;
        State* parent_state = A->getStates()->at(current.parent);
        const bool prev_empty_src =
            (current.B2 == 0u && current.W2 == NO_WITNESS);
        const acc_phase_t phase_after_current =
            (current.phase == ACC_WAIT_parent)
                ? (parent_state->getFinal() ? ACC_WAIT_P2EMPTY : ACC_WAIT_parent)
                : (prev_empty_src ? ACC_WAIT_parent : ACC_WAIT_P2EMPTY);
        const bool reset_epoch =
            (current.phase == ACC_WAIT_P2EMPTY && prev_empty_src);
        const bool front_nonempty_src =
            (current.B1 != 0u || current.W1 != NO_WITNESS);

        for (uint32_t symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            const auto B1res = term.step_bag(current.B1, symbol_id);
            if (!B1res.ok) continue;

            auto B2res = TermCachedBuilder::BagStep{true, 0u, false};
            if (current.B2 != 0u) {
                B2res = term.step_bag(current.B2, symbol_id);
                if (!B2res.ok) continue;
            }

            const auto W1res = step_witness(current.W1, symbol_id);
            if (!W1res.ok) continue;
            const auto W2res = step_witness(current.W2, symbol_id);
            if (!W2res.ok) continue;

            SetStd<Edge*>* succs = parent_state->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* parent_edge : *succs) {
                const uint32_t q_prime = static_cast<uint32_t>(parent_edge->getTo()->getId());
                const uint32_t child_index = static_cast<uint32_t>(
                    edgeWeightToChildIndex(parent_edge->getWeight()->getValue()));
                const bool is_silent =
                    (child_index >= term.k) || !term.child_enabled(child_index);

                bool epoch_nonempty_to =
                    reset_epoch ? front_nonempty_src : current.epoch_nonempty;
                if (B2res.any_discharged || W2res.discharged) epoch_nonempty_to = true;
                if (!is_silent) epoch_nonempty_to = true;

                TermCachedBuilder::BagId B1_base = 0u;
                TermCachedBuilder::BagId B2_base = 0u;
                MMSupCachedBuilder::OblId W1_base = NO_WITNESS;
                MMSupCachedBuilder::OblId W2_base = NO_WITNESS;

                if (prev_empty_src) {
                    B1_base = 0u;
                    B2_base = B1res.next;
                    W1_base = NO_WITNESS;
                    W2_base = W1res.next;
                } else {
                    B1_base = B1res.next;
                    B2_base = B2res.next;
                    W1_base = W1res.next;
                    W2_base = W2res.next;
                }

                assert(W1_base == NO_WITNESS || W2_base == NO_WITNESS);

                if (is_silent) {
                    const Key nxt{
                        q_prime, B1_base, B2_base, W1_base, W2_base,
                        phase_after_current, epoch_nonempty_to
                    };
                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(SILENT)),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                    continue;
                }

                const auto bg_spawn = term.spawn_code(child_index, symbol_id);
                if (bg_spawn != TermCachedBuilder::OBL_DEAD) {
                    auto B1_next = B1_base;
                    if (bg_spawn != TermCachedBuilder::OBL_DISCHARGED) {
                        B1_next = term.bag_add_obl(B1_base, bg_spawn);
                    }

                    const Key nxt{
                        q_prime, B1_next, B2_base, W1_base, W2_base,
                        phase_after_current, epoch_nonempty_to
                    };
                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(0)),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                }

                const bool post_has_witness =
                    (W1_base != NO_WITNESS || W2_base != NO_WITNESS);
                if (!post_has_witness) {
                    const auto wit_spawn = witness.spawn_code(child_index, 1u, symbol_id);
                    if (wit_spawn != MMSupCachedBuilder::OBL_DEAD) {
                        auto W1_next = W1_base;
                        if (wit_spawn != MMSupCachedBuilder::OBL_DISCHARGED) {
                            assert(W1_next == NO_WITNESS);
                            W1_next = wit_spawn;
                        }

                        const Key nxt{
                            q_prime, B1_base, B2_base, W1_next, W2_base,
                            phase_after_current, epoch_nonempty_to
                        };
                        State* to_state = get_or_create(nxt);
                        Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                            get_weight(weight_t(1)),
                                            current_state,
                                            to_state);
                        current_state->addSuccessor(ne);
                        to_state->addPredecessor(ne);
                    }
                }
            }
        }
    }

    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [key, st] : state_map) {
        new_states->insert(st->getId(), st);
        const bool prev_empty =
            (key.B2 == 0u && key.W2 == NO_WITNESS);
        if (key.phase == ACC_WAIT_P2EMPTY && prev_empty && key.epoch_nonempty) {
            st->setFinal(true);
        }
    }

    const std::string name = "BuchiMinMaxSupWitnessCached(" + A->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights,
                         global_min, global_max, init_state);
}

static Automaton* flatten_MinMax_Sup_cached_mmthr_impl(NestedAutomaton* A,
                                                       value_function_t finite_aggregator,
                                                       weight_t threshold) {
    const bool finite_is_max = (finite_aggregator == Max_f);
    if (!finite_is_max && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Sup_cached requires Max_f or Min_f");
    }

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    MMSupCachedBuilder builder(A, finite_is_max, threshold);

    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(A->getAlphabetSize());
    for (size_t i = 0; i < A->getAlphabetSize(); ++i) {
        Symbol* original = A->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    MapStd<weight_t, Weight*> weight_register;
    MapArray<Weight*>* new_weights = new MapArray<Weight*>(3);
    auto get_weight = [&](const weight_t& value) -> Weight* {
        if (!weight_register.contains(value)) {
            Weight* w = new Weight(value);
            new_weights->insert(w->getId(), w);
            weight_register.insert(value, w);
        }
        return weight_register.at(value);
    };
    get_weight(weight_t(0));
    get_weight(weight_t(1));
    get_weight(weight_t(SILENT));

    struct Key {
        uint32_t parent = 0;
        MMSupCachedBuilder::BagId P1 = 0u;
        MMSupCachedBuilder::BagId P2 = 0u;
        acc_phase_t phase = ACC_WAIT_parent;
        bool epoch_nonempty = false;

        bool operator==(const Key& o) const {
            return parent == o.parent
                && P1 == o.P1
                && P2 == o.P2
                && phase == o.phase
                && epoch_nonempty == o.epoch_nonempty;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& key) const noexcept {
            uint64_t h = 0;
            mmsup_cached_hash_combine(h, key.parent);
            mmsup_cached_hash_combine(h, key.P1);
            mmsup_cached_hash_combine(h, key.P2);
            mmsup_cached_hash_combine(h, static_cast<uint8_t>(key.phase));
            mmsup_cached_hash_combine(h, static_cast<uint8_t>(key.epoch_nonempty ? 1u : 0u));
            return static_cast<size_t>(mmsup_cached_mix64(h));
        }
    };

    std::unordered_map<Key, State*, KeyHash> state_map;
    state_map.reserve(4096);
    std::deque<Key> worklist;
    unsigned int state_counter = 0;

    const weight_t global_min = weight_t(0);
    const weight_t global_max = weight_t(1);

    Key init;
    init.parent = static_cast<uint32_t>(A->getInitial()->getId());

    std::ostringstream ss;
    ss << "bmmsupcache_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
    state_map.emplace(init, init_state);
    worklist.push_back(init);

    while (!worklist.empty()) {
        const Key current = worklist.front();
        worklist.pop_front();

        State* current_state = state_map.find(current)->second;
        State* parent_state = A->getStates()->at(current.parent);
        const acc_phase_t phase_after_current =
            (current.phase == ACC_WAIT_parent)
                ? (parent_state->getFinal() ? ACC_WAIT_P2EMPTY : ACC_WAIT_parent)
                : ((current.P2 == 0u) ? ACC_WAIT_parent : ACC_WAIT_P2EMPTY);
        const bool reset_epoch =
            (current.phase == ACC_WAIT_P2EMPTY && current.P2 == 0u);

        for (uint32_t symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            const auto P1res = builder.step_bag(current.P1, symbol_id);
            if (!P1res.ok) continue;

            bool tracked_discharged = false;
            MMSupCachedBuilder::BagId P2_step = 0u;
            if (current.P2 != 0u) {
                const auto P2res = builder.step_bag(current.P2, symbol_id);
                if (!P2res.ok) continue;
                P2_step = P2res.next;
                tracked_discharged = P2res.any_discharged;
            }

            SetStd<Edge*>* succs = parent_state->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* parent_edge : *succs) {
                const uint32_t q_prime = static_cast<uint32_t>(parent_edge->getTo()->getId());
                const uint32_t child_index = static_cast<uint32_t>(
                    edgeWeightToChildIndex(parent_edge->getWeight()->getValue()));
                const bool is_silent =
                    (child_index >= builder.k) || !builder.child_enabled(child_index);
                const bool boundary = (current.P2 == 0u);
                bool epoch_nonempty_to = reset_epoch ? (current.P1 != 0u) : current.epoch_nonempty;
                if (tracked_discharged) epoch_nonempty_to = true;
                if (!is_silent) epoch_nonempty_to = true;

                auto get_or_create = [&](const Key& key) -> State* {
                    auto it = state_map.find(key);
                    if (it != state_map.end()) return it->second;

                    std::ostringstream name;
                    name << "bmmsupcache_" << state_counter++;
                    State* ns = new State(name.str(), new_alphabet->size(), global_min, global_max);
                    state_map.emplace(key, ns);
                    worklist.push_back(key);
                    return ns;
                };

                if (is_silent) {
                    const auto P2_next = boundary ? P1res.next : P2_step;
                    const auto P1_next = boundary ? 0u : P1res.next;
                    const Key nxt{q_prime, P1_next, P2_next, phase_after_current, epoch_nonempty_to};
                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(SILENT)),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                    continue;
                }

                for (uint8_t guess = 0u; guess <= 1u; ++guess) {
                    const auto sc = builder.spawn_code(child_index, guess, symbol_id);
                    if (sc == MMSupCachedBuilder::OBL_DEAD) continue;

                    const auto P2_next = boundary ? P1res.next : P2_step;
                    const auto baseP1 = boundary ? 0u : P1res.next;
                    auto P1_next = baseP1;
                    if (sc != MMSupCachedBuilder::OBL_DISCHARGED) {
                        P1_next = builder.bag_add_obl(baseP1, sc);
                    }

                    const Key nxt{q_prime, P1_next, P2_next, phase_after_current, epoch_nonempty_to};
                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(static_cast<unsigned int>(guess))),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                }
            }
        }
    }

    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [key, st] : state_map) {
        new_states->insert(st->getId(), st);
        if (key.phase == ACC_WAIT_P2EMPTY && key.P2 == 0u && key.epoch_nonempty) {
            st->setFinal(true);
        }
    }

    const std::string name = "BuchiMinMaxSupCached(" + A->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights,
                         global_min, global_max, init_state);
}

} // namespace

Automaton* NestedAutomaton::flatten_MinMax_Sup_cached(value_function_t finite_aggregator,
                                                      weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Sup_cached requires Max_f or Min_f");
    }
    return flatten_MinMax_Sup_cached_mmthr_impl(this, finite_aggregator, threshold);
}

Automaton* NestedAutomaton::flatten_MinMax_Sup_witness_cached(value_function_t finite_aggregator,
                                                              weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Sup_witness_cached requires Max_f or Min_f");
    }
    return flatten_MinMax_Sup_witness_cached_impl(this, finite_aggregator, threshold);
}

namespace {

static inline void mm_cached_sort_unique(std::vector<uint32_t>& v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
}

static inline void mm_cached_canonicalize_sets(uint8_t guess,
                                               std::vector<uint32_t>& y0,
                                               std::vector<uint32_t>& y1) {
    mm_cached_sort_unique(y0);
    mm_cached_sort_unique(y1);

    if (y0.empty() || y1.empty()) return;

    if (guess == 1u) {
        std::vector<uint32_t> out;
        out.reserve(y0.size());
        size_t i = 0, j = 0;
        while (i < y0.size()) {
            if (j >= y1.size() || y0[i] < y1[j]) {
                out.push_back(y0[i++]);
            } else if (y0[i] == y1[j]) {
                ++i;
                ++j;
            } else {
                ++j;
            }
        }
        y0.swap(out);
    } else {
        std::vector<uint32_t> out;
        out.reserve(y1.size());
        size_t i = 0, j = 0;
        while (i < y1.size()) {
            if (j >= y0.size() || y1[i] < y0[j]) {
                out.push_back(y1[i++]);
            } else if (y1[i] == y0[j]) {
                ++i;
                ++j;
            } else {
                ++j;
            }
        }
        y1.swap(out);
    }
}

static inline uint64_t mm_mix64(uint64_t x) {
    x ^= (x >> 33);
    x *= 0xff51afd7ed558ccdULL;
    x ^= (x >> 33);
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= (x >> 33);
    return x;
}

static inline void mm_hash_combine(uint64_t& h, uint64_t x) {
    h ^= mm_mix64(x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

class MMInfCachedBuilder {
public:
    using OblId = uint32_t;
    using BagId = uint32_t;

    static constexpr OblId OBL_UNKNOWN    = 0xFFFFFFFFu;
    static constexpr OblId OBL_DEAD       = 0xFFFFFFFEu;
    static constexpr OblId OBL_DISCHARGED = 0xFFFFFFFDu;

    static constexpr BagId BAG_UNKNOWN = 0xFFFFFFFFu;
    static constexpr BagId BAG_DEAD    = 0xFFFFFFFEu;

    explicit MMInfCachedBuilder(NestedAutomaton* A_,
                                bool finite_is_max_,
                                const weight_t& threshold_)
        : A(A_)
        , finite_is_max(finite_is_max_)
        , threshold(threshold_)
        , alph_size(static_cast<uint32_t>(A_->getAlphabetSize()))
        , k(static_cast<uint32_t>(A_->getChildrenSize()))
        , child_tab(k)
        , child_info(k)
        , spawn(static_cast<size_t>(k) * 2u * static_cast<size_t>(alph_size), OBL_DEAD) {

        const value_function_t finVal = finite_is_max ? Max_f : Min_f;
        for (uint32_t i = 0; i < k; ++i) {
            ChildAutomaton* c = A->getChild(i);
            if (!thrext_child_uses_tracking(c)) continue;
            build_child_tables(c, child_tab[i]);
            thrext_build_child_info(child_tab[i], finVal, threshold, 1u, child_info[i]);
        }

        bags.push_back(Bag{});
        bags[0].step_next.assign(alph_size, 0u);
        bags[0].step_any_discharged.assign(alph_size, 0u);

        precompute_spawns();
        sync_stats_counts();
    }

    inline bool child_enabled(uint32_t child) const {
        return child < k && child_tab[child].child && child_info[child].enabled;
    }

    inline OblId spawn_code(uint32_t child, uint8_t guess, uint32_t sym) const {
        return spawn[(static_cast<size_t>(child) * 2u + static_cast<size_t>(guess)) *
                         static_cast<size_t>(alph_size) +
                     static_cast<size_t>(sym)];
    }

    struct BagStep {
        bool ok = true;
        BagId next = 0u;
        bool any_discharged = false;
    };

    BagId bag_add_obl(BagId base, OblId add) {
        if (add == OBL_DEAD || add == OBL_DISCHARGED || add == OBL_UNKNOWN) return base;
        if (base >= bags.size()) return base;

        if (mmexp_enabled()) g_minmax_inf_experiment.stats.bag_add_calls++;

        const uint64_t key = (static_cast<uint64_t>(base) << 32) | static_cast<uint64_t>(add);
        auto it = bag_add_cache.find(key);
        if (it != bag_add_cache.end()) {
            if (mmexp_enabled()) g_minmax_inf_experiment.stats.bag_add_cache_hits++;
            return it->second;
        }

        const std::vector<OblId>& v = bags[base].obls;
        if (std::binary_search(v.begin(), v.end(), add)) {
            bag_add_cache[key] = base;
            return base;
        }

        std::vector<OblId> out;
        out.reserve(v.size() + 1u);
        bool inserted = false;
        for (OblId x : v) {
            if (!inserted && add < x) {
                out.push_back(add);
                inserted = true;
            }
            out.push_back(x);
        }
        if (!inserted) out.push_back(add);

        const BagId res = intern_bag(std::move(out));
        bag_add_cache[key] = res;
        return res;
    }

    BagStep step_bag(BagId bid, uint32_t sym) {
        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_step_bag_ms : nullptr);

        if (mmexp_enabled()) g_minmax_inf_experiment.stats.step_bag_calls++;
        if (bid == 0u) return BagStep{true, 0u, false};
        if (bid >= bags.size()) return BagStep{false, 0u, false};

        Bag& B = bags[bid];
        if (mmexp_enabled()) {
            seen_bag_steps.insert(std::make_pair(bid, sym));
            g_minmax_inf_experiment.stats.unique_bag_step_keys = seen_bag_steps.size();
        }

        const BagId cached = B.step_next[sym];
        if (cached != BAG_UNKNOWN) {
            if (mmexp_enabled()) g_minmax_inf_experiment.stats.step_bag_cache_hits++;
            if (cached == BAG_DEAD) return BagStep{false, 0u, false};
            return BagStep{true, cached, B.step_any_discharged[sym] != 0u};
        }

        std::vector<OblId> out;
        out.reserve(B.obls.size());
        bool any_d = false;

        for (OblId oid : B.obls) {
            const OblId r = step_obl(oid, sym);
            if (r == OBL_DEAD) {
                B.step_next[sym] = BAG_DEAD;
                B.step_any_discharged[sym] = 0u;
                return BagStep{false, 0u, false};
            }
            if (r == OBL_DISCHARGED) {
                any_d = true;
                continue;
            }
            out.push_back(r);
        }

        const BagId nb = intern_bag(std::move(out));
        bags[bid].step_next[sym] = nb;
        bags[bid].step_any_discharged[sym] = any_d ? 1u : 0u;
        sync_stats_counts();
        return BagStep{true, nb, any_d};
    }

    void sync_stats_counts() const {
        if (!mmexp_enabled()) return;
        g_minmax_inf_experiment.stats.unique_obligation_count = obls.size();
        g_minmax_inf_experiment.stats.unique_bag_count = bags.size();
    }

    uint32_t alph_size = 0;
    uint32_t k = 0;

private:
    struct Obl {
        uint32_t child = 0;
        uint8_t guess = 0;
        std::vector<uint32_t> y0;
        std::vector<uint32_t> y1;
        std::vector<OblId> step_cache;
    };

    struct Bag {
        std::vector<OblId> obls;
        std::vector<BagId> step_next;
        std::vector<uint8_t> step_any_discharged;
    };

    NestedAutomaton* A = nullptr;
    bool finite_is_max = false;
    weight_t threshold = 0;

    std::vector<ChildTables> child_tab;
    std::vector<ThrExtChildInfo> child_info;

    std::vector<Obl> obls;
    std::unordered_map<uint64_t, std::vector<OblId>> obl_buckets;

    std::vector<Bag> bags;
    std::unordered_map<uint64_t, std::vector<BagId>> bag_buckets;
    std::unordered_map<uint64_t, BagId> bag_add_cache;

    std::vector<OblId> spawn;
    std::set<std::pair<BagId, uint32_t>> seen_bag_steps;

    inline uint8_t init_y() const { return finite_is_max ? 0u : 1u; }

    inline uint8_t step_y(uint8_t y, const weight_t& edge_w) const {
        const bool high = !(edge_w < threshold);
        if (finite_is_max) {
            return static_cast<uint8_t>((y != 0u || high) ? 1u : 0u);
        }
        return static_cast<uint8_t>((y != 0u && high) ? 1u : 0u);
    }

    inline bool is_live(uint32_t child, uint8_t guess, uint32_t st, uint8_t y) const {
        if (child >= k) return false;
        if (guess > 1u || y > 1u) return false;

        const ChildTables& T = child_tab[child];
        const ThrExtChildInfo& info = child_info[child];
        if (!T.child || !info.enabled) return false;
        if (st >= T.n_states) return false;
        if (!T.live.empty() && !T.live[st]) return false;

        const auto& live = info.mm_live[guess][y];
        if (live.empty()) return false;
        return live[st] != 0u;
    }

    OblId intern_obl(uint32_t child,
                     uint8_t guess,
                     std::vector<uint32_t>&& y0,
                     std::vector<uint32_t>&& y1) {
        mm_cached_canonicalize_sets(guess, y0, y1);
        if (y0.empty() && y1.empty()) return OBL_DEAD;

        uint64_t h = 0;
        mm_hash_combine(h, child);
        mm_hash_combine(h, guess);
        mm_hash_combine(h, static_cast<uint64_t>(y0.size()));
        for (uint32_t x : y0) mm_hash_combine(h, x);
        mm_hash_combine(h, static_cast<uint64_t>(y1.size()));
        for (uint32_t x : y1) mm_hash_combine(h, x);

        auto& bucket = obl_buckets[h];
        for (OblId id : bucket) {
            const Obl& O = obls[id];
            if (O.child == child && O.guess == guess && O.y0 == y0 && O.y1 == y1) {
                return id;
            }
        }

        const OblId new_id = static_cast<OblId>(obls.size());
        Obl O;
        O.child = child;
        O.guess = guess;
        O.y0 = std::move(y0);
        O.y1 = std::move(y1);
        O.step_cache.assign(alph_size, OBL_UNKNOWN);
        obls.push_back(std::move(O));
        bucket.push_back(new_id);
        sync_stats_counts();
        return new_id;
    }

    BagId intern_bag(std::vector<OblId>&& ids) {
        if (ids.empty()) return 0u;

        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

        uint64_t h = 0;
        mm_hash_combine(h, static_cast<uint64_t>(ids.size()));
        for (OblId id : ids) mm_hash_combine(h, id);

        auto& bucket = bag_buckets[h];
        for (BagId bid : bucket) {
            if (bags[bid].obls == ids) return bid;
        }

        const BagId new_id = static_cast<BagId>(bags.size());
        Bag B;
        B.obls = std::move(ids);
        B.step_next.assign(alph_size, BAG_UNKNOWN);
        B.step_any_discharged.assign(alph_size, 0u);
        bags.push_back(std::move(B));
        bucket.push_back(new_id);
        sync_stats_counts();
        return new_id;
    }

    void precompute_spawns() {
        for (uint32_t child = 0; child < k; ++child) {
            if (!child_enabled(child)) continue;

            const ChildTables& T = child_tab[child];
            if (T.init >= T.n_states) continue;

            for (uint8_t guess = 0; guess <= 1u; ++guess) {
                for (uint32_t sym = 0; sym < alph_size; ++sym) {
                    if (sym >= T.alph) {
                        spawn_code_ref(child, guess, sym) = OBL_DEAD;
                        continue;
                    }

                    std::vector<uint32_t> next0, next1;
                    const uint8_t y0 = init_y();

                    const uint32_t cell = T.idx(T.init, sym);
                    const uint32_t b = T.off[static_cast<size_t>(cell)];
                    const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                    for (uint32_t pos = b; pos < e; ++pos) {
                        const auto& tr = T.edges[static_cast<size_t>(pos)];
                        if (tr.to >= T.n_states) continue;

                        const uint8_t y2 = step_y(y0, tr.w);
                        if (T.is_final[tr.to] && y2 == guess) {
                            spawn_code_ref(child, guess, sym) = OBL_DISCHARGED;
                            goto spawn_done;
                        }

                        if (!is_live(child, guess, tr.to, y2)) continue;
                        (y2 ? next1 : next0).push_back(tr.to);
                    }

                    mm_cached_canonicalize_sets(guess, next0, next1);
                    spawn_code_ref(child, guess, sym) =
                        (next0.empty() && next1.empty())
                            ? OBL_DEAD
                            : intern_obl(child, guess, std::move(next0), std::move(next1));
                spawn_done:
                    ;
                }
            }
        }
    }

    OblId step_obl(OblId id, uint32_t sym) {
        if (mmexp_enabled()) g_minmax_inf_experiment.stats.step_obl_calls++;
        if (id >= obls.size()) return OBL_DEAD;

        Obl& O = obls[id];
        const OblId cached = O.step_cache[sym];
        if (cached != OBL_UNKNOWN) {
            if (mmexp_enabled()) g_minmax_inf_experiment.stats.step_obl_cache_hits++;
            return cached;
        }

        if (!child_enabled(O.child)) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        const ChildTables& T = child_tab[O.child];
        if (sym >= T.alph) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        if (mmexp_enabled()) {
            g_minmax_inf_experiment.stats.frontier_observations++;
            g_minmax_inf_experiment.stats.frontier_config_total += O.y0.size() + O.y1.size();
            g_minmax_inf_experiment.stats.frontier_capacity_total += T.n_states;
        }

        std::vector<uint32_t> next0, next1;
        bool discharged = false;

        auto process = [&](const std::vector<uint32_t>& src, uint8_t y) {
            for (uint32_t st : src) {
                if (!is_live(O.child, O.guess, st, y)) continue;

                const uint32_t cell = T.idx(st, sym);
                const uint32_t b = T.off[static_cast<size_t>(cell)];
                const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                for (uint32_t pos = b; pos < e; ++pos) {
                    const auto& tr = T.edges[static_cast<size_t>(pos)];
                    if (tr.to >= T.n_states) continue;

                    const uint8_t y2 = step_y(y, tr.w);
                    if (T.is_final[tr.to] && y2 == O.guess) {
                        discharged = true;
                        return;
                    }

                    if (!is_live(O.child, O.guess, tr.to, y2)) continue;
                    (y2 ? next1 : next0).push_back(tr.to);
                }
                if (discharged) return;
            }
        };

        process(O.y0, 0u);
        if (!discharged) process(O.y1, 1u);

        if (discharged) {
            O.step_cache[sym] = OBL_DISCHARGED;
            return OBL_DISCHARGED;
        }

        mm_cached_canonicalize_sets(O.guess, next0, next1);
        if (next0.empty() && next1.empty()) {
            O.step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        const OblId nid = intern_obl(O.child, O.guess, std::move(next0), std::move(next1));
        obls[id].step_cache[sym] = nid;
        return nid;
    }

    inline OblId& spawn_code_ref(uint32_t child, uint8_t guess, uint32_t sym) {
        return spawn[(static_cast<size_t>(child) * 2u + static_cast<size_t>(guess)) *
                         static_cast<size_t>(alph_size) +
                     static_cast<size_t>(sym)];
    }
};

static Automaton* flatten_MinMax_Inf_cached_impl(NestedAutomaton* A,
                                                 value_function_t finite_aggregator,
                                                 weight_t threshold) {
    const bool finite_is_max = (finite_aggregator == Max_f);
    if (!finite_is_max && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf requires Max_f or Min_f");
    }

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    MMInfCachedBuilder builder(A, finite_is_max, threshold);

    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(A->getAlphabetSize());
    for (size_t i = 0; i < A->getAlphabetSize(); ++i) {
        Symbol* original = A->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    MapStd<weight_t, Weight*> weight_register;
    MapArray<Weight*>* new_weights = new MapArray<Weight*>(3);
    auto get_weight = [&](const weight_t& value) -> Weight* {
        if (!weight_register.contains(value)) {
            Weight* w = new Weight(value);
            new_weights->insert(w->getId(), w);
            weight_register.insert(value, w);
        }
        return weight_register.at(value);
    };
    get_weight(weight_t(0));
    get_weight(weight_t(1));
    get_weight(weight_t(SILENT));

    struct Key {
        uint32_t parent = 0;
        MMInfCachedBuilder::BagId P1 = 0u;
        MMInfCachedBuilder::BagId P2 = 0u;
        acc_phase_t phase = ACC_WAIT_parent;
        bool epoch_nonempty = false;

        bool operator==(const Key& o) const {
            return parent == o.parent
                && P1 == o.P1
                && P2 == o.P2
                && phase == o.phase
                && epoch_nonempty == o.epoch_nonempty;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& key) const noexcept {
            uint64_t h = 0;
            mm_hash_combine(h, key.parent);
            mm_hash_combine(h, key.P1);
            mm_hash_combine(h, key.P2);
            mm_hash_combine(h, static_cast<uint8_t>(key.phase));
            mm_hash_combine(h, static_cast<uint8_t>(key.epoch_nonempty ? 1u : 0u));
            return static_cast<size_t>(mm_mix64(h));
        }
    };

    std::unordered_map<Key, State*, KeyHash> state_map;
    state_map.reserve(4096);
    std::deque<Key> worklist;
    unsigned int state_counter = 0;

    const weight_t global_min = weight_t(0);
    const weight_t global_max = weight_t(1);

    Key init;
    init.parent = static_cast<uint32_t>(A->getInitial()->getId());

    std::ostringstream ss;
    ss << "bcache_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
    state_map.emplace(init, init_state);
    worklist.push_back(init);

    while (!worklist.empty()) {
        const Key current = worklist.front();
        worklist.pop_front();

        State* current_state = nullptr;
        {
            ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
            if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
            current_state = state_map.find(current)->second;
        }
        State* parent_state = A->getStates()->at(current.parent);
        const acc_phase_t phase_after_current =
            (current.phase == ACC_WAIT_parent)
                ? (parent_state->getFinal() ? ACC_WAIT_P2EMPTY : ACC_WAIT_parent)
                : ((current.P2 == 0u) ? ACC_WAIT_parent : ACC_WAIT_P2EMPTY);
        const bool reset_epoch =
            (current.phase == ACC_WAIT_P2EMPTY && current.P2 == 0u);

        for (uint32_t symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            const auto P1res = builder.step_bag(current.P1, symbol_id);
            if (!P1res.ok) continue;

            MMInfCachedBuilder::BagId P2_step = 0u;
            bool tracked_discharged = false;
            if (current.P2 != 0u) {
                const auto P2res = builder.step_bag(current.P2, symbol_id);
                if (!P2res.ok) continue;
                P2_step = P2res.next;
                tracked_discharged = P2res.any_discharged;
            }

            SetStd<Edge*>* succs = parent_state->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* parent_edge : *succs) {
                const uint32_t q_prime = static_cast<uint32_t>(parent_edge->getTo()->getId());
                const uint32_t child_index = static_cast<uint32_t>(
                    edgeWeightToChildIndex(parent_edge->getWeight()->getValue()));
                const bool is_silent = (child_index >= builder.k) || !builder.child_enabled(child_index);
                const bool boundary = (current.P2 == 0u);
                bool epoch_nonempty_to = reset_epoch ? (current.P1 != 0u) : current.epoch_nonempty;
                if (tracked_discharged) epoch_nonempty_to = true;
                if (!is_silent) epoch_nonempty_to = true;

                auto get_or_create = [&](const Key& key) -> State* {
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
                        auto it = state_map.find(key);
                        if (it != state_map.end()) return it->second;
                    }

                    std::ostringstream s2;
                    s2 << "bcache_" << state_counter++;
                    State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_insert_calls++;
                        state_map.emplace(key, ns);
                    }
                    worklist.push_back(key);
                    return ns;
                };

                if (is_silent) {
                    const auto P2_next = boundary ? P1res.next : P2_step;
                    const auto P1_next = boundary ? 0u : P1res.next;
                    const Key nxt{q_prime, P1_next, P2_next, phase_after_current, epoch_nonempty_to};

                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(SILENT)),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                    continue;
                }

                for (uint8_t guess = 0u; guess <= 1u; ++guess) {
                    mmexp_record_thr_spawn(child_index, guess, symbol_id);
                    const auto sc = builder.spawn_code(child_index, guess, symbol_id);
                    if (sc == MMInfCachedBuilder::OBL_DEAD) continue;

                    const auto P2_next = boundary ? P1res.next : P2_step;
                    const auto baseP1 = boundary ? 0u : P1res.next;
                    auto P1_next = baseP1;
                    if (sc != MMInfCachedBuilder::OBL_DISCHARGED) {
                        P1_next = builder.bag_add_obl(baseP1, sc);
                    }

                    const Key nxt{q_prime, P1_next, P2_next, phase_after_current, epoch_nonempty_to};

                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(static_cast<unsigned int>(guess))),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                }
            }
        }
    }

    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [key, st] : state_map) {
        new_states->insert(st->getId(), st);
        if (key.phase == ACC_WAIT_P2EMPTY && key.P2 == 0u && key.epoch_nonempty) {
            st->setFinal(true);
        }
    }

    builder.sync_stats_counts();
    const std::string name = "BuchiMMCached(" + A->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights,
                         global_min, global_max, init_state);
}

} // namespace

Automaton* NestedAutomaton::flatten_MinMax_Inf_cached(value_function_t finite_aggregator,
                                                      weight_t threshold) {
    if (finite_aggregator != Max_f && finite_aggregator != Min_f) {
        QUAK_FAIL("flatten_MinMax_Inf_cached requires Max_f or Min_f");
    }
    return flatten_MinMax_Inf_cached_impl(this, finite_aggregator, threshold);
}

namespace {

class SumInfCachedBuilder {
public:
    using OblId = uint32_t;
    using BagId = uint32_t;

    static constexpr OblId OBL_UNKNOWN    = 0xFFFFFFFFu;
    static constexpr OblId OBL_DEAD       = 0xFFFFFFFEu;
    static constexpr OblId OBL_DISCHARGED = 0xFFFFFFFDu;

    static constexpr BagId BAG_UNKNOWN = 0xFFFFFFFFu;
    static constexpr BagId BAG_DEAD    = 0xFFFFFFFEu;

    explicit SumInfCachedBuilder(NestedAutomaton* A_,
                                 value_function_t finite_aggregator_,
                                 const weight_t& threshold_)
        : A(A_)
        , finite_aggregator(finite_aggregator_)
        , threshold(threshold_)
        , alph_size(static_cast<uint32_t>(A_->getAlphabetSize()))
        , k(static_cast<uint32_t>(A_->getChildrenSize()))
        , child_tab(k)
        , child_info(k)
        , spawn(static_cast<size_t>(k) * 2u * static_cast<size_t>(alph_size), OBL_DEAD) {

        if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
            QUAK_FAIL("SumInfCachedBuilder requires SumPlus or SumMinus");
        }

        const thrext_int_t weight_scale = compute_weight_scale(A);
        for (uint32_t i = 0; i < k; ++i) {
            ChildAutomaton* c = A->getChild(i);
            if (!thrext_child_uses_tracking(c)) continue;
            build_child_tables(c, child_tab[i]);
            thrext_build_child_info(child_tab[i], finite_aggregator, threshold, weight_scale, child_info[i]);
        }

        bags.push_back(Bag{});
        bags[0].step_next.assign(alph_size, 0u);
        bags[0].step_any_discharged.assign(alph_size, 0u);

        precompute_spawns();
        sync_stats_counts();
    }

    inline bool child_enabled(uint32_t child) const {
        return child < k && child_tab[child].child && child_info[child].enabled;
    }

    inline OblId spawn_code(uint32_t child, uint8_t guess, uint32_t sym) const {
        return spawn[(static_cast<size_t>(child) * 2u + static_cast<size_t>(guess)) *
                         static_cast<size_t>(alph_size) +
                     static_cast<size_t>(sym)];
    }

    struct BagStep {
        bool ok = true;
        BagId next = 0u;
        bool any_discharged = false;
    };

    BagId bag_add_obl(BagId base, OblId add) {
        if (add == OBL_DEAD || add == OBL_DISCHARGED || add == OBL_UNKNOWN) return base;
        if (base >= bags.size()) return base;

        if (mmexp_enabled()) g_minmax_inf_experiment.stats.bag_add_calls++;

        const uint64_t key = (static_cast<uint64_t>(base) << 32) | static_cast<uint64_t>(add);
        auto it = bag_add_cache.find(key);
        if (it != bag_add_cache.end()) {
            if (mmexp_enabled()) g_minmax_inf_experiment.stats.bag_add_cache_hits++;
            return it->second;
        }

        const std::vector<OblId>& v = bags[base].obls;
        if (std::binary_search(v.begin(), v.end(), add)) {
            bag_add_cache[key] = base;
            return base;
        }

        std::vector<OblId> out;
        out.reserve(v.size() + 1u);
        bool inserted = false;
        for (OblId x : v) {
            if (!inserted && add < x) {
                out.push_back(add);
                inserted = true;
            }
            out.push_back(x);
        }
        if (!inserted) out.push_back(add);

        const BagId res = intern_bag(std::move(out));
        bag_add_cache[key] = res;
        return res;
    }

    BagStep step_bag(BagId bid, uint32_t sym) {
        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_step_bag_ms : nullptr);

        if (mmexp_enabled()) g_minmax_inf_experiment.stats.step_bag_calls++;
        if (bid == 0u) return BagStep{true, 0u, false};
        if (bid >= bags.size()) return BagStep{false, 0u, false};

        if (mmexp_enabled()) {
            seen_bag_steps.insert(std::make_pair(bid, sym));
            g_minmax_inf_experiment.stats.unique_bag_step_keys = seen_bag_steps.size();
        }

        const BagId cached = bags[bid].step_next[sym];
        if (cached != BAG_UNKNOWN) {
            if (mmexp_enabled()) g_minmax_inf_experiment.stats.step_bag_cache_hits++;
            if (cached == BAG_DEAD) return BagStep{false, 0u, false};
            return BagStep{true, cached, bags[bid].step_any_discharged[sym] != 0u};
        }

        std::vector<OblId> out;
        out.reserve(bags[bid].obls.size());
        bool any_d = false;

        for (OblId oid : bags[bid].obls) {
            const OblId r = step_obl(oid, sym);
            if (r == OBL_DEAD) {
                bags[bid].step_next[sym] = BAG_DEAD;
                bags[bid].step_any_discharged[sym] = 0u;
                return BagStep{false, 0u, false};
            }
            if (r == OBL_DISCHARGED) {
                any_d = true;
                continue;
            }
            out.push_back(r);
        }

        const BagId nb = intern_bag(std::move(out));
        bags[bid].step_next[sym] = nb;
        bags[bid].step_any_discharged[sym] = any_d ? 1u : 0u;
        sync_stats_counts();
        return BagStep{true, nb, any_d};
    }

    void sync_stats_counts() const {
        if (!mmexp_enabled()) return;
        g_minmax_inf_experiment.stats.unique_obligation_count = obls.size();
        g_minmax_inf_experiment.stats.unique_bag_count = bags.size();
    }

    uint32_t alph_size = 0;
    uint32_t k = 0;

    struct OblStep {
        bool ok = true;
        bool discharged = false;
        OblId next = OBL_DEAD;
    };

    OblStep step_obl_public(OblId id, uint32_t sym) {
        if (id == OBL_DEAD || id == OBL_UNKNOWN) {
            return OblStep{false, false, OBL_DEAD};
        }
        if (id == OBL_DISCHARGED) {
            return OblStep{true, true, OBL_DISCHARGED};
        }

        const OblId r = step_obl(id, sym);
        if (r == OBL_DEAD) {
            return OblStep{false, false, OBL_DEAD};
        }
        if (r == OBL_DISCHARGED) {
            return OblStep{true, true, OBL_DISCHARGED};
        }
        return OblStep{true, false, r};
    }

private:
    struct Obl {
        uint32_t child = 0;
        uint8_t guess = 0;
        ThrExtFrontier conf;
        std::vector<OblId> step_cache;
    };

    struct Bag {
        std::vector<OblId> obls;
        std::vector<BagId> step_next;
        std::vector<uint8_t> step_any_discharged;
    };

    NestedAutomaton* A = nullptr;
    value_function_t finite_aggregator = SumPlus;
    weight_t threshold = 0;

    std::vector<ChildTables> child_tab;
    std::vector<ThrExtChildInfo> child_info;

    std::vector<Obl> obls;
    std::unordered_map<uint64_t, std::vector<OblId>> obl_buckets;

    std::vector<Bag> bags;
    std::unordered_map<uint64_t, std::vector<BagId>> bag_buckets;
    std::unordered_map<uint64_t, BagId> bag_add_cache;

    std::vector<OblId> spawn;
    std::set<std::pair<BagId, uint32_t>> seen_bag_steps;

    OblId intern_obl(uint32_t child, uint8_t guess, ThrExtFrontier&& conf) {
        if (child >= k) return OBL_DEAD;
        if (!child_enabled(child)) return OBL_DEAD;

        thrext_frontier_canonicalize(conf, child_info[child], guess);
        if (conf.empty()) return OBL_DEAD;

        uint64_t h = 0;
        mm_hash_combine(h, child);
        mm_hash_combine(h, guess);
        mm_hash_combine(h, static_cast<uint64_t>(conf.size()));
        for (const ThrExtConf& c : conf) {
            mm_hash_combine(h, c.st);
            mm_hash_combine(h, c.prog);
        }

        auto& bucket = obl_buckets[h];
        for (OblId id : bucket) {
            const Obl& O = obls[id];
            if (O.child == child && O.guess == guess && O.conf == conf) {
                return id;
            }
        }

        const OblId new_id = static_cast<OblId>(obls.size());
        Obl O;
        O.child = child;
        O.guess = guess;
        O.conf = std::move(conf);
        O.step_cache.assign(child_tab[child].alph, OBL_UNKNOWN);
        obls.push_back(std::move(O));
        bucket.push_back(new_id);
        sync_stats_counts();
        return new_id;
    }

    BagId intern_bag(std::vector<OblId>&& ids) {
        if (ids.empty()) return 0u;

        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

        uint64_t h = 0;
        mm_hash_combine(h, static_cast<uint64_t>(ids.size()));
        for (OblId id : ids) mm_hash_combine(h, id);

        auto& bucket = bag_buckets[h];
        for (BagId bid : bucket) {
            if (bags[bid].obls == ids) return bid;
        }

        const BagId new_id = static_cast<BagId>(bags.size());
        Bag B;
        B.obls = std::move(ids);
        B.step_next.assign(alph_size, BAG_UNKNOWN);
        B.step_any_discharged.assign(alph_size, 0u);
        bags.push_back(std::move(B));
        bucket.push_back(new_id);
        sync_stats_counts();
        return new_id;
    }

    void precompute_spawns() {
        for (uint32_t child = 0; child < k; ++child) {
            if (!child_enabled(child)) continue;

            for (uint8_t guess = 0; guess <= 1u; ++guess) {
                for (uint32_t sym = 0; sym < alph_size; ++sym) {
                    ThrExtFrontier conf;
                    const ThrExtSpawnStatus st = thrext_spawn_frontier(child,
                                                                       sym,
                                                                       guess,
                                                                       conf,
                                                                       child_tab,
                                                                       child_info);
                    if (st == ThrExtSpawnStatus::REJECT) {
                        spawn_code_ref(child, guess, sym) = OBL_DEAD;
                    } else if (st == ThrExtSpawnStatus::EMPTY) {
                        spawn_code_ref(child, guess, sym) = OBL_DISCHARGED;
                    } else {
                        spawn_code_ref(child, guess, sym) =
                            intern_obl(child, guess, std::move(conf));
                    }
                }
            }
        }
    }

    OblId step_obl(OblId id, uint32_t sym) {
        if (mmexp_enabled()) g_minmax_inf_experiment.stats.step_obl_calls++;
        if (id >= obls.size()) return OBL_DEAD;

        const uint32_t child = obls[id].child;
        if (!child_enabled(child)) return OBL_DEAD;
        if (sym >= obls[id].step_cache.size()) return OBL_DEAD;

        const OblId cached = obls[id].step_cache[sym];
        if (cached != OBL_UNKNOWN) {
            if (mmexp_enabled()) g_minmax_inf_experiment.stats.step_obl_cache_hits++;
            return cached;
        }

        if (mmexp_enabled()) {
            g_minmax_inf_experiment.stats.frontier_observations++;
            g_minmax_inf_experiment.stats.frontier_config_total += obls[id].conf.size();
            g_minmax_inf_experiment.stats.frontier_capacity_total += child_tab[child].n_states;
        }

        ThrExtFrontier next_conf;
        const ThrExtStepStatus st = thrext_step_frontier(child,
                                                         obls[id].guess,
                                                         obls[id].conf,
                                                         sym,
                                                         next_conf,
                                                         child_tab,
                                                         child_info);
        if (st == ThrExtStepStatus::DEAD) {
            obls[id].step_cache[sym] = OBL_DEAD;
            return OBL_DEAD;
        }

        if (st == ThrExtStepStatus::DISCHARGED) {
            obls[id].step_cache[sym] = OBL_DISCHARGED;
            return OBL_DISCHARGED;
        }

        const OblId nid = intern_obl(child, obls[id].guess, std::move(next_conf));
        obls[id].step_cache[sym] = nid;
        return nid;
    }

    inline OblId& spawn_code_ref(uint32_t child, uint8_t guess, uint32_t sym) {
        return spawn[(static_cast<size_t>(child) * 2u + static_cast<size_t>(guess)) *
                         static_cast<size_t>(alph_size) +
                     static_cast<size_t>(sym)];
    }
};

static Automaton* flatten_SumPlusMinus_Sup_witness_cached_impl(NestedAutomaton* A,
                                                               value_function_t finite_aggregator,
                                                               weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Sup_witness_cached requires SumPlus or SumMinus");
    }

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    TermCachedBuilder term(A);
    SumInfCachedBuilder witness(A, finite_aggregator, threshold);

    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(A->getAlphabetSize());
    for (size_t i = 0; i < A->getAlphabetSize(); ++i) {
        Symbol* original = A->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    MapStd<weight_t, Weight*> weight_register;
    MapArray<Weight*>* new_weights = new MapArray<Weight*>(3);
    auto get_weight = [&](const weight_t& value) -> Weight* {
        if (!weight_register.contains(value)) {
            Weight* w = new Weight(value);
            new_weights->insert(w->getId(), w);
            weight_register.insert(value, w);
        }
        return weight_register.at(value);
    };
    get_weight(weight_t(0));
    get_weight(weight_t(1));
    get_weight(weight_t(SILENT));

    const weight_t global_min = weight_t(0);
    const weight_t global_max = weight_t(1);
    static constexpr SumInfCachedBuilder::OblId NO_WITNESS = 0xFFFFFFFCu;

    struct Key {
        uint32_t parent = 0;
        TermCachedBuilder::BagId B1 = 0u;
        TermCachedBuilder::BagId B2 = 0u;
        SumInfCachedBuilder::OblId W1 = NO_WITNESS;
        SumInfCachedBuilder::OblId W2 = NO_WITNESS;
        acc_phase_t phase = ACC_WAIT_parent;
        bool epoch_nonempty = false;

        bool operator==(const Key& o) const {
            return parent == o.parent
                && B1 == o.B1
                && B2 == o.B2
                && W1 == o.W1
                && W2 == o.W2
                && phase == o.phase
                && epoch_nonempty == o.epoch_nonempty;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& key) const noexcept {
            uint64_t h = 0;
            mm_hash_combine(h, key.parent);
            mm_hash_combine(h, key.B1);
            mm_hash_combine(h, key.B2);
            mm_hash_combine(h, key.W1);
            mm_hash_combine(h, key.W2);
            mm_hash_combine(h, static_cast<uint8_t>(key.phase));
            mm_hash_combine(h, static_cast<uint8_t>(key.epoch_nonempty ? 1u : 0u));
            return static_cast<size_t>(mm_mix64(h));
        }
    };

    std::unordered_map<Key, State*, KeyHash> state_map;
    state_map.reserve(4096);
    std::deque<Key> worklist;
    unsigned int state_counter = 0;

    auto get_or_create = [&](const Key& key) -> State* {
        auto it = state_map.find(key);
        if (it != state_map.end()) return it->second;

        std::ostringstream name;
        name << "bsumsupwitcache_" << state_counter++;
        State* ns = new State(name.str(), new_alphabet->size(), global_min, global_max);
        state_map.emplace(key, ns);
        worklist.push_back(key);
        return ns;
    };

    Key init;
    init.parent = static_cast<uint32_t>(A->getInitial()->getId());
    State* init_state = get_or_create(init);

    struct WitStep {
        bool ok = true;
        SumInfCachedBuilder::OblId next = NO_WITNESS;
        bool discharged = false;
    };

    auto step_witness = [&](SumInfCachedBuilder::OblId w, uint32_t sym) -> WitStep {
        if (w == NO_WITNESS) {
            return WitStep{true, NO_WITNESS, false};
        }

        const auto r = witness.step_obl_public(w, sym);
        if (!r.ok) {
            return WitStep{false, NO_WITNESS, false};
        }
        if (r.discharged) {
            return WitStep{true, NO_WITNESS, true};
        }
        return WitStep{true, r.next, false};
    };

    while (!worklist.empty()) {
        const Key current = worklist.front();
        worklist.pop_front();

        State* current_state = state_map.find(current)->second;
        State* parent_state = A->getStates()->at(current.parent);
        const bool prev_empty_src =
            (current.B2 == 0u && current.W2 == NO_WITNESS);
        const acc_phase_t phase_after_current =
            (current.phase == ACC_WAIT_parent)
                ? (parent_state->getFinal() ? ACC_WAIT_P2EMPTY : ACC_WAIT_parent)
                : (prev_empty_src ? ACC_WAIT_parent : ACC_WAIT_P2EMPTY);
        const bool reset_epoch =
            (current.phase == ACC_WAIT_P2EMPTY && prev_empty_src);
        const bool front_nonempty_src =
            (current.B1 != 0u || current.W1 != NO_WITNESS);

        for (uint32_t symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            const auto B1res = term.step_bag(current.B1, symbol_id);
            if (!B1res.ok) continue;

            auto B2res = TermCachedBuilder::BagStep{true, 0u, false};
            if (current.B2 != 0u) {
                B2res = term.step_bag(current.B2, symbol_id);
                if (!B2res.ok) continue;
            }

            const auto W1res = step_witness(current.W1, symbol_id);
            if (!W1res.ok) continue;
            const auto W2res = step_witness(current.W2, symbol_id);
            if (!W2res.ok) continue;

            SetStd<Edge*>* succs = parent_state->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* parent_edge : *succs) {
                const uint32_t q_prime = static_cast<uint32_t>(parent_edge->getTo()->getId());
                const uint32_t child_index = static_cast<uint32_t>(
                    edgeWeightToChildIndex(parent_edge->getWeight()->getValue()));
                const bool is_silent =
                    (child_index >= term.k) || !term.child_enabled(child_index);

                bool epoch_nonempty_to =
                    reset_epoch ? front_nonempty_src : current.epoch_nonempty;
                if (B2res.any_discharged || W2res.discharged) epoch_nonempty_to = true;
                if (!is_silent) epoch_nonempty_to = true;

                TermCachedBuilder::BagId B1_base = 0u;
                TermCachedBuilder::BagId B2_base = 0u;
                SumInfCachedBuilder::OblId W1_base = NO_WITNESS;
                SumInfCachedBuilder::OblId W2_base = NO_WITNESS;

                if (prev_empty_src) {
                    B1_base = 0u;
                    B2_base = B1res.next;
                    W1_base = NO_WITNESS;
                    W2_base = W1res.next;
                } else {
                    B1_base = B1res.next;
                    B2_base = B2res.next;
                    W1_base = W1res.next;
                    W2_base = W2res.next;
                }

                assert(W1_base == NO_WITNESS || W2_base == NO_WITNESS);

                if (is_silent) {
                    const Key nxt{
                        q_prime, B1_base, B2_base, W1_base, W2_base,
                        phase_after_current, epoch_nonempty_to
                    };
                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(SILENT)),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                    continue;
                }

                const auto bg_spawn = term.spawn_code(child_index, symbol_id);
                if (bg_spawn != TermCachedBuilder::OBL_DEAD) {
                    auto B1_next = B1_base;
                    if (bg_spawn != TermCachedBuilder::OBL_DISCHARGED) {
                        B1_next = term.bag_add_obl(B1_base, bg_spawn);
                    }

                    const Key nxt{
                        q_prime, B1_next, B2_base, W1_base, W2_base,
                        phase_after_current, epoch_nonempty_to
                    };
                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(0)),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                }

                const bool post_has_witness =
                    (W1_base != NO_WITNESS || W2_base != NO_WITNESS);
                if (!post_has_witness) {
                    const auto wit_spawn = witness.spawn_code(child_index, 1u, symbol_id);
                    if (wit_spawn != SumInfCachedBuilder::OBL_DEAD) {
                        auto W1_next = W1_base;
                        if (wit_spawn != SumInfCachedBuilder::OBL_DISCHARGED) {
                            assert(W1_next == NO_WITNESS);
                            W1_next = wit_spawn;
                        }

                        const Key nxt{
                            q_prime, B1_base, B2_base, W1_next, W2_base,
                            phase_after_current, epoch_nonempty_to
                        };
                        State* to_state = get_or_create(nxt);
                        Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                            get_weight(weight_t(1)),
                                            current_state,
                                            to_state);
                        current_state->addSuccessor(ne);
                        to_state->addPredecessor(ne);
                    }
                }
            }
        }
    }

    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [key, st] : state_map) {
        new_states->insert(st->getId(), st);
        const bool prev_empty =
            (key.B2 == 0u && key.W2 == NO_WITNESS);
        if (key.phase == ACC_WAIT_P2EMPTY && prev_empty && key.epoch_nonempty) {
            st->setFinal(true);
        }
    }

    witness.sync_stats_counts();
    const std::string name = "BuchiSumSupWitnessCached(" + A->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights,
                         global_min, global_max, init_state);
}

static Automaton* flatten_SumPlusMinus_Inf_cached_impl(NestedAutomaton* A,
                                                       value_function_t finite_aggregator,
                                                       weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Inf_cached requires SumPlus or SumMinus");
    }

    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    SumInfCachedBuilder builder(A, finite_aggregator, threshold);

    MapArray<Symbol*>* new_alphabet = new MapArray<Symbol*>(A->getAlphabetSize());
    for (size_t i = 0; i < A->getAlphabetSize(); ++i) {
        Symbol* original = A->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    MapStd<weight_t, Weight*> weight_register;
    MapArray<Weight*>* new_weights = new MapArray<Weight*>(3);
    auto get_weight = [&](const weight_t& value) -> Weight* {
        if (!weight_register.contains(value)) {
            Weight* w = new Weight(value);
            new_weights->insert(w->getId(), w);
            weight_register.insert(value, w);
        }
        return weight_register.at(value);
    };
    get_weight(weight_t(0));
    get_weight(weight_t(1));
    get_weight(weight_t(SILENT));

    struct Key {
        uint32_t parent = 0;
        SumInfCachedBuilder::BagId P1 = 0u;
        SumInfCachedBuilder::BagId P2 = 0u;
        acc_phase_t phase = ACC_WAIT_parent;
        bool epoch_nonempty = false;

        bool operator==(const Key& o) const {
            return parent == o.parent
                && P1 == o.P1
                && P2 == o.P2
                && phase == o.phase
                && epoch_nonempty == o.epoch_nonempty;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& key) const noexcept {
            uint64_t h = 0;
            mm_hash_combine(h, key.parent);
            mm_hash_combine(h, key.P1);
            mm_hash_combine(h, key.P2);
            mm_hash_combine(h, static_cast<uint8_t>(key.phase));
            mm_hash_combine(h, static_cast<uint8_t>(key.epoch_nonempty ? 1u : 0u));
            return static_cast<size_t>(mm_mix64(h));
        }
    };

    std::unordered_map<Key, State*, KeyHash> state_map;
    state_map.reserve(4096);
    std::deque<Key> worklist;
    unsigned int state_counter = 0;

    const weight_t global_min = weight_t(0);
    const weight_t global_max = weight_t(1);

    Key init;
    init.parent = static_cast<uint32_t>(A->getInitial()->getId());

    std::ostringstream ss;
    ss << "bcache_sum_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
    state_map.emplace(init, init_state);
    worklist.push_back(init);

    while (!worklist.empty()) {
        const Key current = worklist.front();
        worklist.pop_front();

        State* current_state = nullptr;
        {
            ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
            if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
            current_state = state_map.find(current)->second;
        }
        State* parent_state = A->getStates()->at(current.parent);
        const acc_phase_t phase_after_current =
            (current.phase == ACC_WAIT_parent)
                ? (parent_state->getFinal() ? ACC_WAIT_P2EMPTY : ACC_WAIT_parent)
                : ((current.P2 == 0u) ? ACC_WAIT_parent : ACC_WAIT_P2EMPTY);
        const bool reset_epoch =
            (current.phase == ACC_WAIT_P2EMPTY && current.P2 == 0u);

        for (uint32_t symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            const auto P1res = builder.step_bag(current.P1, symbol_id);
            if (!P1res.ok) continue;

            SumInfCachedBuilder::BagId P2_step = 0u;
            bool tracked_discharged = false;
            if (current.P2 != 0u) {
                const auto P2res = builder.step_bag(current.P2, symbol_id);
                if (!P2res.ok) continue;
                P2_step = P2res.next;
                tracked_discharged = P2res.any_discharged;
            }

            SetStd<Edge*>* succs = parent_state->getSuccessors(symbol_id);
            if (!succs) continue;

            for (Edge* parent_edge : *succs) {
                const uint32_t q_prime = static_cast<uint32_t>(parent_edge->getTo()->getId());
                const uint32_t child_index = static_cast<uint32_t>(
                    edgeWeightToChildIndex(parent_edge->getWeight()->getValue()));
                const bool is_silent = (child_index >= builder.k) || !builder.child_enabled(child_index);
                const bool boundary = (current.P2 == 0u);
                bool epoch_nonempty_to = reset_epoch ? (current.P1 != 0u) : current.epoch_nonempty;
                if (tracked_discharged) epoch_nonempty_to = true;
                if (!is_silent) epoch_nonempty_to = true;

                auto get_or_create = [&](const Key& key) -> State* {
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_lookup_calls++;
                        auto it = state_map.find(key);
                        if (it != state_map.end()) return it->second;
                    }

                    std::ostringstream s2;
                    s2 << "bcache_sum_" << state_counter++;
                    State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
                    {
                        ScopedStatsTimer timer(mmexp_enabled() ? &g_minmax_inf_experiment.stats.time_state_map_ms : nullptr);
                        if (mmexp_enabled()) g_minmax_inf_experiment.stats.state_map_insert_calls++;
                        state_map.emplace(key, ns);
                    }
                    worklist.push_back(key);
                    return ns;
                };

                if (is_silent) {
                    const auto P2_next = boundary ? P1res.next : P2_step;
                    const auto P1_next = boundary ? 0u : P1res.next;
                    const Key nxt{q_prime, P1_next, P2_next, phase_after_current, epoch_nonempty_to};

                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(SILENT)),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                    continue;
                }

                for (uint8_t guess = 0u; guess <= 1u; ++guess) {
                    mmexp_record_thr_spawn(child_index, guess, symbol_id);
                    const auto sc = builder.spawn_code(child_index, guess, symbol_id);
                    if (sc == SumInfCachedBuilder::OBL_DEAD) continue;

                    const auto P2_next = boundary ? P1res.next : P2_step;
                    const auto baseP1 = boundary ? 0u : P1res.next;
                    auto P1_next = baseP1;
                    if (sc != SumInfCachedBuilder::OBL_DISCHARGED) {
                        P1_next = builder.bag_add_obl(baseP1, sc);
                    }

                    const Key nxt{q_prime, P1_next, P2_next, phase_after_current, epoch_nonempty_to};

                    State* to_state = get_or_create(nxt);
                    Edge* ne = new Edge(new_alphabet->at(symbol_id),
                                        get_weight(weight_t(static_cast<unsigned int>(guess))),
                                        current_state,
                                        to_state);
                    current_state->addSuccessor(ne);
                    to_state->addPredecessor(ne);
                }
            }
        }
    }

    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    for (const auto& [key, st] : state_map) {
        new_states->insert(st->getId(), st);
        if (key.phase == ACC_WAIT_P2EMPTY && key.P2 == 0u && key.epoch_nonempty) {
            st->setFinal(true);
        }
    }

    builder.sync_stats_counts();
    const std::string name = "BuchiSumCached(" + A->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights,
                         global_min, global_max, init_state);
}

} // namespace

Automaton* NestedAutomaton::flatten_SumPlusMinus_Sup_witness_cached(value_function_t finite_aggregator,
                                                                    weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Sup_witness_cached requires SumPlus or SumMinus");
    }
    return flatten_SumPlusMinus_Sup_witness_cached_impl(this, finite_aggregator, threshold);
}

Automaton* NestedAutomaton::flatten_SumPlusMinus_Inf_cached(value_function_t finite_aggregator,
                                                            weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Inf_cached requires SumPlus or SumMinus");
    }
    return flatten_SumPlusMinus_Inf_cached_impl(this, finite_aggregator, threshold);
}

bool NestedAutomaton::isCompleteNested(std::vector<bool>* complete_flags) const {
    bool ret = true;

    if (complete_flags == nullptr) {
        if (!this->isComplete()) {
            return false;
        }

        for (unsigned int i = 0; i < this->getChildrenSize(); ++i) {
            if (this->getChild(i)->getStates()->size() < 2) continue;
            for (unsigned int state_id = 0; state_id < this->getChild(i)->getStates()->size(); ++state_id) {
                for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
                    bool isFinal = this->getChild(i)->getStates()->at(state_id)->getFinal();
                    unsigned int succ_size = this->getChild(i)->getStates()->at(state_id)->getSuccessors(symbol_id)->size();
                    if (!isFinal && succ_size < 1) {
                        return false;
                    }
                }
            }
        }
    } else {
        complete_flags->clear();
        complete_flags->resize(this->getChildrenSize() + 1, true);

        if (!this->isComplete()) {
            (*complete_flags)[0] = false;
            ret = false;
        }

        for (unsigned int i = 0; i < this->getChildrenSize(); ++i) {
            if (this->getChild(i)->getStates()->size() < 2) continue;
            for (unsigned int state_id = 0; state_id < this->getChild(i)->getStates()->size(); ++state_id) {
                for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
                    bool isFinal = this->getChild(i)->getStates()->at(state_id)->getFinal();
                    unsigned int succ_size = this->getChild(i)->getStates()->at(state_id)->getSuccessors(symbol_id)->size();
                    if (!isFinal && succ_size < 1) {
                        (*complete_flags)[i+1] = false;
                        ret = false;
                    }
                }
            }
        }
    }

    return ret;
}

bool NestedAutomaton::isDeterministicNested() const {
    if (!this->isDeterministic()) {
        return false;
    }

    for (unsigned int i = 0; i < this->getChildrenSize(); ++i) {
        if (this->getChild(i)->getStates()->size() < 2) continue;
        for (unsigned int state_id = 0; state_id < this->getChild(i)->getStates()->size(); ++state_id) {
            for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
                auto st = this->getChild(i)->getStates()->at(state_id);
                bool isFinal = st->getFinal();
                if (st->getSuccessors(symbol_id) == nullptr) continue;
                unsigned int succ_size = st->getSuccessors(symbol_id)->size();
                if ((isFinal && 0 < succ_size) || (!isFinal && 1 < succ_size)) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool NestedAutomaton::isDeterministicAndCompleteNested() const {
    if (!this->isDeterministic() || !this->isComplete()) {
        return false;
    }

    for (unsigned int i = 0; i < this->getChildrenSize(); ++i) {
        if (this->getChild(i)->getStates()->size() < 2) continue;
        for (unsigned int state_id = 0; state_id < this->getChild(i)->getStates()->size(); ++state_id) {
            for (unsigned int symbol_id = 0; symbol_id < this->alphabet->size(); ++symbol_id) {
                bool isFinal = this->getChild(i)->getStates()->at(state_id)->getFinal();
                unsigned int succ_size = this->getChild(i)->getStates()->at(state_id)->getSuccessors(symbol_id)->size();
                if ((isFinal && 0 < succ_size) || (!isFinal && 1 != succ_size)) {
                    return false;
                }
            }
        }
    }

    return true;
}


bool NestedAutomaton::isNonEmpty(value_function_t infVal, value_function_t finVal, weight_t x, weight_t bound) {
    // TRIVIAL CASES
    if (finVal == SumPlus && x <= weight_t(0)) {
        return hasAcceptingNonSilentNestedRun(this);
    }
    if (finVal == SumMinus && x > weight_t(0)) {
        return false;
    }

    // SPECIAL CASE: SumPlus + LimSupAvg has a fast path
    // Try supremum-based check first; if it succeeds, return early
    weight_t theoretical_bound = -1;  // Will be computed if needed for SumPlus + LimSupAvg
    if (finVal == SumPlus && infVal == LimSupAvg) {
        // Compute theoretical bound for Key Lemma
        unsigned int tb = 0;
        unsigned int max_weight = 1;
        for (unsigned int i = 0; i < this->children_->size(); i++) {
            tb = std::max(tb, (unsigned int)this->getChild(i)->getStates()->size());
            max_weight = std::max(max_weight, this->getChild(i)->getMaxDomain().to_uint());
        }
        theoretical_bound = weight_t(tb * max_weight * this->getStates()->size());

        // Fast path: check if supremum is unbounded
        Automaton* fastFlat = this->flatten_SumPlusMinus_Sup(SumPlus, theoretical_bound);
        Automaton* fastNonSilent = Automaton::removeSilentTransitions(fastFlat, LimSup, false);
#ifdef DEBUG
        std::cout << "unnested (fast path): " << fastNonSilent->getStates()->size() << " states, "
                  << fastNonSilent->getNbTransitions() << " edges, "
                  << fastNonSilent->getNbSCCs() << " SCCs ("
                  << fastNonSilent->getNbAcceptingSCCs() << " accepting)" << std::endl;
#endif
        bool fastResult = fastNonSilent->isNonEmpty_withFinal(LimSup, weight_t(1));
        delete fastNonSilent;
        delete fastFlat;

        if (fastResult) {
            return true;  // Fast path succeeded
        }
        // Fast path failed; fall through to slow path (SumB-based)
    }

    // STEP 1: FLATTEN
    Automaton* flat = nullptr;
    NestedAutomaton* split_nwa = nullptr;      // For SumMinus + LimAvg final-stop normalization
    NestedAutomaton* pre_sync_owned = nullptr; // For SumMinus + LimAvg preprocessing
    NestedAutomaton* sync_nwa = nullptr;

    // Monotonic nesting: SumPlus/SumMinus + Sup/LimSup/Inf/LimInf
    // These produce 0/1 automata encoding threshold achievement
    if ((finVal == SumPlus || finVal == SumMinus) &&
        (infVal == Sup || infVal == LimSup || infVal == Inf || infVal == LimInf)) {
        if (infVal == Sup || infVal == LimSup) {
            flat = this->flatten_SumPlusMinus_Sup(finVal, x);
        } else {
            flat = this->flatten_SumPlusMinus_Inf(finVal, x);
        }
    }
    // SumPlus + LimSupAvg: slow path using SumB (fast path already tried above)
    else if (finVal == SumPlus && infVal == LimSupAvg) {
        flat = this->flatten_regular(SumB, theoretical_bound);
    }
    // SumMinus + LimAvg: pseudo-determinization + synchronization
    else if (finVal == SumMinus && (infVal == LimInfAvg || infVal == LimSupAvg)) {
        split_nwa = this->splitContinuableChildFinals();
        NestedAutomaton* base_nwa = split_nwa ? split_nwa : this;
        NestedAutomaton* pre_sync = nullptr;

        std::vector<bool> complete_flags;
        if (base_nwa->isDeterministicNested()) {
            if (base_nwa->isCompleteNested(&complete_flags)) {
                pre_sync = base_nwa;
            } else {
                pre_sync_owned = base_nwa->makeCompleteNested(&complete_flags);
                pre_sync = pre_sync_owned;
            }
        } else {
            pre_sync_owned = base_nwa->determinizeWithMacroAlphabet();
            pre_sync = pre_sync_owned;
        }

        const uint64_t c_bound = compute_c_bound(pre_sync);
        sync_nwa = pre_sync->synchronizeChildren();
        flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
    }
    // Min_f/Max_f + Sup/LimSup/Inf/LimInf: monotonic min/max construction
    else if ((finVal == Max_f || finVal == Min_f) &&
             (infVal == Sup || infVal == LimSup || infVal == Inf || infVal == LimInf)) {
        if (infVal == Sup || infVal == LimSup) {
            flat = this->flatten_MinMax_Sup(finVal, x);
        } else {
            flat = this->flatten_MinMax_Inf(finVal, x);
        }
    }
    // Min_f/Max_f + LimAvg: use flatten_regular
    else if ((finVal == Max_f || finVal == Min_f) &&
             (infVal == LimInfAvg || infVal == LimSupAvg)) {
        flat = this->flatten_regular(finVal);
    }
    // SumB: use flatten_regular with bound
    else if (finVal == SumB) {
        flat = this->flatten_regular(finVal, bound);
    }
    else {
        QUAK_FAIL("isNonEmpty: unsupported aggregator combination");
    }

    // STEP 2: REMOVE SILENT TRANSITIONS (if necessary)
    Automaton* nonSilent = nullptr;
    bool monotoneThresholdBackend =
        ((finVal == SumPlus || finVal == SumMinus || finVal == Max_f || finVal == Min_f) &&
         (infVal == Sup || infVal == LimSup || infVal == Inf || infVal == LimInf));

    bool needsSilentRemoval = monotoneThresholdBackend ||
                              (finVal == SumPlus && infVal == LimSupAvg) ||
                              (finVal == SumMinus && (infVal == LimInfAvg || infVal == LimSupAvg)) ||
                              ((finVal == Max_f || finVal == Min_f) && (infVal == LimInfAvg || infVal == LimSupAvg)) ||
                              (finVal == SumB);

    if (needsSilentRemoval) {
        bool withShortcuts = monotoneThresholdBackend ? false : !(infVal == Inf || infVal == Sup);
        nonSilent = Automaton::removeSilentTransitions(flat, infVal, withShortcuts);
    } else {
        nonSilent = flat;
        flat = nullptr;  // Prevent double-delete
    }

#ifdef DEBUG
    std::cout << "unnested: " << nonSilent->getStates()->size() << " states, "
              << nonSilent->getNbTransitions() << " edges, "
              << nonSilent->getNbSCCs() << " SCCs ("
              << nonSilent->getNbAcceptingSCCs() << " accepting)" << std::endl;
    std::cout << "=== FLATTENED FOR isNonEmpty ===" << std::endl;
    nonSilent->print();
    std::cout << "=== END FLATTENED ===" << std::endl;
#endif

    // STEP 3: STANDARD EMPTINESS CHECK
    // Monotonic nesting cases use threshold 1 (0/1 automata); others use original threshold x
    weight_t checkThreshold = ((finVal == SumPlus || finVal == SumMinus || finVal == Max_f || finVal == Min_f) &&
                               (infVal == Sup || infVal == LimSup || infVal == Inf || infVal == LimInf))
                              ? weight_t(1) : x;
    bool result = nonSilent->isNonEmpty_withFinal(infVal, checkThreshold);

    // CLEANUP
    if (flat != nullptr) delete flat;
    delete nonSilent;
    delete sync_nwa;
    delete pre_sync_owned;
    delete split_nwa;

    return result;
}


bool NestedAutomaton::isUniversal(value_function_t infVal, value_function_t finVal, weight_t x, weight_t bound) {
    // Validate supported aggregator combinations
    if (!((finVal == Max_f || finVal == Min_f || finVal == SumB || finVal == SumPlus || finVal == SumMinus) &&
          (infVal == Sup || infVal == LimSup || infVal == Inf || infVal == LimInf))) {
        QUAK_FAIL("isUniversal: unsupported aggregator combination");
    }

    // Boundary cases for total-sign aggregators. SumPlus is always >= 0.
    // SumMinus is always <= 0, but a positive threshold is a counterexample
    // only when the accepted nested domain emits infinitely many real child
    // values; otherwise accepted-domain universality is vacuous.
    if (finVal == SumPlus && x <= weight_t(0)) return true;
    if (finVal == SumMinus && x > weight_t(0)) {
        return !hasAcceptingNonSilentNestedRun(this);
    }

    // STEP 1: FLATTEN
    // SumPlus/SumMinus are handled via SumB with appropriate bound
    value_function_t effectiveFinVal = finVal;
    weight_t effectiveBound = bound;

    if (finVal == SumPlus) {
        effectiveFinVal = SumB;
        effectiveBound = x;
    } else if (finVal == SumMinus) {
        effectiveFinVal = SumB;
        // SumMinus = min(0, sum). For threshold x <= 0:
        // SumMinus >= x iff sum >= x. Use bound = -x + 1 (positive)
        // to ensure proper SumB reduction (matches pattern in flatten_SumPlusMinus_*)
        effectiveBound = -x + weight_t(1);
    }

    Automaton* flat = this->flatten_regular(effectiveFinVal, effectiveBound);

    // STEP 2: REMOVE SILENT TRANSITIONS
    bool withShortcuts = !(infVal == Inf || infVal == Sup);
    Automaton* nonSilent = Automaton::removeSilentTransitions(flat, infVal, withShortcuts);

#ifdef DEBUG
    std::cout << "unnested: " << nonSilent->getStates()->size() << " states, "
              << nonSilent->getNbTransitions() << " edges, "
              << nonSilent->getNbSCCs() << " SCCs ("
              << nonSilent->getNbAcceptingSCCs() << " accepting)" << std::endl;
    std::cout << "=== FLATTENED AUTOMATON ===" << std::endl;
    nonSilent->print();
    std::cout << "=== END FLATTENED ===" << std::endl;
#endif

    // STEP 3: UNIVERSALITY OVER ACCEPTED FLATTENED RUNS
    bool result = nonSilent->isUniversal_withFinal(infVal, x);

    // CLEANUP
    delete nonSilent;
    delete flat;

    return result;
}
