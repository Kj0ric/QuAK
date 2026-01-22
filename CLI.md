# QuAK CLI Documentation

## Build

```bash
mkdir -p build && cd build
cmake ..
make -j4
cd ..
```

The executable `quak` is placed in the project root.

## Usage

```bash
./quak [OPTIONS] automaton-file [ACTION ...]
```

### Options
| Option           | Description          |
| ---------------- | -------------------- |
| `-cputime`       | Show execution time  |
| `-debug`         | Verbose debug output |
| `-print-witness` | Print witness word   |

---

## Library Functions

### Non-Nested Automata

| #   | Library Function            | CLI Command                  |
| --- | --------------------------- | ---------------------------- |
| 1   | `A->isNonEmpty(Val, v)`     | `non-empty VALF <weight>`    |
| 2   | `A->isUniversal(Val, v)`    | `universal VALF <weight>`    |
| 3   | `A->isIncludedIn(B, Val)`   | `isIncluded VALF file2`      |
| 4   | `A->isEquivalentTo(B, Val)` | `isEquivalent VALF file2`    |
| 5   | `A->isConstant(Val)`        | `constant VALF`              |
| 6   | `A->isSafe(Val)`            | `safe VALF`                  |
| 7   | `A->isLive(Val)`            | `live VALF`                  |
| 8   | `A->getTopValue(Val)`       | `top-value VALF`             |
| 9   | `A->getBottomValue(Val)`    | `bottom-value VALF`          |
| 10  | `safetyClosure(A, Val)`     | `safetyComponent VALF out`   |
| 11  | `livenessComponent(A, Val)` | `livenessComponent VALF out` |

**Aggregators (VALF):** `Inf | Sup | LimInf | LimSup | LimInfAvg | LimSupAvg`

### Nested Automata

| #   | Library Function                     | CLI Command                                 |
| --- | ------------------------------------ | ------------------------------------------- |
| 1   | `NA->isNonEmpty(InfVal, FinVal, x)`  | `non-empty VALF FINVAL <threshold> [bound]` |
| 2   | `NA->isUniversal(InfVal, FinVal, x)` | `universal VALF FINVAL <threshold> [bound]` |

**Finite Aggregators (FINVAL):** `Max | Min | SumB | SumPlus | SumMinus`

Nested files are auto-detected by the `@PARENT` marker.

### Supported Nested Combinations

| Decision  | Finite Agg     | Infinite Agg             |
| --------- | -------------- | ------------------------ |
| non-empty | SumPlus        | All                      |
| non-empty | SumMinus       | LimInfAvg, LimSupAvg     |
| non-empty | Max, Min, SumB | All                      |
| universal | Max, Min, SumB | Inf, LimInf, Sup, LimSup |

---

## Examples

```bash
# Non-nested
./quak A.txt non-empty LimInf 0
./quak A.txt top-value LimSup
./quak A.txt isEquivalent LimInf B.txt
./quak A.txt decompose LimInf safe.txt live.txt
./quak -print-witness A.txt non-empty LimInf 0

# Nested
./quak nested.txt non-empty LimInf SumPlus 1
./quak nested.txt universal LimInf Max 1
./quak nested.txt non-empty LimSupAvg SumB 2 10
```

## Output

```
----------
isNonEmpty(LimInf, weight=0) = 1
----------
```

Result: `1` = true, `0` = false
