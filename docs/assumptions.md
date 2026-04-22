# QuAK: Assumptions and Enforcements 

This document describes the core assumptions, requirements enforced by QuAK's implementation.

## 1. Automaton Class Hierarchy

### 1.1 Inheritance Structure
- **Base class**: `Automaton`
- **Derived classes**: `ChildAutomaton`, `NestedAutomaton`
- All automaton types share common properties:
  - Single initial state (enforced)
  - Immutable after construction (except `setMinDomain()` and `setMaxDomain()`)

### 1.2 Non-Determinism Resolution
- In quantitative automata, non-determinism is resolved by **Supremum** value function (as opposed to **Infimum** resolver in the NWA paper)

## 2. State and Structural Properties

### 2.1 State Ownership
- Each state must be owned by exactly one automaton
- Enforced by `Automaton::appropriateStates()`
- Verifies no state is already owned before assignment

### 2.2 Completeness Requirement
- All automata must be **complete**: for every state `q` and symbol `a`, there exists at least one transition `q --a--> q'`
- Checked by `Automaton::isComplete()`
- Incomplete automata are completed by adding sink states

### 2.3 Reachability and SCC Structure
- **Algorithm**: Tarjan's algorithm computes Strongly Connected Components (SCCs)
- **SCC immutability**: SCC structure is computed once and never modified
- **State tags**: 
  - `tag >= 0`: reachable state (indicates SCC ID)
  - `tag == -1`: unreachable state
- **Initial state**: always reachable
- Lower SCC IDs are reachable from higher IDs
- **Trimming**: If unreachable states are found, a new automaton is built from trimmed data

## 3. Acceptance Conditions

### 3.1 Non-Nested Automata (from file input)
- **Explicit final states**: Declared with blank separated `final: s0 s1` (optional)
- **Default behavior**: If no `final:` declaration, all states are final (F = Q)
- **Acceptance**: Accept infinite runs visiting at least one accepting run infinitely

### 3.2 Parent Automata
- All states are implicitly final

### 3.3 Child Automata (Nested)
- At least one explicit final state required
-  Must use `final: state1 state2 ...` keyword in input file
- **Acceptance**: Finite word accepted if it reaches a final state
- **Enforcement**: Parser validation ensures at least one final state per non-dummy child

### 3.4 Büchi Automata (from `transformToBuchi()`)
- **Final states**: Explicit subset F $\subseteq$ Q
- **Acceptance**: Büchi condition (infinite run visits F infinitely often)

## 4. Nested Automaton Specific Rules

### 4.1 Child Indexing
- **Index 0**: Reserved for dummy child (always)
- **Actual children**: Start from index 1 (`@CHILD 1`, `@CHILD 2`, ...)
- **Parent weights**: Encode child indices (integer values)

### 4.2 Dummy Child Properties
- **Alphabet**: Empty (no symbols)
- **States**: One state named `"dummy"`
- **Transitions**: None
- **Final states**: None (not required for dummy)
- **Purpose**: Placeholder for parent transitions that don't invoke any child

### 4.3 Alphabet Synchronization
- **Requirement**: Parent and all non-dummy children must share the same alphabet
- Not enforced (?)

### 4.4 Silent Transitions
- **Parent**: May have silent transitions (keyword `SILENT` = max 32-bit float)
- **Children**: Should **not** have silent transitions (undefined behavior if present (?))
- **Removing silent**: `Automaton::removeSilentTransitions()` handles different value functions differently

### 4.5 Child Weight Sign Requirements

The sign requirements on child weights depend on the chosen finite aggregator (finVal):

| finVal | Required sign | Why |
|--------|--------------|-----|
| `SumPlus` | All weights ≥ 0 | SumPlus = Σ\|xᵢ\|; negative weights indicate mixed-sign input |
| `SumMinus` | All weights ≤ 0 | SumMinus = −Σ\|xᵢ\|; positive weights indicate mixed-sign input |
| All others | No constraint | Sign is irrelevant or handled internally |

**Non-LimAvg paths** (`Sup`, `LimSup`, `Inf`, `LimInf`): `flatten_SumPlusMinus_Sup/Inf` handle absolute-value normalization internally — mixed-sign children are accepted and automatically normalized.

**LimAvg paths** (`LimSupAvg`, `LimInfAvg`): The pseudo-determinization + synchronization pipeline in `flatten_Avg_SumMinus` assumes pre-normalized weights. Mixed-sign children are therefore **rejected by default** with a clear error message.

To enable automatic normalization for LimAvg paths, recompile with:
```
cmake -DNORMALIZE_MIXED_SIGN=ON ...
```
This silently applies absolute-value normalization before entering the pipeline.

## 5. Input File Format

### 5.1 General Syntax
- **Transition format**: `symbol : weight, from_state -> to_state`
- **Weight format**: `weight_t` (float)
- **Initial state**: from_state of the first transition in the file
- **Comments**: Lines starting with `#` are ignored

### 5.2 Non-Nested Automaton Files
- **Structure**: List of transitions only (no section headers)
- **Detection**: Files without `@PARENT` keyword are treated as non-nested

### 5.3 Nested Automaton Files
- **Structure**: 
  - `@PARENT` section: master automaton transitions with child indices as weights
  - `@CHILD 0`: dummy child (no transitions, no final states)
  - `@CHILD n` (n ≥ 1): actual children with explicit final state declarations
- **Final state syntax**: `final: state1 state2 state3 ...`
- **Detection**: Presence of `@PARENT` keyword

### 5.4 Silent Transitions
- **Keyword**: `SILENT` as weight value
- **Internal representation**: `std::numeric_limits<float>::max()`
- **Allowed in**: Parent automaton only