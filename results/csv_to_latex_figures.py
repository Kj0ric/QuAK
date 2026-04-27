#!/usr/bin/env python3
"""
csv_to_latex_figures.py

Read benchmark CSVs and write a compact standalone LaTeX file with multiple
tables arranged side-by-side using nested tabulars and adjustbox.

Status mapping:
  OOM/ERR/ERROR  -> \\oom  (out of memory, 30GB)
  TIMEOUT    -> \\oot  (out of time, 300s)
  OK         -> formatted mean_s (compact: .00, .01, 2.6, 154, etc.)

Default output:
  - benchmark_tables.tex in the input directory
  - benchmark_tables.pdf compiled automatically

Outputs two table groups:
  1. Response time benchmarks (3 tables side-by-side)
  2. Resource consumption benchmarks (2 tables side-by-side)
"""

from __future__ import annotations

import argparse
import math
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple

import pandas as pd


LATEX_INDENT = "\t"


def latex_line(level: int, text: str) -> str:
    return (LATEX_INDENT * level) + text


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
    if st in {"OOM", "ERR", "ERROR"}:
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
class TableGroupSpec:
    tables: List[TableSpec]
    caption: str
    label: str
    before_caption: str = ""
    inner_separator: str = "\n&\n"


def render_inner_table(
    spec: TableSpec,
    cells: Dict[Tuple[int, int], str],
    table_label: str,  # e.g., "(a)"
) -> List[str]:
    """Render a single inner tabular (no outer table environment)."""
    k_vals = spec.k_values
    n_vals = spec.n_values
    
    num_cols = 1 + len(k_vals)
    
    lines: List[str] = []
    lines.append(r"\begin{tabular}[t]{@{}r*{" + str(len(k_vals)) + r"}{c}@{}}")
    title_str = table_label + " " + spec.subtitle
    lines.append(latex_line(1, r"\multicolumn{" + str(num_cols) + r"}{c}{\makebox[0pt]{" + title_str + r"}}\\[2pt]"))
    lines.append(latex_line(1, r"\toprule"))
    
    # Header row
    header_cells = [r"$n\backslash k$"] + [str(k) for k in k_vals]
    lines.append(latex_line(1, " & ".join(header_cells) + r" \\"))
    lines.append(latex_line(1, r"\midrule"))
    
    # Data rows
    for n in n_vals:
        row = [str(n)]
        for k in k_vals:
            if spec.mask is not None and not spec.mask(n, k):
                row.append("--")
                continue
            row.append(cells.get((n, k), ""))
        lines.append(latex_line(1, " & ".join(row) + r" \\"))
    
    lines.append(latex_line(1, r"\bottomrule"))
    lines.append(r"\end{tabular}")
    
    return lines


def render_table_group(spec: TableGroupSpec, all_cells: Dict[Path, Dict[Tuple[int, int], str]]) -> str:
    """Render a complete table group with multiple tables side-by-side."""
    num_tables = len(spec.tables)
    
    lines: List[str] = []
    lines.append(r"\begin{table}[t]")
    lines.append(latex_line(1, r"\centering"))
    lines.append(latex_line(1, r"\begin{adjustbox}{max width=\linewidth}"))
    lines.append(latex_line(2, r"\scriptsize"))
    lines.append(latex_line(2, r"\setlength{\tabcolsep}{2pt}"))
    lines.append(latex_line(2, r"\renewcommand{\arraystretch}{1.05}"))
    
    # Outer tabular to arrange tables side-by-side
    col_sep = r"@{\qquad}"
    col_spec = col_sep.join(["c"] * num_tables)
    lines.append(latex_line(2, r"\begin{tabular}{@{}" + col_spec + r"@{}}"))
    
    # Render each inner table
    table_labels = ["(a)", "(b)", "(c)", "(d)", "(e)"][:num_tables]
    inner_tables = []
    
    for i, tspec in enumerate(spec.tables):
        cells = all_cells[tspec.csv_path]
        inner_lines = render_inner_table(tspec, cells, table_labels[i])
        inner_tables.append("\n".join(inner_lines))
    
    separator = spec.inner_separator.strip()
    for i, inner_table in enumerate(inner_tables):
        if i > 0:
            lines.append(latex_line(3, separator))
        for inner_line in inner_table.splitlines():
            lines.append(latex_line(3, inner_line))
    
    lines.append(latex_line(2, r"\end{tabular}"))
    lines.append(latex_line(1, r"\end{adjustbox}"))
    if spec.before_caption:
        lines.append(latex_line(1, spec.before_caption))
    lines.append(latex_line(1, r"\caption{" + spec.caption + r"}"))
    lines.append(latex_line(1, r"\label{" + spec.label + r"}"))
    lines.append(r"\end{table}")
    lines.append("")
    
    return "\n".join(lines)


