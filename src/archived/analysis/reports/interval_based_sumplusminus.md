# Interval-Based SumPlus/SumMinus Implementation

This document describes the SumPlus/SumMinus implementation in `NestedAutomaton.cpp`, which uses interval-based budget tracking with integer scaling for exact arithmetic.

---

## 1. Overview

The implementation solves threshold emptiness for nested automata with SumPlus/SumMinus finite aggregation:

> **isNonEmpty(infVal, SumPlus/SumMinus, threshold)**: Is there an infinite word where `infVal(child_sums) >= threshold`?

The approach:
1. **Scale** fractional weights to integers for exact arithmetic
2. **Track budget intervals** representing possible accumulated values
3. **Flatten** to a 0/1 automaton encoding threshold achievement
4. **Apply** standard emptiness check on the flattened automaton

---

## 2. Weight Scaling

### 2.1 The Problem with Floating Point

Floating point arithmetic introduces precision errors:
```
-1.5 + -2.7 + -0.8 = -4.99999999999... (not exactly -5.0)
```

This causes incorrect boundary behavior when checking `sum >= threshold`.

### 2.2 The Solution: Integer Scaling

Convert all weights to integers by finding a common scale factor:

```cpp
typedef uint64_t internal_weight_t;  // Exact integer arithmetic

static internal_weight_t compute_weight_scale(NestedAutomaton* nwa) {
    internal_weight_t scale = 1;
    // Find scale that makes all weights integral (up to 6 decimal places)
    for (each weight w in children) {
        while (w * scale is not close to integer) {
            scale *= 10;
        }
    }
    return scale;
}
```

Example with weights -1.5, -2.7, -0.8:
- Scale = 10
- Scaled weights: 15, 27, 8
- Scaled sum: 15 + 27 + 8 = 50 (exactly)

### 2.3 Converting Weights to Internal Representation

**For edge weights** (round to nearest):
```cpp
inline internal_weight_t to_internal(weight_t w, internal_weight_t scale) {
    float scaled = w.to_float() * scale;
    return static_cast<internal_weight_t>(scaled + 0.5f);
}
```

**For thresholds** (truncate toward zero):
```cpp
inline internal_weight_t to_internal_trunc(weight_t w, internal_weight_t scale) {
    float scaled = w.to_float() * scale;
    return static_cast<internal_weight_t>(scaled);  // truncates
}
```

### 2.4 Why Truncation for Thresholds?

For SumMinus with non-integer threshold, truncation ensures correct boundary behavior.

**Example**: threshold = -0.5, best child sum = -1, scale = 1
- We want: is -1 >= -0.5? **NO** (should return FALSE)
- With truncation: `abs_threshold = trunc(0.5) = 0`
- Budget starts at 0, edge costs 1, budget becomes -1 < 0 → **FALSE** ✓

If we rounded instead: `abs_threshold = round(0.5) = 1`
- Budget starts at 1, edge costs 1, budget becomes 0 >= 0 → **TRUE** ✗

### 2.5 Working with Absolute Values

All internal computations use absolute values:

```cpp
internal_weight_t abs_edge_value = (edge->getWeight()->getValue() < 0)
    ? to_internal(-(edge->getWeight()->getValue()), scale)
    : to_internal(edge->getWeight()->getValue(), scale);
```

For SumMinus with threshold T < 0:
- `abs_threshold = to_internal_trunc(-T, scale)` (positive)
- Budget tracks "remaining slack before exceeding |T|"
- Edge costs are |edge_weight|
- Success when `budget - cost >= 0`

---

## 3. Nested Automaton Structure

A nested automaton has:
- A **parent automaton** with states and transitions
- **Child automata** summoned by parent transitions
- Each parent transition's weight encodes which child to summon

When the parent takes a transition:
1. The corresponding child automaton is activated
2. The child runs from initial state to a final state
3. The child's **value** is the sum of edge weights along its path

---

## 4. Flattening to 0/1 Automaton

For threshold checking, we create a **flattened automaton** with 0/1 weights:
- Weight **1**: The child achieved value >= threshold
- Weight **0**: The child did not achieve value >= threshold

