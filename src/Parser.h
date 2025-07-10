
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
	SetStd<std::string> states;
	SetStd<std::string> alphabet;
	SetSorted<weight_t> weights;
	SetStd<std::pair<std::pair<std::string, weight_t>,std::pair<std::string, std::string>>> edges;

	// for nested automata
	std::vector<Parser*> child_parsers;	// Not MapArray because we need dynamic growth as parsing goes on
	unsigned int max_child_index = 0;	
	
	Parser(weight_t min_domain, weight_t max_domain);
	Parser (std::string filename);
	Parser (std::string filename, MapStd<std::string, Symbol*>* symbol_register);
	~Parser();

	// for nested automata
	unsigned int getChildCount() const;
	Parser* getChildParser(unsigned int index) const;
	void switchToParentSection();
	void switchToChildSection(unsigned int child_index);

private:
	// for nested automata
	Parser* current_parser = nullptr;
	bool in_parent_section = true;

};

// Free function prototypes
void abort(std::string message);
bool detectNestedAutomaton(std::ifstream& file);
std::string readLine(std::string line, Parser* parser);
void readNonNestedFile(std::ifstream& file, Parser* parser);
void readDomain(std::string line, Parser* parser);
std::string readEdge(std::string line, Parser* parser);
void readFile(std::string filename, Parser* parser);
void readNestedFile(std::ifstream& file, Parser* parser);



#endif /* PARSER_H_ */