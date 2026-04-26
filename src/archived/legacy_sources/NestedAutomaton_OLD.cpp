#include <cstddef>
#include <ostream>
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <queue>
#include <sstream>
#include <cmath>
#include <type_traits>
#include <functional>

#include "Automaton.h"
#include "ChildAutomaton.h"
#include "NestedAutomaton.h"
#include "Map.h"
#include "Parser.h"
#include "Edge.h"
#include "Set.h"
#include "State.h"
#include "Symbol.h"
#include "Weight.h"
#include "utility.h"
#include "FORKLIFT/inclusion.h"

/* ------------------------------ DESTRUCTOR & CONSTRUCTORS ------------------------------ */
NestedAutomaton::~NestedAutomaton() {
    // Clean up children_ array
    if (children_ != nullptr) {
        for (size_t i = 0; i < children_->size(); ++i) {
            delete children_->at(i);
        }
        delete children_;
    }
}

NestedAutomaton::NestedAutomaton(std::string name, Parser* parser, MapStd<std::string, Symbol*> sync_register)
    : Automaton(name, parser, sync_register)
{
    // Allocate children_ array with the num of child parsers
    children_ = new MapArray<ChildAutomaton*>(parser->child_parsers.size());

    //  For each child parser, initialize ChildAutomaton objects
    for (unsigned i = 0; i < parser->child_parsers.size(); ++i) {
        Parser* child_parser = parser->child_parsers[i];
        if (child_parser) {
            //auto* child = new ChildAutomaton(name + "_child" + std::to_string(i), child_parser, sync_register);
            auto* child = new ChildAutomaton(std::to_string(i), child_parser, sync_register);
            children_->insert(i, child);
        }
    }
}

// Constructs a nested automaton from a file with the same alphabet as another automaton
NestedAutomaton::NestedAutomaton(std::string filename, Automaton* other)
    : Automaton(filename, other)
{
    // Create a new parser from the file to get child information
    Parser* parser = new Parser(filename);

    // Set up sync_register if other automaton is provided
    MapStd<std::string, Symbol*> sync_register;
    if (other != nullptr) {
        for (unsigned int symbol_id = 0; symbol_id < other->getAlphabet()->size(); ++symbol_id) {
            Symbol* symbol = other->getAlphabet()->at(symbol_id);
            sync_register.insert(symbol->getName(), symbol);
        }
    }

    // Allocate children_ array with child parsers
    children_ = new MapArray<ChildAutomaton*>(parser->child_parsers.size());

    // Initialize ChildAutomaton objects
    for (unsigned i = 0; i < parser->child_parsers.size(); ++i) {
        Parser* child_parser = parser->child_parsers[i];
        if (child_parser) {
            //auto* child = new ChildAutomaton(name + "_child" + std::to_string(i), child_parser, sync_register);
            auto* child = new ChildAutomaton(std::to_string(i), child_parser, sync_register);
            children_->insert(i, child);
        }
    }
    delete parser;
}

// Helper constructor for NestedAutomaton
NestedAutomaton::NestedAutomaton(const Automaton* parent, MapArray<ChildAutomaton*>* children)
    : Automaton(*parent), // Use the public copy constructor
      children_(children) {

        this->setName(parent->getName() + "_noSilent");
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
    children_(children) {}

/* ------------------------------ REMOVING SILENT TRANSITIONS ------------------------------ */
NestedAutomaton* NestedAutomaton::removeSilentTransitions(const NestedAutomaton* A, value_function_t f) {
    // 1. Transform the parent automaton using the base class method
    Automaton* transformed_parent = Automaton::removeSilentTransitions(A, f);

    // 2. Shallow copy the children array (children remain unchanged)
    MapArray<ChildAutomaton*>* copied_children = new MapArray<ChildAutomaton*>(A->children_->size());
    for (unsigned i = 0; i < A->children_->size(); ++i) {
        if (A->children_->at(i)) {
            // Use the copy constructor for ChildAutomaton
            copied_children->insert(i, new ChildAutomaton(*A->children_->at(i)));
        }
    }

    // 3. Create new NestedAutomaton with transformed parent and copied children
    NestedAutomaton* result = new NestedAutomaton(transformed_parent, copied_children);

    delete transformed_parent;
    return result;
}

/* ------------------------------ HELPERS ------------------------------ */
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

                    ProdState init = { m_after, c1, w0 };

                    if (!visited.contains(init)) {
                        visited.insert(init);
                        if (child->isFinal(c1)) {
                            return_values.insert(w0);
                        } else {
                            worklist.push(init);
                        }
                    }
                }
            }
        }
    }

    // BFS on synchronized master x child (master must remain in good)
    while (!worklist.empty()) {
        ProdState cur = worklist.front(); worklist.pop();

        State* mcur = std::get<0>(cur);
        State* ccur = std::get<1>(cur);
        weight_t val = std::get<2>(cur);

        if (child->isFinal(ccur)) {
            return_values.insert(val);
            continue;
        }

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
                    ProdState nxt = { m2, c2, next_val };

                    if (!visited.contains(nxt)) {
                        visited.insert(nxt);
                        if (child->isFinal(c2)) {
                            return_values.insert(next_val);
                        } else {
                            worklist.push(nxt);
                        }
                    }
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

                    ProdState init = { m_after, c1, sum1, hit1 };

                    if (!visited.contains(init)) {
                        visited.insert(init);
                        if (child->isFinal(c1)) {
                            return_values.insert(hit1 != weight_t(0) ? hit1 : sum1);
                        } else {
                            worklist.push(init);
                        }
                    }
                }
            }
        }
    }

    // BFS on synchronized master x child (master must remain in good)
    while (!worklist.empty()) {
        ProdState cur = worklist.front(); worklist.pop();

        State* mcur = std::get<0>(cur);
        State* ccur = std::get<1>(cur);
        weight_t sum = std::get<2>(cur);
        weight_t hit = std::get<3>(cur);

        if (child->isFinal(ccur)) {
            return_values.insert(hit != weight_t(0) ? hit : sum);
            continue;
        }

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

                    ProdState nxt = { m2, c2, next_sum, next_hit };

                    if (!visited.contains(nxt)) {
                        visited.insert(nxt);
                        if (child->isFinal(c2)) {
                            return_values.insert(next_hit != weight_t(0) ? next_hit : next_sum);
                        } else {
                            worklist.push(nxt);
                        }
                    }
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


void computeGlobalDomains(const NestedAutomaton* nwa, weight_t& global_min, weight_t& global_max){
    global_min = std::numeric_limits<float>::max();
    global_max = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < nwa->getChildrenSize(); ++i) {
        ChildAutomaton* child = nwa->getChild(i);
        if (child == nullptr) continue;
        global_min = std::min(global_min, child->getMinDomain());
        global_max = std::max(global_max, child->getMaxDomain());
    }
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

            ChildValState seed = { q, w0 };
            if (visited.contains(seed)) continue;
            visited.insert(seed);

            if (child->isFinal(q)) {
                return_values.insert(w0);   // return after consuming exactly one letter
            } else {
                worklist.push(seed);
            }
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

                if (visited.contains(nxt)) continue;
                visited.insert(nxt);

                if (child->isFinal(t)) {
                    return_values.insert(next_val);
                } else {
                    worklist.push(nxt);
                }
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
        SumState s = {st, sum, bound_hit};
        if (visited.contains(s)) return;
        visited.insert(s);

        if (child->isFinal(st)) {
            // Finals are sinks: record and do not enqueue
            record_return(sum, bound_hit);
        } else {
            worklist.push(s);
        }
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

/*SetStd<weight_t> computeMinMaxReturnValues(ChildAutomaton* child, value_function_t finVal) {
    SetStd<weight_t> return_values;

    using ValueState = std::pair<State*, weight_t>;
    std::queue<ValueState> worklist;

    // best_value[q] = best (Min_f: smallest, Max_f: largest) value seen so far for state q
    MapStd<State*, weight_t> best_value;

    // Set initial value
    weight_t initial_value;
    if (finVal == Min_f) {
        initial_value = weight_t(std::numeric_limits<float>::max());     // +infty
    } else { // Max_f
        initial_value = weight_t(std::numeric_limits<float>::lowest());  // -infty
    }

    State* init = child->getInitial();
    worklist.push({init, initial_value});
    best_value.insert(init, initial_value);

    while (!worklist.empty()) {
        ValueState current = worklist.front();
        worklist.pop();

        State* curr_state = current.first;
        weight_t curr_value = current.second;

        // Skip outdated entries
        if (!best_value.contains(curr_state) || best_value.at(curr_state) != curr_value) {
            continue;
        }

        // Explore successors
        for (size_t sym_id = 0; sym_id < child->getAlphabetSize(); ++sym_id) {
            SetStd<Edge*>* successors = curr_state->getSuccessors(sym_id);
            if (!successors) continue;

            for (Edge* edge : *successors) {
                State* next_state = edge->getTo();
                weight_t edge_weight = edge->getWeight()->getValue();

                // Update accumulated value
                weight_t next_value;
                if (finVal == Min_f) {
                    next_value = std::min(curr_value, edge_weight);
                } else { // Max_f
                    next_value = std::max(curr_value, edge_weight);
                }

                // If reached a final state, record this value
                if (child->isFinal(next_state)) {
                    return_values.insert(next_value);
                    // treat finals as sinks, like before
                    continue;
                }

                bool improved = false;
                if (!best_value.contains(next_state)) {
                    improved = true;
                } else {
                    weight_t old = best_value.at(next_state);
                    if (finVal == Min_f && next_value < old) {
                        improved = true;
                    } else if (finVal == Max_f && next_value > old) {
                        improved = true;
                    }
                }

                if (improved) {
                    best_value.update(next_state, next_value);
                    worklist.push({next_state, next_value});
                }
            }
        }
    }

    return return_values;
}*/

/*SetStd<weight_t> computeSumBReturnValues(ChildAutomaton* child, weight_t bound) {
    SetStd<weight_t> return_values;

    // BFS to explore all possible sums
    // State: (automaton_state, accumulated_sum, bound_value_hit)
    // bound_value_hit: 0 = never exceeded, +bound = hit upper bound, -bound = hit lower bound
    using SumState = std::tuple<State*, weight_t, weight_t>;
    std::queue<SumState> worklist;
    SetStd<SumState> visited;

    // Start from initial state with sum 0, no bound hit
    SumState init_state = {child->getInitial(), weight_t(0), weight_t(0)};
    worklist.push(init_state);
    visited.insert(init_state);

    // Check if initial state is final
    if (child->isFinal(child->getInitial())) {
        return_values.insert(weight_t(0));
    }

    while (!worklist.empty()) {
        SumState current = worklist.front();
        worklist.pop();

        State* curr_state = std::get<0>(current);
        weight_t curr_sum = std::get<1>(current);
        weight_t bound_hit = std::get<2>(current);  // 0, +bound, or -bound

        // If this is a final state, record the return value
        if (child->isFinal(curr_state)) {
            if (bound_hit != weight_t(0)) {
                // Path exceeded bounds at some point -> return the bound value that was hit
                return_values.insert(bound_hit);
            } else {
                // Never exceeded bounds -> return actual sum
                return_values.insert(curr_sum);
            }
            continue;   // Final states are treated as sinks
        }

        // Explore all outgoing transitions
        for (size_t sym_id = 0; sym_id < child->getAlphabetSize(); ++sym_id) {
            for (Edge* edge : *(curr_state->getSuccessors(sym_id))) {
                State* next_state = edge->getTo();
                weight_t edge_weight = edge->getWeight()->getValue();
                weight_t raw_sum = curr_sum + edge_weight;

                // Determine next state values
                weight_t next_sum;
                weight_t next_bound_hit = bound_hit;

                if (raw_sum > bound && bound_hit == weight_t(0)) {
                    // First time hitting upper bound
                    next_sum = bound;
                    next_bound_hit = bound;  // Remember we hit +bound
                } else if (raw_sum < -bound && bound_hit == weight_t(0)) {
                    // First time hitting lower bound
                    next_sum = -bound;
                    next_bound_hit = -bound;  // Remember we hit -bound
                } else if (bound_hit != weight_t(0)) {
                    // Already exceeded bounds before, so continue with bounded value
                    next_sum = applyBound(raw_sum, bound);
                    // next_bound_hit stays the same (already hit bound)
                } else {
                    // Normal case: within bounds
                    next_sum = raw_sum;
                    // next_bound_hit stays 0
                }

                SumState next_sum_state = {next_state, next_sum, next_bound_hit};

                // Continue exploration if not visited
                if (!visited.contains(next_sum_state)) {
                    visited.insert(next_sum_state);

                    if (child->isFinal(next_state)) {
                        // Final state: return bound value if ever exceeded
                        if (next_bound_hit != weight_t(0)) {
                            return_values.insert(next_bound_hit);  // Return the bound that was hit
                        } else {
                            return_values.insert(next_sum);
                        }
                    } else {
                        // Non-final: continue BFS
                        worklist.push(next_sum_state);
                    }
                }
            }
        }
    }

    return return_values;
}*/

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

// Compute the global set of all possible return values across all children -- used only for debugging
SetStd<weight_t> NestedAutomaton::computeGlobalReturnValues(value_function_t finVal, weight_t bound) {
    SetStd<weight_t> global_values;

    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (child == nullptr) continue;

        SetStd<weight_t> child_values = this->computeChildReturnValues(child, finVal, bound);

        // Union with global set
        for (weight_t val : child_values) {
            global_values.insert(val);
        }
    }

    // Add silent value as well
    global_values.insert(weight_t(SILENT));

    return global_values;
}

using MonitorKey = std::pair<size_t, weight_t>;  // (i, j)
// Construct all S_ij (monitors) and collect Q_S and F_S
void constructMonitors(
    const NestedAutomaton* nwa,
    const SetStd<weight_t>& global_return_values,
    MapStd<MonitorKey, ChildAutomaton*>& monitors,
    SetStd<State*>& Q_S,
    SetStd<State*>& F_S,
    value_function_t finVal,
    weight_t bound,
    std::vector<SetStd<weight_t>>& child_return_values
) {
    for (size_t i = 0; i < nwa->getChildrenSize(); ++i) {
        ChildAutomaton* child = nwa->getChild(i);
        if (child == nullptr || child->getStates()->size() < 2) continue;

        // Only create monitors for possible return values by the child i
        for (weight_t j : child_return_values[i]) {
            ChildAutomaton* monitor = child->determiniseToS_ij(i, j, finVal, bound);

            MonitorKey key = {i,j};
            monitors.insert(key, monitor);

            // Collect Q_S and F_S
            for (size_t s = 0; s < monitor->getStates()->size(); ++s) {
                Q_S.insert(monitor->getStates()->at(s));
            }
            for (State* s : *(monitor->getFinalStates())) {
                s->setFinal(true);
                F_S.insert(s);
            }

            // monitor->print();
        }
    }
}

// Find the sucessors of the states in P1 or P2. Exclude if final state
SetStd<State*> stepMonitors(const SetStd<State*>& P, Symbol* a, const SetStd<State*>& F_S) {
    SetStd<State*> result;

    for (State* q : P) {
        // Since monitors are DFA, there's either 1 or 0 transitions
        // std::cout << "Stepping monitor state " << q->getName() << " on symbol " << a->getName() << std::endl;
        for (Edge* e : *(q->getSuccessors(a->getId()))){
            State* q_prime = e->getTo();

            // Add only non-accepting states
            if (F_S.contains(q_prime) != true) {
                result.insert(q_prime);
            }
        }
    }
    return result;
}

// Extra function to remove the accepting states in P
void removeFinalStates(SetStd<State*>&P, const SetStd<State*>& F_S) {
    auto it = P.begin();
    while (it != P.end()) {
        if (F_S.contains(*it)) {
            State* to_remove = *it;
            ++it;
            P.erase(to_remove);
        } else {
            ++it;
        }
    }
}

// Helper to print a BuchiState
void printBuchiState(const BuchiState& bs) {
    std::cout << "<";
    if (bs.parent_state) std::cout << bs.parent_state->getName();
    else std::cout << "null";
    std::cout << ", " << bs.last_guess;
    std::cout << ", P1={";

    bool first = true;
    for (State* s : bs.P1) {
        if (!first) std::cout << ",";
        std::cout << s->getName();
        first = false;
    }
    std::cout << "}, P2={";

    first = true;
    for (State* s : bs.P2) {
        if (!first) std::cout << ",";
        std::cout << s->getName();
        first = false;
    }
    std::cout << "}>";
}

// Helper: Initialize büchi automaton components
State* initializeBuchi(
    const NestedAutomaton* nwa,
    MapArray<Symbol*>*& new_alphabet,
    MapArray<Weight*>*& new_weights,
    MapStd<weight_t, Weight*>& weight_register,
    SetStd<weight_t>& global_return_values,
    MapStd<BuchiState, State*>& state_map,
    BuchiState init_buchi,
    weight_t global_min,
    weight_t global_max,
    std::queue<BuchiState>& worklist,
    unsigned int& state_counter
) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    // Copy alphabet from master
    size_t alph_size = nwa->getAlphabetSize();
    new_alphabet = new MapArray<Symbol*>(alph_size);
    for (size_t i = 0; i < alph_size; ++i) {
        Symbol* original = nwa->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    // Create weights array and weight register
    new_weights = new MapArray<Weight*>(global_return_values.size());
    weight_register.clear();

    for (weight_t value : global_return_values) {
        Weight* w = new Weight(value);
        new_weights->insert(w->getId(), w);
        weight_register.insert(value, w);
    }

    // Initialize state counter to name the states of Buchi
    state_counter = 0;

    // Fill in Buchi state
    init_buchi.parent_state = nwa->getInitial();
    init_buchi.last_guess = weight_t(INIT_BUCHI_VALUE);
    init_buchi.P1 = SetStd<State*>();
    init_buchi.P2 = SetStd<State*>();

    std::ostringstream ss;
    ss << "b_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);

    // Map the state to the tuple
    state_map[init_buchi] = init_state;
    // Add init Buchi state to start exploration
    worklist.push(init_buchi);

    return init_state;
}

void processBuchiTransition(
    const BuchiState& current_gs,
    unsigned int symbol_id,
    MapStd<BuchiState, State*>& state_map,
    MapArray<Symbol*>* new_alphabet,
    MapArray<Weight*>* new_weights,
    MapStd<weight_t, Weight*>& weight_register,
    const MapStd<MonitorKey, ChildAutomaton*>& monitors,
    const SetStd<State*>& F_S,
    unsigned int& state_counter,
    SetStd<weight_t>& global_return_values,
    weight_t global_min,
    weight_t global_max,
    std::queue<BuchiState>& worklist,
    const std::vector<SetStd<weight_t>>& child_return_values
) {
    Symbol* symbol = new_alphabet->at(symbol_id);
    State* current_state = state_map[current_gs];

    // Precompute P1next, P2next once per (BuchiState, symbol)
    SetStd<State*> P1next = stepMonitors(current_gs.P1, symbol, F_S);
    SetStd<State*> P2next = stepMonitors(current_gs.P2, symbol, F_S);

    for (Edge* parent_edge : *(current_gs.parent_state)->getSuccessors(symbol_id)) {
        State* q_prime = parent_edge->getTo();

        bool is_silent = (parent_edge->getWeight()->getValue() == 0);

        if (is_silent) {
            // CASE (A): Silent transition
            BuchiState next_global(q_prime, SILENT, P1next, P2next);

            if (!state_map.contains(next_global)) {
                std::ostringstream ss;
                ss << "b_" << state_counter++;
                State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                state_map[next_global] = next_state;
                worklist.push(next_global);
            }

            Weight* weight = weight_register.at(SILENT);
            Edge* new_edge = new Edge(symbol, weight, current_state, state_map[next_global]);
            current_state->addSuccessor(new_edge);
            state_map[next_global]->addPredecessor(new_edge);
        } else {
            // CASE (B/C): Call transitions
            weight_t parent_weight = parent_edge->getWeight()->getValue();
            size_t child_index = static_cast<size_t>(parent_weight.to_float());

            const SetStd<weight_t>& child_vals = child_return_values[child_index];

            for (const weight_t& guess : child_vals) {
                // child-specific guesses only, SILENT not in child_vals
                MonitorKey key = {child_index, guess};
                if (!monitors.contains(key)) continue;

                ChildAutomaton* monitor = monitors.at(key);
                State* monitor_init = monitor->getInitial();
                monitor_init = (*monitor_init->getSuccessors(symbol_id)->begin())->getTo(); // step monitor on call symbol


                SetStd<State*> P1new, P2new;

                if (current_gs.P2.size() == 0) {
                    // CASE (B)
                    P1new.insert(monitor_init);
                    P2new = P1next;
                } else {
                    // CASE (C)
                    P1new = P1next;
                    P1new.insert(monitor_init);
                    removeFinalStates(P1new, F_S);
                    P2new = P2next;
                }

                BuchiState next_global(q_prime, guess, P1new, P2new);

                if (!state_map.contains(next_global)) {
                    std::ostringstream ss;
                    ss << "b_" << state_counter++;
                    State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                    state_map[next_global] = next_state;
                    worklist.push(next_global);
                }

                Weight* weight = weight_register.at(guess);
                Edge* new_edge = new Edge(symbol, weight, current_state, state_map[next_global]);
                current_state->addSuccessor(new_edge);
                state_map[next_global]->addPredecessor(new_edge);
            }
        }
    }
}

//}

// Assumes:
//      - a NWA AA <A_mas; f; B_1, ... B_k>
//      - finite-word deterministic automata S_ij for each i and j
//      - input alphabet is the same for parent and children
//      - single finVal function for all child automata
// Ensures: Outputs a büchi automaton A' such that L(A') = L(A)
Automaton* NestedAutomaton::flatten_regular_parent_trivial(value_function_t finVal, weight_t bound) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    // Initialize containers for Buchi automaton
    MapArray<Symbol*>* new_alphabet;
    MapArray<Weight*>* new_weights;
    weight_t global_min, global_max;
    BuchiState init_buchi;

    // Helper containers
    MapStd<BuchiState, State*> state_map;
    std::queue<BuchiState> worklist;
    MapStd<weight_t, Weight*> weight_register;
    unsigned int state_counter;

    // 1. Compute global return values for all children
    size_t k = this->getChildrenSize();

    // Per-child return values
    std::vector<SetStd<weight_t>> child_return_values(k);

    // Global union
    SetStd<weight_t> global_return_values;
    global_return_values.insert(weight_t(SILENT));  // silent always present

    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (!child) continue;

        // child_return_values[i] = computeChildReturnValues(child, finVal, bound);
        child_return_values[i] = this->computeChildReturnValuesParentAware(i, finVal, bound);

        for (const weight_t& v : child_return_values[i]) {
            global_return_values.insert(v);
        }
    }


    // computeGlobalDomains(this, global_min, global_max);
    for(weight_t val : global_return_values) {
        if (val != SILENT) {
            global_min = std::min(global_min, val);
            global_max = std::max(global_max, val);
        }
    }

    // 2. Construct all S_ij and collect Q_S and F_S
    MapStd<MonitorKey, ChildAutomaton*> monitors;
    SetStd<State*> Q_S, F_S;
    constructMonitors(this, global_return_values, monitors, Q_S, F_S, finVal, bound, child_return_values);

    // 3. Initialize
    State* init_state = initializeBuchi(this, new_alphabet, new_weights, weight_register, global_return_values, state_map, init_buchi, global_min, global_max, worklist, state_counter);

    // 4. Build the product automaton on-the-fly
    while (worklist.empty() != true)  {
        BuchiState current_gs = worklist.front(); worklist.pop();

        // for each symbol start transition from current
        for (unsigned symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            processBuchiTransition(current_gs, symbol_id, state_map,
                new_alphabet, new_weights, weight_register, monitors, F_S, state_counter, global_return_values, global_min, global_max, worklist, child_return_values);
        }
    }
    // 5. Create state and accepting state arrays
    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
    // SetStd<State*>* accepting_states = new SetStd<State*>();

    for (const auto& [global_state, state] : state_map) {
        new_states->insert(state->getId(), state);  // TODO: Check if state->getId is correct

        // Mark as accepting if P2 is empty
        if (global_state.P2.size() == 0) {
            // accepting_states->insert(state);
            state->setFinal(true);
        }
    }

    // 6. Construct and return the product automaton
    std::string buchi_name = "Buchi(" + this->getName() + ")";
    Automaton* buchi = new Automaton(
        buchi_name, new_alphabet, new_states,
        new_weights, global_min, global_max,
        init_state
    );

    // 7. Cleanup
    for (const auto& [key, monitor] : monitors) {
        delete monitor;
    }

    return buchi;
}

bool NestedAutomaton::allParentStatesFinal() const {
    for (State* q : *(this->getStates())) {
        if (!q->getFinal()) return false;
    }
    return true;
}


// On-the-fly generalized-buchi to buchi conversion:
// first visit parent-accepting states, then visit states with P2 empty
// buchi accepting states in the flattened automaton are exactly those in phase WAIT_P2EMPTY with P2 empty
static constexpr bool ACC_WAIT_MASTER_  = 0; // waiting to see parent in accepting state
static constexpr bool ACC_WAIT_P2EMPTY_ = 1; // waiting to see P2 nonempty

static inline bool advance_acc_phase(const BuchiState_acceptance& s) {
    if (s.acceptance_flag == ACC_WAIT_MASTER_) {
        return (s.parent_state->getFinal()) ? ACC_WAIT_P2EMPTY_ : ACC_WAIT_MASTER_;
    } else {
        return (s.P2.size() == 0) ? ACC_WAIT_MASTER_ : ACC_WAIT_P2EMPTY_;
    }
}

State* initializeBuchi_acceptance(
    const NestedAutomaton* nwa,
    MapArray<Symbol*>*& new_alphabet,
    MapArray<Weight*>*& new_weights,
    MapStd<weight_t, Weight*>& weight_register,
    SetStd<weight_t>& global_return_values,
    MapStd<BuchiState_acceptance, State*>& state_map,
    BuchiState_acceptance init_buchi,
    weight_t global_min,
    weight_t global_max,
    std::queue<BuchiState_acceptance>& worklist,
    unsigned int& state_counter
) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    // Copy alphabet from master
    size_t alph_size = nwa->getAlphabetSize();
    new_alphabet = new MapArray<Symbol*>(alph_size);
    for (size_t i = 0; i < alph_size; ++i) {
        Symbol* original = nwa->getAlphabet()->at(i);
        Symbol* copy = new Symbol(original->getName());
        new_alphabet->insert(i, copy);
    }

    // Create weights array and weight register
    new_weights = new MapArray<Weight*>(global_return_values.size());
    weight_register.clear();

    for (weight_t value : global_return_values) {
        Weight* w = new Weight(value);
        new_weights->insert(w->getId(), w);
        weight_register.insert(value, w);
    }

    // Initialize state counter to name the states of Buchi
    state_counter = 0;

    // Fill in Buchi state
    init_buchi.parent_state = nwa->getInitial();
    init_buchi.P1           = SetStd<State*>();
    init_buchi.P2           = SetStd<State*>();

    // start in phase "wait for master accept"
    init_buchi.acceptance_flag    = ACC_WAIT_MASTER_;

    std::ostringstream ss;
    ss << "b_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);

    // Map the state to the tuple
    state_map[init_buchi] = init_state;

    // Add init Buchi state to start exploration
    worklist.push(init_buchi);

    return init_state;
}

void processBuchiTransition_acceptance(
    const BuchiState_acceptance& current_gs,
    unsigned int symbol_id,
    MapStd<BuchiState_acceptance, State*>& state_map,
    MapArray<Symbol*>* new_alphabet,
    MapArray<Weight*>* new_weights,
    MapStd<weight_t, Weight*>& weight_register,
    const MapStd<MonitorKey, ChildAutomaton*>& monitors,
    const SetStd<State*>& F_S,
    unsigned int& state_counter,
    SetStd<weight_t>& global_return_values,
    weight_t global_min,
    weight_t global_max,
    std::queue<BuchiState_acceptance>& worklist,
    const std::vector<SetStd<weight_t>>& child_return_values
) {
    Symbol* symbol = new_alphabet->at(symbol_id);
    State* current_state = state_map[current_gs];

    uint8_t phase_after_current = advance_acc_phase(current_gs);

    // Precompute P1next, P2next once per (BuchiState, symbol)
    SetStd<State*> P1next = stepMonitors(current_gs.P1, symbol, F_S);
    SetStd<State*> P2next = stepMonitors(current_gs.P2, symbol, F_S);

    for (Edge* parent_edge : *(current_gs.parent_state)->getSuccessors(symbol_id)) {
        State* q_prime = parent_edge->getTo();

        bool is_silent = (parent_edge->getWeight()->getValue() == 0);

        if (is_silent) {
            // CASE (A): Silent transition
            BuchiState_acceptance next_global(q_prime, P1next, P2next, phase_after_current);

            if (!state_map.contains(next_global)) {
                std::ostringstream ss;
                ss << "b_" << state_counter++;
                State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                state_map[next_global] = next_state;
                worklist.push(next_global);
            }

            Weight* weight = weight_register.at(SILENT);
            Edge* new_edge = new Edge(symbol, weight, current_state, state_map[next_global]);
            current_state->addSuccessor(new_edge);
            state_map[next_global]->addPredecessor(new_edge);

        } else {
            // CASE (B/C): Call transitions
            weight_t parent_weight = parent_edge->getWeight()->getValue();
            size_t child_index = static_cast<size_t>(parent_weight.to_float());

            const SetStd<weight_t>& child_vals = child_return_values[child_index];

            for (const weight_t& guess : child_vals) {
                // child-specific guesses only, SILENT not in child_vals
                MonitorKey key = {child_index, guess};
                if (!monitors.contains(key)) continue;

                ChildAutomaton* monitor = monitors.at(key);

                // Step monitor on the call symbol
                State* monitor_init = monitor->getInitial();
                monitor_init = (*monitor_init->getSuccessors(symbol_id)->begin())->getTo();

                SetStd<State*> P1new, P2new;

                if (current_gs.P2.size() == 0) {
                    // CASE (B)
                    P1new.insert(monitor_init);
                    P2new = P1next;
                } else {
                    // CASE (C)
                    P1new = P1next;
                    P1new.insert(monitor_init);
                    removeFinalStates(P1new, F_S);
                    P2new = P2next;
                }

                BuchiState_acceptance next_global(q_prime, P1new, P2new, phase_after_current);

                if (!state_map.contains(next_global)) {
                    std::ostringstream ss;
                    ss << "b_" << state_counter++;
                    State* next_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);
                    state_map[next_global] = next_state;
                    worklist.push(next_global);
                }

                Weight* weight = weight_register.at(guess);
                Edge* new_edge = new Edge(symbol, weight, current_state, state_map[next_global]);
                current_state->addSuccessor(new_edge);
                state_map[next_global]->addPredecessor(new_edge);
            }
        }
    }
}

Automaton* NestedAutomaton::flatten_regular_parent_acceptance(value_function_t finVal, weight_t bound) {
    State::RESET();
    Symbol::RESET();
    Weight::RESET();

    // Initialize containers for Buchi automaton
    MapArray<Symbol*>* new_alphabet = nullptr;
    MapArray<Weight*>* new_weights  = nullptr;

    weight_t global_min, global_max;

    // Helper containers
    BuchiState_acceptance init_buchi;
    MapStd<BuchiState_acceptance, State*> state_map;
    std::queue<BuchiState_acceptance> worklist;
    MapStd<weight_t, Weight*> weight_register;
    unsigned int state_counter = 0;

    // 1. Compute global return values for all children
    size_t k = this->getChildrenSize();

    // Per-child return values
    std::vector<SetStd<weight_t>> child_return_values(k);

    // Global union
    SetStd<weight_t> global_return_values;
    global_return_values.insert(weight_t(SILENT));  // silent always present

    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (!child) continue;

        // child_return_values[i] = computeChildReturnValues(child, finVal, bound);
        child_return_values[i] = this->computeChildReturnValuesParentAware(i, finVal, bound);

        for (const weight_t& v : child_return_values[i]) {
            global_return_values.insert(v);
        }
    }

    // Robust domain init (avoid UB if globals were uninitialized)
    bool have_non_silent = false;
    for (weight_t val : global_return_values) {
        if (val == SILENT) continue;
        if (!have_non_silent) {
            global_min = val;
            global_max = val;
            have_non_silent = true;
        } else {
            global_min = std::min(global_min, val);
            global_max = std::max(global_max, val);
        }
    }
    if (!have_non_silent) {
        global_min = weight_t(0);
        global_max = weight_t(0);
    }

    // 2. Construct all S_ij and collect Q_S and F_S
    MapStd<MonitorKey, ChildAutomaton*> monitors;
    SetStd<State*> Q_S, F_S;
    constructMonitors(this, global_return_values, monitors, Q_S, F_S, finVal, bound, child_return_values);

    // 3. Initialize
    State* init_state = initializeBuchi_acceptance(
        this, new_alphabet, new_weights, weight_register, global_return_values,
        state_map, init_buchi, global_min, global_max, worklist, state_counter
    );

    // 4. Build the product automaton on-the-fly
    while (!worklist.empty()) {
        BuchiState_acceptance current_gs = worklist.front();
        worklist.pop();

        for (unsigned symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
            processBuchiTransition_acceptance(
                current_gs, symbol_id, state_map,
                new_alphabet, new_weights, weight_register,
                monitors, F_S, state_counter, global_return_values,
                global_min, global_max, worklist, child_return_values
            );
        }
    }

    // 5. Create state array and mark Buchi-accepting states
    MapArray<State*>* new_states = new MapArray<State*>(state_map.size());

    for (const auto& [global_state, state] : state_map) {
        new_states->insert(state->getId(), state);

        // Accept iff we're in phase WAIT_P2EMPTY and P2 is empty.
        if (global_state.acceptance_flag == ACC_WAIT_P2EMPTY_ && global_state.P2.size() == 0) {
            state->setFinal(true);
        }
    }

    // 6. Construct and return the product automaton
    std::string buchi_name = "Buchi(" + this->getName() + ")";
    Automaton* buchi = new Automaton(
        buchi_name,
        new_alphabet,
        new_states,
        new_weights,
        global_min,
        global_max,
        init_state
    );

    // 7. Cleanup
    for (const auto& [key, monitor] : monitors) {
        delete monitor;
    }

    return buchi;
}

























