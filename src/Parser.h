
#ifndef PARSER_H_
#define PARSER_H_

#include <string>
#include <fstream>
#include "Map.h"
#include "Set.h"

// Parses files (that describes automata), checks syntax, error handling
class Parser {
public:
	std::string initial = "";
	bool domain_defined = false;
	weight_t min_domain = 0;	// Min weight of the transitions of A
	weight_t max_domain = 0;	// Max weight of the transitions of A
	bool final_states_specified = false;
	bool final_states_all = false;
	
	std::vector<Parser*> child_parsers;	// Not MapArray because we need dynamic growth as parsing goes on

	unsigned int max_child_index = 0;	// Might not be necessary
	SetStd<std::string> final_states;
	
	SetStd<std::string> states;
	SetStd<std::string> alphabet;
	SetSorted<weight_t> weights;
	SetStd<std::pair<std::pair<std::string, weight_t>,std::pair<std::string, std::string>>> edges;

	Parser();	// Default constructor for child_parser when parsing a nested automaton
	Parser(weight_t min_domain, weight_t max_domain);
	Parser (std::string filename);
	Parser (std::string filename, MapStd<std::string, Symbol*>* symbol_register);
	~Parser();

	// Nested automata support
	unsigned int getChildCount() const;
	Parser* getChildParser(unsigned int index) const;
	Parser* getCurrentParser() const;
	void switchToParentSection();
	void switchToChildSection(unsigned int child_index);
	bool inParent() const;

	// Debug
	void print(std::ostream& os);

private:
	// Nested automata support
	Parser* current_parser = this;
	bool in_parent_section = true;
};

// Free function prototypes
void abort(std::string message);
bool detectNestedAutomaton(std::ifstream& file);
void readFile(std::string filename, Parser* parser);
void readNonNestedFile(std::ifstream& file, Parser* parser);
void readNestedFile(std::ifstream& file, Parser* parser);
void readFinalStates(std::ifstream& file, Parser* parser, int line_counter);
std::string readLine(std::string line, Parser* parser, bool allow_silent_weight = false);
std::string readEdge(std::string line, Parser* parser, bool allow_silent_weight = false);
void readDomain(std::string line, Parser* parser);

#endif /* PARSER_H_ */
