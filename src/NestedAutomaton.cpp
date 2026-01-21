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
            // monitor->print();

            MonitorKey key = {i,j};
            monitors.insert(key, monitor);
            
            // Collect Q_S and F_S
            for (size_t s = 0; s < monitor->getStates()->size(); ++s) {
                Q_S.insert(monitor->getStates()->at(s));
            }
            for (State* s : *(monitor->getFinalStates())) {
                F_S.insert(s);
            }
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
static constexpr bool ACC_WAIT_MASTER  = 0; // waiting to see parent in accepting state
static constexpr bool ACC_WAIT_P2EMPTY = 1; // waiting to see P2 nonempty

static inline bool advance_acc_phase(const BuchiState_acceptance& s) {
    if (s.acceptance_flag == ACC_WAIT_MASTER) {
        return (s.parent_state->getFinal()) ? ACC_WAIT_P2EMPTY : ACC_WAIT_MASTER;
    } else {
        return (s.P2.size() == 0) ? ACC_WAIT_MASTER : ACC_WAIT_P2EMPTY;
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
    init_buchi.last_guess   = weight_t(INIT_BUCHI_VALUE);
    init_buchi.P1           = SetStd<State*>();
    init_buchi.P2           = SetStd<State*>();

    // start in phase "wait for master accept"
    init_buchi.acceptance_flag    = ACC_WAIT_MASTER;

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
            BuchiState_acceptance next_global(q_prime, SILENT, P1next, P2next, phase_after_current);

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

                BuchiState_acceptance next_global(q_prime, guess, P1new, P2new, phase_after_current);

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
        if (global_state.acceptance_flag == ACC_WAIT_P2EMPTY && global_state.P2.size() == 0) {
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


Automaton* NestedAutomaton::flatten_regular(value_function_t finVal, weight_t bound) {
    if (this->allParentStatesFinal()) {
        return flatten_regular_parent_trivial(finVal, bound); // accept iff P2 empty
    } else {
        return flatten_regular_parent_acceptance(finVal, bound); // track + accept in (track==1 && P2 empty)
    }
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


// // After pseudo-determinization, synchronize all children automata wrt silent transitions in the parent.
// // TODO: UPDATE AFTER MODIFYING CHILD CLASS: ensure initial states are correctly handled from call sites
// NestedAutomaton* NestedAutomaton::synchronizeChildren(std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual>& macro_alphabet) {
//     // ---------- accumulator caps ----------
//     weight_t X = 2 * this->getStates()->size();
//     for (size_t i = 0; i < this->getChildrenSize(); ++i) {
//         ChildAutomaton* child = this->getChild(i);
//         if (child) X = X * child->getStates()->size();
//     }
//     std::vector<weight_t> maxWeights(this->getChildrenSize(), weight_t(0));
//     for (size_t i = 0; i < this->getChildrenSize(); ++i) {
//         ChildAutomaton* child = this->getChild(i);
//         if (child) maxWeights[i] = X * child->getMinDomain();
//     }

//     // ---------- build synchronized children on-the-fly ----------
//     MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(this->getChildrenSize());

//     // Helper: take the single edge (if any) from a SetStd<Edge*>
//     auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
//         if (!succs) return nullptr;
//         for (Edge* e : *succs) return e; // at most one after determinization
//         return nullptr;
//     };

//     for (size_t ci = 0; ci < this->getChildrenSize(); ++ci) {
//         Symbol::RESET();
//         State::RESET();
//         Weight::RESET();

//         ChildAutomaton* child = this->getChild(ci);
//         if (!child) { new_children->insert(ci, nullptr); continue; }

//         // Reuse the encoded alphabet from determinizeWithMacroAlphabet, but with fresh Symbol objects
//         MapArray<Symbol*>* src_alpha = child->getAlphabet();
//         const size_t A = src_alpha->size();
//         std::vector<Symbol*> calpha_vec;
//         calpha_vec.reserve(A);
//         for (size_t a = 0; a < A; ++a) {
//             Symbol* s_src = src_alpha->at(a);
//             Symbol* s_new = new Symbol(s_src->getName()); // ID assigned in creation order
//             // After RESET and sequential creation, IDs match indices and s_src->getId()
//             // (Optional sanity during debugging)
//             // assert(s_new->getId() == s_src->getId());
//             calpha_vec.push_back(s_new);
//         }

//         // Materialize fixed-size MapArray with identical indices/IDs
//         MapArray<Symbol*>* calpha = new MapArray<Symbol*>(A);
//         for (Symbol* s : calpha_vec) {
//             calpha->insert(s->getId(), s); // s->getId() == its index 'a'
//         }

//         // ---- Use std::vector for dynamic sizing during on-the-fly construction ----
//         std::vector<State*>  cstates_vec;
//         std::vector<Weight*> cweights_vec;
//         MapStd<weight_t, Weight*> weight_register;

//         // Ensure weight 0 exists
//         {
//             Weight* w0 = new Weight(weight_t(0));
//             cweights_vec.push_back(w0);
//             weight_register.insert(weight_t(0), w0);
//         }

//         SetStd<State*>* cfinals = new SetStd<State*>();

//         // Product key (master, child, accumulator)
//         struct SyncKey {
//             State* m;
//             State* s;
//             weight_t acc;
//             bool operator==(const SyncKey& o) const { return m==o.m && s==o.s && acc==o.acc; }
//         };
//         struct SyncKeyHash {
//             size_t operator()(const SyncKey& k) const {
//                 size_t h = 1469598103934665603ull;
//                 auto mix = [&](uint64_t x){ h ^= x; h *= 1099511628211ull; };
//                 mix((uint64_t)k.m);
//                 mix((uint64_t)k.s);
//                 mix((uint64_t)std::hash<float>{}(static_cast<float>(k.acc)));
//                 return h;
//             }
//         };

//         std::unordered_map<SyncKey, State*, SyncKeyHash> state_map;
//         std::queue<SyncKey> worklist;

//         auto get_weight = [&](weight_t v) -> Weight* {
//             if (weight_register.contains(v) != true) {
//                 Weight* w = new Weight(v);
//                 cweights_vec.push_back(w);
//                 weight_register.insert(v, w);
//             }
//             return weight_register.at(v);
//         };

//         auto get_or_make_state = [&](const SyncKey& key) -> State* {
//             auto it = state_map.find(key);
//             if (it != state_map.end()) return it->second;

//             std::ostringstream ss; ss << "sync_" << state_map.size();
//             State* ns = new State(ss.str(), A, child->getMinDomain(), child->getMaxDomain());
//             cstates_vec.push_back(ns);

//             if (child->isFinal(key.s)) cfinals->insert(ns);

//             state_map.insert({key, ns});
//             worklist.push(key);
//             return ns;
//         };

//         // --- Seed: from all call sites (m, s0, 0) ------------------------------------
//         // TODO: CHILD'S INITIAL STATE MAY BE DIFFERENT FROM CALL TARGETS
//         // UPDATE AFTER MODIFYING CHILD CLASS: START EXPLORATION FROM EACH CALL SITE WITH THE CORRECT INITIAL STATE
//         State* initial_state = nullptr;
//         {
//             State* s0 = child->getInitial();
//             const size_t M = this->getStates()->size();

//             for (size_t mi = 0; mi < M; ++mi) {
//                 State* m = this->getStates()->at(mi);
//                 bool callable_here = false;
//                 for (size_t a = 0; a < A && !callable_here; ++a) {
//                     Edge* me = first_edge_or_null(m->getSuccessors(a));
//                     Edge* se = first_edge_or_null(s0->getSuccessors(a));
//                     if (me && se) callable_here = true;
//                 }
//                 if (callable_here) {
//                     SyncKey k{ m, s0, weight_t(0) };
//                     State* seed_state = get_or_make_state(k); // enqueues into worklist
//                     if (!initial_state) initial_state = seed_state; // pick the first as the designated initial
//                 }
//             }
//             if (!initial_state) {
//                 SyncKey k{ this->getInitial(), s0, weight_t(0) };
//                 initial_state = get_or_make_state(k);
//             }
//         }

//         const weight_t accCap = maxWeights[ci];

//         // Explore lazily
//         unsigned int ctr = 0;
//         while (!worklist.empty()) {
//             std::cout << ctr << std::endl; ++ctr;
//             SyncKey cur = worklist.front(); worklist.pop();
//             State* cur_node = state_map.at(cur);

//             for (size_t a = 0; a < A; ++a) {
//                 // Master: after pseudo-determinization, at most one outgoing per symbol
//                 Edge* me = first_edge_or_null(cur.m->getSuccessors(a));
//                 if (!me) continue;
//                 const bool master_silent = (me->getWeight()->getValue() == weight_t(0));
//                 State* m2 = me->getTo();

//                 // Child: similarly, at most one outgoing per symbol
//                 Edge* se = first_edge_or_null(cur.s->getSuccessors(a));
//                 if (!se) continue;

//                 State* s2 = se->getTo();
//                 weight_t ws = se->getWeight()->getValue();

//                 weight_t emit, acc2;
//                 if (master_silent) {
//                     // emit 0, accumulate child weight
//                     emit = weight_t(0);
//                     acc2 = cur.acc + ws;
//                 } else {
//                     // flush: emit acc + current, then reset
//                     emit = cur.acc + ws;
//                     acc2 = weight_t(0);
//                 }

//                 SyncKey nxt{ m2, s2, acc2 };
//                 State* nxt_node = get_or_make_state(nxt);

//                 Weight* w = get_weight(emit);
//                 Edge* ne = new Edge(calpha->at(a), w, cur_node, nxt_node);
//                 cur_node->addSuccessor(ne);
//                 nxt_node->addPredecessor(ne);
//             }
//         }

//         // ---- Materialize fixed-size MapArray from vectors ----
//         size_t state_count  = cstates_vec.size();
//         size_t weight_count = cweights_vec.size();

//         MapArray<State*>*  cstates  = new MapArray<State*>(state_count);
//         MapArray<Weight*>* cweights = new MapArray<Weight*>(weight_count);

//         for (State* s : cstates_vec)   cstates->insert(s->getId(), s);
//         for (Weight* w : cweights_vec) cweights->insert(w->getId(), w);

//         // Build synchronized child (shares alphabet with the determinized child)
//         std::string cname = child->getName() + "_sync";
//         ChildAutomaton* synced = new ChildAutomaton(
//             cname, calpha, cstates, cweights,
//             child->getMinDomain(), child->getMaxDomain(),
//             initial_state, cfinals
//         );
//         new_children->insert(ci, synced);
//     }

//     // Return a fresh NWA with the same parent and synchronized children
//     NestedAutomaton* result = new NestedAutomaton(this, new_children);
//     result->setName("Sync(" + this->getName() + ")");
//     return result;
// }




/*
// assuming the input NWA is pseudo-deterministic and children are synchronized
Automaton* NestedAutomaton::flatten() {
    using std::size_t;

    // ---------- helper: single outgoing edge after determinization ----------
    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e; // at most one after determinization
        return nullptr;
    };

    // ---------- cache children and basic metadata ----------
    const size_t C = this->getChildrenSize();
    std::vector<ChildAutomaton*> children(C, nullptr);
    for (size_t ci = 0; ci < C; ++ci) {
        children[ci] = this->getChild(ci);
    }

    // |U| = total #states over all (synchronized) children
    size_t U = 0;
    // W_abs = largest absolute weight in U, as weight_t
    weight_t W_abs = weight_t(0);

    auto weight_zero = []() { return weight_t(0); };
    auto weight_two  = []() { return weight_t(2); };

    auto weight_abs = [&](weight_t v) -> weight_t {
        // Non-positive weights (Sum-) ⇒ |v| = (v < 0 ? -v : v)
        if (v < weight_zero()) return -v;
        return v;
    };

    for (size_t ci = 0; ci < C; ++ci) {
        ChildAutomaton* child = children[ci];
        if (!child) continue;

        MapArray<State*>* cstates = child->getStates();
        U += cstates->size();

        MapArray<Weight*>* cws = child->getWeights();
        for (size_t wid = 0; wid < cws->size(); ++wid) {
            weight_t wv = cws->at(wid)->getValue();
            weight_t mag = weight_abs(wv);
            if (mag > W_abs) W_abs = mag;
        }
    }

    // ---------- X, Y, Z with overflow-safe integer arithmetic on the combinatorial part ----------
    const size_t M_states = this->getStates()->size();

    auto sat_mul = [](size_t a, size_t b) -> size_t {
        if (a == 0 || b == 0) return 0;
        const size_t maxv = std::numeric_limits<size_t>::max();
        if (a > maxv / b) return maxv;
        return a * b;
    };

    auto sat_pow = [&](size_t base, size_t exp) -> size_t {
        if (exp == 0) return 1;
        const size_t maxv = std::numeric_limits<size_t>::max();
        size_t res = 1;
        while (exp > 0) {
            if (base != 0 && res > maxv / base) return maxv;
            res *= base;
            if (res == maxv) return maxv;
            --exp;
        }
        return res;
    };

    // X = 2 * |M| * Π_i |S_i|
    size_t X_states = 2;
    X_states = sat_mul(X_states, M_states);
    for (size_t ci = 0; ci < C; ++ci) {
        ChildAutomaton* child = children[ci];
        if (!child) continue;
        X_states = sat_mul(X_states, child->getStates()->size());
    }

    // Y = X * (|U| + 2) * |U|^{2|U|}
    size_t exp   = sat_mul(2, U);          // 2|U|
    size_t U_pow = sat_pow(U, exp);        // |U|^{2|U|} (saturating)

    size_t Y = sat_mul(X_states, U + 2);
    Y = sat_mul(Y, U_pow);

    // Z = 2 * X * (|U| + 2) * |U|^{2|U|} * W_abs  (all in weight_t)
    weight_t Z = weight_zero();
    if (W_abs > weight_zero()) {
        // Conversions size_t -> weight_t must be supported (they are used elsewhere in QuAK)
        weight_t WX = weight_two() * weight_t(X_states);
        WX = WX * weight_t(U + 2);
        WX = WX * weight_t(U_pow);
        Z  = WX * W_abs;
    }

    // ---------- flattened alphabet: copy from NWA ----------
    Symbol::RESET();
    MapArray<Symbol*>* src_alpha = this->getAlphabet();
    const size_t A = src_alpha->size();

    MapArray<Symbol*>* falpha = new MapArray<Symbol*>(A);
    for (size_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(src_alpha->at(a)->getName());
        falpha->insert(s_new->getId(), s_new); // id == index
    }

    // ---------- flat states and weights ----------
    State::RESET();
    Weight::RESET();

    std::vector<State*>  fstates_vec;
    std::vector<Weight*> fweights_vec;
    fstates_vec.reserve(64);
    fweights_vec.reserve(16);

    MapStd<weight_t, Weight*> weight_register;

    weight_t flat_min = weight_zero();
    weight_t flat_max = weight_zero();
    bool flat_has_weight = false;

    auto get_weight = [&](weight_t v) -> Weight* {
        if (weight_register.contains(v) != true) {
            Weight* w = new Weight(v);
            fweights_vec.push_back(w);
            weight_register.insert(v, w);

            if (!flat_has_weight) {
                flat_min = flat_max = v;
                flat_has_weight = true;
            } else {
                if (v < flat_min) flat_min = v;
                if (v > flat_max) flat_max = v;
            }
        }
        return weight_register.at(v);
    };

    // ---------- encoding of flatten states ----------
    struct BoundedInst {
        size_t   child_index;
        State*   state;
        weight_t budget;  // remaining |weight|-budget ∈ [0, Z]
    };

    using UInst = std::pair<size_t, State*>; // (child_index, child_state)

    struct FlatKey {
        State*                   master;
        std::vector<UInst>       unbounded;
        std::vector<BoundedInst> bounded;

        bool operator==(const FlatKey& o) const {
            if (master != o.master) return false;
            if (unbounded.size() != o.unbounded.size()) return false;
            if (bounded.size()   != o.bounded.size())   return false;

            for (size_t i = 0; i < unbounded.size(); ++i) {
                if (unbounded[i].first  != o.unbounded[i].first)  return false;
                if (unbounded[i].second != o.unbounded[i].second) return false;
            }
            for (size_t i = 0; i < bounded.size(); ++i) {
                const BoundedInst& b1 = bounded[i];
                const BoundedInst& b2 = o.bounded[i];
                if (b1.child_index != b2.child_index) return false;
                if (b1.state       != b2.state)       return false;
                if (b1.budget      != b2.budget)      return false;
            }
            return true;
        }
    };

    struct FlatKeyHash {
        size_t operator()(FlatKey const& k) const {
            size_t h = 1469598103934665603ull;
            auto mix = [&](uint64_t x) {
                h ^= x;
                h *= 1099511628211ull;
            };
            mix(reinterpret_cast<uint64_t>(k.master));
            for (auto const& u : k.unbounded) {
                mix(static_cast<uint64_t>(u.first));
                mix(reinterpret_cast<uint64_t>(u.second));
            }
            for (auto const& b : k.bounded) {
                mix(static_cast<uint64_t>(b.child_index));
                mix(reinterpret_cast<uint64_t>(b.state));
                // we deliberately ignore budget in the hash to avoid depending on hash<weight_t>
            }
            return h;
        }
    };

    auto normalize_key = [](FlatKey& k) {
        auto cmpU = [](const UInst& a, const UInst& b) {
            if (a.first != b.first) return a.first < b.first;
            return a.second->getId() < b.second->getId();
        };
        std::sort(k.unbounded.begin(), k.unbounded.end(), cmpU);

        auto cmpB = [](const BoundedInst& a, const BoundedInst& b) {
            if (a.child_index != b.child_index) return a.child_index < b.child_index;
            int ida = a.state->getId();
            int idb = b.state->getId();
            if (ida != idb) return ida < idb;
            if (a.budget < b.budget) return true;
            if (a.budget > b.budget) return false;
            return false;
        };
        std::sort(k.bounded.begin(), k.bounded.end(), cmpB);
    };

    std::unordered_map<FlatKey, State*, FlatKeyHash> state_map;
    std::queue<FlatKey> worklist;

    auto get_or_make_state = [&](FlatKey key) -> State* {
        normalize_key(key);
        auto it = state_map.find(key);
        if (it != state_map.end()) return it->second;

        std::ostringstream ss;
        ss << "flat_" << state_map.size();
        State* ns = new State(ss.str(), A, 0, 0); // no children: domain [0,0]
        fstates_vec.push_back(ns);

        state_map.insert(std::make_pair(key, ns));
        worklist.push(key);
        return ns;
    };

    // ---------- initial flat state: (master_initial, no slaves) ----------
    FlatKey initKey;
    initKey.master = this->getInitial();
    State* flat_initial = get_or_make_state(initKey);

    // ---------- BFS over flatten states ----------
    while (!worklist.empty()) {
        FlatKey key = worklist.front();
        worklist.pop();

        State* from_flat = state_map.at(key);

        for (size_t a = 0; a < A; ++a) {
            // master step
            Edge* me = first_edge_or_null(key.master->getSuccessors(a));
            if (!me) continue;
            State*   m2 = me->getTo();
            weight_t wm = me->getWeight()->getValue(); // ≤ 0 under Sum-

            bool ok = true;

            std::vector<UInst>       next_unbounded;
            std::vector<BoundedInst> next_bounded;
            next_unbounded.reserve(key.unbounded.size());
            next_bounded.reserve(key.bounded.size());

            weight_t sum_unbounded = weight_zero();
            weight_t sum_bounded   = weight_zero();

            // --- existing unbounded instances ---
            for (const UInst& u : key.unbounded) {
                const size_t ci    = u.first;
                State* const s_cur = u.second;

                ChildAutomaton* child = children[ci];
                if (!child) { ok = false; break; }

                Edge* se = first_edge_or_null(s_cur->getSuccessors(a));
                if (!se) { ok = false; break; }

                State*   s2 = se->getTo();
                weight_t xu = se->getWeight()->getValue(); // ≤ 0

                sum_unbounded += xu;
                next_unbounded.emplace_back(ci, s2);
            }
            if (!ok) continue;

            // --- existing bounded instances ---
            for (const BoundedInst& b : key.bounded) {
                const size_t ci    = b.child_index;
                State* const s_cur = b.state;
                weight_t     bud   = b.budget; // ≥ 0

                ChildAutomaton* child = children[ci];
                if (!child) { ok = false; break; }

                Edge* se = first_edge_or_null(s_cur->getSuccessors(a));
                if (!se) { ok = false; break; }

                State*   s2 = se->getTo();
                weight_t z  = se->getWeight()->getValue();     // ≤ 0
                weight_t mag_z = weight_abs(z);                // |z| ≥ 0

                // strictly decreasing absolute budget
                if (bud < mag_z) { ok = false; break; }
                weight_t bud2 = bud - mag_z;

                sum_bounded += z; // actual contribution is still z (≤ 0)

                // if budget is exhausted and child is in final, drop this instance
                if (bud2 == weight_zero() && children[ci]->isFinal(s2)) {
                    continue;
                }

                BoundedInst nb;
                nb.child_index = ci;
                nb.state       = s2;
                nb.budget      = bud2;
                next_bounded.push_back(nb);
            }
            if (!ok) continue;

            // -------- (i) no new instantiation --------
            {
                FlatKey k2;
                k2.master    = m2;
                k2.unbounded = next_unbounded;
                k2.bounded   = next_bounded;

                State* to_flat = get_or_make_state(k2);
                weight_t x = wm + sum_unbounded + sum_bounded; // Sum- ⇒ x ≤ 0

                Edge* e = new Edge(falpha->at(a), get_weight(x), from_flat, to_flat);
                from_flat->addSuccessor(e);
                to_flat->addPredecessor(e);
            }

            // -------- (ii) spawn unbounded instance --------
            if (Y > 0 && next_unbounded.size() < Y && U > 0) {
                for (size_t ci = 0; ci < C; ++ci) {
                    ChildAutomaton* child = children[ci];
                    if (!child) continue;

                    State* s0 = child->getInitial();
                    Edge*  se0 = first_edge_or_null(s0->getSuccessors(a));
                    if (!se0) continue;

                    State*   s1    = se0->getTo();
                    weight_t x_new = se0->getWeight()->getValue(); // ≤ 0

                    FlatKey k2;
                    k2.master    = m2;
                    k2.unbounded = next_unbounded;
                    k2.bounded   = next_bounded;
                    k2.unbounded.emplace_back(ci, s1);

                    State* to_flat = get_or_make_state(k2);
                    weight_t x = wm + sum_unbounded + sum_bounded + x_new;

                    Edge* e = new Edge(falpha->at(a), get_weight(x), from_flat, to_flat);
                    from_flat->addSuccessor(e);
                    to_flat->addPredecessor(e);
                }
            }

            // -------- (iii) spawn bounded instance --------
            if (U > 0 && Z > weight_zero() && next_bounded.size() < U) {
                for (size_t ci = 0; ci < C; ++ci) {
                    ChildAutomaton* child = children[ci];
                    if (!child) continue;

                    State* s0 = child->getInitial();
                    Edge*  se0 = first_edge_or_null(s0->getSuccessors(a));
                    if (!se0) continue;

                    State*   s1 = se0->getTo();
                    weight_t z  = se0->getWeight()->getValue();   // ≤ 0
                    weight_t mag_z = weight_abs(z);
                    if (mag_z > Z) continue;

                    weight_t bud2 = Z - mag_z;

                    FlatKey k2;
                    k2.master    = m2;
                    k2.unbounded = next_unbounded;
                    k2.bounded   = next_bounded;

                    BoundedInst nb;
                    nb.child_index = ci;
                    nb.state       = s1;
                    nb.budget      = bud2;
                    k2.bounded.push_back(nb);

                    State* to_flat = get_or_make_state(k2);
                    weight_t x = wm + sum_unbounded + sum_bounded + z; // ≤ 0

                    Edge* e = new Edge(falpha->at(a), get_weight(x), from_flat, to_flat);
                    from_flat->addSuccessor(e);
                    to_flat->addPredecessor(e);
                }
            }
        }
    }

    // ---------- materialize states / weights ----------
    const size_t state_count  = fstates_vec.size();
    const size_t weight_count = fweights_vec.size();

    MapArray<State*>*  fstates  = new MapArray<State*>(state_count);
    MapArray<Weight*>* fweights = new MapArray<Weight*>(weight_count);

    for (State* s : fstates_vec)   fstates->insert(s->getId(), s);
    for (Weight* w : fweights_vec) fweights->insert(w->getId(), w);

    if (!flat_has_weight) {
        flat_min = flat_max = weight_zero();
    }

    // ---------- select final states (logic only, wiring into Automaton left to you) ----------
    // Final iff master is final and there are no active slave instances.
    SetStd<State*>* flat_finals = new SetStd<State*>();
    for (const auto& kv : state_map) {
        const FlatKey& k = kv.first;
        State* s = kv.second;
        if (k.unbounded.empty() && k.bounded.empty()) {
            flat_finals->insert(s);
        }
    }
    (void)flat_finals; // TODO: wire into acceptance once Automaton/NestedAutomaton exposes it

    // ---------- build and return a childless NestedAutomaton as Automaton* ----------
    std::string fname = "Flat(" + this->getName() + ")";
    // MapArray<ChildAutomaton*>* no_children = new MapArray<ChildAutomaton*>(0);

    Automaton* flatNA = new Automaton(
        fname,
        falpha,
        fstates,
        fweights,
        flat_min,
        flat_max,
        flat_initial
    );

    return flatNA;
}
*/


/*
NestedAutomaton* NestedAutomaton::synchronizeChildren() {
    auto sat_mul_u64 = [](uint64_t a, uint64_t b) -> uint64_t {
        if (a == 0 || b == 0) return 0;
        if (a > std::numeric_limits<uint64_t>::max() / b) {
            // return std::numeric_limits<uint64_t>::max();
            QUAK_FAIL("Overflow in sat_mul_u64 during synchronizeChildren");
        }
        return a * b;
    };

    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e;
        return nullptr;
    };

    auto master_called_child_id = [](const Edge* me) -> size_t {
        if (!me || !me->getWeight()) return 0;
        float id = me->getWeight()->getValue().to_float();
        return (id < 0) ? 0u : static_cast<size_t>(id);
    };

    // compute X=2*conf(this) with overflow-safe arithmetic
    uint64_t conf = 1;
    const uint64_t nm = static_cast<uint64_t>(this->getStates()->size());
    if (nm > 1) conf = sat_mul_u64(conf, nm);
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* c = this->getChild(i);
        const uint64_t nc = static_cast<uint64_t>(c->getStates()->size());
        if (nc > 1) conf = sat_mul_u64(conf, nc);
    }
    const uint64_t X_u64 = sat_mul_u64(2, conf);


    MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(this->getChildrenSize());
    const size_t M = this->getStates()->size();

    for (size_t ci = 0; ci < this->getChildrenSize(); ++ci) {
        Symbol::RESET();
        State::RESET();
        Weight::RESET();

        ChildAutomaton* child = this->getChild(ci);
        MapArray<Symbol*>* src_alpha = child->getAlphabet();
        const size_t A = src_alpha->size();

        MapArray<Symbol*>* calpha = new MapArray<Symbol*>(A);
        for (size_t a = 0; a < A; ++a) {
            Symbol* s_new = new Symbol(src_alpha->at(a));
            calpha->insert(s_new->getId(), s_new);
        }

        // compute child weight stats and accumulator cap
        weight_t minW = weight_t(0);
        weight_t maxW = weight_t(0);
        weight_t maxAbsW = weight_t(0);
        MapArray<Weight*>* ws = child->getWeights();
        bool first = true;
        for (size_t wi = 0; wi < ws->size(); ++wi) {
            Weight* w = ws->at(wi);
            weight_t v = w->getValue();
            if (first) { minW = maxW = v; first = false; }
            else { if (v < minW) minW = v; if (v > maxW) maxW = v; }
            weight_t av = (v < weight_t(0)) ? -v : v;
            if (av > maxAbsW) maxAbsW = av;
        }
        const weight_t cap = (maxAbsW == weight_t(0)) ? weight_t(0) : weight_t(static_cast<double>(X_u64)) * maxAbsW;

        // range for children value accumulation
        weight_t accMin = weight_t(0);
        weight_t accMax = weight_t(0);
        if (cap == weight_t(0)) {
            accMin = accMax = weight_t(0);
        } else if (maxW <= weight_t(0)) {
            accMin = -cap; accMax = weight_t(0);
        } else if (minW >= weight_t(0)) {
            accMin = weight_t(0); accMax = cap;
        } else {
            accMin = -cap; accMax = cap;
        }

        // auto acc_in_range = [&](weight_t a) -> bool {
        //     return (a >= accMin && a <= accMax);
        // };

        // output domain bounds after synchronization (outputs are 0 or accumulatedValue+transitionWeight)
        weight_t outMinBound = std::min(weight_t(0), accMin + minW);
        weight_t outMaxBound = std::max(weight_t(0), accMax + maxW);

        // build the synchronized child on the fly
        std::vector<State*>  cstates_vec;
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
        get_weight(weight_t(0)); // ensure 0 exists

        SetStd<State*>* cfinals = new SetStd<State*>();

        // Key = (master state id, child state id, accumulator, pending_accept)
        struct Key {
            uint32_t mid;
            uint32_t sid;
            weight_t acc;
            bool pending;
            bool operator==(const Key& o) const {
                return mid == o.mid && sid == o.sid && acc == o.acc && pending == o.pending;
            }
        };

        struct KeyHash {
            size_t operator()(const Key& k) const noexcept {
                size_t h = 1469598103934665603ull;
                auto mix64 = [&](uint64_t x){ h ^= x; h *= 1099511628211ull; };

                mix64(static_cast<uint64_t>(k.mid));
                mix64(static_cast<uint64_t>(k.sid));

                // hash accumulator in a way consistent with operator== (handles -0.0 == 0.0)
                weight_t v = k.acc;
                if (std::is_floating_point<weight_t>::value) {
                    if (v == weight_t(0)) v = weight_t(0);
                }
                mix64(static_cast<uint64_t>(std::hash<weight_t>{}(v)));

                mix64(static_cast<uint64_t>(k.pending ? 1 : 0));
                return h;
            }
        };

        std::unordered_map<Key, State*, KeyHash> state_map;
        std::queue<Key> worklist;

        auto child_is_final_sid = [&](uint32_t sid) -> bool {
            State* cs = child->getStates()->at(sid);
            return cs && child->isFinal(cs);
        };

        auto get_or_make_state = [&](const Key& key) -> State* {
            auto it = state_map.find(key);
            if (it != state_map.end()) return it->second;

            std::ostringstream ss; ss << "sync_" << state_map.size();
            State* ns = new State(ss.str(), A, outMinBound, outMaxBound);
            cstates_vec.push_back(ns);

            if (child_is_final_sid(key.sid) && !key.pending) {
                ns->setFinal(true);
                cfinals->insert(ns);
            }

            state_map.insert({key, ns});
            worklist.push(key);
            return ns;
        };

        const uint32_t m0 = static_cast<uint32_t>(this->getInitial()->getId());
        const uint32_t s0 = static_cast<uint32_t>(child->getInitial()->getId());
        State* initial_state = get_or_make_state(Key{m0, s0, weight_t(0), false});

        while (!worklist.empty()) {
            Key cur = worklist.front(); worklist.pop();
            if (child_is_final_sid(cur.sid) && !cur.pending) {
                continue; // accepting states of children are terminal
            }

            State* cur_node = state_map.at(cur);
            State* m = this->getStates()->at(cur.mid);
            State* s = child->getStates()->at(cur.sid);

            for (size_t a = 0; a < A; ++a) {
                Edge* me = first_edge_or_null(m->getSuccessors(a));
                if (!me) continue;

                const size_t called = master_called_child_id(me);
                const bool msilent = (called == 0);
                const uint32_t mid2 = static_cast<uint32_t>(me->getTo()->getId());

                // child step (if pending but child has already terminated, we freeze it and use x=0)
                Edge* se = nullptr;
                uint32_t sid2 = cur.sid;
                weight_t x = weight_t(0);

                if (!cur.pending) {
                    se = first_edge_or_null(s->getSuccessors(a));
                    if (!se) continue;
                    sid2 = static_cast<uint32_t>(se->getTo()->getId());
                    x = se->getWeight()->getValue();
                }

                // apply synchronization rule
                if (msilent) {
                    // emit 0, accumulate
                    const weight_t acc2 = cur.acc + x;
                    // if (!acc_in_range(acc2)) continue; // CHECK: WHY DO WE NEED THIS HERE? IS THIS AN AGGRESIVELY DEFENSIVE CHECK? THESE BOUNDS MUST BE THEORETICALLY SOUND

                    // if we just reached an accepting child state on a silent master step
                    // we must delay termination until next non-silent step to flush acc
                    bool pending2 = cur.pending;
                    if (!cur.pending && child->isFinal(se->getTo())) pending2 = true;

                    Key nxt{mid2, sid2, acc2, pending2};
                    State* nxt_node = get_or_make_state(nxt);

                    Weight* wy = get_weight(weight_t(0));
                    Edge* ne = new Edge(calpha->at(a), wy, cur_node, nxt_node);
                    cur_node->addSuccessor(ne);
                    nxt_node->addPredecessor(ne);
                } else {
                    // non-silent: emit acc+x, reset acc
                    const weight_t y = cur.acc + x;
                    Weight* wy = get_weight(y);

                    // if child terminated now, we go to acc and stop
                    // if we were pending already, this flushes and stops as well
                    const bool terminates_now = cur.pending || (!cur.pending && se && child->isFinal(se->getTo()));


                    // CHECK: SOMETHING IS OFF HERE. ISN'T THE IF BLOCK EXACTLY THE SAME AS WHAT COMES AFTER?
                    if (terminates_now) {
                        // go to the ordinary product state whose child component is accepting
                        Key end{mid2, sid2, weight_t(0), false};
                        State* end_node = get_or_make_state(end);

                        Edge* ne = new Edge(calpha->at(a), wy, cur_node, end_node);
                        cur_node->addSuccessor(ne);
                        end_node->addPredecessor(ne);
                        continue;
                    }

                    Key nxt{mid2, sid2, weight_t(0), false};
                    State* nxt_node = get_or_make_state(nxt);

                    Edge* ne = new Edge(calpha->at(a), wy, cur_node, nxt_node);
                    cur_node->addSuccessor(ne);
                    nxt_node->addPredecessor(ne);
                }
            }
        }

        // compute exact min/max domain from created weights
        weight_t realMin = weight_t(0), realMax = weight_t(0);
        first = true;
        for (Weight* w : cweights_vec) {
            weight_t v = w->getValue();
            if (first) { realMin = realMax = v; first = false; }
            else { if (v < realMin) realMin = v; if (v > realMax) realMax = v; }
        }

        MapArray<State*>*  cstates  = new MapArray<State*>(cstates_vec.size());
        MapArray<Weight*>* cweights = new MapArray<Weight*>(cweights_vec.size());
        for (State* st : cstates_vec)   cstates->insert(st->getId(), st);
        for (Weight* wt : cweights_vec) cweights->insert(wt->getId(), wt);

        std::string cname = child->getName() + "_sync";
        ChildAutomaton* synced = new ChildAutomaton(cname, calpha, cstates, cweights, realMin, realMax, initial_state, cfinals);
        new_children->insert(ci, synced);
    }

    NestedAutomaton* result = new NestedAutomaton(this, new_children);
    result->setName("Sync(" + this->getName() + ")");
    return result;
}
*/




/**
 * SYNCHRONIZATION SEMANTICS:
 * ==========================
 * The goal is to transform each child automaton so that it "observes" the master's behavior:
 * - When the master fires a SILENT transition, the synchronized child emits weight 0
 *   but internally accumulates the weight it would have emitted.
 * - When the master fires a NON-SILENT transition (calling some child), the synchronized
 *   child emits its accumulated weight plus its current transition weight, then resets.
 *
 * STATE REPRESENTATION:
 * ====================
 * Each state of the synchronized child is a tuple (m, s, acc, pending) where:
 *   - m: current state of the master automaton
 *   - s: current state of the original child automaton
 *   - acc: accumulated weight (to be flushed on next non-silent master step)
 *   - pending: boolean flag indicating the child has reached an accepting state during
 *              a silent master step and is waiting to flush its accumulator
 *
 * TERMINATION SEMANTICS:
 * =====================
 * - A synchronized child state is FINAL iff the original child state is accepting AND pending=false.
 *   (This ensures the accumulator has been flushed before termination.)
 * - Final states have no outgoing transitions (they are terminal).
 * - When the child reaches an accepting state during a SILENT master step, we set pending=true
 *   to delay termination until the next non-silent step allows us to flush the accumulator.
 *
 * BOUNDS:
 * ======
 * - X = 2 * conf(A) where conf(A) = product of all non-trivial automaton state counts
 * - Accumulator is bounded by [-X*maxAbsW, X*maxAbsW] to handle both positive and negative weights
 */


NestedAutomaton* NestedAutomaton::synchronizeChildren() {
    // =========================================================================
    // UTILITY LAMBDAS
    // =========================================================================
    
    /**
     * Saturating multiplication for uint64_t.
     * Returns max value on overflow instead of wrapping.
     */
    auto sat_mul_u64 = [](uint64_t a, uint64_t b) -> uint64_t {
        if (a == 0 || b == 0) return 0;
        if (a > std::numeric_limits<uint64_t>::max() / b) {
            QUAK_FAIL("Overflow in sat_mul_u64 during synchronizeChildren");
        }
        return a * b;
    };

    /**
     * Returns the first edge from a successor set, or nullptr if empty/null.
     * Assumes pseudo-determinism (at most one edge per letter from each state).
     */
    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e;
        return nullptr;
    };

    /**
     * Extracts the called child ID from a master edge's weight.
     * Convention: weight encodes child ID; 0 or negative means silent transition.
     */
    auto master_called_child_id = [](const Edge* me) -> size_t {
        if (!me || !me->getWeight()) return 0;
        float id = me->getWeight()->getValue().to_float();
        return (id < 0) ? 0u : static_cast<size_t>(id);
    };

    // =========================================================================
    // COMPUTE CONFIGURATION BOUND X = 2 * conf(this)
    // =========================================================================
    // conf(A) = product of state counts for all non-trivial automata
    // This bounds the number of steps before a cycle must occur.
    
    uint64_t conf = 1;
    const uint64_t nm = static_cast<uint64_t>(this->getStates()->size());
    if (nm > 1) conf = sat_mul_u64(conf, nm);
    
    for (size_t i = 0; i < this->getChildrenSize(); ++i) {
        ChildAutomaton* c = this->getChild(i);
        const uint64_t nc = static_cast<uint64_t>(c->getStates()->size());
        if (nc > 1) conf = sat_mul_u64(conf, nc);
    }
    const uint64_t X_u64 = sat_mul_u64(2, conf);

    // =========================================================================
    // SYNCHRONIZE EACH CHILD AUTOMATON
    // =========================================================================
    
    MapArray<ChildAutomaton*>* new_children = new MapArray<ChildAutomaton*>(this->getChildrenSize());
    const size_t M = this->getStates()->size();

    for (size_t ci = 0; ci < this->getChildrenSize(); ++ci) {
        // Reset ID counters for fresh symbol/state/weight IDs
        Symbol::RESET();
        State::RESET();
        Weight::RESET();

        ChildAutomaton* child = this->getChild(ci);
        MapArray<Symbol*>* src_alpha = child->getAlphabet();
        const size_t A = src_alpha->size();

        // Copy alphabet (symbols are value objects)
        MapArray<Symbol*>* calpha = new MapArray<Symbol*>(A);
        for (size_t a = 0; a < A; ++a) {
            Symbol* s_new = new Symbol(src_alpha->at(a));
            calpha->insert(s_new->getId(), s_new);
        }

        // =====================================================================
        // COMPUTE WEIGHT STATISTICS AND ACCUMULATOR BOUNDS
        // =====================================================================
        
        weight_t minW = weight_t(0);
        weight_t maxW = weight_t(0);
        weight_t maxAbsW = weight_t(0);
        MapArray<Weight*>* ws = child->getWeights();
        bool first = true;
        
        for (size_t wi = 0; wi < ws->size(); ++wi) {
            Weight* w = ws->at(wi);
            weight_t v = w->getValue();
            if (first) { 
                minW = maxW = v; 
                first = false; 
            } else { 
                if (v < minW) minW = v; 
                if (v > maxW) maxW = v; 
            }
            weight_t av = (v < weight_t(0)) ? -v : v;
            if (av > maxAbsW) maxAbsW = av;
        }
        
        // The accumulator is bounded by X * maxAbsW
        const weight_t cap = (maxAbsW == weight_t(0)) 
            ? weight_t(0) 
            : weight_t(static_cast<double>(X_u64)) * maxAbsW;

        // Determine accumulator range based on weight signs
        weight_t accMin = weight_t(0);
        weight_t accMax = weight_t(0);
        if (cap == weight_t(0)) {
            accMin = accMax = weight_t(0);
        } else if (maxW <= weight_t(0)) {
            // All weights non-positive: accumulator is non-positive
            accMin = -cap; 
            accMax = weight_t(0);
        } else if (minW >= weight_t(0)) {
            // All weights non-negative: accumulator is non-negative
            accMin = weight_t(0); 
            accMax = cap;
        } else {
            // Mixed signs: accumulator can be anything in [-cap, cap]
            accMin = -cap; 
            accMax = cap;
        }

        // Output domain bounds (outputs are either 0 or accumulated+transition weight)
        weight_t outMinBound = std::min(weight_t(0), accMin + minW);
        weight_t outMaxBound = std::max(weight_t(0), accMax + maxW);

        // =====================================================================
        // BUILD SYNCHRONIZED CHILD AUTOMATON
        // =====================================================================
        
        std::vector<State*>  cstates_vec;
        std::vector<Weight*> cweights_vec;
        MapStd<weight_t, Weight*> weight_register;

        // Memoized weight creation
        auto get_weight = [&](weight_t v) -> Weight* {
            if (!weight_register.contains(v)) {
                Weight* w = new Weight(v);
                cweights_vec.push_back(w);
                weight_register.insert(v, w);
            }
            return weight_register.at(v);
        };
        get_weight(weight_t(0)); // Ensure zero weight exists

        SetStd<State*>* cfinals = new SetStd<State*>();

        // ---------------------------------------------------------------------
        // Synchronized state key: (master_state, child_state, accumulator, pending)
        // ---------------------------------------------------------------------
        struct SyncKey {
            uint32_t master_id;     // Master automaton state ID
            uint32_t child_id;      // Child automaton state ID  
            weight_t accumulator;   // Accumulated weight waiting to be flushed
            bool pending_accept;    // True if child reached accepting during silent step
            
            bool operator==(const SyncKey& o) const {
                return master_id == o.master_id 
                    && child_id == o.child_id 
                    && accumulator == o.accumulator 
                    && pending_accept == o.pending_accept;
            }
        };

        struct SyncKeyHash {
            size_t operator()(const SyncKey& k) const noexcept {
                size_t h = 1469598103934665603ull;
                auto mix64 = [&](uint64_t x) { 
                    h ^= x; 
                    h *= 1099511628211ull; 
                };

                mix64(static_cast<uint64_t>(k.master_id));
                mix64(static_cast<uint64_t>(k.child_id));

                // Hash accumulator consistently (handle -0.0 == 0.0)
                weight_t v = k.accumulator;
                if (std::is_floating_point<weight_t>::value) {
                    if (v == weight_t(0)) v = weight_t(0);
                }
                mix64(static_cast<uint64_t>(std::hash<weight_t>{}(v)));
                mix64(static_cast<uint64_t>(k.pending_accept ? 1 : 0));
                
                return h;
            }
        };

        std::unordered_map<SyncKey, State*, SyncKeyHash> state_map;
        std::queue<SyncKey> worklist;

        // Helper: check if a child state ID corresponds to a final state
        auto child_is_final = [&](uint32_t sid) -> bool {
            State* cs = child->getStates()->at(sid);
            return cs && child->isFinal(cs);
        };

        // ---------------------------------------------------------------------
        // Get or create synchronized state for a given key
        // ---------------------------------------------------------------------
        auto get_or_make_state = [&](const SyncKey& key) -> State* {
            auto it = state_map.find(key);
            if (it != state_map.end()) return it->second;

            // Create new state
            std::ostringstream ss;
            ss << "sync_m" << key.master_id 
               << "_s" << key.child_id 
               << "_a" << key.accumulator 
               << (key.pending_accept ? "_P" : "");
            
            State* ns = new State(ss.str(), A, outMinBound, outMaxBound);
            cstates_vec.push_back(ns);

            // A synchronized state is final iff:
            // - The child state is accepting, AND
            // - We are NOT pending (i.e., accumulator has been flushed)
            // Note: When pending=false and child is accepting, acc should be 0
            // (we only reach such states via non-silent transitions which reset acc)
            if (child_is_final(key.child_id) && !key.pending_accept) {
                ns->setFinal(true);
                cfinals->insert(ns);
            }

            state_map.insert({key, ns});
            worklist.push(key);
            return ns;
        };

        // Initialize: synchronized initial state
        const uint32_t m0 = static_cast<uint32_t>(this->getInitial()->getId());
        const uint32_t s0 = static_cast<uint32_t>(child->getInitial()->getId());
        State* initial_state = get_or_make_state(SyncKey{m0, s0, weight_t(0), false});

        // ---------------------------------------------------------------------
        // BFS exploration of synchronized state space
        // ---------------------------------------------------------------------
        while (!worklist.empty()) {
            SyncKey cur = worklist.front();
            worklist.pop();

            // Final states of synchronized child are terminal (no outgoing edges)
            if (child_is_final(cur.child_id) && !cur.pending_accept) {
                continue;
            }

            State* cur_node = state_map.at(cur);
            State* m_state = this->getStates()->at(cur.master_id);
            State* s_state = child->getStates()->at(cur.child_id);

            // Process each letter in the alphabet
            for (size_t a = 0; a < A; ++a) {
                // Get master transition on letter a
                Edge* master_edge = first_edge_or_null(m_state->getSuccessors(a));
                if (!master_edge) continue; // No master transition (assumes completeness)

                const size_t called_child = master_called_child_id(master_edge);
                const bool master_is_silent = (called_child == 0);
                const uint32_t next_master_id = static_cast<uint32_t>(master_edge->getTo()->getId());

                // -------------------------------------------------------------
                // Determine child transition and weight contribution
                // -------------------------------------------------------------
                Edge* child_edge = nullptr;
                uint32_t next_child_id = cur.child_id;
                weight_t child_weight = weight_t(0);

                if (cur.pending_accept) {
                    // Child has already terminated (in accepting state, waiting to flush)
                    // Child is "frozen": doesn't step, contributes 0 weight
                    // next_child_id stays the same (accepting state)
                    // child_weight = 0
                } else {
                    // Child is active: must take a transition
                    child_edge = first_edge_or_null(s_state->getSuccessors(a));
                    if (!child_edge) continue; // No child transition (input not complete)
                    
                    next_child_id = static_cast<uint32_t>(child_edge->getTo()->getId());
                    child_weight = child_edge->getWeight()->getValue();
                }

                // -------------------------------------------------------------
                // Apply synchronization rules based on master transition type
                // -------------------------------------------------------------
                if (master_is_silent) {
                    // SILENT MASTER TRANSITION
                    // - Emit weight 0
                    // - Accumulate child's weight
                    // - If child just reached accepting state, set pending
                    
                    const weight_t new_acc = cur.accumulator + child_weight;

                    // Skip if accumulator would exceed bounds
                    if (new_acc < accMin || new_acc > accMax) {
                        continue;  // Don't create this transition
                    }
                    
                    // Determine if we should set pending
                    bool new_pending = cur.pending_accept;
                    if (!cur.pending_accept && child_edge && child->isFinal(child_edge->getTo())) {
                        // Child just reached accepting state during silent step
                        // Delay termination until next non-silent step
                        new_pending = true;
                    }

                    SyncKey next_key{next_master_id, next_child_id, new_acc, new_pending};
                    State* next_node = get_or_make_state(next_key);

                    // Create transition with weight 0
                    Weight* w_out = get_weight(weight_t(0));
                    Edge* new_edge = new Edge(calpha->at(a), w_out, cur_node, next_node);
                    cur_node->addSuccessor(new_edge);
                    next_node->addPredecessor(new_edge);
                    
                } else {
                    // NON-SILENT MASTER TRANSITION
                    // - Emit accumulated weight + child's current weight
                    // - Reset accumulator to 0
                    // - Clear pending flag
                    
                    const weight_t emit_weight = cur.accumulator + child_weight;
                    Weight* w_out = get_weight(emit_weight);

                    // After non-silent step, accumulator resets and pending clears
                    // The next state always has acc=0 and pending=false
                    SyncKey next_key{next_master_id, next_child_id, weight_t(0), false};
                    State* next_node = get_or_make_state(next_key);

                    Edge* new_edge = new Edge(calpha->at(a), w_out, cur_node, next_node);
                    cur_node->addSuccessor(new_edge);
                    next_node->addPredecessor(new_edge);
                }
            }
        }

        // =====================================================================
        // FINALIZE SYNCHRONIZED CHILD AUTOMATON
        // =====================================================================
        
        // Compute actual min/max weights from created weights
        weight_t realMin = weight_t(0), realMax = weight_t(0);
        first = true;
        for (Weight* w : cweights_vec) {
            weight_t v = w->getValue();
            if (first) { 
                realMin = realMax = v; 
                first = false; 
            } else { 
                if (v < realMin) realMin = v; 
                if (v > realMax) realMax = v; 
            }
        }

        // Build final arrays
        MapArray<State*>*  cstates  = new MapArray<State*>(cstates_vec.size());
        MapArray<Weight*>* cweights = new MapArray<Weight*>(cweights_vec.size());
        for (State* st : cstates_vec)   cstates->insert(st->getId(), st);
        for (Weight* wt : cweights_vec) cweights->insert(wt->getId(), wt);

        // Create synchronized child
        std::string cname = child->getName() + "_sync";
        ChildAutomaton* synced = new ChildAutomaton(
            cname, calpha, cstates, cweights, 
            realMin, realMax, initial_state, cfinals
        );
        new_children->insert(ci, synced);
    }

    // =========================================================================
    // CREATE RESULT NESTED AUTOMATON
    // =========================================================================
    
    NestedAutomaton* result = new NestedAutomaton(this, new_children);
    result->setName("Sync(" + this->getName() + ")");
    return result;
}




// assuming the input NWA is pseudo-deterministic and children are synchronized
Automaton* NestedAutomaton::flatten_Avg_SumMinus() {
    using std::size_t;

    // ---------- helper: single outgoing edge after determinization ----------
    auto first_edge_or_null = [](SetStd<Edge*>* succs) -> Edge* {
        if (!succs) return nullptr;
        for (Edge* e : *succs) return e; // at most one after determinization
        return nullptr;
    };

    // ---------- weight helpers ----------
    auto weight_zero = []() { return weight_t(0); };
    auto weight_two  = []() { return weight_t(2); };
    auto weight_abs = [&](weight_t v) -> weight_t {
        // Sum- ⇒ weights are non-positive, but we still define |v|
        if (v < weight_zero()) return -v;
        return v;
    };

    // ---------- cache children ----------
    const size_t C = this->getChildrenSize();
    std::vector<ChildAutomaton*> children(C, nullptr);
    for (size_t ci = 0; ci < C; ++ci) {
        children[ci] = this->getChild(ci);
    }

    // ---------- master metadata ----------
    MapArray<State*>* mstates = this->getStates();
    MapArray<Symbol*>* malpha = this->getAlphabet();
    const size_t M_states = mstates->size();
    const size_t A        = malpha->size();

    // precompute master successor table [master_state_id][symbol_id]
    std::vector<std::vector<Edge*>> masterSucc(M_states, std::vector<Edge*>(A, nullptr));
    for (size_t sid = 0; sid < M_states; ++sid) {
        State* s = mstates->at(sid);
        if (!s) continue;
        // after construction, ids are dense; assert to be safe
        assert(s->getId() == static_cast<int>(sid));
        for (size_t a = 0; a < A; ++a) {
            masterSucc[sid][a] = first_edge_or_null(s->getSuccessors(a));
        }
    }

    // ---------- child metadata ----------
    struct ChildInfo {
        ChildAutomaton* aut = nullptr;
        std::vector<std::vector<Edge*>> succ;   // [state_id][symbol_id]
        std::vector<bool>               is_final; // [state_id]
        std::vector<Edge*>              initSucc; // [symbol_id] from initial state
        size_t                          num_states = 0;
    };

    std::vector<ChildInfo> cinfo(C);

    // global stats: total #child states U, max absolute weight W_abs
    size_t   U      = 0;
    weight_t W_abs  = weight_zero();

    for (size_t ci = 0; ci < C; ++ci) {
        ChildAutomaton* child = children[ci];
        cinfo[ci].aut = child;
        if (!child) continue;

        MapArray<State*>*   cstates  = child->getStates();
        MapArray<Weight*>*  cweights = child->getWeights();
        const size_t        S        = cstates->size();
        cinfo[ci].num_states = S;
        U += S;

        // compute |w| max over this child
        for (size_t wid = 0; wid < cweights->size(); ++wid) {
            Weight* w = cweights->at(wid);
            if (!w) continue;
            weight_t mag = weight_abs(w->getValue());
            if (mag > W_abs) W_abs = mag;
        }

        // allocate tables
        cinfo[ci].succ.assign(S, std::vector<Edge*>(A, nullptr));
        cinfo[ci].is_final.assign(S, false);
        cinfo[ci].initSucc.assign(A, nullptr);

        // fill succ and final flags
        for (size_t sid = 0; sid < S; ++sid) {
            State* s = cstates->at(sid);
            if (!s) continue;
            assert(s->getId() == static_cast<int>(sid));
            cinfo[ci].is_final[sid] = child->isFinal(s);
            
            if (cinfo[ci].is_final[sid]) {
                // final states are terminal after determinization
                continue;
            }
            for (size_t a = 0; a < A; ++a) {
                cinfo[ci].succ[sid][a] = first_edge_or_null(s->getSuccessors(a));
            }
        }

        // fill initSucc from child's initial state
        State* s0 = child->getInitial();
        if (s0) {
            const size_t init_id = static_cast<size_t>(s0->getId());
            assert(init_id < S);
            for (size_t a = 0; a < A; ++a) {
                cinfo[ci].initSucc[a] = cinfo[ci].succ[init_id][a];
            }
        }
    }

    // ---------- X, Y, Z bounds (combinatorial) ----------
    auto sat_mul = [](size_t a, size_t b) -> size_t {
        if (a == 0 || b == 0) return 0;
        const size_t maxv = std::numeric_limits<size_t>::max();
        if (a > maxv / b) return maxv;
        return a * b;
    };

    auto sat_pow = [&](size_t base, size_t exp) -> size_t {
        if (exp == 0) return 1;
        const size_t maxv = std::numeric_limits<size_t>::max();
        size_t res = 1;
        while (exp > 0) {
            if (base != 0 && res > maxv / base) return maxv;
            res *= base;
            if (res == maxv) return maxv;
            --exp;
        }
        return res;
    };

    // X = 2 * |M| * Π_i |S_i|
    size_t X_states = 2;
    X_states = sat_mul(X_states, M_states);
    for (size_t ci = 0; ci < C; ++ci) {
        if (!children[ci]) continue;
        X_states = sat_mul(X_states, cinfo[ci].num_states);
    }

    // Y = X * (|U| + 2) * |U|^{2|U|}
    size_t exp   = sat_mul(2, U);          // 2|U|
    size_t U_pow = sat_pow(U, exp);        // |U|^{2|U|}

    size_t Y = sat_mul(X_states, U + 2);
    Y = sat_mul(Y, U_pow);

    // Z = 2 * X * (|U| + 2) * |U|^{2|U|} * W_abs  (all in weight_t)
    weight_t Z = weight_zero();
    if (W_abs > weight_zero()) {
        weight_t WX = weight_two() * weight_t(X_states);
        WX = WX * weight_t(U + 2);
        WX = WX * weight_t(U_pow);
        Z  = WX * W_abs;
    }

    // ---------- flattened alphabet: copy from NWA ----------
    Symbol::RESET();
    MapArray<Symbol*>* falpha = new MapArray<Symbol*>(A);
    for (size_t a = 0; a < A; ++a) {
        Symbol* s_new = new Symbol(malpha->at(a)->getName());
        falpha->insert(s_new->getId(), s_new); // ids are 0..A-1 after RESET
    }

    // ---------- flattened states & weights ----------
    State::RESET();
    Weight::RESET();

    std::vector<State*>  fstates_vec;
    std::vector<Weight*> fweights_vec;
    fstates_vec.reserve(64);
    fweights_vec.reserve(16);

    MapStd<weight_t, Weight*> weight_register;

    weight_t flat_min       = weight_zero();
    weight_t flat_max       = weight_zero();
    bool     flat_has_weight = false;

    auto get_weight = [&](weight_t v) -> Weight* {
        if (weight_register.contains(v) != true) {
            Weight* w = new Weight(v);
            fweights_vec.push_back(w);
            weight_register.insert(v, w);

            if (!flat_has_weight) {
                flat_min = flat_max = v;
                flat_has_weight = true;
            } else {
                if (v < flat_min) flat_min = v;
                if (v > flat_max) flat_max = v;
            }
        }
        return weight_register.at(v);
    };

    // ---------- flatten state encoding ----------
    struct BoundedInst {
        size_t   child_index;
        State*   state;
        weight_t budget;  // remaining |weight|-budget ∈ [0, Z]
    };
    using UInst = std::pair<size_t, State*>; // (child_index, child_state)

    struct FlatKey {
        State*                   master;
        std::vector<UInst>       unbounded;
        std::vector<BoundedInst> bounded;

        bool operator==(const FlatKey& o) const {
            if (master != o.master) return false;
            if (unbounded.size() != o.unbounded.size()) return false;
            if (bounded.size()   != o.bounded.size())   return false;

            for (size_t i = 0; i < unbounded.size(); ++i) {
                if (unbounded[i].first  != o.unbounded[i].first)  return false;
                if (unbounded[i].second != o.unbounded[i].second) return false;
            }
            for (size_t i = 0; i < bounded.size(); ++i) {
                const BoundedInst& b1 = bounded[i];
                const BoundedInst& b2 = o.bounded[i];
                if (b1.child_index != b2.child_index) return false;
                if (b1.state       != b2.state)       return false;
                if (b1.budget      != b2.budget)      return false;
            }
            return true;
        }
    };

    struct FlatKeyHash {
        size_t operator()(FlatKey const& k) const {
            size_t h = 1469598103934665603ull;
            auto mix = [&](uint64_t x) {
                h ^= x;
                h *= 1099511628211ull;
            };
            mix(reinterpret_cast<uint64_t>(k.master));
            for (auto const& u : k.unbounded) {
                mix(static_cast<uint64_t>(u.first));
                mix(reinterpret_cast<uint64_t>(u.second));
            }
            for (auto const& b : k.bounded) {
                mix(static_cast<uint64_t>(b.child_index));
                mix(reinterpret_cast<uint64_t>(b.state));
                // we deliberately ignore budget in the hash to avoid depending on hash<weight_t>
            }
            return h;
        }
    };

    auto normalize_key = [](FlatKey& k) {
        auto cmpU = [](const UInst& a, const UInst& b) {
            if (a.first != b.first) return a.first < b.first;
            return a.second->getId() < b.second->getId();
        };
        std::sort(k.unbounded.begin(), k.unbounded.end(), cmpU);

        auto cmpB = [](const BoundedInst& a, const BoundedInst& b) {
            if (a.child_index != b.child_index) return a.child_index < b.child_index;
            int ida = a.state->getId();
            int idb = b.state->getId();
            if (ida != idb) return ida < idb;
            if (a.budget < b.budget) return true;
            if (a.budget > b.budget) return false;
            return false;
        };
        std::sort(k.bounded.begin(), k.bounded.end(), cmpB);
    };

    std::unordered_map<FlatKey, State*, FlatKeyHash> state_map;
    std::queue<FlatKey> worklist;

    auto get_or_make_state = [&](FlatKey key) -> State* {
        normalize_key(key);
        auto it = state_map.find(key);
        if (it != state_map.end()) return it->second;

        std::ostringstream ss;
        ss << "flat_" << state_map.size();
        State* ns = new State(ss.str(), A, 0, 0); // no children: domain [0,0]
        fstates_vec.push_back(ns);

        state_map.insert(std::make_pair(key, ns));
        worklist.push(key);
        return ns;
    };

    // scratch buffers reused across transitions (to reduce allocations)
    std::vector<UInst>       scratch_unbounded;
    std::vector<BoundedInst> scratch_bounded;

    // ---------- initial flat state: (master_initial, no slaves) ----------
    FlatKey initKey;
    initKey.master = this->getInitial();
    State* flat_initial = get_or_make_state(initKey);

    // ---------- BFS over flattened state space ----------
    while (!worklist.empty()) {
        FlatKey key = worklist.front();
        worklist.pop();

        State* from_flat = state_map.at(key);

        for (size_t a = 0; a < A; ++a) {
            Edge* me = masterSucc[key.master->getId()][a];
            if (!me) continue;

            State*   m2 = me->getTo();
            weight_t wm = me->getWeight()->getValue(); // ≤ 0 under Sum-

            bool ok = true;
            scratch_unbounded.clear();
            scratch_bounded.clear();

            weight_t sum_unbounded = weight_zero();
            weight_t sum_bounded   = weight_zero();

            // --- existing unbounded instances ---
            for (const UInst& u : key.unbounded) {
                size_t  ci    = u.first;
                State*  s_cur = u.second;

                ChildInfo& ch = cinfo[ci];
                if (!ch.aut) { ok = false; break; }

                const size_t sid = static_cast<size_t>(s_cur->getId());
                if (sid >= ch.succ.size()) { ok = false; break; }

                Edge* se = ch.succ[sid][a];
                if (!se) { ok = false; break; }

                State*   s2 = se->getTo();
                weight_t xu = se->getWeight()->getValue(); // ≤ 0

                sum_unbounded += xu;
                scratch_unbounded.emplace_back(ci, s2);
            }
            if (!ok) continue;

            // --- existing bounded instances ---
            for (const BoundedInst& b : key.bounded) {
                size_t   ci    = b.child_index;
                State*   s_cur = b.state;
                weight_t bud   = b.budget;

                ChildInfo& ch = cinfo[ci];
                if (!ch.aut) { ok = false; break; }

                const size_t sid = static_cast<size_t>(s_cur->getId());
                if (sid >= ch.succ.size()) { ok = false; break; }

                Edge* se = ch.succ[sid][a];
                if (!se) { ok = false; break; }

                State*   s2    = se->getTo();
                weight_t z     = se->getWeight()->getValue(); // ≤ 0
                weight_t mag_z = weight_abs(z);

                // strictly decreasing absolute budget
                if (bud < mag_z) { ok = false; break; }
                weight_t bud2 = bud - mag_z;

                sum_bounded += z;

                // if budget exhausted and child is final, drop this instance
                if (bud2 == weight_zero()) {
                    const size_t s2id = static_cast<size_t>(s2->getId());
                    if (s2id < ch.is_final.size() && ch.is_final[s2id]) {
                        continue;
                    }
                }

                scratch_bounded.push_back(BoundedInst{ci, s2, bud2});
            }
            if (!ok) continue;

            // total contribution of master + active slaves (before spawning)
            weight_t base_weight = wm + sum_unbounded + sum_bounded;

            // -------- (i) no new instantiation --------
            {
                FlatKey k2;
                k2.master    = m2;
                k2.unbounded = scratch_unbounded;
                k2.bounded   = scratch_bounded;

                State* to_flat = get_or_make_state(k2);
                Edge* e = new Edge(falpha->at(a), get_weight(base_weight), from_flat, to_flat);
                from_flat->addSuccessor(e);
                to_flat->addPredecessor(e);
            }

            // -------- (ii) spawn unbounded instance --------
            if (Y > 0 && scratch_unbounded.size() < Y && U > 0) {
                for (size_t ci = 0; ci < C; ++ci) {
                    ChildInfo& ch = cinfo[ci];
                    if (!ch.aut) continue;

                    Edge* se0 = ch.initSucc[a];
                    if (!se0) continue;

                    State*   s1    = se0->getTo();
                    weight_t x_new = se0->getWeight()->getValue(); // ≤ 0

                    FlatKey k2;
                    k2.master    = m2;
                    k2.unbounded = scratch_unbounded;
                    k2.bounded   = scratch_bounded;
                    k2.unbounded.emplace_back(ci, s1);

                    State* to_flat = get_or_make_state(k2);
                    weight_t x = base_weight + x_new;

                    Edge* e = new Edge(falpha->at(a), get_weight(x), from_flat, to_flat);
                    from_flat->addSuccessor(e);
                    to_flat->addPredecessor(e);
                }
            }

            // -------- (iii) spawn bounded instance --------
            if (U > 0 && Z > weight_zero() && scratch_bounded.size() < U) {
                for (size_t ci = 0; ci < C; ++ci) {
                    ChildInfo& ch = cinfo[ci];
                    if (!ch.aut) continue;

                    Edge* se0 = ch.initSucc[a];
                    if (!se0) continue;

                    State*   s1    = se0->getTo();
                    weight_t z     = se0->getWeight()->getValue(); // ≤ 0
                    weight_t mag_z = weight_abs(z);
                    if (mag_z > Z) continue;

                    weight_t bud2 = Z - mag_z;

                    FlatKey k2;
                    k2.master    = m2;
                    k2.unbounded = scratch_unbounded;
                    k2.bounded   = scratch_bounded;
                    k2.bounded.push_back(BoundedInst{ci, s1, bud2});

                    State* to_flat = get_or_make_state(k2);
                    weight_t x = base_weight + z; // ≤ 0

                    Edge* e = new Edge(falpha->at(a), get_weight(x), from_flat, to_flat);
                    from_flat->addSuccessor(e);
                    to_flat->addPredecessor(e);
                }
            }
        }
    }

    // ---------- materialize states / weights ----------
    const size_t state_count  = fstates_vec.size();
    const size_t weight_count = fweights_vec.size();

    MapArray<State*>*  fstates  = new MapArray<State*>(state_count);
    MapArray<Weight*>* fweights = new MapArray<Weight*>(weight_count);

    for (State* s : fstates_vec)   fstates->insert(s->getId(), s);
    for (Weight* w : fweights_vec) fweights->insert(w->getId(), w);

    if (!flat_has_weight) {
        flat_min = flat_max = weight_zero();
    }

    // ---------- select final states (logic only, still not wired into Automaton) ----------
    SetStd<State*>* flat_finals = new SetStd<State*>();
    for (const auto& kv : state_map) {
        const FlatKey& k = kv.first;
        State*         s = kv.second;
        if (k.unbounded.empty() && k.bounded.empty()) {
            flat_finals->insert(s);
        }
    }
    (void)flat_finals; // kept for future use

    // ---------- build and return a childless Automaton ----------
    std::string fname = "Flat(" + this->getName() + ")";
    Automaton* flatNA = new Automaton(
        fname,
        falpha,
        fstates,
        fweights,
        flat_min,
        flat_max,
        flat_initial
    );

    return flatNA;
}


/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////


typedef uint64_t internal_weight_t;

inline internal_weight_t to_internal(weight_t w) {
    return (internal_weight_t)w.to_float();
}

typedef struct global_exploration_data_supremum {
    // constraints
    NestedAutomaton* A;
    Parser* parser;
    internal_weight_t abs_threshold;
    unsigned int* cumulative_size;   // prefix sums for flattening (child_id, local_state) -> global index
    unsigned int track_them_all;     // bitmask with all child-state bits set
    unsigned int children_all;       // total flattened child-state count

    // given (input for current exploration frame)
    std::string global_from;         // encoded as "master_id/activation/tracking/[token_info|@inactive@]"
    unsigned int master_state_id_from;
    internal_weight_t budget_from;   // budget of the single tracked token (only valid if !inactive_from)
    unsigned int child_state_id_from;
    unsigned int child_id_from;
    bool inactive_from;              // true = no token currently being tracked for weight
    internal_weight_t global_tracking_from;   // per-child-state tracking bits
    internal_weight_t global_activation_from; // per-child-state activation bits (token present)

    // initialized per-symbol
    Symbol* symbol;
    std::vector<unsigned int> old_activation_of_children_state;
    std::vector<unsigned int> old_tracking_of_children_state;
    std::vector<unsigned int> new_activation_of_children_state;  // must restore on backtrack
    std::vector<unsigned int> new_tracking_of_children_state;    // must restore on backtrack

    // computed (output)
    unsigned int master_state_id_to;
    internal_weight_t global_edge_weight;
    internal_weight_t budget_to;
    unsigned int child_state_id_to;
    unsigned int child_id_to;
    bool inactive_to;
} data_supremum_t;

void explore_global_initialization_supremum (data_supremum_t* data);

void explore_global_failure_supremum (data_supremum_t* data) {
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

void explore_global_finalization_supremum (data_supremum_t* data, Symbol* symbol) {
    internal_weight_t global_activation_to = 0;
    internal_weight_t global_tracking_to = 0;
    for (unsigned int i = 0; i < data->children_all; i++) {
        global_activation_to = (global_activation_to << 1)
                             + data->new_activation_of_children_state[i];

        global_tracking_to = (global_tracking_to << 1)
                           + data->new_tracking_of_children_state[i];
    }

    bool global_final = false;
    // Epoch boundary: tracking_from==0 means all obligations discharged
    if (data->global_tracking_from == 0) {
        global_tracking_to = data->track_them_all;  // reset for next epoch
        // global_final = true;
        // TODO: CHECK
        global_final = data->A->getStates()->at(data->master_state_id_to)->getFinal();
    }

    // State encodes: master_id/activation/tracking/[child_id/child_state/budget | @inactive@]
    std::string global_to;
    global_to.reserve(64);
    global_to.append(std::to_string(data->master_state_id_to));
    global_to.push_back('/');
    global_to.append(std::to_string(global_activation_to));
    global_to.push_back('/');
    global_to.append(std::to_string(global_tracking_to));

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

    data->parser->edges.insert({
        { symbol->getName(), (weight_t)data->global_edge_weight },
        { data->global_from, global_to }
    });

    if (global_final == true) {
        data->parser->final_states.insert(global_to);
    }

    // DFS: only recurse on newly discovered states
    if (data->parser->states.contains(global_to) == false) {
        data->parser->states.insert(global_to);

        data_supremum_t data_deeper{};
        data_deeper.A             = data->A;
        data_deeper.parser        = data->parser;
        data_deeper.abs_threshold = data->abs_threshold;
        data_deeper.cumulative_size = data->cumulative_size;
        data_deeper.track_them_all  = data->track_them_all;
        data_deeper.children_all    = data->children_all;

        data_deeper.global_from          = global_to;
        data_deeper.master_state_id_from = data->master_state_id_to;
        data_deeper.inactive_from        = data->inactive_to;
        data_deeper.global_tracking_from = global_tracking_to;
        data_deeper.global_activation_from = global_activation_to;

        if (data->inactive_to == false) {
            data_deeper.budget_from         = data->budget_to;
            data_deeper.child_state_id_from = data->child_state_id_to;
            data_deeper.child_id_from       = data->child_id_to;
        }

        explore_global_initialization_supremum(&data_deeper);
    }
}

// Propagate background children (all except the single tracked token)
void explore_global_selection_supremum (unsigned int child_id, unsigned int child_state_id, data_supremum_t* data) {
    // Skip the tracked token -- handled separately in explore_global_child_transition_supremum
    if (child_id == data->child_id_from && child_state_id == data->child_state_id_from) {
        explore_global_selection_supremum(child_id, child_state_id + 1, data);
    }
    else if (child_id < data->A->getChildrenSize()) {
        auto* child  = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            unsigned int i = data->cumulative_size[child_id] + child_state_id;

            if (data->old_activation_of_children_state[i] == 0) {
                explore_global_selection_supremum(child_id, child_state_id + 1, data);
            }
            else if (states->at(child_state_id)->getFinal()) {
                // Final states implicitly terminate -- no successor propagation needed
                explore_global_selection_supremum(child_id, child_state_id + 1, data);
            }
            else {
                State* child_state = states->at(child_state_id);

                auto* succs = child_state->getSuccessors(data->symbol->getId());
                if (succs) {
                    for (Edge* edge : *succs) {
                        unsigned int ii = data->cumulative_size[child_id] + edge->getTo()->getId();
                        unsigned int stored_tracking   = data->new_tracking_of_children_state[ii];
                        unsigned int stored_activation = data->new_activation_of_children_state[ii];

                        // Propagate tracking/activation to successor
                        if (data->old_tracking_of_children_state[i] == 1) {
                            data->new_tracking_of_children_state[ii] = 1;
                        }
                        if (data->old_activation_of_children_state[i] == 1) {
                            data->new_activation_of_children_state[ii] = 1;
                        }

                        explore_global_selection_supremum(child_id + 1, child_state_id, data);

                        data->new_tracking_of_children_state[ii]   = stored_tracking;
                        data->new_activation_of_children_state[ii] = stored_activation;
                    }
                } else {
                    explore_global_selection_supremum(child_id + 1, child_state_id, data);
                }
            }
        }
        else {
            explore_global_selection_supremum(child_id + 1, 0, data);
        }
    }
    else {
        explore_global_finalization_supremum(data, data->symbol);
    }
}

// Handle the single tracked token's transition (the one accumulating weight)
void explore_global_child_transition_supremum (data_supremum_t* data) {
    if (data->inactive_from == true) {
        // No token being tracked -- just propagate background children
        data->inactive_to = true;
        explore_global_selection_supremum(0, 0, data);
        return;
    }

    State* child_state = data->A->getChild(data->child_id_from)->getStates()->at(data->child_state_id_from);

    if (child_state->getFinal() == true) {
        // Termination: budget must be exactly exhausted (validates the guess)
        if (data->budget_from == 0) {
            data->inactive_to = true;
            explore_global_selection_supremum(0, 0, data);
        } else {
            explore_global_failure_supremum(data);
        }
        return;
    }

    unsigned int i = data->cumulative_size[data->child_id_from] + data->child_state_id_from;

    if (child_state->getSuccessors(data->symbol->getId())) {
        for (Edge* child_edge : *child_state->getSuccessors(data->symbol->getId())) {
            unsigned int ii = data->cumulative_size[data->child_id_from] + child_edge->getTo()->getId();
            unsigned int stored_tracking = data->new_tracking_of_children_state[ii];
            unsigned int stored_activation = data->new_activation_of_children_state[ii];

            if (data->old_tracking_of_children_state[i] == 1) {
                data->new_tracking_of_children_state[ii] = 1;
            }
            if (data->old_activation_of_children_state[i] == 1) {
                data->new_activation_of_children_state[ii] = 1;
            }

            data->inactive_to = false;
            data->child_id_to = data->child_id_from;
            data->child_state_id_to = child_edge->getTo()->getId();

            internal_weight_t abs_child_edge_value;
            if (child_edge->getWeight()->getValue() < 0) {
                abs_child_edge_value = to_internal(-(child_edge->getWeight()->getValue()));
            } else {
                abs_child_edge_value = to_internal(child_edge->getWeight()->getValue());
            }

            if (data->budget_from < data->abs_threshold) {
                // Fixed budget mode: deterministic subtraction
                if (data->budget_from < abs_child_edge_value) {
                    explore_global_failure_supremum(data);
                } else {
                    data->budget_to = data->budget_from - abs_child_edge_value;
                    explore_global_selection_supremum(0, 0, data);
                }
            } else {
                // Unlimited budget (abs_threshold): nondeterministically guess successor budget
                // Any guess where guess + edge_cost >= threshold is valid (could still reach threshold)
                for (internal_weight_t abs_weight = data->abs_threshold; ; abs_weight--) {
                    if (abs_weight + abs_child_edge_value >= data->abs_threshold) {
                        data->budget_to = abs_weight;
                        explore_global_selection_supremum(0, 0, data);
                    }
                    if (abs_weight == 0) break;
                }
            }

            data->new_tracking_of_children_state[ii] = stored_tracking;
            data->new_activation_of_children_state[ii] = stored_activation;
        }
    }
}

void explore_global_master_transition_supremum (data_supremum_t* data) {
    auto succs = data->A->getStates()->at(data->master_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    // Save tracked token context -- each master edge explores independently
    unsigned int       saved_child_state_id_from = data->child_state_id_from;
    unsigned int       saved_child_id_from       = data->child_id_from;
    internal_weight_t  saved_budget_from         = data->budget_from;
    bool               saved_inactive_from       = data->inactive_from;

    for (Edge* master_edge : *succs) {
        data->child_state_id_from = saved_child_state_id_from;
        data->child_id_from       = saved_child_id_from;
        data->budget_from         = saved_budget_from;
        data->inactive_from       = saved_inactive_from;

        data->master_state_id_to = static_cast<unsigned int>(master_edge->getTo()->getId());
        unsigned int child_id = static_cast<unsigned int>(master_edge->getWeight()->getValue().to_float());

        if (data->A->getChild(child_id)->getStates()->size() == 1) {
            // Silent transition: no spawn, weight 0
            data->global_edge_weight = 0;
            explore_global_child_transition_supremum(data);
        } else {
            unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
            unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;

            // Mark spawned child as active in OLD (so selection sees it)
            unsigned prev = data->old_activation_of_children_state[ii];
            data->old_activation_of_children_state[ii] = 1;

            // Choice 1: spawn as background (not tracked for weight)
            data->global_edge_weight = 0;
            explore_global_child_transition_supremum(data);

            // Choice 2: start tracking for weight (only if no token currently tracked)
            if (saved_inactive_from) {
                data->global_edge_weight  = 1;  // signal that we're betting on this token
                data->child_state_id_from = summoned_child_state_id;
                data->child_id_from       = child_id;
                data->budget_from         = data->abs_threshold;  // start with "unlimited"
                data->inactive_from       = false;
                explore_global_child_transition_supremum(data);
            }

            data->old_activation_of_children_state[ii] = prev;
        }
    }
}

void explore_global_initialization_supremum (data_supremum_t* data) {
    internal_weight_t activation_from = data->global_activation_from;
    internal_weight_t tracking_from   = data->global_tracking_from;

    const unsigned int n = data->children_all;

    data->new_activation_of_children_state.assign(n, 0);
    data->new_tracking_of_children_state.assign(n, 0);

    data->old_activation_of_children_state.resize(n);
    data->old_tracking_of_children_state.resize(n);

    // Unpack bitmasks
    for (unsigned int i = 0; i < n; i++) {
        data->old_activation_of_children_state[i] = static_cast<unsigned int>(activation_from & 1);
        activation_from >>= 1;

        data->old_tracking_of_children_state[i] = static_cast<unsigned int>(tracking_from & 1);
        tracking_from >>= 1;
    }

    if (data->A->getStates()->at(data->master_state_id_from)->getAlphabet()) {
        for (Symbol* symbol : *(data->A->getStates()->at(data->master_state_id_from)->getAlphabet())) {
            data->symbol = symbol;
            explore_global_master_transition_supremum(data);
        }
    }
}

// Track ONE child token explicitly for weight, others as background
bool NestedAutomaton::emptiness_monotonic_nesting_supremum(value_function_t infinite_aggregator,
                                                           value_function_t finite_aggregator,
                                                           weight_t threshold) {
    if (threshold <= 0 && finite_aggregator == SumPlus) {
        return true;
    }
    if (threshold > 0 && finite_aggregator == SumMinus) {
        return false;
    }

    internal_weight_t abs_threshold;
    if (threshold > 0) {
        abs_threshold = to_internal(threshold);
    } else {
        abs_threshold = to_internal(-threshold + 1);
    }

    std::vector<unsigned int> cumulative_size(this->children_->size() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->children_->size() + 1; i++) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    unsigned int children_all = cumulative_size[this->children_->size()];

    // All bits set for epoch reset
    unsigned int track_them_all = 0;
    for (unsigned int i = 0; i < children_all; i++) {
        track_them_all = track_them_all * 2 + 1;
    }

    Parser* parser = new Parser(0, 1);
    parser->weights.insert(0);
    parser->weights.insert(1);
    parser->states.insert("@sink@");

    for (Symbol* symbol : *this->getAlphabet()) {
        parser->alphabet.insert(symbol->getName());
        std::pair<std::pair<std::string, weight_t>, std::pair<std::string, std::string>> sink_self_loop;
        sink_self_loop.first.first = symbol->getName();
        sink_self_loop.first.second = 0;
        sink_self_loop.second.first = "@sink@";
        sink_self_loop.second.second = "@sink@";
        parser->edges.insert(sink_self_loop);
    }

    // Initial: no children active, none tracked, no token being followed for weight
    std::string global_initial = "";
    global_initial = global_initial + std::to_string(this->initial->getId());
    global_initial = global_initial + "/" + std::to_string(0);  // activation=0
    global_initial = global_initial + "/" + std::to_string(0);  // tracking=0
    global_initial = global_initial + "/" + "@inactive@";
    parser->states.insert(global_initial);
    parser->initial = global_initial;

    data_supremum_t* data = new data_supremum_t();
    if (data == nullptr) QUAK_FAIL("out of memory");

    data->A = this;
    data->parser = parser;
    data->abs_threshold = abs_threshold;
    data->cumulative_size = cumulative_size.data();
    data->track_them_all = track_them_all;
    data->children_all = children_all;
    data->global_from = global_initial;
    data->master_state_id_from = this->initial->getId();
    data->inactive_from = true;
    data->global_tracking_from = 0;
    data->global_activation_from = 0;

    explore_global_initialization_supremum(data);
    delete data;

    std::string newname = "unnested(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    // unnested->print();
    std::cout << "Unnested: " << parser->states.size() << " states, " << parser->edges.size() << " edges" << std::endl;
    delete parser;

    weight_t top = unnested->compute_top_with_final(infinite_aggregator);
    bool result = (top >= 1);

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
    // unnested->print();
    delete parser;

        int numEdges = 0;
        for (size_t s = 0; s < unnested->getStates()->size(); ++s) {
            State* state = unnested->getStates()->at(s);
            if (state) {
                for (size_t a = 0; a < unnested->getAlphabet()->size(); ++a) {
                    SetStd<Edge*>* succs = state->getSuccessors(a);
                    if (succs) {
                        numEdges += succs->size();
                    }
                }
            }
        }
        std::cout << "unnested: " << unnested->getStates()->size() << " states, " << numEdges << " edges" << std::endl;
        std::cout << unnested->getNbSCCs() << " SCCs (" << unnested->getNbAcceptingSCCs() << " accepting)" << std::endl;

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
        max_weight = std::max(max_weight, this->getChild(i)->getMaxDomain().to_uint()); // TODO: CHECK
    } 
    theoretical_bound = theoretical_bound * max_weight * this->getStates()->size();


    // Fast path: if supremum is unbounded, threshold is always achievable
    if (emptiness_monotonic_nesting_supremum(LimSup, SumPlus, theoretical_bound)) {
        return true;
    }
    else {
        Automaton* buchi = flatten_regular(SumB, theoretical_bound); // Key Lemma construction
        int numEdges = 0;
        for (size_t s = 0; s < buchi->getStates()->size(); ++s) {
            State* state = buchi->getStates()->at(s);
            if (state) {
                for (size_t a = 0; a < buchi->getAlphabet()->size(); ++a) {
                    SetStd<Edge*>* succs = state->getSuccessors(a);
                    if (succs) {
                        numEdges += succs->size();
                    }
                }
            }
        }
        std::cout << "buchi: " << buchi->getStates()->size() << " states, " << numEdges << " edges" << std::endl;
        std::cout << buchi->getNbSCCs() << " SCCs (" << buchi->getNbAcceptingSCCs() << " accepting)" << std::endl;
        // buchi->print();

        Automaton* modBuchi = Automaton::removeSilentTransitions(buchi, LimSupAvg, true);
        int numEdges2 = 0;
        for (size_t s = 0; s < modBuchi->getStates()->size(); ++s) {
            State* state = modBuchi->getStates()->at(s);
            if (state) {
                for (size_t a = 0; a < modBuchi->getAlphabet()->size(); ++a) {
                    SetStd<Edge*>* succs = state->getSuccessors(a);
                    if (succs) {
                        numEdges2 += succs->size();
                    }
                }
            }
        }
        std::cout << "modBuchi: " << modBuchi->getStates()->size() << " states, " << numEdges2 << " edges" << std::endl;
        std::cout << modBuchi->getNbSCCs() << " SCCs (" << modBuchi->getNbAcceptingSCCs() << " accepting)" << std::endl;
        // modBuchi->print();
        
        bool res = modBuchi->emptiness_LimAvg_with_final(threshold);
        // bool res = buchi->emptiness_LimAvg_with_final(threshold);
        // bool res = (modBuchi->compute_top_with_final(LimInfAvg)) >= threshold);
        delete modBuchi;
        delete buchi;
        return res;

        // weight_t top = modBuchi->compute_top_with_final(infinite_aggregator);
        // // weight_t top = modBuchi->getTopValue(infinite_aggregator);
        // std::cout << "modBuchi top: " << top.to_string() << std::endl;
        // delete modBuchi;
        // return (top >= threshold);

        
        // translate to Buchi using SumB and theoretical_bound: use Automaton with accepting states
        // remove silent transitions
        // compute top with infinite_aggregator and compare to threshold

        //ChildAutomaton* child = transformToBuchi(SumB, theoretical_bound); // Key Lemma construction
        //Automaton* unnested = transform2Quantitative(child); // this just makes states final 
        //delete child;
        //bool result = (unnested->compute_top_with_final(infinite_aggregator) >= threshold);
        //delete unnested;
        //return result;
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

static inline bool tracking_all_zero(const std::vector<unsigned char>& v) {
    for (unsigned char b : v) {
        if (b != 0) return false;
    }
    return true;
}

static std::string bits_to_string(const std::vector<unsigned char>& v) {
    std::string s;
    s.reserve(v.size());
    for (unsigned char b : v) {
        s.push_back(b ? '1' : '0');
    }
    return s;
}

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
                        explore_global_selection_min_max_supremum(child_id + 1, child_state_id, data);
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

                    explore_global_selection_min_max_supremum(child_id + 1, child_state_id, data);

                    data->new_tracking[ii] = stored_tracking;
                    data->new_activation[ii] = stored_activation;
                }
            } else {
                // No successors on this symbol: treat as silent "no move" for background
                explore_global_selection_min_max_supremum(child_id + 1, child_state_id, data);
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

        int numEdges = 0;
        for (size_t s = 0; s < unnested->getStates()->size(); ++s) {
            State* state = unnested->getStates()->at(s);
            if (state) {
                for (size_t a = 0; a < unnested->getAlphabet()->size(); ++a) {
                    SetStd<Edge*>* succs = state->getSuccessors(a);
                    if (succs) {
                        numEdges += succs->size();
                    }
                }
            }
        }
        std::cout << "unnested: " << unnested->getStates()->size() << " states, " << numEdges << " edges" << std::endl;
        std::cout << unnested->getNbSCCs() << " SCCs (" << unnested->getNbAcceptingSCCs() << " accepting)" << std::endl;

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
    // unnested->print();

        int numEdges = 0;
        for (size_t s = 0; s < unnested->getStates()->size(); ++s) {
            State* state = unnested->getStates()->at(s);
            if (state) {
                for (size_t a = 0; a < unnested->getAlphabet()->size(); ++a) {
                    SetStd<Edge*>* succs = state->getSuccessors(a);
                    if (succs) {
                        numEdges += succs->size();
                    }
                }
            }
        }
        std::cout << "unnested: " << unnested->getStates()->size() << " states, " << numEdges << " edges" << std::endl;
        std::cout << unnested->getNbSCCs() << " SCCs (" << unnested->getNbAcceptingSCCs() << " accepting)" << std::endl;

    delete parser;

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
        else { // (LimInfAvg, SumPlus) nonemptiness is open
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
        else if (infVal == LimInfAvg || infVal == LimSupAvg) {  // TODO: FIX
            NestedAutomaton* det_nwa = nullptr;
            NestedAutomaton* sync_nwa;
            std::vector<bool> complete_flags;

            if (this->isDeterministicNested() && this->isCompleteNested(&complete_flags)) {
                sync_nwa = this->synchronizeChildren();
            }
            else if (this->isDeterministicNested() && !this->isCompleteNested(&complete_flags)) {
                det_nwa = this->makeCompleteNested(&complete_flags);
                sync_nwa = det_nwa->synchronizeChildren();
            }
            else {
                det_nwa = this->determinizeWithMacroAlphabet();
                sync_nwa = det_nwa->synchronizeChildren();
            }

            Automaton* flat = sync_nwa->flatten_Avg_SumMinus();
            weight_t topFlat = flat->getTopValue(infVal);

            delete flat;
            delete sync_nwa;
            delete det_nwa;

            return (topFlat >= x);

            // NestedAutomaton* det_nwa;
            // if (this->isDeterministicAndCompleteNested()) {
            //     det_nwa = this;
            // } else {
            //     det_nwa = this->determinizeWithMacroAlphabet();
            // }
            // NestedAutomaton* sync_nwa = det_nwa->synchronizeChildren();
            // Automaton* flat = sync_nwa->flatten_Avg_SumMinus();
            // auto topFlat = flat->getTopValue(infVal);
            // delete flat;
            // delete sync_nwa;
            // delete det_nwa;
            // return (topFlat >= x);
        }
        else {
            QUAK_FAIL("isNonEmpty: unsupported infinite aggregator with SumMinus");
        }
    }
    else if (finVal == Max_f || finVal == Min_f) {
        if (infVal == Sup || infVal == LimSup) {
            return this->emptiness_monotonic_nesting_min_max_supremum(infVal, finVal, x);
            // return this->emptiness_monotonic_nesting_min_max(infVal, finVal, x);
            // Automaton* flat = this->flatten_regular(finVal);
            // Automaton* nonSilent = Automaton::removeSilentTransitions(flat, infVal);
            // // flat->print();
            // // nonSilent->print();
            // weight_t topFlat = nonSilent->compute_top_with_final(infVal);
            // delete nonSilent;
            // delete flat;
            // return (topFlat >= x);
        }
        else if (infVal == Inf || infVal == LimInf) {
            return this->emptiness_monotonic_nesting_min_max(infVal, finVal, x);
        }
        else if (infVal == LimInfAvg || infVal == LimSupAvg) {
            Automaton* flat = this->flatten_regular(finVal);
            std::cout << flat->getNbStates() << " " << flat->getNbSCCs() << " " << flat->getNbAcceptingSCCs() << std::endl;
            Automaton* nonSilent = Automaton::removeSilentTransitions(flat, infVal, true);
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
            // nonSilent->print();
            bool res = nonSilent->isUniversal(infVal, 1);

            delete nonSilent;
            delete flat;
            return res;
        }
        else {
            QUAK_FAIL("isUniversal: unsupported infinite aggregator");
        }
    }
    else {
        QUAK_FAIL("isUniversal: unsupported finite aggregator");
    }
}