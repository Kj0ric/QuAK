#pragma once

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#ifndef QUAK_NESTED_PATH
#define QUAK_NESTED_PATH "./build/quak-nested"
#endif

namespace fs = std::filesystem;

namespace cli_test {

inline std::string uniquePath(const std::string& prefix, const std::string& suffix) {
    static unsigned counter = 0;
    std::ostringstream name;
    name << prefix << "_" << std::time(nullptr) << "_" << counter++ << suffix;
    return (fs::temp_directory_path() / name.str()).string();
}

inline std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Failed to read file: " + path);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

// Runs quak-nested with the given args. Throws if exit code is non-zero.
inline std::string runCommandExpectSuccess(const std::string& args) {
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

// Runs quak-nested with the given args. Throws if exit code is 0.
inline std::string runCommandExpectFailure(const std::string& args) {
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

inline void assertContains(const std::string& haystack, const std::string& needle,
                            const std::string& context) {
    if (haystack.find(needle) == std::string::npos) {
        throw std::runtime_error(
            context + "\nExpected to find: " + needle + "\nActual output:\n" + haystack);
    }
}

} // namespace cli_test
