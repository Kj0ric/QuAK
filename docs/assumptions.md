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
- **Explicit final states**: Declared with blank separated `final: s0 s1` or `final: all`
- **Requirement**: File input must contain a nonempty `final:` declaration
- **Original all-final behavior**: Use `final: all` to express F = Q
- **Acceptance**: Accept infinite words that have a run visiting a final state infinitely often

### 3.2 Parent Automata
- Parent sections must contain a nonempty `final:` declaration
- Use `final: all` when all parent states should be final

### 3.3 Child Automata (Nested)
- At least one explicit final state required
- Must use `final: state1 state2 ...` or `final: all` in input file
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
- **Final states**: Parser-generated; the dummy child is exempt from the non-dummy final-state requirement
- **Purpose**: Placeholder for parent transitions that don't invoke any child

### 4.3 Alphabet Synchronization
- **Requirement**: Parent and all non-dummy children must share the same alphabet
- Child parsers inherit the parent alphabet

### 4.4 Silent Transitions
- **Parent**: May have no-child transitions using child index `0`, and may also use the `SILENT` keyword (max 32-bit float) for generated silent steps
- **Children**: Should **not** have silent transitions
- **Removing silent**: `Automaton::removeSilentTransitions()` handles different value functions differently

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
  - `@CHILD 0`: dummy child placeholder (no transitions and no `final:` line in the file)
  - `@CHILD n` (n ≥ 1): actual children with explicit final state declarations
- **Final state syntax**: `final: state1 state2 state3 ...` or `final: all`
- **Detection**: Presence of `@PARENT` keyword

### 5.4 Silent Transitions
- **No child**: Parent weight `0` invokes the dummy child and emits no child return value
- **Keyword**: `SILENT` as weight value for generated silent parent steps
- **Internal representation**: `std::numeric_limits<float>::max()`
- **Allowed in**: Parent automaton only
