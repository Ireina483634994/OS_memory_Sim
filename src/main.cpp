#include "../include/MemoryManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

namespace {

struct WorkloadResult {
    bool success = true;
    int executedOps = 0;
};

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

Algorithm parseStrategy(const std::string& raw) {
    const std::string s = toLower(raw);
    if (s == "first") return Algorithm::FIRST_FIT;
    if (s == "best") return Algorithm::BEST_FIT;
    if (s == "worst") return Algorithm::WORST_FIT;
    return Algorithm::FIRST_FIT;
}

bool isStrategyName(const std::string& raw) {
    const std::string s = toLower(raw);
    return s == "first" || s == "best" || s == "worst";
}

std::string strategyName(Algorithm s) {
    switch (s) {
        case Algorithm::FIRST_FIT: return "first";
        case Algorithm::BEST_FIT: return "best";
        case Algorithm::WORST_FIT: return "worst";
    }
    return "first";
}

void printHelp() {
    std::cout
        << "\n==== Memory Manager CLI ====\n"
        << "help                              : 显示帮助\n"
        << "exit                              : 退出程序\n"
        << "show                              : 展示内存分布\n"
        << "stats                             : 展示碎片统计\n"
        << "alloc <pid> <size>                : 按当前策略分配\n"
        << "free <pid>                        : 释放该 PID 占用内存\n"
        << "set strategy first|best|worst     : 切换分配策略\n"
        << "runworkload <file>                : 执行 workload 脚本\n"
        << "compare <file>                    : 同一 workload 对比三种策略\n"
        << "reset <memory_size>               : 重置管理器（清空所有分配）\n"
        << "\nworkload 文件支持命令:\n"
        << "  alloc <pid> <size>\n"
        << "  free <pid>\n"
        << "  show\n"
        << "  stats\n"
        << "  # 注释行\n\n";
}

WorkloadResult runWorkload(MemoryManager& manager, Algorithm strategy, const std::string& file, bool verbose = true) {
    WorkloadResult result;

    std::ifstream in(file);
    if (!in) {
        std::cout << "Error: cannot open workload file: " << file << "\n";
        result.success = false;
        return result;
    }

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char c) { return !std::isspace(c); }));
        if (trimmed.empty() || trimmed[0] == '#') continue;

        std::istringstream iss(trimmed);
        std::string cmd;
        iss >> cmd;
        cmd = toLower(cmd);

        if (cmd == "alloc") {
            int pid = -1, sz = 0;
            if (!(iss >> pid >> sz)) {
                std::cout << "Workload parse error (line " << lineNo << "): alloc <pid> <size>\n";
                result.success = false;
                break;
            }
            const bool ok = manager.allocate(pid, sz, strategy);
            if (verbose) {
                std::cout << "[line " << lineNo << "] alloc " << pid << " " << sz
                          << (ok ? " -> OK" : " -> FAIL") << "\n";
            }
            result.executedOps++;
        } else if (cmd == "free") {
            int pid = -1;
            if (!(iss >> pid)) {
                std::cout << "Workload parse error (line " << lineNo << "): free <pid>\n";
                result.success = false;
                break;
            }
            manager.deallocate(pid);
            if (verbose) {
                std::cout << "[line " << lineNo << "] free " << pid << "\n";
            }
            result.executedOps++;
        } else if (cmd == "show") {
            if (verbose) {
                std::cout << "[line " << lineNo << "] show\n";
                manager.showMemoryMap();
            }
            result.executedOps++;
        } else if (cmd == "stats") {
            if (verbose) {
                std::cout << "[line " << lineNo << "] stats\n";
                manager.showStats();
            }
            result.executedOps++;
        } else {
            std::cout << "Workload parse error (line " << lineNo << "): unknown command '" << cmd << "'\n";
            result.success = false;
            break;
        }
    }

    return result;
}

