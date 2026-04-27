# QuAK CLI Documentation

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The main executable is `build/quak-nested`.

## Usage

```bash
./build/quak-nested [OPTIONS] automaton-file [ACTION ...]
```

### Options
| Option           | Description          |
| ---------------- | -------------------- |
| `-cputime`       | Show execution time  |
| `-v`             | Print input size     |
| `-d`             | Print automaton      |
| `-debug`         | Verbose debug output |

---

## Library Functions

### Non-Nested Automata

| #   | Library Function            | CLI Command                  |
| --- | --------------------------- | ---------------------------- |
| 1   | `A->isNonEmpty_withFinal(Val, v)` | `non-empty VALF <weight>` |
| 2   | `A->isUniversal_withFinal(Val, v)` | `universal VALF <weight>` |

**Aggregators (VALF):** `Inf | Sup | LimInf | LimSup | LimInfAvg | LimSupAvg`

Non-nested CLI actions intentionally expose only Buchi non-emptiness and
universality checks over declared final states. Other non-nested operations
remain library APIs, not supported CLI commands in this working tree. Use
`final: all` to recover the original all-states-accepting QuAK behavior for
these two checks.

### Nested Automata

| #   | Library Function                     | CLI Command                                 |
| --- | ------------------------------------ | ------------------------------------------- |
| 1   | `NA->isNonEmpty(InfVal, FinVal, x)`  | `non-empty VALF FINVAL <threshold> [bound]` |
| 2   | `NA->isUniversal(InfVal, FinVal, x)` | `universal VALF FINVAL <threshold> [bound]` |

**Finite Aggregators (FINVAL):** `Max_f | Min_f | SumB | SumPlus | SumMinus`

Nested files are auto-detected by the `@PARENT` marker.

### Supported Nested Combinations

| Decision  | Finite Agg     | Infinite Agg             |
| --------- | -------------- | ------------------------ |
| non-empty | SumPlus        | Inf, Sup, LimInf, LimSup, LimSupAvg |
| non-empty | Max_f, Min_f, SumB, SumMinus | All          |
| universal | Max_f, Min_f, SumB, SumPlus, SumMinus | Inf, LimInf, Sup, LimSup |

For non-nested automata, `universal` quantifies over Buchi-accepted words
according to the declared final states. For nested automata, `universal` quantifies over
accepted words of the flattened automaton; rejected flattened words are
ignored.

---

## Examples

```bash
# Non-nested
./build/quak-nested samples/A.txt non-empty LimInf 0
./build/quak-nested samples/A.txt universal LimSup 4

# Nested
./build/quak-nested examples/simple_counter.txt non-empty LimInf SumPlus 1
./build/quak-nested examples/priority_tasks.txt universal LimInf Max_f 1
./build/quak-nested examples/request_response.txt non-empty LimSupAvg SumB 2 10
```

## Output

```
----------
isNonEmpty(LimInf, weight=0) = 1
----------
```

Result: `1` = true, `0` = false
