#include <sstream>
#include <cstring> // errno
#include "utility.h"
#include "Parser.h"

std::string name = "";
int line_counter = 0;

void abort(std::string message) {
	std::cerr << "@Error: parsing " << message.c_str() << std::endl;
	std::cerr << "File: " << name.c_str() << std::endl;
	std::cerr << "Line: " << line_counter << std::endl;
	fflush(stdout);fflush(stderr);
	exit(EXIT_FAILURE);
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
	std::string symbolname;
	buffer >> symbolname;
	if (symbolname.empty()) abort("transition without symbol");
	parser->alphabet.insert(symbolname);
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
	parser->weights.insert(weight);
	if (string_to_weight.eof() == false)
        abort("invalid weight: " + weightname);
	parser_verbose("Parser: Weight = '%s'\n", std::to_string(weight).c_str());

	std::string fromname;
	buffer >> fromname;
	if (fromname.empty()) abort("transition without source state");
	parser->states.insert(fromname);
	parser_verbose("Parser: From = '%s'\n", fromname.c_str());

	std::string toname;
	buffer >> toname;
	if (toname.empty()) abort("transition without destination state");
	parser->states.insert(toname);
	parser_verbose("Parser: To = '%s'\n", toname.c_str());

	std::pair<std::pair<std::string, weight_t>,std::pair<std::string, std::string>> edge;
	edge.first.first = symbolname;
	edge.first.second = weight;
	edge.second.first = fromname;
	edge.second.second = toname;
	parser->edges.insert(edge);
	parser_verbose("Parser: Edge = %s -- %s : %d --> %s\n", fromname.c_str(), symbolname.c_str(), weight, toname.c_str());

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


// If weight domain range is predefined
Parser::Parser(weight_t min_domain, weight_t max_domain) :
		domain_defined(true),
		min_domain(min_domain),
		max_domain(max_domain)
{}

// Create a parser out of an automaton file
Parser::Parser(std::string filename) {
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

