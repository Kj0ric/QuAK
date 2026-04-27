#!/usr/bin/env bash
# scripts/smoke-test.sh
# One-command smoke test for QuAK-NQA.
#
# PURPOSE
#   This script is the intended entry point for the AE smoke test phase.
#   It checks that the artifact is correctly installed and that all components
#   start up and run without error.
#
# WHAT IS TESTED (four sections)
#   [1] Binaries        — quak-nested and quak-experiment-single are present
#                         and executable. Catches wrong-architecture images or
#                         a broken Dockerfile COPY step.
#   [2] Input files     — sample automata (samples/A.txt), correctness fixtures
#                         (samples/tests/correctness/), and pre-generated
#                         benchmark inputs (samples/generated_*_N) are present. Catches
#                         missing files that would make experiment.py silently
#                         run zero instances or quak-nested fail to open inputs.
#   [3] Python env      — python3 is installed and experiment.py can be loaded
#                         and its argument parser initialised. Catches a missing
#                         interpreter or an import error in the experiment script.
#   [4] Decision procs  — quak-nested is invoked once per major flattening path
#                         on a small representative input. Confirms the binary
#                         reads input and produces output in the expected format
#                         ("= 1" / "= 0"). Not a correctness claim; a crash or
#                         malformed output here indicates a build/install defect.
#
# WHAT IS NOT TESTED
#   - Paper results (runtimes, scalability) -> see full review instructions
#     in AE_README.md.
#   - Experiment correctness -> experiment.py is only tested for startup; actual
#     runs are part of the full review.
#
# USAGE
#   bash scripts/smoke-test.sh
#   QUAK=./build/quak-nested QUAK_EXP=./build/quak-experiment-single bash scripts/smoke-test.sh
#
# Exit code: 0 = all checks passed, 1 = any check failed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [[ $# -gt 0 ]]; then
  echo "Usage: $0" >&2
  exit 1
fi

START=$SECONDS

QUAK="${QUAK:-$REPO_ROOT/quak-nested}"
QUAK_EXP="${QUAK_EXP:-$REPO_ROOT/quak-experiment-single}"
SAMPLES="$REPO_ROOT/samples"
INPUTS="$SAMPLES/tests/correctness"

PASS=0
FAIL=0

# Helper: assert exit 0 and required expected string in output.
# Usage: run_check "label" "expected_string" cmd [args...]
run_check() {
  local label="$1"
  shift
  local expected="$1"
  shift
  local output exit_code=0
  output=$("$@" 2>&1) || exit_code=$?
  if [[ $exit_code -ne 0 ]]; then
    echo "  FAIL [$label] -- exited $exit_code"
    echo "       output: $(echo "$output" | head -3)"
    FAIL=$((FAIL + 1))
  elif [[ -n "$expected" ]] && ! echo "$output" | grep -qF "$expected"; then
    echo "  FAIL [$label] -- expected '$expected' not found in output"
    echo "       got: $(echo "$output" | head -3)"
    FAIL=$((FAIL + 1))
  else
    echo "  PASS [$label]"
    PASS=$((PASS + 1))
  fi
}

# Helper: assert a path exists with a given test flag (-x, -d, -f).
check_path() {
  local label="$1" flag="$2" path="$3"
  if test "$flag" "$path"; then
    echo "  PASS [$label]"
    PASS=$((PASS + 1))
  else
    echo "  FAIL [$label] — not found or wrong type: $path"
    FAIL=$((FAIL + 1))
  fi
}

# -----------------------------------------------------------------------
# Section 1 — Binary availability
# Verifies that compiled executables are present and executable.
# A failure here means the Docker image was not built correctly or the
# wrong architecture image was loaded.
# -----------------------------------------------------------------------
echo "==> [1/4] Checking binaries..."
check_path "quak-nested exists and is executable" -x "$QUAK"
check_path "quak-experiment-single exists and is executable" -x "$QUAK_EXP"

# -----------------------------------------------------------------------
# Section 2 — Input file availability
# Verifies that the test fixtures and pre-generated benchmark inputs are
# present. A failure here means the COPY step in the Dockerfile is wrong
# or the files were missing from the build context.
# -----------------------------------------------------------------------
echo "==> [2/4] Checking input files..."
check_path "samples/tests/correctness/ directory present" -d "$INPUTS"
check_path "samples/ directory present" -d "$SAMPLES"
check_path "samples/A.txt present (non-nested fixture)" -f "$SAMPLES/A.txt"

