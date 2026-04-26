#include <chrono>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "NestedAutomaton.h"  // adjust include path if needed

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

// --------------------------- small utils ---------------------------

static std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static std::string strip_non_alnum_keep_pm_colon_paren(std::string s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char uc : s) {
        const char c = static_cast<char>(uc);
        if (std::isalnum(uc) || c == '+' || c == '-' || c == ':' || c == '(' || c == ')' || c == '_')
            out.push_back(c);
    }
    return out;
}

// Parse filenames like: response_n3_k4.txt or resource_n10_k7.txt (generic *_nX_kY.txt)
static bool parse_n_k_from_filename_generic(const std::string& name, int& n, int& k) {
    const std::string suffix = ".txt";
    if (name.size() <= suffix.size()) return false;
    if (name.substr(name.size() - suffix.size()) != suffix) return false;

    const std::size_t pos_n = name.find("_n");
    if (pos_n == std::string::npos) return false;
    const std::size_t pos_k = name.find("_k", pos_n + 2);
    if (pos_k == std::string::npos) return false;

    const std::size_t end = name.size() - suffix.size();
    const std::string n_str = name.substr(pos_n + 2, pos_k - (pos_n + 2));
    const std::string k_str = name.substr(pos_k + 2, end - (pos_k + 2));

    if (n_str.empty() || k_str.empty()) return false;

    try {
        n = std::stoi(n_str);
        k = std::stoi(k_str);
    } catch (...) {
        return false;
    }
    return (n > 0 && k > 0);
}

// --------------------------- parse enums ---------------------------

static value_function_t parse_value_function(const std::string& raw) {
    std::string s = strip_non_alnum_keep_pm_colon_paren(to_lower(raw));

    // Infinite aggregators
    if (s == "sup" || s == "supremum") return Sup;
    if (s == "inf" || s == "infimum") return Inf;
    if (s == "limsup" || s == "ls") return LimSup;
    if (s == "liminf" || s == "li") return LimInf;
    if (s == "limsupavg" || s == "limsupaverage" || s == "limsupmean") return LimSupAvg;
    if (s == "liminfavg" || s == "liminfaverage" || s == "liminfmean") return LimInfAvg;

    // Finite aggregators
    if (s == "sumplus" || s == "sum+") return SumPlus;
    if (s == "summinus" || s == "sum-") return SumMinus;
    if (s == "sumb" || s == "sum_b") return SumB;

    if (s == "max" || s == "maxf" || s == "max_f") return Max_f;
    if (s == "min" || s == "minf" || s == "min_f") return Min_f;

    throw std::invalid_argument("Unknown value function: '" + raw + "'");
}

enum class Problem { Emptiness, Universality };

static Problem parse_problem(const std::string& raw) {
    std::string s = strip_non_alnum_keep_pm_colon_paren(to_lower(raw));
    if (s == "emptiness" || s == "nonemptiness" || s == "nonempty") return Problem::Emptiness;
    if (s == "universality" || s == "universal") return Problem::Universality;
    throw std::invalid_argument("Unknown problem: '" + raw + "'");
}

struct FinSpec {
    value_function_t finVal;
    bool needs_bound;
    bool bound_is_auto;
    double bound_value;
};

// Accept: SumB:auto, SumB:7, SumB(7). Others: no bound.
static FinSpec parse_fin_with_optional_bound(const std::string& raw_fin) {
    std::string s = strip_non_alnum_keep_pm_colon_paren(to_lower(raw_fin));

    // "(...)" payload
    std::optional<std::string> paren_payload;
    {
        const auto lp = s.find('(');
        const auto rp = s.find(')');
        if (lp != std::string::npos && rp != std::string::npos && lp + 1 < rp) {
            paren_payload = s.substr(lp + 1, rp - (lp + 1));
            s = s.substr(0, lp);
        }
    }
    // ":..." payload
    std::optional<std::string> colon_payload;
    {
        const auto cpos = s.find(':');
        if (cpos != std::string::npos && cpos + 1 < s.size()) {
            colon_payload = s.substr(cpos + 1);
            s = s.substr(0, cpos);
        }
    }

    value_function_t finVal = parse_value_function(s);

    FinSpec out;
    out.finVal = finVal;
    out.needs_bound = (finVal == SumB);
    out.bound_is_auto = false;
    out.bound_value = 0.0;

    if (!out.needs_bound) return out;

    std::optional<std::string> payload = colon_payload ? colon_payload : paren_payload;
    if (!payload.has_value()) {
        throw std::invalid_argument("FinVal is SumB, but no bound provided. Use SumB:auto or SumB:<B>.");
    }

    std::string b = to_lower(*payload);
    if (b == "auto") {
        out.bound_is_auto = true;
        return out;
    }

    try {
        out.bound_value = std::stod(b);
    } catch (...) {
        throw std::invalid_argument("Could not parse SumB bound: '" + *payload + "'");
    }
    return out;
}

