// Extracted backend reference file.
//
// Contents:
// - split-witness {Sup, LimSup} x {Min, Max} backend code
// - immediate helper/dependency blocks copied from the archived source
//
// Source origins:
// - src/NestedAutomaton_OLD_V2.cpp

#include <queue>
#include <set>

#include "src/NestedAutomaton.h"
#include "src/Edge.h"
#include "src/utility.h"

// ===== From src/NestedAutomaton_OLD_V2.cpp: small helpers used by split-witness backend =====

static inline bool tracking_all_zero(const std::vector<unsigned char>& v) {
    for (unsigned char b : v) {
        if (b != 0) return false;
    }
    return true;
}

static inline bool any_one(const std::vector<unsigned char>& v) {
    for (unsigned char b : v) {
        if (b != 0) return true;
    }
    return false;
}

static inline std::string bits_to_string(const std::vector<unsigned char>& v) {
    std::string s;
    s.reserve(v.size());
    for (unsigned char b : v) s.push_back(b ? '1' : '0');
    return s;
}

static std::vector<bool> compute_can_reach_final_child(ChildAutomaton* child) {
    auto* states = child->getStates();
    if (!states) return {};

    const unsigned int n = states->size();
    std::vector<bool> can_reach(n, false);
    std::vector<bool> visited(n, false);
    std::queue<unsigned int> q;
    for (unsigned int i = 0; i < n; ++i) {
        if (states->at(i)->getFinal()) {
            can_reach[i] = true;
            q.push(i);
            visited[i] = true;
        }
    }
    while (!q.empty()) {
        unsigned int cur = q.front();
        q.pop();
        for (unsigned int pred = 0; pred < n; ++pred) {
            if (visited[pred]) continue;
            State* pred_state = states->at(pred);
            for (Symbol* sym : *pred_state->getAlphabet()) {
                auto* succs = pred_state->getSuccessors(sym->getId());
                if (!succs) continue;
                for (Edge* e : *succs) {
                    if ((unsigned int)e->getTo()->getId() == cur) {
                        can_reach[pred] = true;
                        visited[pred] = true;
                        q.push(pred);
                        goto next_pred;
                    }
                }
            }
            next_pred:;
        }
    }
    return can_reach;
}

// ===== From src/NestedAutomaton_OLD_V2.cpp: split-witness Min/Max Sup/LimSup backend =====

enum split_accept_phase_t : unsigned int {
    SPLIT_ACC_IDLE = 0u,
    SPLIT_ACC_PARENT = 1u,
    SPLIT_ACC_COMPLETE = 2u,
    SPLIT_ACC_PULSE = 3u,
};

struct min_max_sup_split_work_item {
    std::string global_from;
    unsigned int parent_state_id_from;

    // Background tokens only.
    std::vector<unsigned char> bg_activation_from;
    std::vector<unsigned char> bg_tracking_from;

    bool inactive_from = true;
    unsigned int witness_child_id_from = 0;
    unsigned int witness_child_state_id_from = 0;
    unsigned int witness_y_from = 0;
    unsigned int witness_tracked_from = 0;
    unsigned int accept_phase_from = SPLIT_ACC_IDLE;
    bool epoch_nonempty_from = false;
};

