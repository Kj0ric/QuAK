#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "NestedAutomaton.h"

namespace {

constexpr float kThresholdMargin = 1.0f;
constexpr float kDedupEpsilon = 1e-4f;

bool nearly_equal(float a, float b) {
    return std::abs(a - b) < kDedupEpsilon;
}

void collect_automaton_values(const Automaton* automaton, std::vector<float>* values) {
    values->push_back(automaton->getMinDomain().to_float());
    values->push_back(automaton->getMaxDomain().to_float());

    for (Weight* weight : *automaton->getWeights()) {
        values->push_back(weight->getValue().to_float());
    }
}

std::vector<weight_t> collect_thresholds(const NestedAutomaton& nwa) {
    std::vector<float> raw_values;
    collect_automaton_values(&nwa, &raw_values);

    for (std::size_t i = 0; i < nwa.getChildrenSize(); ++i) {
        collect_automaton_values(nwa.getChild(i), &raw_values);
    }

    std::sort(raw_values.begin(), raw_values.end());
    raw_values.erase(
        std::unique(raw_values.begin(), raw_values.end(),
                    [](float lhs, float rhs) { return nearly_equal(lhs, rhs); }),
        raw_values.end());

    std::vector<float> thresholds_raw;
    if (raw_values.empty()) {
        thresholds_raw = {-1.0f, 0.0f, 1.0f};
    } else {
        thresholds_raw.push_back(raw_values.front() - kThresholdMargin);
        for (std::size_t i = 0; i < raw_values.size(); ++i) {
            thresholds_raw.push_back(raw_values[i]);
            if (i + 1 < raw_values.size()) {
                thresholds_raw.push_back((raw_values[i] + raw_values[i + 1]) / 2.0f);
            }
        }
        thresholds_raw.push_back(raw_values.back() + kThresholdMargin);
    }

    thresholds_raw.push_back(0.0f);

    std::sort(thresholds_raw.begin(), thresholds_raw.end());
    thresholds_raw.erase(
        std::unique(thresholds_raw.begin(), thresholds_raw.end(),
                    [](float lhs, float rhs) { return nearly_equal(lhs, rhs); }),
        thresholds_raw.end());

    std::vector<weight_t> thresholds;
    thresholds.reserve(thresholds_raw.size());
    for (float value : thresholds_raw) {
        thresholds.emplace_back(value);
    }
    return thresholds;
}

std::vector<std::filesystem::path> collect_input_files() {
    std::vector<std::filesystem::path> paths;
    const std::filesystem::path input_dir("src/tests/correctness_tests/inputs");
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".txt") {
            paths.push_back(entry.path());
        }
    }

    std::sort(paths.begin(), paths.end(),
              [](const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
                  return lhs.filename().string() < rhs.filename().string();
              });
    return paths;
}

const char* inf_name(value_function_t inf_value) {
    switch (inf_value) {
        case Sup:
            return "Sup";
        case LimSup:
            return "LimSup";
        default:
            return "?";
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::ostream* out = &std::cout;
    std::ofstream file_out;
    if (argc >= 2) {
        file_out.open(argv[1]);
        if (!file_out.is_open()) {
            std::cerr << "failed to open output file: " << argv[1] << "\n";
            return 1;
        }
        out = &file_out;
    }

    const std::vector<std::filesystem::path> inputs = collect_input_files();
    const std::vector<value_function_t> inf_values = {Sup, LimSup};

    unsigned int queries = 0;

    for (const std::filesystem::path& input : inputs) {
        NestedAutomaton nwa(input.string());
        const std::vector<weight_t> thresholds = collect_thresholds(nwa);
        const std::string label = input.stem().string();

        for (value_function_t inf_value : inf_values) {
            for (weight_t threshold : thresholds) {
                ++queries;
                const bool result = nwa.isNonEmpty(inf_value, Max_f, threshold);
                *out << label
                     << ","
                     << inf_name(inf_value)
                     << ",Max_f,"
                     << std::fixed << std::setprecision(2) << threshold.to_float()
                     << ","
                     << (result ? 1 : 0)
                     << "\n";
            }
        }
    }

    *out << "queries=" << queries << "\n";
    return 0;
}
