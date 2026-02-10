#!/usr/bin/env python3
"""
Generate the bounded-pending response-time nested automata family
for all (n, k) with 1 <= n <= N_MAX and 1 <= k <= K_MAX.

Semantics:
  * 'pending requests' = number of 'r' letters since the last 'g'.
    A 'g' grants all currently pending requests at once.
  * We track:
      - c in {0,..,n}: number of pending requests
      - a in {0,..,k-1}: age (in steps) of the oldest pending request
    When a would reach k without seeing a 'g', we move to the sink.

Output format matches the @PARENT / @CHILD sections.
"""

from __future__ import annotations

from pathlib import Path
from typing import Dict, List, Tuple


def alphabet_symbols() -> List[str]:
    """Return the alphabet: o (other), g (grant), r (request)."""
    return ["o", "g", "r"]


def state_name(c: int, a: int) -> str:
    """Return the state name for (pending_count, age_of_oldest) pair."""
    # We use a simple naming scheme: q1, q2, ...
    # but we need consistent indexing, so we compute it on the fly.
    # Actually, let's just return a descriptive name for clarity.
    return f"q_c{c}_a{a}"


def build_state_names(n: int, k: int) -> Dict[Tuple[int, int], str]:
    """Build a mapping from (c, a) pairs to state names."""
    state_names: Dict[Tuple[int, int], str] = {}
    idx = 1
    for c in range(1, n + 1):
        for a in range(0, k):
            state_names[(c, a)] = f"q{idx}"
            idx += 1
    return state_names


def emit_parent(n: int, k: int, q0_is_final: bool) -> List[str]:
    """Emit the parent automaton section."""
    state_names = build_state_names(n, k)
    syms = alphabet_symbols()

    lines: List[str] = []
    lines.append("@PARENT")
    lines.append(f"# Bounded-pending NWA: n={n} max pending, k={k} max wait time")
    lines.append("# q0  : no pending requests")
    if n > 0:
        lines.append("# q_i : some (pending_count, age_of_oldest) for i >= 1")
    lines.append("# s   : violation (too many pending or some request waited >= k steps)")

    if q0_is_final:
        lines.append("final: q0")

    lines.append("")

    # Transitions from q0
    lines.append("o : 0, q0 -> q0")
    lines.append("g : 0, q0 -> q0")
    lines.append("r : 0, q0 -> q0")
    if n > 0:
        # first request: c = 1, age a = 0
        lines.append(f"r : 1, q0 -> {state_names[(1, 0)]}   # Call child 1")
    else:
        # no requests allowed at all
        lines.append("r : 0, q0 -> s")

    lines.append("")

    # Transitions from each (c, a) state
    for c in range(1, n + 1):
        for a in range(0, k):
            st = state_names[(c, a)]
            lines.append(f"# State {st}: pending={c}, oldest_age={a}")

            # 'o' just advances time for all pending requests
            a_next = a + 1
            if a_next < k:
                st_next = state_names[(c, a_next)]
                lines.append(f"o : 0, {st} -> {st_next}")
            else:
                # oldest request would wait k steps without 'g'
                lines.append(f"o : 0, {st} -> s")

            # 'g' grants all current requests
            lines.append(f"g : 0, {st} -> q0")

            # 'r' both advances time and adds a new pending request
            a_next = a + 1
            c_next = c + 1
            if a_next >= k or c_next > n:
                # either the oldest waited too long, or too many requests
                lines.append(f"r : 0, {st} -> s")
            else:
                st_next = state_names[(c_next, a_next)]
                # new accepted request; give weight 1
                lines.append(f"r : 1, {st} -> {st_next}   # Call child 1")

            lines.append("")

    # Sink is absorbing
    lines.append("# Sink state s: violation detected")
    lines.append("o : 0, s -> s")
    lines.append("g : 0, s -> s")
    lines.append("r : 0, s -> s")
    lines.append("")

    return lines


def emit_child0() -> List[str]:
    """Emit the silent/dummy child 0."""
    lines: List[str] = []
    lines.append("@CHILD 0")
    lines.append("# Silent/dummy child used by weight-0 parent edges.")
    lines.append("")
    return lines


def emit_child1() -> List[str]:
    """Emit child 1: counts steps until 'g' is seen."""
    lines: List[str] = []
    lines.append("@CHILD 1")
    lines.append("# Response-time tracking child")
    lines.append("# Counts steps (r or o) until a 'g' is seen, then accepts.")
    lines.append("final: s1")
    lines.append("")
    lines.append("r : -1, s0 -> s0   # increment count on request")
    lines.append("o : -1, s0 -> s0   # increment count on other")
    lines.append("g : 0, s0 -> s1   # grant seen, accept")
    lines.append("")
    return lines


def generate_bounded_pending_automaton(n: int, k: int, q0_is_final: bool) -> str:
    """Generate a complete nested automaton for given parameters."""
    out: List[str] = []
    out.extend(emit_parent(n, k, q0_is_final))
    out.extend(emit_child0())
    out.extend(emit_child1())
    return "\n".join(out).rstrip() + "\n"


def main() -> None:
    # Hardcode bounds here.
    # N_LIST = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 32, 64, 128, 256, 512]
    # K_LIST = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 32, 64, 128, 256, 512]
    # N_LIST = [16, 32, 64, 128, 256]
    # K_LIST = [16, 32, 64, 128, 256, 512]
    N_LIST = [1, 2, 3, 4, 5]
    K_LIST = [1, 2, 3, 4, 5]
    Q0_IS_FINAL = True

    out_dir = Path("generated_response_time_negative")
    out_dir.mkdir(parents=True, exist_ok=True)

    for n in N_LIST:
        for k in K_LIST:
            if (k >= n):
                txt = generate_bounded_pending_automaton(n, k, Q0_IS_FINAL)
                path = out_dir / f"response_n{n}_k{k}.txt"
                path.write_text(txt, encoding="utf-8")
                print(f"Wrote {path}  (n={n}, k={k})")


if __name__ == "__main__":
    main()