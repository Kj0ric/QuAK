#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: compare_linewise <left> <right> [limit]\n";
        return 1;
    }

    std::ifstream left(argv[1]);
    std::ifstream right(argv[2]);
    if (!left.is_open() || !right.is_open()) {
        std::cerr << "failed to open input files\n";
        return 1;
    }

    unsigned int limit = 50;
    if (argc >= 4) {
        limit = static_cast<unsigned int>(std::stoul(argv[3]));
    }

    std::string left_line;
    std::string right_line;
    unsigned int line_no = 0;
    unsigned int mismatches = 0;

    while (true) {
        const bool left_ok = static_cast<bool>(std::getline(left, left_line));
        const bool right_ok = static_cast<bool>(std::getline(right, right_line));
        if (!left_ok && !right_ok) {
            break;
        }

        ++line_no;
        if (left_ok != right_ok || left_line != right_line) {
            ++mismatches;
            if (mismatches <= limit) {
                std::cout << "line " << line_no << "\n";
                std::cout << "  left : " << (left_ok ? left_line : "<eof>") << "\n";
                std::cout << "  right: " << (right_ok ? right_line : "<eof>") << "\n";
            }
        }
    }

    std::cout << "mismatches=" << mismatches << "\n";
    return 0;
}
