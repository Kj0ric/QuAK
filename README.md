# QuAK: Quantitative Automata Kit (Nested Quantitative Automata Extension)

QuAK is an open source C++ library for analyzing quantitative automata. This version extends the original QuAK with support for **nested quantitative automata (NQA)** -- hierarchical automata where a parent automaton invokes finite-word child automata during its infinite run.

## Overview

A nested quantitative automaton consists of:
- A **parent automaton** that reads an infinite word and invokes child automata along the way.
- One or more **child automata** that process finite sub-words and produce a return value.
- Two aggregation functions:
  - **Finite aggregator (finVal)**: aggregates the weights within each child run into a single return value.
  - **Infinite aggregator (infVal)**: aggregates the sequence of child return values over the infinite parent run.

Given a nested automaton and a threshold, QuAK can answer:
1. **Non-emptiness**: Does there exist an infinite word whose value is >= the threshold?
2. **Universality**: Do all infinite words have value >= the threshold?

---

## Building

### Requirements

- C++17 compatible compiler (g++ recommended)
- CMake (>= 3.9)
- Make

No external dependencies.

### Quick Start

```bash
cmake . -DCMAKE_BUILD_TYPE=Release
make -j4
```

This produces:
- `quak-nested` -- the main CLI executable

### Build Targets

| Command | What it builds |
|---------|----------------|
| `make` | Library + `quak-nested` CLI |
| `make tests` | All test executables |
| `make examples` | Example programs |
| `make experiments` | Experiment runners |
| `ctest` | Run all tests (must `make tests` first) |

After building, run from the project root:

```bash
# Main CLI
./quak-nested [OPTIONS] automaton-file [ACTION ...]

# Tests
make tests        # build test executables
ctest             # run all tests
./test_sanity_all # or run individual test executables directly

# Examples
make examples
./example1_basic
./example2_value_functions
./example3_response_time

# Experiments
make experiments
./quak-experiment-single [args]   # single automaton experiment runner
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `-DCMAKE_BUILD_TYPE=Release` | Release | Build type (Release / Debug) |
| `-DENABLE_SCC_SEARCH_OPT=ON` | ON | SCC-based optimization in FORKLIFT |
| `-DENABLE_IPO=ON` | ON | Link-time (inter-procedural) optimizations |

---

## Value Functions

### Infinite Aggregators (infVal)

Applied over the infinite sequence of child return values:

| Name | Description |
|------|-------------|
| `Inf` | Infimum (minimum over the entire run) |
| `Sup` | Supremum (maximum over the entire run) |
| `LimInf` | Limit inferior |
| `LimSup` | Limit superior |
| `LimInfAvg` | Limit inferior of the running average |
| `LimSupAvg` | Limit superior of the running average |

### Finite Aggregators (finVal)

Applied to the weights within a single child run to produce a return value:

| Name | Description |
|------|-------------|
| `Max_f` | Maximum weight seen during the child run |
| `Min_f` | Minimum weight seen during the child run |
| `SumB` | Bounded sum of weights (requires a `bound` parameter) |
| `SumPlus` | Sum of absolute values of all weights (always ≥ 0) |
| `SumMinus` | Negated sum of absolute values of all weights (always ≤ 0) |

For nested automata, `SumPlus` and `SumMinus` use those semantics even when child automata contain a mix of positive and negative transition weights.

### Supported Combinations

Not every (finVal, infVal) pair is supported for every decision problem:

**Non-emptiness:**

| finVal | Supported infVal |
|--------|-----------------|
| Max_f, Min_f, SumB, SumMinus | Inf, Sup, LimInf, LimSup, LimInfAvg, LimSupAvg |
| SumPlus | Inf, Sup, LimInf, LimSup, LimSupAvg |

**Universality:**

| finVal | Supported infVal |
|--------|-----------------|
| Max_f, Min_f, SumB, SumPlus, SumMinus | Inf, Sup, LimInf, LimSup |

---

## Input File Format

### Transition Syntax

Each transition is written as:
```
symbol : weight, source_state -> target_state
```
Comments start with `#`.

### Nested Automaton File Structure

A nested automaton file contains a `@PARENT` section followed by `@CHILD` sections:

```
@PARENT
a : 1, p0 -> p0
b : 0, p0 -> p1
a : 1, p1 -> p0
b : 0, p1 -> p1

@CHILD 0

@CHILD 1
final: done
a : 1, count -> count
b : 0, count -> done
```

**Sections:**
- `@PARENT` -- The parent automaton. Weights on parent transitions encode which child automaton is invoked (0 = no child / dummy).
- `@CHILD 0` -- Dummy child (always present, always empty). A placeholder for parent transitions that do not invoke any child.
- `@CHILD n` (n >= 1) -- Actual child automata.

