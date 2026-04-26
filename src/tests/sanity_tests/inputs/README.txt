Test Case Inputs for NestedAutomaton Sanity Tests
=================================================

These input files are designed with known expected behaviors documented
in comments. They are sanity-test fixtures and can also seed future
semantic regression tests after the expected behavior is made explicit in
registered test code.

Test Cases:
-----------

tc01_simple_det.txt
    Simple deterministic nested automaton
    - 1 parent state, 2 children (one empty)
    - Tests basic flatten and emptiness
    - Child returns exactly 2

tc02_two_children_det.txt
    Two children with different return values
    - Deterministic parent and child
    - Child returns 3 (Max), 0 (Min after reaching final)
    - Tests varying aggregator behavior

tc03_nondet_child.txt
    Non-deterministic child automaton
    - Parent deterministic, child non-deterministic
    - Two paths with different values (1 vs 5)
    - Tests determinization

tc04_multi_symbol.txt
    Multiple symbols (a, b)
    - Tests alphabet handling
    - Both parent and child use both symbols
    - Verifies macro alphabet generation

tc05_zero_weights.txt
    All weights are zero
    - Edge case: minimum possible weights
    - Tests boundary conditions

tc06_large_weights.txt
    Large weight values (100, 1000)
    - Tests handling of larger numbers
    - Verifies bound capping in SumB

tc07_multiple_finals.txt
    Child has multiple final states
    - Three final states with different paths
    - Non-deterministic choice of which final to reach
    - Different SumB values per path

tc08_chain_parent.txt
    Linear chain in parent
    - Parent: q0 -> q1 -> q2 -> q2 (loop)
    - Tests non-complete parent
    - Eventually loops with weight 3

tc09_empty_child.txt
    All children empty
    - Edge case: no transitions in children
    - Tests graceful handling of empty children

tc10_three_children.txt
    Three children with distinct return values
    - Tests synchronization of multiple children
    - Each child returns 1, 2, 3 respectively
    - Product state space = 8 states


Usage:
------
These files use the standard nested automaton format:
  @PARENT
  final: all
  symbol : weight, source -> target

  @CHILD n
  final: state_name
  symbol : weight, source -> target

Expected behaviors are documented in comments at the top of each file.
To promote one into a correctness test, assert the documented expected
values in C++ test code instead of relying on comments.


Format Notes:
-------------
- Lines starting with # are comments (ignored by parser)
- @PARENT marks the parent automaton section
- @CHILD n marks child automaton n
- Every parent automaton and every child automaton except `@CHILD 0` must
  contain a nonempty `final:` declaration
- `final: all` marks all states as final/accepting
- `final: s1 s2 ...` marks exactly that explicit list of states as final/accepting
- `@CHILD 0` is reserved by the current parser as the implicit dummy/silent child
- Transitions: "symbol : weight, source -> target"
- First transition's source is the initial state
