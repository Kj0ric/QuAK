#!/usr/bin/env python3
"""
csv_to_latex_figures.py

Read benchmark CSVs and write compact LaTeX figures with multiple tables
arranged side-by-side using nested tabulars and adjustbox.

Status mapping:
  ERR/ERROR  -> \\oom  (out of memory, 30GB)
  TIMEOUT    -> \\oot  (out of time, 300s)
  OK         -> formatted mean_s (compact: .00, .01, 2.6, 154, etc.)

Outputs two figures:
  1. Response time benchmarks (3 tables side-by-side)
  2. Resource consumption benchmarks (2 tables side-by-side)
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple

import pandas as pd


def fmt_compact(x: float) -> str:
    """
    Compact number formatting:
      - < 0.005: .00
      - < 1: .XX (2 decimal places, no leading zero)
      - < 10: X.X (1 decimal place)
      - >= 10: integer
    """
    if x is None:
        return ""
    x = float(x)
    if math.isnan(x) or math.isinf(x):
        return ""
    
    if x < 0.005:
        return ".00"
    elif x < 1:
        # Format as .XX with 2 decimal places
        return f"{x:.2f}"[1:]  # strip leading 0, keep ".XX"
    elif x < 10:
        return f"{x:.1f}"
    elif x < 1000:
        return f"{x:.0f}"
    else:
        return f"{x:.0f}"


def status_to_cell(status: str, mean_s: Optional[float]) -> str:
    st = (status or "").strip().upper()
    if st == "OK":
        return fmt_compact(mean_s)
    if st in {"ERR", "ERROR"}:
        return r"\oom"
    if st in {"TIMEOUT", "OOT"}:
        return r"\oot"
    if st == "":
        return ""
    # Fallback
    return r"\texttt{" + st.lower() + "}"


def load_cells(csv_path: Path) -> Dict[Tuple[int, int], str]:
    df = pd.read_csv(csv_path)
    
    required = {"n", "k", "status"}
    missing = sorted(required - set(df.columns))
    if missing:
        raise ValueError(f"{csv_path}: missing required columns: {missing}")
    
    if "mean_s" not in df.columns:
        raise ValueError(f"{csv_path}: missing column 'mean_s'")
    
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
    subtitle: str  # e.g., "Emp., $\mathrm{Sup}/\mathrm{SumPlus}$"
    n_values: List[int]
    k_values: List[int]
    mask: Optional[Callable[[int, int], bool]] = None


@dataclass(frozen=True)
class FigureSpec:
    tables: List[TableSpec]
    caption: str
    label: str


def render_inner_table(
    spec: TableSpec,
    cells: Dict[Tuple[int, int], str],
    table_label: str,  # e.g., "(a)"
) -> List[str]:
    """Render a single inner tabular (no outer table environment)."""
    k_vals = spec.k_values
    n_vals = spec.n_values
    
    num_cols = 1 + len(k_vals)
    
    # lines: List[str] = []
    # lines.append(r"\begin{tabular}[t]{@{}r*{" + str(len(k_vals)) + r"}{c}@{}}")
    # lines.append(r"\multicolumn{" + str(num_cols) + r"}{c}{" + table_label + " " + spec.subtitle + r"}\\[2pt]")
    # lines.append(r"\toprule")
    lines: List[str] = []
    lines.append(r"\begin{tabular}[t]{@{}r*{" + str(len(k_vals)) + r"}{c}@{}}")
    title_str = table_label + " " + spec.subtitle
    lines.append(r"\multicolumn{" + str(num_cols) + r"}{c}{\makebox[0pt]{" + title_str + r"}}\\[2pt]")
    lines.append(r"\toprule")
    
    # Header row
    header_cells = [r"$n\backslash k$"] + [str(k) for k in k_vals]
    lines.append(" & ".join(header_cells) + r" \\")
    lines.append(r"\midrule")
    
    # Data rows
    for n in n_vals:
        row = [str(n)]
        for k in k_vals:
            if spec.mask is not None and not spec.mask(n, k):
                row.append("--")
                continue
            row.append(cells.get((n, k), ""))
        lines.append(" & ".join(row) + r" \\")
    
    lines.append(r"\bottomrule")
    lines.append(r"\end{tabular}")
    
    return lines


def render_figure(spec: FigureSpec, all_cells: Dict[Path, Dict[Tuple[int, int], str]]) -> str:
    """Render a complete figure with multiple tables side-by-side."""
    num_tables = len(spec.tables)
    
    lines: List[str] = []
    lines.append(r"\begin{figure}[t]")
    lines.append(r"\centering")
    lines.append(r"\begin{adjustbox}{max width=\linewidth}")
    lines.append(r"\scriptsize")
    lines.append(r"\setlength{\tabcolsep}{2pt}")
    lines.append(r"\renewcommand{\arraystretch}{1.05}")
    
    # Outer tabular to arrange tables side-by-side
    col_sep = r"@{\qquad}"
    col_spec = col_sep.join(["c"] * num_tables)
    lines.append(r"\begin{tabular}{@{}" + col_spec + r"@{}}")
    
    # Render each inner table
    table_labels = ["(a)", "(b)", "(c)", "(d)", "(e)"][:num_tables]
    inner_tables = []
    
    for i, tspec in enumerate(spec.tables):
        cells = all_cells[tspec.csv_path]
        inner_lines = render_inner_table(tspec, cells, table_labels[i])
        inner_tables.append("\n".join(inner_lines))
    
    lines.append("\n&\n".join(inner_tables))
    
    lines.append(r"\end{tabular}")
    lines.append(r"\end{adjustbox}")
    lines.append(r"\caption{" + spec.caption + r"}")
    lines.append(r"\label{" + spec.label + r"}")
    lines.append(r"\end{figure}")
    lines.append("")
    
    return "\n".join(lines)


def pow2_list(start: int, stop: int) -> List[int]:
    out = []
    x = start
    while x <= stop:
        out.append(x)
        x *= 2
    return out


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Generate compact LaTeX figures from benchmark CSVs"
    )
    ap.add_argument("--out", type=Path, default=None, 
                    help="Output .tex file (default: stdout)")
    ap.add_argument("--preamble", action="store_true",
                    help="Include document preamble (for standalone compilation)")
    args = ap.parse_args()

    # -------------------- CONFIGURATION --------------------
    
    # Mask for k >= n (response time tables)
    MASK_K_GE_N = lambda n, k: (k >= n)
    
    # Table specifications
    response_tables = [
        TableSpec(
            csv_path=Path("response_sup_sumplus_emptiness.csv"),
            subtitle=r"$(\Sup,\SumPlus)$ emptiness",
            n_values=pow2_list(4, 512),
            k_values=pow2_list(4, 512),
            mask=MASK_K_GE_N,
        ),
        TableSpec(
            csv_path=Path("response_limsupavg_sumplus_emptiness.csv"),
            subtitle=r"$(\LimSupAvg,\SumPlus)$ emptiness",
            n_values=list(range(1, 9)),
            k_values=list(range(1, 9)),
            mask=MASK_K_GE_N,
        ),
        TableSpec(
            csv_path=Path("response_sup_sumb_universality.csv"),
            subtitle=r"$(\Sup,\SumBound)$ universality",
            n_values=list(range(1, 6)),
            k_values=list(range(1, 6)),
            mask=MASK_K_GE_N,
        ),
    ]
    
    resource_tables = [
        TableSpec(
            csv_path=Path("resource_sup_max_emptiness.csv"),
            subtitle=r"$(\Sup,\Max)$ emptiness",
            n_values=list(range(1, 7)),
            k_values=list(range(1, 7)),
            mask=None,
        ),
        TableSpec(
            csv_path=Path("resource_limsupavg_max_emptiness.csv"),
            subtitle=r"$(\LimSupAvg,\Max)$ emptiness",
            n_values=list(range(1, 5)),
            k_values=list(range(1, 5)),
            mask=None,
        ),
    ]
    
    figure_specs = [
        FigureSpec(
            tables=response_tables,
            caption=r"Response time benchmarks: runtime in seconds. "
                    r"\oom~= out of memory (30\,GB), \oot~= timeout (300\,s), -- = $n > k$.",
            label="fig:response_time_benchmarks",
        ),
        FigureSpec(
            tables=resource_tables,
            caption=r"Resource consumption benchmarks: runtime in seconds. "
                    r"\oom~= out of memory (30\,GB), \oot~= timeout (300\,s).",
            label="fig:resource_consumption_benchmarks",
        ),
    ]
    
    # -------------------- LOAD DATA --------------------
    
    all_csv_paths = set()
    for fspec in figure_specs:
        for tspec in fspec.tables:
            all_csv_paths.add(tspec.csv_path)
    
    all_cells: Dict[Path, Dict[Tuple[int, int], str]] = {}
    for csv_path in all_csv_paths:
        if not csv_path.exists():
            raise FileNotFoundError(
                f"CSV not found: {csv_path}\n"
                f"Tip: run this script in the directory containing the CSVs."
            )
        all_cells[csv_path] = load_cells(csv_path)
    
    # -------------------- RENDER --------------------
    
    out_chunks: List[str] = []
    
    if args.preamble:
        out_chunks.append(r"""\documentclass{article}
\usepackage{booktabs}
\usepackage{amsmath}
\usepackage{adjustbox}

% Shorthand for oom/oot
\newcommand{\oom}{\texttt{m}}
\newcommand{\oot}{\texttt{t}}

\begin{document}
""")
    
    for fspec in figure_specs:
        out_chunks.append(render_figure(fspec, all_cells))
    
    if args.preamble:
        out_chunks.append(r"\end{document}")
    
    latex = "\n".join(out_chunks)
    
    if args.out is None:
        sys.stdout.write(latex)
    else:
        args.out.write_text(latex, encoding="utf-8")
        print(f"Wrote {args.out}", file=sys.stderr)
    
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
