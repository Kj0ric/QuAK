
#include "Symbol.h"
#include "Automaton.h"
#include "utility.h"

unsigned int ID_of_Symbols = 0;
void Symbol::Symbol::RESET() { ID_of_Symbols = 0; }
void Symbol::Symbol::RESET(unsigned int n) { ID_of_Symbols = n; }


Symbol::~Symbol() {
	delete_verbose("@Memory: Symbol deleted (%s)\n", this->toString().c_str());
}

Symbol::Symbol(std::string name) : my_id(ID_of_Symbols++), name(name) {}


Symbol::Symbol(Symbol* symbol) : my_id(symbol->my_id), name(symbol->name) {}

Symbol::Symbol(const Symbol& other) : my_id(other.my_id), name(other.name) {}


std::string Symbol::getName() const { return this->name; }


unsigned int Symbol::getId() const { return this->my_id; }


std::string Symbol::Symbol::toString (Symbol* symbol) { return symbol->toString(); }


std::string Symbol::toString() const { return this->name; }

/* ----------------------------------------- */
MacroSymbol::MacroSymbol(Symbol* symbol, const std::vector<SetStd<Edge*>>& resolver)
    : symbol_(symbol), resolver_(resolver) {}

MacroSymbol::MacroSymbol(const MacroSymbol& other)
    : symbol_(other.symbol_), resolver_(other.resolver_) {}

MacroSymbol::~MacroSymbol() {
	// Don't delete symbol_ if it is shared with the original automaton
	// Don't delete edges in resolver_ if they are shared with the original automaton
}

bool MacroSymbol::operator==(const MacroSymbol &other) const {
	if (symbol_->getId() != other.symbol_->getId()) return false;
	if (resolver_.size() != other.resolver_.size()) return false;

	for (size_t i = 0; i < resolver_.size(); ++i) {
		if (resolver_[i].size() != other.resolver_[i].size()) return false;

		// Compare set of edges by their pointers
		for (Edge* edge : resolver_[i]) {
			if (!other.resolver_[i].contains(edge)) return false;
		}
	} 

	return true;
}

/* ------------- PSEUDO-DETERMINIZATION ------------- */

// Helper that generates all possible paths (creates a macroSymbol for each possible path) for a single symbol using resolver container 
void generateResolvers(
    const size_t symbol_id,
    const size_t automaton_id,
    const size_t state_id,
    std::vector<SetStd<Edge*>>& resolver,
    std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual>& alphabet,
    const std::vector<Automaton*>& automata_list,
    const std::vector<Symbol*>& symbol_list
) {
    // Base case: all automata have been processed for the current symbol
    if (automaton_id >= automata_list.size()) {
        // MacroSymbol* macroSymbol = new MacroSymbol(symbol_list[symbol_id], resolver);
        // alphabet.insert(macroSymbol);
        MacroSymbol* macroSymbol = new MacroSymbol(symbol_list[symbol_id], resolver);
        auto [it, inserted] = alphabet.insert(macroSymbol);
        if (!inserted) {
            delete macroSymbol; // avoid leak on duplicate
        }
        return;
    }

    // Move to the next automaton if all states of the current one are processed
    if (state_id >= automata_list[automaton_id]->getStates()->size()) {
        generateResolvers(symbol_id, automaton_id + 1, 0, resolver, alphabet, automata_list, symbol_list);
        return;
    }

    // Skip automata with empty alphabet (dummy children) For performance reasons
    if (automata_list[automaton_id]->getAlphabetSize() == 0) {
        generateResolvers(symbol_id, automaton_id + 1, 0, resolver, alphabet, automata_list, symbol_list);
        return;
    }

    // Skip to next state if symbol_id is not valid for this automaton's alphabet
    if (symbol_id >= automata_list[automaton_id]->getAlphabetSize()) {
        generateResolvers(symbol_id, automaton_id, state_id + 1, resolver, alphabet, automata_list, symbol_list);
        return;
    }

    State* current_state = automata_list[automaton_id]->getStates()->at(state_id);
    // DEBUG: print automaton info
    // std::cout << "Automaton " << automaton_id << " alphabet size: " 
    //           << automata_list[automaton_id]->getAlphabetSize() 
    //           << " symbol_id: " << symbol_id << std::endl;
    SetStd<Edge*>* successors = current_state->getSuccessors(symbol_id);

    if (successors && successors->size() != 0) {
        for (Edge* edge : *successors) {
            resolver[automaton_id].insert(edge);
            generateResolvers(symbol_id, automaton_id, state_id + 1, resolver, alphabet, automata_list, symbol_list);
            resolver[automaton_id].erase(edge); // Backtrack
        }
    } else {
        // No successors, just move to the next state
        generateResolvers(symbol_id, automaton_id, state_id + 1, resolver, alphabet, automata_list, symbol_list);
    }
}

// Create macroSymbols for each possible path with each symbol in the alphabet
void generateMacro(
    std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual >& alphabet,
    const std::vector<Automaton*>& automata_list,
    const std::vector<Symbol*>& symbol_list
) {
    for (size_t symbol_id = 0; symbol_id < symbol_list.size(); ++symbol_id) {
        // Create a fresh resolver for each symbol
        std::vector<SetStd<Edge*>> resolver(automata_list.size());
        generateResolvers(symbol_id, 0, 0, resolver, alphabet, automata_list, symbol_list);
    }
}
