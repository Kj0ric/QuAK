#define PARSER_VERBOSE

#include <sstream>
#include <cstring> // errno
#include "utility.h"
#include "Parser.h"

std::string name = "";
int line_counter = 0;

/* ------------ CONSTRUCTORS & Destructor ----------- */
// Default
Parser::Parser() {
	initial = "";
	domain_defined = false;
	min_domain = 0;
	max_domain = 0;
	max_child_index = 0;
	current_parser = nullptr;
	in_parent_section = true;
	// Sets and vectors are default-initialized
}


// If weight domain range is predefined
Parser::Parser(weight_t min_domain, weight_t max_domain) :
		domain_defined(true),
		min_domain(min_domain),
		max_domain(max_domain)
{}

// Create a parser out of an automaton file
Parser::Parser(std::string filename) {
	//std::cout << filename << std::endl;
	readFile(filename, this);
}

// If alphabet is given (symbol_register)
// This is more efficient. Because symbol objects are reused in this way.
// For some operations (inclusion checking) alphabet must be the same (symbol objects). So they must share a common symbol_register
Parser::Parser(std::string path, MapStd<std::string, Symbol*>* symbol_register) {
	for (std::pair<std::string, Symbol*> pair : *symbol_register) {		// <string, symbol*> pairs <"a", Symbol for a>
		this->alphabet.insert(pair.first);
	}
	readFile(path, this);
}

Parser::~Parser() {
	delete_verbose("@Detail: 4 SetStd will be deleted (parser)\n");
}
/* ------------ CONSTRUCTORS & Destructor ----------- */


/* ------------ Main parsing functions ----------- */
void readFile (std::string filename, Parser* parser) {
	name = filename;
	line_counter = 0;
	std::ifstream file(name);

	if (file.is_open() == false) {
		std::cerr << "@Error: opening file " << name << std::endl;
		std::cerr << "Message: " << strerror(errno) << std::endl;
		fflush(stdout);fflush(stderr);
		exit(EXIT_FAILURE);
	}

	// Pre-scan file to detect if it's a nested automaton
	bool is_nested = detectNestedAutomaton(file);
	
	// Reset file to beginning
	file.clear();
	file.seekg(0, std::ios::beg);
	line_counter = 0;

	if (is_nested) {
		parser_verbose("The file is for a nested automaton.\n");
		readNestedFile(file, parser);
	} else {
		parser_verbose("The file is for a non-nested automaton.\n");
		readNonNestedFile(file, parser);
	}
	
	file.close();
}

void readNonNestedFile(std::ifstream& file, Parser* parser) {
	if (file.is_open() == false) {
		std::cerr << "@Error: opening file " << name << std::endl;
		std::cerr << "Message: " << strerror(errno) << std::endl;
		fflush(stdout);fflush(stderr);
		exit(EXIT_FAILURE);
	}
	std::string line;
	
	// Read the first transition (edge line) to get the initial state
	while (parser->initial == "" && getline(file, line)) {
		line_counter++;
		parser->initial = readLine(line, parser);
	}

	// Read the rest and update the Parser object
	while (getline(file, line)) { 
		line_counter++;
		readLine(line, parser);
	}

	if (parser->initial == "") abort("automaton without transitions");	// Means no edge line parsed
	
	// Compare domain declarations and actual weights used in transitions to decide on domain ranges
	if (parser->domain_defined == true) {
		parser->min_domain = std::min(parser->min_domain, parser->weights.getMin());
		parser->max_domain = std::max(parser->max_domain, parser->weights.getMax());
	}
	else {
		parser->min_domain = parser->weights.getMin();
		parser->max_domain = parser->weights.getMax();
	}
	file.close();
}