// // =============================================================================
// // CORRECTED LEMMA 10 FLATTENING IMPLEMENTATION
// // =============================================================================
// // Drop-in replacement for flatten_regular_parent_acceptance_obligations
// //
// // Key fix: Uses pre-built verification DFAs with SET semantics (not bags)
// // Achieves PTIME for deterministic children with bounded sizes
// //
// // FIXES APPLIED (vs your previous "new" version):
// //  1) STRICT obligation stepping: if ANY obligation dies (INVALID / non-live), reject the successor.
// //  2) Spawn consumes the current symbol: verify_after_call = step(verify_init, symbol_id).
// // =============================================================================

// // External function (assumed to be defined elsewhere)
// weight_t transitionFunction(weight_t state_value,
//                             weight_t transit_value,
//                             value_function_t finVal,
//                             weight_t bound);

// // =============================================================================
// // PART 1: CHILD TABLES (reused from original, with determinism check)
// // =============================================================================

// struct ChildTables {
//     ChildAutomaton* child = nullptr;
//     uint32_t n_states = 0;
//     uint32_t alph = 0;
//     uint32_t init = 0;

//     struct Trans {
//         uint32_t to;
//         weight_t w;
//     };

//     // CSR-like storage for (st, a) -> edges[ off[idx] .. off[idx+1] )
//     std::vector<uint32_t> off;
//     std::vector<Trans> edges;

//     std::vector<uint8_t> is_final;
//     std::vector<uint8_t> live;

//     inline uint32_t idx(uint32_t st, uint32_t a) const {
//         return st * alph + a;
//     }

//     // Check if this child is deterministic
//     bool is_deterministic() const {
//         for (size_t i = 0; i + 1 < off.size(); ++i) {
//             if (off[i + 1] - off[i] > 1) return false;
//         }
//         return true;
//     }
// };

// static bool build_child_tables(ChildAutomaton* c, ChildTables& out) {
//     if (!c) return false;
//     out.child = c;
//     out.n_states = (uint32_t)c->getStates()->size();
//     out.alph     = (uint32_t)c->getAlphabet()->size();
//     out.init     = (uint32_t)c->getInitial()->getId();

//     out.is_final.assign(out.n_states, 0);
//     for (uint32_t s = 0; s < out.n_states; ++s) {
//         State* st = c->getStates()->at(s);
//         out.is_final[s] = st->getFinal() ? 1 : 0;
//     }

//     const size_t cells = (size_t)out.n_states * (size_t)out.alph;
//     out.off.assign(cells + 1, 0);

//     // count edges per cell
//     for (uint32_t s = 0; s < out.n_states; ++s) {
//         State* from = c->getStates()->at(s);
//         for (uint32_t a = 0; a < out.alph; ++a) {
//             SetStd<Edge*>* succs = from->getSuccessors(a);
//             out.off[(size_t)out.idx(s, a) + 1] = succs ? (uint32_t)succs->size() : 0;
//         }
//     }

//     // prefix sum
//     for (size_t i = 1; i < out.off.size(); ++i) out.off[i] += out.off[i - 1];
//     out.edges.resize(out.off.back());

//     // fill + predecessors for LIVE computation
//     std::vector<uint32_t> cur = out.off;
//     std::vector<std::vector<uint32_t>> pred(out.n_states);

//     for (uint32_t s = 0; s < out.n_states; ++s) {
//         State* from = c->getStates()->at(s);
//         for (uint32_t a = 0; a < out.alph; ++a) {
//             SetStd<Edge*>* succs = from->getSuccessors(a);
//             if (!succs) continue;
//             const uint32_t id = out.idx(s, a);
//             for (Edge* e : *succs) {
//                 if (!e) continue;
//                 const uint32_t t = (uint32_t)e->getTo()->getId();
//                 const uint32_t pos = cur[(size_t)id]++;
//                 out.edges[(size_t)pos] = ChildTables::Trans{t, e->getWeight()->getValue()};
//                 pred[t].push_back(s);
//             }
//         }
//     }

//     // LIVE = reverse BFS from finals
//     out.live.assign(out.n_states, 0);
//     std::deque<uint32_t> q;
//     for (uint32_t s = 0; s < out.n_states; ++s) {
//         if (out.is_final[s]) {
//             out.live[s] = 1;
//             q.push_back(s);
//         }
//     }
//     while (!q.empty()) {
//         uint32_t v = q.front(); q.pop_front();
//         for (uint32_t u : pred[v]) {
//             if (!out.live[u]) {
//                 out.live[u] = 1;
//                 q.push_back(u);
//             }
//         }
//     }

//     return true;
// }

// // =============================================================================
// // PART 2: VERIFICATION DFA
// // =============================================================================
// // For each (child, return_value) pair, we build a DFA S_{child,value} that
// // recognizes words where the child returns that value.
// // States are (child_state, bounded_accumulator) pairs.
// // =============================================================================

// struct VerificationDFA {
//     uint32_t child_idx;
//     weight_t target_value;

//     uint32_t num_child_states;
//     uint32_t alphabet_size;

//     // Accumulator range: for bound B, we use [-B-1, B+1] to handle saturation
//     // Mapped to indices [0, 2B+2]
//     int32_t acc_min;
//     int32_t acc_max;
//     uint32_t acc_range;

//     uint32_t num_states;
//     uint32_t initial_state;

//     static constexpr uint32_t INVALID = UINT32_MAX;

//     // Transition table: transitions[state * alphabet_size + symbol] -> next_state
//     std::vector<uint32_t> transitions;

//     // Accepting and live states
//     std::vector<uint8_t> is_accepting;
//     std::vector<uint8_t> is_live;

//     // Convert (child_state, accumulator) to flat index
//     uint32_t to_flat(uint32_t cs, int32_t acc) const {
//         int32_t acc_idx = acc - acc_min;
//         if (acc_idx < 0) acc_idx = 0;
//         if (acc_idx >= (int32_t)acc_range) acc_idx = (int32_t)acc_range - 1;
//         return cs * acc_range + (uint32_t)acc_idx;
//     }

//     // Get successor state for a symbol
//     uint32_t successor(uint32_t state, uint32_t symbol) const {
//         if (state >= num_states || symbol >= alphabet_size) return INVALID;
//         return transitions[(size_t)state * alphabet_size + symbol];
//     }
// };

// // Initial accumulator value for each value function
// static inline weight_t initial_acc_for_finVal(value_function_t finVal) {
//     switch (finVal) {
//         case Max_f: return std::numeric_limits<float>::lowest();
//         case Min_f: return std::numeric_limits<float>::infinity();
//         default: return weight_t(0);
//     }
// }

// // Check if accumulated value matches target.
// // For Max_f / Min_f there is NO saturation / bound-equivalence.
// // Only SumB keeps the saturation convention.
// static inline bool value_matches_target(weight_t acc, weight_t target,
//                                         value_function_t finVal, weight_t bound) {
//     if (acc == target) return true;

//     if (finVal == SumB) {
//         if (target >= bound  && acc >= bound + 1) return true;
//         if (target <= -bound && acc <= -bound - 1) return true;
//     }

//     // Max_f / Min_f: no saturation
//     return false;
// }

// static std::vector<weight_t> compute_acc_domain_minmax(
//     const ChildTables& child,
//     value_function_t finVal,
//     weight_t bound
// ) {
//     // Collect distinct transition weights
//     std::vector<weight_t> W;
//     W.reserve(child.edges.size());
//     for (const auto& tr : child.edges) W.push_back(tr.w);
//     std::sort(W.begin(), W.end());
//     W.erase(std::unique(W.begin(), W.end()), W.end());

//     // Closure under transitionFunction starting from the proper initial accumulator
//     std::set<weight_t> dom;
//     std::deque<weight_t> q;

//     const weight_t init = initial_acc_for_finVal(finVal); // -inf or +inf
//     dom.insert(init);
//     q.push_back(init);

//     while (!q.empty()) {
//         const weight_t acc = q.front();
//         q.pop_front();
//         for (const weight_t& w : W) {
//             const weight_t acc2 = transitionFunction(acc, w, finVal, bound);
//             auto ins = dom.insert(acc2);
//             if (ins.second) q.push_back(acc2);
//         }
//     }

//     return std::vector<weight_t>(dom.begin(), dom.end());
// }

// // Build verification DFA for a specific (child, target_value)
// //
// // Semantics (finite slave runs):
// //  - A run TERMINATES immediately upon entering a final child state.
// //  - The run is ACCEPTING iff it terminates in a final state whose accumulated value matches target_value.
// // Therefore:
// //  - Final states must be TERMINAL (no outgoing transitions).
// //  - Transitions that would enter a final state with the WRONG value are INVALID (that branch dies).
// //
// // Implementation strategy:
// //  - If child is deterministic: compact product DFA over (child_state, bounded_acc).
// //  - If child is nondeterministic: determinize on-the-fly (subset construction over pairs).
// static VerificationDFA build_verification_dfa(
//     const ChildTables& child,
//     uint32_t child_idx,
//     weight_t target_value,
//     value_function_t finVal,
//     weight_t bound
// ) {
//     static int once = 0;
//     if (!once++) std::cerr << "[PATCHED build_verification_dfa] running\n";

//     VerificationDFA dfa;
//     dfa.child_idx = child_idx;
//     dfa.target_value = target_value;
//     dfa.num_child_states = child.n_states;
//     dfa.alphabet_size = child.alph;

// //     // Accumulator range
// //     const int32_t B = (int32_t)std::ceil(std::abs(bound.to_float()));
// //     dfa.acc_min = -B - 1;
// //     dfa.acc_max =  B + 1;
// //     dfa.acc_range = (uint32_t)(dfa.acc_max - dfa.acc_min + 1);

// //     auto clamp_acc = [&](int32_t x) -> int32_t {
// //         if (x < dfa.acc_min) return dfa.acc_min;
// //         if (x > dfa.acc_max) return dfa.acc_max;
// //         return x;
// //     };

// //     auto init_acc_int = [&]() -> int32_t {
// //         if (finVal == Max_f) return dfa.acc_min; // -infinity sentinel
// //         if (finVal == Min_f) return dfa.acc_max; // +infinity sentinel
// //         return 0;
// //     }();

//     const bool use_domain_minmax = (finVal == Max_f || finVal == Min_f);

//     // For SumB we keep the old bounded integer buckets.
//     // For Max/Min we switch to a finite exact domain (no bound buckets).
//     std::vector<weight_t> acc_values; // only used when use_domain_minmax == true

//     auto clamp_acc = [&](int32_t x) -> int32_t {
//         // only meaningful for SumB
//         if (x < dfa.acc_min) return dfa.acc_min;
//         if (x > dfa.acc_max) return dfa.acc_max;
//         return x;
//     };

//     auto acc_value_of = [&](int32_t acc_int) -> weight_t {
//         if (!use_domain_minmax) return (weight_t)acc_int;           // SumB path
//         return acc_values[(uint32_t)acc_int];                       // Max/Min path (acc_int is an index)
//     };

//     auto acc_index_of = [&](const weight_t& v) -> int32_t {
//         // only used for Max/Min path
//         auto it = std::lower_bound(acc_values.begin(), acc_values.end(), v);
//         if (it == acc_values.end() || !(*it == v)) return -1;
//         return (int32_t)(it - acc_values.begin());
//     };

//     int32_t init_acc_int = 0;

//     if (!use_domain_minmax) {
//         // SumB: bounded integer buckets [-B-1 .. B+1]
//         const int32_t B = (int32_t)std::ceil(std::abs(bound.to_float()));
//         dfa.acc_min = -B - 1;
//         dfa.acc_max =  B + 1;
//         dfa.acc_range = (uint32_t)(dfa.acc_max - dfa.acc_min + 1);
//         init_acc_int = 0;
//     } else {
//         // Max/Min: finite exact domain, acc_int is an INDEX into acc_values
//         acc_values = compute_acc_domain_minmax(child, finVal, bound);
//         if (acc_values.empty()) {
//             // should not happen; keep a 1-element domain to avoid UB
//             acc_values.push_back(initial_acc_for_finVal(finVal));
//         }

//         dfa.acc_min = 0;
//         dfa.acc_max = (int32_t)acc_values.size() - 1;
//         dfa.acc_range = (uint32_t)acc_values.size();

//         init_acc_int = acc_index_of(initial_acc_for_finVal(finVal));
//         if (init_acc_int < 0) init_acc_int = 0; // defensive
//     }

//     const bool det = child.is_deterministic();

//     // -------------------------------------------------------------------------
//     // Deterministic child: compact product DFA over (cs, acc)
//     // -------------------------------------------------------------------------
//     if (det) {
//         dfa.num_states = dfa.num_child_states * dfa.acc_range;
//         dfa.initial_state = dfa.to_flat(child.init, init_acc_int);

//         dfa.transitions.assign((size_t)dfa.num_states * dfa.alphabet_size, VerificationDFA::INVALID);
//         dfa.is_accepting.assign(dfa.num_states, 0);

//         for (uint32_t cs = 0; cs < dfa.num_child_states; ++cs) {
//             for (int32_t acc = dfa.acc_min; acc <= dfa.acc_max; ++acc) {
//                 const uint32_t state = dfa.to_flat(cs, acc);

//                 // Accepting iff we are in a FINAL child state and value matches target.
//                 if (child.is_final[cs]) {
//                     if (value_matches_target(acc_value_of(acc), target_value, finVal, bound)) {
//                         dfa.is_accepting[state] = 1;
//                     }
//                     continue;
//                 }
//                 // Non-final: build symbol transitions
//                 for (uint32_t a = 0; a < dfa.alphabet_size; ++a) {
//                     const uint32_t cell = child.idx(cs, a);
//                     const uint32_t b0 = child.off[cell];
//                     const uint32_t e0 = child.off[cell + 1];
//                     if (b0 >= e0) continue;

//                     // Deterministic: there should be exactly one edge. If not, still be safe.
//                     const auto& tr = child.edges[(size_t)b0];

//                     const weight_t acc_cur = acc_value_of(acc);
//                     const weight_t acc2_w  = transitionFunction(acc_cur, tr.w, finVal, bound);

//                     int32_t acc2;
//                     if (!use_domain_minmax) {
//                         acc2 = clamp_acc((int32_t)acc2_w);
//                     } else {
//                         acc2 = acc_index_of(acc2_w);
//                         if (acc2 < 0) continue; // should not happen if domain closure is correct
//                     }

//                     if (child.is_final[tr.to]) {
//                         if (value_matches_target(acc_value_of(acc2), target_value, finVal, bound)) {
//                             dfa.transitions[(size_t)state * dfa.alphabet_size + a] = dfa.to_flat(tr.to, acc2);
//                         }
//                         continue;
//                     }
//                     // Otherwise, continue only if the child state can still reach some final (coarse pruning)
//                     if (!child.live[tr.to]) continue;

//                     dfa.transitions[(size_t)state * dfa.alphabet_size + a] = dfa.to_flat(tr.to, acc2);
//                 }
//             }
//         }

//         // Live states: reverse BFS from accepting states
//         dfa.is_live.assign(dfa.num_states, 0);
//         std::vector<std::vector<uint32_t>> preds(dfa.num_states);

//         preds.assign(dfa.num_states, {});
//         for (uint32_t s = 0; s < dfa.num_states; ++s) {
//             for (uint32_t a = 0; a < dfa.alphabet_size; ++a) {
//                 const uint32_t nxt = dfa.transitions[(size_t)s * dfa.alphabet_size + a];
//                 if (nxt != VerificationDFA::INVALID) preds[nxt].push_back(s);
//             }
//         }
//         // Optional micro-dedup to avoid quadratic blowups when many symbols share successors.
//         for (auto& v : preds) {
//             std::sort(v.begin(), v.end());
//             v.erase(std::unique(v.begin(), v.end()), v.end());
//         }

//         std::deque<uint32_t> q;
//         for (uint32_t s = 0; s < dfa.num_states; ++s) {
//             if (dfa.is_accepting[s]) {
//                 dfa.is_live[s] = 1;
//                 q.push_back(s);
//             }
//         }
//         while (!q.empty()) {
//             const uint32_t v = q.front();
//             q.pop_front();
//             for (uint32_t u : preds[v]) {
//                 if (!dfa.is_live[u]) {
//                     dfa.is_live[u] = 1;
//                     q.push_back(u);
//                 }
//             }
//         }

//         uint32_t bad_live_wrong_final = 0;
//         for (uint32_t cs = 0; cs < dfa.num_child_states; ++cs) {
//             if (!child.is_final[cs]) continue;
//             for (int32_t acc = dfa.acc_min; acc <= dfa.acc_max; ++acc) {
//                 const uint32_t s = dfa.to_flat(cs, acc);
//                 if (dfa.is_accepting[s]) continue;     // correct terminal
//                 if (dfa.is_live[s]) bad_live_wrong_final++;
//             }
//         }
//         std::cerr << "DFA(child=" << child_idx << ",target=" << target_value.to_float()
//                 << "): bad_live_wrong_final=" << bad_live_wrong_final << "\n";


//         return dfa;
//     }

//     // -------------------------------------------------------------------------
//     // Nondeterministic child: determinize on-the-fly (subset construction)
//     // State = canonical set of (child_state, bounded_acc_int).
//     //
//     // IMPORTANT: existential nondet inside the slave:
//     //  - If ANY branch can terminate correctly on this symbol, we can ACCEPT (discharge).
//     // So we route such transitions to a single accepting sink state.
//     // -------------------------------------------------------------------------
//     struct Pair {
//         uint32_t st;
//         int32_t  acc;
//     };
//     using Conf = std::vector<Pair>;

//     auto conf_canon = [&](Conf& c) {
//         if (c.empty()) return;
//         std::sort(c.begin(), c.end(), [](const Pair& x, const Pair& y) {
//             if (x.st != y.st) return x.st < y.st;
//             return x.acc < y.acc;
//         });
//         c.erase(std::unique(c.begin(), c.end(), [](const Pair& x, const Pair& y) {
//             return x.st == y.st && x.acc == y.acc;
//         }), c.end());
//     };

//     struct ConfHash {
//         size_t operator()(const Conf& c) const noexcept {
//             // FNV-1a-ish
//             size_t h = 1469598103934665603ull;
//             for (const auto& p : c) {
//                 h ^= (size_t)p.st + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
//                 h ^= (size_t)(uint32_t)p.acc + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
//             }
//             return h;
//         }
//     };
//     struct ConfEq {
//         bool operator()(const Conf& a, const Conf& b) const noexcept {
//             if (a.size() != b.size()) return false;
//             for (size_t i = 0; i < a.size(); ++i) {
//                 if (a[i].st != b[i].st || a[i].acc != b[i].acc) return false;
//             }
//             return true;
//         }
//     };

//     // State 0 = accepting sink
//     const uint32_t ACCEPT = 0;

//     std::vector<Conf> id2conf;
//     id2conf.reserve(256);
//     id2conf.push_back(Conf{}); // ACCEPT has no configuration

//     std::unordered_map<Conf, uint32_t, ConfHash, ConfEq> conf2id;
//     conf2id.reserve(256);

//     // helper to append a new DFA state
//     auto add_state = [&](Conf&& conf) -> uint32_t {
//         conf_canon(conf);
//         auto it = conf2id.find(conf);
//         if (it != conf2id.end()) return it->second;

//         const uint32_t id = (uint32_t)id2conf.size();
//         conf2id.emplace(conf, id);
//         id2conf.push_back(std::move(conf));

//         // extend DFA tables
//         dfa.is_accepting.push_back(0);
//         dfa.is_live.push_back(0);
//         dfa.transitions.insert(dfa.transitions.end(), dfa.alphabet_size, VerificationDFA::INVALID);
//         return id;
//     };

//     // init tables
//     dfa.is_accepting.clear();
//     dfa.is_live.clear();
//     dfa.transitions.clear();

//     dfa.is_accepting.push_back(1); // ACCEPT
//     dfa.is_live.push_back(0);
//     dfa.transitions.insert(dfa.transitions.end(), dfa.alphabet_size, VerificationDFA::INVALID);

//     // initial configuration (before reading any symbol)
//     Conf init;
//     init.push_back(Pair{child.init, init_acc_int});
//     const uint32_t INIT = add_state(std::move(init));
//     dfa.initial_state = INIT;

//     // BFS determinization
//     std::deque<uint32_t> wl;
//     wl.push_back(INIT);
//     std::vector<uint8_t> expanded;
//     expanded.resize(2, 0); // will grow as needed

//     auto ensure_expanded_size = [&](uint32_t n) {
//         if (expanded.size() < (size_t)n) expanded.resize(n, 0);
//     };

//     while (!wl.empty()) {
//         const uint32_t sid = wl.front();
//         wl.pop_front();

//         ensure_expanded_size((uint32_t)id2conf.size());
//         if (sid == ACCEPT) continue;
//         if (expanded[sid]) continue;
//         expanded[sid] = 1;

//         const Conf& conf = id2conf[sid];

//         for (uint32_t a = 0; a < dfa.alphabet_size; ++a) {
//             bool can_accept = false;
//             Conf succ;

//             for (const auto& p : conf) {
//                 const uint32_t cs = p.st;
//                 const int32_t  acc = p.acc;

//                 // A final state should never be carried in Conf, but ignore defensively.
//                 if (child.is_final[cs]) continue;

//                 const uint32_t cell = child.idx(cs, a);
//                 const uint32_t b0 = child.off[cell];
//                 const uint32_t e0 = child.off[cell + 1];
//                 for (uint32_t k0 = b0; k0 < e0; ++k0) {
//                     const auto& tr = child.edges[(size_t)k0];

//                     const weight_t acc_cur = acc_value_of(acc);
//                     const weight_t acc2_w  = transitionFunction(acc_cur, tr.w, finVal, bound);

//                     int32_t acc2;
//                     if (!use_domain_minmax) {
//                         acc2 = clamp_acc((int32_t)acc2_w);
//                     } else {
//                         acc2 = acc_index_of(acc2_w);
//                         if (acc2 < 0) continue;
//                     }

//                     if (child.is_final[tr.to]) {
//                         if (value_matches_target(acc_value_of(acc2), target_value, finVal, bound)) {
//                             can_accept = true;
//                         }
//                         continue;
//                     }

//                     if (!child.live[tr.to]) continue;
//                     succ.push_back(Pair{tr.to, acc2});
//                 }
//             }

//             uint32_t next_id = VerificationDFA::INVALID;
//             if (can_accept) {
//                 next_id = ACCEPT;
//             } else {
//                 conf_canon(succ);
//                 if (!succ.empty()) {
//                     const uint32_t before = (uint32_t)id2conf.size();
//                     next_id = add_state(std::move(succ));
//                     if (id2conf.size() > before) {
//                         wl.push_back(next_id);
//                     }
//                 }
//             }

//             dfa.transitions[(size_t)sid * dfa.alphabet_size + a] = next_id;
//         }
//     }

//     dfa.num_states = (uint32_t)id2conf.size();

//     // Live states: reverse BFS from ACCEPT
//     dfa.is_live.assign(dfa.num_states, 0);
//     std::vector<std::vector<uint32_t>> preds(dfa.num_states);

//     for (uint32_t s = 0; s < dfa.num_states; ++s) {
//         for (uint32_t a = 0; a < dfa.alphabet_size; ++a) {
//             const uint32_t nxt = dfa.transitions[(size_t)s * dfa.alphabet_size + a];
//             if (nxt != VerificationDFA::INVALID) preds[nxt].push_back(s);
//         }
//     }
//     for (auto& v : preds) {
//         std::sort(v.begin(), v.end());
//         v.erase(std::unique(v.begin(), v.end()), v.end());
//     }

//     std::deque<uint32_t> q;
//     dfa.is_live[ACCEPT] = 1;
//     q.push_back(ACCEPT);
//     while (!q.empty()) {
//         const uint32_t v = q.front();
//         q.pop_front();
//         for (uint32_t u : preds[v]) {
//             if (!dfa.is_live[u]) {
//                 dfa.is_live[u] = 1;
//                 q.push_back(u);
//             }
//         }
//     }

//     return dfa;
// }


// // =============================================================================
// // PART 3: GLOBAL VERIFICATION SYSTEM
// // =============================================================================
// // Manages all verification DFAs with global state indexing.
// // Q_S = disjoint union of all verification DFA states
// // =============================================================================

// struct GlobalVerificationSystem {
//     std::vector<VerificationDFA> dfas;
//     std::vector<uint32_t> dfa_offset;  // Global state offset for each DFA
//     uint32_t total_states;             // |Q_S|

//     // Map (child_idx, return_value) -> DFA index
//     MapStd<std::pair<uint32_t, weight_t>, uint32_t> child_value_to_dfa;

//     GlobalVerificationSystem() : total_states(0) {}

//     // Get global initial state for spawning (BEFORE consuming any symbol)
//     uint32_t get_initial(uint32_t child_idx, weight_t guess) const {
//         auto key = std::make_pair(child_idx, guess);
//         if (!child_value_to_dfa.contains(key)) return VerificationDFA::INVALID;
//         uint32_t idx = child_value_to_dfa.at(key);
//         return dfa_offset[idx] + dfas[idx].initial_state;
//     }

//     // Check if global state is accepting
//     bool is_accepting(uint32_t global) const {
//         if (global == VerificationDFA::INVALID) return false;
//         for (size_t i = 0; i < dfas.size(); ++i) {
//             uint32_t start = dfa_offset[i];
//             uint32_t end = start + dfas[i].num_states;
//             if (global >= start && global < end) {
//                 return dfas[i].is_accepting[global - start] != 0;
//             }
//         }
//         return false;
//     }

//     // Check if global state is live
//     bool is_live(uint32_t global) const {
//         if (global == VerificationDFA::INVALID) return false;
//         for (size_t i = 0; i < dfas.size(); ++i) {
//             uint32_t start = dfa_offset[i];
//             uint32_t end = start + dfas[i].num_states;
//             if (global >= start && global < end) {
//                 return dfas[i].is_live[global - start] != 0;
//             }
//         }
//         return false;
//     }

//     // Step a global state by a symbol
//     uint32_t step(uint32_t global, uint32_t symbol) const {
//         if (global == VerificationDFA::INVALID) return VerificationDFA::INVALID;
//         for (size_t i = 0; i < dfas.size(); ++i) {
//             uint32_t start = dfa_offset[i];
//             uint32_t end = start + dfas[i].num_states;
//             if (global >= start && global < end) {
//                 uint32_t local = global - start;
//                 uint32_t next_local = dfas[i].successor(local, symbol);
//                 if (next_local == VerificationDFA::INVALID) return VerificationDFA::INVALID;
//                 return start + next_local;
//             }
//         }
//         return VerificationDFA::INVALID;
//     }
// };

// // Build verification system from all children
// static GlobalVerificationSystem build_verification_system(
//     const std::vector<ChildTables>& child_tables,
//     const std::vector<SetStd<weight_t>>& child_return_values,
//     value_function_t finVal,
//     weight_t bound
// ) {
//     GlobalVerificationSystem sys;

//     for (uint32_t i = 0; i < child_tables.size(); ++i) {
//         if (!child_tables[i].child) continue;
//         if (child_tables[i].n_states < 2) continue;

//         for (const weight_t& val : child_return_values[i]) {
//             uint32_t dfa_idx = (uint32_t)sys.dfas.size();
//             sys.child_value_to_dfa.insert(std::make_pair(i, val), dfa_idx);

//             sys.dfa_offset.push_back(sys.total_states);
//             sys.dfas.push_back(build_verification_dfa(
//                 child_tables[i], i, val, finVal, bound
//             ));
//             sys.total_states += sys.dfas.back().num_states;
//         }
//     }

//     return sys;
// }

// // =============================================================================
// // PART 4: STATE SET (for P1, P2)
// // =============================================================================
// // Uses sorted vector for set semantics. For very small Q_S, this is efficient.
// // Key property: add() is idempotent (set semantics, not bag).
// // =============================================================================

// struct StateSet {
//     std::vector<uint32_t> elems;  // Always sorted, unique

//     void clear() { elems.clear(); }

//     void add(uint32_t s) {
//         auto it = std::lower_bound(elems.begin(), elems.end(), s);
//         if (it == elems.end() || *it != s) {
//             elems.insert(it, s);
//         }
//         // If already present, do nothing (set semantics!)
//     }

//     bool empty() const { return elems.empty(); }

//     bool operator==(const StateSet& o) const { return elems == o.elems; }
//     bool operator<(const StateSet& o) const { return elems < o.elems; }

//     template<typename F>
//     void for_each(F&& f) const {
//         for (uint32_t s : elems) f(s);
//     }
// };

// // Step all states in a set for one symbol.
// //
// // IMPORTANT: Obligations are universal constraints. If ANY tracked verification state
// // has no valid continuation (INVALID transition or reaches a non-live non-accepting state),
// // then this whole symbol-step is impossible and the successor must be rejected.
// // On success, we fill 'next' with the stepped set and DROP accepting states (discharged).
// static bool step_state_set_strict(
//     const StateSet& current,
//     uint32_t symbol,
//     const GlobalVerificationSystem& sys,
//     StateSet& next
// ) {
//     next.clear();
//     for (uint32_t s : current.elems) {
//         const uint32_t ns = sys.step(s, symbol);
//         if (ns == VerificationDFA::INVALID) {
//             return false;
//         }
//         if (sys.is_accepting(ns)) {
//             continue; // discharged
//         }
//         if (!sys.is_live(ns)) {
//             return false; // obligation cannot be met anymore
//         }
//         next.add(ns);
//     }
//     return true;
// }

// // =============================================================================
// // PART 5: FLATTENED BÜCHI STATE
// // =============================================================================

// enum acc_phase_t : uint8_t { ACC_WAIT_MASTER = 0, ACC_WAIT_P2EMPTY = 1 };

// struct BuchiState_Lemma10 {
//     State* parent_state;
//     weight_t last_guess;
//     StateSet P1;      // SET of verification DFA states (not bag!)
//     StateSet P2;      // SET of verification DFA states (not bag!)
//     acc_phase_t phase;

//     BuchiState_Lemma10()
//         : parent_state(nullptr), last_guess(0), P1(), P2(), phase(ACC_WAIT_MASTER) {}

//     bool operator==(const BuchiState_Lemma10& o) const {
//         return parent_state == o.parent_state
//             && last_guess == o.last_guess
//             && P1 == o.P1
//             && P2 == o.P2
//             && phase == o.phase;
//     }

//     bool operator<(const BuchiState_Lemma10& o) const {
//         if (parent_state != o.parent_state) return parent_state < o.parent_state;
//         if (last_guess != o.last_guess) return last_guess < o.last_guess;
//         if (!(P1 == o.P1)) return P1 < o.P1;
//         if (!(P2 == o.P2)) return P2 < o.P2;
//         return phase < o.phase;
//     }
// };

// static inline acc_phase_t advance_phase(const BuchiState_Lemma10& s) {
//     if (s.phase == ACC_WAIT_MASTER) {
//         return (s.parent_state->getFinal()) ? ACC_WAIT_P2EMPTY : ACC_WAIT_MASTER;
//     } else {
//         return (s.P2.empty()) ? ACC_WAIT_MASTER : ACC_WAIT_P2EMPTY;
//     }
// }

// // =============================================================================
// // PART 6: MAIN FLATTENING FUNCTION
// // =============================================================================

// Automaton* NestedAutomaton::flatten_regular_parent_acceptance_obligations(
//     value_function_t finVal,
//     weight_t bound
// ) {
//     if (!(finVal == SumB || finVal == Max_f || finVal == Min_f)) {
//         QUAK_FAIL("flatten_regular_parent_acceptance_lemma10: finVal must be SumB / Max_f / Min_f");
//     }

