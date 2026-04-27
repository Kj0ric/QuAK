#!/usr/bin/env python3
import argparse
import csv
import signal
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, List, Optional, Set, Tuple

# Try to import resource (Unix-only). If missing, memory limits won't work.
try:
    import resource
except ImportError:
    resource = None


def parse_n_k_from_filename(name: str) -> Optional[Tuple[int, int]]:
    if not name.endswith(".txt"):
        return None
    pos_n = name.find("_n")
    if pos_n < 0:
        return None
    pos_k = name.find("_k", pos_n + 2)
    if pos_k < 0:
        return None
    n_str = name[pos_n + 2 : pos_k]
    k_str = name[pos_k + 2 : -4]
    if not n_str.isdigit() or not k_str.isdigit():
        return None
    n = int(n_str)
    k = int(k_str)
    if n <= 0 or k <= 0:
        return None
    return (n, k)


def collect_by_nk(directory: Path) -> Dict[int, Dict[int, Path]]:
    grid: Dict[int, Dict[int, Path]] = {}
    if not directory.exists() or not directory.is_dir():
        return grid
    for ent in directory.iterdir():
        if not ent.is_file():
            continue
        nk = parse_n_k_from_filename(ent.name)
        if nk is None:
            continue
        n, k = nk
        grid.setdefault(n, {})[k] = ent
    return grid


@dataclass(frozen=True)
class Experiment:
    name: str
    input_dir: Path
    problem: str
    infval: str
    finval: str
    threshold_mode: str   # "k" or "const"
    threshold_const: float
    out_csv: Path
    # -- Abort Flags --
    abort_k_on_timeout: bool = True          # If True, stop increasing k for the current n on TIMEOUT/OOM
    abort_n_on_timeout_at_kn: bool = False   # If True, and we fail at k=n, stop increasing n
    abort_n_on_timeout_at_k1: bool = False   # If True, and we fail at k=1, stop increasing n


def ensure_csv_header(csv_path: Path, header: List[str]) -> None:
    if csv_path.exists():
        return
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        csv.writer(f).writerow(header)


def load_done_pairs(csv_path: Path) -> Set[Tuple[int, int]]:
    done: Set[Tuple[int, int]] = set()
    if not csv_path.exists():
        return done
    try:
        with csv_path.open("r", newline="", encoding="utf-8") as f:
            r = csv.DictReader(f)
            if not r.fieldnames or "n" not in r.fieldnames or "k" not in r.fieldnames:
                return done
            for row in r:
                try:
                    done.add((int(row["n"]), int(row["k"])))
                except Exception:
                    pass
    except Exception:
        pass
    return done


def parse_bench_output(stdout: str) -> Optional[Tuple[float, int, str]]:
    # Expected format: MEAN_S=0.123 RESULT=1 STATUS=OK
    kv = {}
    for tok in stdout.strip().split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            kv[k.strip()] = v.strip()
    if "STATUS" not in kv:
        return None
    status = kv["STATUS"]
    mean_s = float(kv.get("MEAN_S", "0"))
    result01 = int(kv.get("RESULT", "0"))
    return (mean_s, result01, status)


def make_memory_limit_preexec(limit_bytes: int) -> Callable[[], None]:
    """Return a preexec_fn that sets RLIMIT_AS to limit_bytes."""
    def set_limit() -> None:
        if resource:
            resource.setrlimit(resource.RLIMIT_AS, (limit_bytes, limit_bytes))
    return set_limit


_MEM_HINTS = (
    "std::bad_alloc",
    "bad_alloc",
    "cannot allocate memory",
    "cannot mmap",
    "out of memory",
    "enomem",
)

def _looks_like_oom(rc: int, out: str, err: str) -> bool:
    """Check explicit error messages or signal codes."""
    blob = (out + "\n" + err).lower()
    if any(h in blob for h in _MEM_HINTS):
        return True

    # Check for signal kills (negative RC in python subprocess often means signal)
    if rc < 0:
        sig = -rc
        if sig in (signal.SIGKILL, signal.SIGABRT, signal.SIGSEGV, signal.SIGBUS):
            return True
    return False