void readNestedFile(std::ifstream& file, Parser* parser) {
	/* Expected format: 
	@PARENT
	a : 1, q0 -> q0
	b : 0, q0 -> q1
	@CHILD 0
	final: sF
	a : 0, s0 -> sF
	b : 1, s0 -> sF
	@CHILD 1
	final: m2
	...
	*/
	std::string line;
	//SetStd<int> parent_indices;	// Keep parent and child indices encountered to check if they are equal
	//SetStd<int> child_indices;
	bool expect_first_parent_edge = false;
	bool expect_first_child_edge = false;

	while(std::getline(file, line)) {
		line_counter++;

		if (line.empty()) continue;
		
		// Delete comments
		size_t index = line.find('#');
		if (index != std::string::npos) {
			auto i = index;
			while (i < line.length()) {
				line[i++] = ' ';
			}
		}

		// Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

		// if (line.empty()) continue;

		// Section headers must start with @PARENT/CHILD
		if (line.rfind("@PARENT", 0) == 0) {
			parser->switchToParentSection();
			expect_first_parent_edge = true;
			continue;
		}
		
		if (line.rfind("@CHILD", 0) == 0) {	
			std::istringstream iss(line);
			std::string child_token; int child_index = -1;
			iss >> child_token >> child_index;
			if (child_token != "@CHILD" || iss.fail() || child_index < 0) {
				abort("Invalid @CHILD section header " + line);
			}
			// If no syntax errors then proceed
			//child_indices.insert(child_index);
			parser->switchToChildSection(child_index);

			// Read next line to parse the final states
			readFinalStates(file, parser, line_counter);
			
			expect_first_child_edge = true;
			continue;
		}

		if (parser->inParent()) {
			// TODO: Check for indices in parent transitions n >= 0
			if (expect_first_parent_edge) {
				// Parse initial state
				std::string from_state = readEdge(line,parser);
				parser->getCurrentParser()->initial = from_state;
				expect_first_parent_edge = false;
			} else {
				// TODO: Extract index from edge line
				readEdge(line, parser);
			}
		} else {
			if (expect_first_child_edge) {
				// Parse initial state
				std::string from_state = readEdge(line,parser);
				parser->getCurrentParser()->initial = from_state;
				expect_first_child_edge = false;
			} else {
				readEdge(line, parser);
			}
		}
	}

	// Check if all parent indices are matched with child indices
	for (unsigned int i = 0; i <= parser->max_child_index; i++ ) {
		if (parser->child_parsers.size() <= i || parser->child_parsers[i] == nullptr){
			abort("Missing @CHILD section for index N = " + std::to_string(i));
		}
	}
}

std::string readLine (std::string line, Parser* parser) {
	if (line.empty()) return "";

	size_t index = line.find("--");
	// If there is no "--" then it's a edge representation
	if (index == std::string::npos){
		return readEdge(line, parser);
	}
	// Else it's a domain range representation
	else {
		// Remove '--'
		line[index] = ' '; line[index+1] = ' ';
		readDomain(line, parser);
		return "";
	}
}

void readFinalStates(std::ifstream& file, Parser* parser, int line_counter) {
	std::string final_line;
	while (std::getline(file, final_line)) {
		line_counter++;

		// Remove comments and trim whitespace
		size_t comment_pos = final_line.find('#');
		if (comment_pos != std::string::npos) final_line = final_line.substr(0, comment_pos);
		final_line.erase(0, final_line.find_first_not_of(" \t"));
		final_line.erase(final_line.find_last_not_of(" \t") + 1);
		if (!final_line.empty()) break;
	}
	
	// Syntax check
	if (final_line.rfind("final:",0) != 0) {
		abort("Expected 'final:' line after @CHILD header, got: " + final_line);
	} 

	// Parse final states
	std::istringstream final_stream(final_line.substr(6));	// Skip "final:" prefix
	std::string final_state;
	parser->getCurrentParser()->final_states.clear();
	while (final_stream >> final_state) {
		parser->getCurrentParser()->final_states.insert(final_state);	// Store them in SetStd<std::string> states;
	}

}