def render_document(body: str) -> str:
    """Wrap the rendered table groups in a small standalone document."""
    return "\n".join([
        r"\documentclass[10pt]{article}",
        r"\usepackage[margin=0.55in]{geometry}",
        r"\usepackage{booktabs}",
        r"\usepackage{amsmath}",
        r"\usepackage{adjustbox}",
        r"\usepackage{xspace}",
        r"\pagestyle{empty}",
        "",
        r"\newcommand{\ValFunction}[1]{{\textsf{#1}}\xspace}",
        r"\newcommand{\Val}{\ValFunction{Val}}",
        r"\newcommand{\Min}{\ValFunction{Min}}",
        r"\newcommand{\Max}{\ValFunction{Max}}",
        r"\newcommand{\Sum}{\ValFunction{Sum}}",
        r"\newcommand{\Avg}{\ValFunction{Avg}}",
        r"\newcommand{\Inf}{\ValFunction{Inf}}",
        r"\newcommand{\Sup}{\ValFunction{Sup}}",
        r"\newcommand{\LimInf}{\ValFunction{LimInf}}",
        r"\newcommand{\LimSup}{\ValFunction{LimSup}}",
        r"\newcommand{\LimInfAvg}{\ValFunction{LimInfAvg}}",
        r"\newcommand{\LimSupAvg}{\ValFunction{LimSupAvg}}",
        r"\newcommand{\SumPlus}{\ValFunction{Sum$^+$}}",
        r"\newcommand{\SumMinus}{\ValFunction{Sum$^-$}}",
        r"\newcommand{\SumBound}{\ValFunction{Sum$^B$}}",
        r"\newcommand{\oom}{\texttt{m}}",
        r"\newcommand{\oot}{\texttt{t}}",
        "",
        r"\begin{document}",
        body,
        r"\end{document}",
        "",
    ])