**Rules:**

1. **Initial state**: The source state of the first transition in each section.
2. **Child final states**: Every non-dummy child must declare at least one final state with `final: state1 state2 ...`. A child run is accepted when it reaches a final state.
3. **Parent final states**: All parent states are implicitly final unless explicit final states are declared with `final:` in the `@PARENT` section.
4. **Child index 0**: Reserved for the dummy child. Must always be present (can be empty).
5. **Completeness**: All automata (parent and children) should be complete -- for every state and symbol, at least one outgoing transition must exist.
6. **Alphabet**: Parent and all non-dummy children share the same alphabet.

### Silent Transitions

The parent automaton may contain silent transitions by using `SILENT` as the weight value. Silent transitions represent steps where no child is invoked and no value is emitted. Internally, `SILENT` is stored as `std::numeric_limits<float>::max()`. Children should **not** contain silent transitions.

---

## Using the CLI

### Usage

```bash
./quak-nested [OPTIONS] automaton-file [ACTION ...]
```

Nested automata files are auto-detected by the presence of the `@PARENT` marker.

### Options

| Option | Description |
|--------|-------------|
| `-cputime` | Print execution time |
| `-v` | Print input size |
| `-d` | Print automaton structure |
| `-debug` | Verbose debug output |

### Actions for Nested Automata

```
VALF   = <Inf | Sup | LimInf | LimSup | LimInfAvg | LimSupAvg>
FINVAL = <Max_f | Min_f | SumB | SumPlus | SumMinus>

non-empty VALF FINVAL <threshold> [bound]
universal VALF FINVAL <threshold> [bound]
```

The `bound` parameter is **required** for `SumB` and optional otherwise.

### Examples

```bash
# Non-emptiness with LimSup + Max_f, threshold 5
./quak-nested nested.txt non-empty LimSup Max_f 5

# Universality with Inf + Min_f, threshold 0
./quak-nested nested.txt universal Inf Min_f 0

# Non-emptiness with SumB (bound required)
./quak-nested nested.txt non-empty LimInf SumB 3 10

# Non-emptiness with LimSupAvg + SumPlus, threshold 2
./quak-nested nested.txt non-empty LimSupAvg SumPlus 2

# With timing
./quak-nested -cputime nested.txt universal LimSup Max_f 1

# Print the automaton structure first, then decide
./quak-nested -d nested.txt non-empty Sup Max_f 3
```

### Output Format

```
----------
isNonEmpty(LimSup, Max_f, threshold=5) = 1
----------
```

Result: `1` = true, `0` = false.

---

## Using the Library API

### Include Headers

```cpp
#include "NestedAutomaton.h"
```

### Loading and Querying

```cpp
// Load a nested automaton from file
NestedAutomaton* NA = new NestedAutomaton("nested.txt");

// Non-emptiness: exists a word with value >= threshold?
bool exists = NA->isNonEmpty(LimSup, Max_f, 5.0);

// With SumB (bound required)
bool existsSumB = NA->isNonEmpty(LimInf, SumB, 3.0, 10);

// Universality: all words have value >= threshold?
bool universal = NA->isUniversal(Inf, Min_f, 0.0);

// Inspect structure
std::size_t nChildren = NA->getChildrenSize();
ChildAutomaton* child = NA->getChild(1);

// Print
NA->print();

delete NA;
```

### API Reference

```cpp
class NestedAutomaton : public Automaton {
public:
    NestedAutomaton(std::string filename);

    // Decision procedures
    bool isNonEmpty(value_function_t infVal, value_function_t finVal,
                    weight_t threshold, weight_t bound = -1);
    bool isUniversal(value_function_t infVal, value_function_t finVal,
                     weight_t threshold, weight_t bound = -1);

    // Flattening: convert nested automaton to equivalent non-nested automaton
    Automaton* flatten_regular(value_function_t finVal, weight_t bound = -1);
    Automaton* flatten_SumPlusMinus_Sup(value_function_t finVal, weight_t threshold);
    Automaton* flatten_SumPlusMinus_Inf(value_function_t finVal, weight_t threshold);
    Automaton* flatten_MinMax_Sup(value_function_t finVal, weight_t threshold);
    Automaton* flatten_MinMax_Inf(value_function_t finVal, weight_t threshold);
    Automaton* flatten_Avg_SumMinus(uint64_t c_bound);

    // Structure inspection
    std::size_t getChildrenSize() const;
    ChildAutomaton* getChild(std::size_t index) const;
    bool isDeterministicNested() const;
    bool isCompleteNested() const;

    void print() const;
};
```