// Parses a single line from the automata representation
// Does syntactic check on the automata representation
// Updates the Parser object
std::string readEdge (std::string line, Parser* parser) {
	// -- expected shape: symbol : weight, from -> to #comment
	size_t index;

	// Replace special char (:, ->) with " "
	index = line.find("->");
	if (index == std::string::npos) abort("edge without '->'");
	line[index] = ' '; line[index+1] = ' ';

	index = line.find(',');
	if (index == std::string::npos) abort("edge without ','");
	line[index] = ' ';

	index = line.find(':');
	if (index == std::string::npos) abort("edge without ':'");
	line[index] = ' ';

	index = line.find('#');
	if (index != std::string::npos) {
		auto i = index;
		while (i < line.length()) {
			line[i++] = ' ';
		}
	}

	// Now line has "symbol   weight   from   to     "
	std::istringstream buffer(line);

	// Get symbols, weight, states and update the Parser object
	// parser->getCurrentParser() returns a ptr to either parser itself or another parser for a child automaton
	std::string symbolname;
	buffer >> symbolname;
	if (symbolname.empty()) abort("transition without symbol");
	parser->getCurrentParser()->alphabet.insert(symbolname);
	parser_verbose("Parser: Symbol = '%s'\n", symbolname.c_str());

	std::string weightname;
	buffer >> weightname;
	if (weightname.empty()) abort("transition without weight");
	std::istringstream string_to_weight(weightname);
	weight_t weight;
    if (weightname.size() > 2 && weightname[1] == 'x' && weightname[0] == '0') {
        // the weight is given as a bitvector (unsigned number in hex)
	    if (weightname.size() > 10) abort("wrong 32-bit hex number");
        uint32_t tmp;
	    string_to_weight >> std::hex >> tmp;
        weight = weight_t::from_bv(tmp);
    } else {
	    string_to_weight >> weight;
    }
	parser->getCurrentParser()->weights.insert(weight);
	if (string_to_weight.eof() == false)
        abort("invalid weight: " + weightname);
	parser_verbose("Parser: Weight = '%s'\n", std::to_string(weight).c_str());

	std::string fromname;
	buffer >> fromname;
	if (fromname.empty()) abort("transition without source state");
	parser->getCurrentParser()->states.insert(fromname);
	parser_verbose("Parser: From = '%s'\n", fromname.c_str());

	std::string toname;
	buffer >> toname;
	if (toname.empty()) abort("transition without destination state");
	parser->getCurrentParser()->states.insert(toname);
	parser_verbose("Parser: To = '%s'\n", toname.c_str());

	std::pair<std::pair<std::string, weight_t>,std::pair<std::string, std::string>> edge;
	edge.first.first = symbolname;
	edge.first.second = weight;
	edge.second.first = fromname;
	edge.second.second = toname;
	parser->getCurrentParser()->edges.insert(edge);
	parser_verbose("Parser: Edge = %s -- %s : %f --> %s\n", fromname.c_str(), symbolname.c_str(), weight.to_float(), toname.c_str());

	return fromname;	// Return from to use readEdge() to find the initial state.
}

void readDomain (std::string line, Parser* parser) {
	// -- expected shape: min_domain -- max_domain #comment
	size_t index;

	// Remove comments
	index = line.find('#');
	if (index != std::string::npos) {
		auto i = index;
		while (i < line.length()) {
			line[i++] = ' ';
		}
	}

	std::istringstream buffer(line);

	// Parse min weight
	std::string minname;
	buffer >> minname;
	if (minname.empty()) abort("domain without minimal value");
	std::istringstream string_to_min(minname);
	weight_t minweight;
	string_to_min >> minweight;
	if (string_to_min.eof() == false) abort("non-integer value");

	// You may ask where '--' went, we did not remove it here. It is removed in readLine before the call to readDomain.

	// Parse max weight
	std::string maxname;
	buffer >> maxname;
	if (maxname.empty()) abort("domain without maximal value");
	std::istringstream string_to_max(maxname);
	weight_t maxweight;
	string_to_max >> maxweight;
	if (string_to_max.eof() == false) abort("non-integer value");
	parser_verbose("Parser: Domain = '%s' -- '%s'\n", std::to_string(minweight).c_str(), std::to_string(maxweight).c_str());

	// Update parser domain range (min and max)
	if (parser->domain_defined == true) {
		parser->min_domain = std::min(parser->min_domain, minweight);
		parser->min_domain = std::min(parser->min_domain, maxweight);
		parser->max_domain = std::max(parser->max_domain, minweight);
		parser->max_domain = std::max(parser->max_domain, maxweight);
	}
	else {
		parser->min_domain = std::min(minweight, maxweight);
		parser->max_domain = std::max(minweight, maxweight);
		parser->domain_defined = true;
	}
}
/* ------------ Main parsing functions ----------- */


