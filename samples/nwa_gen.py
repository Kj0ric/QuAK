# import os
# import random
# import string
# import argparse
# from typing import List, Tuple, Set # For type hints

# class NestedAutomatonConfig:
#     def __init__(self, num_parent_states: int, parent_is_deterministic: bool,
#                  num_children: int, num_child_states: int, child_is_deterministic: bool,
#                  alphabet_size: int, num_unique_return_values: int, value_function : str,
#                  min_weight: float = 0.0, max_weight: float = 10.0):
#         # Parent automaton
#         self.num_parent_states = num_parent_states
#         self.parent_is_deterministic = parent_is_deterministic
        
#         # Child automata
#         self.num_children = num_children
#         self.num_child_states = num_child_states
#         self.child_is_deterministic = child_is_deterministic
        
#         # Return values and alphabet
#         self.alphabet_size = alphabet_size
#         self.num_unique_return_values = num_unique_return_values
#         self.value_function = value_function
#         self.min_weight = min_weight
#         self.max_weight = max_weight

# def generate_alphabet(size : int) -> List[str]:
#     if size <= 26:
#         return list(string.ascii_lowercase[:size]) #abcdefghijklmnopqrstuvwxyz
#     else: 
#         # use a1, a2...
#         return [f"a{i+1}" for i in range(size)]

# class NestedAutomatonGenerator:
#     def __init__(self, config : NestedAutomatonConfig):
#         self.config = config
#         self.alphabet = generate_alphabet(config.alphabet_size)
#         self.return_values = self._generate_return_values()
    
#     def _generate_return_values(self) -> List[float]:
#         values = set()
#         while len(values) < self.config.num_unique_return_values:
#             val = round(random.uniform(self.config.min_weight, self.config.max_weight), 1)
#             values.add(val)
#         return sorted(list(values))
        
#     def generate(self) -> str:
#         """Generate the complete nested automaton as a string"""
#         result = []
        
#         # Generate parent
#         result.append("@PARENT")
#         result.extend(self._generate_parent())
#         result.append("")
        
#         # Generate dummy child 0
#         result.append("@CHILD 0")
#         result.append("# Dummy child automaton (always returns SILENT)")
#         result.append("")
        
#         # Generate actual children
#         for i in range(1, self.config.num_children + 1):
#             result.append(f"@CHILD {i}")
#             result.extend(self._generate_child(i))
#             result.append("")
        
#         # Combine each string element into a string by separating them with \n
#         return "\n".join(result)

#     def _generate_parent(self) -> List[str]:
#         transitions = []

#         for state in range(self.config.num_parent_states):
#             for symbol in self.alphabet:
#                 # Ensure at least some transitions call actual children (not just SILENT)
#                 # Weight = child index (1 to num_children for actual children, 0 for SILENT)
                
#                 # Force at least 50% of transitions to be non-silent
#                 if random.random() < 0.7:  # 70% chance for non-silent
#                     child_index = random.randint(1, self.config.num_children)  # Exclude 0 (SILENT)
#                 else:
#                     child_index = 0  # SILENT
                    
#                 target_state = random.randint(0, self.config.num_parent_states - 1)
                
#                 if child_index == 0:
#                     transitions.append(f"{symbol} : SILENT, q{state} -> q{target_state}")
#                 else:
#                     transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")
        
#         # If non-deterministic, add extra transitions
#         if not self.config.parent_is_deterministic:
#             extra_transitions = random.randint(0, len(transitions) // 2)
#             for _ in range(extra_transitions):
#                 state = random.randint(0, self.config.num_parent_states - 1)
#                 symbol = random.choice(self.alphabet)
                
#                 # Also ensure extra transitions aren't all SILENT
#                 if random.random() < 0.7:
#                     child_index = random.randint(1, self.config.num_children)
#                 else:
#                     child_index = 0
                    
#                 target_state = random.randint(0, self.config.num_parent_states - 1)
                
#                 if child_index == 0:
#                     transitions.append(f"{symbol} : SILENT, q{state} -> q{target_state}")
#                 else:
#                     transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")
            
#         return transitions
    
#     def _generate_child(self, child_index : int) -> List[str]:
#         """Generate a single child automaton"""
#         lines = []
        
#         # Always use the last state as the single final state (sink)
#         final_state = self.config.num_child_states - 1
#         final_names = [f"s{child_index}_{final_state}"]
#         lines.append(f"final: {' '.join(final_names)}")
        
#         # Generate transitions
#         if self.config.child_is_deterministic:
#             lines.extend(self._generate_deterministic_child_transitions(child_index, final_state))
#         else:
#             lines.extend(self._generate_nondeterministic_child_transitions(child_index, final_state))
            
#         return lines

#     def _generate_deterministic_child_transitions(self, child_index: int, final_state: int) -> List[str]:
#         transitions = []
        
#         # Generate transitions for all non-final states
#         non_final_states = list(range(self.config.num_child_states - 1))    # Exclude final state
        
#         # Ensure reachability
#         reachable = {0}  # Start with initial state
#         unreachable = set(non_final_states[1:]) # States 1, 2, ... n-2
        
#         # Step 1: Build reachability tree from state 0
#         for state in non_final_states:
#             for symbol in self.alphabet:
#                 if unreachable and state in reachable:
#                     # Connect to an unreachable state to ensure reachability
#                     target = unreachable.pop()
#                     reachable.add(target)
#                 else:
#                     # Choose target: mix of self-loops, other states, and final state
#                     target_choices = []
                    
#                     # Add self-loop possibility (important for SumB accumulation)
#                     target_choices.extend([state] * 2)  # 2x weight for self-loops
                    
