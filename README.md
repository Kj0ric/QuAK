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
1. **Non-emptiness**: Does there exist an accepted infinite word whose value is >= the threshold?
2. **Universality**: Do all accepted infinite words have value >= the threshold?

---

## Building

### Requirements

- C++17 compatible compiler (g++ recommended)
- CMake (>= 3.9)
- Make

No external dependencies.

### Quick Start

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This produces:
- `build/quak-nested` -- the main CLI executable

In-source builds are intentionally disabled. Keep generated CMake files and
compiled executables under `build/` or another out-of-tree build directory.

### Build Targets

| Command | What it builds |
|---------|----------------|
| `cmake --build build` | Library + `quak-nested` CLI |
| `cmake --build build --target tests` | All registered test executables |
| `cmake --build build --target examples` | Example programs |
| `cmake --build build --target experiments` | Experiment and probe runners |
| `ctest --test-dir build --output-on-failure` | Run all registered tests |

After building, run from the project root:

```bash
# Main CLI
./build/quak-nested [OPTIONS] automaton-file [ACTION ...]

# Tests
cmake --build build --target tests
ctest --test-dir build -N
ctest --test-dir build --output-on-failure
./build/test_universality_correctness # or run any registered test executable directly

# Examples
cmake --build build --target examples
./build/example1_basic
./build/example2_value_functions
./build/example3_response_time

# Experiments
cmake --build build --target experiments
./build/quak-experiment-single [args]   # single automaton experiment runner
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `-DCMAKE_BUILD_TYPE=Release` | Release | Build type (Release / Debug) |
| `-DENABLE_SCC_SEARCH_OPT=ON` | ON | SCC-based optimization in FORKLIFT |
| `-DENABLE_IPO=ON` | ON | Link-time (inter-procedural) optimizations |
| `-DNORMALIZE_MIXED_SIGN=OFF` | OFF | When ON: auto-normalize mixed-sign child weights for LimAvg+SumPlus/SumMinus (emits a warning). When OFF (default): reject with a hard error. |

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
| `SumPlus` | Sum of positive weights |
| `SumMinus` | Sum of negative weights (negated) |

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
final: all
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
- `@CHILD 0` -- Dummy child placeholder. It has no transitions and does not need a `final:` line in the file.
- `@CHILD n` (n >= 1) -- Actual child automata.

**Rules:**

1. **Initial state**: The source state of the first transition in each section.
2. **Final states**: Non-nested automata, the parent section, and every non-dummy child must declare final states with `final: state1 state2 ...` or `final: all`.
3. **Parent final states**: Use `final: all` in the `@PARENT` section when every parent state should be accepting.
4. **Child index 0**: Reserved for the dummy child. Must always be present (can be empty).
5. **Completeness**: All automata (parent and children) should be complete -- for every state and symbol, at least one outgoing transition must exist.
6. **Alphabet**: Parent and all non-dummy children share the same alphabet.

### Silent Transitions

The parent automaton uses weight `0` for the dummy child, meaning no child value
is emitted. It may also contain generated silent transitions by using `SILENT`
as the weight value; internally, `SILENT` is stored as
`std::numeric_limits<float>::max()`. Children should **not** contain silent
transitions.

---

## Using the CLI

### Usage

```bash
./build/quak-nested [OPTIONS] automaton-file [ACTION ...]
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

### Actions for Non-Nested Automata

```
VALF = <Inf | Sup | LimInf | LimSup | LimInfAvg | LimSupAvg>

non-empty VALF <weight>
universal VALF <weight>
```

For non-nested automata, the CLI intentionally exposes only Buchi
non-emptiness and universality checks over declared final states. Use
`final: all` to recover the original all-states-accepting QuAK behavior for
those checks. For nested automata,
`universal` quantifies over accepted words of the flattened automaton; rejected
flattened words are ignored.

### Examples