typedef struct global_exploration_data_min_max_sup_split {
    NestedAutomaton* A = nullptr;
    Parser* parser = nullptr;
    weight_t threshold{};
    unsigned int* cumulative_size = nullptr;
    unsigned int children_all = 0;
    unsigned int finite_is_max = 0; // 0 = Min_f, 1 = Max_f

    std::vector<unsigned char> track_them_all;
    std::vector<min_max_sup_split_work_item>* worklist = nullptr;
    std::vector<bool> can_reach_final;

    std::string global_from;
    unsigned int parent_state_id_from = 0;
    std::vector<unsigned char> bg_activation_from;
    std::vector<unsigned char> bg_tracking_from;
    bool inactive_from = true;
    unsigned int witness_child_id_from = 0;
    unsigned int witness_child_state_id_from = 0;
    unsigned int witness_y_from = 0;
    unsigned int witness_tracked_from = 0;
    unsigned int accept_phase_from = SPLIT_ACC_IDLE;
    bool epoch_nonempty_from = false;

    Symbol* symbol = nullptr;
    std::vector<unsigned char> old_bg_activation;
    std::vector<unsigned char> old_bg_tracking;
    std::vector<unsigned char> new_bg_activation;
    std::vector<unsigned char> new_bg_tracking;
    bool spawned_from = false;
    bool spawned_is_witness = false;
    unsigned int spawned_child_id_from = 0;
    unsigned int spawned_child_state_id_from = 0;
    bool current_parent_edge_is_real = false;

    unsigned int parent_state_id_to = 0;
    weight_t global_edge_weight = 0;
    bool inactive_to = true;
    unsigned int witness_child_id_to = 0;
    unsigned int witness_child_state_id_to = 0;
    unsigned int witness_y_to = 0;
    unsigned int witness_tracked_to = 0;
} data_min_max_sup_split_t;

static void explore_global_initialization_min_max_sup_split(data_min_max_sup_split_t* data);
static void explore_global_parent_transition_min_max_sup_split(data_min_max_sup_split_t* data);
static void explore_global_child_transition_min_max_sup_split(data_min_max_sup_split_t* data);
static void explore_global_selection_min_max_sup_split(unsigned int child_id,
                                                       unsigned int child_state_id,
                                                       data_min_max_sup_split_t* data);
static void explore_global_finalization_min_max_sup_split(data_min_max_sup_split_t* data);
static void explore_global_failure_min_max_sup_split(data_min_max_sup_split_t* data);

static inline unsigned int min_max_y_update_split(const weight_t& edge_value,
                                                  unsigned int y_current,
                                                  const data_min_max_sup_split_t* data) {
    const bool pass = !(edge_value < data->threshold);
    if (data->finite_is_max) {
        return (y_current != 0u || pass) ? 1u : 0u;
    }
    return (y_current != 0u && pass) ? 1u : 0u;
}

static void explore_global_failure_min_max_sup_split(data_min_max_sup_split_t* data) {
#ifdef DEBUG
    std::cout << "FAILURE_SPLIT: " << data->symbol->getName() << " from " << data->global_from
              << " -> @sink@" << std::endl;
#endif
    data->parser->edges.insert({
        { data->symbol->getName(), weight_t(0) },
        { data->global_from, "@sink@" }
    });
}