#                     # Add other non-final states
#                     target_choices.extend(non_final_states)
                    
#                     # Add final state (to eventually terminate)
#                     target_choices.append(final_state)
                    
#                     target = random.choice(target_choices)
                
#                 weight = random.choice(self.return_values)
#                 from_state = f"s{child_index}_{state}"
#                 to_state = f"s{child_index}_{target}"
#                 transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")
        
#         # Final state has NO outgoing transitions (sink state)
    
#         return transitions
    
#     def _generate_nondeterministic_child_transitions(self, child_index: int, final_state: int) -> List[str]:
#         transitions = []
        
#         non_final_states = list(range(self.config.num_child_states - 1))
        
#         # Build spanning tree for reachability
#         reachable = {0}
#         unreachable = set(non_final_states[1:])
        
#         # Ensure reachability first
#         while unreachable:
#             unreachable_state = unreachable.pop()
#             from_state_id = random.choice(list(reachable))
#             symbol = random.choice(self.alphabet)
#             weight = random.choice(self.return_values)
            
#             from_state = f"s{child_index}_{from_state_id}"
#             to_state = f"s{child_index}_{unreachable_state}"
#             transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")
            
#             reachable.add(unreachable_state)
        
#         # Add base transitions for all non-final states
#         for state in non_final_states:
#             for symbol in self.alphabet:
#                 # Multiple target choices with bias toward self-loops and final state
#                 target_choices = []
#                 target_choices.extend([state] * 3)  # 3x weight for self-loops (SumB accumulation)
#                 target_choices.extend(non_final_states)
#                 target_choices.extend([final_state] * 2)  # 2x weight for reaching final
                
#                 target = random.choice(target_choices)
#                 weight = random.choice(self.return_values)
#                 from_state = f"s{child_index}_{state}"
#                 to_state = f"s{child_index}_{target}"
#                 transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")
        
#         # Add extra non-deterministic transitions
#         base_count = len(non_final_states) * len(self.alphabet)
#         extra_count = random.randint(0, base_count // 2)
        
#         for _ in range(extra_count):
#             state = random.choice(non_final_states)  # Only from non-final states
#             symbol = random.choice(self.alphabet)
            
#             # Favor self-loops and final state for extra transitions too
#             if random.random() < 0.4:  # 40% self-loops
#                 target = state
#             elif random.random() < 0.3:  # 30% final state
#                 target = final_state
#             else:  # 30% other non-final states
#                 target = random.choice(non_final_states)
                
#             weight = random.choice(self.return_values)
#             from_state = f"s{child_index}_{state}"
#             to_state = f"s{child_index}_{target}"
#             transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")
        
#         # Final state has NO outgoing transitions
        
#         return transitions
    
# def save_nested_automaton(automaton_text : str, filename : str, directory : str = "generated_nested") -> str:
#     """Save nested automaton to file"""
#     scrpit_dir = os.path.dirname(os.path.abspath(__file__))
#     output_dir = os.path.join(scrpit_dir, directory)
#     os.makedirs(output_dir, exist_ok=True)
    
#     file_path = os.path.join(output_dir, filename)
#     with open(file_path, 'w') as f:
#         f.write(automaton_text)
    
#     return file_path

# def parse_arguments():
#     parser = argparse.ArgumentParser(description='Generate nested word automata for QuAK testing')
    
#     # Required arguments
#     parser.add_argument('num_parent_states', type=int, 
#                        help='Number of states in the parent automaton')
#     parser.add_argument('num_children', type=int,
#                        help='Number of child automata (excluding dummy child 0)')
#     parser.add_argument('num_child_states', type=int,
#                        help='Number of states in each child automaton')
#     parser.add_argument('alphabet_size', type=int,
#                        help='Size of the input alphabet')
#     parser.add_argument('num_unique_return_values', type=int,
#                        help='Number of unique return values/weights')
#     parser.add_argument('value_function', choices=['Min_f', 'Max_f', 'SumB'],
#                         help='Value function type')

#     return parser.parse_args()

# def main():
#     args = parse_arguments()
    
#     # Validate arguments
#     if args.num_parent_states <= 0:
#         raise ValueError("Number of parent states must be positive")
#     if args.num_children <= 0:
#         raise ValueError("Number of children must be positive")
#     if args.num_child_states <= 0:
#         raise ValueError("Number of child states must be positive")
#     if args.alphabet_size <= 0:
#         raise ValueError("Alphabet size must be positive")
#     if args.num_unique_return_values <= 0:
#         raise ValueError("Number of unique return values must be positive")
#     #if args.min_weight >= args.max_weight:
#         raise ValueError("Min weight must be less than max weight")
    
#     config = NestedAutomatonConfig(
#         num_parent_states=args.num_parent_states,
#         parent_is_deterministic = True,
#         num_children=args.num_children,
#         num_child_states=args.num_child_states,
#         child_is_deterministic = True,
#         alphabet_size=args.alphabet_size,
#         num_unique_return_values=args.num_unique_return_values,
#         value_function=args.value_function
#         #min_weight=args.min_weight,
#         #max_weight=args.max_weight
#     )
    
#     generator = NestedAutomatonGenerator(config)
#     automaton = generator.generate()
    
#     print("Generated NWA:")
#     print("=" * 50)
#     print(automaton)
    
#     # Save to file
#     filename = f"nested_{config.num_parent_states}p_{config.num_children}c_{config.num_child_states}cs.txt"
#     saved_path = save_nested_automaton(automaton, filename)
#     print (f"\nSaved to: {saved_path}")
    
# if __name__ == "__main__":
#     main()

        

        


# import os
# import random
# import string
# import argparse
# from typing import List