//     State::RESET();
//     Symbol::RESET();
//     Weight::RESET();

//     MapArray<Symbol*>* new_alphabet = nullptr;
//     MapArray<Weight*>* new_weights  = nullptr;

//     weight_t global_min = weight_t(0), global_max = weight_t(0);

//     MapStd<BuchiState_Lemma10, State*> state_map;
//     std::deque<BuchiState_Lemma10> worklist;
//     MapStd<weight_t, Weight*> weight_register;
//     unsigned int state_counter = 0;

//     // 1) Compute return values per child
//     const size_t k = this->getChildrenSize();
//     std::vector<SetStd<weight_t>> child_return_values(k);
//     SetStd<weight_t> global_return_values;
//     global_return_values.insert(weight_t(SILENT));

//     for (size_t i = 0; i < k; ++i) {
//         ChildAutomaton* child = this->getChild(i);
//         if (!child) continue;
//         child_return_values[i] = this->computeChildReturnValuesParentAware(i, finVal, bound);
//         for (const weight_t& v : child_return_values[i]) {
//             global_return_values.insert(v);
//         }
//     }

//     bool have_non_silent = false;
//     for (weight_t val : global_return_values) {
//         if (val == weight_t(SILENT)) continue;
//         if (!have_non_silent) {
//             global_min = val; global_max = val;
//             have_non_silent = true;
//         } else {
//             global_min = std::min(global_min, val);
//             global_max = std::max(global_max, val);
//         }
//     }
//     if (!have_non_silent) {
//         global_min = weight_t(0);
//         global_max = weight_t(0);
//     }

//     // 2) Build child tables
//     std::vector<ChildTables> child_tab(k);
//     for (size_t i = 0; i < k; ++i) {
//         ChildAutomaton* c = this->getChild(i);
//         if (!c) continue;
//         if (c->getStates()->size() < 2) continue;
//         build_child_tables(c, child_tab[i]);
//     }

//     // 3) Build verification system (the key to correct complexity!)
//     GlobalVerificationSystem verify_sys = build_verification_system(
//         child_tab, child_return_values, finVal, bound
//     );

//     std::cerr << "master_states=" << this->getStates()->size() << "\n";
//     for (size_t i = 0; i < this->getChildrenSize(); ++i) {
//         auto* c = this->getChild(i);
//         if (!c) continue;
//         std::cerr << "child["<<i<<"] states=" << c->getStates()->size()
//                     << " det=" << (child_tab[i].is_deterministic() ? 1 : 0)
//                     << " return_values=" << child_return_values[i].size()
//                     << "\n";
//     }
//     std::cerr << "|Q_S|=" << verify_sys.total_states << "\n";

//     // 4) Copy alphabet + build weight objects
//     {
//         const size_t alph_size = this->getAlphabetSize();
//         new_alphabet = new MapArray<Symbol*>(alph_size);
//         for (size_t i = 0; i < alph_size; ++i) {
//             Symbol* original = this->getAlphabet()->at(i);
//             Symbol* copy = new Symbol(original->getName());
//             new_alphabet->insert(i, copy);
//         }

//         new_weights = new MapArray<Weight*>(global_return_values.size());
//         weight_register.clear();
//         for (weight_t value : global_return_values) {
//             Weight* w = new Weight(value);
//             new_weights->insert(w->getId(), w);
//             weight_register.insert(value, w);
//         }
//     }

//     // 5) Initial Büchi state
//     BuchiState_Lemma10 init;
//     init.parent_state = this->getInitial();
//     init.last_guess = weight_t(INIT_BUCHI_VALUE);
//     init.P1.clear();
//     init.P2.clear();
//     init.phase = ACC_WAIT_MASTER;

//     std::ostringstream ss;
//     ss << "b_" << state_counter++;
//     State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);

//     state_map[init] = init_state;
//     worklist.push_back(init);

//     size_t maxP1 = 0;
//     size_t maxP2 = 0;
//     std::unordered_set<uint64_t> seen;
//     auto key = [&](uint32_t src, uint32_t sym, uint32_t w_id, uint32_t dst) -> uint64_t {
//         // Simple 64-bit mix (collision-resistant enough for this use)
//         uint64_t h = 1469598103934665603ULL;
//         auto add = [&](uint64_t v) {
//             h ^= v + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2);
//         };
//         add(src); add(sym); add(w_id); add(dst);
//         return h;
//     };




//     // 6) BFS construction
//     while (!worklist.empty()) {
//         BuchiState_Lemma10 current = std::move(worklist.front());
//         worklist.pop_front();

//         maxP1 = std::max(maxP1, current.P1.elems.size());
//         maxP2 = std::max(maxP2, current.P2.elems.size());

//         State* current_state = state_map[current];
//         const acc_phase_t phase_after_current = advance_phase(current);

//         for (unsigned symbol_id = 0; symbol_id < new_alphabet->size(); ++symbol_id) {
//             // Step verification states in P1 and P2 (STRICT: failure rejects the whole successor)
//             StateSet P1_stepped;
//             if (!step_state_set_strict(current.P1, symbol_id, verify_sys, P1_stepped)) {
//                 continue;
//             }

//             StateSet P2_stepped;
//             if (!current.P2.empty()) {
//                 if (!step_state_set_strict(current.P2, symbol_id, verify_sys, P2_stepped)) {
//                     continue;
//                 }
//             } else {
//                 P2_stepped.clear();
//             }

//             SetStd<Edge*>* succs = current.parent_state->getSuccessors(symbol_id);
//             if (!succs) continue;

//             for (Edge* parent_edge : *succs) {
//                 State* q_prime = parent_edge->getTo();
//                 const weight_t pw = parent_edge->getWeight()->getValue();
//                 const bool is_silent = (pw == weight_t(0));

//                 if (is_silent) {
//                     // Silent transition
//                     BuchiState_Lemma10 nxt;
//                     nxt.parent_state = q_prime;
//                     nxt.last_guess = weight_t(SILENT);
//                     nxt.phase = phase_after_current;

//                     // Handle P1/P2 boundary
//                     if (current.P2.empty()) {
//                         nxt.P1.clear();
//                         nxt.P2 = P1_stepped;
//                     } else {
//                         nxt.P1 = P1_stepped;
//                         nxt.P2 = P2_stepped;
//                     }

//                     if (!state_map.contains(nxt)) {
//                         std::ostringstream s2;
//                         s2 << "b_" << state_counter++;
//                         State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
//                         state_map[nxt] = ns;
//                         worklist.push_back(nxt);
//                     }

//                     Weight* w = weight_register.at(weight_t(SILENT));
//                     Edge* ne = new Edge(new_alphabet->at(symbol_id), w, current_state, state_map[nxt]);
//                     current_state->addSuccessor(ne);
//                     state_map[nxt]->addPredecessor(ne);

//                 } else {
//                     // Non-silent: invoke child
//                     const long long cid_ll = (long long)std::llround(pw.to_float());
//                     if (cid_ll < 0) continue;
//                     const size_t child_index = (size_t)cid_ll;
//                     if (child_index >= k) continue;

//                     const SetStd<weight_t>& guesses = child_return_values[child_index];
//                     const bool boundary = current.P2.empty();

//                     for (const weight_t& guess : guesses) {
//                         // Spawn a verification obligation for this call.
//                         // The child consumes the *current* symbol as its first symbol.
//                         const uint32_t verify_init = verify_sys.get_initial((uint32_t)child_index, guess);
//                         if (verify_init == VerificationDFA::INVALID) continue;

//                         const uint32_t verify_after_call = verify_sys.step(verify_init, symbol_id);
//                         if (verify_after_call == VerificationDFA::INVALID) continue;

//                         const bool immediate_accept = verify_sys.is_accepting(verify_after_call);
//                         const bool is_live = verify_sys.is_live(verify_after_call);
//                         if (!immediate_accept && !is_live) continue;  // Dead end -> reject this guess

//                         BuchiState_Lemma10 nxt;
//                         nxt.parent_state = q_prime;
//                         nxt.last_guess = guess;
//                         nxt.phase = phase_after_current;

//                         if (boundary) {
//                             nxt.P2 = P1_stepped;
//                             nxt.P1.clear();
//                             // Add to P1 only if not immediately discharged
//                             if (!immediate_accept && is_live) {
//                                 nxt.P1.add(verify_after_call);  // SET ADD - idempotent!
//                             }
//                         } else {
//                             nxt.P2 = P2_stepped;
//                             nxt.P1 = P1_stepped;
//                             if (!immediate_accept && is_live) {
//                                 nxt.P1.add(verify_after_call);  // SET ADD - idempotent!
//                             }
//                         }

//                         if (!state_map.contains(nxt)) {
//                             std::ostringstream s2;
//                             s2 << "b_" << state_counter++;
//                             State* ns = new State(s2.str(), new_alphabet->size(), global_min, global_max);
//                             state_map[nxt] = ns;
//                             worklist.push_back(nxt);
//                         }

//                             // before creating Edge*:
//                         const uint32_t sym = symbol_id;
//                         Weight* w = weight_register.at(guess);
//                         const uint32_t wid = w->getId();          // or your stable weight index
//                         const uint32_t dst = state_map[nxt]->getId();
//                         const uint32_t src = current_state->getId();
//                         if (!seen.insert(key(src, sym, wid, dst)).second) continue;

//                         Edge* ne = new Edge(new_alphabet->at(symbol_id), w, current_state, state_map[nxt]);
//                         current_state->addSuccessor(ne);
//                         state_map[nxt]->addPredecessor(ne);
//                     }
//                 }
//             }
//         }
//     }

//     // 7) Build automaton + set acceptance
//     MapArray<State*>* new_states = new MapArray<State*>(state_map.size());
//     for (const auto& [gs, st] : state_map) {
//         new_states->insert(st->getId(), st);
//         // Accepting: phase is WAIT_P2EMPTY and P2 is empty
//         if (gs.phase == ACC_WAIT_P2EMPTY && gs.P2.empty()) {
//             st->setFinal(true);
//         }
//     }

//     std::cout << "(maxP1, maxP2) = " << maxP1 << " " << maxP2 << std::endl;

//     std::string name = "BuchiLemma10(" + this->getName() + ")";
//     return new Automaton(name, new_alphabet, new_states, new_weights, global_min, global_max, init_state);
// }

// // =============================================================================
// // COMPLEXITY ANALYSIS
// // =============================================================================
// //
// // State space: |Q_m| × |weights| × 2^|Q_S| × 2^|Q_S| × 2
// //
// // For DETERMINISTIC children with bounded total size:
// //   - Each verification DFA S_{i,v} has O(|child_i| × bound_range) states
// //   - Total |Q_S| = O(sum_i |child_i| × bound_range × |return_values_i|) = O(1)
// //   - Therefore 2^|Q_S| = O(1)
// //   - Total state space = O(|Q_m|)
// //   - PTIME!
// //
// // For NONDETERMINISTIC children:
// //   - Verification DFAs may need subset construction (exponential)
// //   - |Q_S| can be exponential in child sizes
// //   - State space exponential in slave sizes, polynomial in |Q_m|
// //   - EXPSPACE (matches paper's Theorem 13)
// //
// // =============================================================================






























// -----------------------------------------------------------------------------
// OPTIMIZED FLATTENING: "tracked obligations" instead of monitor DFAs
// UNIFIED VERSION: handles both deterministic and nondeterministic children
// -----------------------------------------------------------------------------
weight_t transitionFunction(weight_t state_value,
                            weight_t transit_value,
                            value_function_t finVal,
                            weight_t bound);


// -----------------------------------------------------------------------------
// OBLIGATION-BASED FLATTENING (unified deterministic/nondeterministic)
// Supports finVal in {SumB, Max_f, Min_f}.
// -----------------------------------------------------------------------------

// ------------------------- configuration set -------------------------
struct ConfPair {
    uint32_t st;
    weight_t acc;

    bool operator==(const ConfPair& o) const { return st == o.st && acc == o.acc; }
    bool operator<(const ConfPair& o) const {
        if (st != o.st) return st < o.st;
        return acc < o.acc;
    }
};

using ConfSet = std::vector<ConfPair>; // always canonical: sorted, unique

static inline void conf_canonicalize(ConfSet& cs) {
    if (cs.empty()) return;
    std::sort(cs.begin(), cs.end());
    cs.erase(std::unique(cs.begin(), cs.end()), cs.end());
}

// ------------------------- obligations: SET semantics -------------------------
// Key = (child, guessed return value, child configuration-set)
// No multiplicities. Adding the same key twice is idempotent.

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

using OblBag = std::vector<OblEntry>; // canonical: sorted, unique

static inline void bag_canonicalize(OblBag& bag) {
    if (bag.empty()) return;
    std::sort(bag.begin(), bag.end());
    bag.erase(std::unique(bag.begin(), bag.end()), bag.end());
}

static inline void bag_add_one_sorted(OblBag& bag, OblEntry&& e) {
    auto it = std::lower_bound(bag.begin(), bag.end(), e);
    if (it != bag.end() && it->key == e.key) return; // idempotent add
    bag.insert(it, std::move(e));
}

// ------------------------- acceptance phase -------------------------
enum acc_phase_t : uint8_t { ACC_WAIT_MASTER = 0, ACC_WAIT_P2EMPTY = 1 };

// ------------------------- child tables (unified) ---------------
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

    inline uint32_t idx(uint32_t st, uint32_t a) const {
        return st * alph + a;
    }
};

static bool build_child_tables(ChildAutomaton* c, ChildTables& out) {
    if (!c) return false;
    out.child = c;
    out.n_states = (uint32_t)c->getStates()->size();
    out.alph     = (uint32_t)c->getAlphabet()->size();
    out.init     = (uint32_t)c->getInitial()->getId();

    out.is_final.assign(out.n_states, 0);
    for (uint32_t s = 0; s < out.n_states; ++s) {
        State* st = c->getStates()->at(s);
        out.is_final[s] = st->getFinal() ? 1 : 0;
    }

    const size_t cells = (size_t)out.n_states * (size_t)out.alph;
    out.off.assign(cells + 1, 0);

    // count edges per cell
    for (uint32_t s = 0; s < out.n_states; ++s) {
        State* from = c->getStates()->at(s);
        for (uint32_t a = 0; a < out.alph; ++a) {
            SetStd<Edge*>* succs = from->getSuccessors(a);
            out.off[(size_t)out.idx(s, a) + 1] = succs ? (uint32_t)succs->size() : 0;
        }
    }

    // prefix sum
    for (size_t i = 1; i < out.off.size(); ++i) out.off[i] += out.off[i - 1];
    out.edges.resize(out.off.back());

    // fill + predecessors for LIVE computation
    std::vector<uint32_t> cur = out.off;
    std::vector<std::vector<uint32_t>> pred(out.n_states);

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
                pred[t].push_back(s);
            }
        }
    }

    // LIVE = reverse BFS from finals (exists-path reachability)
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
        for (uint32_t u : pred[v]) {
            if (!out.live[u]) {
                out.live[u] = 1;
                q.push_back(u);
            }
        }
    }

    return true;
}

// --------------------- finite aggregator semantics -------------------
static inline weight_t acc0_for_finVal(value_function_t finVal) {
    if (finVal == Max_f) {
        return std::numeric_limits<float>::lowest();
    }
    else if (finVal == Min_f) {
        return std::numeric_limits<float>::infinity();
    }
    return weight_t(0);
}

// // Discharge test
// static inline bool discharge_ok_finite(value_function_t finVal,
//                                       const weight_t& acc,
//                                       const weight_t& guess,
//                                       const weight_t& bound) {
//     if (acc == guess) return true;

//     // Common saturation conventions
//     if (finVal == SumB) {
//         if (guess == bound) {
//             if (acc == bound + 1) return true;
//             if (acc == -bound - 1) return true;
//         }
//         return false;
//     }

//     if (finVal == Max_f) {
//         if (guess == bound && acc == bound + 1) return true;
//         return false;
//     }

//     if (finVal == Min_f) {
//         if (guess == -bound && acc == -bound - 1) return true;
//         return false;
//     }

//     return false;
// }
// Discharge test.
// Option B: For Max_f / Min_f there is NO saturation / bound-equivalence.
// Only SumB keeps the saturation convention.
static inline bool discharge_ok_finite(value_function_t finVal,
                                      const weight_t& acc,
                                      const weight_t& guess,
                                      const weight_t& bound) {
    if (acc == guess) return true;

    if (finVal == SumB) {
        if (guess == bound) {
            if (acc == bound + 1) return true;
            if (acc == -bound - 1) return true;
        }
        return false;
    }

    // Max_f / Min_f: no saturation
    return false;
}


// --------------------- target-aware liveness for obligations ------------------
// live(child, guess, st, acc) = can reach a matching final (for that guess)
// under the same "wrong-final branches are dropped" semantics as step/spawn.

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

            // --- 1) collect edge weights ---
            SetStd<weight_t> W;
            for (const auto& tr : T.edges) W.insert(tr.w);

            // --- 2) build finite accumulator domain by closure ---
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

            // materialize dom + index map
            acc_dom[i].reserve(dom.size());
            uint32_t idx = 0;
            for (const weight_t& a : dom) {
                acc_dom[i].push_back(a);
                acc_id[i].insert(a, idx++);
            }
            dom_sz[i] = (uint32_t)acc_dom[i].size();

            // --- 3) build reverse edges among NON-FINAL child states only (guess-independent) ---
            const uint32_t N = T.n_states;
            const uint32_t A = dom_sz[i];
            const uint32_t P = N * A;

            std::vector<std::vector<uint32_t>> rev(P);

            auto node_id = [&](uint32_t st, uint32_t aidx) -> uint32_t {
                return st * A + aidx;
            };

            // add reverse edges for transitions that stay in NON-FINAL states
            for (uint32_t st = 0; st < N; ++st) {
                if (T.is_final[st]) continue;
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

                            // we do NOT create product-nodes for final states (final = termination),
                            // and wrong-final edges are also "dropped" later via seeding only.
                            if (T.is_final[to]) continue;

                            const uint32_t aidx2 = acc_index(i, acc2);
                            if (aidx2 == UINT32_MAX) continue;

                            const uint32_t u = node_id(st, aidx);
                            const uint32_t v = node_id(to, aidx2);
                            rev[v].push_back(u);
                        }
                    }
                }
            }

            // --- 4) for each guess: seed nodes that can discharge in ONE step to a matching final,
            //         then reverse-BFS using rev[] ---
            for (const weight_t& guess : child_return_values[i]) {
                std::vector<uint8_t> live(P, 0);
                std::deque<uint32_t> qq;

                for (uint32_t st = 0; st < N; ++st) {
                    if (T.is_final[st]) continue;
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


// --------------------- stepping obligations (unified, SET) -------------------
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

                if (T.is_final[to]) {
                    if (discharge_ok_finite(finVal, acc2, ent.key.guess, bound)) {
                        discharged = true;
                        break; // existential: choose this branch and return now
                    }
                    continue; // wrong-return branch => drop
                }

                if (!live.is_live(i, ent.key.guess, to, acc2)) continue; // target-aware pruning
                next_conf.push_back(ConfPair{to, acc2});
            }

            if (discharged) break;
        }

        if (discharged) continue; // this obligation discharged

        conf_canonicalize(next_conf);
        if (next_conf.empty()) return false; // no branch can continue to a valid return

        // SET semantics: no multiplicities
        out.push_back(OblEntry{OblKey{i, ent.key.guess, std::move(next_conf)}});
    }

    bag_canonicalize(out);
    return true;
}


// --------------------- spawning obligations (unified, SET) -------------------
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

        if (T.is_final[to]) {
            if (discharge_ok_finite(finVal, acc2, guess, bound)) {
                return SpawnStatus::EMPTY; // choose this edge and terminate immediately
            }
            continue; // wrong-return branch => drop
        }

        if (!live.is_live(child_idx, guess, to, acc2)) continue;
        conf.push_back(ConfPair{to, acc2});
    }

    conf_canonicalize(conf);
    if (conf.empty()) return SpawnStatus::REJECT;

    // SET semantics: no multiplicities
    spawned = OblEntry{OblKey{child_idx, guess, std::move(conf)}};
    return SpawnStatus::NONEMPTY;
}

// ------------------------------ Buchi global state -------------------
struct BuchiState_obl {
    State* parent_state;
    weight_t last_guess;
    OblBag P1;
    OblBag P2;
    acc_phase_t phase;

    BuchiState_obl()
        : parent_state(nullptr), last_guess(0), P1(), P2(), phase(ACC_WAIT_MASTER) {}

    BuchiState_obl(State* p, weight_t g, const OblBag& p1, const OblBag& p2, acc_phase_t ph)
        : parent_state(p), last_guess(g), P1(p1), P2(p2), phase(ph) {}

    bool operator==(const BuchiState_obl& o) const {
        return parent_state == o.parent_state
            && last_guess == o.last_guess
            && P1 == o.P1
            && P2 == o.P2
            && phase == o.phase;
    }

    bool operator<(const BuchiState_obl& o) const {
        if (parent_state != o.parent_state) return parent_state < o.parent_state;
        if (last_guess   != o.last_guess)   return last_guess   < o.last_guess;
        if (P1 != o.P1) return P1 < o.P1;     // lexicographic
        if (P2 != o.P2) return P2 < o.P2;
        return phase < o.phase;
    }
};

static inline acc_phase_t advance_phase(const BuchiState_obl& s) {
    if (s.phase == ACC_WAIT_MASTER) {
        return (s.parent_state->getFinal()) ? ACC_WAIT_P2EMPTY : ACC_WAIT_MASTER;
    } else {
        return (s.P2.empty()) ? ACC_WAIT_MASTER : ACC_WAIT_P2EMPTY;
    }
}

// -----------------------------------------------------------------------------
// The unified flattening function (handles both deterministic and nondeterministic)
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// The unified flattening function (handles both deterministic and nondeterministic)
// INSTRUMENTED VERSION
// -----------------------------------------------------------------------------
Automaton* NestedAutomaton::flatten_regular_parent_acceptance_obligations(value_function_t finVal, weight_t bound) {
    if (!(finVal == SumB || finVal == Max_f || finVal == Min_f)) {
        QUAK_FAIL("flatten_regular_parent_acceptance_obligations: finVal must be SumB / Max_f / Min_f");
    }

    // --- INSTRUMENTATION START: Master Info ---
    std::cout << "master_states=" << this->getStates()->size() << std::endl;
    // --- INSTRUMENTATION END ---

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

    // Instrumentation vars
    size_t max_p1_size = 0;
    size_t max_p2_size = 0;

    // 1) Return values per child
    const size_t k = this->getChildrenSize();
    std::vector<SetStd<weight_t>> child_return_values(k);

    SetStd<weight_t> global_return_values;
    global_return_values.insert(weight_t(SILENT));

    for (size_t i = 0; i < k; ++i) {
        ChildAutomaton* child = this->getChild(i);
        if (!child) {
            // --- INSTRUMENTATION: Null child ---
            std::cout << "child[" << i << "] (null)" << std::endl;
            continue;
        }
        child_return_values[i] = this->computeChildReturnValuesParentAware(i, finVal, bound);
        for (const weight_t& v : child_return_values[i]) global_return_values.insert(v);

        // --- INSTRUMENTATION START: Child Info ---
        std::cout << "child[" << i << "] states=" << child->getStates()->size()
                  << " det=" << (child->isDeterministic() ? 1 : 0) // Assuming isDeterministic() exists
                  << " return_values={";
        bool first_v = true;
        for (const auto& val : child_return_values[i]) {
            if (!first_v) std::cout << ",";
            std::cout << val; // Assumes weight_t has operator<<
            first_v = false;
        }
        std::cout << "}" << std::endl;
        // --- INSTRUMENTATION END ---
    }

    bool have_non_silent = false;
    for (weight_t val : global_return_values) {
        if (val == weight_t(SILENT)) continue;
        if (!have_non_silent) { global_min = val; global_max = val; have_non_silent = true; }
        else { global_min = std::min(global_min, val); global_max = std::max(global_max, val); }
    }
    if (!have_non_silent) { global_min = weight_t(0); global_max = weight_t(0); }

    // 2) Build child tables (unified: handles both deterministic and nondeterministic)
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
    init.phase = ACC_WAIT_MASTER;

    std::ostringstream ss;
    ss << "b_" << state_counter++;
    State* init_state = new State(ss.str(), new_alphabet->size(), global_min, global_max);

    state_map[init] = init_state;
    worklist.push_back(init);

    // 5) BFS construction
    OblBag P1_step, P2_step;
    OblBag P1_next, P2_next;

    while (!worklist.empty()) {
        BuchiState_obl current = std::move(worklist.front());
        worklist.pop_front();

        // --- INSTRUMENTATION START: Max Bags ---
        // We track the maximum number of distinct obligations (bag size)
        if (current.P1.size() > max_p1_size) max_p1_size = current.P1.size();
        if (current.P2.size() > max_p2_size) max_p2_size = current.P2.size();
        // --- INSTRUMENTATION END ---

        State* current_state = state_map[current];
        const acc_phase_t phase_after_current = advance_phase(current);

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
                const bool is_silent = (pw == weight_t(0)); // keep your existing convention

                if (is_silent) {
                    if (current.P2.empty()) {
                        P1_next.clear();
                        P2_next = P1_step;
                    } else {
                        P1_next = P1_step;
                        P2_next = P2_step;
                    }

                    BuchiState_obl nxt(q_prime, weight_t(SILENT), P1_next, P2_next, phase_after_current);

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
                                P1_next.push_back(spawned);
                                bag_canonicalize(P1_next);
                            }
                        } else {
                            P2_next = P2_step;
                            P1_next = P1_step;
                            if (st == SpawnStatus::NONEMPTY) {
                                bag_add_one_sorted(P1_next, std::move(spawned));
                            }
                        }

                        BuchiState_obl nxt(q_prime, guess, P1_next, P2_next, phase_after_current);

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
        if (gs.phase == ACC_WAIT_P2EMPTY && gs.P2.empty()) {
            st->setFinal(true);
        }
    }

    // --- INSTRUMENTATION START: Final Stats ---
    // std::cout << "|Q_S|=" << state_map.size() << std::endl;
    std::cout << "(maxP1, maxP2) = (" << max_p1_size << ", " << max_p2_size << ")" << std::endl;
    // --- INSTRUMENTATION END ---

    std::string name = "BuchiObl(" + this->getName() + ")";
    return new Automaton(name, new_alphabet, new_states, new_weights, global_min, global_max, init_state);
}





















Automaton* NestedAutomaton::flatten_regular(value_function_t finVal, weight_t bound) {
    // if (this->allParentStatesFinal()) {
    //     return flatten_regular_parent_trivial(finVal, bound); // accept iff P2 empty
    // } else {
    //     return flatten_regular_parent_acceptance(finVal, bound); // track + accept in (track==1 && P2 empty)
    // }

    // return flatten_regular_parent_acceptance(finVal, bound); // track + accept in (track==1 && P2 empty)
    return flatten_regular_parent_acceptance_obligations(finVal, bound); // track + accept in (track==1 && P2 empty)
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

        // Build final states set
        State* child_new_init = child_states->at(child->getInitial()->getId());
        SetStd<State*>* child_new_finals = new SetStd<State*>();
        for (State* of : *(child->getFinalStates())) {
            child_new_finals->insert(child_states->at(of->getId()));
        }

        ChildAutomaton* new_child = new ChildAutomaton(
            child->getName(),
            child_alpha,
            child_states,
            child_weights,
            child->getMinDomain(),
            child->getMaxDomain(),
            child_new_init,
            child_new_finals
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

    // States stay the same         TODO: EXCEPT IF WE HAVE TO ADD A SINK
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

                State* from = states->at(from_src->getId());
                State* to   = states->at(to_src->getId());
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

            SetStd<State*>* copied_finals = new SetStd<State*>();
            for (State* of : *(child->getFinalStates())) {
                copied_finals->insert(children_states[i]->at(of->getId()));
            }

            ChildAutomaton* new_child = new ChildAutomaton(
                child->getName(),
                children_alphabet[i],
                children_states[i],
                children_weights[i],
                child->getMinDomain(),
                child->getMaxDomain(),
                new_init,
                copied_finals
            );

            new_children->insert(i, new_child);
        }
    }

    // Construct and return the new nested automaton
    NestedAutomaton* det_nwa = new NestedAutomaton("PsuedoDet(" + this->getName() + ")", new_alphabet, new_states, new_weights, 0, this->getChildrenSize() - 1, new_initial, new_children);
    // det_nwa->print();
    // NestedAutomaton* complete_det_nwa = det_nwa->makeCompleteNested();


    for (MacroSymbol* m : macro_alphabet) delete m;
    macro_alphabet.clear();

    // delete det_nwa;
    // return complete_det_nwa;

    return det_nwa;
}

/**
 * Synchronization + Ultimate slave construction.
 *
 * Output shape:
 *  - Exactly ONE child automaton: the synchronized ultimate slave U_sync.
 *  - The parent (master) is copied, but every transition weight is rewritten as:
 *      * -1  : silent transition (no spawn)
 *      * >=0 : ID of the initial state in U_sync from which the spawned instance starts
 *              (this ID already encodes the master-state-at-spawn in the synchronized product).
 *
 * Input convention (pre-sync):
 *  - Parent edge weight encodes called child ID:
 *      * <= 0 : silent (dummy/no call)
 *      * >  0 : calls that child ID
 *
 * Important:
 *  - Pseudodeterminization guarantees at most one transition per letter from each state
 *    in both master and all children.
 *  - This pass does NOT modify existing classes. It rewrites only by rebuilding objects.
 */
