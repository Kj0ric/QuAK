#include <iostream>
#include <fstream>
#include <memory>
#include <cstring>
#include <variant>
#include <ctime>

#include "Automaton.h"
#include "NestedAutomaton.h"
#include "Map.h"
#include "Monitor.h"

#include "utils.h"

// Global debug flag for verbose output
bool g_debug_mode = false;

void monEvalTrace(Monitor *M, const std::string& trace);

static std::pair<unsigned, unsigned>
getAutomatonStats(const Automaton *A) {
    auto *states = A->getStates();
    unsigned n_states = states->size();
    unsigned n_edges = 0;

    auto *alphabet = A->getAlphabet();
    assert(alphabet);

    for (unsigned s = 0; s < n_states; ++s) {
        auto *state = states->at(s);
        assert(state);

        for (auto *symbol : *alphabet) {
            n_edges += state->getSuccessors(symbol->getId())->size();
        }
    }

    return {n_states, n_edges};
}

// Check if automatonfile contains @PARENT marker (indicating nested automaton)
static bool isNestedAutomatonFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("@PARENT") != std::string::npos) {
            return true;
        }
    }
    return false;
}

enum class Operation {
  INVALID,
  stats,
  dump,
  isEmpty,
  isNonempty,
  isUniversal,
  isIncluded,
  isIncludedBool,
  isEquivalent,
  isEquivalentBool,
  isConstant,
  isSafe,
  isLive,
  topValue,
  bottomValue,
  livenessComponent,
  safetyComponent,
  decompose,
  eval,
  monitor,
  nestedNonEmpty,
  nestedUniversal
};

static void printUsage(const char *bin) {
  std::cerr << "Usage: " << bin
            << " [-cputime] [-v] [-d] [-debug] [-print-witness] automaton-file" << " [ACTION ACTION ...]\n";
  std::cerr << "\nFor regular automata, ACTIONs are (VALF = <Inf | Sup | LimInf | LimSup | LimSupAvg | LimInfAvg>):\n";
  std::cerr << "  stats\n";
  std::cerr << "  dump\n";
  std::cerr << "  empty VALF <weight>\n";
  std::cerr << "  non-empty VALF <weight>\n";
  std::cerr << "  universal VALF <weight>\n";
  std::cerr << "  constant VALF\n";
  std::cerr << "  safe VALF\n";
  std::cerr << "  live VALF\n";
  std::cerr << "  isIncluded VALF automaton2-file\n";
  std::cerr << "  isIncludedBool VALF automaton2-file\n";
  std::cerr << "  isEquivalent VALF automaton2-file\n";
  std::cerr << "  isEquivalentBool VALF automaton2-file\n";
  std::cerr << "  livenessComponent VALF output-file\n";
  std::cerr << "  safetyComponent VALF output-file\n";
  std::cerr << "  decompose VALF safety-output-file liveness-output-file\n";
  std::cerr << "  eval <Inf | Sup | Avg> word-file\n";
  std::cerr << "  monitor <Inf | Sup | Avg> word-file\n";
  std::cerr << "  witness-file file-name\n";
  std::cerr << "\nFor nested automata (files with @PARENT), ACTIONs are:\n";
  std::cerr << "  FINVAL = <Max | Min | SumB | SumPlus | SumMinus>\n";
  std::cerr << "  non-empty VALF FINVAL <threshold> [bound]\n";
  std::cerr << "  universal VALF FINVAL <threshold> [bound]  (only Max/Min/SumB with Inf/Sup/LimInf/LimSup)\n";
  std::cerr << "\nThe action 'witness-file' instructs the previous action to store the witness into the given name.\n";
}

struct OperationClosure {
  Operation op{Operation::INVALID};
  std::vector<std::variant<std::string, weight_t, value_function_t>> args;
  std::string witness_file{};
};


struct Options {
  std::string automaton;
  std::vector<OperationClosure> actions;
  std::string error;
  bool cputime{false};
  bool verbose{false};
  bool dump{false};
  bool print_witness{false};
  bool isNested{false};
  std::string witness_file{};

