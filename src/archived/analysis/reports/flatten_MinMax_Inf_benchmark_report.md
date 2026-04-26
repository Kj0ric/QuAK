# flatten_MinMax_Inf: Implementation Report

## Problem Statement

For nested weighted automata with **(Inf/LimInf, Max_f/Min_f)** value functions, we need to check:

> Is there an infinite run where the infimum (or lim-infimum) of all aggregated child values meets the threshold?

The parent automaton spawns child automata on each transition. Each child runs until termination, and its value is aggregated via Max_f or Min_f. The outer value function (Inf or LimInf) combines these aggregated values over time.

## Key Mathematical Insight

The critical observation that enables an efficient implementation:

**For `Inf >= threshold`, ALL children must succeed.**

If ANY child fails (returns a value below threshold), the infimum becomes that low value, failing the check. This means:

1. We don't need to "guess" child outcomes at spawn time
2. No 2^n branching explosion from exploring both success/failure paths
3. We can simply track progress toward threshold per child

**For `LimInf >= threshold`, EVENTUALLY all children must succeed.**

Early failures are tolerable - what matters is that from some point onward, all children succeed. This requires:

1. Epoch-based tracking (which children are "current" vs "past")
2. Final states only on epoch boundaries where all tracked children succeeded
3. Failures emit weight 0 (not immediate rejection), letting LimInf handle "eventually" semantics

## The Implementation

### State Encoding

Each state in the flattened automaton tracks:

- **Parent state**: Current state in the parent automaton
- **Per-child status**: For each active/tracked child instance:
  - **Max_f**: 1 bit - has it seen an edge >= threshold? (Success requires `seen_high = true` at final)
  - **Min_f**: 1 bit - has it seen an edge < threshold? (Any low edge means failure)
- **Tracking bit**: Is this child "tracked" (from current epoch) or "active" (newly spawned)?
- **Phase**: Epoch tracking for Büchi acceptance (0, 1, or 2=final pulse)

### Edge Weights

The flattened automaton uses binary weights (0 or 1):

- **Weight 1**: All tracked children succeeded in this step
- **Weight 0**: At least one tracked child failed

This encoding lets `compute_top_with_final(Inf)` or `compute_top_with_final(LimInf)` determine the answer:

- **Inf >= 1**: TRUE iff ALL edges have weight 1
- **LimInf >= 1**: TRUE iff EVENTUALLY all edges have weight 1

### Epoch Boundaries

When all tracked children complete (reach final states):

1. Promote all active children to tracked (start new epoch)
2. Emit final pulse (phase = 2) if no failures occurred in this epoch
3. Reset phase to 0 for next epoch

## Benchmark Results

### Performance Comparison

| Version | Total Time (42 tests) | Speedup | Correct |
|---------|----------------------|---------|---------|
| **Default (simple)** | 7.0 ms | **12.6x** | Yes |
| V1 (buggy) | 15.5 ms | 5.7x | No (5 errors) |
| V2 (complex) | 88.0 ms | 1.0x | Yes |
| Regular | 42.2 ms | 2.1x | Yes |

### State Space Comparison

Example: `gen_3p_7c_1ch` automaton (3 parent states, 7 child states, 1 child type)

| Version | States (threshold=8) |
|---------|---------------------|
| **Default (simple)** | 16 |
| V2 (complex) | 2,812 |
| Regular | 453 |

**The simple implementation produces 175x fewer states than V2.**

## Archived Versions

Two previous implementations are preserved for reference:

### V1 (`flatten_MinMax_Inf_v1`) - Buggy

The original implementation with several correctness issues:

- Epoch boundary detection checked wrong arrays (TO instead of FROM)
- No active-to-tracked promotion at epoch boundaries
- No doomed/at-risk child detection

**Error rate: 5/42 tests (12%)**

### V2 (`flatten_MinMax_Inf_v2`) - Correct but Complex

A fixed version using full 4-status encoding per child:

| Status | Meaning |
|--------|---------|
| 0_0 | Guessed low, currently low |
| 0_1 | Guessed low, currently high (doomed for Max_f) |
| 1_0 | Guessed high, currently low (doomed for Min_f) |
| 1_1 | Guessed high, currently high |

This creates 2^n branching at each spawn (explore both guesses), leading to state explosion. The implementation includes:

- Doomed check (children guaranteed to fail)
- At-risk check (children stuck in dead-end states)
- `can_succeed` precomputation via backward fixpoint

**Correct but 12.6x slower due to unnecessary state exploration.**

## Why the Simple Version Works

The complex version explores two branches when spawning a child:

1. **Objective 0**: "I bet this child will NOT meet threshold" (edge weight 0)
2. **Objective 1**: "I bet this child WILL meet threshold" (edge weight 1)

For `Inf >= threshold`, if any edge weight is 0, the infimum fails the check immediately. So the objective=0 branch can NEVER lead to acceptance - it's dead weight in the state space.

For `LimInf >= threshold`, early failures are tolerable, but the simple version handles this by:

1. Not going to a sink on failure (unlike a naive Inf-only implementation)
2. Emitting weight 0 on failure, weight 1 on success
3. Letting `compute_top_with_final(LimInf)` evaluate "eventually all weights are 1"

The key fix for LimInf support: instead of `if (failure) goto sink`, we use `weight = failure ? 0 : 1` and continue normally. This allows LimInf's "eventually" semantics to work correctly.

## Conclusion

The current `flatten_MinMax_Inf` implementation:

1. **Is correct** - matches `flatten_regular` on all test cases for both Inf and LimInf
2. **Is fast** - 12.6x faster than the complex V2 version
3. **Is efficient** - produces exponentially fewer states by eliminating unnecessary branching

For **(Inf, Max_f/Min_f)** and **(LimInf, Max_f/Min_f)** emptiness checking, this is the recommended approach.

---

## Running Benchmarks and Individual Tests

### Prerequisites

Build the project first:

```bash
cmake . -DCMAKE_BUILD_TYPE=Release
make -j4
```

### Running the Full Benchmark

Create a file `benchmark.cpp`:

```cpp
#include <iostream>
#include <chrono>
#include "src/NestedAutomaton.h"
#include "src/tests/sanity_tests/test_common.h"

int main() {
    std::string file = "src/tests/correctness_tests/inputs/scc_chain_binary.txt";
    NestedAutomaton* nwa = new NestedAutomaton(file);
    weight_t threshold(5);

    // Test all four implementations
    auto test = [&](const char* name, auto flatten_fn, bool use_threshold_in_check) {
        auto t0 = std::chrono::high_resolution_clock::now();
        Automaton* flat = flatten_fn(nwa, Max_f, threshold);
        weight_t top = flat->compute_top_with_final(Inf);
        bool result = use_threshold_in_check ? (top >= threshold) : (top >= weight_t(1));
        double ms = std::chrono::duration<double,std::milli>(
            std::chrono::high_resolution_clock::now() - t0).count();
        std::cout << name << ": " << (result ? "TRUE" : "FALSE")
                  << " (" << flat->getStates()->size() << " states, "
                  << ms << " ms)\n";
        delete flat;
    };

    test("Default", NestedAutomatonTester::flatten_MinMax_Inf, false);
    test("V1 (buggy)", NestedAutomatonTester::flatten_MinMax_Inf_v1, false);
    test("V2 (complex)", NestedAutomatonTester::flatten_MinMax_Inf_v2, false);
    test("Regular", NestedAutomatonTester::flatten_regular, true);

    delete nwa;
    return 0;
}
```

Compile and run:

```bash
g++ -std=c++17 -O2 -I. benchmark.cpp \
    src/Automaton.cpp src/NestedAutomaton.cpp src/ChildAutomaton.cpp \
    src/Parser.cpp src/Edge.cpp src/Monitor.cpp src/State.cpp \
    src/Symbol.cpp src/Weight.cpp src/Word.cpp src/FORKLIFT/*.cpp \
    -o benchmark
./benchmark
```

### Running Individual Algorithms

#### Using the Public API (Recommended)

```cpp
#include "src/NestedAutomaton.h"

int main() {
    // Load nested automaton from file
    NestedAutomaton* nwa = new NestedAutomaton("path/to/automaton.txt");

    // Check emptiness with (Inf, Max_f) and threshold 5
    bool result = nwa->isNonEmpty(Inf, Max_f, weight_t(5));
    std::cout << "isNonEmpty(Inf, Max_f, 5): " << (result ? "TRUE" : "FALSE") << "\n";

    // Check with LimInf instead
    bool result2 = nwa->isNonEmpty(LimInf, Max_f, weight_t(5));
    std::cout << "isNonEmpty(LimInf, Max_f, 5): " << (result2 ? "TRUE" : "FALSE") << "\n";

    delete nwa;
    return 0;
}
```

#### Using Individual Flatten Functions (For Comparison)

To access the internal flatten functions directly, use the `NestedAutomatonTester` helper class:

```cpp
#include "src/NestedAutomaton.h"
#include "src/tests/sanity_tests/test_common.h"

int main() {
    NestedAutomaton* nwa = new NestedAutomaton("path/to/automaton.txt");
    weight_t threshold(5);

    // Default (simple) - recommended
    Automaton* flat1 = NestedAutomatonTester::flatten_MinMax_Inf(nwa, Max_f, threshold);
    bool result1 = flat1->compute_top_with_final(Inf) >= weight_t(1);
    std::cout << "Default: " << (result1 ? "TRUE" : "FALSE")
              << " (" << flat1->getStates()->size() << " states)\n";
    delete flat1;

    // V1 (buggy) - for historical comparison only
    Automaton* flat2 = NestedAutomatonTester::flatten_MinMax_Inf_v1(nwa, Max_f, threshold);
    bool result2 = flat2->compute_top_with_final(Inf) >= weight_t(1);
    std::cout << "V1: " << (result2 ? "TRUE" : "FALSE")
              << " (" << flat2->getStates()->size() << " states)\n";
    delete flat2;

    // V2 (complex) - correct but slow
    Automaton* flat3 = NestedAutomatonTester::flatten_MinMax_Inf_v2(nwa, Max_f, threshold);
    bool result3 = flat3->compute_top_with_final(Inf) >= weight_t(1);
    std::cout << "V2: " << (result3 ? "TRUE" : "FALSE")
              << " (" << flat3->getStates()->size() << " states)\n";
    delete flat3;

    // Regular (general-purpose flattening)
    // Note: threshold comparison is done AFTER compute_top_with_final
    Automaton* flat4 = NestedAutomatonTester::flatten_regular(nwa, Max_f);
    bool result4 = flat4->compute_top_with_final(Inf) >= threshold;
    std::cout << "Regular: " << (result4 ? "TRUE" : "FALSE")
              << " (" << flat4->getStates()->size() << " states)\n";
    delete flat4;

    delete nwa;
    return 0;
}
```

### API Reference

| Function | Description | Result Check |
|----------|-------------|--------------|
| `flatten_MinMax_Inf(nwa, agg, thr)` | Default (simple, fast) | `top >= 1` |
| `flatten_MinMax_Inf_v1(nwa, agg, thr)` | Archived buggy version | `top >= 1` |
| `flatten_MinMax_Inf_v2(nwa, agg, thr)` | Archived complex version | `top >= 1` |
| `flatten_regular(nwa, agg)` | General-purpose | `top >= threshold` |

**Parameters:**
- `nwa`: Pointer to `NestedAutomaton`
- `agg`: Finite aggregator (`Max_f` or `Min_f`)
- `thr`: Threshold value (`weight_t`)

**Note:** The threshold-specialized functions (Default, V1, V2) bake the threshold into the construction and use binary weights (0/1). Check `top >= 1`. The `flatten_regular` function preserves original weights; check `top >= threshold`.

### Test Input Files

Pre-existing test automata are in `src/tests/correctness_tests/inputs/`:

```
baseline_det.txt              - Simple deterministic case
baseline_fractional.txt       - Fractional weights
nondet_child_binary.txt       - Nondeterministic child
two_children_binary.txt       - Multiple child types
scc_chain_binary.txt          - SCC structure in child
deep_nondet_binary.txt        - Deep nondeterminism
three_children_varied.txt     - Three different children
epsilon_boundary.txt          - Boundary threshold cases
adversarial_racing.txt        - Racing children
adversarial_exact_threshold.txt - Exact threshold edge case
```

### Running the Test Suite

```bash
# Run all correctness tests (300 test cases)
make test

# Run just the emptiness correctness tests
./test_emptiness_correctness
```

---

## Technical Details

### Source Files

- `src/NestedAutomaton.cpp`: Contains all implementations
  - `flatten_MinMax_Inf()` - Default (simple, correct, fast)
  - `flatten_MinMax_Inf_v1()` - Archived buggy version
  - `flatten_MinMax_Inf_v2()` - Archived complex version

### Test Coverage

- 10 handcrafted test automata covering edge cases
- 4 generated automata with varying parameters
- 3 thresholds per automaton (1, 5, 8)
- Total: 42 test cases
- All tests pass with Default matching Regular exactly

### Value Function Semantics

For clarity on how the outer value functions work:

- **Inf**: `inf{w_0, w_1, w_2, ...}` - minimum over ALL edge weights
- **LimInf**: `liminf{w_0, w_1, ...}` - eventual minimum (largest value exceeded infinitely often)

The simple implementation correctly handles both by using binary weights and letting the existing `compute_top_with_final()` machinery evaluate the result.
