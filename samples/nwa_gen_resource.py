#!/usr/bin/env python3
"""
Generate the resource-consumption nested automata family (Example 6.1 style)
for all (n, k) with 1 <= n <= N_MAX and 1 <= k <= K_MAX.

IMPORTANT FIX:
- When the master calls child i on symbol s_i, the child immediately consumes that
  same symbol as its first input letter. So the child's initial state must have
  an s_i-transition into the tracking region.

Output format matches the user's @PARENT / @CHILD sections.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple


def alphabet_symbols(n: int, k: int) -> List[str]:
    syms: List[str] = []
    # Starts, then terminations, then resource accesses (stable order).
    for i in range(1, n + 1):
        syms.append(f"s{i}")
    for i in range(1, n + 1):
        syms.append(f"t{i}")
    for i in range(1, n + 1):
        for j in range(1, k + 1):
            syms.append(f"a{i}_{j}")
    return syms


def popcount(x: int) -> int:
    return int(x.bit_count())


def track_state_name(proc: int, mask: int) -> str:
    # mask is an integer in [0, 2^k - 1]
    return f"track{proc}_{mask}"


def emit_parent(n: int, k: int, syms: List[str]) -> List[str]:
    lines: List[str] = []
    lines.append("@PARENT")
    lines.append(f"# Resource-consumption NWA: n={n} processes, k={k} resources")
    lines.append("# Single-state parent: on s_i it calls child i, otherwise SILENT.")
    lines.append("final: q0")
    lines.append("")

    # Start symbols call the corresponding child.
    for i in range(1, n + 1):
        lines.append(f"s{i} : {i}, q0 -> q0     # Call child {i}")

    # All other symbols are silent transitions.
    # We keep the explicit SILENT keyword, matching your format.
    for i in range(1, n + 1):
        lines.append(f"t{i} : 0, q0 -> q0     # Silent transition")
    for i in range(1, n + 1):
        for j in range(1, k + 1):
            lines.append(f"a{i}_{j} : 0, q0 -> q0   # Silent transition")

    lines.append("")
    return lines


def emit_child0(n: int, k: int, syms: List[str]) -> List[str]:
    lines: List[str] = []
    lines.append("@CHILD 0")
    lines.append("# Silent/dummy child used by SILENT parent edges.")
    lines.append("# Intended to accept epsilon immediately (no letter consumed).")
    lines.append("")
    return lines


def emit_child_i(proc: int, n: int, k: int, syms: List[str]) -> List[str]:
    """
    Child i:
      - Must consume the call letter s_i immediately from its initial state.
      - Then tracks which resources j were accessed via a_{i,j} until the first t_i.
      - On that t_i transition, it goes to acc_i with weight = |seen resources|.
      - Seeing s_i again before t_i forces rej_i.
      - All other symbols are ignored (weight 0, stay).
    """
    init = f"init{proc}"
    rej = f"rej{proc}"
    acc = f"acc{proc}"

    lines: List[str] = []
    lines.append(f"@CHILD {proc}")
    lines.append(f"# Resource-tracking child for process {proc} with k={k} resources")
    lines.append(f"# States: init + {2**k} tracking subsets + rej + acc = {2**k + 3}")
    lines.append(f"final: {acc}")
    lines.append("")

    # --- Initial state transitions ---
    # Ensure the very first transition mentions init{proc} on the LHS.
    s_call = f"s{proc}"
    track0 = track_state_name(proc, 0)

    # Put the correct call symbol first for readability.
    lines.append(f"{s_call} : 0, {init} -> {track0}   # consume call letter, start tracking")

    # All other symbols from init go to reject (wrong first letter).
    for sym in syms:
        if sym == s_call:
            continue
        lines.append(f"{sym} : 0, {init} -> {rej}")

    lines.append("")

    # --- Tracking subset states ---
    # For each subset mask, define a total deterministic transition function over the alphabet.
    t_term = f"t{proc}"
    for mask in range(0, 2**k):
        st = track_state_name(proc, mask)
        lines.append(f"# Tracking state {st}: mask={mask} (popcount={popcount(mask)})")

        # For readability, emit in this order: all starts, then all terminations, then all accesses,
        # which matches syms order.
        for sym in syms:
            # Restart before termination is rejected
            if sym == s_call:
                lines.append(f"{sym} : 0, {st} -> {rej}")
                continue

            # Termination of this process ends the slave with weight popcount(mask)
            if sym == t_term:
                w = popcount(mask)
                lines.append(f"{sym} : {w}, {st} -> {acc}")
                continue

            # Access by this process updates the subset
            if sym.startswith(f"a{proc}_"):
                # sym is a{proc}_{j}
                # parse j
                j_str = sym.split("_", 1)[1]
                j = int(j_str)  # 1..k
                new_mask = mask | (1 << (j - 1))
                to = track_state_name(proc, new_mask)
                lines.append(f"{sym} : 0, {st} -> {to}")
                continue

            # Everything else is ignored
            lines.append(f"{sym} : 0, {st} -> {st}")

        lines.append("")

    # --- Reject state (sink) ---
    lines.append(f"# Reject sink {rej}: loops on all symbols with weight 0")
    for sym in syms:
        lines.append(f"{sym} : 0, {rej} -> {rej}")
    lines.append("")

    return lines


def generate_resource_consumption_automaton(n: int, k: int) -> str:
    syms = alphabet_symbols(n, k)
    out: List[str] = []
    out.extend(emit_parent(n, k, syms))
    out.extend(emit_child0(n, k, syms))
    for i in range(1, n + 1):
        out.extend(emit_child_i(i, n, k, syms))
    return "\n".join(out).rstrip() + "\n"


def main() -> None:
    # Hardcode bounds here.
    N_MAX = 10
    K_MAX = 10

    out_dir = Path("generated_resource_consumption")
    out_dir.mkdir(parents=True, exist_ok=True)

    for n in range(1, N_MAX + 1):
        for k in range(1, K_MAX + 1):
            txt = generate_resource_consumption_automaton(n, k)
            path = out_dir / f"resource_n{n}_k{k}.txt"
            path.write_text(txt, encoding="utf-8")
            print(f"Wrote {path}  (n={n}, k={k})")


if __name__ == "__main__":
    main()