def _likely_memlimit_hit(status: str, rc: int, out: str, err: str) -> bool:
    """
    Heuristic to determine if a failure was caused by RLIMIT_AS.
    If the process exits with error (rc!=0) and produces NO stderr,
    it is highly likely the allocator failed and the process died silently.
    """
    if _looks_like_oom(rc, out, err):
        return True

    # If parsing failed or returned ERR, and rc is non-zero
    if rc != 0:
        # If stderr is empty, we assume OOM (std::bad_alloc often creates this signature with RLIMIT)
        if not err.strip():
            return True
        
        # Also check stdout if stderr was empty? (Sometimes output buffers are lost)
        pass

    # Check for "cannot allocate" in lower case output just in case
    blob = (out + "\n" + err).lower()
    if "cannot allocate memory" in blob or "failed to map" in blob:
        return True

    return False


def run_cpp_once(
    exe: Path,
    file_path: Path,
    problem: str,
    infval: str,
    finval: str,
    threshold: float,
    rep: int,
    timeout_s: float,
    warmup: int,
    memory_limit_bytes: Optional[int],
) -> Tuple[str, float, int, str, str, int]:
    cmd = [
        str(exe),
        str(file_path),
        problem,
        infval,
        finval,
        str(threshold),
        "--rep",
        str(rep),
        "--timeout-s",
        str(timeout_s),
        "--warmup",
        str(warmup),
    ]

    preexec_fn = None
    if memory_limit_bytes is not None:
        preexec_fn = make_memory_limit_preexec(memory_limit_bytes)

    try:
        p = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
            preexec_fn=preexec_fn,
        )
        out = p.stdout
        err = p.stderr
        rc = p.returncode
    except Exception as e:
        return ("ERR", 0.0, 0, "", str(e), -1)

    parsed = parse_bench_output(out)
    
    # 1. Parsing Failed (Empty or garbage output)
    if parsed is None:
        # Check OOM heuristics
        if memory_limit_bytes is not None and _likely_memlimit_hit("PARSEERR", rc, out, err):
            return ("OOM", 0.0, 0, out, err, rc)
        
        if rc < 0:
            return ("KILLED", 0.0, 0, out, err, rc)
        
        # If rc is positive (e.g. 2) and parsing failed, it's a generic error
        return ("PARSEERR", 0.0, 0, out, err, rc)

    # 2. Parsing Succeeded (we have a STATUS like OK, TIMEOUT, ERR, etc.)
    mean_s, result01, status = parsed

    # Check if a parsed "ERR" or "PARSEERR" is actually an OOM in disguise
    if memory_limit_bytes is not None and status not in ("OK", "TIMEOUT", "OOM"):
        if _likely_memlimit_hit(status, rc, out, err):
            return ("OOM", 0.0, 0, out, err, rc)

    return (status, mean_s, result01, out, err, rc)


