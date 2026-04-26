#!/usr/bin/env python3
import argparse
import csv
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


HEADER = [
    "backend",
    "library",
    "n",
    "k",
    "file",
    "threshold",
    "status",
    "result",
    "elapsed_ms",
    "peak_rss_kb",
    "rss_delta_kb",
    "states",
    "transitions",
]


def parse_probe_row(line: str) -> Dict[str, str]:
    row = next(csv.reader([line]))
    if len(row) != 9:
        raise ValueError(f"Expected 9 probe columns, got {len(row)}: {line!r}")
    return {
        "backend": row[0],
        "file": row[1],
        "threshold": row[2],
        "states": row[3],
        "transitions": row[4],
        "result": row[5],
        "elapsed_ms": row[6],
        "peak_rss_kb": row[7],
        "rss_delta_kb": row[8],
    }


def load_done_rows(csv_path: Path) -> Set[Tuple[str, int, int]]:
    done: Set[Tuple[str, int, int]] = set()
    if not csv_path.exists():
        return done
    with csv_path.open("r", newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            try:
                done.add((row["backend"], int(row["n"]), int(row["k"])))
            except Exception:
                continue
    return done


def ensure_header(csv_path: Path) -> None:
    if csv_path.exists():
        return
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        csv.writer(handle).writerow(HEADER)


def instance_path(root: Path, n: int, k: int) -> Path:
    return root / f"resource_n{n}_k{k}.txt"


def run_probe(exe: Path,
              backend: str,
              file_path: Path,
              timeout_s: float) -> Tuple[str, Optional[Dict[str, str]], str]:
    try:
        proc = subprocess.run(
            [str(exe), backend, str(file_path), str(k_from_path(file_path))],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
            timeout=timeout_s,
        )
    except subprocess.TimeoutExpired as exc:
        stderr = exc.stderr or ""
        return ("TIMEOUT", None, stderr.strip())

    stdout = proc.stdout.strip()
    stderr = proc.stderr.strip()

    if proc.returncode != 0:
        return ("ERR", None, stderr if stderr else stdout)

    if not stdout:
        return ("ERR", None, "empty stdout")

    try:
        return ("OK", parse_probe_row(stdout.splitlines()[-1]), stderr)
    except Exception as exc:
        return ("ERR", None, str(exc))


def k_from_path(file_path: Path) -> int:
    stem = file_path.stem
    try:
        return int(stem.split("_k", 1)[1])
    except Exception as exc:
        raise ValueError(f"Could not parse k from {file_path}") from exc


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Benchmark Sup-Max nonemptiness backends on resource-consumption instances."
    )
    ap.add_argument("--current-exe", required=True, help="Path to the current-library Sup-Max probe.")
    ap.add_argument("--split-exe", required=True, help="Path to the old-v2 split-witness Sup-Max probe.")
    ap.add_argument("--out", default="results/minmax_sup_resource_small.csv",
                    help="CSV file for raw benchmark rows.")
    ap.add_argument("--timeout", type=float, default=30.0,
                    help="Per-backend timeout in seconds.")
    ap.add_argument("--max-n", type=int, default=5,
                    help="Largest n to include from the resource-consumption family.")
    ap.add_argument("--max-k", type=int, default=5,
                    help="Largest k to include from the resource-consumption family.")
    args = ap.parse_args()

    current_exe = Path(args.current_exe).resolve()
    split_exe = Path(args.split_exe).resolve()
    out_csv = Path(args.out)
    samples_root = Path("samples/generated_resource_consumption")

    for exe in (current_exe, split_exe):
        if not exe.exists():
            print(f"ERROR: executable not found: {exe}", file=sys.stderr)
            return 2

    ensure_header(out_csv)
    done = load_done_rows(out_csv)

    configs = [
        {"backend": "threshold_extremal", "library": "quak", "exe": current_exe},
        {"backend": "regular", "library": "quak", "exe": current_exe},
        {"backend": "split_witness", "library": "quak_old_v2", "exe": split_exe},
    ]

    with out_csv.open("a", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        for n in range(1, args.max_n + 1):
            for k in range(1, args.max_k + 1):
                file_path = instance_path(samples_root, n, k)
                if not file_path.exists():
                    print(f"ERROR: missing benchmark input: {file_path}", file=sys.stderr)
                    return 2

                for config in configs:
                    key = (config["backend"], n, k)
                    if key in done:
                        continue

                    print(f"[{config['backend']}] n={n} k={k} ...", end="", flush=True)
                    status, parsed, err = run_probe(
                        exe=config["exe"],
                        backend=config["backend"],
                        file_path=file_path,
                        timeout_s=args.timeout,
                    )

                    if status == "OK" and parsed is not None:
                        writer.writerow([
                            config["backend"],
                            config["library"],
                            n,
                            k,
                            parsed["file"],
                            parsed["threshold"],
                            status,
                            parsed["result"],
                            parsed["elapsed_ms"],
                            parsed["peak_rss_kb"],
                            parsed["rss_delta_kb"],
                            parsed["states"],
                            parsed["transitions"],
                        ])
                        print(
                            f" OK elapsed_ms={parsed['elapsed_ms']} "
                            f"rss_kb={parsed['peak_rss_kb']} states={parsed['states']}",
                            flush=True,
                        )
                    else:
                        writer.writerow([
                            config["backend"],
                            config["library"],
                            n,
                            k,
                            str(file_path),
                            str(k),
                            status,
                            "",
                            "",
                            "",
                            "",
                            "",
                            "",
                        ])
                        if err:
                            print(f" {status} ({err[:120]})", flush=True)
                        else:
                            print(f" {status}", flush=True)

                    handle.flush()
                    done.add(key)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
