#!/usr/bin/env python3
import sys

def generate_nested_automaton(n: int, k: int, q0_is_final: bool) -> str:
    """Generate a nested automaton enforcing:
      - at most n pending 'r' requests at all times, and
      - every 'r' is followed by a 'g' within k steps.

    Semantics:
      * 'pending requests' = number of 'r' letters since the last 'g'.
        A 'g' grants all currently pending requests at once.
      * We track:
          - c in {0,..,n}: number of pending requests
          - a in {0,..,k-1}: age (in steps) of the oldest pending request
        When a would reach k without seeing a 'g', we move to the sink.
    """
    if k <= 0:
        raise ValueError("k must be >= 1")
    if n < 0:
        raise ValueError("n must be >= 0")

    # Name states:
    #   q0    : no pending requests (c = 0)
    #   q1,q2,... : pairs (c,a) with 1 <= c <= n and 0 <= a < k
    #   s     : violation (too many pending, or oldest waited >= k steps)
    state_names = {}
    idx = 1
    for c in range(1, n + 1):
        for a in range(0, k):
            state_names[(c, a)] = f"q{idx}"
            idx += 1

    lines = []
    lines.append("@PARENT")
    lines.append("# q0  : no pending requests")
    if n > 0:
        lines.append("# q_i : some (pending_count, age_of_oldest) for i >= 1")
    lines.append("# s   : violation (too many pending or some request waited >= k steps)")
    
    # NEW: Handle q0 finality
    if q0_is_final:
        lines.append("final: q0")

    # Transitions from q0
    lines.append("o : 0, q0 -> q0")
    lines.append("g : 0, q0 -> q0")
    if n > 0:
        # first request: c = 1, age a = 0
        lines.append(f"r : 1, q0 -> {state_names[(1, 0)]}")
    else:
        # no requests allowed at all
        lines.append("r : 0, q0 -> s")

    # Transitions from each (c,a) state, in the order o, g, r
    for c in range(1, n + 1):
        for a in range(0, k):
            st = state_names[(c, a)]

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
                # new accepted request; give weight 1 as in your examples
                lines.append(f"r : 1, {st} -> {st_next}")

    # Sink is absorbing
    lines.append("o : 0, s -> s")
    lines.append("g : 0, s -> s")
    lines.append("r : 0, s -> s")
    lines.append("")
    lines.append("@CHILD 0")
    lines.append("")
    lines.append("@CHILD 1")
    lines.append("final: s1")
    lines.append("r : -1, s0 -> s0")
    lines.append("o : -1, s0 -> s0")
    lines.append("g : 0, s0 -> s1")

    return "\n".join(lines)

def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]

    if len(argv) != 3:
        print("Usage: python gen_bounded_pending.py <n_max> <k_max> <q0_is_final (true/false)>", file=sys.stderr)
        sys.exit(1)

    try:
        n_max = int(argv[0])
        k_max = int(argv[1])
        q0_final_str = argv[2].lower()
        
        if q0_final_str in ['true', '1', 'yes', 't']:
            q0_is_final = True
        elif q0_final_str in ['false', '0', 'no', 'f']:
            q0_is_final = False
        else:
            raise ValueError("Third argument must be a boolean (true/false)")
            
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Generating automata for i=1..{n_max} and j=1..{k_max}...")
    
    count = 0
    # Loop on all (i,j) with i <= n and j <= k
    for i in range(1, n_max + 1):
        for j in range(1, k_max + 1):
            content = generate_nested_automaton(i, j, q0_is_final)
            filename = f"avg_resp_{i}_{j}.txt"
            
            try:
                with open(filename, "w") as f:
                    f.write(content)
                count += 1
            except IOError as e:
                print(f"Failed to write {filename}: {e}", file=sys.stderr)

    print(f"Successfully generated {count} files.")

if __name__ == "__main__":
    main()