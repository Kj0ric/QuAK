#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef QUAK_NESTED_PATH
#define QUAK_NESTED_PATH "./build/quak-nested"
#endif

namespace {

namespace fs = std::filesystem;

std::string commandQuote(const std::string& value) {
    std::string result = "\"";
    for (char ch : value) {
        if (ch == '"') {
            result += "\\\"";
        } else {
            result += ch;
        }
    }
    result += "\"";
    return result;
}

fs::path tempPath(const std::string& stem) {
    static unsigned int counter = 0;
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
           ("quak_" + stem + "_" + std::to_string(now) + "_" + std::to_string(counter++) + ".txt");
}

void writeFile(const fs::path& path, const std::string& content) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("could not write " + path.string());
    }
    out << content;
}

std::string readFile(const fs::path& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string runQuak(const fs::path& automaton, const std::string& args, bool expect_success) {
    fs::path output_path = tempPath("output");
    std::string command = commandQuote(QUAK_NESTED_PATH) + " " +
                          commandQuote(automaton.string()) + " " +
                          args + " > " + commandQuote(output_path.string()) + " 2>&1";

    int status = std::system(command.c_str());
    std::string output = readFile(output_path);
    fs::remove(output_path);

    bool succeeded = (status == 0);
    if (succeeded != expect_success) {
        throw std::runtime_error("unexpected command status for " + automaton.string() + "\n" + output);
    }
    return output;
}

void assertContains(const std::string& haystack, const std::string& needle) {
    if (haystack.find(needle) == std::string::npos) {
        throw std::runtime_error("expected output to contain '" + needle + "'\n" + haystack);
    }
}

void testNonNestedRejectsNonnumericWeight() {
    fs::path automaton = tempPath("non_nested_bad_weight");
    writeFile(automaton,
              "final: q0\n"
              "a : notanumber, q0 -> q0\n");

    std::string output = runQuak(automaton, "non-empty LimSup 0", false);
    fs::remove(automaton);
    assertContains(output, "invalid numeric weight 'notanumber'");
}

void testChildRejectsSilentWeight() {
    fs::path automaton = tempPath("child_silent_weight");
    writeFile(automaton,
              "@PARENT\n"
              "final: all\n"
              "a : 1, p0 -> p0\n"
              "@CHILD 0\n"
              "@CHILD 1\n"
              "final: c1\n"
              "a : SILENT, c0 -> c1\n");

    std::string output = runQuak(automaton, "non-empty LimSup Max_f 0", false);
    fs::remove(automaton);
    assertContains(output, "SILENT weights are allowed only on nested parent transitions");
}

void testChildRejectsNonnumericWeight() {
    fs::path automaton = tempPath("child_bad_weight");
    writeFile(automaton,
              "@PARENT\n"
              "final: all\n"
              "a : 1, p0 -> p0\n"
              "@CHILD 0\n"
              "@CHILD 1\n"
              "final: c1\n"
              "a : notanumber, c0 -> c1\n");

    std::string output = runQuak(automaton, "non-empty LimSup Max_f 0", false);
    fs::remove(automaton);
    assertContains(output, "invalid numeric weight 'notanumber'");
}

void testParentAcceptsLiteralSilentWeight() {
    fs::path automaton = tempPath("parent_silent_weight");
    writeFile(automaton,
              "@PARENT\n"
              "final: all\n"
              "a : SILENT, p0 -> p0\n"
              "b : 1, p0 -> p0\n"
              "@CHILD 0\n"
              "@CHILD 1\n"
              "final: c1\n"
              "a : 0, c0 -> c1\n"
              "b : 0, c0 -> c1\n");

    runQuak(automaton, "non-empty LimSup Max_f 0", true);
    fs::remove(automaton);
}

void testParentRejectsSilentAlias() {
    fs::path automaton = tempPath("parent_silent_alias");
    writeFile(automaton,
              "@PARENT\n"
              "final: all\n"
              "a : SIL, p0 -> p0\n"
              "b : 1, p0 -> p0\n"
              "@CHILD 0\n"
              "@CHILD 1\n"
              "final: c1\n"
              "a : 0, c0 -> c1\n"
              "b : 0, c0 -> c1\n");

    std::string output = runQuak(automaton, "non-empty LimSup Max_f 0", false);
    fs::remove(automaton);
    assertContains(output, "invalid parent weight 'SIL': expected numeric weight or SILENT");
}

} // namespace

int main() {
    const std::vector<std::pair<std::string, void (*)()>> tests = {
        {"testNonNestedRejectsNonnumericWeight", testNonNestedRejectsNonnumericWeight},
        {"testChildRejectsSilentWeight", testChildRejectsSilentWeight},
        {"testChildRejectsNonnumericWeight", testChildRejectsNonnumericWeight},
        {"testParentAcceptsLiteralSilentWeight", testParentAcceptsLiteralSilentWeight},
        {"testParentRejectsSilentAlias", testParentRejectsSilentAlias},
    };

    for (const auto& test : tests) {
        try {
            test.second();
            std::cout << "[PASS] " << test.first << "\n";
        } catch (const std::exception& ex) {
            std::cerr << "[FAIL] " << test.first << ": " << ex.what() << "\n";
            return 1;
        }
    }

    std::cout << "All parser error-handling tests passed.\n";
    return 0;
}
