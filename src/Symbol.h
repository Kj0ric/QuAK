
#ifndef SYMBOL_H_
#define SYMBOL_H_

#include <string>
#include <unordered_set>
#include <vector>
#include "Set.h"
#include "State.h"


class Symbol {
private:
	const unsigned int my_id;
	std::string name;
public:
	Symbol(std::string name);
	Symbol(Symbol* symbol);
	Symbol(const Symbol& other);
	~Symbol();
	std::string getName() const;
	unsigned int getId() const;
	static void RESET();
	static void RESET(unsigned int n);
	static std::string toString(Symbol* symbol);
	std::string toString() const;
};

class Edge;	// Forward declaration

class MacroSymbol {
private: 
    Symbol* symbol_;
    std::vector<SetStd<Edge*>> resolver_;
public:
    // Destructor
    ~MacroSymbol();
    // Constructor
    MacroSymbol(Symbol* symbol, const std::vector<SetStd<Edge*>>& resolver);
    // Copy constructor
	MacroSymbol(const MacroSymbol& other);

    // Equality == operator
    bool operator==(const MacroSymbol& other) const;

	// Getters
	Symbol* getSymbol() const { return symbol_; }
	const std::vector<SetStd<Edge*>>& getResolver() const { return resolver_; }

};

struct MacroSymbolHash {
    std::size_t operator()(const MacroSymbol& ms) const {
        std::size_t seed = std::hash<unsigned int>{}(ms.getSymbol()->getId());
        
        // Hash resolver size with mixing
        seed ^= std::hash<size_t>{}(ms.getResolver().size()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        
        // Hash each resolver set
        for (size_t i = 0; i < ms.getResolver().size(); ++i) {
            // Hash set size
            seed ^= std::hash<size_t>{}(ms.getResolver()[i].size()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            
            // Hash edge pointers (order-independent)
            std::size_t set_hash = 0;
            for (Edge* edge : ms.getResolver()[i]) {
                set_hash ^= std::hash<uintptr_t>{}(reinterpret_cast<uintptr_t>(edge));
            }
            seed ^= set_hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        
        return seed;
    }
};

struct MacroSymbolPtrHash {
    std::size_t operator()(const MacroSymbol* ms) const {
        if (!ms) return 0;  // Handle nulls

        MacroSymbolHash hasher;
        return hasher(*ms);
    }
};

struct MacroSymbolPtrEqual {
    bool operator()(const MacroSymbol* lhs, const MacroSymbol* rhs) const {
        if (!lhs || !rhs) return lhs == rhs;    // Handle nulls
        return *lhs == *rhs;
    }
};

void generateResolvers(
    const size_t symbol_id,
    const size_t automaton_id,
    const size_t state_id,
    std::vector<SetStd<Edge*>>& resolver,
    std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual>& alphabet,
    const std::vector<Automaton*>& automata_list,
    const std::vector<Symbol*>& symbol_list
);

void generateMacro(
    std::unordered_set<MacroSymbol*, MacroSymbolPtrHash, MacroSymbolPtrEqual >& alphabet,
    const std::vector<Automaton*>& automata_list,
    const std::vector<Symbol*>& symbol_list
);


#endif /* SYMBOL_H_ */
