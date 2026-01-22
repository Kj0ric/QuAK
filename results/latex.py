#!/usr/bin/env python3
"""
csv_to_latex_tables.py

Read benchmark CSVs and write compact LaTeX tables of mean runtime (mean_s),
with status mapping:
  ERR      -> \texttt{oom}  (out of memory, 30GB)
  TIMEOUT  -> \texttt{oot}  (out of time, 300s)
  OK       -> formatted mean_s

Tables are generated in "matrix" form:
  header: n\k, then k-values
  rows: each n-value
  cell: value for (n,k) if present, otherwise blank

Customize the N_VALUES / K_VALUES lists in TABLE_SPECS below.
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, Iterable, List, Optional, Tuple

import pandas as pd


def latex_escape(s: str) -> str:
    # Minimal escaping for captions/labels.
    repl = {
        "\\": r"\textbackslash{}",
        "&": r"\&",
        "%": r"\%",
        "$": r"\$",
        "#": r"\#",
        "_": r"\_",
        "{": r"\{",
        "}": r"\}",
        "~": r"\textasciitilde{}",
        "^": r"\textasciicircum{}",
    }
    return "".join(repl.get(ch, ch) for ch in s)


# def fmt_seconds(x: float, sig: int = 5) -> str:
#     # Compact, stable formatting. Avoid NaN/inf.
#     if x is None or (isinstance(x, float) and (math.isnan(x) or math.isinf(x))):
#         return ""
#     return format(float(x), f".{sig}g")
def fmt_seconds(x: float) -> str:
    # Always exactly 3 digits after the decimal point.
    if x is None:
        return ""
    x = float(x)
    if math.isnan(x) or math.isinf(x):
        return ""
    return f"{x:.3f}"


# def status_to_cell(status: str, mean_s: Optional[float], sig: int) -> str:
#     st = (status or "").strip().upper()
#     if st == "OK":
#         return fmt_seconds(mean_s, sig=sig)
#     if st in {"ERR", "ERROR"}:
#         return r"\texttt{oom}"
#     if st in {"TIMEOUT", "OOT"}:
#         return r"\texttt{oot}"
#     # Fallback: show raw status in monospace (lowercased) to catch surprises.
#     if st == "":
#         return ""
#     return r"\texttt{" + latex_escape(st.lower()) + "}"
def status_to_cell(status: str, mean_s: Optional[float]) -> str:
    st = (status or "").strip().upper()
    if st == "OK":
        return fmt_seconds(mean_s)
    if st in {"ERR", "ERROR"}:
        return r"\texttt{oom}"
    if st in {"TIMEOUT", "OOT"}:
        return r"\texttt{oot}"
    if st == "":
        return ""
    return r"\texttt{" + latex_escape(st.lower()) + "}"


# def load_cells(csv_path: Path, sig: int) -> Dict[Tuple[int, int], str]:
#     df = pd.read_csv(csv_path)

#     required = {"n", "k", "status"}
#     missing = sorted(required - set(df.columns))
#     if missing:
#         raise ValueError(f"{csv_path}: missing required columns: {missing}")

#     has_mean = "mean_s" in df.columns
#     if not has_mean:
#         raise ValueError(f"{csv_path}: missing column 'mean_s' (expected pre-aggregated runtimes)")

#     cells: Dict[Tuple[int, int], str] = {}
#     for row in df.itertuples(index=False):
#         n = int(getattr(row, "n"))
#         k = int(getattr(row, "k"))
#         status = str(getattr(row, "status"))
#         mean_s = getattr(row, "mean_s")
#         cell = status_to_cell(status, mean_s, sig=sig)
#         cells[(n, k)] = cell

#     return cells
def load_cells(csv_path: Path) -> Dict[Tuple[int, int], str]:
    df = pd.read_csv(csv_path)
    # ... same checks ...
    cells: Dict[Tuple[int, int], str] = {}
    for row in df.itertuples(index=False):
        n = int(getattr(row, "n"))
        k = int(getattr(row, "k"))
        status = str(getattr(row, "status"))
        mean_s = getattr(row, "mean_s")
        cells[(n, k)] = status_to_cell(status, mean_s)
    return cells


@dataclass(frozen=True)
class TableSpec:
    csv_path: Path
    caption: str
    label: str
    n_values: List[int]
    k_values: List[int]
    mask: Optional[Callable[[int, int], bool]] = None  # if provided, False -> blank cell


def render_table(spec: TableSpec, cells: Dict[Tuple[int, int], str]) -> str:
    k_vals = spec.k_values
    n_vals = spec.n_values

    # Column format: first col right-aligned, then centered k-columns.
    colfmt = "r" + ("c" * len(k_vals))

    lines: List[str] = []
    lines.append(r"\begin{table}[t]")
    lines.append(r"\centering")
    lines.append(r"\scriptsize")
    lines.append(r"\setlength{\tabcolsep}{3pt}")
    lines.append(r"\renewcommand{\arraystretch}{1.05}")
    lines.append(r"\caption{" + spec.caption + r"}")
    lines.append(r"\label{" + spec.label + r"}")
    lines.append(r"\resizebox{\linewidth}{!}{%")
    lines.append(r"\begin{tabular}{" + colfmt + r"}")
    lines.append(r"\toprule")

    # Header row.
    header_cells = [r"$n\backslash k$"] + [str(k) for k in k_vals]
    lines.append(" " + " & ".join(header_cells) + r" \\")
    lines.append(r"\midrule")

    # Data rows.
    for n in n_vals:
        row = [str(n)]
        for k in k_vals:
            if spec.mask is not None and not spec.mask(n, k):
                row.append("")  # masked out (e.g., k < n)
                continue
            row.append(cells.get((n, k), ""))
        lines.append(" " + " & ".join(row) + r" \\")

    lines.append(r"\bottomrule")
    lines.append(r"\end{tabular}}")
    lines.append(r"\end{table}")
    lines.append("")  # blank line between tables
    return "\n".join(lines)


def pow2_list(start: int, stop: int) -> List[int]:
    # inclusive stop if it's a power of two; otherwise stops before exceeding stop
    out = []
    x = start
    while x <= stop:
        out.append(x)
        x *= 2
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=None, help="Output .tex file (default: stdout)")
    # ap.add_argument("--sig", type=int, default=5, help="Significant digits for mean_s formatting")
    args = ap.parse_args()

    # N1 = pow2_list(4, 256)
    # K1 = pow2_list(4, 512)

    # N2 = list(range(1, 6))
    # K2 = list(range(1, 11))

    # N3 = list(range(1, 6))
    # K3 = list(range(1, 7))

    # N4 = list(range(1, 8))
    # K4 = list(range(1, 11))

    # N5 = list(range(1, 4))
    # K5 = list(range(1, 11))

    N1 = pow2_list(4, 256)
    K1 = pow2_list(4, 512)

    N2 = list(range(1, 6))
    K2 = list(range(1, 7))

    N3 = list(range(1, 5))
    K3 = list(range(1, 6))

    N4 = list(range(1, 8))
    K4 = list(range(1, 8))

    N5 = list(range(1, 4))
    K5 = list(range(1, 8))


    # Mask used for your “leave k<n blank” layout.
    MASK_K_GE_N = lambda n, k: (k >= n)

    # -------------------- TABLE SPECS (5 CSVs) --------------------
    # Adjust captions/labels, and especially n_values/k_values to match what you want to show.
    table_specs: List[TableSpec] = [
        TableSpec(
            csv_path=Path("response_sup_sumplus_emptiness.csv"),
            caption=r"Response time: emptiness for $\mathrm{Sup}/\mathrm{SumPlus}$. "
                    r"Entries are mean runtime in seconds; \texttt{oom}=out of memory (30GB), "
                    r"\texttt{oot}=out of time (300s). Only powers of two are shown.",
            label="tab:response_sup_sumplus_emptiness_pow2",
            n_values=N1,
            k_values=K1,
            mask=MASK_K_GE_N,
        ),
        TableSpec(
            csv_path=Path("response_limsupavg_sumplus_emptiness.csv"),
            caption=r"Response time: emptiness for $\mathrm{LimSupAvg}/\mathrm{SumPlus}$. "
                    r"Entries are mean runtime in seconds; \texttt{oom}=out of memory (30GB), "
                    r"\texttt{oot}=out of time (300s).",
            label="tab:response_limsupavg_sumplus_emptiness",
            n_values=N2,
            k_values=K2,
            mask=MASK_K_GE_N,
        ),
        TableSpec(
            csv_path=Path("response_sup_sumb_universality.csv"),
            caption=r"Response time: universality for $\mathrm{Sup}/\mathrm{SumB}$. "
                    r"Entries are mean runtime in seconds; \texttt{oom}=out of memory (30GB), "
                    r"\texttt{oot}=out of time (300s).",
            label="tab:response_sup_sumb_universality",
            n_values=N3,
            k_values=K3,
            mask=MASK_K_GE_N,
        ),
        TableSpec(
            csv_path=Path("resource_sup_max_emptiness.csv"),
            caption=r"Resource consumption: emptiness for $\mathrm{Sup}/\mathrm{Max}$. "
                    r"Entries are mean runtime in seconds; \texttt{oom}=out of memory (30GB), "
                    r"\texttt{oot}=out of time (300s).",
            label="tab:resource_sup_max_emptiness",
            n_values=N4,
            k_values=K4,
            mask=None,
        ),
        TableSpec(
            csv_path=Path("resource_limsupavg_max_emptiness.csv"),
            caption=r"Resource consumption: emptiness for $\mathrm{LimSupAvg}/\mathrm{Max}$. "
                    r"Entries are mean runtime in seconds; \texttt{oom}=out of memory (30GB), "
                    r"\texttt{oot}=out of time (300s).",
            label="tab:resource_limsupavg_max_emptiness",
            n_values=N5,
            k_values=K5,
            mask=None,
        ),
    ]

    # Resolve paths relative to this script's working directory.
    out_chunks: List[str] = []
    for spec in table_specs:
        if not spec.csv_path.exists():
            raise FileNotFoundError(
                f"CSV not found: {spec.csv_path}\n"
                f"Tip: run this script in the directory containing the CSVs, or edit csv_path."
            )
        # cells = load_cells(spec.csv_path, sig=args.sig)
        cells = load_cells(spec.csv_path)
        out_chunks.append(render_table(spec, cells))

    latex = "\n".join(out_chunks)

    if args.out is None:
        sys.stdout.write(latex)
    else:
        args.out.write_text(latex, encoding="utf-8")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