# class NestedAutomatonConfig:
#     def __init__(self, num_parent_states: int, parent_is_deterministic: bool,
#                  num_children: int, num_child_states: int, child_is_deterministic: bool,
#                  alphabet_size: int, num_unique_return_values: int, value_function: str,
#                  min_weight: int = 0, max_weight: int = 10):
#         # Parent automaton
#         self.num_parent_states = num_parent_states
#         self.parent_is_deterministic = parent_is_deterministic

#         # Child automata
#         self.num_children = num_children
#         self.num_child_states = num_child_states
#         self.child_is_deterministic = child_is_deterministic

#         # Return values and alphabet
#         self.alphabet_size = alphabet_size
#         self.num_unique_return_values = num_unique_return_values
#         self.value_function = value_function
#         self.min_weight = min_weight
#         self.max_weight = max_weight

# def generate_alphabet(size: int) -> List[str]:
#     if size <= 26:
#         return list(string.ascii_lowercase[:size])
#     else:
#         return [f"a{i+1}" for i in range(size)]

# class NestedAutomatonGenerator:
#     def __init__(self, config: NestedAutomatonConfig):
#         self.config = config
#         self.alphabet = generate_alphabet(config.alphabet_size)
#         self.return_values = self._generate_return_values()

#     def _generate_return_values(self) -> List[int]:
#         # Integer weights, unique
#         values = set()
#         lo, hi = self.config.min_weight, self.config.max_weight
#         while len(values) < self.config.num_unique_return_values:
#             values.add(random.randint(lo, hi))
#         return sorted(list(values))

#     def generate(self) -> str:
#         """Generate the complete nested automaton as a string"""
#         result = []

#         # Generate parent
#         result.append("@PARENT")
#         result.extend(self._generate_parent())
#         result.append("")

#         # Generate dummy child 0
#         result.append("@CHILD 0")
#         result.append("# Dummy child automaton (always returns SILENT)")
#         result.append("")

#         # Generate actual children
#         for i in range(1, self.config.num_children + 1):
#             result.append(f"@CHILD {i}")
#             result.extend(self._generate_child(i))
#             result.append("")

#         return "\n".join(result)

#     def _generate_parent(self) -> List[str]:
#         lines = []

#         # --- NEW: random non-empty set of final parent states ---
#         all_states = list(range(self.config.num_parent_states))
#         k = random.randint(1, self.config.num_parent_states)  # at least one final
#         finals = sorted(random.sample(all_states, k))
#         final_names = [f"q{i}" for i in finals]
#         lines.append(f"final: {' '.join(final_names)}")

#         transitions = []

#         for state in range(self.config.num_parent_states):
#             for symbol in self.alphabet:
#                 # Force at least 70% of transitions to be non-silent
#                 if random.random() < 0.7:
#                     child_index = random.randint(1, self.config.num_children)  # Exclude 0 (SILENT)
#                 else:
#                     child_index = 0  # SILENT

#                 target_state = random.randint(0, self.config.num_parent_states - 1)

#                 if child_index == 0:
#                     transitions.append(f"{symbol} : SILENT, q{state} -> q{target_state}")
#                 else:
#                     transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")

#         # If non-deterministic, add extra transitions
#         if not self.config.parent_is_deterministic:
#             extra_transitions = random.randint(0, len(transitions) // 2)
#             for _ in range(extra_transitions):
#                 state = random.randint(0, self.config.num_parent_states - 1)
#                 symbol = random.choice(self.alphabet)

#                 if random.random() < 0.7:
#                     child_index = random.randint(1, self.config.num_children)
#                 else:
#                     child_index = 0

#                 target_state = random.randint(0, self.config.num_parent_states - 1)

#                 if child_index == 0:
#                     transitions.append(f"{symbol} : SILENT, q{state} -> q{target_state}")
#                 else:
#                     transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")

#         lines.extend(transitions)
#         return lines

#     def _generate_child(self, child_index: int) -> List[str]:
#         """Generate a single child automaton"""
#         lines = []

#         # Always use the last state as the single final state (sink)
#         final_state = self.config.num_child_states - 1
#         final_names = [f"s{child_index}_{final_state}"]
#         lines.append(f"final: {' '.join(final_names)}")

#         # Generate transitions
#         if self.config.child_is_deterministic:
#             lines.extend(self._generate_deterministic_child_transitions(child_index, final_state))
#         else:
#             lines.extend(self._generate_nondeterministic_child_transitions(child_index, final_state))

#         return lines

#     def _generate_deterministic_child_transitions(self, child_index: int, final_state: int) -> List[str]:
#         transitions = []

#         non_final_states = list(range(self.config.num_child_states - 1))  # Exclude final state

#         # Ensure reachability
#         reachable = {0}
#         unreachable = set(non_final_states[1:])

#         for state in non_final_states:
#             for symbol in self.alphabet:
#                 if unreachable and state in reachable:
#                     target = unreachable.pop()
#                     reachable.add(target)
#                 else:
#                     target_choices = []
#                     target_choices.extend([state] * 2)  # self-loop bias
#                     target_choices.extend(non_final_states)
#                     target_choices.append(final_state)
#                     target = random.choice(target_choices)

#                 weight = random.choice(self.return_values)  # integer
#                 from_state = f"s{child_index}_{state}"
#                 to_state = f"s{child_index}_{target}"
#                 transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

#         return transitions

#     def _generate_nondeterministic_child_transitions(self, child_index: int, final_state: int) -> List[str]:
#         transitions = []

#         non_final_states = list(range(self.config.num_child_states - 1))

#         reachable = {0}
#         unreachable = set(non_final_states[1:])