  static Options createError(const std::string& err) {
    Options O;
    O.error = err;
    return O;
  }
};

static weight_t getWeight(const char *str, bool& succ) {
  char *endptr;
  static_assert(std::is_same<float, weight_t::T>::value,
                "Weights are not floats, fix the code");
  double val = strtof(str, &endptr);

  succ = endptr != str && *endptr == '\0';
  return val;
}


Options parseArgs(int argc, char *argv[]) {
#define streq(s, cs) (strncmp(s, cs, sizeof(cs)) == 0)

  int idx = 1;
  if (argc < 2) {
    return Options::createError("No automaton given");
  }

  Options O;

  while (idx < argc) {
    if (streq(argv[idx], "-cputime"))
      O.cputime = true;
    else if (streq(argv[idx], "-v"))
      O.verbose = true;
    else if (streq(argv[idx], "-d"))
      O.dump = true;
    else if (streq(argv[idx], "-print-witness"))
      O.print_witness = true;
    else if (streq(argv[idx], "-debug"))
      g_debug_mode = true;
    else if (argv[idx][0] != '-')
      break;

    ++idx;
  }

  if (idx < argc) {
    O.automaton = std::string(argv[idx++]);
  } else {
    return Options::createError("Invalid arguments, expected automaton file.");
  }

  // Detect if this is a nested automaton file early for proper arg parsing
  O.isNested = isNestedAutomatonFile(O.automaton);

  while (idx < argc) {
    OperationClosure cl;

    if (streq(argv[idx], "witness-file")) {
      if (O.actions.empty()) {
        return Options::createError("witness-file has no associated action.");
      }

      ++idx;
      if (idx >= argc) {
        return Options::createError("witness-file expects an argument (a file name).");
      }
      
      O.actions.back().witness_file = argv[idx];
      
      ++idx;
      continue;
    }

    if (streq(argv[idx], "stats")) {
      cl.op = Operation::stats;
    } else if (streq(argv[idx], "dump")) {
      cl.op = Operation::dump;
    } else if (streq(argv[idx], "empty")) {
      cl.op = Operation::isEmpty;
    } else if (streq(argv[idx], "non-empty")) {
      cl.op = Operation::isNonempty;
    } else if (streq(argv[idx], "universal")) {
      cl.op = Operation::isUniversal;
    } else if (streq(argv[idx], "constant")) {
      cl.op = Operation::isConstant;
    } else if (streq(argv[idx], "safe")) {
      cl.op = Operation::isSafe;
    } else if (streq(argv[idx], "live")) {
      cl.op = Operation::isLive;
    } else if (streq(argv[idx], "isIncluded")) {
      cl.op = Operation::isIncluded;
    } else if (streq(argv[idx], "isIncludedBool")) {
      cl.op = Operation::isIncludedBool;
    } else if (streq(argv[idx], "livenessComponent")) {
      cl.op = Operation::livenessComponent;
    } else if (streq(argv[idx], "safetyComponent")) {
      cl.op = Operation::safetyComponent;
    } else if (streq(argv[idx], "decompose")) {
      cl.op = Operation::decompose;
    } else if (streq(argv[idx], "isEquivalent")) {
      cl.op = Operation::isEquivalent;
    } else if (streq(argv[idx], "isEquivalentBool")) {
      cl.op = Operation::isEquivalentBool;
    } else if (streq(argv[idx], "monitor")) {
      cl.op = Operation::monitor;
    }


    if (cl.op == Operation::stats || cl.op == Operation::dump) {
         O.actions.push_back(cl);
         ++idx;
    } else if (cl.op == Operation::isNonempty ||
        cl.op == Operation::isEmpty ||
        cl.op == Operation::isUniversal) {
      
      // For nested automata: INFVAL FINVAL threshold [bound]
      // For regular automata: VALF threshold
      if (O.isNested) {
        if (idx + 3 >= argc) {
          return Options::createError("Nested automata require: " + std::string(argv[idx]) + " INFVAL FINVAL <threshold> [bound]");
        }
        // Parse aggregators first for validation
        value_function_t valf = getValueFunction(argv[idx + 1]);
        value_function_t finval = getFiniteAggregator(argv[idx + 2]);

        // Route to nested operation with proper validation
        if (cl.op == Operation::isNonempty) {
          cl.op = Operation::nestedNonEmpty;
        } else if (cl.op == Operation::isUniversal) {
          cl.op = Operation::nestedUniversal;
          // isUniversal only supports (Max_f|Min_f|SumB) x (Inf|LimInf|Sup|LimSup)
          if (finval == SumPlus || finval == SumMinus) {
            return Options::createError("Nested universal does not support SumPlus or SumMinus.");
          }
          if (valf == LimInfAvg || valf == LimSupAvg) {
            return Options::createError("Nested universal does not support LimInfAvg or LimSupAvg.");
          }
        } else {
          return Options::createError("Operation " + std::string(argv[idx]) + " not supported for nested automata.");
        }
        
        // for non-empty: SumMinus only supports LimInfAvg/LimSupAvg
        if (cl.op == Operation::nestedNonEmpty && finval == SumMinus) {
          if (valf != LimInfAvg && valf != LimSupAvg) {
            return Options::createError("SumMinus finite aggregator only supports LimInfAvg or LimSupAvg.");
          }
        }

        cl.args.push_back(valf);      // INFVAL
        cl.args.push_back(finval);    // FINVAL
        
        bool succ;
        weight_t threshold = getWeight(argv[idx + 3], succ);
        if (!succ) {
          return Options::createError("Invalid threshold: " + std::string(argv[idx + 3]));
        }
        cl.args.push_back(threshold);
        
        idx += 4;
        
        // Optional bound parameter
        if (idx < argc && argv[idx][0] != '-') {
          weight_t bound = getWeight(argv[idx], succ);
          if (succ) {
            cl.args.push_back(bound);
            ++idx;
          }
        }
        
        // Validation: SumB requires bound
        if (finval == SumB) {
            bool hasBound = (cl.args.size() > 3); // 0=valf, 1=finval, 2=threshold, 3=bound
            if (!hasBound) {
                return Options::createError("SumB aggregator requires a 'bound' argument.");
            }
        }

        O.actions.push_back(cl);
      } else {
        // Regular automaton parsing
        if (idx + 2 >= argc) {
          return Options::createError("Invalid arguments for " + std::string(argv[idx]));
        }

        cl.args.push_back(getValueFunction(argv[idx + 1]));

        bool succ;
        weight_t w = getWeight(argv[idx + 2], succ);
        if (!succ) {
          return Options::createError("Invalid weight: " + std::string(argv[idx + 2]));
        }
        cl.args.push_back(w);
        O.actions.push_back(cl);

        idx += 3;
      }
    } else if (cl.op == Operation::isConstant ||
               cl.op == Operation::isSafe ||
               cl.op == Operation::isLive) {
      if (idx + 1 >= argc) {
        return Options::createError("Invalid arguments for " + std::string(argv[idx]));
      }

      cl.args.push_back(getValueFunction(argv[idx + 1]));
      O.actions.push_back(cl);

      idx += 2;
    } else if (cl.op == Operation::isIncluded ||
               cl.op == Operation::isIncludedBool ||
               cl.op == Operation::isEquivalent ||
               cl.op == Operation::isEquivalentBool ||
               cl.op == Operation::livenessComponent ||
               cl.op == Operation::safetyComponent) {
      if (idx + 2 >= argc) {
        return Options::createError("Invalid arguments for " + std::string(argv[idx]));
      }

      cl.args.push_back(getValueFunction(argv[idx + 1]));
      cl.args.push_back(std::string(argv[idx + 2]));
      O.actions.push_back(cl);

      idx += 3;
    } else if (cl.op == Operation::decompose) {
      if (idx + 3 >= argc) {
        return Options::createError("Invalid arguments for " + std::string(argv[idx]));
      }

      cl.args.push_back(getValueFunction(argv[idx + 1]));
      cl.args.push_back(std::string(argv[idx + 2]));
      cl.args.push_back(std::string(argv[idx + 3]));
      O.actions.push_back(cl);

      idx += 4;
    } else if (cl.op == Operation::monitor) {
      if (idx + 2 >= argc) {
        return Options::createError("Invalid arguments for " + std::string(argv[idx]));
      }

      cl.args.push_back(getValueFunction(argv[idx + 1]));
      cl.args.push_back(std::string(argv[idx + 2]));
      O.actions.push_back(cl);

      idx += 3;
    }
    else {
      return Options::createError("Unknown action: " + std::string(argv[idx]));
    }
  }

#undef streq
  return O;
}

