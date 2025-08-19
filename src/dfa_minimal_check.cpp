/* ----------- DFA Monitor (S_i,j) minimality checker - IMPROVED VERSION ---------- */

#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <queue>

#include "Parser.h"
#include "Automaton.h"
#include "ChildAutomaton.h"
#include "NestedAutomaton.h"
#include "Set.h"
#include "Weight.h"
#include "utility.h"

/**
 * Helper function: Check if all states are reachable from initial state
 */
bool allStatesReachable(const ChildAutomaton* dfa) {
    if (!dfa || !dfa->getStates() || dfa->getStates()->size() == 0) {
        return true;
    }
    
    MapArray<State*>* states = dfa->getStates();
    MapArray<Symbol*>* alphabet = dfa->getAlphabet();
    State* initial = dfa->getInitial();
    
    if (!initial) {
        return false;
    }
    
    // BFS to find all reachable states
    SetStd<State*> reachable;
    std::queue<State*> worklist;
    
    reachable.insert(initial);
    worklist.push(initial);
    
    while (!worklist.empty()) {
        State* current = worklist.front();
        worklist.pop();
        
        if (!alphabet) continue;
        
        for (Symbol* symbol : *alphabet) {
            if (!symbol) continue;
            
            auto* successors = current->getSuccessors(symbol->getId());
            if (!successors) continue;
            
            for (Edge* edge : *successors) {
                if (!edge || !edge->getTo()) continue;
                
                State* successor = edge->getTo();
                
                if (!reachable.contains(successor)) {
                    reachable.insert(successor);
                    worklist.push(successor);
                }
            }
        }
    }
    
    bool all_reachable = (reachable.size() == states->size());
    
    #ifdef DEBUG
    std::cout << "Reachability check for '" << dfa->getName() << "': " 
              << reachable.size() << "/" << states->size() << " states reachable" << std::endl;
    
    if (!all_reachable) {
        std::cout << "Unreachable states:" << std::endl;
        for (size_t i = 0; i < states->size(); ++i) {
            State* state = states->at(i);
            if (!reachable.contains(state)) {
                std::cout << "  - " << state->getName() << std::endl;
            }
        }
    }
    #endif
    
    return all_reachable;
}

/**
 * Helper class to manage state pair comparisons efficiently using QuAK containers
 */
class StatePairTable {
private:
    MapArray<State*>* states;
    MapStd<State*, size_t> stateToIndex;
    std::vector<std::vector<bool>> distinguishable;
    size_t numStates;
    
public:
    StatePairTable(MapArray<State*>* stateArray) : states(stateArray) {
        if (!stateArray) {
            numStates = 0;
            return;
        }
        
        numStates = stateArray->size();
        
        // Build state index mapping using QuAK MapStd
        for (size_t i = 0; i < numStates; ++i) {
            State* state = stateArray->at(i);
            stateToIndex.insert(state, i);
        }
        
        // Initialize distinguishability table (lower triangular)
        distinguishable.resize(numStates);
        for (size_t i = 0; i < numStates; ++i) {
            distinguishable[i].resize(i, false);
        }
    }
    
    void setDistinguishable(State* s1, State* s2) {
        if (!s1 || !s2 || s1 == s2) return;
        
        if (!stateToIndex.contains(s1) || !stateToIndex.contains(s2)) return;
        
        size_t i = stateToIndex.at(s1);
        size_t j = stateToIndex.at(s2);
        
        // Ensure i > j for lower triangular matrix
        if (i < j) std::swap(i, j);
        if (i < numStates && j < i) {
            distinguishable[i][j] = true;
        }
    }
    
    bool areDistinguishable(State* s1, State* s2) const {
        if (!s1 || !s2) return true;
        if (s1 == s2) return false;
        
        if (!stateToIndex.contains(s1) || !stateToIndex.contains(s2)) return true;
        
        size_t i = stateToIndex.at(s1);
        size_t j = stateToIndex.at(s2);
        
        // Ensure i > j for lower triangular matrix
        if (i < j) std::swap(i, j);
        if (i < numStates && j < i) {
            return distinguishable[i][j];
        }
        return false;
    }
    
