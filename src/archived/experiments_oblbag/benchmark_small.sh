#!/bin/bash

# Small-scale OblBag benchmark
# Response time: n,k <= 10
# Resource consumption: n,k <= 5

QUAK_DIR="$(cd "$(dirname "$0")" && pwd)"
QUAK_BIN="$QUAK_DIR/quak-main"
SRC_FILE="$QUAK_DIR/src/NestedAutomaton.cpp"

INFVAL="LimInfAvg"
FINVAL="SumB:auto"
THRESHOLD="0"
REPS=1
TIMEOUT=120

RESULTS_FILE="$QUAK_DIR/oblbag_small_results.csv"

set_implementation() {
    if [ "$1" = "set" ]; then
        sed -i 's|^// #define USE_SET_OBLBAG|#define USE_SET_OBLBAG|' "$SRC_FILE"
    else
        sed -i 's|^#define USE_SET_OBLBAG|// #define USE_SET_OBLBAG|' "$SRC_FILE"
    fi
}

rebuild() {
    cd "$QUAK_DIR" && make -j4 > /dev/null 2>&1
}

run_benchmark() {
    local file=$1
    result=$(/usr/bin/time -v "$QUAK_BIN" "$file" nonempty "$INFVAL" "$FINVAL" "$THRESHOLD" --rep $REPS --timeout-s $TIMEOUT 2>&1)
    mean_s=$(echo "$result" | grep -oP 'MEAN_S=\K[0-9.]+' || echo "NA")
    status=$(echo "$result" | grep -oP 'STATUS=\K\w+' || echo "NA")
    max_rss=$(echo "$result" | grep -oP 'Maximum resident set size.*: \K[0-9]+' || echo "NA")
    echo "$mean_s,$max_rss,$status"
}

echo "benchmark,n,k,impl,mean_time_s,max_rss_kb,status" > "$RESULTS_FILE"

RESPONSE_DIR="$QUAK_DIR/samples/generated_response_time"
RESOURCE_DIR="$QUAK_DIR/samples/generated_resource_consumption"

# Response time configs: select representative points n,k <= 10
RESPONSE_CONFIGS="2,2 3,3 4,4 5,5 6,6 7,7 8,8 4,8 8,4 6,8 8,6"

# Resource consumption configs: n,k <= 5
RESOURCE_CONFIGS="1,1 1,2 1,3 2,1 2,2 2,3 3,1 3,2 3,3 3,4 4,3 4,4"

for impl in "vector" "set"; do
    echo "=========================================="
    echo "Testing $impl implementation"
    echo "=========================================="
    set_implementation "$impl"
    rebuild

    echo ""
    echo "--- Response Time Benchmarks (n,k <= 10) ---"
    for config in $RESPONSE_CONFIGS; do
        n=$(echo $config | cut -d, -f1)
        k=$(echo $config | cut -d, -f2)
        file="$RESPONSE_DIR/response_n${n}_k${k}.txt"
        if [ -f "$file" ]; then
            echo -n "  n=$n k=$k: "
            result=$(run_benchmark "$file")
            echo "$result"
            echo "response_time,$n,$k,$impl,$result" >> "$RESULTS_FILE"
        fi
    done

    echo ""
    echo "--- Resource Consumption Benchmarks (n,k <= 5) ---"
    for config in $RESOURCE_CONFIGS; do
        n=$(echo $config | cut -d, -f1)
        k=$(echo $config | cut -d, -f2)
        file="$RESOURCE_DIR/resource_n${n}_k${k}.txt"
        if [ -f "$file" ]; then
            echo -n "  n=$n k=$k: "
            result=$(run_benchmark "$file")
            echo "$result"
            echo "resource_consumption,$n,$k,$impl,$result" >> "$RESULTS_FILE"
        fi
    done
done

# Reset to vector
set_implementation "vector"
rebuild

echo ""
echo "=========================================="
echo "RESULTS SUMMARY"
echo "=========================================="
echo ""
cat "$RESULTS_FILE"
