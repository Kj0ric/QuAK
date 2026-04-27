#!/usr/bin/env python3
"""Run every configured QuAK experiment input without resume or abort skips."""

from __future__ import annotations

import argparse
import csv
import importlib.util
import sys
from pathlib import Path
from typing import List, Optional, Tuple


def load_base_module():
    base_path = Path(__file__).resolve().parent / "src" / "archived" / "experiment_skip_oot_oom.py"
    spec = importlib.util.spec_from_file_location("experiment_skip_oot_oom_base", base_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load base experiment module from {base_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


base = load_base_module()


CSV_HEADER = [
    "experiment",
    "n",
    "k",
    "file",
    "problem",
    "infval",
    "finval",
    "threshold",
    "rep",
    "timeout_s",
    "warmup",
    "status",
    "mean_s",
    "result01",
]


def discover_inputs(directory: Path) -> Tuple[List[Tuple[int, int, Path]], int]:
    """Return every parseable input file, sorted by n, k, and filename."""
    inputs: List[Tuple[int, int, Path]] = []
    ignored_txt = 0

    if not directory.exists() or not directory.is_dir():
        return inputs, ignored_txt

    for ent in directory.iterdir():
        if not ent.is_file() or ent.suffix != ".txt":
            continue
        nk = base.parse_n_k_from_filename(ent.name)
        if nk is None:
            ignored_txt += 1
            continue
        n, k = nk
        inputs.append((n, k, ent))

    inputs.sort(key=lambda item: (item[0], item[1], item[2].name))
    return inputs, ignored_txt


def threshold_for(exp: base.Experiment, k: int) -> float:
    return float(k) if exp.threshold_mode == "k" else float(exp.threshold_const)


def open_output(csv_path: Path, append: bool):
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    should_write_header = (
        not append or not csv_path.exists() or csv_path.stat().st_size == 0
    )
    f = csv_path.open("a" if append else "w", newline="", encoding="utf-8")
    w = csv.writer(f)
    if should_write_header:
        w.writerow(CSV_HEADER)
        f.flush()
    return f, w


def print_status(status: str, mean_s: float, result01: int, err: str, rc: int) -> None:
    if status == "OK":
        print(f" OK  mean_s={mean_s:.9f}  result={result01}", flush=True)
        return

    if status == "TIMEOUT":
        print(f" OOT  rc={rc}  (continuing)", flush=True)
        return

    if status == "OOM":
        print(f" OOM  rc={rc}  (continuing)", flush=True)
        return

    if status == "KILLED":
        print(f" KILLED  rc={rc}  (continuing)", flush=True)
        return

    err_clean = err.strip().replace("\n", " || ")
    print(f" {status}  rc={rc}  stderr='{err_clean[:80]}...'", flush=True)


def csv_status(status: str) -> str:
    return "OOT" if status == "TIMEOUT" else status


def run_experiment_all_inputs(
    exp: base.Experiment,
    exe: Path,
    rep: int,
    timeout_s: float,
    warmup: int,
    memory_limit_bytes: Optional[int],
    append: bool,
) -> None:
    inputs, ignored_txt = discover_inputs(exp.input_dir)
    if not inputs:
        print(f"[{exp.name}] WARNING: no inputs found in {exp.input_dir}", file=sys.stderr)
        return
    if ignored_txt:
        print(
            f"[{exp.name}] WARNING: ignored {ignored_txt} .txt files without parseable _n/_k names",
            file=sys.stderr,
        )

    print(
        f"[{exp.name}] running {len(inputs)} inputs from {exp.input_dir} -> {exp.out_csv}",
        flush=True,
    )

    f, w = open_output(exp.out_csv, append=append)
    with f:
        for idx, (n, k, file_path) in enumerate(inputs, start=1):
            threshold = threshold_for(exp, k)
            print(
                f"[{exp.name}] {idx}/{len(inputs)} n={n} k={k} "
                f"file={file_path.name} -> running...",
                end="",
                flush=True,
            )

            status, mean_s, result01, out, err, rc = base.run_cpp_once(
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
            print_status(status, mean_s, result01, err, rc)

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
                csv_status(status),
                (f"{mean_s:.9f}" if status == "OK" else ""),
                (result01 if status in ("OK", "INCONSISTENT") else ""),
            ])
            f.flush()


def build_experiments(outdir: Path) -> List[base.Experiment]:
    response_dir_1 = Path("samples/generated_response_time_1")
    response_dir_2 = Path("samples/generated_response_time_2")
    response_dir_3 = Path("samples/generated_response_time_3")
    resource_dir_1 = Path("samples/generated_resource_consumption_1")
    resource_dir_2 = Path("samples/generated_resource_consumption_2")

    return [
        base.Experiment(
            name="response_sup_sumplus_emptiness",
            input_dir=response_dir_1,
            problem="emptiness",
            infval="Sup",
            finval="SumPlus",
            threshold_mode="k",
            threshold_const=0.0,
            out_csv=outdir / "response_sup_sumplus_emptiness.csv",
        ),
        base.Experiment(
            name="response_limsupavg_sumplus_emptiness",
            input_dir=response_dir_2,
            problem="emptiness",
            infval="LimSupAvg",
            finval="SumPlus",
            threshold_mode="k",
            threshold_const=0.0,
            out_csv=outdir / "response_limsupavg_sumplus_emptiness.csv",
        ),
        base.Experiment(
            name="response_sup_sumb_universality",
            input_dir=response_dir_3,
            problem="universality",
            infval="Sup",
            finval="SumB:auto",
            threshold_mode="const",
            threshold_const=1.0,
            out_csv=outdir / "response_sup_sumb_universality.csv",
        ),
        base.Experiment(
            name="resource_sup_max_emptiness",
            input_dir=resource_dir_1,
            problem="emptiness",
            infval="Sup",
            finval="Max",
            threshold_mode="k",
            threshold_const=0.0,
            out_csv=outdir / "resource_sup_max_emptiness.csv",
        ),
        base.Experiment(
            name="resource_limsupavg_max_emptiness",
            input_dir=resource_dir_2,
            problem="emptiness",
            infval="LimSupAvg",
            finval="Max",
            threshold_mode="k",
            threshold_const=0.0,
            out_csv=outdir / "resource_limsupavg_max_emptiness.csv",
        ),
    ]


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Run all configured QuAK experiment inputs without skipping."
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
        default="results/full",
        help="Directory to write CSVs. Existing CSVs are overwritten unless --append is set.",
    )
    ap.add_argument(
        "--append",
        action="store_true",
        help="Append rows to existing CSVs. The script still runs every input.",
    )
    args = ap.parse_args()

    exe = Path(args.exe).expanduser()
    exe = (Path.cwd() / exe).resolve() if not exe.is_absolute() else exe.resolve()
    if not exe.exists():
        print(f"ERROR: --exe not found: {exe}", file=sys.stderr)
        return 2

    if base.resource is None and args.memory_limit:
        print(
            "WARNING: 'resource' module not available. Memory limits will be ignored.",
            file=sys.stderr,
        )
        memory_limit_bytes = None
    elif args.memory_limit:
        memory_limit_bytes = base.parse_memory_limit(args.memory_limit)
        print(
            f"Memory limit: {memory_limit_bytes} bytes "
            f"({memory_limit_bytes / (1024**3):.2f} GB)"
        )
    else:
        memory_limit_bytes = None

    for exp in build_experiments(Path(args.outdir)):
        print(f"=== {exp.name} ===", flush=True)
        run_experiment_all_inputs(
            exp=exp,
            exe=exe,
            rep=args.rep,
            timeout_s=args.timeout,
            warmup=args.warmup,
            memory_limit_bytes=memory_limit_bytes,
            append=args.append,
        )

    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
