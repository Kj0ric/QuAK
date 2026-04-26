#!/usr/bin/env python3
"""
Generate max-based response-time benchmark families that mirror the checked-in
SumPlus benchmark layouts.

The parent automaton is unchanged. Child 1 uses increasing weights so its Max
value equals the same per-request response time that the SumPlus child returns.
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Dict, Iterable, List, Tuple


PAIR_RE = re.compile(r"response_n(\d+)_k(\d+)\.txt$")


def reachable_pairs(n: int, k: int) -> List[Tuple[int, int]]:
    pairs: List[Tuple[int, int]] = []
    for c in range(1, n + 1):
        a_min = c - 1
        if a_min >= k:
            continue
        for a in range(a_min, k):
            pairs.append((c, a))
    return pairs


def build_state_names(n: int, k: int) -> Dict[Tuple[int, int], str]:
    state_names: Dict[Tuple[int, int], str] = {}
    idx = 1
    for (c, a) in reachable_pairs(n, k):
        state_names[(c, a)] = f"q{idx}"
        idx += 1
    return state_names


def emit_parent(n: int, k: int, q0_is_final: bool) -> List[str]:
    state_names = build_state_names(n, k)
    lines: List[str] = []
    lines.append("@PARENT")
    lines.append(f"# Bounded-pending NWA: n={n} max pending, k={k} max wait time")
    lines.append("# q0  : no pending requests")
    lines.append("# q_i : reachable (pending_count, oldest_age) with oldest_age >= pending_count-1")
    lines.append("# s   : violation (too many pending or some request waited >= k steps)")

    if q0_is_final:
        lines.append("final: q0")
    lines.append("")

    lines.append("o : 0, q0 -> q0")
    lines.append("g : 0, q0 -> q0")
    if n >= 1 and k >= 1:
        lines.append(f"r : 1, q0 -> {state_names[(1, 0)]}   # Call child 1")
    else:
        lines.append("r : 0, q0 -> s")
    lines.append("")

    for (c, a) in reachable_pairs(n, k):
        st = state_names[(c, a)]
        lines.append(f"# State {st}: pending={c}, oldest_age={a}")

        a_next = a + 1
        if a_next < k:
            lines.append(f"o : 0, {st} -> {state_names[(c, a_next)]}")
        else:
            lines.append(f"o : 0, {st} -> s")

        lines.append(f"g : 0, {st} -> q0")

        c_next = c + 1
        if a_next >= k or c_next > n:
            lines.append(f"r : 0, {st} -> s")
        else:
            lines.append(f"r : 1, {st} -> {state_names[(c_next, a_next)]}   # Call child 1")

        lines.append("")

    lines.append("# Sink state s: violation detected")
    lines.append("o : 0, s -> s")
    lines.append("g : 0, s -> s")
    lines.append("r : 0, s -> s")
    lines.append("")
    return lines


def emit_child0() -> List[str]:
    lines: List[str] = []
    lines.append("@CHILD 0")
    lines.append("# Silent/dummy child used by weight-0 parent edges.")
    lines.append("")
    return lines


def emit_child1_max(k: int) -> List[str]:
    lines: List[str] = []
    lines.append("@CHILD 1")
    lines.append("# Response-time tracking child")
    lines.append("# Uses increasing weights so Max equals the exact response time.")
    lines.append("final: done")
    lines.append("")

    if k < 1:
        return lines

    lines.append("r : 1, s0 -> s1   # consume call letter, elapsed=1")
    lines.append("")

    for elapsed in range(1, k + 1):
        state = f"s{elapsed}"
        lines.append(f"# State {state}: current elapsed response time = {elapsed}")
        lines.append(f"g : 0, {state} -> done")
        if elapsed < k:
            next_state = f"s{elapsed + 1}"
            weight = elapsed + 1
            lines.append(f"r : {weight}, {state} -> {next_state}")
            lines.append(f"o : {weight}, {state} -> {next_state}")
        lines.append("")

    return lines


def generate_bounded_pending_automaton_max(n: int, k: int, q0_is_final: bool) -> str:
    out: List[str] = []
    out.extend(emit_parent(n, k, q0_is_final))
    out.extend(emit_child0())
    out.extend(emit_child1_max(k))
    return "\n".join(out).rstrip() + "\n"


def parse_pair(path: Path) -> Tuple[int, int]:
    match = PAIR_RE.fullmatch(path.name)
    if match is None:
        raise ValueError(f"Unexpected response benchmark filename: {path.name}")
    return (int(match.group(1)), int(match.group(2)))


def pairs_from_dir(source_dir: Path) -> List[Tuple[int, int]]:
    if not source_dir.is_dir():
        raise FileNotFoundError(f"Missing source benchmark directory: {source_dir}")
    return sorted(parse_pair(path) for path in source_dir.glob("response_n*_k*.txt"))


def write_family(pairs: Iterable[Tuple[int, int]], out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    for (n, k) in pairs:
        txt = generate_bounded_pending_automaton_max(n, k, q0_is_final=True)
        path = out_dir / f"response_n{n}_k{k}.txt"
        path.write_text(txt, encoding="utf-8")
        print(f"Wrote {path}  (n={n}, k={k})")


def main() -> None:
    samples_dir = Path(__file__).resolve().parent
    jobs = [
        ("generated_response_time", "generated_response_time_max"),
        ("generated_response_time_1", "generated_response_time_max_1"),
        ("generated_response_time_2", "generated_response_time_max_2"),
        (
            "experiments_main_tables/generated_response_time_1",
            "experiments_main_tables/generated_response_time_max_1",
        ),
        (
            "experiments_main_tables/generated_response_time_2",
            "experiments_main_tables/generated_response_time_max_2",
        ),
    ]

    for (source_name, target_name) in jobs:
        source_dir = samples_dir / source_name
        target_dir = samples_dir / target_name
        write_family(pairs_from_dir(source_dir), target_dir)


if __name__ == "__main__":
    main()