/* ------------ HELPERS ----------- */
unsigned int Parser::getChildCount() const {
	return child_parsers.size();
}

// Returns a pointer to the child parser at given index
Parser* Parser::getChildParser(unsigned int index) const {
	if (index >= child_parsers.size()) return nullptr;
	return child_parsers[index];
}

// Function to switch the internal current_parser to Parent 
void Parser::switchToParentSection() {
	current_parser = this;		// Parent data goes in the main parser
	this->in_parent_section = true;
}

// Function to switch the internal current_parser to Child 
// and create a Parser obj for the child automaton
void Parser::switchToChildSection(unsigned int index) {
	// Ensure enough space in child_parsers
	if (child_parsers.size() <= index) {
		child_parsers.resize(index + 1, nullptr);			// Resize and initialize new indices to null
	}
	if (child_parsers[index] == nullptr) {
		child_parsers[index] = new Parser();
		child_parsers[index]->alphabet = this->alphabet;	// Copy parent's alphabet to child
	}

	current_parser = child_parsers[index];
	in_parent_section = false;
	max_child_index = std::max(max_child_index, index);
}

bool Parser::inParent() const {
	return (in_parent_section);
}

Parser* Parser::getCurrentParser() const {
	return current_parser;
}

/* ------------ DEBUG ----------- */
void Parser::print(std::ostream& os) {
	os << "Initial state: " << initial << std::endl;
	os << "No final states for the Parent autmaton.";
	//for (auto it = final_states.begin(); it != final_states.end(); ++it) {os << *it << " ";}

	os << std::endl << "States: ";
	for (auto it = states.begin(); it != states.end(); ++it) {os << *it << " ";}

	os << std::endl << "Alphabet: ";
	for (auto it = alphabet.begin(); it != alphabet.end(); ++it) {os << *it << " ";}

	os << std::endl << "Weights: ";
	for (auto it = weights.begin(); it != weights.end(); ++it) {os << *it << " ";}

	os << std::endl << "Edges: ";
	for (auto it = edges.begin(); it != edges.end(); ++it) {
		os << "  " << it->second.first << " -- " << it->first.first << " : " << it->first.second << " --> " << it->second.second << std::endl;
	}

	//os << std::endl;
	if (!child_parsers.empty()) {
        os << "Child parsers:\n";
        for (size_t i = 0; i < child_parsers.size(); ++i) {
            if (child_parsers[i]) {
                os << "Child " << i << ":\n";
                child_parsers[i]->print(os);
            }
		}	
	}
}

// Detect if the file being read is for nested automaton
bool detectNestedAutomaton(std::ifstream& file) {
	std::string line;
	while (std::getline(file, line)) {
		// Strip comments
		size_t comment_pos = line.find("#");
		if (comment_pos != std::string::npos) {
			line = line.substr(0, comment_pos);
		}

		if (line.empty()) continue;

		if (line.find("@PARENT") != std::string::npos) {
			return true;
		}
		// If encounter a transition before section header
		if (line.find("->") != std::string::npos || line.find("--") != std::string::npos) {
			return false;
		}
	}
	return false;
}

void abort(std::string message) {
	std::cerr << "@Error: parsing " << message.c_str() << std::endl;
	std::cerr << "File: " << name.c_str() << std::endl;
	std::cerr << "Line: " << line_counter << std::endl;
	fflush(stdout);fflush(stderr);
	exit(EXIT_FAILURE);
}
/* ------------ HELPERS ----------- */