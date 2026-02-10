#!/usr/bin/env python3
"""
Benchmark comparison for OblBag implementations.
Tests three approaches: SET, INCREMENTAL, BATCH (default).
"""

import os
import subprocess
import sys
import re
import time
from pathlib import Path

PROJECT_DIR = Path(__file__).parent.parent
SRC_FILE = PROJECT_DIR / "src" / "NestedAutomaton.cpp"
TEST_DIR = PROJECT_DIR / "experiments" / "test_automata"
BENCHMARK_EXE = PROJECT_DIR / "benchmark_oblbag"

TIMEOUT = 120  # seconds

def modify_source(approach: str):
    """Modify source file to enable specific approach."""
    content = SRC_FILE.read_text()

    # Reset both flags to commented
    content = re.sub(r'^#define USE_SET_OBLBAG', '// #define USE_SET_OBLBAG', content, flags=re.MULTILINE)
    content = re.sub(r'^#define USE_INCREMENTAL_BAG', '// #define USE_INCREMENTAL_BAG', content, flags=re.MULTILINE)

    # Enable the requested approach
    if approach == "SET":
        content = re.sub(r'^// #define USE_SET_OBLBAG', '#define USE_SET_OBLBAG', content, flags=re.MULTILINE)
    elif approach == "INCREMENTAL":
        content = re.sub(r'^// #define USE_INCREMENTAL_BAG', '#define USE_INCREMENTAL_BAG', content, flags=re.MULTILINE)
    # BATCH is default (both commented)

    SRC_FILE.write_text(content)