#         # Ensure reachability first
#         while unreachable:
#             unreachable_state = unreachable.pop()
#             from_state_id = random.choice(list(reachable))
#             symbol = random.choice(self.alphabet)
#             weight = random.choice(self.return_values)  # integer

#             from_state = f"s{child_index}_{from_state_id}"
#             to_state = f"s{child_index}_{unreachable_state}"
#             transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

#             reachable.add(unreachable_state)

#         # Add base transitions for all non-final states
#         for state in non_final_states:
#             for symbol in self.alphabet:
#                 target_choices = []
#                 target_choices.extend([state] * 3)          # self-loop bias
#                 target_choices.extend(non_final_states)
#                 target_choices.extend([final_state] * 2)    # bias to reach final

#                 target = random.choice(target_choices)
#                 weight = random.choice(self.return_values)  # integer
#                 from_state = f"s{child_index}_{state}"
#                 to_state = f"s{child_index}_{target}"
#                 transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

#         base_count = len(non_final_states) * len(self.alphabet)
#         extra_count = random.randint(0, base_count // 2)

#         for _ in range(extra_count):
#             state = random.choice(non_final_states)
#             symbol = random.choice(self.alphabet)

#             r = random.random()
#             if r < 0.4:
#                 target = state
#             elif r < 0.7:
#                 target = final_state
#             else:
#                 target = random.choice(non_final_states)

#             weight = random.choice(self.return_values)  # integer
#             from_state = f"s{child_index}_{state}"
#             to_state = f"s{child_index}_{target}"
#             transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

#         return transitions

# def save_nested_automaton(automaton_text: str, filename: str, directory: str = "generated_nested") -> str:
#     """Save nested automaton to file"""
#     script_dir = os.path.dirname(os.path.abspath(__file__))
#     output_dir = os.path.join(script_dir, directory)
#     os.makedirs(output_dir, exist_ok=True)

#     file_path = os.path.join(output_dir, filename)
#     with open(file_path, 'w') as f:
#         f.write(automaton_text)

#     return file_path

# def parse_arguments():
#     parser = argparse.ArgumentParser(description='Generate nested word automata for QuAK testing')

#     # Required arguments
#     parser.add_argument('num_parent_states', type=int,
#                         help='Number of states in the parent automaton')
#     parser.add_argument('num_children', type=int,
#                         help='Number of child automata (excluding dummy child 0)')
#     parser.add_argument('num_child_states', type=int,
#                         help='Number of states in each child automaton')
#     parser.add_argument('alphabet_size', type=int,
#                         help='Size of the input alphabet')
#     parser.add_argument('num_unique_return_values', type=int,
#                         help='Number of unique return values/weights')
#     parser.add_argument('value_function', choices=['Min_f', 'Max_f', 'SumB'],
#                         help='Value function type')

#     # --- NEW: integer weight range for children ---
#     parser.add_argument('--min_weight', type=int, default=0,
#                         help='Minimum integer transition weight in child automata (inclusive)')
#     parser.add_argument('--max_weight', type=int, default=10,
#                         help='Maximum integer transition weight in child automata (inclusive)')

#     return parser.parse_args()

# def main():
#     args = parse_arguments()

#     # Validate arguments
#     if args.num_parent_states <= 0:
#         raise ValueError("Number of parent states must be positive")
#     if args.num_children <= 0:
#         raise ValueError("Number of children must be positive")
#     if args.num_child_states <= 0:
#         raise ValueError("Number of child states must be positive")
#     if args.alphabet_size <= 0:
#         raise ValueError("Alphabet size must be positive")
#     if args.num_unique_return_values <= 0:
#         raise ValueError("Number of unique return values must be positive")

#     if args.min_weight > args.max_weight:
#         raise ValueError("min_weight must be <= max_weight")

#     # Ensure we can actually pick enough unique integer weights
#     available = args.max_weight - args.min_weight + 1
#     if args.num_unique_return_values > available:
#         raise ValueError(
#             f"Cannot generate {args.num_unique_return_values} unique integer weights "
#             f"from range [{args.min_weight}, {args.max_weight}] (only {available} available)"
#         )

#     config = NestedAutomatonConfig(
#         num_parent_states=args.num_parent_states,
#         parent_is_deterministic=False,
#         num_children=args.num_children,
#         num_child_states=args.num_child_states,
#         child_is_deterministic=True,
#         alphabet_size=args.alphabet_size,
#         num_unique_return_values=args.num_unique_return_values,
#         value_function=args.value_function,
#         min_weight=args.min_weight,
#         max_weight=args.max_weight
#     )

#     generator = NestedAutomatonGenerator(config)
#     automaton = generator.generate()

#     print("Generated NWA:")
#     print("=" * 50)
#     print(automaton)

#     filename = f"nested_{config.num_parent_states}p_{config.num_children}c_{config.num_child_states}cs.txt"
#     saved_path = save_nested_automaton(automaton, filename)
#     print(f"\nSaved to: {saved_path}")

# if __name__ == "__main__":
#     main()

        
        






# import os
# import random
# import string
# import itertools
# from typing import List


# class NestedAutomatonConfig:
#     def __init__(
#         self,
#         num_parent_states: int,
#         parent_is_deterministic: bool,
#         num_children: int,
#         num_child_states: int,
#         child_is_deterministic: bool,
#         alphabet_size: int,
#         num_unique_return_values: int,
#         value_function: str,
#         min_weight: int = 0,
#         max_weight: int = 10,
#     ):
#         self.num_parent_states = num_parent_states
#         self.parent_is_deterministic = parent_is_deterministic

#         self.num_children = num_children
#         self.num_child_states = num_child_states
#         self.child_is_deterministic = child_is_deterministic

