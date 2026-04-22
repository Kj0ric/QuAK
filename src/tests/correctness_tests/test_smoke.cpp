/**
 * test_smoke.cpp
 *
 * Smoke test: one representative CLI call per major flattening path.
 * Intended for artifact evaluation — verifies end-to-end correctness
 * of the quak-nested binary without running the full correctness suite.
 *
 * Flattening paths covered:
 *   (1) flatten_regular          -- {LimSupAvg, LimInfAvg} x {Min, Max} and any x {SumB}
 *   (2) flatten_avg_summinus     -- {LimSupAvg, LimInfAvg} x {SumMinus}
 *   (3) flatten_sumplusminus_sup -- {Sup, LimSup} x {SumPlus, SumMinus}
 *   (4) flatten_sumplusminus_inf -- {Inf, LimInf} x {SumPlus, SumMinus}
 *   (5) flatten_minmax_sup       -- {Sup, LimSup} x {Min, Max}
 *   (6) flatten_minmax_inf       -- {Inf, LimInf} x {Min, Max}
 *
 * All tests use baseline_det.txt (deterministic unary automaton):
 *   parent loops on 'a:1', always invoking child 1
 *   child 1: path [c0 -a:3-> c1 -a:5-> c2(final)]
 *   => Max_f=5, Min_f=3, SumPlus=8, SumMinus=-8 (constant across all infVal)
 *
 * Expected to complete in under 5 seconds.
 */

#include <iostream>
#include <string>

#include "test_cli_helpers.h"

using namespace cli_test;

static const std::string INPUT =
    "src/tests/correctness_tests/inputs/baseline_det.txt";

namespace {

// (1) flatten_regular: LimSupAvg x Max_f
void smokeRegular() {
    const std::string out_t = runCommandExpectSuccess(INPUT + " non-empty LimSupAvg Max_f 4");
    assertContains(out_t, "= 1", "flatten_regular: LimSupAvg/Max_f threshold=4 should be true");

    const std::string out_f = runCommandExpectSuccess(INPUT + " non-empty LimSupAvg Max_f 6");
    assertContains(out_f, "= 0", "flatten_regular: LimSupAvg/Max_f threshold=6 should be false");
}

// (2) flatten_avg_summinus: LimSupAvg x SumMinus
// Uses baseline_det_neg.txt (child weights -3, -5) so all child weights are <= 0.
// SumMinus = -(|-3|+|-5|) = -8. baseline_det.txt has positive weights which are
// rejected for SumMinus+LimAvg unless compiled with -DNORMALIZE_MIXED_SIGN=ON.
void smokeAvgSumMinus() {
    const std::string neg_input =
        "src/tests/correctness_tests/inputs/baseline_det_neg.txt";

    const std::string out_t = runCommandExpectSuccess(neg_input + " non-empty LimSupAvg SumMinus -9");
    assertContains(out_t, "= 1", "flatten_avg_summinus: LimSupAvg/SumMinus threshold=-9 should be true");

    const std::string out_f = runCommandExpectSuccess(neg_input + " non-empty LimSupAvg SumMinus -7");
    assertContains(out_f, "= 0", "flatten_avg_summinus: LimSupAvg/SumMinus threshold=-7 should be false");
}

// (3) flatten_sumplusminus_sup: LimSup x SumPlus
void smokeSumPlusMinusSup() {
    const std::string out_t = runCommandExpectSuccess(INPUT + " non-empty LimSup SumPlus 7");
    assertContains(out_t, "= 1", "flatten_sumplusminus_sup: LimSup/SumPlus threshold=7 should be true");

    const std::string out_f = runCommandExpectSuccess(INPUT + " non-empty LimSup SumPlus 9");
    assertContains(out_f, "= 0", "flatten_sumplusminus_sup: LimSup/SumPlus threshold=9 should be false");
}

// (4) flatten_sumplusminus_inf: Inf x SumMinus
void smokeSumPlusMinusInf() {
    const std::string out_t = runCommandExpectSuccess(INPUT + " non-empty Inf SumMinus -9");
    assertContains(out_t, "= 1", "flatten_sumplusminus_inf: Inf/SumMinus threshold=-9 should be true");

    const std::string out_f = runCommandExpectSuccess(INPUT + " non-empty Inf SumMinus -7");
    assertContains(out_f, "= 0", "flatten_sumplusminus_inf: Inf/SumMinus threshold=-7 should be false");
}

// (5) flatten_minmax_sup: LimSup x Max_f
void smokeMinMaxSup() {
    const std::string out_t = runCommandExpectSuccess(INPUT + " non-empty LimSup Max_f 4");
    assertContains(out_t, "= 1", "flatten_minmax_sup: LimSup/Max_f threshold=4 should be true");

    const std::string out_f = runCommandExpectSuccess(INPUT + " non-empty LimSup Max_f 6");
    assertContains(out_f, "= 0", "flatten_minmax_sup: LimSup/Max_f threshold=6 should be false");
}

// (6) flatten_minmax_inf: Inf x Min_f
void smokeMinMaxInf() {
    const std::string out_t = runCommandExpectSuccess(INPUT + " non-empty Inf Min_f 2");
    assertContains(out_t, "= 1", "flatten_minmax_inf: Inf/Min_f threshold=2 should be true");

    const std::string out_f = runCommandExpectSuccess(INPUT + " non-empty Inf Min_f 4");
    assertContains(out_f, "= 0", "flatten_minmax_inf: Inf/Min_f threshold=4 should be false");
}

// (7) universal: LimSup x Max_f
void smokeUniversal() {
    const std::string out_t = runCommandExpectSuccess(INPUT + " universal LimSup Max_f 4");
    assertContains(out_t, "= 1", "universal: LimSup/Max_f threshold=4 should be true");

    const std::string out_f = runCommandExpectSuccess(INPUT + " universal LimSup Max_f 6");
    assertContains(out_f, "= 0", "universal: LimSup/Max_f threshold=6 should be false");
}

} // namespace

int main() {
    try {
        smokeRegular();
        smokeAvgSumMinus();
        smokeSumPlusMinusSup();
        smokeSumPlusMinusInf();
        smokeMinMaxSup();
        smokeMinMaxInf();
        smokeUniversal();
        std::cout << "Smoke tests passed (7 paths, 14 checks)." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Smoke test FAILED: " << e.what() << std::endl;
        return 1;
    }
}