NestedAutomaton* NestedAutomaton::synchronizeChildren() {
    // =========================================================================
    // UTILITY LAMBDAS
    // =========================================================================

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
    auto old_master_called_child_id = [](const Edge* me) -> long long {
        if (!me || !me->getWeight()) return 0;
        // The project historically used to_float() for decoding.
        // We round to nearest integer to be robust to "1.0" style encodings.
        const double d = me->getWeight()->getValue().to_float();
        return static_cast<long long>(std::llround(d));
    };

    // =========================================================================
    // COMPUTE CONFIGURATION BOUND X = 2 * conf(this)
    // =========================================================================
    // uint64_t conf = 1;

    // const uint64_t nm = static_cast<uint64_t>(this->getStates()->size());
    // if (nm > 1) conf = sat_mul_u64(conf, nm);

    // for (size_t i = 0; i < this->getChildrenSize(); ++i) {
    //     ChildAutomaton* c = this->getChild(i);
    //     const uint64_t nc = static_cast<uint64_t>(c->getStates()->size());
    //     if (nc > 1) conf = sat_mul_u64(conf, nc);
    // }
    // const uint64_t X_u64 = sat_mul_u64(2, conf);

    // =========================================================================
    // COMPUTE CONFIGURATION BOUND X = 2 * conf(this)
    // conf(A) = |Q_m| * 2^|Q_slv|, where Q_slv is disjoint union of slave states
    // =========================================================================

    // Compute |Q_slv| = sum of states across all slave automata
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

    // =========================================================================
    // BASIC SIZES, ALPHABET SOURCE
    // =========================================================================
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

    // =========================================================================
    // BUILD THE SYNCHRONIZED ULTIMATE SLAVE U_sync (single child)
    // =========================================================================
    Symbol::RESET();
    State::RESET();
    Weight::RESET();

    // ---- Copy alphabet for U_sync ----
    MapArray<Symbol*>* calpha = new MapArray<Symbol*>(A);
    for (size_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(src_alpha->at(a));
        calpha->insert(s_new->getId(), s_new);
    }

    // ---- Compute GLOBAL weight statistics over all "real" children (id > 0) ----
    // We include all children indices > 0 because index 0 is commonly dummy/silent.
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

    // Output weights are either 0 (silent master step) or acc + child_weight (non-silent)
    const weight_t outMinBound = std::min(weight_t(0), accMin + minW);
    const weight_t outMaxBound = std::max(weight_t(0), accMax + maxW);

    // ---- Weight register for U_sync outputs ----
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
    // get_weight(weight_t(0)); // ensure 0 exists TODO: WHY

    // ---- State space of U_sync, explored on-demand ----
    struct SyncKey {
        uint32_t master_id;       // master state at the *same time step*
        uint32_t child_index;     // which original child component (ultimate-slave selector)
        uint32_t child_state_id;  // local state in that child
        weight_t accumulator;     // buffered sum waiting to flush
        bool pending_accept;      // child reached accept on silent master step, waiting to flush

        bool operator==(const SyncKey& o) const {
            return master_id == o.master_id
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

            mix64(static_cast<uint64_t>(k.master_id));
            mix64(static_cast<uint64_t>(k.child_index));
            mix64(static_cast<uint64_t>(k.child_state_id));
            mix64(static_cast<uint64_t>(std::hash<weight_t>{}(k.accumulator)));
            mix64(static_cast<uint64_t>(k.pending_accept ? 1 : 0));
            return h;
        }
    };

    std::unordered_map<SyncKey, State*, SyncKeyHash> state_map;
    std::queue<SyncKey> worklist;

    SetStd<State*>* cfinals = new SetStd<State*>();
    std::vector<State*> cstates_vec;

    auto child_is_final = [&](uint32_t child_index, uint32_t local_sid) -> bool {
        ChildAutomaton* c = this->getChild(child_index);
        if (!c) return false;
        State* s = c->getStates()->at(local_sid);
        return s && c->isFinal(s);
    };

    auto get_or_make_state = [&](const SyncKey& key) -> State* {
        auto it = state_map.find(key);
        if (it != state_map.end()) return it->second;

        std::ostringstream ss;
        ss << "U_sync_m" << key.master_id
           << "_c" << key.child_index
           << "_s" << key.child_state_id
           << "_a" << key.accumulator
           << (key.pending_accept ? "_P" : "");

        State* ns = new State(ss.str(), A, outMinBound, outMaxBound);
        cstates_vec.push_back(ns);

        // FINAL iff underlying local state is accepting AND we are not pending (flushed).
        if (child_is_final(key.child_index, key.child_state_id) && !key.pending_accept) {
            ns->setFinal(true);
            cfinals->insert(ns);
        }

        state_map.insert({key, ns});
        worklist.push(key);
        return ns;
    };

    // =========================================================================
    // Seed initial states of U_sync for every *calling* master transition.
    // Also compute the new parent payload table: -1 for silent, else init_state_id in U_sync.
    // =========================================================================
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

            const long long cid = old_master_called_child_id(me);

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

            // The spawn state must encode the master state at the call site (mid).
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

    // =========================================================================
    // BFS exploration of U_sync transitions
    // =========================================================================
    while (!worklist.empty()) {
        SyncKey cur = worklist.front();
        worklist.pop();

        // Terminality: final iff (accepting && !pending)
        if (child_is_final(cur.child_index, cur.child_state_id) && !cur.pending_accept) {
            continue;
        }

        State* cur_node = state_map.at(cur);

        State* m_state = this->getStates()->at(cur.master_id);
        ChildAutomaton* child = this->getChild(cur.child_index);
        if (!child) QUAK_FAIL("synchronizeChildren: null child during exploration");

        State* s_state = child->getStates()->at(cur.child_state_id);

        for (uint32_t a = 0; a < static_cast<uint32_t>(A); ++a) {
            Edge* master_edge = first_edge_or_null(m_state->getSuccessors(a));
            if (!master_edge) continue;

            const long long cid = old_master_called_child_id(master_edge);
            const bool master_is_silent = (cid <= 0);

            const uint32_t next_master_id = static_cast<uint32_t>(master_edge->getTo()->getId());

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

            if (master_is_silent) {
                // Emit 0, accumulate child_w, possibly set pending_accept
                const weight_t new_acc = cur.accumulator + child_w;

                if (new_acc < accMin || new_acc > accMax) {
                    // out of allowed accumulator range
                    continue;
                }

                bool new_pending = cur.pending_accept;
                if (!cur.pending_accept && child_edge && child->isFinal(child_edge->getTo())) {
                    new_pending = true;
                }

                SyncKey nxt{next_master_id, cur.child_index, next_child_state_id, new_acc, new_pending};
                State* nxt_node = get_or_make_state(nxt);

                Weight* w0 = get_weight(weight_t(0));
                Edge* e = new Edge(calpha->at(a), w0, cur_node, nxt_node);
                cur_node->addSuccessor(e);
                nxt_node->addPredecessor(e);
            } else {
                // Emit acc + child_w, reset accumulator, clear pending
                const weight_t emit = cur.accumulator + child_w;
                Weight* w_out = get_weight(emit);

                SyncKey nxt{next_master_id, cur.child_index, next_child_state_id, weight_t(0), false};
                State* nxt_node = get_or_make_state(nxt);

                Edge* e = new Edge(calpha->at(a), w_out, cur_node, nxt_node);
                cur_node->addSuccessor(e);
                nxt_node->addPredecessor(e);
            }
        }
    }

    // ---- finalize U_sync weights min/max ----
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
        nullptr,
        cfinals
    );

    MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(1);
    new_children->insert(0, ultimate_synced);

    // =========================================================================
    // REBUILD THE PARENT (MASTER) WITH NEW PAYLOADS
    // =========================================================================
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
    SetStd<State*>* mfinals = new SetStd<State*>();
    for (uint32_t sid = 0; sid < static_cast<uint32_t>(M); ++sid) {
        State* os = this->getStates()->at(sid);
        if (os->getFinal()) {
            State* ns = mstates->at(sid);
            ns->setFinal(true);
            mfinals->insert(ns);
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

    // =========================================================================
    // BUILD RESULT NESTED AUTOMATON
    // =========================================================================
    // NOTE: adapt the constructor call below to whatever "explicit components"
    // constructor your NestedAutomaton provides (alphabet, states, weights, initial, finals, children).
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



// -----------------------------------------------------------------------------
// TOCL definition helpers (Lemma 5.3 proof):
//
//   Q_slv  = disjoint union of the state sets of all slave automata of A
//   conf(A)= number of configurations (q_m, A) where q_m in Q_m and A ⊆ Q_slv
//
// Exact conf(A) (reachable configurations) is expensive to compute.
// This computes the standard TOCL upper bound:  conf(A) <= |Q_m| * 2^{|Q_slv|}.
//
// Then we set
//   N      = (|Q_slv| + 2) * conf(A) * |Q_slv|^{2*|Q_slv|}
//   c_bound= 2 * N
//
// Call this on the pseudodeterminized, pre-synchronization NWA (before U_sync blowup).
// -----------------------------------------------------------------------------
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

    // |Q_slv| = sum of sizes of slave state sets (disjoint union).
    // If child 0 is your dummy, skip it.
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
        return 0; // no real slaves, multiplicity bound is irrelevant
    }

    // TOCL upper bound on number of configurations:
    // conf(A) <= |Q_m| * 2^{|Q_slv|}
    const uint64_t conf_upper = sat_mul_u64(Qm, sat_pow2_u64(Qslv));

    // N = (|Q_slv| + 2) * conf(A) * |Q_slv|^{2|Q_slv|}
    const uint64_t pow_part = sat_pow_u64(Qslv, sat_mul_u64(2ULL, Qslv));
    uint64_t N = sat_mul_u64(Qslv + 2ULL, conf_upper);
    N = sat_mul_u64(N, pow_part);

    // c_bound = 2 * N
    return sat_mul_u64(2ULL, N);
}

// Typical usage in your pipeline:
//
// NestedAutomaton* A_det = pseudodeterminize(...);
// const uint64_t c_bound = compute_c_bound_TOCL_pre_sync_upper(A_det);
// NestedAutomaton* A_sync = A_det->synchronizeChildren();
// Automaton* flat = A_sync->flatten_TOCL_silLimAvg_powerset(c_bound);




// Assumptions (consistent with your synchronizeChildren output):
//  - this NWA is pseudodeterministic: <= 1 edge per letter from any state in master and in the unique child.
//  - children size == 1, child is the synchronized ultimate slave U_sync.
//  - master edge weight payload: <0 means SILENT, >=0 means spawn from that U_sync state-id.
//  - U_sync final states have no outgoing transitions (your BFS already enforced this).

Automaton* NestedAutomaton::flatten_Avg_SumMinus(uint64_t c_bound) {
    // ----------------------------- helpers -----------------------------
    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e;
        return nullptr;
    };

    auto master_payload_ll = [](const Edge* me) -> long long {
        if (!me || !me->getWeight()) return -1;
        const double d = me->getWeight()->getValue().to_float();
        return static_cast<long long>(std::llround(d));
    };

    // ----------------------------- sanity -----------------------------
    if (this->getChildrenSize() != 1) {
        QUAK_FAIL("flatten_Avg_SumMinus: expected exactly one child (ultimate synchronized slave)");
    }
    ChildAutomaton* U = this->getChild(0);
    if (!U) QUAK_FAIL("flatten_Avg_SumMinus: null ultimate slave");

    MapArray<State*>* mstates_src = this->getStates();
    const uint32_t M = static_cast<uint32_t>(mstates_src->size());

    MapArray<State*>* ustates_src = U->getStates();
    const uint32_t UQ = static_cast<uint32_t>(ustates_src->size());

    MapArray<Symbol*>* alpha_src = U->getAlphabet();
    if (!alpha_src) QUAK_FAIL("flatten_Avg_SumMinus: null alphabet in ultimate slave");
    const uint32_t A = static_cast<uint32_t>(alpha_src->size());

    // ----------------------------- precompute master step table -----------------------------
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
            mstep[ms][a].payload = master_payload_ll(e);
        }
    }

    // ----------------------------- precompute slave step table -----------------------------
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
        // In your construction, finals have no outgoing; we can skip precomputing.
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

    // ----------------------------- compute loose weight bounds for State ctor -----------------------------
    // Bounds are only used by your State(...) ctor; we keep them conservative.
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

    // Worst-case per-step sum ≤ (total active + 1 spawn) * maxAbs.
    weight_t maxAbs = (uMax < weight_t(0)) ? -uMin : std::max(uMax, (uMin < weight_t(0) ? -uMin : uMin));
    const double factor = static_cast<double>(c_bound) * static_cast<double>(UQ) + 1.0;
    weight_t outAbs = weight_t(factor) * maxAbs;

    weight_t outMinBound = (uMin >= weight_t(0)) ? weight_t(0) : -outAbs;
    weight_t outMaxBound = (uMax <= weight_t(0)) ? weight_t(0) : outAbs;

    // ----------------------------- build new alphabet -----------------------------
    Symbol::RESET();
    State::RESET();
    Weight::RESET();

    MapArray<Symbol*>* falpha = new MapArray<Symbol*>(A);
    for (uint32_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(alpha_src->at(a));
        falpha->insert(s_new->getId(), s_new);
    }

    // ----------------------------- weight register -----------------------------
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

    // ----------------------------- key type: (master_id, sparse multiset) -----------------------------
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

    // ----------------------------- state table + BFS -----------------------------
    std::unordered_map<Key, State*, KeyHash> stmap;
    std::queue<Key> work;

    std::vector<State*> fstates_vec;
    SetStd<State*>* ffinals = new SetStd<State*>();

    auto master_is_final = [&](uint32_t mid) -> bool {
        State* ms = mstates_src->at(mid);
        return ms && ms->getFinal();
    };

    auto get_or_make = [&](const Key& k) -> State* {
        auto it = stmap.find(k);
        if (it != stmap.end()) return it->second;

        State* ns = new State(encode_name(k), A, outMinBound, outMaxBound);
        fstates_vec.push_back(ns);

        if (master_is_final(k.mid)) {
            ns->setFinal(true);
            ffinals->insert(ns);
        }

        stmap.insert({k, ns});
        work.push(k);
        return ns;
    };

    // initial: (master_initial, empty multiset)
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

    // ----------------------------- BFS build edges -----------------------------
    while (!work.empty()) {
        Key cur = work.front();
        work.pop();

        State* from = stmap.at(cur);

        for (uint32_t a = 0; a < A; ++a) {
            const MStep& ms = mstep[cur.mid][a];
            if (!ms.exists) continue;

            const bool master_silent = (ms.payload < 0);
            const bool do_spawn = (!master_silent);
            const uint32_t spawn_sid = do_spawn ? static_cast<uint32_t>(ms.payload) : 0;

            weight_t sumw;
            std::vector<uint32_t> nz_next;

            if (!step_multiset(cur.nz, do_spawn, spawn_sid, a, sumw, nz_next)) {
                continue; // reject this letter from this state
            }

            if (master_silent && sumw != weight_t(0)) {
                QUAK_FAIL("U_sync violates synchronized silent transitions: nonzero sum on silent master step");
            }

            Key nxtK;
            nxtK.mid = ms.to;
            nxtK.nz = std::move(nz_next);

            State* to = get_or_make(nxtK);

            Weight* ew = master_silent ? w_silent : get_w(sumw);

            Edge* e = new Edge(falpha->at(a), ew, from, to);
            from->addSuccessor(e);
            to->addPredecessor(e);
        }
    }

    // ----------------------------- finalize arrays and build Automaton -----------------------------
    // weights min/max (ignore SILENT if you want; here we include it for bookkeeping)
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

    // IMPORTANT: adapt this constructor call to your Automaton API.
    // Many parts of your code use a similar shape as ChildAutomaton(...).
    Automaton* flat = new Automaton(
        "Flat(" + this->getName() + ")",
        falpha, fstates, fweights,
        fMin, fMax,
        finitial
    );

    return flat;
}






// // assuming the input NWA is pseudo-deterministic and children are synchronized
// Automaton* NestedAutomaton::flatten_Avg_SumMinus() {
//     using std::size_t;

//     // ---------- helper: single outgoing edge after determinization ----------
//     auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
//         if (!succs) return nullptr;
//         for (Edge* e : *succs) return e; // at most one after determinization
//         return nullptr;
//     };

//     // ---------- weight helpers ----------
//     auto weight_zero = []() { return weight_t(0); };
//     auto weight_two  = []() { return weight_t(2); };
//     auto weight_abs = [&](weight_t v) -> weight_t {
//         // Sum- ⇒ weights are non-positive, but we still define |v|
//         if (v < weight_zero()) return -v;
//         return v;
//     };

//     // ---------- cache children ----------
//     const size_t C = this->getChildrenSize();
//     std::vector<ChildAutomaton*> children(C, nullptr);
//     for (size_t ci = 0; ci < C; ++ci) {
//         children[ci] = this->getChild(ci);
//     }

//     // ---------- master metadata ----------
//     MapArray<State*>* mstates = this->getStates();
//     MapArray<Symbol*>* malpha = this->getAlphabet();
//     const size_t M_states = mstates->size();
//     const size_t A        = malpha->size();

//     // precompute master successor table [master_state_id][symbol_id]
//     std::vector<std::vector<Edge*>> masterSucc(M_states, std::vector<Edge*>(A, nullptr));
//     for (size_t sid = 0; sid < M_states; ++sid) {
//         State* s = mstates->at(sid);
//         if (!s) continue;
//         // after construction, ids are dense; assert to be safe
//         assert(s->getId() == static_cast<int>(sid));
//         for (size_t a = 0; a < A; ++a) {
//             masterSucc[sid][a] = first_edge_or_null(s->getSuccessors(a));
//         }
//     }

//     // ---------- child metadata ----------
//     struct ChildInfo {
//         ChildAutomaton* aut = nullptr;
//         std::vector<std::vector<Edge*>> succ;   // [state_id][symbol_id]
//         std::vector<bool>               is_final; // [state_id]
//         std::vector<Edge*>              initSucc; // [symbol_id] from initial state
//         size_t                          num_states = 0;
//     };

//     std::vector<ChildInfo> cinfo(C);

//     // global stats: total #child states U, max absolute weight W_abs
//     size_t   U      = 0;
//     weight_t W_abs  = weight_zero();

//     for (size_t ci = 0; ci < C; ++ci) {
//         ChildAutomaton* child = children[ci];
//         cinfo[ci].aut = child;
//         if (!child) continue;

//         MapArray<State*>*   cstates  = child->getStates();
//         MapArray<Weight*>*  cweights = child->getWeights();
//         const size_t        S        = cstates->size();
//         cinfo[ci].num_states = S;
//         U += S;

//         // compute |w| max over this child
//         for (size_t wid = 0; wid < cweights->size(); ++wid) {
//             Weight* w = cweights->at(wid);
//             if (!w) continue;
//             weight_t mag = weight_abs(w->getValue());
//             if (mag > W_abs) W_abs = mag;
//         }

//         // allocate tables
//         cinfo[ci].succ.assign(S, std::vector<Edge*>(A, nullptr));
//         cinfo[ci].is_final.assign(S, false);
//         cinfo[ci].initSucc.assign(A, nullptr);

//         // fill succ and final flags
//         for (size_t sid = 0; sid < S; ++sid) {
//             State* s = cstates->at(sid);
//             if (!s) continue;
//             assert(s->getId() == static_cast<int>(sid));
//             cinfo[ci].is_final[sid] = child->isFinal(s);

//             if (cinfo[ci].is_final[sid]) {
//                 // final states are terminal after determinization
//                 continue;
//             }
//             for (size_t a = 0; a < A; ++a) {
//                 cinfo[ci].succ[sid][a] = first_edge_or_null(s->getSuccessors(a));
//             }
//         }

//         // fill initSucc from child's initial state
//         State* s0 = child->getInitial();
//         if (s0) {
//             const size_t init_id = static_cast<size_t>(s0->getId());
//             assert(init_id < S);
//             for (size_t a = 0; a < A; ++a) {
//                 cinfo[ci].initSucc[a] = cinfo[ci].succ[init_id][a];
//             }
//         }
//     }

//     // ---------- X, Y, Z bounds (combinatorial) ----------
//     auto sat_mul = [](size_t a, size_t b) -> size_t {
//         if (a == 0 || b == 0) return 0;
//         const size_t maxv = std::numeric_limits<size_t>::max();
//         if (a > maxv / b) return maxv;
//         return a * b;
//     };

//     auto sat_pow = [&](size_t base, size_t exp) -> size_t {
//         if (exp == 0) return 1;
//         const size_t maxv = std::numeric_limits<size_t>::max();
//         size_t res = 1;
//         while (exp > 0) {
//             if (base != 0 && res > maxv / base) return maxv;
//             res *= base;
//             if (res == maxv) return maxv;
//             --exp;
//         }
//         return res;
//     };

//     // X = 2 * |M| * Π_i |S_i|
//     size_t X_states = 2;
//     X_states = sat_mul(X_states, M_states);
//     for (size_t ci = 0; ci < C; ++ci) {
//         if (!children[ci]) continue;
//         X_states = sat_mul(X_states, cinfo[ci].num_states);
//     }

//     // Y = X * (|U| + 2) * |U|^{2|U|}
//     size_t exp   = sat_mul(2, U);          // 2|U|
//     size_t U_pow = sat_pow(U, exp);        // |U|^{2|U|}

//     size_t Y = sat_mul(X_states, U + 2);
//     Y = sat_mul(Y, U_pow);

//     // Z = 2 * X * (|U| + 2) * |U|^{2|U|} * W_abs  (all in weight_t)
//     weight_t Z = weight_zero();
//     if (W_abs > weight_zero()) {
//         weight_t WX = weight_two() * weight_t(X_states);
//         WX = WX * weight_t(U + 2);
//         WX = WX * weight_t(U_pow);
//         Z  = WX * W_abs;
//     }

//     // ---------- flattened alphabet: copy from NWA ----------
//     Symbol::RESET();
//     MapArray<Symbol*>* falpha = new MapArray<Symbol*>(A);
//     for (size_t a = 0; a < A; ++a) {
//         Symbol* s_new = new Symbol(malpha->at(a)->getName());
//         falpha->insert(s_new->getId(), s_new); // ids are 0..A-1 after RESET
//     }

//     // ---------- flattened states & weights ----------
//     State::RESET();
//     Weight::RESET();

//     std::vector<State*>  fstates_vec;
//     std::vector<Weight*> fweights_vec;
//     fstates_vec.reserve(64);
//     fweights_vec.reserve(16);

//     MapStd<weight_t, Weight*> weight_register;

//     weight_t flat_min       = weight_zero();
//     weight_t flat_max       = weight_zero();
//     bool     flat_has_weight = false;

//     auto get_weight = [&](weight_t v) -> Weight* {
//         if (weight_register.contains(v) != true) {
//             Weight* w = new Weight(v);
//             fweights_vec.push_back(w);
//             weight_register.insert(v, w);

//             if (!flat_has_weight) {
//                 flat_min = flat_max = v;
//                 flat_has_weight = true;
//             } else {
//                 if (v < flat_min) flat_min = v;
//                 if (v > flat_max) flat_max = v;
//             }
//         }
//         return weight_register.at(v);
//     };

//     // ---------- flatten state encoding ----------
//     struct BoundedInst {
//         size_t   child_index;
//         State*   state;
//         weight_t budget;  // remaining |weight|-budget ∈ [0, Z]
//     };
//     using UInst = std::pair<size_t, State*>; // (child_index, child_state)

//     struct FlatKey {
//         State*                   master;
//         std::vector<UInst>       unbounded;
//         std::vector<BoundedInst> bounded;

//         bool operator==(const FlatKey& o) const {
//             if (master != o.master) return false;
//             if (unbounded.size() != o.unbounded.size()) return false;
//             if (bounded.size()   != o.bounded.size())   return false;

//             for (size_t i = 0; i < unbounded.size(); ++i) {
//                 if (unbounded[i].first  != o.unbounded[i].first)  return false;
//                 if (unbounded[i].second != o.unbounded[i].second) return false;
//             }
//             for (size_t i = 0; i < bounded.size(); ++i) {
//                 const BoundedInst& b1 = bounded[i];
//                 const BoundedInst& b2 = o.bounded[i];
//                 if (b1.child_index != b2.child_index) return false;
//                 if (b1.state       != b2.state)       return false;
//                 if (b1.budget      != b2.budget)      return false;
//             }
//             return true;
//         }
//     };

//     struct FlatKeyHash {
//         size_t operator()(FlatKey const& k) const {
//             size_t h = 1469598103934665603ull;
//             auto mix = [&](uint64_t x) {
//                 h ^= x;
//                 h *= 1099511628211ull;
//             };
//             mix(reinterpret_cast<uint64_t>(k.master));
//             for (auto const& u : k.unbounded) {
//                 mix(static_cast<uint64_t>(u.first));
//                 mix(reinterpret_cast<uint64_t>(u.second));
//             }
//             for (auto const& b : k.bounded) {
//                 mix(static_cast<uint64_t>(b.child_index));
//                 mix(reinterpret_cast<uint64_t>(b.state));
//                 // we deliberately ignore budget in the hash to avoid depending on hash<weight_t>
//             }
//             return h;
//         }
//     };

//     auto normalize_key = [](FlatKey& k) {
//         auto cmpU = [](const UInst& a, const UInst& b) {
//             if (a.first != b.first) return a.first < b.first;
//             return a.second->getId() < b.second->getId();
//         };
//         std::sort(k.unbounded.begin(), k.unbounded.end(), cmpU);

//         auto cmpB = [](const BoundedInst& a, const BoundedInst& b) {
//             if (a.child_index != b.child_index) return a.child_index < b.child_index;
//             int ida = a.state->getId();
//             int idb = b.state->getId();
//             if (ida != idb) return ida < idb;
//             if (a.budget < b.budget) return true;
//             if (a.budget > b.budget) return false;
//             return false;
//         };
//         std::sort(k.bounded.begin(), k.bounded.end(), cmpB);
//     };

//     std::unordered_map<FlatKey, State*, FlatKeyHash> state_map;
//     std::queue<FlatKey> worklist;

//     auto get_or_make_state = [&](FlatKey key) -> State* {
//         normalize_key(key);
//         auto it = state_map.find(key);
//         if (it != state_map.end()) return it->second;

//         std::ostringstream ss;
//         ss << "flat_" << state_map.size();
//         State* ns = new State(ss.str(), A, 0, 0); // no children: domain [0,0]
//         fstates_vec.push_back(ns);

//         state_map.insert(std::make_pair(key, ns));
//         worklist.push(key);
//         return ns;
//     };

//     // scratch buffers reused across transitions (to reduce allocations)
//     std::vector<UInst>       scratch_unbounded;
//     std::vector<BoundedInst> scratch_bounded;

//     // ---------- initial flat state: (master_initial, no slaves) ----------
//     FlatKey initKey;
//     initKey.master = this->getInitial();
//     State* flat_initial = get_or_make_state(initKey);

//     // ---------- BFS over flattened state space ----------
//     while (!worklist.empty()) {
//         FlatKey key = worklist.front();
//         worklist.pop();

//         State* from_flat = state_map.at(key);

//         for (size_t a = 0; a < A; ++a) {
//             Edge* me = masterSucc[key.master->getId()][a];
//             if (!me) continue;

//             State*   m2 = me->getTo();
//             weight_t wm = me->getWeight()->getValue(); // ≤ 0 under Sum-

//             bool ok = true;
//             scratch_unbounded.clear();
//             scratch_bounded.clear();

//             weight_t sum_unbounded = weight_zero();
//             weight_t sum_bounded   = weight_zero();

//             // --- existing unbounded instances ---
//             for (const UInst& u : key.unbounded) {
//                 size_t  ci    = u.first;
//                 State*  s_cur = u.second;

//                 ChildInfo& ch = cinfo[ci];
//                 if (!ch.aut) { ok = false; break; }

//                 const size_t sid = static_cast<size_t>(s_cur->getId());
//                 if (sid >= ch.succ.size()) { ok = false; break; }

//                 Edge* se = ch.succ[sid][a];
//                 if (!se) { ok = false; break; }

//                 State*   s2 = se->getTo();
//                 weight_t xu = se->getWeight()->getValue(); // ≤ 0

//                 sum_unbounded += xu;
//                 scratch_unbounded.emplace_back(ci, s2);
//             }
//             if (!ok) continue;

//             // --- existing bounded instances ---
//             for (const BoundedInst& b : key.bounded) {
//                 size_t   ci    = b.child_index;
//                 State*   s_cur = b.state;
//                 weight_t bud   = b.budget;

//                 ChildInfo& ch = cinfo[ci];
//                 if (!ch.aut) { ok = false; break; }

//                 const size_t sid = static_cast<size_t>(s_cur->getId());
//                 if (sid >= ch.succ.size()) { ok = false; break; }

//                 Edge* se = ch.succ[sid][a];
//                 if (!se) { ok = false; break; }

//                 State*   s2    = se->getTo();
//                 weight_t z     = se->getWeight()->getValue(); // ≤ 0
//                 weight_t mag_z = weight_abs(z);

//                 // strictly decreasing absolute budget
//                 if (bud < mag_z) { ok = false; break; }
//                 weight_t bud2 = bud - mag_z;

//                 sum_bounded += z;

//                 // if budget exhausted and child is final, drop this instance
//                 if (bud2 == weight_zero()) {
//                     const size_t s2id = static_cast<size_t>(s2->getId());
//                     if (s2id < ch.is_final.size() && ch.is_final[s2id]) {
//                         continue;
//                     }
//                 }

//                 scratch_bounded.push_back(BoundedInst{ci, s2, bud2});
//             }
//             if (!ok) continue;

//             // total contribution of master + active slaves (before spawning)
//             weight_t base_weight = wm + sum_unbounded + sum_bounded;

//             // -------- (i) no new instantiation --------
//             {
//                 FlatKey k2;
//                 k2.master    = m2;
//                 k2.unbounded = scratch_unbounded;
//                 k2.bounded   = scratch_bounded;

//                 State* to_flat = get_or_make_state(k2);
//                 Edge* e = new Edge(falpha->at(a), get_weight(base_weight), from_flat, to_flat);
//                 from_flat->addSuccessor(e);
//                 to_flat->addPredecessor(e);
//             }

//             // -------- (ii) spawn unbounded instance --------
//             if (Y > 0 && scratch_unbounded.size() < Y && U > 0) {
//                 for (size_t ci = 0; ci < C; ++ci) {
//                     ChildInfo& ch = cinfo[ci];
//                     if (!ch.aut) continue;

//                     Edge* se0 = ch.initSucc[a];
//                     if (!se0) continue;

//                     State*   s1    = se0->getTo();
//                     weight_t x_new = se0->getWeight()->getValue(); // ≤ 0

//                     FlatKey k2;
//                     k2.master    = m2;
//                     k2.unbounded = scratch_unbounded;
//                     k2.bounded   = scratch_bounded;
//                     k2.unbounded.emplace_back(ci, s1);

//                     State* to_flat = get_or_make_state(k2);
//                     weight_t x = base_weight + x_new;

//                     Edge* e = new Edge(falpha->at(a), get_weight(x), from_flat, to_flat);
//                     from_flat->addSuccessor(e);
//                     to_flat->addPredecessor(e);
//                 }
//             }

//             // -------- (iii) spawn bounded instance --------
//             if (U > 0 && Z > weight_zero() && scratch_bounded.size() < U) {
//                 for (size_t ci = 0; ci < C; ++ci) {
//                     ChildInfo& ch = cinfo[ci];
//                     if (!ch.aut) continue;

//                     Edge* se0 = ch.initSucc[a];
//                     if (!se0) continue;

//                     State*   s1    = se0->getTo();
//                     weight_t z     = se0->getWeight()->getValue(); // ≤ 0
//                     weight_t mag_z = weight_abs(z);
//                     if (mag_z > Z) continue;

//                     weight_t bud2 = Z - mag_z;

//                     FlatKey k2;
//                     k2.master    = m2;
//                     k2.unbounded = scratch_unbounded;
//                     k2.bounded   = scratch_bounded;
//                     k2.bounded.push_back(BoundedInst{ci, s1, bud2});

//                     State* to_flat = get_or_make_state(k2);
//                     weight_t x = base_weight + z; // ≤ 0

//                     Edge* e = new Edge(falpha->at(a), get_weight(x), from_flat, to_flat);
//                     from_flat->addSuccessor(e);
//                     to_flat->addPredecessor(e);
//                 }
//             }
//         }
//     }

//     // ---------- materialize states / weights ----------
//     const size_t state_count  = fstates_vec.size();
//     const size_t weight_count = fweights_vec.size();

//     MapArray<State*>*  fstates  = new MapArray<State*>(state_count);
//     MapArray<Weight*>* fweights = new MapArray<Weight*>(weight_count);

//     for (State* s : fstates_vec)   fstates->insert(s->getId(), s);
//     for (Weight* w : fweights_vec) fweights->insert(w->getId(), w);

//     if (!flat_has_weight) {
//         flat_min = flat_max = weight_zero();
//     }

//     // ---------- select final states (logic only, still not wired into Automaton) ----------
//     SetStd<State*>* flat_finals = new SetStd<State*>();
//     for (const auto& kv : state_map) {
//         const FlatKey& k = kv.first;
//         State*         s = kv.second;
//         if (k.unbounded.empty() && k.bounded.empty()) {
//             flat_finals->insert(s);
//         }
//     }
//     (void)flat_finals; // kept for future use

//     // ---------- build and return a childless Automaton ----------
//     std::string fname = "Flat(" + this->getName() + ")";
//     Automaton* flatNA = new Automaton(
//         fname,
//         falpha,
//         fstates,
//         fweights,
//         flat_min,
//         flat_max,
//         flat_initial
//     );

//     return flatNA;
// }


/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////


// typedef uint64_t internal_weight_t;

// inline internal_weight_t to_internal(weight_t w) {
//     return (internal_weight_t)w.to_float();
// }

// typedef struct global_exploration_data_supremum {
//     // constraints
//     NestedAutomaton* A;
//     Parser* parser;
//     internal_weight_t abs_threshold;
//     unsigned int* cumulative_size;   // prefix sums for flattening (child_id, local_state) -> global index
//     unsigned int track_them_all;     // bitmask with all child-state bits set
//     unsigned int children_all;       // total flattened child-state count

//     // given (input for current exploration frame)
//     std::string global_from;         // encoded as "master_id/activation/tracking/[token_info|@inactive@]"
//     unsigned int master_state_id_from;
//     internal_weight_t budget_from;   // budget of the single tracked token (only valid if !inactive_from)
//     unsigned int child_state_id_from;
//     unsigned int child_id_from;
//     bool inactive_from;              // true = no token currently being tracked for weight
//     internal_weight_t global_tracking_from;   // per-child-state tracking bits
//     internal_weight_t global_activation_from; // per-child-state activation bits (token present)

//     // initialized per-symbol
//     Symbol* symbol;
//     std::vector<unsigned int> old_activation_of_children_state;
//     std::vector<unsigned int> old_tracking_of_children_state;
//     std::vector<unsigned int> new_activation_of_children_state;  // must restore on backtrack
//     std::vector<unsigned int> new_tracking_of_children_state;    // must restore on backtrack

//     // computed (output)
//     unsigned int master_state_id_to;
//     internal_weight_t global_edge_weight;
//     internal_weight_t budget_to;
//     unsigned int child_state_id_to;
//     unsigned int child_id_to;
//     bool inactive_to;
// } data_supremum_t;

// void explore_global_initialization_supremum (data_supremum_t* data);

// void explore_global_failure_supremum (data_supremum_t* data) {
//     data->parser->edges.insert({
//         { data->symbol->getName(), weight_t(0) },
//         { data->global_from, "@sink@" }
//     });
// }

// void explore_global_finalization_supremum (data_supremum_t* data, Symbol* symbol) {
//     internal_weight_t global_activation_to = 0;
//     internal_weight_t global_tracking_to = 0;
//     for (unsigned int i = 0; i < data->children_all; i++) {
//         global_activation_to = (global_activation_to << 1)
//                              + data->new_activation_of_children_state[i];

//         global_tracking_to = (global_tracking_to << 1)
//                            + data->new_tracking_of_children_state[i];
//     }