#define TIMER_INIT struct timespec start_time{0, 0}, end_time{0, 0};
#define TIMER_START if (opts.cputime) { clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time); }
#define TIMER_END if (opts.cputime) { clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time); }
#define TIMER_GET static_cast<uint64_t>((end_time.tv_sec * 1000) + (end_time.tv_nsec / 1000000.0))
#define TIMER_PRINT(msg) if (opts.cputime) { std::cout << msg << TIMER_GET << " ms\n"; }
#define PRINT_DIV { std::cout << "----------\n"; }

void processWitness(UltimatelyPeriodicWord *witness, OperationClosure& act, Options &opts, bool append=false) {
  if (!witness)
    return;

  if (opts.print_witness) {
    std::cout << "Witness: " << witness->toString() << "\n";
  }
  
  if (!act.witness_file.empty()) {
    if (opts.verbose)
      std::cerr << "Witness written to : " << act.witness_file << "\n";
      std::ofstream fl;
      if (append)
        fl.open(act.witness_file, std::ios_base::app);
      else
        fl.open(act.witness_file);

      fl << witness->toString() << "\n";
      fl.close();
  }
}

void writeAutomaton(Automaton *A, const std::string& path) {
	std::ofstream fl(path);
	A->write(fl);
}

int main(int argc, char **argv) {

    auto opts = parseArgs(argc, argv);
    if (!opts.error.empty()) {
      std::cerr << "ERROR: " << opts.error << "\n";
      printUsage(argv[0]);
      return -1;
    }   
    if (opts.actions.empty()) {
      std::cerr << "No actions given\n";
      printUsage(argv[0]);
      return -1;
    }   

    TIMER_INIT

    // Detect if this is a nested automaton file
    opts.isNested = isNestedAutomatonFile(opts.automaton);
    
    // Load the appropriate automaton type
    std::unique_ptr<Automaton> A;
    std::unique_ptr<NestedAutomaton> NA;
    
    TIMER_START
    if (opts.isNested) {
        NA = std::unique_ptr<NestedAutomaton>(new NestedAutomaton(opts.automaton));
        A.reset(NA.get());  // A points to NA for common operations
        A.release();        // Release ownership, NA owns it
    } else {
        A = std::unique_ptr<Automaton>(new Automaton(opts.automaton));
    }
    TIMER_END
    TIMER_PRINT("Cputime of building the automaton: ")

    if (opts.verbose) {
      if (opts.isNested) {
        std::cout << "Detected nested automaton with " << NA->getChildrenSize() << " children.\n";
      }
      Automaton* baseA = opts.isNested ? static_cast<Automaton*>(NA.get()) : A.get();
      unsigned n_states, n_edges;
      std::tie(n_states, n_edges) = getAutomatonStats(baseA);
      std::cout << "Input automaton has " << n_states
                << " states and " << n_edges << " edges.\n";
    }

    if (opts.dump) {
      if (opts.isNested) {
        NA->print();
      } else {
        A->print();
      }
    }

    PRINT_DIV

    value_function_t value_fun;
    value_function_t fin_agg;  // Finite aggregator for nested automata
    weight_t weight;


    for (auto& act : opts.actions) {
      UltimatelyPeriodicWord *witness = nullptr;
      switch (act.op) {
      case Operation::stats:
        {
          unsigned n_states, n_edges;
          std::tie(n_states, n_edges) = getAutomatonStats(A.get());
          std::cout << "Input automaton has " << n_states
                    << " states and " << n_edges << " edges.\n";
          PRINT_DIV
        }
        break;
      case Operation::dump:
        {
           A->print();
          PRINT_DIV
        }
        break;
      case Operation::isEmpty:
        value_fun = std::get<value_function_t>(act.args[0]);
        weight = std::get<weight_t>(act.args[1]);
        std::cout << "isEmpty("
                  << valueFunctionToStr(value_fun)
                  << ", weight=" << weight << ") = ";
        {
        bool r;
        if (opts.print_witness || !act.witness_file.empty()) {
          TIMER_START
          r = !A->isNonEmpty(value_fun, weight, &witness);
          TIMER_END
        } else {
          TIMER_START
          r = !A->isNonEmpty(value_fun, weight);
          TIMER_END
        }
        std::cout << r << "\n";
        processWitness(witness, act, opts);
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }
        break;

      case Operation::isNonempty:
        value_fun = std::get<value_function_t>(act.args[0]);
        weight = std::get<weight_t>(act.args[1]);
        std::cout << "isNonEmpty("
                  << valueFunctionToStr(value_fun)
                  << ", weight=" << weight << ") = ";
        {
        bool r;
        if (opts.print_witness || !act.witness_file.empty()) {
          TIMER_START
          r = A->isNonEmpty(value_fun, weight, &witness);
          TIMER_END
        } else {
          TIMER_START
          r = A->isNonEmpty(value_fun, weight);
          TIMER_END
        }
        std::cout << r << "\n";
        processWitness(witness, act, opts);
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }
        break;

      case Operation::isUniversal:
        value_fun = std::get<value_function_t>(act.args[0]);
        weight = std::get<weight_t>(act.args[1]);
        std::cout << "isUniversal("
                  << valueFunctionToStr(value_fun)
                  << ", weight=" << weight << ") = ";
        {
        bool r;
        if (opts.print_witness || !act.witness_file.empty()) {
          TIMER_START
          r = A->isUniversal(value_fun, weight, &witness);
          TIMER_END
        } else {
          TIMER_START
          r = A->isUniversal(value_fun, weight);
          TIMER_END
        }
        std::cout << r << "\n";
        processWitness(witness, act, opts);
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }

        break;

      case Operation::isConstant:
        value_fun = std::get<value_function_t>(act.args[0]);
        std::cout << "isConstant("
                  << valueFunctionToStr(value_fun)
                  << ") = ";
        {
        bool r;
        if (opts.print_witness || !act.witness_file.empty()) {
          TIMER_START
          r = A->isConstant(value_fun, &witness);
          TIMER_END
        } else {
          TIMER_START
          r = A->isConstant(value_fun);
          TIMER_END
        }
        std::cout << r << "\n";
        processWitness(witness, act, opts);
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }
        break;

      case Operation::isSafe:
        value_fun = std::get<value_function_t>(act.args[0]);
        std::cout << "isSafe("
                  << valueFunctionToStr(value_fun)
                  << ") = ";
        {
        bool r;
        if (opts.print_witness || !act.witness_file.empty()) {
          TIMER_START
          r = A->isSafe(value_fun, &witness);
          TIMER_END
        } else {
          TIMER_START
          r = A->isSafe(value_fun);
          TIMER_END
        }
        std::cout << r << "\n";
        processWitness(witness, act, opts);
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }

        break;

      case Operation::isLive:
        value_fun = std::get<value_function_t>(act.args[0]);
        std::cout << "isLive("
                  << valueFunctionToStr(value_fun)
                  << ") = ";
        {
        bool r;
        if (opts.print_witness || !act.witness_file.empty()) {
          TIMER_START
          r = A->isLive(value_fun, &witness);
          TIMER_END
        } else {
          TIMER_START
          r = A->isLive(value_fun);
          TIMER_END
        }
        std::cout << r << "\n";
        processWitness(witness, act, opts);
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }
        break;

      case Operation::isIncluded:
      case Operation::isIncludedBool:
        {
        TIMER_START
        auto B = std::unique_ptr<Automaton>(
            new Automaton(std::get<std::string>(act.args[1]), A.get()));
        TIMER_END
        TIMER_PRINT("Cputime of building the right-hand side automaton: ")

        if (opts.dump) {
          B->print();
        }

        bool r;
        value_fun = std::get<value_function_t>(act.args[0]);
        std::cout << "isIncluded(";
        if (act.op == Operation::isIncludedBool)
          std::cout << "bool, ";
        std::cout << valueFunctionToStr(value_fun)
                  << ") = ";
        
        
        if (opts.print_witness || !act.witness_file.empty()) {
          TIMER_START
          r =  A->isIncludedIn(B.get(), value_fun,
                                    act.op == Operation::isIncludedBool,
                                    &witness);
          TIMER_END
        } else {
          TIMER_START
          r =  A->isIncludedIn(B.get(), value_fun,
                                    act.op == Operation::isIncludedBool);
          TIMER_END
        }
        std::cout << r << "\n";
        processWitness(witness, act, opts);
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }
        break;
      case Operation::isEquivalent:
      case Operation::isEquivalentBool:
        {
        TIMER_START
        auto B = std::unique_ptr<Automaton>(
            new Automaton(std::get<std::string>(act.args[1]), A.get()));
        TIMER_END
        TIMER_PRINT("Cputime of building the right-hand side automaton: ")

        if (opts.dump) {
          B->print();
        }

        bool r;
        value_fun = std::get<value_function_t>(act.args[0]);
        std::cout << "isEquivalent(";
        if (act.op == Operation::isEquivalentBool)
          std::cout << "bool, ";
        std::cout << valueFunctionToStr(value_fun)
                  << ") = ";

        if (opts.print_witness || !act.witness_file.empty()) {
          TIMER_START
          r =  A->isEquivalentTo(B.get(), value_fun,
                                    act.op == Operation::isEquivalentBool,
                                    &witness);
          TIMER_END
        } else {
          TIMER_START
          r =  A->isEquivalentTo(B.get(), value_fun,
                                    act.op == Operation::isEquivalentBool);
          TIMER_END
        }
        std::cout << r << "\n";
        processWitness(witness, act, opts);
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }
        break;
      case Operation::livenessComponent:
        {
        value_fun = std::get<value_function_t>(act.args[0]);
        TIMER_START
        auto liveA
          = std::unique_ptr<Automaton>(Automaton::livenessComponent(A.get(), value_fun));
        TIMER_END

        if (opts.dump) {
          std::cout << "Liveness component automaton:\n";
          liveA->print();
        }

        writeAutomaton(liveA.get(), std::get<std::string>(act.args[1]));
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }
        break;
      case Operation::safetyComponent:
        {
        value_fun = std::get<value_function_t>(act.args[0]);
        TIMER_START
        auto safeA
          = std::unique_ptr<Automaton>(Automaton::safetyClosure(A.get(), value_fun));
        TIMER_END

        if (opts.dump) {
          std::cout << "Safety component automaton:\n";
          safeA->print();
        }

        writeAutomaton(safeA.get(), std::get<std::string>(act.args[1]));
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }
        break;
      case Operation::decompose:
        {
        value_fun = std::get<value_function_t>(act.args[0]);
        TIMER_START
        auto liveA
          = std::unique_ptr<Automaton>(Automaton::livenessComponent(A.get(), value_fun));
        auto safeA
          = std::unique_ptr<Automaton>(Automaton::safetyClosure(A.get(), value_fun));
        TIMER_END

        if (opts.dump) {
          std::cout << "Safety component automaton:\n";
          safeA->print();
          std::cout << "Liveness component automaton:\n";
          liveA->print();
        }

        writeAutomaton(safeA.get(), std::get<std::string>(act.args[1]));
        writeAutomaton(liveA.get(), std::get<std::string>(act.args[2]));
        TIMER_PRINT("Cputime: ")
        PRINT_DIV
        }
        break;
      case Operation::monitor:
        value_fun = std::get<value_function_t>(act.args[0]);
        std::cout << "monitor("
                  << valueFunctionToStr(value_fun)
                  << ") =\n";

        {
          auto M = std::unique_ptr<Monitor>(
            new Monitor(A.get(), value_fun)
          );

          auto trace = std::get<std::string>(act.args[1]);
          TIMER_START
          monEvalTrace(M.get(), trace);
          TIMER_END
          TIMER_PRINT("Cputime (incl. prints): ")
          PRINT_DIV
        }
        break;
      
      // Nested automata operations
      case Operation::nestedNonEmpty:
        {
          if (!opts.isNested || !NA) {
            std::cerr << "Error: non-empty with nested arguments requires a nested automaton file\n";
            return -1;
          }
          value_fun = std::get<value_function_t>(act.args[0]);
          fin_agg = std::get<value_function_t>(act.args[1]);
          weight = std::get<weight_t>(act.args[2]);
          weight_t bound = -1;
          if (act.args.size() > 3) {
            bound = std::get<weight_t>(act.args[3]);
          }
          
          std::cout << "isNonEmpty("
                    << valueFunctionToStr(value_fun) << ", "
                    << finiteAggregatorToStr(fin_agg)
                    << ", threshold=" << weight;
          if (bound >= 0) std::cout << ", bound=" << bound;
          std::cout << ") = ";
          
          bool r;
          TIMER_START
          r = NA->isNonEmpty(value_fun, fin_agg, weight, bound);
          TIMER_END
          std::cout << r << "\n";
          TIMER_PRINT("Cputime: ")
          PRINT_DIV
        }
        break;

      case Operation::nestedUniversal:
        {
          if (!opts.isNested || !NA) {
            std::cerr << "Error: universal with nested arguments requires a nested automaton file\n";
            return -1;
          }
          value_fun = std::get<value_function_t>(act.args[0]);
          fin_agg = std::get<value_function_t>(act.args[1]);
          weight = std::get<weight_t>(act.args[2]);
          weight_t bound = -1;
          if (act.args.size() > 3) {
            bound = std::get<weight_t>(act.args[3]);
          }
          
          std::cout << "isUniversal("
                    << valueFunctionToStr(value_fun) << ", "
                    << finiteAggregatorToStr(fin_agg)
                    << ", threshold=" << weight;
          if (bound >= 0) std::cout << ", bound=" << bound;
          std::cout << ") = ";
          
          bool r;
          TIMER_START
          r = NA->isUniversal(value_fun, fin_agg, weight, bound);
          TIMER_END
          std::cout << r << "\n";
          TIMER_PRINT("Cputime: ")
          PRINT_DIV
        }
        break;

      default:
        std::cerr << "Unknown operation\n";
        abort();
      }
    }
    

    /*
    auto A2 =  std::unique_ptr<Automaton>(new Automaton(argv[2]));

    bool included;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
    included = A1->isIncludedIn(A2.get(), value_fun, booleanize);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);

    end_time.tv_sec -= start_time.tv_sec;
    end_time.tv_nsec -= start_time.tv_nsec;

    unsigned n_states, n_edges;
    std::tie(n_states, n_edges) = getAutomatonStats(A1.get());
    std::cout << "A1 states/edges: " << n_states << "," << n_edges << "\n";
    std::tie(n_states, n_edges) = getAutomatonStats(A2.get());
    std::cout << "A2 states/edges: " << n_states << "," << n_edges << "\n";
    std::cout << "Is included: " << included << "\n";
    std::cout << "Cputime: "
              << static_cast<uint64_t>((end_time.tv_sec * 1000000) +
                                       (end_time.tv_nsec / 1000.0))
              << " ms\n";
  */

	return EXIT_SUCCESS;
}

void monEvalTrace(Monitor *M, const std::string& trace) {
  std::ifstream stream(trace);
  if (!stream.is_open()) {
    std::cerr << "Failed opening file: " << trace << "\n";
    abort();
  }

  std::string symbol;
  while (stream) {
    stream >> symbol;
    std::cout << symbol << " -> " << M->next(symbol) << "\n" << std::flush;

  }

}