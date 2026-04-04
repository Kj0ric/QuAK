#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "test_cli_helpers.h"

using namespace cli_test;

namespace {

std::string canonicalizeFileContents(const std::string& contents) {
    std::istringstream in(contents);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    std::sort(lines.begin(), lines.end());

    std::ostringstream out;
    for (const std::string& entry : lines) {
        out << entry << '\n';
    }
    return out.str();
}

void assertFileNonEmpty(const fs::path& path, const std::string& context) {
    if (!fs::exists(path)) {
        throw std::runtime_error(context + ": file does not exist: " + path.string());
    }
    if (fs::file_size(path) == 0) {
        throw std::runtime_error(context + ": file is empty: " + path.string());
    }
}

void testScalarCommands() {
    assertContains(runCommandExpectSuccess("samples/A.txt non-empty LimInf 0"),
                   "isNonEmpty(LimInf, weight=0) = 1",
                   "non-empty should remain supported for regular automata");
    assertContains(runCommandExpectSuccess("samples/A.txt top-value LimSup"),
                   "topValue(LimSup) = 4",
                   "top-value output regressed");
    assertContains(runCommandExpectSuccess("samples/A.txt bottom-value LimInf"),
                   "bottomValue(LimInf) = 0",
                   "bottom-value output regressed");
    assertContains(runCommandExpectSuccess("samples/A.txt constant LimInf"),
                   "isConstant(LimInf) = 0",
                   "constant output regressed");
    assertContains(runCommandExpectSuccess("samples/A.txt safe LimInf"),
                   "isSafe(LimInf) = 0",
                   "safe output regressed");
    assertContains(runCommandExpectSuccess("samples/A.txt live LimInf"),
                   "isLive(LimInf) = 1",
                   "live output regressed");
}

void testComparisonCommands() {
    assertContains(runCommandExpectSuccess("samples/A.txt isIncluded LimInf samples/B.txt"),
                   "isIncluded(LimInf) = 0",
                   "antichain inclusion output regressed");
    assertContains(runCommandExpectSuccess("samples/A.txt isIncludedBool LimInf samples/B.txt"),
                   "isIncluded(bool, LimInf) = 0",
                   "booleanized inclusion output regressed");
    assertContains(runCommandExpectSuccess("samples/A.txt isEquivalent LimInf samples/A.txt"),
                   "isEquivalent(LimInf) = 1",
                   "equivalence output regressed");
    assertContains(runCommandExpectSuccess("samples/A.txt isEquivalentBool LimInf samples/A.txt"),
                   "isEquivalent(bool, LimInf) = 1",
                   "booleanized equivalence output regressed");
}

void testComponentCommands() {
    const fs::path temp_dir = fs::temp_directory_path() /
                              ("quak_non_nested_" + std::to_string(std::time(nullptr)));
    fs::create_directories(temp_dir);

    const fs::path safety_path = temp_dir / "safe.txt";
    const fs::path liveness_path = temp_dir / "live.txt";
    const fs::path decompose_safety_path = temp_dir / "safe2.txt";
    const fs::path decompose_liveness_path = temp_dir / "live2.txt";

    runCommandExpectSuccess("samples/A.txt safetyComponent LimInf \"" + safety_path.string() + "\"");
    runCommandExpectSuccess("samples/A.txt livenessComponent LimInf \"" + liveness_path.string() + "\"");

    const std::string decompose_output =
        runCommandExpectSuccess("samples/A.txt decompose LimInf \"" + decompose_safety_path.string() +
                   "\" \"" + decompose_liveness_path.string() + "\"");

    assertContains(decompose_output,
                   "Safety component written to:",
                   "decompose should report the safety output path");
    assertContains(decompose_output,
                   "Liveness component written to:",
                   "decompose should report the liveness output path");

    assertFileNonEmpty(safety_path, "safetyComponent");
    assertFileNonEmpty(liveness_path, "livenessComponent");
    assertFileNonEmpty(decompose_safety_path, "decompose safety output");
    assertFileNonEmpty(decompose_liveness_path, "decompose liveness output");

    const std::string safety = readFile(safety_path.string());
    const std::string decomposed_safety = readFile(decompose_safety_path.string());
    const std::string liveness = readFile(liveness_path.string());
    const std::string decomposed_liveness = readFile(decompose_liveness_path.string());

    if (canonicalizeFileContents(safety) != canonicalizeFileContents(decomposed_safety)) {
        throw std::runtime_error("decompose safety output differs from safetyComponent output");
    }
    if (canonicalizeFileContents(liveness) != canonicalizeFileContents(decomposed_liveness)) {
        throw std::runtime_error("decompose liveness output differs from livenessComponent output");
    }

    fs::remove_all(temp_dir);
}

} // namespace

int main() {
    try {
        testScalarCommands();
        testComparisonCommands();
        testComponentCommands();
        std::cout << "Non-nested CLI backward compatibility checks passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Non-nested backward compatibility test failed: " << e.what() << std::endl;
        return 1;
    }
}