#         self.alphabet_size = alphabet_size
#         self.num_unique_return_values = num_unique_return_values
#         self.value_function = value_function

#         self.min_weight = min_weight
#         self.max_weight = max_weight


# def generate_alphabet(size: int) -> List[str]:
#     if size <= 26:
#         return list(string.ascii_lowercase[:size])
#     return [f"a{i+1}" for i in range(size)]


# class NestedAutomatonGenerator:
#     def __init__(self, config: NestedAutomatonConfig):
#         self.config = config
#         self.alphabet = generate_alphabet(config.alphabet_size)
#         self.return_values = self._generate_return_values()

#     def _generate_return_values(self) -> List[int]:
#         # Integer weights, unique
#         values = set()
#         lo, hi = self.config.min_weight, self.config.max_weight
#         while len(values) < self.config.num_unique_return_values:
#             values.add(random.randint(lo, hi))
#         return sorted(values)

#     def generate(self) -> str:
#         result = []

#         result.append("@PARENT")
#         result.extend(self._generate_parent())
#         result.append("")

#         result.append("@CHILD 0")
#         result.append("# Dummy child automaton (always returns SILENT)")
#         result.append("")

#         for i in range(1, self.config.num_children + 1):
#             result.append(f"@CHILD {i}")
#             result.extend(self._generate_child(i))
#             result.append("")

#         return "\n".join(result)

#     def _generate_parent(self) -> List[str]:
#         lines = []

#         # Random non-empty set of final parent states
#         all_states = list(range(self.config.num_parent_states))
#         k = random.randint(1, self.config.num_parent_states)
#         finals = sorted(random.sample(all_states, k))
#         lines.append("final: " + " ".join(f"q{i}" for i in finals))

#         transitions = []

#         for state in range(self.config.num_parent_states):
#             for symbol in self.alphabet:
#                 # 70% chance of non-silent
#                 if random.random() < 0.7:
#                     child_index = random.randint(1, self.config.num_children)
#                 else:
#                     child_index = 0

#                 target_state = random.randint(0, self.config.num_parent_states - 1)
#                 transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")

#                 # if child_index == 0:
#                 #     transitions.append(f"{symbol} : SILENT, q{state} -> q{target_state}")
#                 # else:
#                 #     transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")

#         if not self.config.parent_is_deterministic:
#             extra_transitions = random.randint(0, len(transitions) // 2)
#             for _ in range(extra_transitions):
#                 state = random.randint(0, self.config.num_parent_states - 1)
#                 symbol = random.choice(self.alphabet)

#                 if random.random() < 0.7:
#                     child_index = random.randint(1, self.config.num_children)
#                 else:
#                     child_index = 0

#                 target_state = random.randint(0, self.config.num_parent_states - 1)
#                 transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")

#                 # if child_index == 0:
#                 #     transitions.append(f"{symbol} : SILENT, q{state} -> q{target_state}")
#                 # else:
#                 #     transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")

#         lines.extend(transitions)
#         return lines

#     def _generate_child(self, child_index: int) -> List[str]:
#         lines = []

#         final_state = self.config.num_child_states - 1
#         lines.append(f"final: s{child_index}_{final_state}")

#         if self.config.child_is_deterministic:
#             lines.extend(self._generate_deterministic_child_transitions(child_index, final_state))
#         else:
#             lines.extend(self._generate_nondeterministic_child_transitions(child_index, final_state))

#         return lines

#     def _generate_deterministic_child_transitions(self, child_index: int, final_state: int) -> List[str]:
#         transitions = []

#         non_final_states = list(range(self.config.num_child_states - 1))
#         reachable = {0}
#         unreachable = set(non_final_states[1:])

#         for state in non_final_states:
#             for symbol in self.alphabet:
#                 if unreachable and state in reachable:
#                     target = unreachable.pop()
#                     reachable.add(target)
#                 else:
#                     target_choices = []
#                     target_choices.extend([state] * 2)
#                     target_choices.extend(non_final_states)
#                     target_choices.append(final_state)
#                     target = random.choice(target_choices)

#                 weight = random.choice(self.return_values)
#                 from_state = f"s{child_index}_{state}"
#                 to_state = f"s{child_index}_{target}"
#                 transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

#         return transitions

#     def _generate_nondeterministic_child_transitions(self, child_index: int, final_state: int) -> List[str]:
#         transitions = []

#         non_final_states = list(range(self.config.num_child_states - 1))

#         reachable = {0}
#         unreachable = set(non_final_states[1:])

#         while unreachable:
#             unreachable_state = unreachable.pop()
#             from_state_id = random.choice(list(reachable))
#             symbol = random.choice(self.alphabet)
#             weight = random.choice(self.return_values)

#             from_state = f"s{child_index}_{from_state_id}"
#             to_state = f"s{child_index}_{unreachable_state}"
#             transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

#             reachable.add(unreachable_state)

#         for state in non_final_states:
#             for symbol in self.alphabet:
#                 target_choices = []
#                 target_choices.extend([state] * 3)
#                 target_choices.extend(non_final_states)
#                 target_choices.extend([final_state] * 2)

#                 target = random.choice(target_choices)
#                 weight = random.choice(self.return_values)
#                 from_state = f"s{child_index}_{state}"
#                 to_state = f"s{child_index}_{target}"
#                 transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

#         base_count = len(non_final_states) * len(self.alphabet)
#         extra_count = random.randint(0, base_count // 2)

#         for _ in range(extra_count):
#             state = random.choice(non_final_states)
#             symbol = random.choice(self.alphabet)

#             r = random.random()
#             if r < 0.4:
#                 target = state
#             elif r < 0.7:
#                 target = final_state
#             else:
#                 target = random.choice(non_final_states)