Then apply the infinite-word value function (Sup/Inf/LimSup/LimInf):
- **Sup = 1** iff at least one transition has weight 1
- **Inf = 1** iff all transitions have weight 1
- **LimSup = 1** iff infinitely many transitions have weight 1
- **LimInf = 1** iff eventually all transitions have weight 1

---

## 5. Budget Tracking

### 5.1 Data Structures

```cpp
struct BudgetInterval {
    internal_weight_t lo;
    internal_weight_t hi;
};

struct BudgetSet {
    std::vector<BudgetInterval> fixed;  // Disjoint sorted intervals
    bool has_unlimited = false;         // Whether values >= threshold possible

    bool empty() const;
    bool contains_zero() const;      // For Sup termination
    bool has_non_negative() const;   // For SumMinus termination
    bool can_satisfy(internal_weight_t v) const;  // For Inf termination
};
```

### 5.2 Budget Semantics

**Budget** tracks progress toward achieving the threshold:
- Initial budget = |threshold| (scaled)
- After edge with |weight| = cost: budget = budget - cost
- At termination: success if budget >= 0

### 5.3 Budget Operations

**Subtract cost from budget set:**
```cpp
static BudgetSet budgetset_subtract_cost(const BudgetSet& bs,
                                          internal_weight_t cost,
                                          internal_weight_t abs_threshold) {
    BudgetSet result;
    for (const auto& iv : bs.fixed) {
        if (iv.hi < cost) continue;  // Interval exhausted
        internal_weight_t new_lo = (iv.lo >= cost) ? (iv.lo - cost) : 0;
        internal_weight_t new_hi = iv.hi - cost;
        result.fixed.push_back({new_lo, new_hi});
    }
    return result;
}
```

**Process unlimited budget (for Sup nondeterministic guessing):**
```cpp
static BudgetSet budgetset_from_unlimited(internal_weight_t cost,
                                           internal_weight_t abs_threshold,
                                           bool negative_threshold) {
    BudgetSet result;
    if (negative_threshold) {
        // Deterministic: unlimited means exactly abs_threshold
        if (abs_threshold >= cost) {
            internal_weight_t new_budget = abs_threshold - cost;
            result.fixed.push_back({new_budget, new_budget});
        }
    } else {
        // Nondeterministic guessing for SumPlus
        // Budgets in [threshold, threshold+cost) become [0, cost)
        // Budgets >= threshold+cost remain unlimited
        if (cost > 0 && abs_threshold > 0) {
            internal_weight_t new_lo = (abs_threshold > cost) ? (abs_threshold - cost) : 0;
            result.fixed.push_back({new_lo, abs_threshold - 1});
        }
        result.has_unlimited = true;
    }
    return result;
}
```

---

## 6. Sup vs Inf Semantics

### 6.1 Sup (Existential)

**Question**: Is there a run where Sup(child values) >= T?

**Meaning**: At least ONE epoch achieves >= T.

**Approach**: Nondeterministically guess the target value:
- Budget starts with `has_unlimited = true` (guessing >= T)
- As edges are consumed, some guesses drop below threshold
- Success if any guess reaches budget = 0

```cpp
// Sup initialization
BudgetSet budget;
budget.has_unlimited = true;

// Sup termination
success = budget.contains_zero();
```

### 6.2 Inf (Universal)

**Question**: Is there a run where Inf(child values) >= T?

**Meaning**: EVERY epoch achieves >= T.

**Approach**: Fixed requirement, no guessing:
- Budget is exactly `[abs_threshold, abs_threshold]`
- Each child must achieve >= T
- No nondeterminism in the requirement

```cpp
// Inf initialization
BudgetSet budget;
budget.fixed.push_back({abs_threshold, abs_threshold});
budget.has_unlimited = false;

// Inf termination (SumPlus)
success = budget.can_satisfy(edge_value);

// Inf termination (SumMinus)
BudgetSet after = budgetset_after_edge(budget, abs_edge_value, ...);
success = after.has_non_negative();
```

### 6.3 Summary Table