def run_experiment(
    exp: Experiment,
    exe: Path,
    rep: int,
    timeout_s: float,
    warmup: int,
    memory_limit_bytes: Optional[int],
) -> None:
    header = [
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
    ensure_csv_header(exp.out_csv, header)
    done = load_done_pairs(exp.out_csv)

    grid = collect_by_nk(exp.input_dir)
    if not grid:
        print(f"[{exp.name}] WARNING: no inputs found in {exp.input_dir}", file=sys.stderr)
        return

    ns = sorted(grid.keys())

    with exp.out_csv.open("a", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        abort_rest_n = False

        for n in ns:
            if abort_rest_n:
                break
            abort_rest_k = False
            for k in sorted(grid[n].keys()):
                if abort_rest_k:
                    break
                if (n, k) in done:
                    continue

                file_path = grid[n][k]
                threshold = float(k) if exp.threshold_mode == "k" else float(exp.threshold_const)

                prefix = f"[{exp.name}] n={n} k={k} -> running..."
                print(prefix, end="", flush=True)

                status, mean_s, result01, out, err, rc = run_cpp_once(
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

                # -- Print Outcome --
                if status == "OK":
                    print(f" OK  mean_s={mean_s:.9f}  result={result01}", flush=True)
                elif status == "OOM":
                    act = "skipping remaining k" if exp.abort_k_on_timeout else "continuing"
                    print(f" OOM  rc={rc}  ({act})", flush=True)
                elif status == "TIMEOUT":
                    act = "skipping remaining k" if exp.abort_k_on_timeout else "continuing"
                    print(f" TIMEOUT  rc={rc}  ({act})", flush=True)
                elif status == "KILLED":
                    act = "skipping remaining k" if exp.abort_k_on_timeout else "continuing"
                    print(f" KILLED  rc={rc}  ({act})", flush=True)
                else:
                    # Generic error (ERR, PARSEERR)
                    err_clean = err.strip().replace("\n", " || ")
                    print(f" {status}  rc={rc}  stderr='{err_clean[:50]}...'", flush=True)

                # -- Abort / Skip Logic --
                
                # Check for OOM/TIMEOUT/KILLED events
                if status in ("TIMEOUT", "OOM", "KILLED"):
                    # 1. Skip remaining k for this n?
                    if exp.abort_k_on_timeout:
                        abort_rest_k = True
                    
                    # 2. Skip remaining n? (Escalation)
                    if exp.abort_n_on_timeout_at_kn and k == n:
                        print(f"   -> Aborting remaining n (hit limit at k=n)", flush=True)
                        abort_rest_n = True
                    elif exp.abort_n_on_timeout_at_k1 and k == 1:
                        print(f"   -> Aborting remaining n (hit limit at k=1)", flush=True)
                        abort_rest_n = True
                
                # Optional: If you also want to skip remaining k on generic ERR (e.g. file not found),
                # uncomment the lines below:
                # if status in ("ERR", "PARSEERR") and exp.abort_k_on_timeout:
                #     abort_rest_k = True

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
                    status,
                    (f"{mean_s:.9f}" if status == "OK" else ""),
                    (result01 if status in ("OK", "INCONSISTENT") else ""),
                ])
                f.flush()
                done.add((n, k))


def parse_memory_limit(s: str) -> int:
    """Parse memory limit string like '8G', '8192M', '8589934592' into bytes."""
    s = s.strip().upper()
    if s.endswith("G"):
        return int(float(s[:-1]) * 1024 * 1024 * 1024)
    elif s.endswith("M"):
        return int(float(s[:-1]) * 1024 * 1024)
    elif s.endswith("K"):
        return int(float(s[:-1]) * 1024)
    else:
        return int(s)


def main() -> int:
    ap = argparse.ArgumentParser(description="Run QuAK experiments.")
    ap.add_argument("--exe", required=True, help="Path to compiled quak-bench executable.")
    ap.add_argument("--rep", type=int, default=3, help="How many repetitions C++ should run per instance.")
    ap.add_argument("--timeout", type=float, default=300.0, help="Per-repetition timeout in seconds.")
    ap.add_argument("--warmup", type=int, default=1, help="Warmup inside C++ (0 or 1).")
    ap.add_argument("--memory-limit", type=str, default=None,
                    help="Memory limit per process (e.g., '8G', '4096M'). Uses RLIMIT_AS.")
    ap.add_argument("--outdir", default="results", help="Directory to write CSVs.")
    args = ap.parse_args()

    exe = Path(args.exe).expanduser()
    if not exe.is_absolute():
        exe = (Path.cwd() / exe).resolve()
    else:
        exe = exe.resolve()

    if not exe.exists():
        print(f"ERROR: --exe not found: {exe}", file=sys.stderr)
        return 2

    if resource is None and args.memory_limit:
        print("WARNING: 'resource' module not available (Windows?). Memory limits will be ignored.", file=sys.stderr)
        memory_limit_bytes = None
    elif args.memory_limit:
        memory_limit_bytes = parse_memory_limit(args.memory_limit)
        print(f"Memory limit: {memory_limit_bytes} bytes ({memory_limit_bytes / (1024**3):.2f} GB)")
    else:
        memory_limit_bytes = None

    outdir = Path(args.outdir)
    response_dir_1 = Path("samples/generated_response_time_1")
    response_dir_2 = Path("samples/generated_response_time_2")
    response_dir_3 = Path("samples/generated_response_time_3")
    resource_dir_1 = Path("samples/generated_resource_consumption_1")
    resource_dir_2 = Path("samples/generated_resource_consumption_2")

    experiments: List[Experiment] = [
        # RESPONSE TIME
        Experiment(
            name="response_sup_sumplus_emptiness",
            input_dir=response_dir_1,
            problem="emptiness",
            infval="Sup",
            finval="SumPlus",
            threshold_mode="k",
            threshold_const=0.0,
            out_csv=outdir / "response_sup_sumplus_emptiness.csv",
            abort_n_on_timeout_at_kn=True,
            abort_n_on_timeout_at_k1=False,
        ),
        # Experiment(
        #     name="response_sup_sumb_emptiness",
        #     input_dir=response_dir_1,
        #     problem="emptiness",
        #     infval="Sup",
        #     finval="SumB:auto",
        #     threshold_mode="k",
        #     threshold_const=0.0,
        #     out_csv=outdir / "response_sup_sumb_emptiness.csv",
        #     abort_n_on_timeout_at_kn=True,
        #     abort_n_on_timeout_at_k1=False,
        # ),
        Experiment(
            name="response_limsupavg_sumplus_emptiness",
            input_dir=response_dir_2,
            problem="emptiness",
            infval="LimSupAvg",
            finval="SumPlus",
            threshold_mode="k",
            threshold_const=0.0,
            out_csv=outdir / "response_limsupavg_sumplus_emptiness.csv",
            abort_n_on_timeout_at_kn=True,
            abort_n_on_timeout_at_k1=False,
        ),
        Experiment(
            name="response_sup_sumb_universality",
            input_dir=response_dir_3,
            problem="universality",
            infval="Sup",
            finval="SumB:auto",
            threshold_mode="const",
            threshold_const=1.0,
            out_csv=outdir / "response_sup_sumb_universality.csv",
            abort_n_on_timeout_at_kn=False,
            abort_n_on_timeout_at_k1=False,
        ),
        # Experiment(
        #     name="response_limsupavg_summinus_emptiness",
        #     input_dir=response_dir_4,
        #     problem="emptiness",
        #     infval="LimSupAvg",
        #     finval="SumMinus",
        #     threshold_mode="const",
        #     threshold_const=-1.0,
        #     out_csv=outdir / "response_limsupavg_summinus_emptiness.csv",
        #     abort_n_on_timeout_at_kn=True,
        #     abort_n_on_timeout_at_k1=False,
        # ),
        # RESOURCE CONSUMPTION
        Experiment(
            name="resource_sup_max_emptiness",
            input_dir=resource_dir_1,
            problem="emptiness",
            infval="Sup",
            finval="Max",
            threshold_mode="k",
            threshold_const=0.0,
            out_csv=outdir / "resource_sup_max_emptiness.csv",
            abort_k_on_timeout=False,
            abort_n_on_timeout_at_kn=False,
            abort_n_on_timeout_at_k1=False,
        ),
        # Experiment(
        #     name="resource_sup_sumplus_emptiness",
        #     input_dir=resource_dir_1,
        #     problem="emptiness",
        #     infval="Sup",
        #     finval="SumPlus",
        #     threshold_mode="k",
        #     threshold_const=0.0,
        #     out_csv=outdir / "resource_sup_sumplus_emptiness.csv",
        #     abort_n_on_timeout_at_kn=False,
        #     abort_n_on_timeout_at_k1=True,
        # ),
        Experiment(
            name="resource_limsupavg_max_emptiness",
            input_dir=resource_dir_2,
            problem="emptiness",
            infval="LimSupAvg",
            finval="Max",
            threshold_mode="k",
            threshold_const=0.0,
            out_csv=outdir / "resource_limsupavg_max_emptiness.csv",
            abort_k_on_timeout=False,
            abort_n_on_timeout_at_kn=False,
            abort_n_on_timeout_at_k1=False,
        ),
    ]

    for exp in experiments:
        print(f"=== {exp.name} ===")
        run_experiment(
            exp,
            exe,
            rep=args.rep,
            timeout_s=args.timeout,
            warmup=args.warmup,
            memory_limit_bytes=memory_limit_bytes,
        )

    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
