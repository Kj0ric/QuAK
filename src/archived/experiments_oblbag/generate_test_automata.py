#!/usr/bin/env python3
"""
Generate random nested automata for benchmarking flatten_regular.
Creates complete, connected automata with reachable final states.
"""

import os
import random
import argparse

def generate_nested_automaton(
    parent_states: int,
    alphabet_size: int,
    num_children: int,
    child_states_range: tuple,
    weight_range: tuple,
    output_file: str,
    seed: int = None
):
    """Generate a nested automaton file."""
    if seed is not None:
        random.seed(seed)

    alphabet = [chr(ord('a') + i) for i in range(alphabet_size)]

    with open(output_file, 'w') as f:
        # Parent automaton
        f.write("@PARENT\n")

        parent_state_names = [f"p{i}" for i in range(parent_states)]

        # First, ensure connectivity: create a path visiting all states
        for i in range(parent_states - 1):
            sym = alphabet[i % alphabet_size]
            child_idx = random.randint(1, num_children)
            f.write(f"{sym} : {child_idx}, {parent_state_names[i]} -> {parent_state_names[i+1]}\n")

        # Track which (state, symbol) pairs have transitions
        has_transition = {}
        for i in range(parent_states - 1):
            sym = alphabet[i % alphabet_size]
            has_transition[(parent_state_names[i], sym)] = True

        # Complete the parent: for each state and symbol not yet covered, add transition
        for ps in parent_state_names:
            for sym in alphabet:
                if (ps, sym) not in has_transition:
                    dest = random.choice(parent_state_names)
                    child_idx = random.randint(1, num_children)
                    f.write(f"{sym} : {child_idx}, {ps} -> {dest}\n")

        f.write("\n")

        # Child 0 (dummy - required by parser)
        f.write("@CHILD 0\n\n")

        # Generate children with guaranteed path to final state
        for child_idx in range(1, num_children + 1):
            child_states = random.randint(child_states_range[0], child_states_range[1])
            child_state_names = [f"c{child_idx}s{i}" for i in range(child_states)]
            final_state = child_state_names[-1]

            f.write(f"@CHILD {child_idx}\n")
            f.write(f"final: {final_state}\n")

            # First, ensure a path exists from initial (s0) to final
            # Create a chain: s0 -> s1 -> ... -> s_final
            has_transition = {}
            for i in range(child_states - 1):
                sym = alphabet[i % alphabet_size]
                weight = random.randint(weight_range[0], weight_range[1])
                f.write(f"{sym} : {weight}, {child_state_names[i]} -> {child_state_names[i+1]}\n")
                has_transition[(child_state_names[i], sym)] = True

            # Complete non-final states: add missing transitions
            for i, cs in enumerate(child_state_names[:-1]):  # Non-final states
                for sym in alphabet:
                    if (cs, sym) not in has_transition:
                        # Randomly go to any state (including final for variety)
                        dest = random.choice(child_state_names)
                        weight = random.randint(weight_range[0], weight_range[1])
                        f.write(f"{sym} : {weight}, {cs} -> {dest}\n")

            f.write("\n")

    return output_file


def main():
    parser = argparse.ArgumentParser(description="Generate nested automata for benchmarking")
    parser.add_argument("--output-dir", default="test_automata", help="Output directory")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    # Test configurations: (name, parent_states, alphabet, num_children, child_states_range)
    configs = [
        # Small - baseline
        ("small_3", 3, 2, 3, (3, 3)),

        # Medium
        ("medium_1", 3, 2, 2, (4, 4)),
        ("medium_2", 3, 3, 2, (3, 4)),

        # Large - good for benchmarking
        ("large_3", 3, 3, 3, (3, 4)),
        ("large_4", 4, 3, 3, (4, 4)),

        # Extra large - for stress testing
        ("xlarge_1", 4, 3, 4, (4, 5)),
        ("xlarge_2", 5, 3, 3, (4, 5)),
        ("xlarge_3", 4, 4, 3, (4, 4)),
    ]

    random.seed(args.seed)

    for name, ps, alpha, nc, cs_range in configs:
        output_file = os.path.join(args.output_dir, f"{name}.txt")
        generate_nested_automaton(
            parent_states=ps,
            alphabet_size=alpha,
            num_children=nc,
            child_states_range=cs_range,
            weight_range=(0, 5),
            output_file=output_file,
            seed=random.randint(0, 1000000)
        )
        print(f"Generated: {output_file}")
        print(f"  Parent: {ps} states, {alpha} alphabet, {nc} children")
        print(f"  Child states: {cs_range[0]}-{cs_range[1]}")


if __name__ == "__main__":
    main()