def build():
    """Build the benchmark executable."""
    result = subprocess.run(
        ["make", "-j4", "benchmark_oblbag"],
        cwd=PROJECT_DIR,
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        print(f"Build failed: {result.stderr}")
        return False
    return True

def run_benchmark(automaton: Path, bound: int) -> dict:
    """Run benchmark and parse results."""
    try:
        result = subprocess.run(
            [str(BENCHMARK_EXE), str(automaton), str(bound), "SumB"],
            capture_output=True,
            text=True,
            timeout=TIMEOUT
        )
        output = result.stdout

        data = {
            "automaton": automaton.stem,
            "bound": bound,
            "status": "OK",
        }

        # Parse output
        for line in output.split("\n"):
            if "Wall time:" in line:
                match = re.search(r'Wall time: (\d+) ms', line)
                if match:
                    data["wall_time_ms"] = int(match.group(1))
            elif "Peak RSS:" in line:
                match = re.search(r'Peak RSS: (\d+) KB', line)
                if match:
                    data["peak_rss_kb"] = int(match.group(1))
            elif "States:" in line and "Parent" not in line:
                match = re.search(r'States: (\d+)', line)
                if match:
                    data["output_states"] = int(match.group(1))
            elif "bag_add calls:" in line:
                match = re.search(r'bag_add calls: (\d+)', line)
                if match:
                    data["bag_add_calls"] = int(match.group(1))
            elif "bag_add total time:" in line:
                match = re.search(r'bag_add total time: ([\d.]+) ms', line)
                if match:
                    data["bag_add_ms"] = float(match.group(1))
            elif "bag_finalize total time:" in line:
                match = re.search(r'bag_finalize total time: ([\d.]+) ms', line)
                if match:
                    data["bag_finalize_ms"] = float(match.group(1))

        return data

    except subprocess.TimeoutExpired:
        return {"automaton": automaton.stem, "bound": bound, "status": "TIMEOUT"}
    except Exception as e:
        return {"automaton": automaton.stem, "bound": bound, "status": f"ERROR: {e}"}

def main():
    print("=" * 80)
    print("OblBag Implementation Benchmark Comparison - Large Scale Test")
    print("=" * 80)

    approaches = ["BATCH", "INCREMENTAL", "SET"]

    # Test configurations focusing on larger instances
    test_configs = [
        # Medium-sized for quick comparison
        ("large_3", [6, 7, 8, 9]),

        # Larger instances for stress testing
        ("xlarge_3", [6, 7, 8]),
        ("large_4", [6, 7, 8]),
    ]

    # Collect all results
    all_results = []

    for approach in approaches:
        print(f"\n{'='*80}")
        print(f"Testing approach: {approach}")
        print("="*80)

        modify_source(approach)
        if not build():
            print(f"Failed to build {approach}")
            continue

        for pattern, bounds in test_configs:
            import glob
            automata = sorted(glob.glob(str(TEST_DIR / f"{pattern}.txt")))

            for automaton in automata:
                automaton = Path(automaton)
                for bound in bounds:
                    print(f"  {automaton.stem} bound={bound}...", end=" ", flush=True)
                    start = time.time()
                    result = run_benchmark(automaton, bound)
                    result["approach"] = approach
                    elapsed = time.time() - start

                    if result["status"] == "OK":
                        wall = result.get("wall_time_ms", 0)
                        rss = result.get("peak_rss_kb", 0)
                        states = result.get("output_states", 0)
                        print(f"{wall} ms, {rss/1024:.1f} MB, {states} states")
                    else:
                        print(result["status"])

                    all_results.append(result)

                    # Skip remaining bounds if this one timed out
                    if result["status"] == "TIMEOUT":
                        print(f"    Skipping higher bounds for {automaton.stem}")
                        break

    # Reset to default (BATCH)
    print("\nResetting to default (BATCH)...")
    modify_source("BATCH")
    build()

    # Print summary table
    print("\n" + "=" * 80)
    print("SUMMARY - Wall Time Comparison")
    print("=" * 80)

    # Group by automaton+bound
    from collections import defaultdict
    grouped = defaultdict(dict)
    for r in all_results:
        if r["status"] == "OK":
            key = (r["automaton"], r["bound"])
            grouped[key][r["approach"]] = r

    print(f"\n{'Automaton':<12} {'Bound':<6} {'States':<10} {'BATCH':<12} {'INCR':<12} {'SET':<12} {'Best':<8}")
    print("-" * 80)

    for (automaton, bound), approaches_data in sorted(grouped.items()):
        batch = approaches_data.get("BATCH", {})
        incr = approaches_data.get("INCREMENTAL", {})
        set_data = approaches_data.get("SET", {})

        batch_t = batch.get("wall_time_ms")
        incr_t = incr.get("wall_time_ms")
        set_t = set_data.get("wall_time_ms")
        states = batch.get("output_states") or incr.get("output_states") or set_data.get("output_states") or 0

        times = {"BATCH": batch_t, "INCR": incr_t, "SET": set_t}
        valid_times = {k: v for k, v in times.items() if v is not None}
        winner = min(valid_times, key=valid_times.get) if valid_times else "-"

        b_str = f"{batch_t}ms" if batch_t is not None else "-"
        i_str = f"{incr_t}ms" if incr_t is not None else "-"
        s_str = f"{set_t}ms" if set_t is not None else "-"

        print(f"{automaton:<12} {bound:<6} {states:<10} {b_str:<12} {i_str:<12} {s_str:<12} {winner:<8}")

    # Speedup analysis
    print("\n" + "=" * 80)
    print("SPEEDUP ANALYSIS (relative to BATCH)")
    print("=" * 80)
    print(f"\n{'Automaton':<12} {'Bound':<6} {'INCR speedup':<15} {'SET speedup':<15}")
    print("-" * 60)

    for (automaton, bound), approaches_data in sorted(grouped.items()):
        batch_t = approaches_data.get("BATCH", {}).get("wall_time_ms")
        incr_t = approaches_data.get("INCREMENTAL", {}).get("wall_time_ms")
        set_t = approaches_data.get("SET", {}).get("wall_time_ms")

        if batch_t and batch_t > 0:
            incr_speedup = f"{batch_t/incr_t:.2f}x" if incr_t else "-"
            set_speedup = f"{batch_t/set_t:.2f}x" if set_t else "-"
        else:
            incr_speedup = "-"
            set_speedup = "-"

        print(f"{automaton:<12} {bound:<6} {incr_speedup:<15} {set_speedup:<15}")

    # Memory summary
    print("\n" + "=" * 80)
    print("MEMORY USAGE (Peak RSS in MB)")
    print("=" * 80)
    print(f"\n{'Automaton':<12} {'Bound':<6} {'BATCH':<12} {'INCR':<12} {'SET':<12}")
    print("-" * 60)

    for (automaton, bound), approaches_data in sorted(grouped.items()):
        batch = approaches_data.get("BATCH", {}).get("peak_rss_kb")
        incr = approaches_data.get("INCREMENTAL", {}).get("peak_rss_kb")
        set_rss = approaches_data.get("SET", {}).get("peak_rss_kb")

        b_str = f"{batch/1024:.1f}" if batch else "-"
        i_str = f"{incr/1024:.1f}" if incr else "-"
        s_str = f"{set_rss/1024:.1f}" if set_rss else "-"

        print(f"{automaton:<12} {bound:<6} {b_str:<12} {i_str:<12} {s_str:<12}")

    # Save detailed results to CSV
    csv_path = PROJECT_DIR / "experiments" / "results" / "benchmark_results.csv"
    csv_path.parent.mkdir(exist_ok=True)

    with open(csv_path, "w") as f:
        headers = ["approach", "automaton", "bound", "status", "wall_time_ms", "peak_rss_kb",
                   "output_states", "bag_add_calls", "bag_add_ms", "bag_finalize_ms"]
        f.write(",".join(headers) + "\n")
        for r in all_results:
            row = [str(r.get(h, "")) for h in headers]
            f.write(",".join(row) + "\n")

    print(f"\nDetailed results saved to: {csv_path}")

if __name__ == "__main__":
    main()