    State* getState(size_t index) const {
        return (index < numStates && states) ? states->at(index) : nullptr;
    }
    
    size_t size() const { return numStates; }
    
    // Get all indistinguishable pairs for debugging using QuAK containers
    SetStd<std::pair<State*, State*>> getIndistinguishablePairs() const {
        SetStd<std::pair<State*, State*>> pairs;
        for (size_t i = 1; i < numStates; ++i) {
            for (size_t j = 0; j < i; ++j) {
                if (!distinguishable[i][j]) {
                    pairs.insert(std::make_pair(states->at(i), states->at(j)));
                }
            }
        }
        return pairs;
    }
};

/**
 * Get the unique successor state for a given symbol (DFA property)
 * Returns nullptr if no successor or multiple successors (invalid DFA)
 */
State* getUniqueSuccessor(State* state, Symbol* symbol) {
    if (!state || !symbol) return nullptr;
    
    auto* successors = state->getSuccessors(symbol->getId());
    if (!successors || successors->size() == 0) {
        return nullptr; // No transition
    }
    
    if (successors->size() > 1) {
        #ifdef DEBUG
        std::cout << "Warning: State " << state->getName() 
                  << " has multiple successors for symbol " << symbol->getName()
                  << " (non-deterministic)" << std::endl;
        #endif
        return nullptr; // Non-deterministic - not a proper DFA
    }
    
    Edge* edge = *(successors->begin());
    return edge ? edge->getTo() : nullptr;
}

/**
 * Get the edge weight for a transition (for monitor DFAs)
 * Returns -1 if no transition exists
 */
weight_t getTransitionWeight(State* state, Symbol* symbol) {
    if (!state || !symbol) return weight_t(-1);
    
    auto* successors = state->getSuccessors(symbol->getId());
    if (!successors || successors->size() == 0) {
        return weight_t(-1); // No transition
    }
    
    if (successors->size() > 1) {
        return weight_t(-1); // Non-deterministic
    }
    
    Edge* edge = *(successors->begin());
    if (!edge || !edge->getWeight()) return weight_t(-1);
    
    return edge->getWeight()->getValue();
}

/**
 * Standard table-filling algorithm for computing distinguishable state pairs
 * Adapted for QuAK's container system
 */