#             weight = random.choice(self.return_values)
#             from_state = f"s{child_index}_{state}"
#             to_state = f"s{child_index}_{target}"
#             transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

#         return transitions


# def save_nested_automaton(automaton_text: str, filename: str, directory: str = "generated_nested") -> str:
#     script_dir = os.path.dirname(os.path.abspath(__file__))
#     output_dir = os.path.join(script_dir, directory)
#     os.makedirs(output_dir, exist_ok=True)

#     file_path = os.path.join(output_dir, filename)
#     with open(file_path, "w") as f:
#         f.write(automaton_text)

#     return file_path


# def _is_valid_combo(num_unique_return_values: int, min_weight: int, max_weight: int) -> bool:
#     if min_weight > max_weight:
#         return False
#     available = max_weight - min_weight + 1
#     return num_unique_return_values <= available


# def main():
#     # Optional reproducibility:
#     SEED = None  # set to an int (e.g. 0) for reproducible generation
#     if SEED is not None:
#         random.seed(SEED)

#     # ------------------------------------------------------------
#     # Parameter grids (edit these lists as you like)
#     # All numerical parameters are lists, as requested.
#     # ------------------------------------------------------------
#     NUM_PARENT_STATES_LIST = [10, 20, 30, 40, 50]
#     NUM_CHILDREN_LIST = [2, 3, 4, 5, 6]
#     NUM_CHILD_STATES_LIST = [2, 4, 8, 16, 32]
#     ALPHABET_SIZE_LIST = [2]
#     NUM_UNIQUE_RETURN_VALUES_LIST = [2, 4, 8, 16]
#     MIN_WEIGHT_LIST = [0]
#     MAX_WEIGHT_LIST = [100]

#     # Non-numerical knobs (still lists so you can batch them too)
#     VALUE_FUNCTIONS = ["Max_f"] #"Min_f", "Max_f", "SumB"
#     PARENT_DETERMINISTIC_LIST = [False]   # set [True, False] if you want
#     CHILD_DETERMINISTIC_LIST = [True]    # set [True, False] if you want

#     # How many random automata per parameter combination
#     AUTOMATA_PER_COMBO = 30

#     # Safety cap to avoid accidental explosion
#     MAX_TOTAL_AUTOMATA = 500000

#     total_written = 0
#     skipped = 0

#     combos = itertools.product(
#         NUM_PARENT_STATES_LIST,
#         NUM_CHILDREN_LIST,
#         NUM_CHILD_STATES_LIST,
#         ALPHABET_SIZE_LIST,
#         NUM_UNIQUE_RETURN_VALUES_LIST,
#         MIN_WEIGHT_LIST,
#         MAX_WEIGHT_LIST,
#         VALUE_FUNCTIONS,
#         PARENT_DETERMINISTIC_LIST,
#         CHILD_DETERMINISTIC_LIST,
#     )

#     for (
#         num_parent_states,
#         num_children,
#         num_child_states,
#         alphabet_size,
#         num_unique_return_values,
#         min_weight,
#         max_weight,
#         value_function,
#         parent_det,
#         child_det,
#     ) in combos:
#         if total_written >= MAX_TOTAL_AUTOMATA:
#             break

#         if num_parent_states <= 0 or num_children <= 0 or num_child_states <= 0 or alphabet_size <= 0:
#             skipped += 1
#             continue

#         if not _is_valid_combo(num_unique_return_values, min_weight, max_weight):
#             skipped += 1
#             continue

#         config = NestedAutomatonConfig(
#             num_parent_states=num_parent_states,
#             parent_is_deterministic=parent_det,
#             num_children=num_children,
#             num_child_states=num_child_states,
#             child_is_deterministic=child_det,
#             alphabet_size=alphabet_size,
#             num_unique_return_values=num_unique_return_values,
#             value_function=value_function,
#             min_weight=min_weight,
#             max_weight=max_weight,
#         )

#         for rep in range(AUTOMATA_PER_COMBO):
#             if total_written >= MAX_TOTAL_AUTOMATA:
#                 break

#             gen = NestedAutomatonGenerator(config)
#             automaton = gen.generate()

#             # Unique-ish filename
#             # stamp = f"{random.getrandbits(32):08x}"
#             filename = (
#                 f"nested_p{num_parent_states}_c{num_children}_cs{num_child_states}_"
#                 f"A{alphabet_size}_R{num_unique_return_values}_W{min_weight}to{max_weight}_"
#                 f"vf{value_function}_pd{int(parent_det)}_cd{int(child_det)}_rep{rep}.txt"
#             )

#             saved_path = save_nested_automaton(automaton, filename)
#             total_written += 1

#             # Print a short log line so you see progress without dumping full automata
#             print(f"[{total_written}] wrote {saved_path}")

#             if total_written >= MAX_TOTAL_AUTOMATA:
#                 break

#     print("")
#     print("Done.")
#     print(f"Generated files: {total_written}")
#     print(f"Skipped combos: {skipped}")
#     print("Output directory: generated_nested")


# if __name__ == "__main__":
#     main()




import os
import random
import string
import itertools
from typing import List, Tuple


class NestedAutomatonConfig:
    def __init__(
        self,
        num_parent_states: int,
        parent_is_deterministic: bool,
        num_children: int,
        num_child_states: int,
        child_is_deterministic: bool,
        alphabet_size: int,
        num_unique_return_values: int,
        value_function: str,
        min_weight: int = 0,
        max_weight: int = 10,
    ):
        self.num_parent_states = num_parent_states
        self.parent_is_deterministic = parent_is_deterministic

        self.num_children = num_children
        self.num_child_states = num_child_states
        self.child_is_deterministic = child_is_deterministic

        self.alphabet_size = alphabet_size
        self.num_unique_return_values = num_unique_return_values
        self.value_function = value_function

        self.min_weight = min_weight
        self.max_weight = max_weight