// --------------------------- signature detection ---------------------------

namespace detail {

template <class T, class = void>
struct has_nonempty3 : std::false_type {};
template <class T>
struct has_nonempty3<T, std::void_t<decltype(std::declval<T&>().isNonEmpty(
    std::declval<value_function_t>(), std::declval<value_function_t>(), std::declval<weight_t>()))>> : std::true_type {};

template <class T, class = void>
struct has_nonempty4 : std::false_type {};
template <class T>
struct has_nonempty4<T, std::void_t<decltype(std::declval<T&>().isNonEmpty(
    std::declval<value_function_t>(), std::declval<value_function_t>(), std::declval<weight_t>(), std::declval<weight_t>()))>> : std::true_type {};

template <class T, class = void>
struct has_univ3 : std::false_type {};
template <class T>
struct has_univ3<T, std::void_t<decltype(std::declval<T&>().isUniversal(
    std::declval<value_function_t>(), std::declval<value_function_t>(), std::declval<weight_t>()))>> : std::true_type {};

template <class T, class = void>
struct has_univ4 : std::false_type {};
template <class T>
struct has_univ4<T, std::void_t<decltype(std::declval<T&>().isUniversal(
    std::declval<value_function_t>(), std::declval<value_function_t>(), std::declval<weight_t>(), std::declval<weight_t>()))>> : std::true_type {};

}  // namespace detail

static bool call_nonempty(NestedAutomaton* nwa,
                          value_function_t infVal,
                          value_function_t finVal,
                          weight_t threshold,
                          weight_t bound_for_sumb) {
    if (finVal == SumB) {
        if constexpr (detail::has_nonempty4<NestedAutomaton>::value) {
            return nwa->isNonEmpty(infVal, finVal, threshold, bound_for_sumb);
        } else {
            return nwa->isNonEmpty(infVal, finVal, threshold);
        }
    } else {
        if constexpr (detail::has_nonempty3<NestedAutomaton>::value) {
            return nwa->isNonEmpty(infVal, finVal, threshold);
        } else {
            return nwa->isNonEmpty(infVal, finVal, threshold, bound_for_sumb);
        }
    }
}

static bool call_universal(NestedAutomaton* nwa,
                           value_function_t infVal,
                           value_function_t finVal,
                           weight_t threshold,
                           weight_t bound_for_sumb) {
    if (finVal == SumB) {
        if constexpr (detail::has_univ4<NestedAutomaton>::value) {
            return nwa->isUniversal(infVal, finVal, threshold, bound_for_sumb);
        } else {
            return nwa->isUniversal(infVal, finVal, threshold);
        }
    } else {
        if constexpr (detail::has_univ3<NestedAutomaton>::value) {
            return nwa->isUniversal(infVal, finVal, threshold);
        } else {
            return nwa->isUniversal(infVal, finVal, threshold, bound_for_sumb);
        }
    }
}

// --------------------------- fork-run protocol ---------------------------

struct ChildMsg {
    uint32_t ok;        // 1 = success, 0 = error
    uint32_t result01;  // solver boolean
    double elapsed_s;   // solve-only time
};