def run_latex_command(cmd: List[str], cwd: Path) -> None:
    result = subprocess.run(
        cmd,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        raise RuntimeError(f"LaTeX command failed: {' '.join(cmd)}")


def compile_pdf(tex_path: Path, compiler: str, keep_aux: bool) -> Path:
    """Compile tex_path to PDF with latexmk when available, otherwise pdflatex."""
    tex_path = tex_path.resolve()
    pdf_path = tex_path.with_suffix(".pdf")

    if compiler == "auto":
        compiler = "latexmk" if shutil.which("latexmk") else "pdflatex"

    exe = shutil.which(compiler)
    if exe is None:
        raise RuntimeError(
            f"Could not find '{compiler}' on PATH. Re-run with --no-compile "
            "or install a LaTeX distribution."
        )

    if compiler == "latexmk":
        run_latex_command(
            [exe, "-pdf", "-interaction=nonstopmode", "-halt-on-error", tex_path.name],
            tex_path.parent,
        )
        if not keep_aux:
            subprocess.run(
                [exe, "-c", tex_path.name],
                cwd=tex_path.parent,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
    elif compiler == "pdflatex":
        cmd = [exe, "-interaction=nonstopmode", "-halt-on-error", tex_path.name]
        run_latex_command(cmd, tex_path.parent)
        run_latex_command(cmd, tex_path.parent)
        if not keep_aux:
            for suffix in [".aux", ".log", ".out", ".fls", ".fdb_latexmk"]:
                tex_path.with_suffix(suffix).unlink(missing_ok=True)
    else:
        raise ValueError(f"unsupported compiler: {compiler}")

    if not pdf_path.exists():
        raise RuntimeError(f"Expected PDF was not created: {pdf_path}")

    return pdf_path


def pow2_list(start: int, stop: int) -> List[int]:
    out = []
    x = start
    while x <= stop:
        out.append(x)
        x *= 2
    return out


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Generate and compile compact LaTeX benchmark tables from a CSV directory"
    )
    ap.add_argument("directory", type=Path,
                    help="Directory containing the benchmark CSV files")
    ap.add_argument("--out", type=Path, default=None, 
                    help="Output .tex file (default: DIRECTORY/benchmark_tables.tex)")
    ap.add_argument("--no-compile", action="store_true",
                    help="Write the .tex file without compiling a PDF")
    ap.add_argument("--compiler", choices=("auto", "latexmk", "pdflatex"), default="auto",
                    help="LaTeX compiler to use (default: auto)")
    ap.add_argument("--keep-aux", action="store_true",
                    help="Keep LaTeX auxiliary files")
    ap.add_argument("--fragment", action="store_true",
                    help="Write only the LaTeX table snippets; skips PDF compilation")
    ap.add_argument("--stdout", action="store_true",
                    help="Write LaTeX to stdout instead of a file; skips PDF compilation")
    ap.add_argument("--preamble", action="store_true",
                    help="Legacy alias for standalone document output, which is now the default")
    args = ap.parse_args()

    input_dir = args.directory
    if not input_dir.is_dir():
        ap.error(f"CSV directory not found: {input_dir}")

    if args.stdout and args.out is not None:
        ap.error("--stdout cannot be combined with --out")

    # -------------------- CONFIGURATION --------------------
    
    # Mask for k >= n (response time tables)
    MASK_K_GE_N = lambda n, k: (k >= n)
    
    # Table specifications
    response_tables = [
        TableSpec(
            csv_path=Path("response_sup_sumplus_emptiness.csv"),
            subtitle=r"(\Sup,\SumPlus) emptiness",
            n_values=pow2_list(4, 512),
            k_values=pow2_list(4, 512),
            mask=MASK_K_GE_N,
        ),
        TableSpec(
            csv_path=Path("response_limsupavg_sumplus_emptiness.csv"),
            subtitle=r"(\LimSupAvg,\SumPlus) emptiness",
            n_values=list(range(3, 11)),
            k_values=list(range(3, 11)),
            mask=MASK_K_GE_N,
        ),
        TableSpec(
            csv_path=Path("response_sup_sumb_universality.csv"),
            subtitle=r"(\Sup,\SumBound) universality",
            n_values=list(range(2, 10)),
            k_values=list(range(2, 10)),
            mask=MASK_K_GE_N,
        ),
    ]
    
    resource_tables = [
        TableSpec(
            csv_path=Path("resource_sup_max_emptiness.csv"),
            subtitle=r"(\Sup,\Max) emptiness",
            n_values=list(range(1, 7)),
            k_values=list(range(1, 7)),
            mask=None,
        ),
        TableSpec(
            csv_path=Path("resource_limsupavg_max_emptiness.csv"),
            subtitle=r"(\LimSupAvg,\Max) emptiness",
            n_values=list(range(1, 5)),
            k_values=list(range(1, 5)),
            mask=None,
        ),
    ]
    
    table_group_specs = [
        TableGroupSpec(
            tables=response_tables,
            caption=r"Response time benchmarks: runtime in seconds, "
                    r"\oom~= out of memory (30\,GB), \oot~= out of time (300\,s). "
                    r"\vspace{-2.5em}",
            label="fig:response_time_benchmarks",
            before_caption=r"\vspace{0.25em}",
        ),
        TableGroupSpec(
            tables=resource_tables,
            caption=r"Resource consumption benchmarks: runtime in seconds, "
                    r"\oom~= out of memory (30\,GB), \oot~= out of time (300\,s). "
                    r"\vspace{-2.5em}",
            label="fig:resource_consumption_benchmarks",
            before_caption=r"\vspace{0.25em}",
            inner_separator="\n&\\hspace{4em}\n",
        ),
    ]
    
    # -------------------- LOAD DATA --------------------
    
    all_csv_paths = set()
    for fspec in table_group_specs:
        for tspec in fspec.tables:
            all_csv_paths.add(tspec.csv_path)
    
    all_cells: Dict[Path, Dict[Tuple[int, int], str]] = {}
    for csv_path in all_csv_paths:
        resolved_path = csv_path if csv_path.is_absolute() else input_dir / csv_path
        if not resolved_path.exists():
            raise FileNotFoundError(
                f"CSV not found: {resolved_path}\n"
                f"Tip: pass the directory containing the benchmark CSVs."
            )
        all_cells[csv_path] = load_cells(resolved_path)
    
    # -------------------- RENDER --------------------

    body = "\n".join(render_table_group(fspec, all_cells) for fspec in table_group_specs)
    latex = body if args.fragment and not args.preamble else render_document(body)

    if args.stdout:
        sys.stdout.write(latex)
        return 0

    out_path = args.out if args.out is not None else input_dir / "benchmark_tables.tex"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(latex, encoding="utf-8")
    print(f"Wrote {out_path}", file=sys.stderr)

    if args.fragment and not args.preamble:
        if not args.no_compile:
            print("Skipped PDF compilation for --fragment output.", file=sys.stderr)
        return 0

    if not args.no_compile:
        try:
            pdf_path = compile_pdf(out_path, args.compiler, args.keep_aux)
        except RuntimeError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1
        print(f"Wrote {pdf_path}", file=sys.stderr)
    
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