| Aspect | Sup | Inf |
|--------|-----|-----|
| Quantifier | ∃ epoch with value >= T | ∀ epochs, value >= T |
| Budget init | `has_unlimited = true` | Single point [T, T] |
| Termination | `contains_zero()` | `can_satisfy()` or `has_non_negative()` |

---

## 7. SumPlus vs SumMinus

### 7.1 SumPlus (Positive Threshold)

- Threshold T > 0
- Goal: accumulate sum >= T
- Budget = T, subtract edge values, success if budget <= 0

### 7.2 SumMinus (Negative Threshold)

- Threshold T < 0
- Goal: sum >= T (i.e., not too negative)
- `abs_threshold = |T|` (positive)
- Budget = |T| represents "slack before exceeding bound"
- Subtract |edge_weight| from budget
- Success if budget >= 0 (still have non-negative slack)

**Example**: threshold = -5, edge weights = -2, -2, -1
- abs_threshold = 5
- Budget: 5 → 5-2=3 → 3-2=1 → 1-1=0
- Final budget = 0 >= 0 → **SUCCESS** (sum = -5 >= -5) ✓

**Example**: threshold = -4, same edges
- abs_threshold = 4
- Budget: 4 → 4-2=2 → 2-2=0 → 0-1=-1
- Final budget = -1 < 0 → **FAILURE** (sum = -5 < -4) ✓

---

## 8. Failure Handling: Continue vs Sink

### 8.1 The Problem with Sink

Original approach: when child fails, go to sink state (stuck forever with weight 0).

For LimInf, this is wrong. Consider:
- Parent path: p0 → p1 → p2 → p2 → ...
- Child values: 1, 1, 7, 7, 7, ... (threshold = 7)
- Weight sequence: 0, 0, 1, 1, 1, ...
- LimInf = 1 (eventually all 1s)

With sink: first failure sends to sink, LimInf = 0. **WRONG!**

### 8.2 The Correct Approach

Continue exploration after failure, just emit weight 0:

```cpp
if (!success) {
    data->global_edge_weight = 0;  // Mark epoch as failed
}
// Always continue exploration
data->new_activation[ii] = false;
data->new_tracked_children_state[ii] = false;
data->new_value_of_children_state[ii] = BudgetSet{};
should_recurse = true;
```

This allows reaching states where children do meet the threshold.

---

## 9. Key Implementation Details

### 9.1 The budget_limit Field

For Inf, budget starts at exactly `abs_threshold`. But `normalize_budget_set` converts values >= `abs_threshold` to `has_unlimited`.

**Solution**: Use `budget_limit = abs_threshold + 1` for normalization, so the threshold value stays fixed.

### 9.2 Termination Edge Handling

For SumMinus termination, use the **full edge value**, not capped:

```cpp
if (to_final) {
    if (data->negative_threshold) {
        // Use full edge value for correct boundary behavior
        BudgetSet after = budgetset_after_edge(oldb, abs_edge_value, ...);
        success = after.has_non_negative();
    } else {
        // SumPlus: can cap at threshold
        internal_weight_t required = std::min(abs_edge_value, abs_threshold);
        success = oldb.can_satisfy(required);
    }
}
```

### 9.3 Path Merging (Inf only)

When multiple paths reach the same state, intersect their budgets:

```cpp
BudgetSet inter = budgetset_intersect(existing_budget, new_budget, budget_limit);
if (inter.empty()) {
    // No consistent budget possible
    explore_global_failure(data);
} else {
    data->new_value_of_children_state[ii] = inter;
}
```

---

## 10. Test Results

All tests pass:
- **test_emptiness_correctness**: 300/300 (10 automata × 6 infVal × 5 finVal)
- **test_emptiness_summinus**: 40/40 (10 automata × 4 infVal)
- All 15 test suites pass

Key test cases verified:
- **Fractional weights**: Correctly handled via integer scaling
- **Boundary thresholds**: Truncation ensures correct behavior at exact boundaries
- **scc_chain_binary + LimInf**: Transient failures don't prevent eventual success
- **child_pump_loop + SumMinus**: Loops with accumulating negative contributions work
