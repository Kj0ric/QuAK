#!/usr/bin/env python3
"""
Generate the Modulo-n Cycle Automata Family.

Structure:
1. Alphabet: {a, b}
2. Parent: 
   - n states (q0..qn-1), cyclic transitions (i -> i+1 mod n).
   - Initial q0 is accepting.
   - Transition q0 --a--> q1 spawns Child 1 (Counting).
   - All other transitions spawn Child 0 (Dummy).
3. Child 1:
   - Counts 'a's (including the call symbol).
   - Accepts on 'b'.
"""

from __future__ import annotations

from pathlib import Path
from typing import List

def alphabet_symbols() -> List[str]:
    return ["a", "b"]

def emit_parent(n: int) -> List[str]:
    lines: List[str] = []
    lines.append("@PARENT")
    lines.append(f"# Modulo-{n} cycle parent")
    lines.append("# States: q0 to q{n-1}. q0 is initial and final.")
    lines.append("# Transition behavior: always i -> (i+1)%n")
    lines.append("# Spawning: (q0, a) -> Child 1, else -> Child 0")
    lines.append("final: q0")
    lines.append("")

    for i in range(n):
        current_state = f"q{i}"
        next_state = f"q{(i + 1) % n}"
        
        # Transition on 'a'
        lines.append(f"a : 1, {current_state} -> {next_state}   # Call Counter Child")
        # if i == 0:
        #     # The only non-silent call
        #     lines.append(f"a : 1, {current_state} -> {next_state}   # Call Counter Child")
        # else:
        #     lines.append(f"a : 0, {current_state} -> {next_state}   # Call Dummy")

        # Transition on 'b'
        # Always dummy, even from q0 (prompt says "from initial with a it spawns...")
        lines.append(f"b : 0, {current_state} -> {next_state}   # Call Dummy")

    lines.append("")
    return lines


def emit_child0() -> List[str]:
    """Silent dummy child."""
    lines: List[str] = []
    lines.append("@CHILD 0")
    lines.append("# Dummy child for silent transitions")
    lines.append("")
    return lines


def emit_child1() -> List[str]:
    """
    Counting Child (ID 1).
    - Called on 'a'. 
    - Consumes 'a' immediately (weight 1).
    - Loops on 'a' (weight 1).
    - Accepts on 'b' (weight 0).
    """
    lines: List[str] = []
    lines.append("@CHILD 1")
    lines.append("# Counting child: counts 'a's, accepts on 'b'")
    lines.append("final: acc")
    lines.append("")

    # States
    init = "init"
    acc = "acc"

    # 1. Initial Transition (Consuming the call symbol 'a')
    lines.append(f"# Initial call consumption")
    lines.append(f"a : 1, {init} -> {init}   # Consume call 'a', count 1")
    lines.append(f"b : 0, {init} -> {acc}    # Accept")
    lines.append("")
    return lines


def generate_automaton(n: int) -> str:
    out: List[str] = []
    out.extend(emit_parent(n))
    out.extend(emit_child0())
    out.extend(emit_child1())
    return "\n".join(out).rstrip() + "\n"


def main() -> None:
    # Set bounds
    N_MAX = 100
    
    out_dir = Path("generated_modulo_counter_more")
    out_dir.mkdir(parents=True, exist_ok=True)

    for n in range(1, N_MAX + 1):
        txt = generate_automaton(n)
        path = out_dir / f"modulo_cycle_n{n}.txt"
        path.write_text(txt, encoding="utf-8")
        print(f"Wrote {path}")


if __name__ == "__main__":
    main()