// Objective: Constructing deterministic finite-word automaton S_i,j that recognizes the words on which the child automaton B_i 
// returns the value j.

// Type of function: ChildAutomaton class member function
// Return: pointer to the newly constructed ChildAutomaton object
// Parameters: integer j value

DeterminiseToS_ij(integer j):
    /* CHECKLIST: 
    + name 
    + states: state_map_DFA
    + initial
    + alphabet: same as the child automaton B_i
    + weights: Each weight is 0 or 1, since S_ij is a boolean automata
    + min_domain, max_domain: min is 0 and max is 1 since S_ij is a boolean automata
    + final_states_: Power set of F states with value j
    */

    abbreviate Subset set of states
    abbreviate Pair as {Subset, integer} pair

    state_map_DFA is a mapping from Pair to States of the DFA
    worklist is a queue of Pair objects

    // Create initial state of S_i,j
    initial_subset = {initial_state_Bi}
    initial_value = 0   // Depends on the FinVal function
    initial_pair = {initial_subset, initial_value}
    new_state = new State(
        name = "DFA_0", ...
    )
    new_state.setDFAValue(initial_value)
    
    //  Map initial state of S_ij
    state_map_DFA.insert(initial_pair, new_state) 
    worklist.push(initial_pair)

    // Explore reachable DFA states and map them
    while (worklist.empty() != true):
        current = worklist.front()
        worklist.pop()
        subset = current.first
        value = current.second

        for (symbol in alphabet):
            next_subset = empty set
            for state in subset:
                for s in state->getSuccessors(symbol):
                    next_subset.insert(s)
            next_value = value // update value if necessary
            next_pair = {next_subset, next_value}

            // If this pair is a new DFA state, then add it to DFA states
            if (state_map_DFA.contains(next_pair) != true):
                new_state = new State(
                    name = "DFA_" + state_map_DFA.size(),
                    ...
                )
                new_state.setDFAValue(next_value)
                state_map_DFA.insert(next_pair, new_state) 
                worklist.push(next_pair)    // Queue it to discover later
            
            // Add edge from current DFA state to next DFA state
            from_state= state_map_DFA[current]
            to_state = state_map_DFA[next_pair]
            if to_state is accepting:
                weight = 1
            else:
                weight = 0
            edge = new Edge(symbol, weight, from_state, to_state) 
            from_state.addSuccessor(edge)
            to_state.addPredecessor(edge)
    
        /* - Automaton pointer will be set in AppropriateStates() 
        - my_id is set automatically by the State constructor
        - min_weight and max_weight will be set in ???
        */
    
    // Set alphabet as the same as of B_i
    dfa_alphabet = alphabet

    // Set min and max domain as 0 and 1
    /* WHAT IF ALL TRANSITIONS ARE ACCEPTING? or NONE OF THEM ARE ACCEPTING */
    dfa_min = 0
    dfa_max = 1

    // Set name for DFA
    dfa_name = "S_{" + name + "," + j + "}"

    // Set final states
    for each (Pair, State) in state_map_DFA:
        subset = Pair.first
        value = Pair.second
        if (value == j) and (subset \in final_states_Bi):
            final_states_DFA.insert(State)
            
    
    // Construct S_ij
    
    return s_ij









            





    