//     bool global_final = false;
//     // Epoch boundary: tracking_from==0 means all obligations discharged
//     if (data->global_tracking_from == 0) {
//         global_tracking_to = data->track_them_all;  // reset for next epoch
//         // global_final = true;
//         // TODO: CHECK
//         global_final = data->A->getStates()->at(data->master_state_id_to)->getFinal();
//     }

//     // State encodes: master_id/activation/tracking/[child_id/child_state/budget | @inactive@]
//     std::string global_to;
//     global_to.reserve(64);
//     global_to.append(std::to_string(data->master_state_id_to));
//     global_to.push_back('/');
//     global_to.append(std::to_string(global_activation_to));
//     global_to.push_back('/');
//     global_to.append(std::to_string(global_tracking_to));

//     if (data->inactive_to) {
//         global_to.append("/@inactive@");
//     } else {
//         global_to.push_back('/');
//         global_to.append(std::to_string(data->child_id_to));
//         global_to.push_back('/');
//         global_to.append(std::to_string(data->child_state_id_to));
//         global_to.push_back('/');
//         global_to.append(std::to_string(data->budget_to));
//     }

//     data->parser->edges.insert({
//         { symbol->getName(), (weight_t)data->global_edge_weight },
//         { data->global_from, global_to }
//     });

//     if (global_final == true) {
//         data->parser->final_states.insert(global_to);
//     }

//     // DFS: only recurse on newly discovered states
//     if (data->parser->states.contains(global_to) == false) {
//         data->parser->states.insert(global_to);

//         data_supremum_t data_deeper{};
//         data_deeper.A             = data->A;
//         data_deeper.parser        = data->parser;
//         data_deeper.abs_threshold = data->abs_threshold;
//         data_deeper.cumulative_size = data->cumulative_size;
//         data_deeper.track_them_all  = data->track_them_all;
//         data_deeper.children_all    = data->children_all;

//         data_deeper.global_from          = global_to;
//         data_deeper.master_state_id_from = data->master_state_id_to;
//         data_deeper.inactive_from        = data->inactive_to;
//         data_deeper.global_tracking_from = global_tracking_to;
//         data_deeper.global_activation_from = global_activation_to;

//         if (data->inactive_to == false) {
//             data_deeper.budget_from         = data->budget_to;
//             data_deeper.child_state_id_from = data->child_state_id_to;
//             data_deeper.child_id_from       = data->child_id_to;
//         }

//         explore_global_initialization_supremum(&data_deeper);
//     }
// }

// // Propagate background children (all except the single tracked token)
// void explore_global_selection_supremum (unsigned int child_id, unsigned int child_state_id, data_supremum_t* data) {
//     // Skip the tracked token -- handled separately in explore_global_child_transition_supremum
//     if (child_id == data->child_id_from && child_state_id == data->child_state_id_from) {
//         explore_global_selection_supremum(child_id, child_state_id + 1, data);
//     }
//     else if (child_id < data->A->getChildrenSize()) {
//         auto* child  = data->A->getChild(child_id);
//         auto* states = child->getStates();

//         if (child_state_id < states->size()) {
//             unsigned int i = data->cumulative_size[child_id] + child_state_id;

//             if (data->old_activation_of_children_state[i] == 0) {
//                 explore_global_selection_supremum(child_id, child_state_id + 1, data);
//             }
//             else if (states->at(child_state_id)->getFinal()) {
//                 // Final states implicitly terminate -- no successor propagation needed
//                 explore_global_selection_supremum(child_id, child_state_id + 1, data);
//             }
//             else {
//                 State* child_state = states->at(child_state_id);

//                 auto* succs = child_state->getSuccessors(data->symbol->getId());
//                 if (succs) {
//                     for (Edge* edge : *succs) {
//                         unsigned int ii = data->cumulative_size[child_id] + edge->getTo()->getId();
//                         unsigned int stored_tracking   = data->new_tracking_of_children_state[ii];
//                         unsigned int stored_activation = data->new_activation_of_children_state[ii];

//                         // Propagate tracking/activation to successor
//                         if (data->old_tracking_of_children_state[i] == 1) {
//                             data->new_tracking_of_children_state[ii] = 1;
//                         }
//                         if (data->old_activation_of_children_state[i] == 1) {
//                             data->new_activation_of_children_state[ii] = 1;
//                         }

//                         explore_global_selection_supremum(child_id + 1, child_state_id, data);

//                         data->new_tracking_of_children_state[ii]   = stored_tracking;
//                         data->new_activation_of_children_state[ii] = stored_activation;
//                     }
//                 } else {
//                     explore_global_selection_supremum(child_id + 1, child_state_id, data);
//                 }
//             }
//         }
//         else {
//             explore_global_selection_supremum(child_id + 1, 0, data);
//         }
//     }
//     else {
//         explore_global_finalization_supremum(data, data->symbol);
//     }
// }

// // Handle the single tracked token's transition (the one accumulating weight)
// void explore_global_child_transition_supremum (data_supremum_t* data) {
//     if (data->inactive_from == true) {
//         // No token being tracked -- just propagate background children
//         data->inactive_to = true;
//         explore_global_selection_supremum(0, 0, data);
//         return;
//     }

//     State* child_state = data->A->getChild(data->child_id_from)->getStates()->at(data->child_state_id_from);

//     if (child_state->getFinal() == true) {
//         // Termination: budget must be exactly exhausted (validates the guess)
//         if (data->budget_from == 0) {
//             data->inactive_to = true;
//             explore_global_selection_supremum(0, 0, data);
//         } else {
//             explore_global_failure_supremum(data);
//         }
//         return;
//     }

//     unsigned int i = data->cumulative_size[data->child_id_from] + data->child_state_id_from;

//     if (child_state->getSuccessors(data->symbol->getId())) {
//         for (Edge* child_edge : *child_state->getSuccessors(data->symbol->getId())) {
//             unsigned int ii = data->cumulative_size[data->child_id_from] + child_edge->getTo()->getId();
//             unsigned int stored_tracking = data->new_tracking_of_children_state[ii];
//             unsigned int stored_activation = data->new_activation_of_children_state[ii];

//             if (data->old_tracking_of_children_state[i] == 1) {
//                 data->new_tracking_of_children_state[ii] = 1;
//             }
//             if (data->old_activation_of_children_state[i] == 1) {
//                 data->new_activation_of_children_state[ii] = 1;
//             }

//             data->inactive_to = false;
//             data->child_id_to = data->child_id_from;
//             data->child_state_id_to = child_edge->getTo()->getId();

//             internal_weight_t abs_child_edge_value;
//             if (child_edge->getWeight()->getValue() < 0) {
//                 abs_child_edge_value = to_internal(-(child_edge->getWeight()->getValue()));
//             } else {
//                 abs_child_edge_value = to_internal(child_edge->getWeight()->getValue());
//             }

//             if (data->budget_from < data->abs_threshold) {
//                 // Fixed budget mode: deterministic subtraction
//                 if (data->budget_from < abs_child_edge_value) {
//                     explore_global_failure_supremum(data);
//                 } else {
//                     data->budget_to = data->budget_from - abs_child_edge_value;
//                     explore_global_selection_supremum(0, 0, data);
//                 }
//             } else {
//                 // Unlimited budget (abs_threshold): nondeterministically guess successor budget
//                 // Any guess where guess + edge_cost >= threshold is valid (could still reach threshold)
//                 for (internal_weight_t abs_weight = data->abs_threshold; ; abs_weight--) {
//                     if (abs_weight + abs_child_edge_value >= data->abs_threshold) {
//                         data->budget_to = abs_weight;
//                         explore_global_selection_supremum(0, 0, data);
//                     }
//                     if (abs_weight == 0) break;
//                 }
//             }

//             data->new_tracking_of_children_state[ii] = stored_tracking;
//             data->new_activation_of_children_state[ii] = stored_activation;
//         }
//     }
// }

// void explore_global_master_transition_supremum (data_supremum_t* data) {
//     auto succs = data->A->getStates()->at(data->master_state_id_from)->getSuccessors(data->symbol->getId());
//     if (!succs) return;

//     // Save tracked token context -- each master edge explores independently
//     unsigned int       saved_child_state_id_from = data->child_state_id_from;
//     unsigned int       saved_child_id_from       = data->child_id_from;
//     internal_weight_t  saved_budget_from         = data->budget_from;
//     bool               saved_inactive_from       = data->inactive_from;

//     for (Edge* master_edge : *succs) {
//         data->child_state_id_from = saved_child_state_id_from;
//         data->child_id_from       = saved_child_id_from;
//         data->budget_from         = saved_budget_from;
//         data->inactive_from       = saved_inactive_from;

//         data->master_state_id_to = static_cast<unsigned int>(master_edge->getTo()->getId());
//         unsigned int child_id = static_cast<unsigned int>(master_edge->getWeight()->getValue().to_float());

//         if (data->A->getChild(child_id)->getStates()->size() == 1) {
//             // Silent transition: no spawn, weight 0
//             data->global_edge_weight = 0;
//             explore_global_child_transition_supremum(data);
//         } else {
//             unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
//             unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

//             // Mark spawned child as active in OLD (so selection sees it)
//             unsigned prev = data->old_activation_of_children_state[ii];
//             data->old_activation_of_children_state[ii] = 1;

//             // Choice 1: spawn as background (not tracked for weight)
//             data->global_edge_weight = 0;
//             explore_global_child_transition_supremum(data);

//             // Choice 2: start tracking for weight (only if no token currently tracked)
//             if (saved_inactive_from) {
//                 data->global_edge_weight  = 1;  // signal that we're betting on this token
//                 data->child_state_id_from = summoned_child_state_id;
//                 data->child_id_from       = child_id;
//                 data->budget_from         = data->abs_threshold;  // start with "unlimited"
//                 data->inactive_from       = false;
//                 explore_global_child_transition_supremum(data);
//             }

//             data->old_activation_of_children_state[ii] = prev;
//         }
//     }
// }

// void explore_global_initialization_supremum (data_supremum_t* data) {
//     internal_weight_t activation_from = data->global_activation_from;
//     internal_weight_t tracking_from   = data->global_tracking_from;

//     const unsigned int n = data->children_all;

//     data->new_activation_of_children_state.assign(n, 0);
//     data->new_tracking_of_children_state.assign(n, 0);

//     data->old_activation_of_children_state.resize(n);
//     data->old_tracking_of_children_state.resize(n);

//     // Unpack bitmasks
//     for (unsigned int i = 0; i < n; i++) {
//         data->old_activation_of_children_state[i] = static_cast<unsigned int>(activation_from & 1);
//         activation_from >>= 1;

//         data->old_tracking_of_children_state[i] = static_cast<unsigned int>(tracking_from & 1);
//         tracking_from >>= 1;
//     }

//     if (data->A->getStates()->at(data->master_state_id_from)->getAlphabet()) {
//         for (Symbol* symbol : *(data->A->getStates()->at(data->master_state_id_from)->getAlphabet())) {
//             data->symbol = symbol;
//             explore_global_master_transition_supremum(data);
//         }
//     }
// }

// // Track ONE child token explicitly for weight, others as background
// bool NestedAutomaton::emptiness_monotonic_nesting_supremum(value_function_t infinite_aggregator,
//                                                            value_function_t finite_aggregator,
//                                                            weight_t threshold) {
//     if (threshold <= 0 && finite_aggregator == SumPlus) {
//         return true;
//     }
//     if (threshold > 0 && finite_aggregator == SumMinus) {
//         return false;
//     }

//     internal_weight_t abs_threshold;
//     if (threshold > 0) {
//         abs_threshold = to_internal(threshold);
//     } else {
//         abs_threshold = to_internal(-threshold + 1);
//     }

//     std::vector<unsigned int> cumulative_size(this->children_->size() + 1);
//     cumulative_size[0] = 0;
//     for (unsigned int i = 1; i < this->children_->size() + 1; i++) {
//         cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
//     }
//     unsigned int children_all = cumulative_size[this->children_->size()];

//     // All bits set for epoch reset
//     unsigned int track_them_all = 0;
//     for (unsigned int i = 0; i < children_all; i++) {
//         track_them_all = track_them_all * 2 + 1;
//     }

//     Parser* parser = new Parser(0, 1);
//     parser->weights.insert(0);
//     parser->weights.insert(1);
//     parser->states.insert("@sink@");

//     for (Symbol* symbol : *this->getAlphabet()) {
//         parser->alphabet.insert(symbol->getName());
//         std::pair<std::pair<std::string, weight_t>, std::pair<std::string, std::string>> sink_self_loop;
//         sink_self_loop.first.first = symbol->getName();
//         sink_self_loop.first.second = 0;
//         sink_self_loop.second.first = "@sink@";
//         sink_self_loop.second.second = "@sink@";
//         parser->edges.insert(sink_self_loop);
//     }

//     // Initial: no children active, none tracked, no token being followed for weight
//     std::string global_initial = "";
//     global_initial = global_initial + std::to_string(this->initial->getId());
//     global_initial = global_initial + "/" + std::to_string(0);  // activation=0
//     global_initial = global_initial + "/" + std::to_string(0);  // tracking=0
//     global_initial = global_initial + "/" + "@inactive@";
//     parser->states.insert(global_initial);
//     parser->initial = global_initial;

//     data_supremum_t* data = new data_supremum_t();
//     if (data == nullptr) QUAK_FAIL("out of memory");

//     data->A = this;
//     data->parser = parser;
//     data->abs_threshold = abs_threshold;
//     data->cumulative_size = cumulative_size.data();
//     data->track_them_all = track_them_all;
//     data->children_all = children_all;
//     data->global_from = global_initial;
//     data->master_state_id_from = this->initial->getId();
//     data->inactive_from = true;
//     data->global_tracking_from = 0;
//     data->global_activation_from = 0;

//     explore_global_initialization_supremum(data);
//     delete data;

//     std::string newname = "unnested(" + this->getName() + ")";
//     MapStd<std::string, Symbol*> sync_register;
//     Automaton* unnested = new Automaton(newname, parser, sync_register);
//     // unnested->print();
//     std::cout << "Unnested: " << parser->states.size() << " states, " << parser->edges.size() << " edges" << std::endl;
//     delete parser;

//     weight_t top = unnested->compute_top_with_final(infinite_aggregator);
//     bool result = (top >= 1);

//     delete unnested;
//     return result;
// }






// typedef uint64_t internal_weight_t;

// inline internal_weight_t to_internal(weight_t w) {
//     return (internal_weight_t)w.to_float();
// }

// struct pending_state_t {
//     std::string global_from;
//     unsigned int master_state_id_from;
//     internal_weight_t budget_from;
//     unsigned int child_state_id_from;
//     unsigned int child_id_from;
//     bool inactive_from;
//     internal_weight_t global_tracking_from;
//     internal_weight_t global_activation_from;
// };

// typedef struct global_exploration_data_supremum {
//     // constraints
//     NestedAutomaton* A;
//     Parser* parser;
//     internal_weight_t abs_threshold;
//     unsigned int* cumulative_size;   // prefix sums for flattening (child_id, local_state) -> global index
//     unsigned int track_them_all;     // bitmask with all child-state bits set
//     unsigned int children_all;       // total flattened child-state count

//     // pending states for iterative exploration
//     std::vector<pending_state_t>* pending_states;

//     // given (input for current exploration frame)
//     std::string global_from;         // encoded as "master_id/activation/tracking/[token_info|@inactive@]"
//     unsigned int master_state_id_from;
//     internal_weight_t budget_from;   // budget of the single tracked token (only valid if !inactive_from)
//     unsigned int child_state_id_from;
//     unsigned int child_id_from;
//     bool inactive_from;              // true = no token currently being tracked for weight
//     internal_weight_t global_tracking_from;   // per-child-state tracking bits
//     internal_weight_t global_activation_from; // per-child-state activation bits (token present)

//     // initialized per-symbol
//     Symbol* symbol;
//     std::vector<unsigned int> old_activation_of_children_state;
//     std::vector<unsigned int> old_tracking_of_children_state;
//     std::vector<unsigned int> new_activation_of_children_state;  // must restore on backtrack
//     std::vector<unsigned int> new_tracking_of_children_state;    // must restore on backtrack

//     // computed (output)
//     unsigned int master_state_id_to;
//     internal_weight_t global_edge_weight;
//     internal_weight_t budget_to;
//     unsigned int child_state_id_to;
//     unsigned int child_id_to;
//     bool inactive_to;
// } data_supremum_t;

// void explore_global_initialization_supremum (data_supremum_t* data);

// void explore_global_failure_supremum (data_supremum_t* data) {
//     data->parser->edges.insert({
//         { data->symbol->getName(), weight_t(0) },
//         { data->global_from, "@sink@" }
//     });
// }

// void explore_global_finalization_supremum (data_supremum_t* data, Symbol* symbol) {
//     internal_weight_t global_activation_to = 0;
//     internal_weight_t global_tracking_to = 0;
//     for (unsigned int i = 0; i < data->children_all; i++) {
//         global_activation_to = (global_activation_to << 1)
//                              + data->new_activation_of_children_state[i];

//         global_tracking_to = (global_tracking_to << 1)
//                            + data->new_tracking_of_children_state[i];
//     }

//     bool global_final = false;
//     // Epoch boundary: tracking_from==0 means all obligations discharged
//     if (data->global_tracking_from == 0) {
//         global_tracking_to = data->track_them_all;  // reset for next epoch
//         // global_final = true;
//         // TODO: CHECK
//         global_final = data->A->getStates()->at(data->master_state_id_to)->getFinal();
//     }

//     // State encodes: master_id/activation/tracking/[child_id/child_state/budget | @inactive@]
//     std::string global_to;
//     global_to.reserve(64);
//     global_to.append(std::to_string(data->master_state_id_to));
//     global_to.push_back('/');
//     global_to.append(std::to_string(global_activation_to));
//     global_to.push_back('/');
//     global_to.append(std::to_string(global_tracking_to));

//     if (data->inactive_to) {
//         global_to.append("/@inactive@");
//     } else {
//         global_to.push_back('/');
//         global_to.append(std::to_string(data->child_id_to));
//         global_to.push_back('/');
//         global_to.append(std::to_string(data->child_state_id_to));
//         global_to.push_back('/');
//         global_to.append(std::to_string(data->budget_to));
//     }

//     data->parser->edges.insert({
//         { symbol->getName(), (weight_t)data->global_edge_weight },
//         { data->global_from, global_to }
//     });

//     if (global_final == true) {
//         data->parser->final_states.insert(global_to);
//     }

//     // Iterative: push newly discovered states onto the work stack
//     if (data->parser->states.contains(global_to) == false) {
//         data->parser->states.insert(global_to);

//         pending_state_t ps;
//         ps.global_from          = global_to;
//         ps.master_state_id_from = data->master_state_id_to;
//         ps.inactive_from        = data->inactive_to;
//         ps.global_tracking_from = global_tracking_to;
//         ps.global_activation_from = global_activation_to;

//         if (data->inactive_to == false) {
//             ps.budget_from         = data->budget_to;
//             ps.child_state_id_from = data->child_state_id_to;
//             ps.child_id_from       = data->child_id_to;
//         }

//         data->pending_states->push_back(ps);
//     }
// }

// // Propagate background children (all except the single tracked token)
// void explore_global_selection_supremum (unsigned int child_id, unsigned int child_state_id, data_supremum_t* data) {
//     // Skip the tracked token -- handled separately in explore_global_child_transition_supremum
//     if (child_id == data->child_id_from && child_state_id == data->child_state_id_from) {
//         explore_global_selection_supremum(child_id, child_state_id + 1, data);
//     }
//     else if (child_id < data->A->getChildrenSize()) {
//         auto* child  = data->A->getChild(child_id);
//         auto* states = child->getStates();

//         if (child_state_id < states->size()) {
//             unsigned int i = data->cumulative_size[child_id] + child_state_id;

//             if (data->old_activation_of_children_state[i] == 0) {
//                 explore_global_selection_supremum(child_id, child_state_id + 1, data);
//             }
//             else if (states->at(child_state_id)->getFinal()) {
//                 // Final states implicitly terminate -- no successor propagation needed
//                 explore_global_selection_supremum(child_id, child_state_id + 1, data);
//             }
//             else {
//                 State* child_state = states->at(child_state_id);

//                 auto* succs = child_state->getSuccessors(data->symbol->getId());
//                 if (succs) {
//                     for (Edge* edge : *succs) {
//                         unsigned int ii = data->cumulative_size[child_id] + edge->getTo()->getId();
//                         unsigned int stored_tracking   = data->new_tracking_of_children_state[ii];
//                         unsigned int stored_activation = data->new_activation_of_children_state[ii];

//                         // Propagate tracking/activation to successor
//                         if (data->old_tracking_of_children_state[i] == 1) {
//                             data->new_tracking_of_children_state[ii] = 1;
//                         }
//                         if (data->old_activation_of_children_state[i] == 1) {
//                             data->new_activation_of_children_state[ii] = 1;
//                         }

//                         explore_global_selection_supremum(child_id + 1, child_state_id, data);

//                         data->new_tracking_of_children_state[ii]   = stored_tracking;
//                         data->new_activation_of_children_state[ii] = stored_activation;
//                     }
//                 } else {
//                     explore_global_selection_supremum(child_id + 1, child_state_id, data);
//                 }
//             }
//         }
//         else {
//             explore_global_selection_supremum(child_id + 1, 0, data);
//         }
//     }
//     else {
//         explore_global_finalization_supremum(data, data->symbol);
//     }
// }

// // Handle the single tracked token's transition (the one accumulating weight)
// void explore_global_child_transition_supremum (data_supremum_t* data) {
//     if (data->inactive_from == true) {
//         // No token being tracked -- just propagate background children
//         data->inactive_to = true;
//         explore_global_selection_supremum(0, 0, data);
//         return;
//     }

//     State* child_state = data->A->getChild(data->child_id_from)->getStates()->at(data->child_state_id_from);

//     if (child_state->getFinal() == true) {
//         // Termination: budget must be exactly exhausted (validates the guess)
//         if (data->budget_from == 0) {
//             data->inactive_to = true;
//             explore_global_selection_supremum(0, 0, data);
//         } else {
//             explore_global_failure_supremum(data);
//         }
//         return;
//     }

//     unsigned int i = data->cumulative_size[data->child_id_from] + data->child_state_id_from;

//     if (child_state->getSuccessors(data->symbol->getId())) {
//         for (Edge* child_edge : *child_state->getSuccessors(data->symbol->getId())) {
//             unsigned int ii = data->cumulative_size[data->child_id_from] + child_edge->getTo()->getId();
//             unsigned int stored_tracking = data->new_tracking_of_children_state[ii];
//             unsigned int stored_activation = data->new_activation_of_children_state[ii];

//             if (data->old_tracking_of_children_state[i] == 1) {
//                 data->new_tracking_of_children_state[ii] = 1;
//             }
//             if (data->old_activation_of_children_state[i] == 1) {
//                 data->new_activation_of_children_state[ii] = 1;
//             }

//             data->inactive_to = false;
//             data->child_id_to = data->child_id_from;
//             data->child_state_id_to = child_edge->getTo()->getId();

//             internal_weight_t abs_child_edge_value;
//             if (child_edge->getWeight()->getValue() < 0) {
//                 abs_child_edge_value = to_internal(-(child_edge->getWeight()->getValue()));
//             } else {
//                 abs_child_edge_value = to_internal(child_edge->getWeight()->getValue());
//             }

//             if (data->budget_from < data->abs_threshold) {
//                 // Fixed budget mode: deterministic subtraction
//                 if (data->budget_from < abs_child_edge_value) {
//                     explore_global_failure_supremum(data);
//                 } else {
//                     data->budget_to = data->budget_from - abs_child_edge_value;
//                     explore_global_selection_supremum(0, 0, data);
//                 }
//             } else {
//                 // Unlimited budget (abs_threshold): nondeterministically guess successor budget
//                 // Any guess where guess + edge_cost >= threshold is valid (could still reach threshold)
//                 for (internal_weight_t abs_weight = data->abs_threshold; ; abs_weight--) {
//                     if (abs_weight + abs_child_edge_value >= data->abs_threshold) {
//                         data->budget_to = abs_weight;
//                         explore_global_selection_supremum(0, 0, data);
//                     }
//                     if (abs_weight == 0) break;
//                 }
//             }

//             data->new_tracking_of_children_state[ii] = stored_tracking;
//             data->new_activation_of_children_state[ii] = stored_activation;
//         }
//     }
// }

// void explore_global_master_transition_supremum (data_supremum_t* data) {
//     auto succs = data->A->getStates()->at(data->master_state_id_from)->getSuccessors(data->symbol->getId());
//     if (!succs) return;

//     // Save tracked token context -- each master edge explores independently
//     unsigned int       saved_child_state_id_from = data->child_state_id_from;
//     unsigned int       saved_child_id_from       = data->child_id_from;
//     internal_weight_t  saved_budget_from         = data->budget_from;
//     bool               saved_inactive_from       = data->inactive_from;

//     for (Edge* master_edge : *succs) {
//         data->child_state_id_from = saved_child_state_id_from;
//         data->child_id_from       = saved_child_id_from;
//         data->budget_from         = saved_budget_from;
//         data->inactive_from       = saved_inactive_from;

//         data->master_state_id_to = static_cast<unsigned int>(master_edge->getTo()->getId());
//         unsigned int child_id = static_cast<unsigned int>(master_edge->getWeight()->getValue().to_float());

//         if (data->A->getChild(child_id)->getStates()->size() == 1) {
//             // Silent transition: no spawn, weight 0
//             data->global_edge_weight = 0;
//             explore_global_child_transition_supremum(data);
//         } else {
//             unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
//             unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

//             // Mark spawned child as active in OLD (so selection sees it)
//             unsigned prev = data->old_activation_of_children_state[ii];
//             data->old_activation_of_children_state[ii] = 1;

//             // Choice 1: spawn as background (not tracked for weight)
//             data->global_edge_weight = 0;
//             explore_global_child_transition_supremum(data);

//             // Choice 2: start tracking for weight (only if no token currently tracked)
//             if (saved_inactive_from) {
//                 data->global_edge_weight  = 1;  // signal that we're betting on this token
//                 data->child_state_id_from = summoned_child_state_id;
//                 data->child_id_from       = child_id;
//                 data->budget_from         = data->abs_threshold;  // start with "unlimited"
//                 data->inactive_from       = false;
//                 explore_global_child_transition_supremum(data);
//             }

//             data->old_activation_of_children_state[ii] = prev;
//         }
//     }
// }

// void explore_global_initialization_supremum (data_supremum_t* data) {
//     internal_weight_t activation_from = data->global_activation_from;
//     internal_weight_t tracking_from   = data->global_tracking_from;

//     const unsigned int n = data->children_all;

//     data->new_activation_of_children_state.assign(n, 0);
//     data->new_tracking_of_children_state.assign(n, 0);

//     data->old_activation_of_children_state.resize(n);
//     data->old_tracking_of_children_state.resize(n);

//     // Unpack bitmasks
//     for (unsigned int i = 0; i < n; i++) {
//         data->old_activation_of_children_state[i] = static_cast<unsigned int>(activation_from & 1);
//         activation_from >>= 1;

//         data->old_tracking_of_children_state[i] = static_cast<unsigned int>(tracking_from & 1);
//         tracking_from >>= 1;
//     }

//     if (data->A->getStates()->at(data->master_state_id_from)->getAlphabet()) {
//         for (Symbol* symbol : *(data->A->getStates()->at(data->master_state_id_from)->getAlphabet())) {
//             data->symbol = symbol;
//             explore_global_master_transition_supremum(data);
//         }
//     }
// }

// // Track ONE child token explicitly for weight, others as background
// bool NestedAutomaton::emptiness_monotonic_nesting_supremum(value_function_t infinite_aggregator,
//                                                            value_function_t finite_aggregator,
//                                                            weight_t threshold) {
//     if (threshold <= 0 && finite_aggregator == SumPlus) {
//         return true;
//     }
//     if (threshold > 0 && finite_aggregator == SumMinus) {
//         return false;
//     }

//     internal_weight_t abs_threshold;
//     if (threshold > 0) {
//         abs_threshold = to_internal(threshold);
//     } else {
//         abs_threshold = to_internal(-threshold + 1);
//     }

//     std::vector<unsigned int> cumulative_size(this->children_->size() + 1);
//     cumulative_size[0] = 0;
//     for (unsigned int i = 1; i < this->children_->size() + 1; i++) {
//         cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
//     }
//     unsigned int children_all = cumulative_size[this->children_->size()];

//     // All bits set for epoch reset
//     unsigned int track_them_all = 0;
//     for (unsigned int i = 0; i < children_all; i++) {
//         track_them_all = track_them_all * 2 + 1;
//     }

//     Parser* parser = new Parser(0, 1);
//     parser->weights.insert(0);
//     parser->weights.insert(1);
//     parser->states.insert("@sink@");

//     for (Symbol* symbol : *this->getAlphabet()) {
//         parser->alphabet.insert(symbol->getName());
//         std::pair<std::pair<std::string, weight_t>, std::pair<std::string, std::string>> sink_self_loop;
//         sink_self_loop.first.first = symbol->getName();
//         sink_self_loop.first.second = 0;
//         sink_self_loop.second.first = "@sink@";
//         sink_self_loop.second.second = "@sink@";
//         parser->edges.insert(sink_self_loop);
//     }

//     // Initial: no children active, none tracked, no token being followed for weight
//     std::string global_initial = "";
//     global_initial = global_initial + std::to_string(this->initial->getId());
//     global_initial = global_initial + "/" + std::to_string(0);  // activation=0
//     global_initial = global_initial + "/" + std::to_string(0);  // tracking=0
//     global_initial = global_initial + "/" + "@inactive@";
//     parser->states.insert(global_initial);
//     parser->initial = global_initial;

//     data_supremum_t* data = new data_supremum_t();
//     if (data == nullptr) QUAK_FAIL("out of memory");

//     data->A = this;
//     data->parser = parser;
//     data->abs_threshold = abs_threshold;
//     data->cumulative_size = cumulative_size.data();
//     data->track_them_all = track_them_all;
//     data->children_all = children_all;

//     // Iterative DFS using explicit stack
//     std::vector<pending_state_t> pending_states;
//     pending_states.reserve(1024);  // avoid early reallocations

//     pending_state_t initial_ps;
//     initial_ps.global_from          = global_initial;
//     initial_ps.master_state_id_from = this->initial->getId();
//     initial_ps.inactive_from        = true;
//     initial_ps.global_tracking_from = 0;
//     initial_ps.global_activation_from = 0;
//     initial_ps.budget_from          = 0;
//     initial_ps.child_state_id_from  = 0;
//     initial_ps.child_id_from        = 0;
//     pending_states.push_back(initial_ps);

//     data->pending_states = &pending_states;

//     while (!pending_states.empty()) {
//         pending_state_t ps = pending_states.back();
//         pending_states.pop_back();

//         data->global_from          = ps.global_from;
//         data->master_state_id_from = ps.master_state_id_from;
//         data->inactive_from        = ps.inactive_from;
//         data->global_tracking_from = ps.global_tracking_from;
//         data->global_activation_from = ps.global_activation_from;

//         if (ps.inactive_from == false) {
//             data->budget_from         = ps.budget_from;
//             data->child_state_id_from = ps.child_state_id_from;
//             data->child_id_from       = ps.child_id_from;
//         }

//         explore_global_initialization_supremum(data);
//     }

//     delete data;

//     std::string newname = "unnested(" + this->getName() + ")";
//     MapStd<std::string, Symbol*> sync_register;
//     Automaton* unnested = new Automaton(newname, parser, sync_register);
//     // unnested->print();
//     // std::cout << "Unnested: " << parser->states.size() << " states, " << parser->edges.size() << " edges" << std::endl;
//     delete parser;

//     weight_t top = unnested->compute_top_with_final(infinite_aggregator);
//     bool result = (top >= 1);

//     delete unnested;
//     return result;
// }



// ==============================
// Sup/LimSup + SumPlus/SumMinus
// ==============================

typedef uint64_t internal_weight_t;

inline internal_weight_t to_internal(weight_t w) {
    return (internal_weight_t)w.to_float();
}

static inline bool tracking_all_zero(const std::vector<unsigned char>& v) {
    for (unsigned char b : v) {
        if (b != 0) return false;
    }
    return true;
}

static inline std::string bits_to_string(const std::vector<unsigned char>& v) {
    std::string s;
    s.reserve(v.size());
    for (unsigned char b : v) s.push_back(b ? '1' : '0');
    return s;
}

struct pending_state_t {
    std::string global_from;
    unsigned int master_state_id_from;

    // Track ONE distinguished token (promise/budget), others as background
    internal_weight_t budget_from;
    unsigned int child_state_id_from;
    unsigned int child_id_from;
    bool inactive_from;

    // Background bookkeeping (no 64-bit limitation)
    std::vector<unsigned char> tracking_from;   // per-flattened-child-state bits
    std::vector<unsigned char> activation_from; // per-flattened-child-state bits
};

