// Extracted backend reference file.
//
// Contents:
// - cached {Inf, LimInf} x {Min, Max, SumPlus, SumMinus} backend code
// - split-witness {Sup, LimSup} x {Min, Max} backend code
// - immediate helper/dependency blocks copied from the corresponding sources
//
// Source origins:
// - src/NestedAutomaton.cpp
// - src/NestedAutomaton_OLD_V2.cpp

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

#include "src/NestedAutomaton.h"
#include "src/Edge.h"
#include "src/utility.h"
#include "src/FORKLIFT/inclusion.h"

// ===== From src/NestedAutomaton.cpp: acceptance phase enum shared by current Inf/LimInf backends =====

enum acc_phase_t : uint8_t { ACC_WAIT_parent = 0, ACC_WAIT_P2EMPTY = 1 };

// ===== From src/NestedAutomaton.cpp: ChildTables helpers =====

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

static inline size_t edgeWeightToChildIndex(const weight_t& w) {
    float f = w.to_float();
    if (f <= 0.0f) return 0;
    return static_cast<size_t>(f);
}

// ===== From src/NestedAutomaton.cpp: weight-scaling / tracking helpers =====

typedef uint64_t internal_weight_t;  // Integer type with scaling for exact arithmetic

// Compute scale factor to convert fractional weights to integers
// Returns the smallest power of 10 that makes all weights integers
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

// Scale weight and round to nearest integer for exact arithmetic
inline internal_weight_t to_internal(weight_t w, internal_weight_t scale) {
    float scaled = w.to_float() * scale;
    return static_cast<internal_weight_t>(scaled + 0.5f);
}

// Scale weight and truncate to integer (for thresholds)
// Truncation ensures correct boundary behavior for non-integer thresholds
// E.g., threshold=-0.5: abs_threshold=trunc(0.5)=0, so child_sum=-1 gives budget=-1, fail
inline internal_weight_t to_internal_trunc(weight_t w, internal_weight_t scale) {
    float scaled = w.to_float() * scale;
    return static_cast<internal_weight_t>(scaled);  // truncates toward zero
}

// Legacy version without scale (for backward compatibility where scale=1)
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

// ===== From src/NestedAutomaton.cpp: shared thrext helpers used by cached Inf/LimInf backends =====

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

    for (uint32_t st = 0; st < T.n_states; ++st) {
        if (T.is_final[st]) continue;

        for (uint8_t p = 0; p <= 1u; ++p) {
            for (uint32_t sym = 0; sym < T.alph; ++sym) {
                const uint32_t cell = T.idx(st, sym);
                const uint32_t b = T.off[static_cast<size_t>(cell)];
                const uint32_t e = T.off[static_cast<size_t>(cell) + 1u];

                for (uint32_t pos = b; pos < e; ++pos) {
                    const auto& tr = T.edges[static_cast<size_t>(pos)];
                    if (tr.to >= T.n_states) continue;

                    const uint8_t p2 = static_cast<uint8_t>(thrext_step_prog(info, p, tr.w));
                    if (T.is_final[tr.to]) continue;

                    rev[node_id(tr.to, p2)].push_back(node_id(st, p));
                }
            }
        }
    }

    for (uint8_t guess = 0; guess <= 1u; ++guess) {
        std::vector<uint8_t> seen(prod_sz, 0u);
        std::deque<uint32_t> q;

        for (uint32_t st = 0; st < T.n_states; ++st) {
            if (T.is_final[st]) continue;

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
            if (T.is_final[st]) continue;
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
        if (T.is_final[st]) continue;
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
            if (T.is_final[tr.to]) {
                if (thrext_discharge_ok(info, guess, prog2)) {
                    next_conf.clear();
                    return ThrExtStepStatus::DISCHARGED;
                }
                continue;
            }

            if (!thrext_is_live(T, info, guess, tr.to, prog2)) continue;
            next_conf.push_back(ThrExtConf{tr.to, prog2});
        }
    }

    thrext_frontier_canonicalize(next_conf, info, guess);
    if (next_conf.empty()) return ThrExtStepStatus::DEAD;
    return ThrExtStepStatus::NONEMPTY;
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
        if (T.is_final[tr.to]) {
            if (thrext_discharge_ok(info, guess, prog2)) {
                return ThrExtSpawnStatus::EMPTY;
            }
            continue;
        }

        if (!thrext_is_live(T, info, guess, tr.to, prog2)) continue;
        conf.push_back(ThrExtConf{tr.to, prog2});
    }

    thrext_frontier_canonicalize(conf, info, guess);
    if (conf.empty()) return ThrExtSpawnStatus::REJECT;
    return ThrExtSpawnStatus::NONEMPTY;
}

// ===== From src/NestedAutomaton.cpp: cached Min/Max and SumPlus/SumMinus Inf/LimInf backends =====

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
                        if (T.is_final[tr.to]) {
                            if (y2 == guess) {
                                spawn_code_ref(child, guess, sym) = OBL_DISCHARGED;
                                goto spawn_done;
                            }
                            continue;
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
                    if (T.is_final[tr.to]) {
                        if (y2 == O.guess) {
                            discharged = true;
                            return;
                        }
                        continue;
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

Automaton* NestedAutomaton::flatten_SumPlusMinus_Inf_cached(value_function_t finite_aggregator,
                                                            weight_t threshold) {
    if (finite_aggregator != SumPlus && finite_aggregator != SumMinus) {
        QUAK_FAIL("flatten_SumPlusMinus_Inf_cached requires SumPlus or SumMinus");
    }
    return flatten_SumPlusMinus_Inf_cached_impl(this, finite_aggregator, threshold);
}


// ===== From src/NestedAutomaton_OLD_V2.cpp: small helpers used by split-witness backend =====

static inline bool any_one(const std::vector<unsigned char>& v) {
    for (unsigned char b : v) {
        if (b != 0) return true;
    }
    return false;
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
