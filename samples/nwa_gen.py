import os
import random
import string
import argparse
from typing import List, Tuple, Set # For type hints

class NestedAutomatonConfig:
    def __init__(self, num_parent_states: int, parent_is_deterministic: bool,
                 num_children: int, num_child_states: int, child_is_deterministic: bool,
                 alphabet_size: int, num_unique_return_values: int, value_function : str,
                 min_weight: float = 0.0, max_weight: float = 10.0):
        # Parent automaton
        self.num_parent_states = num_parent_states
        self.parent_is_deterministic = parent_is_deterministic
        
        # Child automata
        self.num_children = num_children
        self.num_child_states = num_child_states
        self.child_is_deterministic = child_is_deterministic
        
        # Return values and alphabet
        self.alphabet_size = alphabet_size
        self.num_unique_return_values = num_unique_return_values
        self.value_function = value_function
        self.min_weight = min_weight
        self.max_weight = max_weight

def generate_alphabet(size : int) -> List[str]:
    if size <= 26:
        return list(string.ascii_lowercase[:size]) #abcdefghijklmnopqrstuvwxyz
    else: 
        # use a1, a2...
        return [f"a{i+1}" for i in range(size)]

class NestedAutomatonGenerator:
    def __init__(self, config : NestedAutomatonConfig):
        self.config = config
        self.alphabet = generate_alphabet(config.alphabet_size)
        self.return_values = self._generate_return_values()
    
    def _generate_return_values(self) -> List[float]:
        values = set()
        while len(values) < self.config.num_unique_return_values:
            val = round(random.uniform(self.config.min_weight, self.config.max_weight), 1)
            values.add(val)
        return sorted(list(values))
        
    def generate(self) -> str:
        """Generate the complete nested automaton as a string"""
        result = []
        
        # Generate parent
        result.append("@PARENT")
        result.extend(self._generate_parent())
        result.append("")
        
        # Generate dummy child 0
        result.append("@CHILD 0")
        result.append("# Dummy child automaton (always returns SILENT)")
        result.append("")
        
        # Generate actual children
        for i in range(1, self.config.num_children + 1):
            result.append(f"@CHILD {i}")
            result.extend(self._generate_child(i))
            result.append("")
        
        # Combine each string element into a string by separating them with \n
        return "\n".join(result)

    def _generate_parent(self) -> List[str]:
        transitions = []

        for state in range(self.config.num_parent_states):
            for symbol in self.alphabet:
                # Ensure at least some transitions call actual children (not just SILENT)
                # Weight = child index (1 to num_children for actual children, 0 for SILENT)
                
                # Force at least 50% of transitions to be non-silent
                if random.random() < 0.7:  # 70% chance for non-silent
                    child_index = random.randint(1, self.config.num_children)  # Exclude 0 (SILENT)
                else:
                    child_index = 0  # SILENT
                    
                target_state = random.randint(0, self.config.num_parent_states - 1)
                
                if child_index == 0:
                    transitions.append(f"{symbol} : SILENT, q{state} -> q{target_state}")
                else:
                    transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")
        
        # If non-deterministic, add extra transitions
        if not self.config.parent_is_deterministic:
            extra_transitions = random.randint(0, len(transitions) // 2)
            for _ in range(extra_transitions):
                state = random.randint(0, self.config.num_parent_states - 1)
                symbol = random.choice(self.alphabet)
                
                # Also ensure extra transitions aren't all SILENT
                if random.random() < 0.7:
                    child_index = random.randint(1, self.config.num_children)
                else:
                    child_index = 0
                    
                target_state = random.randint(0, self.config.num_parent_states - 1)
                
                if child_index == 0:
                    transitions.append(f"{symbol} : SILENT, q{state} -> q{target_state}")
                else:
                    transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")
            
        return transitions
    
    def _generate_child(self, child_index : int) -> List[str]:
        """Generate a single child automaton"""
        lines = []
        
        # Always use the last state as the single final state (sink)
        final_state = self.config.num_child_states - 1
        final_names = [f"s{child_index}_{final_state}"]
        lines.append(f"final: {' '.join(final_names)}")
        
        # Generate transitions
        if self.config.child_is_deterministic:
            lines.extend(self._generate_deterministic_child_transitions(child_index, final_state))
        else:
            lines.extend(self._generate_nondeterministic_child_transitions(child_index, final_state))
            
        return lines

    def _generate_deterministic_child_transitions(self, child_index: int, final_state: int) -> List[str]:
        transitions = []
        
        # Generate transitions for all non-final states
        non_final_states = list(range(self.config.num_child_states - 1))    # Exclude final state
        
        # Ensure reachability
        reachable = {0}  # Start with initial state
        unreachable = set(non_final_states[1:]) # States 1, 2, ... n-2
        
        # Step 1: Build reachability tree from state 0
        for state in non_final_states:
            for symbol in self.alphabet:
                if unreachable and state in reachable:
                    # Connect to an unreachable state to ensure reachability
                    target = unreachable.pop()
                    reachable.add(target)
                else:
                    # Choose target: mix of self-loops, other states, and final state
                    target_choices = []
                    
                    # Add self-loop possibility (important for SumB accumulation)
                    target_choices.extend([state] * 2)  # 2x weight for self-loops
                    
                    # Add other non-final states
                    target_choices.extend(non_final_states)
                    
                    # Add final state (to eventually terminate)
                    target_choices.append(final_state)
                    
                    target = random.choice(target_choices)
                
                weight = random.choice(self.return_values)
                from_state = f"s{child_index}_{state}"
                to_state = f"s{child_index}_{target}"
                transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")
        
        # Final state has NO outgoing transitions (sink state)
    
        return transitions
    
    def _generate_nondeterministic_child_transitions(self, child_index: int, final_state: int) -> List[str]:
        transitions = []
        
        non_final_states = list(range(self.config.num_child_states - 1))
        
        # Build spanning tree for reachability
        reachable = {0}
        unreachable = set(non_final_states[1:])
        
        # Ensure reachability first
        while unreachable:
            unreachable_state = unreachable.pop()
            from_state_id = random.choice(list(reachable))
            symbol = random.choice(self.alphabet)
            weight = random.choice(self.return_values)
            
            from_state = f"s{child_index}_{from_state_id}"
            to_state = f"s{child_index}_{unreachable_state}"
            transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")
            
            reachable.add(unreachable_state)
        
        # Add base transitions for all non-final states
        for state in non_final_states:
            for symbol in self.alphabet:
                # Multiple target choices with bias toward self-loops and final state
                target_choices = []
                target_choices.extend([state] * 3)  # 3x weight for self-loops (SumB accumulation)
                target_choices.extend(non_final_states)
                target_choices.extend([final_state] * 2)  # 2x weight for reaching final
                
                target = random.choice(target_choices)
                weight = random.choice(self.return_values)
                from_state = f"s{child_index}_{state}"
                to_state = f"s{child_index}_{target}"
                transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")
        
        # Add extra non-deterministic transitions
        base_count = len(non_final_states) * len(self.alphabet)
        extra_count = random.randint(0, base_count // 2)
        
        for _ in range(extra_count):
            state = random.choice(non_final_states)  # Only from non-final states
            symbol = random.choice(self.alphabet)
            
            # Favor self-loops and final state for extra transitions too
            if random.random() < 0.4:  # 40% self-loops
                target = state
            elif random.random() < 0.3:  # 30% final state
                target = final_state
            else:  # 30% other non-final states
                target = random.choice(non_final_states)
                
            weight = random.choice(self.return_values)
            from_state = f"s{child_index}_{state}"
            to_state = f"s{child_index}_{target}"
            transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")
        
        # Final state has NO outgoing transitions
        
        return transitions
    
def save_nested_automaton(automaton_text : str, filename : str, directory : str = "generated_nested") -> str:
    """Save nested automaton to file"""
    scrpit_dir = os.path.dirname(os.path.abspath(__file__))
    output_dir = os.path.join(scrpit_dir, directory)
    os.makedirs(output_dir, exist_ok=True)
    
    file_path = os.path.join(output_dir, filename)
    with open(file_path, 'w') as f:
        f.write(automaton_text)
    
    return file_path

def parse_arguments():
    parser = argparse.ArgumentParser(description='Generate nested word automata for QuAK testing')
    
    # Required arguments
    parser.add_argument('num_parent_states', type=int, 
                       help='Number of states in the parent automaton')
    parser.add_argument('num_children', type=int,
                       help='Number of child automata (excluding dummy child 0)')
    parser.add_argument('num_child_states', type=int,
                       help='Number of states in each child automaton')
    parser.add_argument('alphabet_size', type=int,
                       help='Size of the input alphabet')
    parser.add_argument('num_unique_return_values', type=int,
                       help='Number of unique return values/weights')
    parser.add_argument('value_function', choices=['Min_f', 'Max_f', 'SumB'],
                        help='Value function type')

    return parser.parse_args()

def main():
    args = parse_arguments()
    
    # Validate arguments
    if args.num_parent_states <= 0:
        raise ValueError("Number of parent states must be positive")
    if args.num_children <= 0:
        raise ValueError("Number of children must be positive")
    if args.num_child_states <= 0:
        raise ValueError("Number of child states must be positive")
    if args.alphabet_size <= 0:
        raise ValueError("Alphabet size must be positive")
    if args.num_unique_return_values <= 0:
        raise ValueError("Number of unique return values must be positive")
    #if args.min_weight >= args.max_weight:
        raise ValueError("Min weight must be less than max weight")
    
    config = NestedAutomatonConfig(
        num_parent_states=args.num_parent_states,
        parent_is_deterministic = True,
        num_children=args.num_children,
        num_child_states=args.num_child_states,
        child_is_deterministic = True,
        alphabet_size=args.alphabet_size,
        num_unique_return_values=args.num_unique_return_values,
        value_function=args.value_function
        #min_weight=args.min_weight,
        #max_weight=args.max_weight
    )
    
    generator = NestedAutomatonGenerator(config)
    automaton = generator.generate()
    
    print("Generated NWA:")
    print("=" * 50)
    print(automaton)
    
    # Save to file
    filename = f"nested_{config.num_parent_states}p_{config.num_children}c_{config.num_child_states}cs.txt"
    saved_path = save_nested_automaton(automaton, filename)
    print (f"\nSaved to: {saved_path}")
    
if __name__ == "__main__":
    main()

        

        


            
        
        