typedef struct global_exploration_data_supremum {
    // constraints
    NestedAutomaton* A = nullptr;
    Parser* parser = nullptr;
    internal_weight_t abs_threshold{};
    unsigned int* cumulative_size = nullptr; // prefix sums for flattening (child_id, local_state) -> global index
    unsigned int children_all = 0;

    // epoch reset vector (all ones)
    std::vector<unsigned char> track_them_all;

    // pending states for iterative exploration
    std::vector<pending_state_t>* pending_states = nullptr;

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int master_state_id_from = 0;

    internal_weight_t budget_from = 0; // only valid if !inactive_from
    unsigned int child_state_id_from = 0;
    unsigned int child_id_from = 0;
    bool inactive_from = true;

    std::vector<unsigned char> tracking_from;
    std::vector<unsigned char> activation_from;

    // initialized per-symbol
    Symbol* symbol = nullptr;

    // OLD arrays: copied from activation_from/tracking_from (the "from" state)
    std::vector<unsigned char> old_activation;
    std::vector<unsigned char> old_tracking;

    // NEW arrays: built during exploration for this symbol; must restore on backtrack
    std::vector<unsigned char> new_activation;
    std::vector<unsigned char> new_tracking;

    // computed (output)
    unsigned int master_state_id_to = 0;
    internal_weight_t global_edge_weight = 0; // 0/1 edge weight in flattened automaton

    internal_weight_t budget_to = 0;
    unsigned int child_state_id_to = 0;
    unsigned int child_id_to = 0;
    bool inactive_to = true;
} data_supremum_t;

static void explore_global_initialization_supremum(data_supremum_t* data);
static void explore_global_master_transition_supremum(data_supremum_t* data);
static void explore_global_child_transition_supremum(data_supremum_t* data);
static void explore_global_selection_supremum(unsigned int child_id, unsigned int child_state_id, data_supremum_t* data);
static void explore_global_finalization_supremum(data_supremum_t* data);
static void explore_global_failure_supremum(data_supremum_t* data);

static void explore_global_failure_supremum(data_supremum_t* data) {
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

// Finalize one global transition and enqueue destination if new
static void explore_global_finalization_supremum(data_supremum_t* data) {
    // Compute destination activation/tracking vectors (copies)
    std::vector<unsigned char> activation_to = data->new_activation;
    std::vector<unsigned char> tracking_to   = data->new_tracking;

    bool global_final = false;
    // Epoch boundary: tracking_from==all-zero means all obligations discharged
    if (tracking_all_zero(data->tracking_from)) {
        tracking_to  = data->track_them_all; // reset obligations
        global_final = data->A->getStates()->at(data->master_state_id_to)->getFinal();
    }


    // Encode destination state:
    //   master_id/activation_bits/tracking_bits/[child_id/child_state/budget | @inactive@]
    std::string global_to;
    global_to.reserve(64 + data->children_all * 2);
    global_to.append(std::to_string(data->master_state_id_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(activation_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(tracking_to));

    if (data->inactive_to) {
        global_to.append("/@inactive@");
    } else {
        global_to.push_back('/');
        global_to.append(std::to_string(data->child_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->child_state_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->budget_to));
    }

    // Add edge
    data->parser->edges.insert({
        { data->symbol->getName(), (weight_t)data->global_edge_weight },
        { data->global_from, global_to }
    });

    if (global_final) {
        data->parser->final_states.insert(global_to);
    }

    // Enqueue newly discovered states
    if (!data->parser->states.contains(global_to)) {
        data->parser->states.insert(global_to);

        pending_state_t ps;
        ps.global_from          = global_to;
        ps.master_state_id_from = data->master_state_id_to;
        ps.inactive_from        = data->inactive_to;
        ps.activation_from      = std::move(activation_to);
        ps.tracking_from        = std::move(tracking_to);

        if (!data->inactive_to) {
            ps.budget_from         = data->budget_to;
            ps.child_state_id_from = data->child_state_id_to;
            ps.child_id_from       = data->child_id_to;
        } else {
            ps.budget_from         = 0;
            ps.child_state_id_from = 0;
            ps.child_id_from       = 0;
        }

        data->pending_states->push_back(std::move(ps));
    }
}

// Propagate background children (all except the single tracked token)
static void explore_global_selection_supremum(unsigned int child_id,
                                             unsigned int child_state_id,
                                             data_supremum_t* data) {
    // Skip the tracked token's FROM location (handled in explore_global_child_transition_supremum)
    if (!data->inactive_from &&
        child_id == data->child_id_from &&
        child_state_id == data->child_state_id_from) {
        explore_global_selection_supremum(child_id, child_state_id + 1, data);
        return;
    }

    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            // Not active: keep scanning within this child
            if (data->old_activation[i] == 0) {
                explore_global_selection_supremum(child_id, child_state_id + 1, data);
                return;
            }

            // Active but final: treat as already terminated (no propagation)
            if (states->at(child_state_id)->getFinal()) {
                explore_global_selection_supremum(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());

            if (succs) {
                for (Edge* edge : *succs) {
                    // Terminate in the same symbol: do NOT propagate to successor if successor is final
                    if (edge->getTo()->getFinal()) {
                        explore_global_selection_supremum(child_id + 1, 0, data); // FIX (1)
                        continue;
                    }

                    const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();
                    const unsigned char stored_tracking   = data->new_tracking[ii];
                    const unsigned char stored_activation = data->new_activation[ii];

                    if (data->old_tracking[i] == 1) {
                        data->new_tracking[ii] = 1;
                    }
                    if (data->old_activation[i] == 1) {
                        data->new_activation[ii] = 1;
                    }

                    explore_global_selection_supremum(child_id + 1, 0, data); // FIX (1)

                    data->new_tracking[ii]   = stored_tracking;
                    data->new_activation[ii] = stored_activation;
                }
            } else {
                // No successors on this symbol: keep behavior as in your code (treat as "no move" for background)
                explore_global_selection_supremum(child_id + 1, 0, data); // FIX (1)
            }
        } else {
            // Done scanning this child: move to next child, reset local index
            explore_global_selection_supremum(child_id + 1, 0, data);
        }
    } else {
        // All children processed: emit transition
        explore_global_finalization_supremum(data);
    }
}

// Handle the single tracked token's transition (the one accumulating weight)
static void explore_global_child_transition_supremum(data_supremum_t* data) {
    if (data->inactive_from) {
        data->inactive_to = true;
        explore_global_selection_supremum(0, 0, data);
        return;
    }

    ChildAutomaton* child = data->A->getChild(data->child_id_from);
    State* child_state = child->getStates()->at(data->child_state_id_from);

    // If already final: terminate now; budget must validate the guess
    if (child_state->getFinal()) {
        if (data->budget_from == 0) {
            data->inactive_to = true;
            explore_global_selection_supremum(0, 0, data);
        } else {
            explore_global_failure_supremum(data);
        }
        return;
    }

    const unsigned int i = data->cumulative_size[data->child_id_from] + data->child_state_id_from;

    auto* succs = child_state->getSuccessors(data->symbol->getId());
    if (!succs) {
        // Witness has no move: invalid
        explore_global_failure_supremum(data);
        return;
    }

    for (Edge* child_edge : *succs) {
        const unsigned int to_state_id = (unsigned int)child_edge->getTo()->getId();
        const unsigned int ii = data->cumulative_size[data->child_id_from] + to_state_id;

        const unsigned char stored_tracking   = data->new_tracking[ii];
        const unsigned char stored_activation = data->new_activation[ii];

        // Compute absolute edge cost (as in your code)
        internal_weight_t abs_child_edge_value;
        if (child_edge->getWeight()->getValue() < 0) {
            abs_child_edge_value = to_internal(-(child_edge->getWeight()->getValue()));
        } else {
            abs_child_edge_value = to_internal(child_edge->getWeight()->getValue());
        }

        const bool to_final = child_edge->getTo()->getFinal();

        // Propagate tracking/activation to successor only if token continues (not if it terminates now)
        if (!to_final) {
            if (data->old_tracking[i] == 1) {
                data->new_tracking[ii] = 1;
            }
            if (data->old_activation[i] == 1) {
                data->new_activation[ii] = 1;
            }
        }

        // Set token successor info (unless it terminates)
        data->child_id_to       = data->child_id_from;
        data->child_state_id_to = to_state_id;

        if (data->budget_from < data->abs_threshold) {
            // Fixed budget mode: deterministic subtraction
            if (data->budget_from < abs_child_edge_value) {
                explore_global_failure_supremum(data);
            } else {
                data->budget_to = data->budget_from - abs_child_edge_value;

                if (to_final) {
                    // Terminate in the same symbol: validate immediately
                    if (data->budget_to == 0) {
                        data->inactive_to = true;
                        explore_global_selection_supremum(0, 0, data);
                    } else {
                        explore_global_failure_supremum(data);
                    }
                } else {
                    data->inactive_to = false;
                    explore_global_selection_supremum(0, 0, data);
                }
            }
        } else {
            // Unlimited budget mode: nondeterministically guess successor budget
            if (to_final) {
                // Must terminate now with budget 0. Feasible only if edge alone can reach threshold.
                if (abs_child_edge_value >= data->abs_threshold) {
                    data->budget_to   = 0;
                    data->inactive_to = true;
                    explore_global_selection_supremum(0, 0, data);
                } else {
                    explore_global_failure_supremum(data);
                }
            } else {
                // Any guess where guess + edge_cost >= threshold is valid
                for (internal_weight_t abs_weight = data->abs_threshold; ; --abs_weight) {
                    if (abs_weight + abs_child_edge_value >= data->abs_threshold) {
                        data->budget_to   = abs_weight;
                        data->inactive_to = false;
                        explore_global_selection_supremum(0, 0, data);
                    }
                    if (abs_weight == 0) break;
                }
            }
        }

        data->new_tracking[ii]   = stored_tracking;
        data->new_activation[ii] = stored_activation;
    }
}

static void explore_global_master_transition_supremum(data_supremum_t* data) {
    auto* succs = data->A->getStates()->at(data->master_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    // Save tracked token context: each master edge explores independently
    const unsigned int      saved_child_state_id = data->child_state_id_from;
    const unsigned int      saved_child_id       = data->child_id_from;
    const internal_weight_t saved_budget         = data->budget_from;
    const bool              saved_inactive       = data->inactive_from;

    for (Edge* master_edge : *succs) {
        data->child_state_id_from = saved_child_state_id;
        data->child_id_from       = saved_child_id;
        data->budget_from         = saved_budget;
        data->inactive_from       = saved_inactive;

        data->master_state_id_to = static_cast<unsigned int>(master_edge->getTo()->getId());
        const unsigned int child_id = static_cast<unsigned int>(master_edge->getWeight()->getValue().to_float());

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // Silent transition: no spawn, weight 0
            data->global_edge_weight = 0;
            explore_global_child_transition_supremum(data);
        } else {
            const unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
            const unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

            // Mark spawned child as active in OLD arrays (so selection sees it)
            const unsigned char prev_act = data->old_activation[ii];
            data->old_activation[ii] = 1;

            // Choice 1: spawn as background (not tracked for weight)
            data->global_edge_weight = 0;
            explore_global_child_transition_supremum(data);

            // Choice 2: start tracking for weight (only if no token currently tracked)
            if (saved_inactive) {
                data->global_edge_weight  = 1;                 // bet on this token
                data->child_state_id_from = summoned_child_state_id;
                data->child_id_from       = child_id;
                data->budget_from         = data->abs_threshold; // start in "unlimited" mode
                data->inactive_from       = false;
                explore_global_child_transition_supremum(data);
            }

            data->old_activation[ii] = prev_act;
        }
    }
}

static void explore_global_initialization_supremum(data_supremum_t* data) {
    const unsigned int n = data->children_all;

    // Prepare old arrays from the stored vectors of the current global state
    data->old_activation.assign(n, 0);
    data->old_tracking.assign(n, 0);

    for (unsigned int i = 0; i < n; ++i) {
        if (i < data->activation_from.size()) data->old_activation[i] = data->activation_from[i] ? 1 : 0;
        if (i < data->tracking_from.size())   data->old_tracking[i]   = data->tracking_from[i] ? 1 : 0;
    }

    // Fresh new arrays for this symbol
    data->new_activation.assign(n, 0);
    data->new_tracking.assign(n, 0);

    // Iterate over enabled symbols of the current master state
    auto* alphabet = data->A->getStates()->at(data->master_state_id_from)->getAlphabet();
    if (!alphabet) return;

    for (Symbol* symbol : *alphabet) {
        data->symbol = symbol;
        explore_global_master_transition_supremum(data);
    }
}

// Track ONE child token explicitly for weight, others as background
bool NestedAutomaton::emptiness_monotonic_nesting_supremum(value_function_t infinite_aggregator,
                                                          value_function_t finite_aggregator,
                                                          weight_t threshold) {
    if (threshold <= 0 && finite_aggregator == SumPlus) return true;
    if (threshold > 0 && finite_aggregator == SumMinus) return false;

    internal_weight_t abs_threshold;
    if (threshold > 0) abs_threshold = to_internal(threshold);
    else abs_threshold = to_internal(-threshold + 1);

    // Flatten child state space
    std::vector<unsigned int> cumulative_size(this->children_->size() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->children_->size() + 1; ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int children_all = cumulative_size[this->children_->size()];

    // Epoch reset vector (all ones)
    std::vector<unsigned char> track_them_all(children_all, 1);

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);
    parser->states.insert("@sink@");

    // Sink self-loops on all master symbols
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

    // Initial: no children active, no obligations discharged yet (tracking=0), no witness
    std::vector<unsigned char> zero(children_all, 0);

    std::string global_initial;
    global_initial.reserve(64 + children_all * 2);
    global_initial.append(std::to_string(this->initial->getId()));
    global_initial.push_back('/');
    global_initial.append(bits_to_string(zero)); // activation
    global_initial.push_back('/');
    global_initial.append(bits_to_string(zero)); // tracking
    global_initial.append("/@inactive@");

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    data_supremum_t* data = new data_supremum_t();
    if (data == nullptr) QUAK_FAIL("out of memory");

    data->A = this;
    data->parser = parser;
    data->abs_threshold = abs_threshold;
    data->cumulative_size = cumulative_size.data();
    data->children_all = children_all;
    data->track_them_all = std::move(track_them_all);

    // Iterative DFS using explicit stack
    std::vector<pending_state_t> pending_states;
    pending_states.reserve(1024);

    pending_state_t initial_ps;
    initial_ps.global_from          = global_initial;
    initial_ps.master_state_id_from = this->initial->getId();
    initial_ps.inactive_from        = true;
    initial_ps.activation_from      = zero;
    initial_ps.tracking_from        = zero;
    initial_ps.budget_from          = 0;
    initial_ps.child_state_id_from  = 0;
    initial_ps.child_id_from        = 0;
    pending_states.push_back(std::move(initial_ps));

    data->pending_states = &pending_states;

    while (!pending_states.empty()) {
        pending_state_t ps = std::move(pending_states.back());
        pending_states.pop_back();

        data->global_from          = std::move(ps.global_from);
        data->master_state_id_from = ps.master_state_id_from;
        data->inactive_from        = ps.inactive_from;
        data->activation_from      = std::move(ps.activation_from);
        data->tracking_from        = std::move(ps.tracking_from);

        if (!ps.inactive_from) {
            data->budget_from         = ps.budget_from;
            data->child_state_id_from = ps.child_state_id_from;
            data->child_id_from       = ps.child_id_from;
        } else {
            data->budget_from         = 0;
            data->child_state_id_from = 0;
            data->child_id_from       = 0;
        }

        explore_global_initialization_supremum(data);
    }

    delete data;

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    std::cout << "unnested: " << unnested->getStates()->size() << " states, " << unnested->getNbTransitions() << " edges, " << unnested->getNbSCCs() << " SCCs (" << unnested->getNbAcceptingSCCs() << " accepting)" << std::endl;
    // unnested->print();

    weight_t top = unnested->compute_top_with_final(infinite_aggregator);
    const bool result = (top >= 1);

    delete unnested;
    return result;
}












/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////


typedef internal_weight_t (*beyond_threshold_fn_t)(internal_weight_t, internal_weight_t);

typedef struct global_exploration_data_all {
    // constraints
    NestedAutomaton* A;
    Parser* parser;
    internal_weight_t abs_threshold;
    unsigned int* cumulative_size;
    unsigned int children_all;
    beyond_threshold_fn_t beyond_threshold;

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int master_state_id_from;
    std::vector<unsigned int> global_tracking_from;     // size = children_all, values 0 or 1
    std::vector<internal_weight_t> global_budget_from;  // size = children_all
    unsigned int master_tracking_from;

    // initialized per-symbol
    std::vector<internal_weight_t> old_value_of_children_state;
    std::vector<unsigned int> old_tracked_children_state;
    Symbol* symbol;

    // computed (output accumulators)
    unsigned int master_state_id_to;
    internal_weight_t global_edge_weight;
    std::vector<unsigned int> new_tracked_children_state;
    std::vector<internal_weight_t> new_value_of_children_state;
    unsigned int master_tracking_to;
} data_all_t;

// Weight 1 at discharge point (good guess)
internal_weight_t beyond_good_threshold (internal_weight_t value, internal_weight_t abs_threshold) {
    return (value == abs_threshold) ? 1 : 0;
}

// Inverted: reaching threshold is the losing condition
internal_weight_t beyond_bad_threshold (internal_weight_t value, internal_weight_t abs_threshold) {
    return (value == abs_threshold) ? 0 : 1;
}

void explore_global_initialization (data_all_t* data);
void explore_global_selection (unsigned int child_id, unsigned int child_state_id, data_all_t* data);
void explore_global_master_transition (data_all_t* data);

// Sink loops with weight 0 on all symbols (installed once in caller)
void explore_global_failure (data_all_t* data) {
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

void explore_global_finalization (data_all_t* data) {
    std::vector<internal_weight_t> global_budget_to = data->new_value_of_children_state;
    std::vector<unsigned int> child_tracking_to = data->new_tracked_children_state;

    bool global_final = false;

    bool all_untracked = true;
    for (unsigned int i = 0; i < data->children_all; ++i) {
        if (child_tracking_to[i]) { all_untracked = false; break; }
    }

    if (all_untracked && data->master_tracking_to == 0) {
        for (unsigned int i = 0; i < data->children_all; ++i) {
            child_tracking_to[i] = 1;
        }
        data->master_tracking_to = 1;
        State* master_to_state = data->A->getStates()->at(data->master_state_id_to);
        global_final = master_to_state->getFinal();
    }

    std::string global_to;
    global_to.reserve(64 + data->children_all * 12);
    global_to.append(std::to_string(data->master_state_id_to));
    global_to.push_back('/');
    for (unsigned int i = 0; i < data->children_all; ++i) {
        if (i > 0) global_to.push_back(',');
        global_to.append(std::to_string(global_budget_to[i]));
    }
    global_to.push_back('/');
    for (unsigned int i = 0; i < data->children_all; ++i) {
        global_to.push_back(child_tracking_to[i] ? '1' : '0');
    }
    global_to.push_back(data->master_tracking_to ? '1' : '0');

    if (global_final) {
        data->parser->final_states.insert(global_to);
    }

    data->parser->edges.insert({
        { data->symbol->getName(), (weight_t)data->global_edge_weight },
        { data->global_from, global_to }
    });

    if (!data->parser->states.contains(global_to)) {
        data->parser->states.insert(global_to);

        data_all_t data_deeper{};
        data_deeper.A = data->A;
        data_deeper.parser = data->parser;
        data_deeper.abs_threshold = data->abs_threshold;
        data_deeper.cumulative_size = data->cumulative_size;
        data_deeper.children_all = data->children_all;
        data_deeper.beyond_threshold = data->beyond_threshold;

        data_deeper.global_from = global_to;
        data_deeper.master_state_id_from = data->master_state_id_to;
        data_deeper.global_tracking_from = child_tracking_to;
        data_deeper.global_budget_from = global_budget_to;
        data_deeper.master_tracking_from = data->master_tracking_to;

        explore_global_initialization(&data_deeper);
    }
}
// Enumerate consistent successor assignments for all child-states under current master edge
// Precondition: old_* is complete snapshot; new_* starts as inactive/untracked
void explore_global_selection(unsigned int start_child_id, unsigned int start_child_state_id, data_all_t* data) {
    struct StackFrame {
        unsigned int child_id;
        unsigned int child_state_id;
        std::vector<Edge*> edges;
        size_t edge_index;
        unsigned int ii;
        unsigned int stored_tracking;
        internal_weight_t stored_budget;
    };

    std::vector<StackFrame> stack;

    // Find next active non-final state with edges, starting from (cid, csid)
    auto find_next_choice = [&](unsigned int cid, unsigned int csid)
        -> std::tuple<unsigned int, unsigned int, std::vector<Edge*>>
    {
        while (cid < data->A->getChildrenSize()) {
            ChildAutomaton* child = data->A->getChild(cid);
            auto* states = child->getStates();

            while (csid < states->size()) {
                unsigned int i = data->cumulative_size[cid] + csid;

                if (data->old_value_of_children_state[i] != data->abs_threshold + 1 &&
                    !states->at(csid)->getFinal())
                {
                    State* child_state = states->at(csid);
                    auto* succs = child_state->getSuccessors(data->symbol->getId());
                    if (succs) {
                        return {cid, csid, std::vector<Edge*>(succs->begin(), succs->end())};
                    }
                }
                csid++;
            }
            cid++;
            csid = 0;
        }
        return {cid, csid, {}};
    };

    // Find first choice point
    auto [init_cid, init_csid, init_edges] = find_next_choice(start_child_id, start_child_state_id);

    if (init_edges.empty()) {
        explore_global_finalization(data);
        return;
    }

    stack.push_back({init_cid, init_csid, std::move(init_edges), 0, 0, 0, 0});

    while (!stack.empty()) {
        StackFrame& frame = stack.back();

        // Restore state from previous edge iteration (if any)
        if (frame.edge_index > 0) {
            data->new_tracked_children_state[frame.ii] = frame.stored_tracking;
            data->new_value_of_children_state[frame.ii] = frame.stored_budget;
        }

        bool pushed_new_frame = false;

        while (frame.edge_index < frame.edges.size()) {
            Edge* edge = frame.edges[frame.edge_index];
            unsigned int i = data->cumulative_size[frame.child_id] + frame.child_state_id;
            unsigned int ii = data->cumulative_size[frame.child_id] + edge->getTo()->getId();

            // Save for backtracking
            frame.ii = ii;
            frame.stored_tracking = data->new_tracked_children_state[ii];
            frame.stored_budget = data->new_value_of_children_state[ii];

            const bool to_final = edge->getTo()->getFinal();

            if (data->old_tracked_children_state[i] && !to_final) {
                data->new_tracked_children_state[ii] = true;
            }

            internal_weight_t abs_edge_value = (edge->getWeight()->getValue() < 0)
                ? to_internal(-(edge->getWeight()->getValue()))
                : to_internal(edge->getWeight()->getValue());

            bool should_recurse = false;

            if (to_final) {
                internal_weight_t oldb = data->old_value_of_children_state[i];
                internal_weight_t required =
                    (abs_edge_value >= data->abs_threshold) ? data->abs_threshold : abs_edge_value;

                if (oldb != required) {
                    explore_global_failure(data);
                } else {
                    data->new_tracked_children_state[ii] = false;
                    data->new_value_of_children_state[ii] = data->abs_threshold + 1;
                    should_recurse = true;
                }
            }
            else if (data->old_value_of_children_state[i] <= data->abs_threshold) {
                internal_weight_t oldb = data->old_value_of_children_state[i];

                if (oldb < abs_edge_value) {
                    explore_global_failure(data);
                } else {
                    internal_weight_t nextb = oldb - abs_edge_value;

                    if (data->new_value_of_children_state[ii] == data->abs_threshold + 1) {
                        data->new_value_of_children_state[ii] = nextb;
                        should_recurse = true;
                    } else if (data->new_value_of_children_state[ii] != nextb) {
                        explore_global_failure(data);
                    } else {
                        should_recurse = true;
                    }
                }
            }

            frame.edge_index++;

            if (should_recurse) {
                auto [next_cid, next_csid, next_edges] =
                    find_next_choice(frame.child_id, frame.child_state_id + 1);

                if (next_edges.empty()) {
                    explore_global_finalization(data);
                    // Restore and continue to next edge
                    data->new_tracked_children_state[ii] = frame.stored_tracking;
                    data->new_value_of_children_state[ii] = frame.stored_budget;
                } else {
                    // Push deeper frame
                    stack.push_back({next_cid, next_csid, std::move(next_edges), 0, 0, 0, 0});
                    pushed_new_frame = true;
                    break;
                }
            } else {
                // Failure: restore and continue to next edge
                data->new_tracked_children_state[ii] = frame.stored_tracking;
                data->new_value_of_children_state[ii] = frame.stored_budget;
            }
        }

        if (!pushed_new_frame) {
            stack.pop_back();
        }
    }
}
void explore_global_master_transition (data_all_t* data) {
    auto* succs = data->A->getStates()->at(data->master_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    for (Edge* edge : *succs) {
        data->master_state_id_to = edge->getTo()->getId();
        unsigned int child_id = (edge->getWeight()->getValue()).to_uint();  // edge weight encodes summoned child

        data->master_tracking_to = data->master_tracking_from;

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // Silent: can't end epoch, only advances master control
            data->global_edge_weight = 1;
            explore_global_selection(0, 0, data);
        } else {
            // Non-silent: epoch has now seen observable action
            data->master_tracking_to = 0;

            unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
            unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

            // Spawn via OLD arrays -- selection reads OLD to determine activeness
            internal_weight_t saved_old_budget = data->old_value_of_children_state[ii];
            unsigned int saved_old_track = data->old_tracked_children_state[ii];

            // Clear NEW to avoid leftovers from prior master edges
            internal_weight_t saved_new_budget = data->new_value_of_children_state[ii];
            unsigned int saved_new_track = data->new_tracked_children_state[ii];
            data->new_value_of_children_state[ii] = data->abs_threshold + 1;
            data->new_tracked_children_state[ii] = false;

            if (saved_old_budget == data->abs_threshold + 1) {
                // Slot empty -- nondeterministically guess initial budget
                for (internal_weight_t abs_weight = 0; abs_weight <= data->abs_threshold; ++abs_weight) {
                    data->old_value_of_children_state[ii] = abs_weight;
                    data->old_tracked_children_state[ii] = true;
                    data->global_edge_weight = data->beyond_threshold(abs_weight, data->abs_threshold);
                    explore_global_selection(0, 0, data);
                }
            } else {
                // Token exists -- can't represent two tokens at same state, just ensure tracking
                data->old_tracked_children_state[ii] = true;
                data->global_edge_weight = data->beyond_threshold(saved_old_budget, data->abs_threshold);
                explore_global_selection(0, 0, data);
            }

            data->old_value_of_children_state[ii] = saved_old_budget;
            data->old_tracked_children_state[ii] = saved_old_track;
            data->new_value_of_children_state[ii] = saved_new_budget;
            data->new_tracked_children_state[ii] = saved_new_track;
        }
    }
}

void explore_global_initialization (data_all_t* data) {
    const unsigned int n = data->children_all;

    data->new_value_of_children_state.assign(n, data->abs_threshold + 1);
    data->new_tracked_children_state.assign(n, false);

    data->old_value_of_children_state = data->global_budget_from;
    data->old_tracked_children_state = data->global_tracking_from;

    auto* alphabet = data->A->getStates()->at(data->master_state_id_from)->getAlphabet();
    if (!alphabet) return;

    for (Symbol* symbol : *alphabet) {
        data->symbol = symbol;
        explore_global_master_transition(data);
    }
}


bool NestedAutomaton::emptiness_monotonic_nesting(value_function_t infinite_aggregator,
                                                 value_function_t finite_aggregator,
                                                 weight_t threshold) {
    if (threshold <= 0 && finite_aggregator == SumPlus) {
        return true;
    }
    if (threshold > 0 && finite_aggregator == SumMinus) {
        return false;
    }

    internal_weight_t abs_threshold;
    beyond_threshold_fn_t beyond_threshold;
    if (threshold > 0) {
        abs_threshold = to_internal(threshold);
        beyond_threshold = beyond_good_threshold;
    } else {
        abs_threshold = to_internal(-threshold + 1);
        beyond_threshold = beyond_bad_threshold;
    }

    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->getChildrenSize() + 1; i++) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    unsigned int children_all = cumulative_size[this->getChildrenSize()];

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);

    parser->states.insert("@sink@");
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

    std::vector<internal_weight_t> global_budget_initial(children_all, abs_threshold + 1);
    std::vector<unsigned int> global_tracking_initial(children_all, 0);
    unsigned int master_tracking_initial = 1;

    std::string global_initial;
    global_initial.reserve(64 + children_all * 12);
    global_initial.append(std::to_string(this->initial->getId()));
    global_initial.push_back('/');
    for (unsigned int i = 0; i < children_all; ++i) {
        if (i > 0) global_initial.push_back(',');
        global_initial.append(std::to_string(global_budget_initial[i]));
    }
    global_initial.push_back('/');
    for (unsigned int i = 0; i < children_all; ++i) {
        global_initial.push_back('0');
    }
    global_initial.push_back('1');

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    data_all_t data{};
    data.A = this;
    data.parser = parser;
    data.abs_threshold = abs_threshold;
    data.cumulative_size = cumulative_size.data();
    data.children_all = children_all;
    data.beyond_threshold = beyond_threshold;
    data.global_from = global_initial;
    data.master_state_id_from = this->initial->getId();
    data.global_tracking_from = global_tracking_initial;
    data.global_budget_from = global_budget_initial;
    data.master_tracking_from = master_tracking_initial;

    explore_global_initialization(&data);

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    unnested->print();
    delete parser;

    std::cout << "unnested: " << unnested->getStates()->size() << " states, " << unnested->getNbTransitions() << " edges, " << unnested->getNbSCCs() << " SCCs (" << unnested->getNbAcceptingSCCs() << " accepting)" << std::endl;

    weight_t top = unnested->compute_top_with_final(infinite_aggregator);
    bool result = (top == 1);

    delete unnested;
    return result;
}


/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////


bool NestedAutomaton::emptiness_Avg_SumPlus (weight_t threshold) {
    unsigned int theoretical_bound = 0;
    unsigned int max_weight = 1;
    for (unsigned int i = 0; i < this->children_->size(); i++) {
        theoretical_bound = std::max(theoretical_bound, (unsigned int)this->getChild(i)->getStates()->size());
        max_weight = std::max(max_weight, this->getChild(i)->getMaxDomain().to_uint());
    }
    theoretical_bound = theoretical_bound * max_weight * this->getStates()->size();


    // Fast path: if supremum is unbounded, threshold is always achievable
    if (emptiness_monotonic_nesting_supremum(LimSup, SumPlus, theoretical_bound)) {
        return true;
    }
    else {
        Automaton* flat = flatten_regular(SumB, theoretical_bound); // Key Lemma construction
        // std::cout << "flat: " << flat->getStates()->size() << " states, " << flat->getNbTransitions() << " edges, " << flat->getNbSCCs() << " SCCs (" << flat->getNbAcceptingSCCs() << " accepting)" << std::endl;
        // flat->print();
        Automaton* nonSilent = Automaton::removeSilentTransitions(flat, LimSupAvg, true);
        std::cout << "unnested: " << nonSilent->getStates()->size() << " states, " << nonSilent->getNbTransitions() << " edges, " << nonSilent->getNbSCCs() << " SCCs (" << nonSilent->getNbAcceptingSCCs() << " accepting)" << std::endl;
        // nonSilent->print();

        bool res = nonSilent->emptiness_LimAvg_with_final(threshold);
        delete flat;
        delete nonSilent;
        return res;
    }
}


/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////






static std::string vec_to_string(const std::vector<unsigned int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) s.push_back(',');
        s.append(std::to_string(v[i] & 3u));
    }
    return s;
}


// Worklist item for Min_f/Max_f under Sup/LimSup: track ONE distinguished child-token,
// and represent activation/tracking as variable-length vectors (no fixed-size bitmasks).
struct min_max_sup_work_item {
    std::string global_from;
    unsigned int master_state_id_from;

    // 0/1 per flattened child-state
    std::vector<unsigned char> activation_from;
    std::vector<unsigned char> tracking_from;

    // Distinguished token being tracked for the (0/1) edge weight.
    // If inactive_from==true, the remaining fields are ignored.
    bool inactive_from = true;
    unsigned int witness_child_id_from = 0;
    unsigned int witness_child_state_id_from = 0;
    unsigned int witness_y_from = 0; // monotone bit: Max_f starts 0, Min_f starts 1
};


// ============================================================================
//  Optimized Min_f/Max_f construction for Sup/LimSup (track ONE token)
//  IMPORTANT: activation/tracking are variable-length vectors (no fixed-size
//  bitmasks), so the construction supports arbitrary numbers of flattened
//  child-states.
// ============================================================================

