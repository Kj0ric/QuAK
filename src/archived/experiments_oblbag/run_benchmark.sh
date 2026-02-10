#!/bin/bash
#
# Benchmark script for comparing OblBag implementations.
# Tests three approaches: SET, VECTOR_BATCH (default), VECTOR_INCREMENTAL
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR"
TEST_DIR="$SCRIPT_DIR/test_automata"
RESULTS_DIR="$SCRIPT_DIR/results"

TIMEOUT_SEC=120  # 2 minutes max per test

# Approaches to test
APPROACHES=("BATCH" "INCREMENTAL" "SET")

# Bounds to test
BOUNDS=(1 2 3 4 5)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Generate test automata
generate_automata() {
    log "Generating test automata..."
    mkdir -p "$TEST_DIR"
    python3 "$SCRIPT_DIR/generate_test_automata.py" --output-dir "$TEST_DIR" --seed 42
}

# Compile with specific flags
compile_approach() {
    local approach=$1
    log "Compiling with approach: $approach"

    cd "$PROJECT_DIR/src"

    # Modify NestedAutomaton.cpp based on approach
    case $approach in
        "SET")
            sed -i 's|^// #define USE_SET_OBLBAG|#define USE_SET_OBLBAG|' NestedAutomaton.cpp
            sed -i 's|^#define USE_INCREMENTAL_BAG|// #define USE_INCREMENTAL_BAG|' NestedAutomaton.cpp
            ;;
        "INCREMENTAL")
            sed -i 's|^#define USE_SET_OBLBAG|// #define USE_SET_OBLBAG|' NestedAutomaton.cpp
            sed -i 's|^// #define USE_INCREMENTAL_BAG|#define USE_INCREMENTAL_BAG|' NestedAutomaton.cpp
            ;;
        "BATCH")
            sed -i 's|^#define USE_SET_OBLBAG|// #define USE_SET_OBLBAG|' NestedAutomaton.cpp
            sed -i 's|^#define USE_INCREMENTAL_BAG|// #define USE_INCREMENTAL_BAG|' NestedAutomaton.cpp
            ;;
    esac

    cd "$BUILD_DIR"
    make -j4 benchmark_oblbag 2>/dev/null || make -j4
}

# Run a single benchmark
run_single_benchmark() {
    local automaton=$1
    local bound=$2
    local output_file=$3

    timeout "$TIMEOUT_SEC" "$BUILD_DIR/benchmark_oblbag" "$automaton" "$bound" "SumB" > "$output_file" 2>&1
    return $?
}

# Parse results from output file
parse_results() {
    local output_file=$1
    local wall_time=$(grep "Wall time:" "$output_file" 2>/dev/null | awk '{print $3}')
    local peak_rss=$(grep "Peak RSS:" "$output_file" 2>/dev/null | awk '{print $3}')
    local out_states=$(grep "^States:" "$output_file" 2>/dev/null | awk '{print $2}')
    local bag_add_time=$(grep "bag_add total time:" "$output_file" 2>/dev/null | awk '{print $4}')
    local bag_finalize_time=$(grep "bag_finalize total time:" "$output_file" 2>/dev/null | awk '{print $4}')

    echo "$wall_time,$peak_rss,$out_states,$bag_add_time,$bag_finalize_time"
}

# Main benchmark loop
run_benchmarks() {
    mkdir -p "$RESULTS_DIR"

    # CSV header
    local csv_file="$RESULTS_DIR/benchmark_results.csv"
    echo "approach,automaton,bound,wall_time_ms,peak_rss_kb,output_states,bag_add_ms,bag_finalize_ms" > "$csv_file"

    for approach in "${APPROACHES[@]}"; do
        log "=== Testing approach: $approach ==="
        compile_approach "$approach"

        for automaton in "$TEST_DIR"/*.txt; do
            local name=$(basename "$automaton" .txt)

            for bound in "${BOUNDS[@]}"; do
                local output_file="$RESULTS_DIR/${approach}_${name}_b${bound}.txt"
                log "Running: $name with bound=$bound"

                if run_single_benchmark "$automaton" "$bound" "$output_file"; then
                    local results=$(parse_results "$output_file")
                    echo "$approach,$name,$bound,$results" >> "$csv_file"
                    log "  -> Completed ($(echo $results | cut -d, -f1) ms)"
                else
                    warn "  -> TIMEOUT or ERROR"
                    echo "$approach,$name,$bound,TIMEOUT,,,," >> "$csv_file"
                fi
            done
        done
    done

    log "Results saved to: $csv_file"
}

# Print summary
print_summary() {
    local csv_file="$RESULTS_DIR/benchmark_results.csv"

    log "=== SUMMARY ==="
    echo ""
    echo "Approach comparison (average wall time in ms):"
    echo "----------------------------------------------"

    for approach in "${APPROACHES[@]}"; do
        local avg=$(grep "^$approach," "$csv_file" | grep -v "TIMEOUT" | cut -d, -f4 | awk '{sum+=$1; n++} END {if(n>0) print sum/n; else print "N/A"}')
        printf "%-15s: %s ms\n" "$approach" "$avg"
    done

    echo ""
    echo "Full results in: $csv_file"
}

# Reset to default (BATCH) after benchmarks
reset_to_default() {
    log "Resetting to default (BATCH) approach..."
    cd "$PROJECT_DIR/src"
    sed -i 's|^#define USE_SET_OBLBAG|// #define USE_SET_OBLBAG|' NestedAutomaton.cpp
    sed -i 's|^#define USE_INCREMENTAL_BAG|// #define USE_INCREMENTAL_BAG|' NestedAutomaton.cpp
    cd "$BUILD_DIR"
    make -j4 >/dev/null 2>&1 || true
}

# Main
main() {
    log "OblBag Implementation Benchmark"
    log "================================"

    generate_automata
    run_benchmarks
    print_summary
    reset_to_default

    log "Benchmark complete!"
}

main "$@"