static void explore_global_finalization_min_max_sup_split(data_min_max_sup_split_t* data) {
    std::vector<unsigned char> bg_activation_to = data->new_bg_activation;
    std::vector<unsigned char> bg_tracking_to = data->new_bg_tracking;
    unsigned int witness_tracked_to = data->inactive_to ? 0u : data->witness_tracked_to;

    const bool parent_final_to = data->A->getStates()->at(data->parent_state_id_to)->getFinal();
    const bool destination_tracking_all_zero =
        tracking_all_zero(bg_tracking_to) &&
        (data->inactive_to || witness_tracked_to == 0u);
    const bool epoch_nonempty_here =
        data->epoch_nonempty_from || data->current_parent_edge_is_real;
    const bool completion_now =
        destination_tracking_all_zero && epoch_nonempty_here;

    bool parent_seen_to = false;
    bool completion_seen_to = false;
    if (data->accept_phase_from == SPLIT_ACC_PARENT) {
        parent_seen_to = true;
    } else if (data->accept_phase_from == SPLIT_ACC_COMPLETE) {
        completion_seen_to = true;
    }
    parent_seen_to = parent_seen_to || parent_final_to;
    completion_seen_to = completion_seen_to || completion_now;

    if (destination_tracking_all_zero) {
        // The split witness can discharge the last tracked obligation on the
        // current symbol (for example by terminating on this step), so epoch
        // completion must be detected on the raw destination tracking state
        // before we reset obligations for the next epoch.
        bg_tracking_to = data->track_them_all;
        if (!data->inactive_to) {
            witness_tracked_to = 1u;
        }
    }

    const bool next_epoch_active =
        (!data->inactive_to) || any_one(bg_activation_to);
    const bool epoch_nonempty_to =
        destination_tracking_all_zero ? next_epoch_active : epoch_nonempty_here;

    unsigned int accept_phase_to = SPLIT_ACC_IDLE;
    bool global_final = false;
    if (parent_seen_to && completion_seen_to) {
        accept_phase_to = SPLIT_ACC_PULSE;
        global_final = true;
    } else if (parent_seen_to) {
        accept_phase_to = SPLIT_ACC_PARENT;
    } else if (completion_seen_to) {
        accept_phase_to = SPLIT_ACC_COMPLETE;
    }

    const bool has_tracked_to =
        !tracking_all_zero(bg_tracking_to) || (!data->inactive_to && witness_tracked_to != 0u);
    if (has_tracked_to) {
        for (unsigned int i = 0; i < data->children_all; ++i) {
            if (bg_activation_to[i] && bg_tracking_to[i] && !data->can_reach_final[i]) {
                explore_global_failure_min_max_sup_split(data);
                return;
            }
        }
        if (!data->inactive_to && witness_tracked_to != 0u) {
            const unsigned int wi = data->cumulative_size[data->witness_child_id_to] +
                                    data->witness_child_state_id_to;
            if (!data->can_reach_final[wi]) {
                explore_global_failure_min_max_sup_split(data);
                return;
            }
        }
    }

    std::string global_to;
    global_to.reserve(70 + data->children_all * 2);
    global_to.append(std::to_string(data->parent_state_id_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(bg_activation_to));
    global_to.push_back('/');
    global_to.append(bits_to_string(bg_tracking_to));
    global_to.push_back('/');
    global_to.append(std::to_string(accept_phase_to));
    global_to.push_back('/');
    global_to.push_back(epoch_nonempty_to ? '1' : '0');

    if (data->inactive_to) {
        global_to.append("/@inactive@");
    } else {
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_child_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_child_state_id_to));
        global_to.push_back('/');
        global_to.append(std::to_string(data->witness_y_to));
        global_to.push_back('/');
        global_to.append(std::to_string(witness_tracked_to));
    }

    data->parser->edges.insert({
        { data->symbol->getName(), data->global_edge_weight },
        { data->global_from, global_to }
    });

#ifdef DEBUG
    std::cout << "SUCCESS_SPLIT: " << data->symbol->getName() << " : "
              << data->global_edge_weight.to_float() << ", "
              << data->global_from << " -> " << global_to << std::endl;
#endif

    if (global_final) {
        data->parser->final_states.insert(global_to);
    }

    if (!data->parser->states.contains(global_to)) {
        data->parser->states.insert(global_to);
        data->worklist->push_back({
            global_to,
            data->parent_state_id_to,
            std::move(bg_activation_to),
            std::move(bg_tracking_to),
            data->inactive_to,
            data->witness_child_id_to,
            data->witness_child_state_id_to,
            data->witness_y_to,
            witness_tracked_to,
            accept_phase_to,
            epoch_nonempty_to
        });
    }
}