typedef struct global_exploration_data_min_max_supremum {
    // constraints
    NestedAutomaton* A = nullptr;
    Parser* parser = nullptr;
    weight_t threshold{};
    unsigned int* cumulative_size = nullptr;
    unsigned int children_all = 0;
    unsigned int finite_is_max = 0; // 0 = Min_f, 1 = Max_f

    // epoch reset vector (all ones)
    std::vector<unsigned char> track_them_all;

    // worklist for iterative DFS
    std::vector<min_max_sup_work_item>* worklist = nullptr;

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int master_state_id_from = 0;
    std::vector<unsigned char> activation_from;
    std::vector<unsigned char> tracking_from;
    bool inactive_from = true;
    unsigned int witness_child_id_from = 0;
    unsigned int witness_child_state_id_from = 0;
    unsigned int witness_y_from = 0;

    // initialized per-symbol
    Symbol* symbol = nullptr;
    std::vector<unsigned char> old_activation;
    std::vector<unsigned char> old_tracking;
    std::vector<unsigned char> new_activation;
    std::vector<unsigned char> new_tracking;

    // computed (output accumulators)
    unsigned int master_state_id_to = 0;
    weight_t global_edge_weight = 0;
    bool inactive_to = true;
    unsigned int witness_child_id_to = 0;
    unsigned int witness_child_state_id_to = 0;
    unsigned int witness_y_to = 0;
} data_min_max_supremum_t;

static void explore_global_initialization_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_master_transition_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_child_transition_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_selection_min_max_supremum(unsigned int child_id, unsigned int child_state_id,
                                                      data_min_max_supremum_t* data);
static void explore_global_finalization_min_max_supremum(data_min_max_supremum_t* data);
static void explore_global_failure_min_max_supremum(data_min_max_supremum_t* data);

// static inline bool tracking_all_zero(const std::vector<unsigned char>& v) {
//     for (unsigned char b : v) {
//         if (b != 0) return false;
//     }
//     return true;
// }

// static std::string bits_to_string(const std::vector<unsigned char>& v) {
//     std::string s;
//     s.reserve(v.size());
//     for (unsigned char b : v) {
//         s.push_back(b ? '1' : '0');
//     }
//     return s;
// }

static inline unsigned int min_max_y_update(const weight_t& edge_value,
                                            unsigned int y_current,
                                            const data_min_max_supremum_t* data) {
    const bool pass = !(edge_value < data->threshold); // edge_value >= threshold
    if (data->finite_is_max) {
        // Max_f: y' = y OR pass
        return (y_current != 0u || pass) ? 1u : 0u;
    }
    // Min_f: y' = y AND pass
    return (y_current != 0u && pass) ? 1u : 0u;
}

static void explore_global_failure_min_max_supremum(data_min_max_supremum_t* data) {
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

static void explore_global_finalization_min_max_supremum(data_min_max_supremum_t* data) {
    // Compute destination activation/tracking vectors.
    std::vector<unsigned char> activation_to = data->new_activation; // copy
    std::vector<unsigned char> tracking_to   = data->new_tracking;   // copy

    bool global_final = false;
    // Epoch boundary: exactly as in explore_global_finalization_supremum (Sum+/Sum-)
    if (tracking_all_zero(data->tracking_from)) {
        tracking_to = data->track_them_all; // reset obligations
        global_final = data->A->getStates()->at(data->master_state_id_to)->getFinal();
    }

    // Encode destination state:
    //   master_id/activation_bits/tracking_bits/[child_id/child_state/y | @inactive@]
    std::string global_to;
    global_to.reserve(64 + data->children_all * 2);
    global_to.append(std::to_string(data->master_state_id_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(activation_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(tracking_to));

    if (data->inactive_to) {
        global_to.append("/@inactive@");
    } else {
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_child_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_child_state_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_y_to));
    }

    data->parser->edges.insert({
        { data->symbol->getName(), data->global_edge_weight },
        { data->global_from, global_to }
    });

    if (global_final) {
        data->parser->final_states.insert(global_to);
    }

    // Iterative DFS: push to worklist on first discovery
    if (!data->parser->states.contains(global_to)) {
        data->parser->states.insert(global_to);
        data->worklist->push_back({
            global_to,
            data->master_state_id_to,
            std::move(activation_to),
            std::move(tracking_to),
            data->inactive_to,
            data->witness_child_id_to,
            data->witness_child_state_id_to,
            data->witness_y_to
        });
    }
}

static void explore_global_selection_min_max_supremum(unsigned int child_id,
                                                      unsigned int child_state_id,
                                                      data_min_max_supremum_t* data) {
    // Skip the distinguished token's FROM location (handled in explore_global_child_transition_min_max_supremum)
    if (!data->inactive_from &&
        child_id == data->witness_child_id_from &&
        child_state_id == data->witness_child_state_id_from) {
        explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
        return;
    }

    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_activation[i] == 0) {
                explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
                return;
            }

            if (states->at(child_state_id)->getFinal()) {
                // Background final states terminate immediately.
                explore_global_selection_min_max_supremum(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (succs) {
                for (Edge* edge : *succs) {
                    // If successor is final: terminate in the same symbol (do not propagate)
                    if (edge->getTo()->getFinal()) {
                        explore_global_selection_min_max_supremum(child_id + 1, 0, data);
                        continue;
                    }

                    const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();
                    const unsigned char stored_tracking = data->new_tracking[ii];
                    const unsigned char stored_activation = data->new_activation[ii];

                    if (data->old_tracking[i] == 1) {
                        data->new_tracking[ii] = 1;
                    }
                    if (data->old_activation[i] == 1) {
                        data->new_activation[ii] = 1;
                    }

                    explore_global_selection_min_max_supremum(child_id + 1, 0, data);

                    data->new_tracking[ii] = stored_tracking;
                    data->new_activation[ii] = stored_activation;
                }
            } else {
                // No successors on this symbol: treat as silent "no move" for background
                explore_global_selection_min_max_supremum(child_id + 1, 0, data);
            }
        } else {
            explore_global_selection_min_max_supremum(child_id + 1, 0, data);
        }
    } else {
        explore_global_finalization_min_max_supremum(data);
    }
}

static void explore_global_child_transition_min_max_supremum(data_min_max_supremum_t* data) {
    if (data->inactive_from) {
        data->inactive_to = true;
        explore_global_selection_min_max_supremum(0, 0, data);
        return;
    }

    ChildAutomaton* child = data->A->getChild(data->witness_child_id_from);
    State* child_state = child->getStates()->at(data->witness_child_state_id_from);

    // Should normally not happen if we enforce "terminate in same symbol", but handle robustly.
    if (child_state->getFinal()) {
        if (data->witness_y_from == 1u) {
            data->inactive_to = true;
            explore_global_selection_min_max_supremum(0, 0, data);
        } else {
            explore_global_failure_min_max_supremum(data);
        }
        return;
    }

    const unsigned int i = data->cumulative_size[data->witness_child_id_from] + data->witness_child_state_id_from;

    auto* succs = child_state->getSuccessors(data->symbol->getId());
    if (!succs) {
        explore_global_failure_min_max_supremum(data);
        return;
    }

    for (Edge* child_edge : *succs) {
        const unsigned int to_state_id = (unsigned int)child_edge->getTo()->getId();
        const unsigned int ii = data->cumulative_size[data->witness_child_id_from] + to_state_id;

        const unsigned char stored_tracking = data->new_tracking[ii];
        const unsigned char stored_activation = data->new_activation[ii];

        const unsigned int y_next = min_max_y_update(child_edge->getWeight()->getValue(), data->witness_y_from, data);

        // For Min_f, once y becomes 0 it can never recover, so the branch is dead.
        if (!data->finite_is_max && y_next == 0u) {
            explore_global_failure_min_max_supremum(data);
            continue;
        }

        if (child_edge->getTo()->getFinal()) {
            // Terminate in the same symbol; success requires y_next==1
            if (y_next == 1u) {
                data->inactive_to = true;
                explore_global_selection_min_max_supremum(0, 0, data);
            } else {
                explore_global_failure_min_max_supremum(data);
            }
            continue;
        }

        // Propagate activation/tracking for the witness token to its successor.
        if (data->old_tracking[i] == 1) {
            data->new_tracking[ii] = 1;
        }
        if (data->old_activation[i] == 1) {
            data->new_activation[ii] = 1;
        }

        data->inactive_to = false;
        data->witness_child_id_to = data->witness_child_id_from;
        data->witness_child_state_id_to = to_state_id;
        data->witness_y_to = y_next;

        explore_global_selection_min_max_supremum(0, 0, data);

        data->new_tracking[ii] = stored_tracking;
        data->new_activation[ii] = stored_activation;
    }
}

static void explore_global_master_transition_min_max_supremum(data_min_max_supremum_t* data) {
    auto* succs = data->A->getStates()->at(data->master_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    // Save witness context: each master edge explores independently
    const bool saved_inactive = data->inactive_from;
    const unsigned int saved_w_child = data->witness_child_id_from;
    const unsigned int saved_w_state = data->witness_child_state_id_from;
    const unsigned int saved_w_y = data->witness_y_from;

    for (Edge* master_edge : *succs) {
        data->inactive_from = saved_inactive;
        data->witness_child_id_from = saved_w_child;
        data->witness_child_state_id_from = saved_w_state;
        data->witness_y_from = saved_w_y;

        data->master_state_id_to = static_cast<unsigned int>(master_edge->getTo()->getId());
        const unsigned int child_id = static_cast<unsigned int>(master_edge->getWeight()->getValue().to_float());

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // Silent: no spawn, weight 0
            data->global_edge_weight = 0;
            explore_global_child_transition_min_max_supremum(data);
        } else {
            const unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
            const unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

            // Mark spawned token as active in OLD arrays (so selection sees it)
            const unsigned char prev_act = data->old_activation[ii];
            data->old_activation[ii] = 1;

            // Choice 1: NOT summon as witness (spawn only as background)
            data->global_edge_weight = 0;
            explore_global_child_transition_min_max_supremum(data);

            // Choice 2: start tracking as witness (only if no witness already tracked)
            if (saved_inactive) {
                data->global_edge_weight = 1;
                data->inactive_from = false;
                data->witness_child_id_from = child_id;
                data->witness_child_state_id_from = summoned_child_state_id;
                data->witness_y_from = data->finite_is_max ? 0u : 1u;
                explore_global_child_transition_min_max_supremum(data);
            }

            data->old_activation[ii] = prev_act;
        }
    }
}

static void explore_global_initialization_min_max_supremum(data_min_max_supremum_t* data) {
    const unsigned int n = data->children_all;

    data->old_activation.resize(n);
    data->old_tracking.resize(n);
    data->new_activation.assign(n, 0);
    data->new_tracking.assign(n, 0);

    for (unsigned int i = 0; i < n; ++i) {
        data->old_activation[i] = (i < data->activation_from.size()) ? (data->activation_from[i] ? 1 : 0) : 0;
        data->old_tracking[i]   = (i < data->tracking_from.size()) ? (data->tracking_from[i] ? 1 : 0) : 0;
    }

    auto* alphabet = data->A->getStates()->at(data->master_state_id_from)->getAlphabet();
    if (!alphabet) return;

    for (Symbol* symbol : *alphabet) {
        data->symbol = symbol;
        explore_global_master_transition_min_max_supremum(data);
    }
}

bool NestedAutomaton::emptiness_monotonic_nesting_min_max_supremum(value_function_t infinite_aggregator,
                                                                   value_function_t finite_aggregator,
                                                                   weight_t threshold) {
    if (!(infinite_aggregator == Sup || infinite_aggregator == LimSup)) {
        QUAK_FAIL("emptiness_monotonic_nesting_min_max_supremum: requires Sup/LimSup");
    }
    unsigned int finite_is_max = 0u;
    if (finite_aggregator == Max_f) {
        finite_is_max = 1u;
    } else if (finite_aggregator == Min_f) {
        finite_is_max = 0u;
    } else {
        QUAK_FAIL("emptiness_monotonic_nesting_min_max_supremum: requires Min_f/Max_f");
    }

    // Flatten child state space
    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->getChildrenSize() + 1; ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int children_all = cumulative_size[this->getChildrenSize()];

    // Epoch reset vector (all ones)
    std::vector<unsigned char> track_them_all(children_all, 1);

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);
    parser->states.insert("@sink@");

    // Sink self-loops on all master symbols
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

    // Initial: no children active, no obligations discharged yet (tracking=0), no witness
    std::vector<unsigned char> zero(children_all, 0);
    std::string global_initial;
    global_initial.reserve(64 + children_all * 2);
    global_initial.append(std::to_string(this->initial->getId()));
    global_initial.push_back('/');
    global_initial.append(bits_to_string(zero)); // activation
    global_initial.push_back('/');
    global_initial.append(bits_to_string(zero)); // tracking
    global_initial.append("/@inactive@");

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    std::vector<min_max_sup_work_item> worklist;
    worklist.push_back({
        global_initial,
        (unsigned int)this->initial->getId(),
        zero,
        zero,
        true,
        0u,
        0u,
        finite_is_max ? 0u : 1u
    });

    data_min_max_supremum_t data{};
    data.A = this;
    data.parser = parser;
    data.threshold = threshold;
    data.cumulative_size = cumulative_size.data();
    data.children_all = children_all;
    data.finite_is_max = finite_is_max;
    data.track_them_all = std::move(track_them_all);
    data.worklist = &worklist;

    while (!worklist.empty()) {
        min_max_sup_work_item item = std::move(worklist.back());
        worklist.pop_back();

        data.global_from = std::move(item.global_from);
        data.master_state_id_from = item.master_state_id_from;
        data.activation_from = std::move(item.activation_from);
        data.tracking_from = std::move(item.tracking_from);
        data.inactive_from = item.inactive_from;
        data.witness_child_id_from = item.witness_child_id_from;
        data.witness_child_state_id_from = item.witness_child_state_id_from;
        data.witness_y_from = item.witness_y_from;

        explore_global_initialization_min_max_supremum(&data);
    }

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    std::cout << "unnested: " << unnested->getStates()->size() << " states, " << unnested->getNbTransitions() << " edges, " << unnested->getNbSCCs() << " SCCs (" << unnested->getNbAcceptingSCCs() << " accepting)" << std::endl;
    // unnested->print();

    weight_t top = unnested->compute_top_with_final(infinite_aggregator);
    const bool result = (top >= 1);

    delete unnested;
    return result;
}





// Worklist item for iterative DFS (avoids stack overflow)
struct min_max_work_item {
    std::string global_from;
    unsigned int master_state_id_from;
    unsigned int master_tracking_from;
    std::vector<unsigned int> from_0_0;
    std::vector<unsigned int> from_0_1;
    std::vector<unsigned int> from_1_0;
    std::vector<unsigned int> from_1_1;
};

typedef struct global_exploration_data_min_max {
    // constraints
    NestedAutomaton* A;
    Parser* parser;
    weight_t threshold;
    unsigned int* cumulative_size; // prefix sums for flattening (child_id, local_state) -> global index
    unsigned int children_all;
    unsigned int inf_or_sup;       // 0 = inf-type outer, 1 = sup-type outer
    unsigned int finite_is_max;    // 0 = Min_f, 1 = Max_f

    // worklist for iterative DFS
    std::vector<min_max_work_item>* worklist;

    // given (input for current exploration frame)
    std::string global_from;
    unsigned int master_state_id_from;
    unsigned int master_tracking_from;
    std::vector<unsigned int> from_0_0;
    std::vector<unsigned int> from_0_1;
    std::vector<unsigned int> from_1_0;
    std::vector<unsigned int> from_1_1;

    // initialized per-symbol
    Symbol* symbol = nullptr;
    std::vector<unsigned int> old_0_0, old_0_1, old_1_0, old_1_1;
    std::vector<unsigned int> new_0_0, new_0_1, new_1_0, new_1_1;

    // computed (output accumulators)
    unsigned int master_state_id_to = 0;
    unsigned int master_tracking_to = 0;
    weight_t global_edge_weight = 0;
} data_min_max_t;

static void explore_global_initialization_min_max(data_min_max_t* data);
static void explore_global_master_transition_min_max(data_min_max_t* data);
static void explore_global_finalization_min_max(data_min_max_t* data);
static void explore_global_failure_min_max(data_min_max_t* data);

static void explore_global_selection_case_0_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);
static void explore_global_selection_case_0_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);
static void explore_global_selection_case_1_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);
static void explore_global_selection_case_1_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);

static void explore_global_selection_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
    explore_global_selection_case_0_0_min_max(child_id, child_state_id, data);
}

static void explore_global_failure_min_max(data_min_max_t* data) {
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

/*
  Status x_y:
    x = objective bit (0: want final outcome 0, 1: want final outcome 1)  -> birth guess -> edge weight
    y = current bit (monotone progress w.r.t. threshold, depends on finite aggregator):
        - Max_f: y starts 0 and can flip 0->1 when seeing edge>=threshold
        - Min_f: y starts 1 and can flip 1->0 when seeing edge<threshold

  The helper below returns "ok" in the SAME way your four cases currently use it:
    - in *_0 cases: ok means "flip to *_1"
    - in *_1 cases: ok means "stay in *_1"
*/
static bool beyond_threshold_min_max(const weight_t& edge_value,
                                     bool /*guessed_weight_unused*/,
                                     bool current_is_1,
                                     const data_min_max_t* data) {
    const bool pass = !(edge_value < data->threshold); // edge_value >= threshold

    if (data->finite_is_max) {
        // Max_f: y' = y OR pass
        if (!current_is_1) return pass; // 0 -> 1 iff pass
        return true;                    // 1 stays 1
    } else {
        // Min_f: y' = y AND pass
        if (!current_is_1) return false; // 0 stays 0
        return pass;                     // 1 stays 1 iff pass, else flips to 0
    }
}

/*
  enforce "final-state handling in the same symbol" on the NEW arrays.
  - If a child is in a final state with status 0_0 or 1_1: it MUST terminate now -> drop it from new_*.
  - If a child is in a final state with status 0_1 or 1_0: it would be forced to terminate but cannot -> branch dies.
*/
static bool cleanup_new_on_finals_min_max(data_min_max_t* data) {
    const unsigned int active_bit = 1u;

    for (unsigned int cid = 0; cid < data->A->getChildrenSize(); ++cid) {
        ChildAutomaton* child = data->A->getChild(cid);
        auto* states = child->getStates();
        if (!states) continue;

        for (unsigned int sid = 0; sid < states->size(); ++sid) {
            if (!states->at(sid)->getFinal()) continue;

            const unsigned int idx = data->cumulative_size[cid] + sid;

            // forbidden: reached final but status cannot terminate
            if ((data->new_0_1[idx] & active_bit) || (data->new_1_0[idx] & active_bit)) {
                return false;
            }

            // allowed-terminate: drop immediately (also clears any tracked bit)
            data->new_0_0[idx] = 0u;
            data->new_1_1[idx] = 0u;
        }
    }
    return true;
}

static void explore_global_finalization_min_max(data_min_max_t* data) {
    // IMPORTANT: must handle finals in NEW arrays before packing/acceptance
    if (!cleanup_new_on_finals_min_max(data)) {
        explore_global_failure_min_max(data);
        return;
    }

    // Build vectors from NEW arrays
    std::vector<unsigned int> to_0_0(data->children_all);
    std::vector<unsigned int> to_0_1(data->children_all);
    std::vector<unsigned int> to_1_0(data->children_all);
    std::vector<unsigned int> to_1_1(data->children_all);
    for (unsigned int i = 0; i < data->children_all; ++i) {
        to_0_0[i] = data->new_0_0[i] & 3u;
        to_0_1[i] = data->new_0_1[i] & 3u;
        to_1_0[i] = data->new_1_0[i] & 3u;
        to_1_1[i] = data->new_1_1[i] & 3u;
    }

    bool global_final = false;

    // Check if any child is tracked (bit 1 set in any of the four arrays)
    bool any_tracked = false;
    for (unsigned int i = 0; i < data->children_all && !any_tracked; ++i) {
        if ((to_0_0[i] | to_0_1[i] | to_1_0[i] | to_1_1[i]) & 2u) {
            any_tracked = true;
        }
    }

    // if no tracked children and master saw a non-silent since last completion:
    // emit a one-step "final pulse" (2). This forces infinitely many non-silent segments
    // to visit final states infinitely often.
    if (!any_tracked && data->master_tracking_to == 0u) {
        data->master_tracking_to = 2u; // final pulse
        global_final = data->A->getStates()->at(data->master_state_id_to)->getFinal();
    }

    // Build destination state string.
    std::string global_to;
    global_to.reserve(96 + data->children_all * 8);
    global_to.append(std::to_string(data->master_state_id_to));
    global_to.push_back('/');
    global_to.append(std::to_string(data->master_tracking_to));
    global_to.push_back('/');
    global_to.append(vec_to_string(to_0_0));
    global_to.push_back('/');
    global_to.append(vec_to_string(to_0_1));
    global_to.push_back('/');
    global_to.append(vec_to_string(to_1_0));
    global_to.push_back('/');
    global_to.append(vec_to_string(to_1_1));

    if (global_final) {
        data->parser->final_states.insert(global_to);
    }

    data->parser->edges.insert({
        { data->symbol->getName(), data->global_edge_weight },
        { data->global_from, global_to }
    });

    // Iterative DFS: push to worklist instead of recursive call
    if (!data->parser->states.contains(global_to)) {
        data->parser->states.insert(global_to);
        data->worklist->push_back({
            global_to,
            data->master_state_id_to,
            data->master_tracking_to,
            std::move(to_0_0),
            std::move(to_0_1),
            std::move(to_1_0),
            std::move(to_1_1)
        });
    }
}

static void explore_global_selection_case_1_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_1_1[i] == 0u || data->old_1_1[i] == 2u) {
                explore_global_selection_case_1_1_min_max(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                explore_global_selection_case_1_1_min_max(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_1_1 = data->new_1_1[ii];
                const unsigned int stored_1_0 = data->new_1_0[ii];

                if (beyond_threshold_min_max(edge->getWeight()->getValue(), true, true, data)) {
                    // stay 1_1
                    data->new_1_1[ii] = data->old_1_1[i]; // preserves 1 vs 3
                    explore_global_selection_case_1_1_min_max(child_id + 1, 0, data);
                    data->new_1_1[ii] = stored_1_1;
                } else {
                    // become 1_0
                    data->new_1_0[ii] = data->old_1_1[i];
                    explore_global_selection_case_1_1_min_max(child_id + 1, 0, data);
                    data->new_1_0[ii] = stored_1_0;
                }
            }
        } else {
            explore_global_selection_case_1_1_min_max(child_id + 1, 0, data);
        }
    } else {
        explore_global_finalization_min_max(data);
    }
}

static void explore_global_selection_case_1_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_1_0[i] == 0u || data->old_1_0[i] == 2u) {
                explore_global_selection_case_1_0_min_max(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                // 1_0 cannot terminate
                explore_global_failure_min_max(data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_1_0 = data->new_1_0[ii];
                const unsigned int stored_1_1 = data->new_1_1[ii];

                // current bit is 0 here
                const bool ok = beyond_threshold_min_max(edge->getWeight()->getValue(), true, false, data);
                if (!ok) {
                    // stay 1_0
                    data->new_1_0[ii] = data->old_1_0[i];
                    explore_global_selection_case_1_0_min_max(child_id + 1, 0, data);
                    data->new_1_0[ii] = stored_1_0;
                } else {
                    // become 1_1
                    data->new_1_1[ii] = data->old_1_0[i];
                    explore_global_selection_case_1_0_min_max(child_id + 1, 0, data);
                    data->new_1_1[ii] = stored_1_1;
                }
            }
        } else {
            explore_global_selection_case_1_0_min_max(child_id + 1, 0, data);
        }
    } else {
        explore_global_selection_case_1_1_min_max(0, 0, data);
    }
}

static void explore_global_selection_case_0_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_0_1[i] == 0u || data->old_0_1[i] == 2u) {
                explore_global_selection_case_0_1_min_max(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                // 0_1 cannot terminate
                explore_global_failure_min_max(data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_0_1 = data->new_0_1[ii];
                const unsigned int stored_0_0 = data->new_0_0[ii];

                // current bit is 1 here
                const bool ok = beyond_threshold_min_max(edge->getWeight()->getValue(), false, true, data);
                if (ok) {
                    // stay 0_1
                    data->new_0_1[ii] = data->old_0_1[i];
                    explore_global_selection_case_0_1_min_max(child_id + 1, 0, data);
                    data->new_0_1[ii] = stored_0_1;
                } else {
                    // become 0_0
                    data->new_0_0[ii] = data->old_0_1[i];
                    explore_global_selection_case_0_1_min_max(child_id + 1, 0, data);
                    data->new_0_0[ii] = stored_0_0;
                }
            }
        } else {
            explore_global_selection_case_0_1_min_max(child_id + 1, 0, data);
        }
    } else {
        explore_global_selection_case_1_0_min_max(0, 0, data);
    }
}

static void explore_global_selection_case_0_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_0_0[i] == 0u || data->old_0_0[i] == 2u) {
                explore_global_selection_case_0_0_min_max(child_id, child_state_id + 1, data);
                return;
            }
            if (states->at(child_state_id)->getFinal()) {
                // 0_0 can terminate
                explore_global_selection_case_0_0_min_max(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (!succs) {
                explore_global_failure_min_max(data);
                return;
            }

            for (Edge* edge : *succs) {
                const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

                const unsigned int stored_0_0 = data->new_0_0[ii];
                const unsigned int stored_0_1 = data->new_0_1[ii];

                // current bit is 0 here
                const bool ok = beyond_threshold_min_max(edge->getWeight()->getValue(), false, false, data);
                if (!ok) {
                    // stay 0_0
                    data->new_0_0[ii] = data->old_0_0[i];
                    explore_global_selection_case_0_0_min_max(child_id + 1, 0, data);
                    data->new_0_0[ii] = stored_0_0;
                } else {
                    // become 0_1
                    data->new_0_1[ii] = data->old_0_0[i];
                    explore_global_selection_case_0_0_min_max(child_id + 1, 0, data);
                    data->new_0_1[ii] = stored_0_1;
                }
            }
        } else {
            explore_global_selection_case_0_0_min_max(child_id + 1, 0, data);
        }
    } else {
        explore_global_selection_case_0_1_min_max(0, 0, data);
    }
}


static void explore_global_master_transition_min_max(data_min_max_t* data) {
    auto* succs = data->A->getStates()->at(data->master_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    auto clear_new = [&]() {
        std::fill(data->new_0_0.begin(), data->new_0_0.end(), 0u);
        std::fill(data->new_0_1.begin(), data->new_0_1.end(), 0u);
        std::fill(data->new_1_0.begin(), data->new_1_0.end(), 0u);
        std::fill(data->new_1_1.begin(), data->new_1_1.end(), 0u);
    };

    auto activate_tracked = [](unsigned int& cell) {
        cell = 3u; // force active+tracked
    };

    for (Edge* edge : *succs) {
        data->master_state_id_to = (unsigned int)edge->getTo()->getId();

        const unsigned int child_id =
            (unsigned int)edge->getWeight()->getValue().to_uint();

        // // Default: preserve master tracking unless we take non-silent.
        // data->master_tracking_to = data->master_tracking_from;

        // Default: preserve tracking, but make the "final pulse" (2) last only one step.
        data->master_tracking_to = (data->master_tracking_from == 2u) ? 1u : data->master_tracking_from;

        clear_new();

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // silent: identity element of the OUTER aggregator
            data->global_edge_weight = (data->inf_or_sup == 0u) ? weight_t(1) : weight_t(0);
            explore_global_selection_min_max(0, 0, data);
            continue;
        }

        // non-silent
        data->master_tracking_to = 0u;

        const unsigned int summoned_child_state_id = (unsigned int)data->A->getChild(child_id)->initial->getId();
        const unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

        // Save old cells we might mutate for spawning.
        const unsigned int saved_0_0 = data->old_0_0[ii];
        const unsigned int saved_0_1 = data->old_0_1[ii];
        const unsigned int saved_1_0 = data->old_1_0[ii];
        const unsigned int saved_1_1 = data->old_1_1[ii];

        // Birth status depends on FINITE aggregator:
        //   Max_f: current starts 0  -> *_0
        //   Min_f: current starts 1  -> *_1
        if (data->finite_is_max) {
            // objective 0 -> 0_0 (edge weight 0)
            activate_tracked(data->old_0_0[ii]);
            data->global_edge_weight = weight_t(0);
            explore_global_selection_min_max(0, 0, data);
            data->old_0_0[ii] = saved_0_0;

            // objective 1 -> 1_0 (edge weight 1)
            activate_tracked(data->old_1_0[ii]);
            data->global_edge_weight = weight_t(1);
            explore_global_selection_min_max(0, 0, data);
            data->old_1_0[ii] = saved_1_0;
        } else {
            // objective 0 -> 0_1 (edge weight 0)
            activate_tracked(data->old_0_1[ii]);
            data->global_edge_weight = weight_t(0);
            explore_global_selection_min_max(0, 0, data);
            data->old_0_1[ii] = saved_0_1;

            // objective 1 -> 1_1 (edge weight 1)
            activate_tracked(data->old_1_1[ii]);
            data->global_edge_weight = weight_t(1);
            explore_global_selection_min_max(0, 0, data);
            data->old_1_1[ii] = saved_1_1;
        }

        // Restore any untouched cells too
        data->old_0_0[ii] = saved_0_0;
        data->old_0_1[ii] = saved_0_1;
        data->old_1_0[ii] = saved_1_0;
        data->old_1_1[ii] = saved_1_1;
    }
}

static void explore_global_initialization_min_max(data_min_max_t* data) {
    // Ensure storage exists
    const unsigned int n = data->children_all;

    data->old_0_0.resize(n);
    data->old_0_1.resize(n);
    data->old_1_0.resize(n);
    data->old_1_1.resize(n);

    data->new_0_0.assign(n, 0u);
    data->new_0_1.assign(n, 0u);
    data->new_1_0.assign(n, 0u);
    data->new_1_1.assign(n, 0u);

    // Direct copy from vectors
    for (unsigned int i = 0; i < n; ++i) {
        data->old_0_0[i] = data->from_0_0[i];
        data->old_0_1[i] = data->from_0_1[i];
        data->old_1_0[i] = data->from_1_0[i];
        data->old_1_1[i] = data->from_1_1[i];
    }

    auto* alphabet = data->A->getStates()->at(data->master_state_id_from)->getAlphabet();
    if (!alphabet) return;

    for (Symbol* symbol : *alphabet) {
        data->symbol = symbol;
        explore_global_master_transition_min_max(data);
    }
}

bool NestedAutomaton::emptiness_monotonic_nesting_min_max(value_function_t infinite_aggregator,
                                                         value_function_t finite_aggregator,
                                                         weight_t threshold) {
    // Decide outer inf_or_sup by comparing function pointers
    unsigned int inf_or_sup;
    if (infinite_aggregator == Inf || infinite_aggregator == LimInf) {
        inf_or_sup = 0u;
    } else if (infinite_aggregator == Sup || infinite_aggregator == LimSup) {
        inf_or_sup = 1u;
    } else {
        QUAK_FAIL("bad infinite_aggregator for min_max construction");
    }

    // Decide finite_is_max
    unsigned int finite_is_max;
    if (finite_aggregator == Max_f) {
        finite_is_max = 1u;
    } else if (finite_aggregator == Min_f) {
        finite_is_max = 0u;
    } else {
        QUAK_FAIL("bad finite_aggregator for min_max construction");
    }

    // Build cumulative_size as vector
    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->getChildrenSize() + 1; ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int children_all = cumulative_size[this->getChildrenSize()];

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);

    parser->states.insert("@sink@");

    // Install sink self-loops on all symbols of the master alphabet.
    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        parser->edges.insert({
            { symbol->getName(), weight_t(0) },
            { "@sink@", "@sink@" }
        });
    }

    // Helper to create zero vector string
    std::string zero_vec_str = vec_to_string(std::vector<unsigned int>(children_all, 0u));

    // GLOBAL INITIAL
    // encoding: master_state_id/master_tracking/from_0_0/from_0_1/from_1_0/from_1_1
    std::string global_initial;
    global_initial.reserve(96 + children_all * 8);
    global_initial.append(std::to_string(this->initial->getId()));
    global_initial.push_back('/');
    global_initial.append(std::to_string(1u));   // master_tracking initially "waiting"
    global_initial.push_back('/');
    global_initial.append(zero_vec_str);
    global_initial.push_back('/');
    global_initial.append(zero_vec_str);
    global_initial.push_back('/');
    global_initial.append(zero_vec_str);
    global_initial.push_back('/');
    global_initial.append(zero_vec_str);

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    // Iterative DFS using worklist
    std::vector<min_max_work_item> worklist;
    worklist.push_back({
        global_initial,
        (unsigned int)this->initial->getId(),
        1u,
        std::vector<unsigned int>(children_all, 0u),
        std::vector<unsigned int>(children_all, 0u),
        std::vector<unsigned int>(children_all, 0u),
        std::vector<unsigned int>(children_all, 0u)
    });

    data_min_max_t data{};
    data.A = this;
    data.parser = parser;
    data.threshold = threshold;
    data.cumulative_size = cumulative_size.data();
    data.children_all = children_all;
    data.inf_or_sup = inf_or_sup;
    data.finite_is_max = finite_is_max;
    data.worklist = &worklist;

    while (!worklist.empty()) {
        min_max_work_item item = std::move(worklist.back());
        worklist.pop_back();

        data.global_from = std::move(item.global_from);
        data.master_state_id_from = item.master_state_id_from;
        data.master_tracking_from = item.master_tracking_from;
        data.from_0_0 = std::move(item.from_0_0);
        data.from_0_1 = std::move(item.from_0_1);
        data.from_1_0 = std::move(item.from_1_0);
        data.from_1_1 = std::move(item.from_1_1);

        explore_global_initialization_min_max(&data);
    }

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    std::cout << "unnested: " << unnested->getStates()->size() << " states, " << unnested->getNbTransitions() << " edges, " << unnested->getNbSCCs() << " SCCs (" << unnested->getNbAcceptingSCCs() << " accepting)" << std::endl;
    // unnested->print();

    weight_t top = unnested->compute_top_with_final(infinite_aggregator);
    bool result = (top == 1);

    delete unnested;
    return result;
}