void compareWorkload(int totalMemorySize, const std::string& file) {
    const std::vector<Algorithm> strategies = {
        Algorithm::FIRST_FIT,
        Algorithm::BEST_FIT,
        Algorithm::WORST_FIT
    };

    std::cout << "\n=== Compare workload: " << file << " (memory=" << totalMemorySize << ") ===\n";
    std::cout << std::left << std::setw(10) << "strategy"
              << std::setw(12) << "ops"
              << std::setw(10) << "ok"
              << std::setw(13) << "freeBlocks"
              << std::setw(12) << "freeTotal"
              << std::setw(12) << "maxFree"
              << std::setw(12) << "extFrag" << "\n";

    for (Algorithm s : strategies) {
        auto manager = std::make_unique<MemoryManager>(totalMemorySize);
        WorkloadResult r = runWorkload(*manager, s, file, false);
        MemoryStats st = manager->getStats();

        std::cout << std::left << std::setw(10) << strategyName(s)
                  << std::setw(12) << r.executedOps
                  << std::setw(10) << (r.success ? "yes" : "no")
                  << std::setw(13) << st.freeBlocks
                  << std::setw(12) << st.totalFree
                  << std::setw(12) << st.maxFreeBlock
                  << std::setw(12) << std::fixed << std::setprecision(2) << st.externalFragmentation
                  << "\n";
    }

    std::cout << "============================================\n\n";
}

} // namespace

int main(int argc, char* argv[]) {
    int totalMemorySize = 1024;
    if (argc >= 2) {
        std::istringstream iss(argv[1]);
        int candidate = 0;
        if (iss >> candidate && candidate > 0) {
            totalMemorySize = candidate;
        }
    }

    auto manager = std::make_unique<MemoryManager>(totalMemorySize);
    Algorithm currentStrategy = Algorithm::FIRST_FIT;

    std::cout << "Memory Manager CLI started. total memory = " << totalMemorySize << "\n";
    std::cout << "Current strategy: " << strategyName(currentStrategy) << "\n";
    printHelp();

    std::string line;
    while (true) {
        std::cout << "mm> ";
        if (!std::getline(std::cin, line)) break;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd.empty()) continue;
        cmd = toLower(cmd);

        if (cmd == "help") {
            printHelp();
        } else if (cmd == "exit" || cmd == "quit") {
            std::cout << "Bye.\n";
            break;
        } else if (cmd == "show") {
            manager->showMemoryMap();
        } else if (cmd == "stats") {
            manager->showStats();
        } else if (cmd == "alloc") {
            int pid = -1, sz = 0;
            if (!(iss >> pid >> sz)) {
                std::cout << "Usage: alloc <pid> <size>\n";
                continue;
            }
            bool ok = manager->allocate(pid, sz, currentStrategy);
            std::cout << (ok ? "allocate success\n" : "allocate failed\n");
        } else if (cmd == "free") {
            int pid = -1;
            if (!(iss >> pid)) {
                std::cout << "Usage: free <pid>\n";
                continue;
            }
            manager->deallocate(pid);
        } else if (cmd == "set") {
            std::string subject, strategy;
            iss >> subject >> strategy;
            if (toLower(subject) != "strategy" || !isStrategyName(strategy)) {
                std::cout << "Usage: set strategy first|best|worst\n";
                continue;
            }
            currentStrategy = parseStrategy(strategy);
            std::cout << "Strategy switched to: " << strategyName(currentStrategy) << "\n";
        } else if (cmd == "runworkload") {
            std::string file;
            iss >> file;
            if (file.empty()) {
                std::cout << "Usage: runworkload <file>\n";
                continue;
            }
            WorkloadResult r = runWorkload(*manager, currentStrategy, file, true);
            std::cout << "runworkload done. ops=" << r.executedOps << ", success=" << (r.success ? "yes" : "no") << "\n";
        } else if (cmd == "compare") {
            std::string file;
            iss >> file;
            if (file.empty()) {
                std::cout << "Usage: compare <file>\n";
                continue;
            }
            compareWorkload(totalMemorySize, file);
        } else if (cmd == "reset") {
            int newSize = 0;
            if (!(iss >> newSize) || newSize <= 0) {
                std::cout << "Usage: reset <memory_size>\n";
                continue;
            }
            totalMemorySize = newSize;
            manager = std::make_unique<MemoryManager>(totalMemorySize);
            std::cout << "Manager reset. total memory = " << totalMemorySize << "\n";
        } else {
            std::cout << "Unknown command: " << cmd << " (type help)\n";
        }
    }

    return 0;
}