```bash
# Non-emptiness with LimSup + Max_f, threshold 5
./build/quak-nested nested.txt non-empty LimSup Max_f 5

# Universality with Inf + Min_f, threshold 0
./build/quak-nested nested.txt universal Inf Min_f 0

# Non-emptiness with SumB (bound required)
./build/quak-nested nested.txt non-empty LimInf SumB 3 10

# Non-emptiness with LimSupAvg + SumPlus, threshold 2
./build/quak-nested nested.txt non-empty LimSupAvg SumPlus 2

# With timing
./build/quak-nested -cputime nested.txt universal LimSup Max_f 1

# Print the automaton structure first, then decide
./build/quak-nested -d nested.txt non-empty Sup Max_f 3
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

// Non-emptiness: exists an accepted word with value >= threshold?
bool exists = NA->isNonEmpty(LimSup, Max_f, 5.0);

// With SumB (bound required)
bool existsSumB = NA->isNonEmpty(LimInf, SumB, 3.0, 10);

// Universality: all accepted words have value >= threshold?
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

The flattening methods produce a non-nested `Automaton*` that can be used with
standard `Automaton` operations. If the flattened automaton's final states
define the accepted domain, use final-aware operations such as
`isNonEmpty_withFinal` and `isUniversal_withFinal` for semantic queries over
accepted words. The caller is responsible for deleting the returned automaton.

```cpp
NestedAutomaton* NA = new NestedAutomaton("nested.txt");

// Flatten with Max_f aggregator
Automaton* flat = NA->flatten_regular(Max_f);

// Query accepted flattened words
bool result = flat->isNonEmpty_withFinal(LimSup, 5.0);

delete flat;
delete NA;
```

Which flattening method to use depends on the aggregator combination (see the Internal Algorithm section below).

---

## Internal Algorithm

The decision procedures work by **flattening** the nested automaton into an equivalent non-nested automaton, then applying standard decision procedures on the result. The flattening methods are also available as public API (see above).

The flattening approach depends on the aggregator combination:

| finVal | infVal | Non-emptiness strategy |
|--------|--------|------------------------|
| SumPlus/SumMinus | Inf, Sup, LimInf, LimSup | Specialized flattening for extremal parents and monotone children |
| SumPlus | LimSupAvg | Sup-based fast path, then SumB fallback |
| SumMinus | LimInfAvg, LimSupAvg | Pseudo-determinization + synchronization |
| Max_f/Min_f | Inf, Sup, LimInf, LimSup | Specialized flattening for extremal parents and monotone children |
| Max_f/Min_f | LimInfAvg, LimSupAvg | Regular flattening |
| SumB | All supported infVal modes | Regular flattening with bound |

After flattening, silent transitions are removed when necessary. Non-emptiness
uses final-aware emptiness on the resulting non-nested automaton.

For **universality**, the implementation converts SumPlus and SumMinus to SumB
internally, uses the regular flattening path, and then calls
`Automaton::isUniversal_withFinal`. This checks the threshold only over words
accepted by the flattened automaton. Rejected flattened words are ignored.

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

- **Child index 0** is always the dummy child placeholder. It has no alphabet and no transitions in the input file, and it is exempt from non-dummy child final-state requirements.
- **Final state declarations** are mandatory for non-nested automata, the parent, and non-dummy children. Use `final: all` for the original all-states-final behavior.
- **Child final states** are mandatory for non-dummy children. The parser aborts if a `@CHILD n` (n >= 1) section has no `final:` declaration.
- **Parent final states** are mandatory. Use `final: state1 state2 ...` for an explicit subset or `final: all` for all parent states.
- **Alphabet synchronization**: parent and all non-dummy children must use the same alphabet.
- **Silent/no-child parent steps**: Parent weight `0` invokes the dummy child and emits no child value. The `SILENT` keyword (stored as max float) is also allowed only in the parent for generated silent steps. Children should not use `SILENT`.

### Weight Precision

- Weights are `float` values.
- Weight comparisons use epsilon tolerance: `WEIGHT_EQ_EPSILON = 1e-5`.
- Hexadecimal float representation is also supported for exact bit-level specification.

### Acceptance

- **Parent**: A parent run is accepting if it visits a final state infinitely often **and** invokes a non-silent child infinitely often. Use `final: all` when acceptance should reduce to the non-silent child condition.
- **Children**: Accept finite words. A child run terminates and produces a value when it reaches a final state.
- **Flattened automata**: Use Buchi acceptance. The flattening encodes both conditions (parent final states and infinitely many non-silent invocations) into the accepting-state set of the flattened automaton.

---

## Example Programs

Three example programs are provided in `examples/`:

```bash
cmake --build build --target examples
./build/example1_basic             # Comparing finVals (Max_f, Min_f, SumPlus, SumB)
./build/example2_value_functions   # Varying finVal + non-emptiness vs universality
./build/example3_response_time     # Comparing infVals (Sup, LimSup, LimSupAvg, LimInf, Inf)
```

See `examples/README.md` for details.

---

## Experiments

### Single Runner (`quak-experiment-single`)

Build the experiment runner:

```bash
cmake --build build --target experiments -j
```

Usage:

```bash
./build/quak-experiment-single <file> <problem> <InfVal> <FinVal> <threshold> \
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
./build/quak-experiment-single samples/generated_response_time_1/response_n4_k4.txt \
    emptiness Sup SumPlus 4 --rep 1 --timeout-s 30 --warmup 1
