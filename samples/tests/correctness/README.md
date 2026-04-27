Correctness Test Inputs
=======================

These nested automata are fixtures for registered correctness tests under
`src/tests/correctness_tests/` and for selected diagnostic probes under
`src/tests/probes/`.

The fixtures live under `samples/` so all artifact automata are centralized in
one reviewer-facing tree. Tests should refer to these files through
`samples/tests/correctness/...`.

Conventions
-----------

Files are grouped by purpose:

  baseline_*                  Minimal deterministic NQA fixtures used by the
                              smoke test and as universal regression baselines.
                              `_neg` variants negate child weights for SumMinus
                              and dual-threshold checks.

  baseline_fractional*        Same topology as baseline fixtures, with
                              fractional weights for epsilon and LimAvg paths.

  child_pump_loop*            Child automata with pumpable loops.
  deep_nondet_binary*         Deep nondeterministic binary-alphabet fixtures.
  nondet_child_binary*        Nondeterministic child fixtures.
  two_children_binary*        Two-child binary-alphabet fixtures.
  three_children_varied*      Three-child fixtures with varied weight ranges.
  scc_chain_binary*           Parent SCC-chain fixtures.
  positive_only_nondet*       Positive-only nondeterministic child fixtures.
  epsilon_boundary*           Weights near the configured equality epsilon.

  limavg_adversarial_*        LimAvg adversarial families for Max_f, Min_f,
                              SumB, SumPlus, and SumMinus.

  regular_final_continuation* Final-state continuation regressions.
  threshold_extremal_*        Threshold/extremal backend regressions.
  phase_* and split_witness_* Phase and split-witness regressions.

  sum_sup_* and sup_*         Sup/LimSup witness and background-obligation
                              regressions.

  complete_nonterminating_*   Complete parent with nonterminating background
                              behavior.
  max_merge_bug_complete.txt  Regression for the complete Max merge case.

Smoke-Test Baselines
--------------------

The smoke test uses:

  baseline_det.txt
  baseline_det_neg.txt

These should remain small and fast.
