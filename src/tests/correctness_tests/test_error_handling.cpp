#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#ifndef QUAK_NESTED_PATH
#define QUAK_NESTED_PATH "./build/quak-nested"
#endif

namespace fs = std::filesystem;

namespace {

std::string uniquePath(const std::string& prefix, const std::string& suffix) {
    static unsigned counter = 0;
    std::ostringstream name;
    name << prefix << "_" << std::time(nullptr) << "_" << counter++ << suffix;
    return (fs::temp_directory_path() / name.str()).string();
}

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Failed to read file: " + path);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

// Runs a command and returns its output. Throws if exit code is non-zero (expected success).
std::string runCommandExpectSuccess(const std::string& args) {
    const std::string output_path = uniquePath("quak_output", ".txt");
    { std::ofstream out(output_path); }
    const std::string command =
        "\"" + std::string(QUAK_NESTED_PATH) + "\" " + args +
        " > \"" + output_path + "\" 2>&1";
    const int exit_code = std::system(command.c_str());
    const std::string output = readFile(output_path);
    fs::remove(output_path);
    if (exit_code != 0) {
        throw std::runtime_error(
            "Expected zero exit but command failed.\n"
            "Command: " + command + "\nOutput:\n" + output);
    }
    return output;
}

// Runs a command and returns its output. Throws if exit code is 0 (expected failure).
std::string runCommandExpectFailure(const std::string& args) {
    const std::string output_path = uniquePath("quak_err_output", ".txt");
    { std::ofstream out(output_path); }
    const std::string command =
        "\"" + std::string(QUAK_NESTED_PATH) + "\" " + args +
        " > \"" + output_path + "\" 2>&1";
    const int exit_code = std::system(command.c_str());
    const std::string output = readFile(output_path);
    fs::remove(output_path);
    if (exit_code == 0) {
        throw std::runtime_error(
            "Expected non-zero exit but command succeeded.\n"
            "Command: " + command + "\nOutput:\n" + output);
    }
    return output;
}

void assertContains(const std::string& haystack, const std::string& needle,
                    const std::string& context) {
    if (haystack.find(needle) == std::string::npos) {
        throw std::runtime_error(
            context + "\nExpected to find: " + needle + "\nActual output:\n" + haystack);
    }
}

// ---------------------------------------------------------------------------
// Tests for errors already caught by the CLI argument parser
// ---------------------------------------------------------------------------

void testMissingSumBBound() {
    // SumB requires a bound parameter; CLI should reject if missing.
    const std::string out = runCommandExpectFailure(
        "examples/nested/simple_counter.txt non-empty LimSup SumB 1");
    assertContains(out, "bound", "Missing SumB bound should produce a usage error mentioning 'bound'");
}

void testUnsupportedUniversalCombo() {
    // isUniversal does not support LimSupAvg; CLI should reject it.
    const std::string out = runCommandExpectFailure(
        "examples/nested/simple_counter.txt universal LimSupAvg Max_f 1");
    assertContains(out, "not support", "Unsupported universal combo should mention 'not support'");
}

// ---------------------------------------------------------------------------
// Tests for errors caught by the parser
// ---------------------------------------------------------------------------

void testUndefinedChildIndex() {
    // Parent uses weight 2 (child index 2) but only @CHILD 0 and @CHILD 1 are defined.
    const std::string out = runCommandExpectFailure(
        "src/tests/correctness_tests/inputs/tc_err_undefined_child.txt non-empty LimSup Max_f 1");
    assertContains(out, "child automaton index",
                   "Undefined child index should report 'child automaton index'");
}

// ---------------------------------------------------------------------------
// Tests for errors caught by validateNested()
// ---------------------------------------------------------------------------

void testSilentInChild() {
    // Child 1 has a SILENT transition (non-parseable weight stored as float::max()).
    // validateNested() should abort with a message naming child index and SILENT.
    const std::string out = runCommandExpectFailure(
        "src/tests/correctness_tests/inputs/tc_err_silent_in_child.txt non-empty LimSup Max_f 1");
    assertContains(out, "SILENT",
                   "SILENT in child should produce an error message containing 'SILENT'");
    assertContains(out, "Child automaton 1",
                   "Error message should identify the offending child index");
}

// ---------------------------------------------------------------------------
// Smoke tests: valid inputs that must not crash
// ---------------------------------------------------------------------------

void testEdgeCases() {
    // Threshold = 0 boundary
    runCommandExpectSuccess(
        "examples/nested/simple_counter.txt non-empty LimSup Max_f 0");

    // Large threshold with Max_f — bounded by child weights, so returns false cleanly
    // (Note: Inf+SumPlus with very large thresholds is a known memory limitation;
    //  flatten_SumPlusMinus_Inf allocates O(threshold * states) and should not be called
    //  with thresholds in the millions.)
    runCommandExpectSuccess(
        "examples/nested/simple_counter.txt non-empty LimSup Max_f 1000000");

    // Single-state parent + single-state child
    runCommandExpectSuccess(
        "src/tests/correctness_tests/inputs/tc_single_state.txt non-empty LimSup Max_f 1");

    // Negative threshold: weights are >= 0, so any infVal/Max_f value exceeds -1
    runCommandExpectSuccess(
        "examples/nested/simple_counter.txt non-empty LimInf Max_f -1");

    // Large alphabet: 12-symbol parent/child — tests alphabet-indexed structures in flattening
    runCommandExpectSuccess(
        "src/tests/correctness_tests/inputs/tc_large_alphabet.txt non-empty LimSup Max_f 2");
}

} // namespace

int main() {
    try {
        testMissingSumBBound();
        testUnsupportedUniversalCombo();
        testUndefinedChildIndex();
        testSilentInChild();
        testEdgeCases();
        std::cout << "Error handling checks passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error handling test failed: " << e.what() << std::endl;
        return 1;
    }
}