```

### Python Orchestrator (`experiment.py`)

Runs all configured experiments in batch, with resume support (skips already-completed `(n, k)` pairs).

```bash
python3 experiment.py --exe ./build/quak-experiment-single \
    [--rep R] [--timeout T] [--warmup W] [--memory-limit 30G] [--outdir results/full]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `--exe` | (required) | Path to `quak-experiment-single` binary |
| `--rep` | 3 | Repetitions per instance |
| `--timeout` | 300 | Per-repetition timeout (seconds) |
| `--warmup` | 1 | Warmup (0 or 1) |
| `--memory-limit` | none | Memory limit per process (e.g. `30G`, `8192M`) |
| `--outdir` | `results/full` | Output directory for CSV files |

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
│   ├── archived/               # Historical sources and preserved fragments
│   ├── quak-nested-main.cpp    # Main CLI
│   ├── quak-experiment-single.cpp  # Experiment runner
│   └── tests/
│       ├── sanity_tests/       # Flattening, synchronization, etc.
│       ├── correctness_tests/  # Registered semantic regressions
│       ├── probes/             # Optional backend comparison tools
│       └── benchmarks/         # Optional benchmark harnesses
├── examples/
│   ├── example*.cpp            # Example programs
│   └── *.txt                   # Example nested automata
├── samples/                    # Curated samples, test fixtures, generated inputs
│   └── tests/                  # Registered test automata fixtures
├── scripts/                    # Utility scripts
├── results/                    # Generated experiment output
├── experiment.py               # Python experiment orchestrator
├── experiment_small.py         # Small representative experiment subset
├── results/csv_to_latex_figures.py  # Benchmark CSV to LaTeX tables
├── CMakeLists.txt
└── README.md
```

Registered test code lives under `src/tests/sanity_tests/` and
`src/tests/correctness_tests/`; their input automata live under
`samples/tests/`. Optional probes may still build through the
`experiments` target, but they are not CTest tests. Historical implementation
snapshots and source fragments under `src/archived/` are intentionally not
built.

---

## Differences from the Original QuAK (Non-Nested)

This version of QuAK extends the original with nested automata support. The
non-nested library APIs are still present, but the CLI surface is intentionally
narrower than original QuAK. For non-nested automata, the supported CLI actions
are only:

```
non-empty VALF <weight>
universal VALF <weight>
```

Both actions use Buchi acceptance over declared final states. The main
input-format change is:

**Final state declarations**: Non-nested automata now specify final states
using the `final: state1 state2 ...` or `final: all` syntax in their input
files. The CLI `non-empty` and `universal` actions use those final states as
the Buchi accepting set. Use `final: all` to match the original QuAK behavior
where F = Q.
