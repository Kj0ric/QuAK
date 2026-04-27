#!/usr/bin/env python3
"""
Generate the bounded-pending response-time nested automata family.

Semantics:
  * pending requests = number of 'r' letters since the last 'g'.
    A 'g' grants all currently pending requests at once.
  * We track:
      - c in {0,..,n}: number of pending requests
      - a in {0,..,k-1}: age (in steps) of the oldest pending request
    When a would reach k without seeing a 'g', we move to the sink.

Important reachability invariant (given one symbol per step):
  If c >= 1 then necessarily a >= c-1.
  (To have c pending requests you must have seen c requests, which takes >= c steps,
   so the oldest must have aged at least c-1.)
So we only generate parent states (c,a) with 1 <= c <= n and c-1 <= a <= k-1.
"""

from __future__ import annotations

from pathlib import Path
from typing import Dict, List, Tuple


def alphabet_symbols() -> List[str]:
    return ["o", "g", "r"]


def reachable_pairs(n: int, k: int) -> List[Tuple[int, int]]:
    """
    All reachable (c,a) with 1 <= c <= n and c-1 <= a <= k-1.
    """
    pairs: List[Tuple[int, int]] = []
    for c in range(1, n + 1):
        a_min = c - 1
        if a_min >= k:
            continue
        for a in range(a_min, k):
            pairs.append((c, a))
    return pairs


def build_state_names(n: int, k: int) -> Dict[Tuple[int, int], str]:
    """
    Map each reachable (c,a) to a compact name q1, q2, ...
    """
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

    # From q0
    lines.append("o : 0, q0 -> q0")
    lines.append("g : 0, q0 -> q0")
    if n >= 1 and k >= 1:
        lines.append(f"r : 1, q0 -> {state_names[(1, 0)]}   # Call child 1")
    else:
        lines.append("r : 0, q0 -> s")
    lines.append("")

    # From reachable (c,a)
    for (c, a) in reachable_pairs(n, k):
        st = state_names[(c, a)]
        lines.append(f"# State {st}: pending={c}, oldest_age={a}")

        # 'o' advances time for all pending requests
        a_next = a + 1
        if a_next < k:
            st_next = state_names[(c, a_next)]
            lines.append(f"o : 0, {st} -> {st_next}")
        else:
            lines.append(f"o : 0, {st} -> s")

        # 'g' grants all current requests
        lines.append(f"g : 0, {st} -> q0")

        # 'r' advances time and adds a new pending request
        c_next = c + 1
        if a_next >= k or c_next > n:
            lines.append(f"r : 0, {st} -> s")
        else:
            # (c_next, a_next) is reachable automatically: a_next >= a+1 >= c >= c_next-1
            st_next = state_names[(c_next, a_next)]
            lines.append(f"r : 1, {st} -> {st_next}   # Call child 1")

        lines.append("")

    # Sink
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


def emit_child1() -> List[str]:
    lines: List[str] = []
    lines.append("@CHILD 1")
    lines.append("# Response-time tracking child")
    lines.append("# Counts steps (r or o) until a 'g' is seen, then accepts.")
    lines.append("final: s1")
    lines.append("")
    lines.append("r : 1, s0 -> s0   # increment count on request")
    lines.append("o : 1, s0 -> s0   # increment count on other")
    lines.append("g : 0, s0 -> s1   # grant seen, accept")
    lines.append("")
    return lines


def generate_bounded_pending_automaton(n: int, k: int, q0_is_final: bool) -> str:
    out: List[str] = []
    out.extend(emit_parent(n, k, q0_is_final))
    out.extend(emit_child0())
    out.extend(emit_child1())
    return "\n".join(out).rstrip() + "\n"


def main() -> None:
    # N_LIST = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    # K_LIST = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    N_LIST = [2, 4, 8, 16, 32, 64, 128, 256, 512]
    K_LIST = [2, 4, 8, 16, 32, 64, 128, 256, 512]
    Q0_IS_FINAL = True

    out_dir = Path("generated_response_time")
    out_dir.mkdir(parents=True, exist_ok=True)

    for n in N_LIST:
        for k in K_LIST:
            if k >= n:
                txt = generate_bounded_pending_automaton(n, k, Q0_IS_FINAL)
                path = out_dir / f"response_n{n}_k{k}.txt"
                path.write_text(txt, encoding="utf-8")
                print(f"Wrote {path}  (n={n}, k={k})")


if __name__ == "__main__":
    main()
