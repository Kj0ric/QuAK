# QuAK CLI Documentation

The QuAK command-line interface supports both **regular** and **nested** quantitative automata. File type is automatically detected based on the presence of the `@PARENT` marker.

## Usage

```bash
./quak [OPTIONS] automaton-file [ACTION ...]
```

### Global Options

| Option | Description |
|--------|-------------|
| `-cputime` | Display execution time in milliseconds |
| `-v` | Verbose output |
| `-d` | Dump automaton structure |
| `-debug` | Show detailed debug information |
| `-print-witness` | Print witness word when available |

---

## Regular Automata

For automata files **without** the `@PARENT` marker.

### Aggregators (VALF)
`Inf | Sup | LimInf | LimSup | LimInfAvg | LimSupAvg`

### Actions

```bash
# Statistics
./quak file.txt stats

# Emptiness/Non-emptiness
./quak file.txt non-empty VALF <threshold>
./quak file.txt empty VALF <threshold>
./quak file.txt universal VALF <threshold>

# Properties
./quak file.txt constant VALF
./quak file.txt safe VALF
./quak file.txt live VALF

# Comparisons
./quak file.txt isIncluded VALF file2.txt
./quak file.txt isEquivalent VALF file2.txt

# Decomposition
./quak file.txt livenessComponent VALF output.txt
./quak file.txt safetyComponent VALF output.txt
./quak file.txt decompose VALF safety.txt liveness.txt

# Monitoring
./quak file.txt eval <Inf|Sup|Avg> word-file
./quak file.txt monitor <Inf|Sup|Avg> word-file
```

---

## Nested Automata

For automata files **with** the `@PARENT` marker.

### Infinite Aggregators (VALF)
`Inf | Sup | LimInf | LimSup | LimInfAvg | LimSupAvg`

### Finite Aggregators (FINVAL)
`Max | Min | SumB | SumPlus | SumMinus`

### Actions

```bash
# Non-emptiness: exists word with value >= threshold
./quak nested.txt non-empty VALF FINVAL <threshold> [bound]

# Universality: all words have value >= threshold
./quak nested.txt universal VALF FINVAL <threshold> [bound]
```

### Supported Combinations

| Decision | Finite Aggregator | Infinite Aggregator | Notes |
|----------|-------------------|---------------------|-------|
| non-empty | SumPlus | Inf, LimInf, Sup, LimSup | ✅ |
| non-empty | SumPlus | LimInfAvg, LimSupAvg | ✅ |
| non-empty | SumMinus | LimInfAvg, LimSupAvg | ✅ Only Avg |
| non-empty | Max, Min | All | ✅ |
| non-empty | SumB | All | ✅ Requires `bound` |
| universal | Max, Min, SumB | Inf, LimInf, Sup, LimSup | ✅ |
| universal | SumPlus, SumMinus | * | ❌ Undecidable |
| universal | * | LimInfAvg, LimSupAvg | ❌ Undecidable |

---

## Examples

```bash
# Regular automaton: check if non-empty with LimInf threshold 5
./quak -cputime samples/regular.txt non-empty LimInf 5

# Nested automaton: check emptiness with SumPlus finite aggregator
./quak samples/nested/avg_resp_2_2.txt non-empty LimInf SumPlus 1

# Nested automaton with debug info
./quak -debug nested.txt non-empty LimInf Max 0

# Nested automaton: universality with bound
./quak nested.txt universal Sup SumB 2 10
```

## Output Format

```
----------
isNonEmpty(LimInf, SumPlus, threshold=1) = 1
Cputime: 8 ms (with -cputime flag)
----------
```

- Result `1` = true (non-empty / universal)
- Result `0` = false (empty / not universal)
