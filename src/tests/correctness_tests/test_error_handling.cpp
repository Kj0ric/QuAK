#include <iostream>
#include <string>

#include "test_cli_helpers.h"

using namespace cli_test;

namespace {

// ---------------------------------------------------------------------------
// Tests for errors already caught by the CLI argument parser
// ---------------------------------------------------------------------------

void testMissingSumBBound() {
    // SumB requires a bound parameter; CLI should reject if missing.
    const std::string out = runCommandExpectFailure(
        "examples/nested/simple_counter.txt non-empty LimSup SumB 1");
    assertContains(out, "bound", "Missing SumB bound should produce a usage error mentioning 'bound'");
}

void testUnsupportedUniversalCombo() {
    // isUniversal does not support LimSupAvg; CLI should reject it.
    const std::string out = runCommandExpectFailure(
        "examples/nested/simple_counter.txt universal LimSupAvg Max_f 1");
    assertContains(out, "not support", "Unsupported universal combo should mention 'not support'");
}

// ---------------------------------------------------------------------------
// Tests for errors caught by the parser
// ---------------------------------------------------------------------------

void testUndefinedChildIndex() {
    // Parent uses weight 2 (child index 2) but only @CHILD 0 and @CHILD 1 are defined.
    const std::string out = runCommandExpectFailure(
        "src/tests/correctness_tests/inputs/tc_err_undefined_child.txt non-empty LimSup Max_f 1");
    assertContains(out, "child automaton index",
                   "Undefined child index should report 'child automaton index'");
}

// ---------------------------------------------------------------------------
// Tests for errors caught by validateNested()
// ---------------------------------------------------------------------------

void testSilentInChild() {
    // Child 1 has a SILENT transition (non-parseable weight stored as float::max()).
    // validateNested() should abort with a message naming child index and SILENT.
    const std::string out = runCommandExpectFailure(
        "src/tests/correctness_tests/inputs/tc_err_silent_in_child.txt non-empty LimSup Max_f 1");
    assertContains(out, "SILENT",
                   "SILENT in child should produce an error message containing 'SILENT'");
    assertContains(out, "Child automaton 1",
                   "Error message should identify the offending child index");
}

// ---------------------------------------------------------------------------
// Tests for errors caught by isNonEmpty runtime validation
// ---------------------------------------------------------------------------

void testMixedSignLimAvg() {
    // Child 1 has both positive (3) and negative (-2) weights.
    // LimAvg+SumMinus requires all child weights to be <= 0 before the
    // pseudo-det pipeline. Without -DNORMALIZE_MIXED_SIGN, this must be rejected.
    // Threshold must be <= 0: the trivial-case guard (x > 0 && SumMinus → false)
    // would short-circuit before reaching the mixed-sign check for x > 0.
    const std::string out = runCommandExpectFailure(
        "src/tests/correctness_tests/inputs/tc_err_mixed_sign_limavg.txt non-empty LimSupAvg SumMinus 0");
    assertContains(out, "Mixed-sign",
                   "Mixed-sign child weights for LimAvg+SumMinus should produce a 'Mixed-sign' error");
}

// ---------------------------------------------------------------------------
// Smoke tests: valid inputs that must not crash
// ---------------------------------------------------------------------------

void testEdgeCases() {
    // Threshold = 0 boundary
    runCommandExpectSuccess(
        "examples/nested/simple_counter.txt non-empty LimSup Max_f 0");

    // Large threshold with Max_f — bounded by child weights, so returns false cleanly
    // (Note: Inf+SumPlus with very large thresholds is a known memory limitation;
    //  flatten_SumPlusMinus_Inf allocates O(threshold * states) and should not be called
    //  with thresholds in the millions.)
    runCommandExpectSuccess(
        "examples/nested/simple_counter.txt non-empty LimSup Max_f 1000000");

    // Single-state parent + single-state child
    runCommandExpectSuccess(
        "src/tests/correctness_tests/inputs/tc_single_state.txt non-empty LimSup Max_f 1");

    // Negative threshold: weights are >= 0, so any infVal/Max_f value exceeds -1
    runCommandExpectSuccess(
        "examples/nested/simple_counter.txt non-empty LimInf Max_f -1");

    // Large alphabet: 12-symbol parent/child — tests alphabet-indexed structures in flattening
    runCommandExpectSuccess(
        "src/tests/correctness_tests/inputs/tc_large_alphabet.txt non-empty LimSup Max_f 2");
}

} // namespace

int main() {
    try {
        testMissingSumBBound();
        testUnsupportedUniversalCombo();
        testUndefinedChildIndex();
        testSilentInChild();
        testMixedSignLimAvg();
        testEdgeCases();
        std::cout << "Error handling checks passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error handling test failed: " << e.what() << std::endl;
        return 1;
    }
}