static void explore_global_selection_min_max_sup_split(unsigned int child_id,
                                                       unsigned int child_state_id,
                                                       data_min_max_sup_split_t* data) {
    if (child_id < data->A->getChildrenSize()) {
        ChildAutomaton* child = data->A->getChild(child_id);
        auto* states = child->getStates();

        if (child_state_id < states->size()) {
            const unsigned int i = data->cumulative_size[child_id] + child_state_id;
            const bool freshly_spawned_background =
                data->spawned_from &&
                !data->spawned_is_witness &&
                child_id == data->spawned_child_id_from &&
                child_state_id == data->spawned_child_state_id_from;

            if (data->old_bg_activation[i] == 0) {
                explore_global_selection_min_max_sup_split(child_id, child_state_id + 1, data);
                return;
            }

            if (!freshly_spawned_background && states->at(child_state_id)->getFinal()) {
                explore_global_selection_min_max_sup_split(child_id, child_state_id + 1, data);
                return;
            }

            State* child_state = states->at(child_state_id);
            auto* succs = child_state->getSuccessors(data->symbol->getId());
            if (succs) {
                for (Edge* edge : *succs) {
                    if (edge->getTo()->getFinal()) {
                        explore_global_selection_min_max_sup_split(child_id, child_state_id + 1, data);
                        continue;
                    }

                    const unsigned int ii = data->cumulative_size[child_id] +
                                            (unsigned int)edge->getTo()->getId();
                    const unsigned char stored_tracking = data->new_bg_tracking[ii];
                    const unsigned char stored_activation = data->new_bg_activation[ii];

                    if (data->old_bg_tracking[i] == 1) {
                        data->new_bg_tracking[ii] = 1;
                    }
                    if (data->old_bg_activation[i] == 1) {
                        data->new_bg_activation[ii] = 1;
                    }

                    explore_global_selection_min_max_sup_split(child_id, child_state_id + 1, data);

                    data->new_bg_tracking[ii] = stored_tracking;
                    data->new_bg_activation[ii] = stored_activation;
                }
            } else {
                if (freshly_spawned_background) {
                    explore_global_failure_min_max_sup_split(data);
                } else {
                    explore_global_selection_min_max_sup_split(child_id, child_state_id + 1, data);
                }
            }
        } else {
            explore_global_selection_min_max_sup_split(child_id + 1, 0, data);
        }
    } else {
        explore_global_finalization_min_max_sup_split(data);
    }
}

static void explore_global_child_transition_min_max_sup_split(data_min_max_sup_split_t* data) {
    if (data->inactive_from) {
        data->inactive_to = true;
        data->witness_tracked_to = 0u;
        explore_global_selection_min_max_sup_split(0, 0, data);
        return;
    }

    ChildAutomaton* child = data->A->getChild(data->witness_child_id_from);
    State* child_state = child->getStates()->at(data->witness_child_state_id_from);
    const bool freshly_spawned_witness =
        data->spawned_from &&
        data->spawned_is_witness &&
        data->witness_child_id_from == data->spawned_child_id_from &&
        data->witness_child_state_id_from == data->spawned_child_state_id_from;

    if (!freshly_spawned_witness && child_state->getFinal()) {
        if (data->witness_y_from == 1u) {
            data->inactive_to = true;
            data->witness_tracked_to = 0u;
            explore_global_selection_min_max_sup_split(0, 0, data);
        } else {
            explore_global_failure_min_max_sup_split(data);
        }
        return;
    }

    auto* succs = child_state->getSuccessors(data->symbol->getId());
    if (!succs) {
        explore_global_failure_min_max_sup_split(data);
        return;
    }

    for (Edge* child_edge : *succs) {
        const unsigned int to_state_id = (unsigned int)child_edge->getTo()->getId();
        const unsigned int y_next =
            min_max_y_update_split(child_edge->getWeight()->getValue(), data->witness_y_from, data);

        if (!data->finite_is_max && y_next == 0u) {
            explore_global_failure_min_max_sup_split(data);
            continue;
        }

        if (child_edge->getTo()->getFinal()) {
            if (y_next == 1u) {
                const weight_t stored_edge_weight = data->global_edge_weight;
                data->global_edge_weight = 1;
                data->inactive_to = true;
                data->witness_tracked_to = 0u;
                explore_global_selection_min_max_sup_split(0, 0, data);
                data->global_edge_weight = stored_edge_weight;
            } else {
                explore_global_failure_min_max_sup_split(data);
            }
            continue;
        }

        data->inactive_to = false;
        data->witness_child_id_to = data->witness_child_id_from;
        data->witness_child_state_id_to = to_state_id;
        data->witness_y_to = y_next;
        data->witness_tracked_to = data->witness_tracked_from;

        explore_global_selection_min_max_sup_split(0, 0, data);
    }
}

