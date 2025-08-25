import string
import sys
import os

# Test the feasibility limits of the büchi automaton
def generate_nested_automaton(n, parent_states, outdir="samples/generated_nested"):
    #assert m == n, "m (alphabet size) must be equal n (children count)"
    alphabet = list(string.ascii_lowercase[:n])
    parent_state_names = [f"q{i}" for i in range(parent_states)]
    
    lines = []
    # Parent
    lines.append("@PARENT")
    for i, sym in enumerate(alphabet):
        # Example: cycle through parent states for demonstration
        from_state = parent_state_names[i % parent_states]
        to_state = parent_state_names[(i + 1) % parent_states]
        lines.append(f"{sym} : {i+1}, {from_state} -> {to_state}")
    lines.append("")
    
    # Dummy child 0
    lines.append("@CHILD 0")
    lines.append("")
    
    # Children
    for i, sym in enumerate(alphabet):
        lines.append(f"@CHILD {i+1}")
        state0 = f"s{i+1}_0" if i % 2 == 0 else f"t{i+1}_0"
        state1 = f"s{i+1}_1" if i % 2 == 0 else f"t{i+1}_1"
        lines.append(f"final: {state1}")
        
        # Self loop on symbol i with weight 1, transition to final on other symbol with weight 0
        for j, sym2 in enumerate(alphabet):
            if j == i:
                lines.append(f"{sym2} : 1, {state0} -> {state0}")
            else:
                lines.append(f"{sym2} : 0, {state0} -> {state1}")
        lines.append("")
        
    # Write to file
    os.makedirs(outdir, exist_ok=True)
    filename = os.path.join(outdir, f"nested_{n}a_{n}c_{parent_states}ps.txt")
    with open(filename, "w") as f:
        f.write("\n".join(lines))
    print(f"Nested automaton written to {filename}")
    
if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generate_nested.py <children_count> <parent_state_num>")
        sys.exit(1)
    n = int(sys.argv[1])
    parent_states = int(sys.argv[2])
    generate_nested_automaton(n, parent_states)

        