bool allStatesDistinguishable(const ChildAutomaton* dfa) {
    MapArray<State*>* states = dfa->getStates();
    MapArray<Symbol*>* alphabet = dfa->getAlphabet();
    SetStd<State*>* finalStates = dfa->getFinalStates();
    
    if (!states || states->size() <= 1) {
        return true; // 0 or 1 states are trivially distinguishable
    }
    
    if (!alphabet || !finalStates) {
        return false; // Invalid DFA
    }
    
    StatePairTable table(states);
    size_t numStates = table.size();
    
    if (numStates <= 1) return true;
    
    // Phase 1: Mark pairs with different acceptance status as distinguishable
    for (size_t i = 0; i < numStates; ++i) {
        for (size_t j = 0; j < i; ++j) {
            State* si = table.getState(i);
            State* sj = table.getState(j);
            
            if (!si || !sj) continue;
            
            bool si_final = finalStates->contains(si);
            bool sj_final = finalStates->contains(sj);
            
            if (si_final != sj_final) {
                table.setDistinguishable(si, sj);
                
                #ifdef DEBUG
                std::cout << "Initially distinguishable (acceptance): " 
                         << si->getName() << " and " << sj->getName() << std::endl;
                #endif
            }
        }
    }
    
    // Phase 2: Iteratively find more distinguishable pairs
    bool changed;
    int iteration = 0;
    
    do {
        changed = false;
        iteration++;
        
        #ifdef DEBUG
        std::cout << "Table-filling iteration " << iteration << std::endl;
        #endif
        
        for (size_t i = 0; i < numStates; ++i) {
            for (size_t j = 0; j < i; ++j) {
                State* si = table.getState(i);
                State* sj = table.getState(j);
                
                if (!si || !sj) continue;
                
                // Skip if already marked as distinguishable
                if (table.areDistinguishable(si, sj)) {
                    continue;
                }
                
                // Check if any symbol distinguishes these states
                bool foundDistinguishingSymbol = false;
                
                for (Symbol* symbol : *alphabet) {
                    if (!symbol) continue;
                    
                    State* succ_i = getUniqueSuccessor(si, symbol);
                    State* succ_j = getUniqueSuccessor(sj, symbol);
                    
                    // Check for different transition existence
                    if ((succ_i == nullptr) != (succ_j == nullptr)) {
                        // One has transition, other doesn't
                        foundDistinguishingSymbol = true;
                        break;
                    }
                    
                    // Both have transitions - check weights (for monitor DFAs)
                    if (succ_i && succ_j) {
                        weight_t weight_i = getTransitionWeight(si, symbol);
                        weight_t weight_j = getTransitionWeight(sj, symbol);
                        
                        if (weight_i != weight_j) {
                            foundDistinguishingSymbol = true;
                            break;
                        }
                        
                        // Check if successors are already known to be distinguishable
                        if (succ_i != succ_j && table.areDistinguishable(succ_i, succ_j)) {
                            foundDistinguishingSymbol = true;
                            break;
                        }
                    }
                }
                
                if (foundDistinguishingSymbol) {
                    table.setDistinguishable(si, sj);
                    changed = true;
                    
                    #ifdef DEBUG
                    std::cout << "Newly distinguishable: " 
                             << si->getName() << " and " << sj->getName() << std::endl;
                    #endif
                }
            }
        }
    } while (changed);
    
    // Count indistinguishable pairs using QuAK containers
    auto indistinguishablePairs = table.getIndistinguishablePairs();
    
    #ifdef DEBUG
    std::cout << "Table-filling completed after " << iteration << " iterations" << std::endl;
    std::cout << "Distinguishability check for '" << dfa->getName() << "': " 
              << indistinguishablePairs.size() << " indistinguishable pairs found" << std::endl;
    
    if (!indistinguishablePairs.empty()) {
        std::cout << "Indistinguishable pairs:" << std::endl;
        for (const auto& pair : indistinguishablePairs) {
            std::cout << "  - " << pair.first->getName() 
                     << " and " << pair.second->getName() << std::endl;
        }
    }
    #endif
    
    return indistinguishablePairs.size() == 0;
}

/**
 * Main function: Check if DFA is minimal (all states reachable AND distinguishable)
 */
bool isMinimalDFA(const ChildAutomaton* dfa) {
    if (!dfa || !dfa->getStates() || dfa->getStates()->size() == 0) {
        return true; // Empty automaton is trivially minimal
    }
    
    #ifdef DEBUG
    std::cout << "\n--- Minimality Check for " << dfa->getName() << " ---" << std::endl;
    #endif
    
    // Step 1: Check reachability
    bool reachable = allStatesReachable(dfa);
    
    // Step 2: Check distinguishability (only if all states are reachable)
    bool distinguishable = reachable ? allStatesDistinguishable(dfa) : false;
    
    bool minimal = reachable && distinguishable;
    
    #ifdef DEBUG
    std::cout << "Reachable: " << (reachable ? "YES" : "NO") << std::endl;
    std::cout << "Distinguishable: " << (distinguishable ? "YES" : "NO") << std::endl;
    std::cout << "Minimal: " << (minimal ? "YES" : "NO") << std::endl;
    #endif
    
    return minimal;
}

/**
 * Additional utility: Get equivalence classes of states using QuAK containers
 */