// Write exactly sizeof(ChildMsg) bytes
static bool write_full(int fd, const void* buf, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    size_t off = 0;
    while (off < n) {
        ssize_t w = ::write(fd, p + off, n - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        off += static_cast<size_t>(w);
    }
    return true;
}

// Read exactly sizeof(ChildMsg) bytes
static bool read_full(int fd, void* buf, size_t n) {
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t off = 0;
    while (off < n) {
        ssize_t r = ::read(fd, p + off, n - off);
        if (r == 0) return false;  // EOF
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        off += static_cast<size_t>(r);
    }
    return true;
}

enum class Status { OK, TIMEOUT, ERR, INCONSISTENT };

static const char* status_str(Status s) {
    switch (s) {
        case Status::OK: return "OK";
        case Status::TIMEOUT: return "TIMEOUT";
        case Status::ERR: return "ERR";
        case Status::INCONSISTENT: return "INCONSISTENT";
    }
    return "ERR";
}

// One repetition, fully timed in child, timeout enforced by parent.
// Returns (status, elapsed_s, result01).
static std::tuple<Status, double, int> run_one_rep_forked(
    NestedAutomaton* nwa,
    Problem prob,
    value_function_t infVal,
    value_function_t finVal,
    weight_t threshold,
    weight_t bound_for_sumb,
    double timeout_s
) {
    int pipefd[2];
    if (::pipe(pipefd) != 0) {
        return {Status::ERR, 0.0, 0};
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return {Status::ERR, 0.0, 0};
    }

    if (pid == 0) {
        // Child
        ::close(pipefd[0]); // close read end

        ChildMsg msg;
        msg.ok = 0;
        msg.result01 = 0;
        msg.elapsed_s = 0.0;

        try {
            auto solver_once = [&]() -> bool {
                if (prob == Problem::Emptiness) {
                    return call_nonempty(nwa, infVal, finVal, threshold, bound_for_sumb);
                }
                return call_universal(nwa, infVal, finVal, threshold, bound_for_sumb);
            };

            const auto t0 = Clock::now();
            const bool r = solver_once();
            const auto t1 = Clock::now();

            msg.ok = 1;
            msg.result01 = r ? 1u : 0u;
            msg.elapsed_s = std::chrono::duration<double>(t1 - t0).count();
        } catch (...) {
            msg.ok = 0;
        }

        (void)write_full(pipefd[1], &msg, sizeof(msg));
        ::close(pipefd[1]);
        ::_exit(msg.ok ? 0 : 2);
    }

    // Parent
    ::close(pipefd[1]); // close write end

    const auto start = Clock::now();
    int wstatus = 0;

    while (true) {
        pid_t r = ::waitpid(pid, &wstatus, WNOHANG);
        if (r == pid) break;
        if (r < 0) {
            ::close(pipefd[0]);
            return {Status::ERR, 0.0, 0};
        }

        const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        if (elapsed >= timeout_s) {
            ::kill(pid, SIGKILL);
            (void)::waitpid(pid, &wstatus, 0);
            ::close(pipefd[0]);
            return {Status::TIMEOUT, 0.0, 0};
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ChildMsg msg;
    const bool ok_read = read_full(pipefd[0], &msg, sizeof(msg));
    ::close(pipefd[0]);

    if (!ok_read) return {Status::ERR, 0.0, 0};

    if (msg.ok != 1) return {Status::ERR, 0.0, 0};

    return {Status::OK, msg.elapsed_s, static_cast<int>(msg.result01)};
}

// --------------------------- CLI ---------------------------

static void usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " <file> <problem> <InfVal> <FinVal> <threshold>\n"
        << "       [--rep R] [--timeout-s T] [--warmup 0|1]\n\n"
        << "Solve-only measurement:\n"
        << "  - Parses/builds once (not timed).\n"
        << "  - Runs the solver in forked children R times, timing only the solver call.\n"
        << "  - Parent enforces per-repetition timeout T seconds.\n"
        << "  - Reports MEAN_S over successful repetitions only if STATUS=OK.\n\n"
        << "FinVal for SumB:\n"
        << "  SumB:auto, SumB:<B>, SumB(<B>)\n\n"
        << "Output:\n"
        << "  MEAN_S=<double> RESULT=<0|1> STATUS=<OK|TIMEOUT|ERR|INCONSISTENT>\n";
}

int main(int argc, char** argv) {
    if (argc < 6) {
        usage(argv[0]);
        return 2;
    }

    const std::string filepath = argv[1];

    Problem prob;
    value_function_t infVal;
    FinSpec finSpec;
    double threshold_d = 0.0;

    int rep = 3;
    double timeout_s = 300.0; // 5 minutes
    bool warmup = true;

    try {
        prob = parse_problem(argv[2]);
        infVal = parse_value_function(argv[3]);
        finSpec = parse_fin_with_optional_bound(argv[4]);
        threshold_d = std::stod(argv[5]);

        for (int i = 6; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--rep" && i + 1 < argc) {
                rep = std::stoi(argv[++i]);
            } else if (a == "--timeout-s" && i + 1 < argc) {
                timeout_s = std::stod(argv[++i]);
            } else if (a == "--warmup" && i + 1 < argc) {
                warmup = (std::stoi(argv[++i]) != 0);
            } else if (a == "--help" || a == "-h") {
                usage(argv[0]);
                return 0;
            } else {
                throw std::invalid_argument("Unknown argument: " + a);
            }
        }

        if (rep < 1) throw std::invalid_argument("--rep must be >= 1");
        if (timeout_s <= 0.0) throw std::invalid_argument("--timeout-s must be > 0");
    } catch (const std::exception& e) {
        std::cerr << "Argument error: " << e.what() << "\n";
        usage(argv[0]);
        return 2;
    }

    // Resolve SumB bound if needed
    double bound_d = 0.0;
    if (finSpec.finVal == SumB) {
        if (finSpec.bound_is_auto) {
            const std::string name = fs::path(filepath).filename().string();
            int n = 0, k = 0;
            if (!parse_n_k_from_filename_generic(name, n, k)) {
                std::cerr << "ERROR: SumB:auto but could not parse '_nX_kY.txt' from filename: " << name << "\n";
                return 2;
            }
            bound_d = static_cast<double>(k);
        } else {
            bound_d = finSpec.bound_value;
        }
    }

    // Build automaton once (not timed)
    NestedAutomaton* nwa = nullptr;
    try {
        nwa = new NestedAutomaton(filepath);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: failed to build NestedAutomaton: " << e.what() << "\n";
        return 2;
    } catch (...) {
        std::cerr << "ERROR: failed to build NestedAutomaton (unknown)\n";
        return 2;
    }

    const weight_t thr = static_cast<weight_t>(threshold_d);
    const weight_t bnd = static_cast<weight_t>(bound_d);

    // Optional warmup (not counted), still forked for the same semantics
    if (warmup) {
        auto [st, _t, _r] = run_one_rep_forked(
            nwa, prob, infVal, finSpec.finVal, thr, bnd, timeout_s
        );
        if (st != Status::OK) {
            // Warmup already failing is meaningful: report and exit.
            std::cout << "MEAN_S=0 RESULT=0 STATUS=" << status_str(st) << "\n";
            delete nwa;
            return (st == Status::TIMEOUT ? 124 : 2);
        }
    }

    std::vector<double> times;
    times.reserve(static_cast<size_t>(rep));

    int first_res = -1;
    Status final_status = Status::OK;

    for (int i = 0; i < rep; ++i) {
        auto [st, t, r01] = run_one_rep_forked(
            nwa, prob, infVal, finSpec.finVal, thr, bnd, timeout_s
        );
        if (st != Status::OK) {
            final_status = st;
            break;
        }

        times.push_back(t);

        if (first_res < 0) first_res = r01;
        else if (r01 != first_res) {
            final_status = Status::INCONSISTENT;
            break;
        }
    }

    delete nwa;

    if (final_status != Status::OK) {
        std::cout << "MEAN_S=0 RESULT=" << (first_res < 0 ? 0 : first_res)
                  << " STATUS=" << status_str(final_status) << "\n";
        if (final_status == Status::TIMEOUT) return 124;
        return 2;
    }

    double sum = 0.0;
    for (double t : times) sum += t;
    const double mean = sum / static_cast<double>(times.size());

    std::cout.setf(std::ios::fixed);
    std::cout.precision(9);
    std::cout << "MEAN_S=" << mean
              << " RESULT=" << (first_res < 0 ? 0 : first_res)
              << " STATUS=OK\n";
    return 0;
}