static void explore_global_parent_transition_min_max_sup_split(data_min_max_sup_split_t* data) {
    auto* succs = data->A->getStates()->at(data->parent_state_id_from)->getSuccessors(data->symbol->getId());
    if (!succs) return;

    const bool saved_inactive = data->inactive_from;
    const unsigned int saved_w_child = data->witness_child_id_from;
    const unsigned int saved_w_state = data->witness_child_state_id_from;
    const unsigned int saved_w_y = data->witness_y_from;
    const unsigned int saved_w_tracked = data->witness_tracked_from;
    const bool saved_spawned_from = data->spawned_from;
    const bool saved_spawned_is_witness = data->spawned_is_witness;
    const unsigned int saved_spawned_child = data->spawned_child_id_from;
    const unsigned int saved_spawned_state = data->spawned_child_state_id_from;

    for (Edge* parent_edge : *succs) {
        data->inactive_from = saved_inactive;
        data->witness_child_id_from = saved_w_child;
        data->witness_child_state_id_from = saved_w_state;
        data->witness_y_from = saved_w_y;
        data->witness_tracked_from = saved_w_tracked;
        data->spawned_from = false;
        data->spawned_is_witness = false;
        data->spawned_child_id_from = 0;
        data->spawned_child_state_id_from = 0;

        data->parent_state_id_to = static_cast<unsigned int>(parent_edge->getTo()->getId());
        const unsigned int child_id = static_cast<unsigned int>(parent_edge->getWeight()->getValue().to_float());
        data->current_parent_edge_is_real =
            (data->A->getChild(child_id)->getStates()->size() > 1);

        if (!data->current_parent_edge_is_real) {
            data->global_edge_weight = 0;
            explore_global_child_transition_min_max_sup_split(data);
        } else {
            const unsigned int summoned_child_state_id = data->A->getChild(child_id)->initial->getId();
            const unsigned int ii = data->cumulative_size[child_id] + summoned_child_state_id;
            const unsigned char prev_bg_act = data->old_bg_activation[ii];

            data->global_edge_weight = 0;
            data->old_bg_activation[ii] = 1;
            data->spawned_from = true;
            data->spawned_is_witness = false;
            data->spawned_child_id_from = child_id;
            data->spawned_child_state_id_from = summoned_child_state_id;
            explore_global_child_transition_min_max_sup_split(data);
            data->old_bg_activation[ii] = prev_bg_act;

            if (saved_inactive) {
                data->global_edge_weight = 0;
                data->inactive_from = false;
                data->witness_child_id_from = child_id;
                data->witness_child_state_id_from = summoned_child_state_id;
                data->witness_y_from = data->finite_is_max ? 0u : 1u;
                data->witness_tracked_from = 0u;
                data->spawned_from = true;
                data->spawned_is_witness = true;
                data->spawned_child_id_from = child_id;
                data->spawned_child_state_id_from = summoned_child_state_id;
                explore_global_child_transition_min_max_sup_split(data);
            }

            data->spawned_from = false;
            data->spawned_is_witness = false;
            data->spawned_child_id_from = 0;
            data->spawned_child_state_id_from = 0;
        }
    }

    data->spawned_from = saved_spawned_from;
    data->spawned_is_witness = saved_spawned_is_witness;
    data->spawned_child_id_from = saved_spawned_child;
    data->spawned_child_state_id_from = saved_spawned_state;
}

static void explore_global_initialization_min_max_sup_split(data_min_max_sup_split_t* data) {
    const unsigned int n = data->children_all;

    data->old_bg_activation.resize(n);
    data->old_bg_tracking.resize(n);
    data->new_bg_activation.assign(n, 0);
    data->new_bg_tracking.assign(n, 0);

    for (unsigned int i = 0; i < n; ++i) {
        data->old_bg_activation[i] =
            (i < data->bg_activation_from.size()) ? (data->bg_activation_from[i] ? 1 : 0) : 0;
        data->old_bg_tracking[i] =
            (i < data->bg_tracking_from.size()) ? (data->bg_tracking_from[i] ? 1 : 0) : 0;
    }

    auto* alphabet = data->A->getStates()->at(data->parent_state_id_from)->getAlphabet();
    if (!alphabet) return;

    const bool saved_inactive = data->inactive_from;
    const unsigned int saved_w_child = data->witness_child_id_from;
    const unsigned int saved_w_state = data->witness_child_state_id_from;
    const unsigned int saved_w_y = data->witness_y_from;
    const unsigned int saved_w_tracked = data->witness_tracked_from;

    for (Symbol* symbol : *alphabet) {
        data->inactive_from = saved_inactive;
        data->witness_child_id_from = saved_w_child;
        data->witness_child_state_id_from = saved_w_state;
        data->witness_y_from = saved_w_y;
        data->witness_tracked_from = saved_w_tracked;

        data->symbol = symbol;
        explore_global_parent_transition_min_max_sup_split(data);
    }
}