SetStd<SetStd<State*>> getEquivalenceClasses(const ChildAutomaton* dfa) {
    SetStd<SetStd<State*>> equivalenceClasses;
    MapArray<State*>* states = dfa->getStates();
    
    if (!states || states->size() == 0) {
        return equivalenceClasses;
    }
    
    StatePairTable table(states);
    size_t numStates = table.size();
    
    // Run the distinguishability algorithm first to populate the table
    allStatesDistinguishable(dfa);
    
    // Build equivalence classes
    SetStd<State*> processed;
    
    for (size_t i = 0; i < numStates; ++i) {
        State* si = table.getState(i);
        if (!si || processed.contains(si)) continue;
        
        SetStd<State*> equivalenceClass;
        equivalenceClass.insert(si);
        processed.insert(si);
        
        // Find all states equivalent to si
        for (size_t j = i + 1; j < numStates; ++j) {
            State* sj = table.getState(j);
            if (!sj || processed.contains(sj)) continue;
            
            if (!table.areDistinguishable(si, sj)) {
                equivalenceClass.insert(sj);
                processed.insert(sj);
            }
        }
        
        equivalenceClasses.insert(equivalenceClass);
    }
    
    return equivalenceClasses;
}

/**
 * Enhanced analysis function for monitor minimality using QuAK framework
 */
void analyzeMonitorMinimality(const std::string& filepath, value_function_t finVal, weight_t bound = -1) {
    std::cout << "\n=== Monitor Minimality Analysis (Table-Filling Algorithm) ===" << std::endl;
    std::cout << "File: " << filepath << std::endl;
    std::cout << "Value function: ";
    switch(finVal) {
        case Min_f: std::cout << "Min_f"; break;
        case Max_f: std::cout << "Max_f"; break;
        case SumB: std::cout << "SumB"; break;
        default: std::cout << "Unknown"; break;
    }
    std::cout << std::endl;
    
    try {
        NestedAutomaton* nested = new NestedAutomaton(filepath);
        SetStd<weight_t> global_values = computeGlobalReturnValues(nested, finVal, bound);
        
        MapStd<MonitorKey, ChildAutomaton*> monitors;
        SetStd<State*> Q_S, F_S;
        constructMonitors(nested, global_values, monitors, Q_S, F_S, finVal, bound);
        
        std::cout << "\n--- Results ---" << std::endl;
        
        size_t total_monitors = monitors.size();
        size_t minimal_monitors = 0;
        size_t total_states = 0;
        size_t total_mergeable_states = 0;
        
        for (const auto& [key, monitor] : monitors) {
            size_t child_i = key.first;
            weight_t value_j = key.second;
            
            std::cout << "\nS_{" << child_i << "," << value_j << "}:" << std::endl;
            std::cout << "  States: " << monitor->getStates()->size() << std::endl;
            std::cout << "  Final states: " << monitor->getFinalStates()->size() << std::endl;
            
            bool is_minimal = isMinimalDFA(monitor);
            std::cout << "  Minimal: " << (is_minimal ? "✓ YES" : "❌ NO") << std::endl;
            
            if (is_minimal) {
                minimal_monitors++;
            } else {
                // Show equivalence classes for non-minimal DFAs
                auto equiv_classes = getEquivalenceClasses(monitor);
                std::cout << "  Equivalence classes: " << equiv_classes.size() << std::endl;
                
                size_t mergeable = monitor->getStates()->size() - equiv_classes.size();
                total_mergeable_states += mergeable;
                
                if (mergeable > 0) {
                    std::cout << "  Could reduce by: " << mergeable << " states" << std::endl;
                }
            }
            
            total_states += monitor->getStates()->size();
        }
        
        std::cout << "\n--- Summary ---" << std::endl;
        std::cout << "Total monitors: " << total_monitors << std::endl;
        std::cout << "Minimal monitors: " << minimal_monitors << "/" << total_monitors 
                  << " (" << (total_monitors > 0 ? 100.0 * minimal_monitors / total_monitors : 0) << "%)" << std::endl;
        std::cout << "Total states: " << total_states << std::endl;
        std::cout << "States that could be merged: " << total_mergeable_states << std::endl;
        std::cout << "Potential state reduction: " 
                  << (total_states > 0 ? 100.0 * total_mergeable_states / total_states : 0) << "%" << std::endl;
        std::cout << "Average states per monitor: " 
                  << (total_monitors > 0 ? (double)total_states / total_monitors : 0) << std::endl;
        
        // Cleanup
        for (const auto& [key, monitor] : monitors) {
            delete monitor;
        }
        delete nested;
        
    } catch (const std::exception& e) {
        std::cout << "Error during analysis: " << e.what() << std::endl;
    }
}