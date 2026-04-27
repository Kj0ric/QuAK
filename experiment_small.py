#!/usr/bin/env python3
"""Run the small representative QuAK experiment subset."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import experiment as runner


Pair = Tuple[int, int]

SMALL_SUBSET: Dict[str, List[Pair]] = {
    "response_sup_sumplus_emptiness": [
        (4, 256),
        (4, 512),
        (16, 256),
        (16, 512),
        (64, 256),
        (64, 512),
    ],
    "response_limsupavg_sumplus_emptiness": [
        (3, 9),
        (3, 10),
        (5, 9),
        (5, 10),
        (7, 9),
        (7, 10),
    ],
    "response_sup_sumb_universality": [
        (2, 8),
        (2, 9),
        (4, 8),
        (4, 9),
        (6, 8),
        (6, 9),
    ],
    "resource_sup_max_emptiness": [
        (6, 1),
        (5, 2),
        (4, 3),
        (3, 4),
        (2, 5),
        (1, 6),
    ],
    "resource_limsupavg_max_emptiness": [
        (4, 1),
        (3, 2),
        (2, 3),
        (1, 4),
    ],
}


def run_small_experiment(
    exp,
    pairs: List[Pair],
    exe: Path,
    rep: int,
    timeout_s: float,
    warmup: int,
    memory_limit_bytes: Optional[int],
    append: bool,
) -> bool:
    print(f"[{exp.name}] running {len(pairs)} selected inputs -> {exp.out_csv}", flush=True)

    ok = True
    available = {
        (n, k): path for n, k, path in runner.discover_inputs(exp.input_dir)[0]
    }
    f, w = runner.open_output(exp.out_csv, append=append)
    with f:
        for idx, (n, k) in enumerate(pairs, start=1):
            file_path = available.get((n, k))
            if file_path is None:
                print(
                    f"[{exp.name}] {idx}/{len(pairs)} n={n} k={k} "
                    f"missing input in {exp.input_dir}",
                    file=sys.stderr,
                    flush=True,
                )
                ok = False
                continue

            threshold = runner.threshold_for(exp, k)
            print(
                f"[{exp.name}] {idx}/{len(pairs)} n={n} k={k} "
                f"file={file_path.name} -> running...",
                end="",
                flush=True,
            )

            status, mean_s, result01, out, err, rc = runner.base.run_cpp_once(
                exe=exe,
                file_path=file_path,
                problem=exp.problem,
                infval=exp.infval,
                finval=exp.finval,
                threshold=threshold,
                rep=rep,
                timeout_s=timeout_s,
                warmup=warmup,
                memory_limit_bytes=memory_limit_bytes,
            )
            runner.print_status(status, mean_s, result01, err, rc)

            w.writerow([
                exp.name,
                n,
                k,
                str(file_path),
                exp.problem,
                exp.infval,
                exp.finval,
                threshold,
                rep,
                timeout_s,
                warmup,
                runner.csv_status(status),
                (f"{mean_s:.9f}" if status == "OK" else ""),
                (result01 if status in ("OK", "INCONSISTENT") else ""),
            ])
            f.flush()

    return ok


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Run the small representative QuAK experiment subset."
    )
    ap.add_argument(
        "--exe",
        default="./build/quak-experiment-single",
        help="Path to compiled quak-experiment-single executable.",
    )
    ap.add_argument("--rep", type=int, default=3, help="How many repetitions C++ should run per input.")
    ap.add_argument("--timeout", type=float, default=300.0, help="Per-repetition timeout in seconds.")
    ap.add_argument("--warmup", type=int, default=1, help="Warmup inside C++ (0 or 1).")
    ap.add_argument(
        "--memory-limit",
        type=str,
        default="30G",
        help="Memory limit per process (default: 30G). Uses RLIMIT_AS.",
    )
    ap.add_argument(
        "--outdir",
        default="results/small",
        help="Directory to write CSVs. Existing CSVs are overwritten unless --append is set.",
    )
    ap.add_argument(
        "--append",
        action="store_true",
        help="Append rows to existing CSVs. The script still runs every selected input.",
    )
    args = ap.parse_args()

    exe = Path(args.exe).expanduser()
    exe = (Path.cwd() / exe).resolve() if not exe.is_absolute() else exe.resolve()
    if not exe.exists():
        print(f"ERROR: --exe not found: {exe}", file=sys.stderr)
        return 2

    if runner.base.resource is None and args.memory_limit:
        print(
            "WARNING: 'resource' module not available. Memory limits will be ignored.",
            file=sys.stderr,
        )
        memory_limit_bytes = None
    elif args.memory_limit:
        memory_limit_bytes = runner.base.parse_memory_limit(args.memory_limit)
        print(
            f"Memory limit: {memory_limit_bytes} bytes "
            f"({memory_limit_bytes / (1024**3):.2f} GB)"
        )
    else:
        memory_limit_bytes = None

    experiments = {exp.name: exp for exp in runner.build_experiments(Path(args.outdir))}
    all_inputs_present = True

    for name, pairs in SMALL_SUBSET.items():
        print(f"=== {name} ===", flush=True)
        all_inputs_present = run_small_experiment(
            exp=experiments[name],
            pairs=pairs,
            exe=exe,
            rep=args.rep,
            timeout_s=args.timeout,
            warmup=args.warmup,
            memory_limit_bytes=memory_limit_bytes,
            append=args.append,
        ) and all_inputs_present

    print("Done.")
    return 0 if all_inputs_present else 1


if __name__ == "__main__":
    raise SystemExit(main())