Automaton* NestedAutomaton::flatten_MinMax_Sup_split_witness(value_function_t finite_aggregator,
                                                             weight_t threshold) {
    unsigned int finite_is_max = 0u;
    if (finite_aggregator == Max_f) {
        finite_is_max = 1u;
    } else if (finite_aggregator == Min_f) {
        finite_is_max = 0u;
    } else {
        QUAK_FAIL("flatten_MinMax_Sup_split_witness: requires Min_f/Max_f");
    }

    std::vector<unsigned int> cumulative_size(this->getChildrenSize() + 1);
    cumulative_size[0] = 0;
    for (unsigned int i = 1; i < this->getChildrenSize() + 1; ++i) {
        cumulative_size[i] = cumulative_size[i - 1] + this->getChild(i - 1)->getStates()->size();
    }
    const unsigned int children_all = cumulative_size[this->getChildrenSize()];

    std::vector<unsigned char> track_them_all(children_all, 1);

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

    std::vector<unsigned char> zero(children_all, 0);
    std::string global_initial;
    global_initial.reserve(70 + children_all * 2);
    global_initial.append(std::to_string(this->initial->getId()));
    global_initial.push_back('/');
    global_initial.append(bits_to_string(zero));
    global_initial.push_back('/');
    global_initial.append(bits_to_string(zero));
    global_initial.append("/0/0/@inactive@");

    parser->states.insert(global_initial);
    parser->initial = global_initial;

    std::vector<min_max_sup_split_work_item> worklist;
    worklist.push_back({
        global_initial,
        (unsigned int)this->initial->getId(),
        zero,
        zero,
        true,
        0u,
        0u,
        finite_is_max ? 0u : 1u,
        0u,
        SPLIT_ACC_IDLE,
        false
    });

    std::vector<bool> crf_all(children_all, false);
    for (unsigned int cid = 0; cid < this->getChildrenSize(); ++cid) {
        auto crf = compute_can_reach_final_child(this->getChild(cid));
        for (unsigned int sid = 0; sid < crf.size(); ++sid) {
            crf_all[cumulative_size[cid] + sid] = crf[sid];
        }
    }

    data_min_max_sup_split_t data{};
    data.A = this;
    data.parser = parser;
    data.threshold = threshold;
    data.cumulative_size = cumulative_size.data();
    data.children_all = children_all;
    data.finite_is_max = finite_is_max;
    data.track_them_all = std::move(track_them_all);
    data.worklist = &worklist;
    data.can_reach_final = std::move(crf_all);

    while (!worklist.empty()) {
        min_max_sup_split_work_item item = std::move(worklist.back());
        worklist.pop_back();

        data.global_from = std::move(item.global_from);
        data.parent_state_id_from = item.parent_state_id_from;
        data.bg_activation_from = std::move(item.bg_activation_from);
        data.bg_tracking_from = std::move(item.bg_tracking_from);
        data.inactive_from = item.inactive_from;
        data.witness_child_id_from = item.witness_child_id_from;
        data.witness_child_state_id_from = item.witness_child_state_id_from;
        data.witness_y_from = item.witness_y_from;
        data.witness_tracked_from = item.witness_tracked_from;
        data.accept_phase_from = item.accept_phase_from;
        data.epoch_nonempty_from = item.epoch_nonempty_from;

        explore_global_initialization_min_max_sup_split(&data);
    }

    std::string newname = "unnested_split_witness(" + this->getName() + ")";
    MapStd<std::string, Symbol*> sync_register;
    Automaton* unnested = new Automaton(newname, parser, sync_register);
    delete parser;

    return unnested;
}
