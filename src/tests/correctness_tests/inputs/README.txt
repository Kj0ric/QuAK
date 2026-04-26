Correctness test inputs for QuAK nested automata
=================================================

Convention
----------

Files in this directory are grouped by purpose:

  baseline_*        Minimal deterministic NQA fixtures used by the smoke test and
                    correctness tests as a universal regression baseline.
                    baseline_det.txt / _neg.txt: deterministic parent, 2-step child,
                    weights +3/+5 (positive) and -3/-5 (negative).
                    baseline_fractional.txt / _neg.txt: same topology, fractional weights.
                    Note: baseline_det.txt is intentionally kept separate from
                    sanity_tests/inputs/tc01_simple_det.txt even though the parent
                    topology is similar; baseline uses weights suited for LimAvg
                    correctness testing while tc01 uses unit weights for structural
                    sanity checks. They serve different test families.

  child_pump_loop   Tests child automata that can loop (pump), exercising liveness
  deep_nondet       of the underlying decision procedures.
  nondet_child
  two_children
  three_children
  scc_chain
  positive_only
  epsilon_boundary  Weight at the boundary of the epsilon-equality threshold (1e-5).
  mixed_sign        Child with both positive and negative transition weights.
  *_neg.txt         Mirror of the same automaton with negated child weights; used to
                    test the dual (negative/zero) answer for emptiness / universality.

  limavg_adversarial_*   Adversarial NQA instances crafted to stress the LimAvg
                         flattening procedures (pseudo-determinization + synchronization):
                         _max, _min, _sumb, _sumplus, _summinus variants; diamond and
                         unary/unbounded structural families.

  tc_bug_*          Regression fixture for a specific fixed bug (see commit history).
  tc_err_*          Invalid-input fixtures used by test_error_handling to verify that
                    the CLI rejects malformed automata with the correct error message.
  tc_large_alphabet Fixture with a 12-symbol alphabet.
  tc_single_state   Single-state parent edge case.

Files
-----

  baseline_det.txt                  Minimal det NQA, positive weights (3, 5)
  baseline_det_neg.txt              Same, negative weights (-3, -5)
  baseline_fractional.txt           Minimal det NQA, fractional weights (0.3, 0.5)
  baseline_fractional_neg.txt       Same, negative fractional weights
  child_pump_loop.txt               Child with a looping (pumping) structure, positive
  child_pump_loop_neg.txt           Same, negative weights
  deep_nondet_binary.txt            Deeply non-deterministic binary-alphabet NQA, positive
  deep_nondet_binary_neg.txt        Same, negative weights
  epsilon_boundary.txt              Weights at epsilon threshold, positive
  epsilon_boundary_neg.txt          Same, negative
  limavg_adversarial_max.txt        LimAvg adversarial, Max_f finite aggregator
  limavg_adversarial_min.txt        LimAvg adversarial, Min_f finite aggregator
  limavg_adversarial_sumb.txt       LimAvg adversarial, SumB finite aggregator
  limavg_adversarial_summinus_diamond.txt   SumMinus, diamond topology
  limavg_adversarial_summinus_unary.txt     SumMinus, unary-chain topology
  limavg_adversarial_summinus_unbounded.txt SumMinus, unbounded-weight variant
  limavg_adversarial_sumplus.txt            SumPlus baseline adversarial
  limavg_adversarial_sumplus_diamond.txt    SumPlus, diamond topology
  limavg_adversarial_sumplus_unary.txt      SumPlus, unary-chain topology
  limavg_adversarial_sumplus_unbounded.txt  SumPlus, unbounded-weight variant
  mixed_sign.txt                    Child with mixed-sign weights
  nondet_child_binary.txt           Non-det child, binary alphabet, positive
  nondet_child_binary_neg.txt       Same, negative weights
  positive_only_nondet.txt          Non-det child, positive-only weights
  positive_only_nondet_neg.txt      Same, negated
  scc_chain_binary.txt              SCCs chained in parent, binary alphabet, positive
  scc_chain_binary_neg.txt          Same, negative weights
  tc_bug_limavg_sumplus.txt         Regression for LimAvg+SumPlus projection bug (fixed)
  tc_err_mixed_sign_limavg.txt      Error case: mixed-sign + LimAvg+SumPlus (rejected)
  tc_err_silent_in_child.txt        Error case: SILENT transition inside child automaton
  tc_err_undefined_child.txt        Error case: parent invokes an undefined child index
  tc_large_alphabet.txt             12-symbol alphabet stress test
  tc_single_state.txt               Single-state parent edge case
  three_children_varied.txt         Three children with varied weight ranges, positive
  three_children_varied_neg.txt     Same, negative weights
  two_children_binary.txt           Two children, binary alphabet, positive
  two_children_binary_neg.txt       Same, negative weights