# Verify the exact generated benchmark directories used by experiment.py
# are non-empty so it cannot silently run zero instances.
GENERATED_COUNT=0
MISSING_GENERATED=()
for dir in \
  generated_response_time_1 \
  generated_response_time_2 \
  generated_response_time_3 \
  generated_resource_consumption_1 \
  generated_resource_consumption_2
do
  path="$SAMPLES/$dir"
  if [[ ! -d "$path" ]]; then
    MISSING_GENERATED+=("$dir")
    continue
  fi
  count=$(find "$path" -maxdepth 1 -type f -name "*.txt" 2>/dev/null | wc -l)
  if [[ "$count" -eq 0 ]]; then
    MISSING_GENERATED+=("$dir (empty)")
    continue
  fi
  GENERATED_COUNT=$((GENERATED_COUNT + count))
done

if [[ "${#MISSING_GENERATED[@]}" -eq 0 ]]; then
  echo "  PASS [generated benchmark inputs present ($GENERATED_COUNT files)]"
  PASS=$((PASS + 1))
else
  echo "  FAIL [generated benchmark inputs] -- missing: ${MISSING_GENERATED[*]}"
  FAIL=$((FAIL + 1))
fi

# -----------------------------------------------------------------------
# Section 3 — Python environment and experiment script
# Verifies that python3 is installed and that experiment.py can be loaded
# and its argument parser initialised without error. A failure here means
# the experiment workflow would crash before running a single instance.
# -----------------------------------------------------------------------
echo "==> [3/4] Checking Python environment and experiment script..."
run_check "python3 is available" "Python 3" python3 --version
run_check "experiment.py loads and accepts --help" \
  "usage" python3 "$REPO_ROOT/experiment.py" --help

# -----------------------------------------------------------------------
# Section 4 — Decision procedure CLI checks
# Runs quak-nested on small representative inputs to confirm the binary
# executes, reads input files, and produces output in the expected format.
# One check per major flattening path. This is a technical installation
# check, not a verification of paper claims.
# CLI syntax: quak-nested INPUTFILE ACTION VALF FINVAL THRESHOLD
# Output format: "= 1" (non-empty/universal) or "= 0" (empty/not-universal)
# -----------------------------------------------------------------------
echo "==> [4/4] Checking decision procedures (one per flattening path)..."

run_check "flatten_regular      LimSupAvg/Max_f   (non-empty, expect = 1)" \
  "= 1" "$QUAK" "$INPUTS/baseline_det.txt" non-empty LimSupAvg Max_f 4
run_check "flatten_avg_summinus LimSupAvg/SumMinus (non-empty, expect = 1)" \
  "= 1" "$QUAK" "$INPUTS/baseline_det_neg.txt" non-empty LimSupAvg SumMinus -9
run_check "flatten_sp_sm_sup    LimSup/SumPlus    (non-empty, expect = 1)" \
  "= 1" "$QUAK" "$INPUTS/baseline_det.txt" non-empty LimSup SumPlus 7
run_check "flatten_sp_sm_inf    Inf/SumMinus      (non-empty, expect = 1)" \
  "= 1" "$QUAK" "$INPUTS/baseline_det_neg.txt" non-empty Inf SumMinus -9
run_check "flatten_minmax_sup   LimSup/Max_f      (non-empty, expect = 1)" \
  "= 1" "$QUAK" "$INPUTS/baseline_det.txt" non-empty LimSup Max_f 4
run_check "flatten_minmax_inf   Inf/Min_f         (non-empty, expect = 1)" \
  "= 1" "$QUAK" "$INPUTS/baseline_det.txt" non-empty Inf Min_f 2
run_check "universality         LimSup/Max_f      (universal, expect = 1)" \
  "= 1" "$QUAK" "$INPUTS/baseline_det.txt" universal LimSup Max_f 4
run_check "non-nested curated  LimSup            (non-empty, expect = 1)" \
  "= 1" "$QUAK" "$SAMPLES/A.txt" non-empty LimSup 0

# -----------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------
echo ""
TOTAL=$((PASS + FAIL))
ELAPSED=$((SECONDS - START))
if [[ $FAIL -eq 0 ]]; then
  echo "SMOKE PASSED -- $PASS/$TOTAL checks, ${ELAPSED}s wall"
  exit 0
else
  echo "SMOKE FAILED -- $FAIL/$TOTAL checks failed (${ELAPSED}s wall)"
  exit 1
fi