### Flattening

The flattening methods produce a non-nested `Automaton*` that can be used with all standard `Automaton` operations (emptiness, universality, inclusion, etc.). The caller is responsible for deleting the returned automaton.

```cpp
NestedAutomaton* NA = new NestedAutomaton("nested.txt");

// Flatten with Max_f aggregator
Automaton* flat = NA->flatten_regular(Max_f);

// Use standard operations on the flattened automaton
bool result = flat->isNonEmpty(LimSup, 5.0);

delete flat;
delete NA;
```

Which flattening method to use depends on the aggregator combination (see the Internal Algorithm section below).

---

## Internal Algorithm

The decision procedures work by **flattening** the nested automaton into an equivalent non-nested automaton, then applying standard decision procedures on the result. The flattening methods are also available as public API (see above).

The flattening approach depends on the aggregator combination:

| finVal | infVal | Flattening method |
|--------|--------|-------------------|
| SumPlus/SumMinus | Sup, LimSup, Inf, LimInf | Monotonic 0/1 encoding |
| SumPlus | LimSupAvg | Fast path (Sup-based), then SumB fallback |
| SumMinus | LimInfAvg, LimSupAvg | Pseudo-determinization + synchronization |
| Max_f/Min_f | Sup, LimSup, Inf, LimInf | Monotonic min/max construction |
| Max_f/Min_f | LimInfAvg, LimSupAvg | Regular flattening |
| SumB | All | Regular flattening with bound |

After flattening, silent transitions are removed (when necessary), and the standard `isNonEmpty` or `isUniversal` of the base `Automaton` class is invoked on the resulting non-nested automaton.

For **universality**, the implementation converts SumPlus and SumMinus to SumB internally and always uses the regular flattening path.

---

## Assumptions and Requirements

### Structural

- **Single initial state** per automaton (parent and each child). Determined by the source state of the first transition.
- **Completeness**: All automata must be complete (total). For every state `q` and symbol `a`, there must be at least one transition from `q` labeled `a`. Incomplete automata are completed by adding a sink state.
- **Immutability**: Automata are immutable after construction.
- **State ownership**: Each state object is owned by exactly one automaton instance.

### Internal Details: SCC and Reachability

- SCCs are computed via Tarjan's algorithm during construction and are never recomputed.
- State tags: `tag >= 0` = reachable (value is SCC ID); `tag == -1` = unreachable.
- Lower SCC IDs are reachable from higher SCC IDs.
- Unreachable states are trimmed during construction.

### Non-Determinism

Non-determinism is resolved by the **Supremum** function: among all possible runs, the one producing the highest value is chosen.

### Nested-Specific

- **Child index 0** is always the dummy child (no alphabet, one dummy state, no transitions, no final states). It serves as a placeholder for parent transitions that don't invoke a child.
- **Child final states** are mandatory for non-dummy children. The parser aborts if a `@CHILD n` (n >= 1) section has no `final:` declaration.
- **Parent final states**: If no `final:` declaration is given, all parent states are implicitly final. Explicit final states can be specified with `final: state1 state2 ...` in the `@PARENT` section.
- **Alphabet synchronization**: parent and all non-dummy children must use the same alphabet.
- **Silent transitions** (`SILENT` keyword, stored as max float) are allowed only in the parent. Using them in children leads to undefined behavior.

### Weight Precision

- Weights are `float` values.
- Weight comparisons use epsilon tolerance: `WEIGHT_EQ_EPSILON = 1e-5`.
- Hexadecimal float representation is also supported for exact bit-level specification.

### Acceptance

- **Parent**: A parent run is accepting if it visits a final state infinitely often **and** invokes a non-silent child infinitely often. By default all parent states are final (unless explicit final states are declared), so acceptance reduces to the non-silent child condition.
- **Children**: Accept finite words. A child run terminates and produces a value when it reaches a final state.
- **Flattened automata**: Use Büchi acceptance. The flattening encodes both conditions (parent final states and infinitely many non-silent invocations) into the accepting-state set of the flattened automaton.

---

## Example Programs

Three example programs are provided in `examples/nested/`:

```bash
make examples
./example1_basic             # Comparing finVals (Max_f, Min_f, SumPlus, SumB)
./example2_value_functions   # Varying finVal + non-emptiness vs universality
./example3_response_time     # Comparing infVals (Sup, LimSup, LimSupAvg, LimInf, Inf)
```

See `examples/nested/README.md` for details.

---

## Experiments

### Single Runner (`quak-experiment-single`)

Build the experiment runner:

```bash
make experiments -j4
```

Usage:

```bash
./quak-experiment-single <file> <problem> <InfVal> <FinVal> <threshold> \
    [--rep R] [--timeout-s T] [--warmup 0|1]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `file` | (required) | Path to a nested automaton `.txt` file |
| `problem` | (required) | `emptiness` or `universality` |
| `InfVal` | (required) | Infinite aggregator (e.g. `Sup`, `LimSupAvg`) |
| `FinVal` | (required) | Finite aggregator (e.g. `SumPlus`, `Max`, `SumB:auto`) |
| `threshold` | (required) | Numeric threshold |
| `--rep R` | 3 | Number of timed repetitions |
| `--timeout-s T` | 300 | Per-repetition timeout in seconds |
| `--warmup 0\|1` | 1 | Run one untimed warmup repetition first |

**SumB variants:** `SumB:auto` (infer bound from filename `_kY.txt`), `SumB:<B>`, `SumB(<B>)`.

**Output format:**

```
MEAN_S=<double> RESULT=<0|1> STATUS=<OK|TIMEOUT|ERR|INCONSISTENT>
```

Example:

```bash
./quak-experiment-single samples/generated_response_time_1/response_n2_k2.txt \
    emptiness Sup SumPlus 2 --rep 1 --timeout-s 30 --warmup 1
```

### Python Orchestrator (`experiment.py`)

Runs all configured experiments in batch, with resume support (skips already-completed `(n, k)` pairs).

```bash
python3 experiment.py --exe ./quak-experiment-single \
    [--rep R] [--timeout T] [--warmup W] [--memory-limit 30G] [--outdir results]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `--exe` | (required) | Path to `quak-experiment-single` binary |
| `--rep` | 3 | Repetitions per instance |
| `--timeout` | 300 | Per-repetition timeout (seconds) |
| `--warmup` | 1 | Warmup (0 or 1) |
| `--memory-limit` | none | Memory limit per process (e.g. `30G`, `8192M`) |
| `--outdir` | `results` | Output directory for CSV files |

**Configured experiments:**

| Name | Input dir | Problem | InfVal | FinVal |
|------|-----------|---------|--------|--------|
| `response_sup_sumplus_emptiness` | `generated_response_time_1` | emptiness | Sup | SumPlus |
| `response_limsupavg_sumplus_emptiness` | `generated_response_time_2` | emptiness | LimSupAvg | SumPlus |
| `response_sup_sumb_universality` | `generated_response_time_3` | universality | Sup | SumB:auto |
| `resource_sup_max_emptiness` | `generated_resource_consumption_1` | emptiness | Sup | Max |
| `resource_limsupavg_max_emptiness` | `generated_resource_consumption_2` | emptiness | LimSupAvg | Max |

Each experiment produces one CSV file in the output directory with columns: `experiment`, `n`, `k`, `file`, `problem`, `infval`, `finval`, `threshold`, `rep`, `timeout_s`, `warmup`, `status`, `mean_s`, `result01`.

---

## Project Structure

```
QuAK/
├── src/
│   ├── Automaton.cpp/h         # Base automaton class
│   ├── NestedAutomaton.cpp/h   # Nested automaton (parent + children)
│   ├── ChildAutomaton.cpp/h    # Child automaton (finite-word)
│   ├── Parser.cpp/h            # Input file parser
│   ├── Monitor.cpp/h           # Runtime monitoring
│   ├── utils.cpp/h             # Value function string conversion
│   ├── FORKLIFT/               # Language inclusion algorithm
│   ├── quak-nested-main.cpp    # Main CLI
│   ├── quak-experiment-single.cpp  # Experiment runner
│   └── tests/
│       ├── sanity_tests/       # Flattening, synchronization, etc.
│       └── correctness_tests/  # Emptiness/universality correctness
├── examples/
│   └── nested/                 # Example programs + sample automata
├── samples/                    # Sample automata files
├── experiment.py               # Python experiment orchestrator
├── CMakeLists.txt
└── README.md
```

---

## Differences from the Original QuAK (Non-Nested)

This version of QuAK extends the original with nested automata support. For non-nested automata, the CLI still supports all original operations:

```
stats, dump, empty, non-empty, universal, constant, safe, live,
top-value, bottom-value, isIncluded, isIncludedBool, isEquivalent,
isEquivalentBool, livenessComponent, safetyComponent, decompose,
eval, monitor, witness-file
```

These work exactly as in the original QuAK (see the original QuAK documentation for details). The one addition for non-nested automata is:

**Final state declarations**: Non-nested automata can now optionally specify final states using the `final: state1 state2 ...` syntax in their input files. If no `final:` line is present, all states are final (matching the original QuAK behavior, where F = Q).