// typedef struct global_exploration_data_min_max {
//     // constraints
//     NestedAutomaton* A;
//     Parser* parser;
//     unsigned int check_tracking;   // mask used on packed base-4 encodings
//     weight_t threshold;
//     unsigned int* cumulative_size; // prefix sums for flattening (child_id, local_state) -> global index
//     unsigned int children_all;
//     unsigned int inf_or_sup;       // 0 = inf-type outer, 1 = sup-type outer
//     unsigned int finite_is_max;    // 0 = Min_f, 1 = Max_f

//     // given (input for current exploration frame)
//     std::string global_from;
//     unsigned int master_state_id_from;
//     unsigned int master_tracking_from;
//     unsigned int from_0_0;
//     unsigned int from_0_1;
//     unsigned int from_1_0;
//     unsigned int from_1_1;

//     // initialized per-symbol
//     Symbol* symbol = nullptr;
//     std::vector<unsigned int> old_0_0, old_0_1, old_1_0, old_1_1;
//     std::vector<unsigned int> new_0_0, new_0_1, new_1_0, new_1_1;

//     // computed (output accumulators)
//     unsigned int master_state_id_to = 0;
//     unsigned int master_tracking_to = 0;
//     weight_t global_edge_weight = 0;
// } data_min_max_t;

// static void explore_global_initialization_min_max(data_min_max_t* data);
// static void explore_global_master_transition_min_max(data_min_max_t* data);
// static void explore_global_finalization_min_max(data_min_max_t* data);
// static void explore_global_failure_min_max(data_min_max_t* data);

// static void explore_global_selection_case_0_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);
// static void explore_global_selection_case_0_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);
// static void explore_global_selection_case_1_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);
// static void explore_global_selection_case_1_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data);

// static void explore_global_selection_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
//     explore_global_selection_case_0_0_min_max(child_id, child_state_id, data);
// }

// static void explore_global_failure_min_max(data_min_max_t* data) {
//     data->parser->edges.insert({
//         { data->symbol->getName(), weight_t(0) },
//         { data->global_from, "@sink@" }
//     });
// }

// /*
//   Status x_y:
//     x = objective bit (0: want final outcome 0, 1: want final outcome 1)  -> birth guess -> edge weight
//     y = current bit (monotone progress w.r.t. threshold, depends on finite aggregator):
//         - Max_f: y starts 0 and can flip 0->1 when seeing edge>=threshold
//         - Min_f: y starts 1 and can flip 1->0 when seeing edge<threshold

//   The helper below returns "ok" in the SAME way your four cases currently use it:
//     - in *_0 cases: ok means "flip to *_1"
//     - in *_1 cases: ok means "stay in *_1"
// */
// static bool beyond_threshold_min_max(const weight_t& edge_value,
//                                      bool /*guessed_weight_unused*/,
//                                      bool current_is_1,
//                                      const data_min_max_t* data) {
//     const bool pass = !(edge_value < data->threshold); // edge_value >= threshold

//     if (data->finite_is_max) {
//         // Max_f: y' = y OR pass
//         if (!current_is_1) return pass; // 0 -> 1 iff pass
//         return true;                    // 1 stays 1
//     } else {
//         // Min_f: y' = y AND pass
//         if (!current_is_1) return false; // 0 stays 0
//         return pass;                     // 1 stays 1 iff pass, else flips to 0
//     }
// }

// /*
//   enforce "final-state handling in the same symbol" on the NEW arrays.
//   - If a child is in a final state with status 0_0 or 1_1: it MUST terminate now -> drop it from new_*.
//   - If a child is in a final state with status 0_1 or 1_0: it would be forced to terminate but cannot -> branch dies.
// */
// static bool cleanup_new_on_finals_min_max(data_min_max_t* data) {
//     const unsigned int active_bit = 1u;

//     for (unsigned int cid = 0; cid < data->A->getChildrenSize(); ++cid) {
//         ChildAutomaton* child = data->A->getChild(cid);
//         auto* states = child->getStates();
//         if (!states) continue;

//         for (unsigned int sid = 0; sid < states->size(); ++sid) {
//             if (!states->at(sid)->getFinal()) continue;

//             const unsigned int idx = data->cumulative_size[cid] + sid;

//             // forbidden: reached final but status cannot terminate
//             if ((data->new_0_1[idx] & active_bit) || (data->new_1_0[idx] & active_bit)) {
//                 return false;
//             }

//             // allowed-terminate: drop immediately (also clears any tracked bit)
//             data->new_0_0[idx] = 0u;
//             data->new_1_1[idx] = 0u;
//         }
//     }
//     return true;
// }

// static void explore_global_finalization_min_max(data_min_max_t* data) {
//     // IMPORTANT: must handle finals in NEW arrays before packing/acceptance
//     if (!cleanup_new_on_finals_min_max(data)) {
//         explore_global_failure_min_max(data);
//         return;
//     }

//     // pack base-4 digits from NEW arrays
//     unsigned int to_0_0 = 0, to_0_1 = 0, to_1_0 = 0, to_1_1 = 0;

//     for (int i = (int)data->children_all - 1; i >= 0; --i) {
//         to_0_0 = (to_0_0 << 2) + (data->new_0_0[i] & 3u);
//         to_0_1 = (to_0_1 << 2) + (data->new_0_1[i] & 3u);
//         to_1_0 = (to_1_0 << 2) + (data->new_1_0[i] & 3u);
//         to_1_1 = (to_1_1 << 2) + (data->new_1_1[i] & 3u);
//     }

//     bool global_final = false;

//     const unsigned int any_tracked_masked =
//         (to_0_0 | to_0_1 | to_1_0 | to_1_1) & data->check_tracking;

//     // if no tracked children and master saw a non-silent since last completion:
//     // emit a one-step "final pulse" (2). This forces infinitely many non-silent segments
//     // to visit final states infinitely often.
//     if (any_tracked_masked == 0u && data->master_tracking_to == 0u) {
//         data->master_tracking_to = 2u; // final pulse
//         global_final = data->A->getStates()->at(data->master_state_id_to)->getFinal();
//     }

//     // Build destination state string.
//     std::string global_to;
//     global_to.reserve(96);
//     global_to.append(std::to_string(data->master_state_id_to));
//     global_to.push_back('/');
//     global_to.append(std::to_string(data->master_tracking_to));
//     global_to.push_back('/');
//     global_to.append(std::to_string(to_0_0));
//     global_to.push_back('/');
//     global_to.append(std::to_string(to_0_1));
//     global_to.push_back('/');
//     global_to.append(std::to_string(to_1_0));
//     global_to.push_back('/');
//     global_to.append(std::to_string(to_1_1));

//     if (global_final) {
//         data->parser->final_states.insert(global_to);
//     }

//     data->parser->edges.insert({
//         { data->symbol->getName(), data->global_edge_weight },
//         { data->global_from, global_to }
//     });

//     // DFS only on newly discovered states
//     if (!data->parser->states.contains(global_to)) {
//         data->parser->states.insert(global_to);

//         data_min_max_t data_deeper{};
//         // constraints
//         data_deeper.A = data->A;
//         data_deeper.parser = data->parser;
//         data_deeper.check_tracking = data->check_tracking;
//         data_deeper.threshold = data->threshold;
//         data_deeper.cumulative_size = data->cumulative_size;
//         data_deeper.children_all = data->children_all;
//         data_deeper.inf_or_sup = data->inf_or_sup;
//         data_deeper.finite_is_max = data->finite_is_max;

//         // given
//         data_deeper.global_from = global_to;
//         data_deeper.master_state_id_from = data->master_state_id_to;
//         data_deeper.master_tracking_from = data->master_tracking_to;
//         data_deeper.from_0_0 = to_0_0;
//         data_deeper.from_0_1 = to_0_1;
//         data_deeper.from_1_0 = to_1_0;
//         data_deeper.from_1_1 = to_1_1;

//         explore_global_initialization_min_max(&data_deeper);
//     }
// }

// static void explore_global_selection_case_1_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
//     if (child_id < data->A->getChildrenSize()) {
//         ChildAutomaton* child = data->A->getChild(child_id);
//         auto* states = child->getStates();

//         if (child_state_id < states->size()) {
//             const unsigned int i = data->cumulative_size[child_id] + child_state_id;

//             if (data->old_1_1[i] == 0u || data->old_1_1[i] == 2u) {
//                 explore_global_selection_case_1_1_min_max(child_id, child_state_id + 1, data);
//                 return;
//             }
//             if (states->at(child_state_id)->getFinal()) {
//                 explore_global_selection_case_1_1_min_max(child_id, child_state_id + 1, data);
//                 return;
//             }

//             State* child_state = states->at(child_state_id);
//             auto* succs = child_state->getSuccessors(data->symbol->getId());
//             if (!succs) {
//                 explore_global_failure_min_max(data);
//                 return;
//             }

//             for (Edge* edge : *succs) {
//                 const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

//                 const unsigned int stored_1_1 = data->new_1_1[ii];
//                 const unsigned int stored_1_0 = data->new_1_0[ii];

//                 if (beyond_threshold_min_max(edge->getWeight()->getValue(), true, true, data)) {
//                     // stay 1_1
//                     data->new_1_1[ii] = data->old_1_1[i]; // preserves 1 vs 3
//                     explore_global_selection_case_1_1_min_max(child_id + 1, 0, data);
//                     data->new_1_1[ii] = stored_1_1;
//                 } else {
//                     // become 1_0
//                     data->new_1_0[ii] = data->old_1_1[i];
//                     explore_global_selection_case_1_1_min_max(child_id + 1, 0, data);
//                     data->new_1_0[ii] = stored_1_0;
//                 }
//             }
//         } else {
//             explore_global_selection_case_1_1_min_max(child_id + 1, 0, data);
//         }
//     } else {
//         explore_global_finalization_min_max(data);
//     }
// }

// static void explore_global_selection_case_1_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
//     if (child_id < data->A->getChildrenSize()) {
//         ChildAutomaton* child = data->A->getChild(child_id);
//         auto* states = child->getStates();

//         if (child_state_id < states->size()) {
//             const unsigned int i = data->cumulative_size[child_id] + child_state_id;

//             if (data->old_1_0[i] == 0u || data->old_1_0[i] == 2u) {
//                 explore_global_selection_case_1_0_min_max(child_id, child_state_id + 1, data);
//                 return;
//             }
//             if (states->at(child_state_id)->getFinal()) {
//                 // 1_0 cannot terminate
//                 explore_global_failure_min_max(data);
//                 return;
//             }

//             State* child_state = states->at(child_state_id);
//             auto* succs = child_state->getSuccessors(data->symbol->getId());
//             if (!succs) {
//                 explore_global_failure_min_max(data);
//                 return;
//             }

//             for (Edge* edge : *succs) {
//                 const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

//                 const unsigned int stored_1_0 = data->new_1_0[ii];
//                 const unsigned int stored_1_1 = data->new_1_1[ii];

//                 // current bit is 0 here
//                 const bool ok = beyond_threshold_min_max(edge->getWeight()->getValue(), true, false, data);
//                 if (!ok) {
//                     // stay 1_0
//                     data->new_1_0[ii] = data->old_1_0[i];
//                     explore_global_selection_case_1_0_min_max(child_id + 1, 0, data);
//                     data->new_1_0[ii] = stored_1_0;
//                 } else {
//                     // become 1_1
//                     data->new_1_1[ii] = data->old_1_0[i];
//                     explore_global_selection_case_1_0_min_max(child_id + 1, 0, data);
//                     data->new_1_1[ii] = stored_1_1;
//                 }
//             }
//         } else {
//             explore_global_selection_case_1_0_min_max(child_id + 1, 0, data);
//         }
//     } else {
//         explore_global_selection_case_1_1_min_max(0, 0, data);
//     }
// }

// static void explore_global_selection_case_0_1_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
//     if (child_id < data->A->getChildrenSize()) {
//         ChildAutomaton* child = data->A->getChild(child_id);
//         auto* states = child->getStates();

//         if (child_state_id < states->size()) {
//             const unsigned int i = data->cumulative_size[child_id] + child_state_id;

//             if (data->old_0_1[i] == 0u || data->old_0_1[i] == 2u) {
//                 explore_global_selection_case_0_1_min_max(child_id, child_state_id + 1, data);
//                 return;
//             }
//             if (states->at(child_state_id)->getFinal()) {
//                 // 0_1 cannot terminate
//                 explore_global_failure_min_max(data);
//                 return;
//             }

//             State* child_state = states->at(child_state_id);
//             auto* succs = child_state->getSuccessors(data->symbol->getId());
//             if (!succs) {
//                 explore_global_failure_min_max(data);
//                 return;
//             }

//             for (Edge* edge : *succs) {
//                 const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

//                 const unsigned int stored_0_1 = data->new_0_1[ii];
//                 const unsigned int stored_0_0 = data->new_0_0[ii];

//                 // current bit is 1 here
//                 const bool ok = beyond_threshold_min_max(edge->getWeight()->getValue(), false, true, data);
//                 if (ok) {
//                     // stay 0_1
//                     data->new_0_1[ii] = data->old_0_1[i];
//                     explore_global_selection_case_0_1_min_max(child_id + 1, 0, data);
//                     data->new_0_1[ii] = stored_0_1;
//                 } else {
//                     // become 0_0
//                     data->new_0_0[ii] = data->old_0_1[i];
//                     explore_global_selection_case_0_1_min_max(child_id + 1, 0, data);
//                     data->new_0_0[ii] = stored_0_0;
//                 }
//             }
//         } else {
//             explore_global_selection_case_0_1_min_max(child_id + 1, 0, data);
//         }
//     } else {
//         explore_global_selection_case_1_0_min_max(0, 0, data);
//     }
// }

// static void explore_global_selection_case_0_0_min_max(unsigned int child_id, unsigned int child_state_id, data_min_max_t* data) {
//     if (child_id < data->A->getChildrenSize()) {
//         ChildAutomaton* child = data->A->getChild(child_id);
//         auto* states = child->getStates();

//         if (child_state_id < states->size()) {
//             const unsigned int i = data->cumulative_size[child_id] + child_state_id;

//             if (data->old_0_0[i] == 0u || data->old_0_0[i] == 2u) {
//                 explore_global_selection_case_0_0_min_max(child_id, child_state_id + 1, data);
//                 return;
//             }
//             if (states->at(child_state_id)->getFinal()) {
//                 // 0_0 can terminate
//                 explore_global_selection_case_0_0_min_max(child_id, child_state_id + 1, data);
//                 return;
//             }

//             State* child_state = states->at(child_state_id);
//             auto* succs = child_state->getSuccessors(data->symbol->getId());
//             if (!succs) {
//                 explore_global_failure_min_max(data);
//                 return;
//             }

//             for (Edge* edge : *succs) {
//                 const unsigned int ii = data->cumulative_size[child_id] + (unsigned int)edge->getTo()->getId();

//                 const unsigned int stored_0_0 = data->new_0_0[ii];
//                 const unsigned int stored_0_1 = data->new_0_1[ii];

//                 // current bit is 0 here
//                 const bool ok = beyond_threshold_min_max(edge->getWeight()->getValue(), false, false, data);
//                 if (!ok) {
//                     // stay 0_0
//                     data->new_0_0[ii] = data->old_0_0[i];
//                     explore_global_selection_case_0_0_min_max(child_id + 1, 0, data);
//                     data->new_0_0[ii] = stored_0_0;
//                 } else {
//                     // become 0_1
//                     data->new_0_1[ii] = data->old_0_0[i];
//                     explore_global_selection_case_0_0_min_max(child_id + 1, 0, data);
//                     data->new_0_1[ii] = stored_0_1;
//                 }
//             }
//         } else {
//             explore_global_selection_case_0_0_min_max(child_id + 1, 0, data);
//         }
//     } else {
//         explore_global_selection_case_0_1_min_max(0, 0, data);
//     }
// }


// static void explore_global_master_transition_min_max(data_min_max_t* data) {
//     auto* succs = data->A->getStates()->at(data->master_state_id_from)->getSuccessors(data->symbol->getId());
//     if (!succs) return;

//     auto clear_new = [&]() {
//         std::fill(data->new_0_0.begin(), data->new_0_0.end(), 0u);
//         std::fill(data->new_0_1.begin(), data->new_0_1.end(), 0u);
//         std::fill(data->new_1_0.begin(), data->new_1_0.end(), 0u);
//         std::fill(data->new_1_1.begin(), data->new_1_1.end(), 0u);
//     };

//     auto activate_tracked = [](unsigned int& cell) {
//         cell = 3u; // force active+tracked
//     };

//     for (Edge* edge : *succs) {
//         data->master_state_id_to = (unsigned int)edge->getTo()->getId();

//         const unsigned int child_id =
//             (unsigned int)edge->getWeight()->getValue().to_uint();

//         // // Default: preserve master tracking unless we take non-silent.
//         // data->master_tracking_to = data->master_tracking_from;

//         // Default: preserve tracking, but make the "final pulse" (2) last only one step.
//         data->master_tracking_to = (data->master_tracking_from == 2u) ? 1u : data->master_tracking_from;

//         clear_new();

//         if (data->A->getChild(child_id)->getStates()->size() == 1) {
//             // silent: identity element of the OUTER aggregator
//             data->global_edge_weight = (data->inf_or_sup == 0u) ? weight_t(1) : weight_t(0);
//             explore_global_selection_min_max(0, 0, data);
//             continue;
//         }

//         // non-silent
//         data->master_tracking_to = 0u;

//         const unsigned int summoned_child_state_id = (unsigned int)data->A->getChild(child_id)->initial->getId();
//         const unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

//         // Save old cells we might mutate for spawning.
//         const unsigned int saved_0_0 = data->old_0_0[ii];
//         const unsigned int saved_0_1 = data->old_0_1[ii];
//         const unsigned int saved_1_0 = data->old_1_0[ii];
//         const unsigned int saved_1_1 = data->old_1_1[ii];

//         // Birth status depends on FINITE aggregator:
//         //   Max_f: current starts 0  -> *_0
//         //   Min_f: current starts 1  -> *_1
//         if (data->finite_is_max) {
//             // objective 0 -> 0_0 (edge weight 0)
//             activate_tracked(data->old_0_0[ii]);
//             data->global_edge_weight = weight_t(0);
//             explore_global_selection_min_max(0, 0, data);
//             data->old_0_0[ii] = saved_0_0;

//             // objective 1 -> 1_0 (edge weight 1)
//             activate_tracked(data->old_1_0[ii]);
//             data->global_edge_weight = weight_t(1);
//             explore_global_selection_min_max(0, 0, data);
//             data->old_1_0[ii] = saved_1_0;
//         } else {
//             // objective 0 -> 0_1 (edge weight 0)
//             activate_tracked(data->old_0_1[ii]);
//             data->global_edge_weight = weight_t(0);
//             explore_global_selection_min_max(0, 0, data);
//             data->old_0_1[ii] = saved_0_1;

//             // objective 1 -> 1_1 (edge weight 1)
//             activate_tracked(data->old_1_1[ii]);
//             data->global_edge_weight = weight_t(1);
//             explore_global_selection_min_max(0, 0, data);
//             data->old_1_1[ii] = saved_1_1;
//         }

//         // Restore any untouched cells too
//         data->old_0_0[ii] = saved_0_0;
//         data->old_0_1[ii] = saved_0_1;
//         data->old_1_0[ii] = saved_1_0;
//         data->old_1_1[ii] = saved_1_1;
//     }
// }

// static void explore_global_initialization_min_max(data_min_max_t* data) {
//     // Ensure storage exists
//     const unsigned int n = data->children_all;

//     data->old_0_0.resize(n);
//     data->old_0_1.resize(n);
//     data->old_1_0.resize(n);
//     data->old_1_1.resize(n);

//     data->new_0_0.assign(n, 0u);
//     data->new_0_1.assign(n, 0u);
//     data->new_1_0.assign(n, 0u);
//     data->new_1_1.assign(n, 0u);

//     unsigned int from_0_0 = data->from_0_0;
//     unsigned int from_0_1 = data->from_0_1;
//     unsigned int from_1_0 = data->from_1_0;
//     unsigned int from_1_1 = data->from_1_1;

//     for (unsigned int i = 0; i < n; ++i) {
//         data->old_0_0[i] = from_0_0 & 3u; from_0_0 >>= 2;
//         data->old_0_1[i] = from_0_1 & 3u; from_0_1 >>= 2;
//         data->old_1_0[i] = from_1_0 & 3u; from_1_0 >>= 2;
//         data->old_1_1[i] = from_1_1 & 3u; from_1_1 >>= 2;
//     }

//     auto* alphabet = data->A->getStates()->at(data->master_state_id_from)->getAlphabet();
//     if (!alphabet) return;

//     for (Symbol* symbol : *alphabet) {
//         data->symbol = symbol;
//         explore_global_master_transition_min_max(data);
//     }
// }

// bool NestedAutomaton::emptiness_monotonic_nesting_min_max(value_function_t infinite_aggregator,
//                                                          value_function_t finite_aggregator,
//                                                          weight_t threshold) {
//     // Decide outer inf_or_sup by comparing function pointers
//     unsigned int inf_or_sup;
//     if (infinite_aggregator == Inf || infinite_aggregator == LimInf) {
//         inf_or_sup = 0u;
//     } else if (infinite_aggregator == Sup || infinite_aggregator == LimSup) {
//         inf_or_sup = 1u;
//     } else {
//         QUAK_FAIL("bad infinite_aggregator for min_max construction");
//     }

//     // Decide finite_is_max (THIS WAS MISSING)
//     unsigned int finite_is_max;
//     if (finite_aggregator == Max_f) {
//         finite_is_max = 1u;
//     } else if (finite_aggregator == Min_f) {
//         finite_is_max = 0u;
//     } else {
//         QUAK_FAIL("bad finite_aggregator for min_max construction");
//     }

//     // Build cumulative_size as vector
//     std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
//     cumulative_size[0] = 0;
//     for (unsigned int i = 1; i < this->getChildrenSize() + 1; ++i) {
//         cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
//     }
//     const unsigned int children_all = cumulative_size[this->getChildrenSize()];

//     // Prevent silent overflow of packed base-4 encoding into unsigned int.
//     const unsigned int max_digits = (unsigned int)(sizeof(unsigned int) * 8 / 2);
//     if (children_all > max_digits) {
//         QUAK_FAIL("min_max: too many child states for packed base-4 encoding (would overflow unsigned int)");
//     }

//     // Build check_tracking mask (packed base-4 digits; digit bit-1 set => tracked).
//     unsigned int check_tracking = 0;
//     for (unsigned int i = 0; i < children_all; ++i) {
//         check_tracking = (check_tracking << 2) | 2u;
//     }

//     Parser* parser = new Parser(0, 1);
//     parser->weights.insert(0);
//     parser->weights.insert(1);

//     parser->states.insert("@sink@");

//     // Install sink self-loops on all symbols of the master alphabet.
//     for (Symbol* symbol : *this->getAlphabet()) {
//         parser->alphabet.insert(symbol->getName());
//         parser->edges.insert({
//             { symbol->getName(), weight_t(0) },
//             { "@sink@", "@sink@" }
//         });
//     }

//     // GLOBAL INITIAL
//     // encoding: master_state_id/master_tracking/from_0_0/from_0_1/from_1_0/from_1_1
//     std::string global_initial;
//     global_initial.reserve(96);
//     global_initial.append(std::to_string(this->initial->getId()));
//     global_initial.push_back('/');
//     global_initial.append(std::to_string(1u));   // master_tracking initially "waiting"
//     global_initial.push_back('/');
//     global_initial.append(std::to_string(0u));
//     global_initial.push_back('/');
//     global_initial.append(std::to_string(0u));
//     global_initial.push_back('/');
//     global_initial.append(std::to_string(0u));
//     global_initial.push_back('/');
//     global_initial.append(std::to_string(0u));

//     parser->states.insert(global_initial);
//     parser->initial = global_initial;

//     data_min_max_t data{};
//     data.A = this;
//     data.parser = parser;
//     data.check_tracking = check_tracking;
//     data.threshold = threshold;
//     data.cumulative_size = cumulative_size.data();
//     data.children_all = children_all;
//     data.inf_or_sup = inf_or_sup;
//     data.finite_is_max = finite_is_max;

//     data.global_from = global_initial;
//     data.master_state_id_from = (unsigned int)this->initial->getId();
//     data.master_tracking_from = 1u;
//     data.from_0_0 = 0u;
//     data.from_0_1 = 0u;
//     data.from_1_0 = 0u;
//     data.from_1_1 = 0u;

//     explore_global_initialization_min_max(&data);

//     std::string newname = "unnested(" + this->getName() + ")";
//     MapStd<std::string, Symbol*> sync_register;
//     Automaton* unnested = new Automaton(newname, parser, sync_register);
//     // unnested->print();
//     delete parser;

//     weight_t top = unnested->compute_top_with_final(infinite_aggregator);
//     bool result = (top == 1);

//     delete unnested;
//     return result;
// }

bool NestedAutomaton::isCompleteNested(std::vector<bool>* complete_flags) const {
    bool ret = true;

    if (complete_flags == nullptr) {
        if (!this->isComplete()) {
            return false;
        }

        for (unsigned int i = 0; i < this->getChildrenSize(); ++i) {
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
    if (finVal == SumPlus) {
        if (infVal == Sup || infVal == LimSup) {
            return this->emptiness_monotonic_nesting_supremum(infVal, finVal, x);
        }
        else if (infVal == Inf || infVal == LimInf) {
            return this->emptiness_monotonic_nesting(infVal, finVal, x);
        }
        else if (infVal == LimSupAvg) {
            return this->emptiness_Avg_SumPlus(x);
        }
        else {
            QUAK_FAIL("isNonEmpty: unsupported infinite aggregator with SumPlus");
        }
    }
    else if (finVal == SumMinus) {
        if (infVal == Sup || infVal == LimSup) {
            return this->emptiness_monotonic_nesting_supremum(infVal, finVal, x);
        }
        else if (infVal == Inf || infVal == LimInf) {
            return this->emptiness_monotonic_nesting(infVal, finVal, x);
        }
        else if (infVal == LimInfAvg || infVal == LimSupAvg) {
            NestedAutomaton* det_nwa = nullptr;
            NestedAutomaton* sync_nwa;
            std::vector<bool> complete_flags;
            unsigned long long c_bound;

            if (this->isDeterministicNested() && this->isCompleteNested(&complete_flags)) {
                c_bound = compute_c_bound(this);
                sync_nwa = this->synchronizeChildren();
            }
            else if (this->isDeterministicNested() && !this->isCompleteNested(&complete_flags)) {
                det_nwa = this->makeCompleteNested(&complete_flags);
                c_bound = compute_c_bound(this);
                sync_nwa = det_nwa->synchronizeChildren();
            }
            else {
                det_nwa = this->determinizeWithMacroAlphabet();
                c_bound = compute_c_bound(this);
                sync_nwa = det_nwa->synchronizeChildren();
            }
            // sync_nwa->print();
            Automaton* flat = sync_nwa->flatten_Avg_SumMinus(c_bound);
            // flat->print();
            Automaton* nonSilent = Automaton::removeSilentTransitions(flat, infVal, true);
            std::cout << "unnested: " << nonSilent->getStates()->size() << " states, " << nonSilent->getNbTransitions() << " edges, " << nonSilent->getNbSCCs() << " SCCs (" << nonSilent->getNbAcceptingSCCs() << " accepting)" << std::endl;
            // nonSilent->print();

            bool res = nonSilent->emptiness_LimAvg_with_final(x);
            delete flat;
            delete sync_nwa;
            delete det_nwa;
            return res;
        }
        else {
            QUAK_FAIL("isNonEmpty: unsupported infinite aggregator with SumMinus");
        }
    }
    else if (finVal == Max_f || finVal == Min_f) {
        if (infVal == Sup || infVal == LimSup) {
            return this->emptiness_monotonic_nesting_min_max_supremum(infVal, finVal, x);
        }
        else if (infVal == Inf || infVal == LimInf) {
            return this->emptiness_monotonic_nesting_min_max(infVal, finVal, x);
        }
        else if (infVal == LimInfAvg || infVal == LimSupAvg) {
            Automaton* flat = this->flatten_regular(finVal);
            Automaton* nonSilent = Automaton::removeSilentTransitions(flat, infVal, true);
            std::cout << "unnested: " << nonSilent->getStates()->size() << " states, " << nonSilent->getNbTransitions() << " edges, " << nonSilent->getNbSCCs() << " SCCs (" << nonSilent->getNbAcceptingSCCs() << " accepting)" << std::endl;
            
            bool res = nonSilent->emptiness_LimAvg_with_final(x);
            delete nonSilent;
            delete flat;
            return res;
        }
        else {
            QUAK_FAIL("isNonEmpty: unsupported infinite aggregator with Min_f/Max_f");
        }
    }
    else if (finVal == SumB) {
        Automaton* flat = this->flatten_regular(finVal, bound);
        bool withShortcuts = !(infVal == Inf || infVal == Sup);
        Automaton* nonSilent = Automaton::removeSilentTransitions(flat, infVal, withShortcuts);
        std::cout << "unnested: " << nonSilent->getStates()->size() << " states, " << nonSilent->getNbTransitions() << " edges, " << nonSilent->getNbSCCs() << " SCCs (" << nonSilent->getNbAcceptingSCCs() << " accepting)" << std::endl;
        
        if (infVal == LimInfAvg || infVal == LimSupAvg) {
            bool res = nonSilent->emptiness_LimAvg_with_final(x);
            delete nonSilent;
            delete flat;
            return res;
        }
        else if (infVal == Sup || infVal == LimSup || infVal == Inf || infVal == LimInf) {
            weight_t topFlat = nonSilent->compute_top_with_final(infVal);
            delete nonSilent;
            delete flat;
            return (topFlat >= x);
        }
        else {
            QUAK_FAIL("isNonEmpty: unsupported infinite aggregator with SumB");
        }
    }
    else {
        QUAK_FAIL("isNonEmpty: unsupported finite aggregator");
    }
}


bool NestedAutomaton::isUniversal(value_function_t infVal, value_function_t finVal, weight_t x, weight_t bound) {
    if (finVal == Max_f || finVal == Min_f || finVal == SumB) {
        if (infVal == Sup || infVal == LimSup || infVal == Inf || infVal == LimInf) {
            Automaton* flat = this->flatten_regular(finVal, bound);
            // flat->print();
            bool withShortcuts = !(infVal == Inf || infVal == Sup);
            Automaton* nonSilent = Automaton::removeSilentTransitions(flat, infVal, withShortcuts);
            std::cout << "unnested: " << nonSilent->getStates()->size() << " states, " << nonSilent->getNbTransitions() << " edges, " << nonSilent->getNbSCCs() << " SCCs (" << nonSilent->getNbAcceptingSCCs() << " accepting)" << std::endl;
            // nonSilent->print();

            bool res = nonSilent->isUniversal(infVal, x);
            delete nonSilent;
            delete flat;
            return res;
        }
        
        else {
            QUAK_FAIL("isUniversal: unsupported infinite aggregator");
        }
    }
    else if (finVal == SumPlus) {
        if (infVal == Sup || infVal == LimSup || infVal == Inf || infVal == LimInf) {
            return this->isUniversal(SumB, infVal, x, x);
        }
    }
    else if (finVal == SumMinus) {
        if (infVal == Sup || infVal == LimSup || infVal == Inf || infVal == LimInf) {
            return this->isUniversal(SumB, infVal, x, x - 1);
        } else {
            QUAK_FAIL("isUniversal: unsupported infinite aggregator");
        }
    }
    else {
        QUAK_FAIL("isUniversal: unsupported finite aggregator");
    }
}