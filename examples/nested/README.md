# QuAK Example Programs

This directory contains example programs demonstrating nested weighted automata analysis using QuAK.

## Building the Examples

From the project root directory:

```bash
cmake . -DCMAKE_BUILD_TYPE=Release
make example1_basic example2_value_functions example3_response_time
```

Or build all examples:

```bash
make -j4
```

## Running the Examples

Run from the project root directory (required for sample file paths):

```bash
./example1_basic
./example2_value_functions
./example3_response_time
```

## Example Descriptions

### Example 1: Basic Usage (`example1_basic.cpp`)

Compares four finite value functions (`Max_f`, `Min_f`, `SumPlus`, `SumB`) at two thresholds on a simple counter automaton. Shows how each finVal caps or grows differently.

### Example 2: Value Function Combinations (`example2_value_functions.cpp`)

Uses a priority-task system where the parent alternates high- and low-priority tasks:

- **Varying finVal**: Shows how `Max_f`, `SumPlus`, and `SumB` differ when accumulating child weights
- **Non-emptiness vs Universality**: Contrasts "can we achieve this?" with "is it guaranteed?"

### Example 3: Comparing Infinite Value Functions (`example3_response_time.cpp`)

Compares four infinite value functions (Sup, LimSup, LimInf, Inf) on two automata with different finite value functions.

- On `request_response.txt` with SumPlus: Sup/LimSup are TRUE for any threshold (response time is unbounded), while LimInf/Inf cap at 1 (the last request before each grant always waits just 1 step)
- On `priority_tasks.txt` with Max_f: Sup/LimSup see the best child, LimInf/Inf require all children to meet the threshold

## Sample Automata Files

- `simple_counter.txt`: Counts symbols before termination
- `request_response.txt`: Models request-grant protocol with timing
- `priority_tasks.txt`: Models tasks with different priority levels

## Nested Automaton File Format

```
@PARENT
<symbol> : <weight>, <source> -> <target>
...

@CHILD <index>
final: <final_states>
<symbol> : <weight>, <source> -> <target>
...
```

The parent automaton triggers child automata based on transitions. Each child runs until it reaches a final state, producing a value that contributes to the overall automaton value.

## API Reference

```cpp
// Load nested automaton
NestedAutomaton* nwa = new NestedAutomaton("path/to/file.txt");

// Non-emptiness: exists word w with value >= threshold?
bool result = nwa->isNonEmpty(infVal, finVal, threshold, bound);

// Universality: all words w have value >= threshold?
bool result = nwa->isUniversal(infVal, finVal, threshold, bound);

// Clean up
delete nwa;
```

Parameters:
- `infVal`: Infinite aggregation function (LimSup, LimInf, Sup, Inf, etc.)
- `finVal`: Finite aggregation function (Max_f, Min_f, SumB, etc.)
- `threshold`: Value to compare against
- `bound`: Required for SumB, optional otherwise (use -1 or omit)