def generate_alphabet(size: int) -> List[str]:
    if size <= 26:
        return list(string.ascii_lowercase[:size])
    return [f"a{i+1}" for i in range(size)]


class NestedAutomatonGenerator:
    def __init__(self, config: NestedAutomatonConfig):
        self.config = config
        self.alphabet = generate_alphabet(config.alphabet_size)
        self.return_values = self._generate_return_values()

    def _generate_return_values(self) -> List[int]:
        # FIX 1: Weights must be consecutive integers to avoid "gaps" error.
        # We generate a range starting from min_weight.
        return [self.config.min_weight + i for i in range(self.config.num_unique_return_values)]

    def generate(self) -> str:
        result = []

        result.append("@PARENT")
        result.extend(self._generate_parent())
        result.append("")

        result.append("@CHILD 0")
        result.append("# Dummy child automaton (always returns SILENT)")
        result.append("")

        for i in range(1, self.config.num_children + 1):
            result.append(f"@CHILD {i}")
            result.extend(self._generate_child(i))
            result.append("")

        return "\n".join(result)

    def _generate_parent(self) -> List[str]:
        lines = []

        # Random non-empty set of final parent states
        all_states = list(range(self.config.num_parent_states))
        k = random.randint(1, self.config.num_parent_states)
        finals = sorted(random.sample(all_states, k))
        lines.append("final: " + " ".join(f"q{i}" for i in finals))

        transitions = []
        
        # FIX 2: Ensure ALL non-dummy children are used at least once.
        
        # 1. Identify all available "slots" (state, symbol pairs)
        # We need to guarantee that every child index (1..num_children) is assigned to at least one slot.
        slots = [(q, s) for q in range(self.config.num_parent_states) for s in self.alphabet]
        random.shuffle(slots)
        
        mandatory_children = list(range(1, self.config.num_children + 1))
        
        # If there are fewer slots than children, we can't be deterministic OR cover everyone easily.
        # Ideally num_parent_states * alphabet_size >= num_children.
        # If not, we iterate slots and assign mandatory children, then random ones.
        
        slot_map = {} # (state, symbol) -> child_index

        # Assign mandatory children first
        for i, child_id in enumerate(mandatory_children):
            if i < len(slots):
                slot_map[slots[i]] = child_id
            else:
                # Fallback if we run out of slots (rare given the logic, but safe to handle)
                # Overwrite a random previous slot to ensure coverage? 
                # Or just ignore if configuration is physically impossible (1 state, 1 symbol, 10 children)
                pass 

        # Assign remaining slots randomly
        for i in range(len(mandatory_children), len(slots)):
            # 70% chance of non-silent (child > 0), 30% chance of silent (child 0)
            if random.random() < 0.7:
                c_idx = random.randint(1, self.config.num_children)
            else:
                c_idx = 0
            slot_map[slots[i]] = c_idx

        # Construct the transition strings based on the map
        for state in range(self.config.num_parent_states):
            for symbol in self.alphabet:
                child_index = slot_map.get((state, symbol), 0)
                target_state = random.randint(0, self.config.num_parent_states - 1)
                transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")

        # Add Non-deterministic transitions if requested
        if not self.config.parent_is_deterministic:
            extra_transitions = random.randint(0, len(transitions) // 2)
            for _ in range(extra_transitions):
                state = random.randint(0, self.config.num_parent_states - 1)
                symbol = random.choice(self.alphabet)
                
                # For extra transitions, we don't strictly need to force coverage 
                # because we already guaranteed it in the base transitions above.
                if random.random() < 0.7:
                    child_index = random.randint(1, self.config.num_children)
                else:
                    child_index = 0

                target_state = random.randint(0, self.config.num_parent_states - 1)
                transitions.append(f"{symbol} : {child_index}, q{state} -> q{target_state}")

        lines.extend(transitions)
        return lines

    def _generate_child(self, child_index: int) -> List[str]:
        lines = []

        final_state = self.config.num_child_states - 1
        lines.append(f"final: s{child_index}_{final_state}")

        if self.config.child_is_deterministic:
            lines.extend(self._generate_deterministic_child_transitions(child_index, final_state))
        else:
            lines.extend(self._generate_nondeterministic_child_transitions(child_index, final_state))

        return lines

    def _generate_deterministic_child_transitions(self, child_index: int, final_state: int) -> List[str]:
        transitions = []

        non_final_states = list(range(self.config.num_child_states - 1))
        # Ensure graph is somewhat connected/interesting
        reachable = {0}
        unreachable = set(non_final_states[1:])

        for state in non_final_states:
            for symbol in self.alphabet:
                if unreachable and state in reachable:
                    target = unreachable.pop()
                    reachable.add(target)
                else:
                    target_choices = []
                    target_choices.extend([state] * 2)
                    target_choices.extend(non_final_states)
                    target_choices.append(final_state)
                    target = random.choice(target_choices)

                weight = random.choice(self.return_values)
                from_state = f"s{child_index}_{state}"
                to_state = f"s{child_index}_{target}"
                transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

        return transitions

    def _generate_nondeterministic_child_transitions(self, child_index: int, final_state: int) -> List[str]:
        transitions = []

        non_final_states = list(range(self.config.num_child_states - 1))

        reachable = {0}
        unreachable = set(non_final_states[1:])

        # Ensure basic reachability
        while unreachable:
            unreachable_state = unreachable.pop()
            from_state_id = random.choice(list(reachable))
            symbol = random.choice(self.alphabet)
            weight = random.choice(self.return_values)

            from_state = f"s{child_index}_{from_state_id}"
            to_state = f"s{child_index}_{unreachable_state}"
            transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

            reachable.add(unreachable_state)

        # Fill randomly
        for state in non_final_states:
            for symbol in self.alphabet:
                target_choices = []
                target_choices.extend([state] * 3)
                target_choices.extend(non_final_states)
                target_choices.extend([final_state] * 2)

                target = random.choice(target_choices)
                weight = random.choice(self.return_values)
                from_state = f"s{child_index}_{state}"
                to_state = f"s{child_index}_{target}"
                transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

        # Add extra transitions for NFA
        base_count = len(non_final_states) * len(self.alphabet)
        extra_count = random.randint(0, base_count // 2)

        for _ in range(extra_count):
            state = random.choice(non_final_states)
            symbol = random.choice(self.alphabet)

            r = random.random()
            if r < 0.4:
                target = state
            elif r < 0.7:
                target = final_state
            else:
                target = random.choice(non_final_states)

            weight = random.choice(self.return_values)
            from_state = f"s{child_index}_{state}"
            to_state = f"s{child_index}_{target}"
            transitions.append(f"{symbol} : {weight}, {from_state} -> {to_state}")

        return transitions


def save_nested_automaton(automaton_text: str, filename: str, directory: str = "generated_nested") -> str:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_dir = os.path.join(script_dir, directory)
    os.makedirs(output_dir, exist_ok=True)

    file_path = os.path.join(output_dir, filename)
    with open(file_path, "w") as f:
        f.write(automaton_text)

    return file_path


def _is_valid_combo(num_unique_return_values: int, min_weight: int, max_weight: int) -> bool:
    if min_weight > max_weight:
        return False
    # Since we are now forcing consecutive integers, we just need to make sure 
    # the range fits within max_weight - min_weight.
    # Actually, if we just use min_weight + i, we only care that min + num_unique - 1 <= max
    if (min_weight + num_unique_return_values - 1) > max_weight:
        return False
    return True


def main():
    # Optional reproducibility:
    SEED = None 
    if SEED is not None:
        random.seed(SEED)

    # ------------------------------------------------------------
    # Parameter grids 
    # ------------------------------------------------------------
    NUM_PARENT_STATES_LIST = [10, 20, 30, 40, 50]
    NUM_CHILDREN_LIST = [2, 3, 4, 5, 6]
    NUM_CHILD_STATES_LIST = [2, 4, 8, 16, 32]
    ALPHABET_SIZE_LIST = [2]
    NUM_UNIQUE_RETURN_VALUES_LIST = [2, 4, 8, 16]
    MIN_WEIGHT_LIST = [0]
    MAX_WEIGHT_LIST = [100]

    VALUE_FUNCTIONS = ["Max_f"] 
    PARENT_DETERMINISTIC_LIST = [False]   
    CHILD_DETERMINISTIC_LIST = [True]    

    AUTOMATA_PER_COMBO = 30
    MAX_TOTAL_AUTOMATA = 500000

    total_written = 0
    skipped = 0

    combos = itertools.product(
        NUM_PARENT_STATES_LIST,
        NUM_CHILDREN_LIST,
        NUM_CHILD_STATES_LIST,
        ALPHABET_SIZE_LIST,
        NUM_UNIQUE_RETURN_VALUES_LIST,
        MIN_WEIGHT_LIST,
        MAX_WEIGHT_LIST,
        VALUE_FUNCTIONS,
        PARENT_DETERMINISTIC_LIST,
        CHILD_DETERMINISTIC_LIST,
    )

    for (
        num_parent_states,
        num_children,
        num_child_states,
        alphabet_size,
        num_unique_return_values,
        min_weight,
        max_weight,
        value_function,
        parent_det,
        child_det,
    ) in combos:
        if total_written >= MAX_TOTAL_AUTOMATA:
            break

        if num_parent_states <= 0 or num_children <= 0 or num_child_states <= 0 or alphabet_size <= 0:
            skipped += 1
            continue

        if not _is_valid_combo(num_unique_return_values, min_weight, max_weight):
            skipped += 1
            continue

        config = NestedAutomatonConfig(
            num_parent_states=num_parent_states,
            parent_is_deterministic=parent_det,
            num_children=num_children,
            num_child_states=num_child_states,
            child_is_deterministic=child_det,
            alphabet_size=alphabet_size,
            num_unique_return_values=num_unique_return_values,
            value_function=value_function,
            min_weight=min_weight,
            max_weight=max_weight,
        )

        for rep in range(AUTOMATA_PER_COMBO):
            if total_written >= MAX_TOTAL_AUTOMATA:
                break

            gen = NestedAutomatonGenerator(config)
            automaton = gen.generate()

            filename = (
                f"nested_p{num_parent_states}_c{num_children}_cs{num_child_states}_"
                f"A{alphabet_size}_R{num_unique_return_values}_W{min_weight}to{max_weight}_"
                f"vf{value_function}_pd{int(parent_det)}_cd{int(child_det)}_rep{rep}.txt"
            )

            saved_path = save_nested_automaton(automaton, filename)
            total_written += 1

            # Log every 50 to avoid clutter
            if total_written % 50 == 0:
                print(f"[{total_written}] wrote .../{filename}")

            if total_written >= MAX_TOTAL_AUTOMATA:
                break

    print("")
    print("Done.")
    print(f"Generated files: {total_written}")
    print(f"Skipped combos: {skipped}")
    print("Output directory: generated_nested")


if __name__ == "__main__":
    main()