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
    bool success = true;// workload是否成功执行完毕（如果中途有命令解析错误或者执行错误就标记为false）
    int executedOps = 0;// 成功执行的命令数量（不包括解析错误或者执行错误的命令）
};

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });//将字符串s当中的每个字符都转换为小写字母，返回转换后的字符串
    return s;
}

Algorithm parseStrategy(const std::string& raw) {//将字符串raw解析为对应的Algorithm枚举值，如果无法解析就默认返回Algorithm::FIRST_FIT
    const std::string s = toLower(raw);
    if (s == "first") return Algorithm::FIRST_FIT;
    if (s == "best") return Algorithm::BEST_FIT;
    if (s == "worst") return Algorithm::WORST_FIT;
    return Algorithm::FIRST_FIT;
}

bool isStrategyName(const std::string& raw) {//判断字符串raw是否是合法的策略名称（first、best、worst），不区分大小写，如果是合法的策略名称就返回true，否则返回false
    const std::string s = toLower(raw);
    return s == "first" || s == "best" || s == "worst";
}

std::string strategyName(Algorithm s) {//将Algorithm枚举值转换为对应的字符串名称（first、best、worst），如果枚举值不合法就默认返回"first"
    switch (s) {
        case Algorithm::FIRST_FIT: return "first";
        case Algorithm::BEST_FIT: return "best";
        case Algorithm::WORST_FIT: return "worst";
    }
    return "first";
}

void printHelp() {//打印帮助信息，告诉用户当前有哪些命令可以使用，以及这些命令的用法和功能
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
    //这个函数的作用是读取指定的workload文件，逐行解析并执行其中的命令，使用给定的MemoryManager实例和分配策略来执行分配和释放操作，同时统计执行结果并返回一个WorkloadResult结构体来表示执行是否成功以及执行了多少条命令
    WorkloadResult result;

    std::ifstream in(file);//打开指定的workload文件进行读取，如果文件无法打开就打印错误信息并返回一个标记为失败的WorkloadResult
    if (!in) {
        std::cout << "Error: cannot open workload file: " << file << "\n";
        result.success = false;
        return result;
    }

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {//使用getline函数逐行读取workload文件中的命令，解析并执行这些命令
        ++lineNo;//lineNo变量用于记录当前正在处理的命令行号，便于在解析错误或者执行错误时提供更具体的错误信息
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char c) { return !std::isspace(c); }));
        if (trimmed.empty() || trimmed[0] == '#') continue;//如果这一行是空行或者注释行（以#开头），就跳过不执行

        std::istringstream iss(trimmed);//将这一行的内容放入一个istringstream对象中，方便后续解析命令和参数
        std::string cmd;//从istringstream对象中读取第一个单词作为命令名称，并将其转换为小写字母，方便后续比较命令类型
        iss >> cmd;
        cmd = toLower(cmd);

        if (cmd == "alloc") {
            int pid = -1, sz = 0;
            if (!(iss >> pid >> sz)) {//如果命令是alloc，那么需要从istringstream对象中读取两个参数，分别是pid和size，如果读取失败（比如参数缺失或者格式错误），就打印错误信息并标记执行结果为失败
                std::cout << "Workload parse error (line " << lineNo << "): alloc <pid> <size>\n";
                result.success = false;
                break;
            }
            const bool ok = manager.allocate(pid, sz, strategy);
            if (verbose) {//如果verbose参数为true，那么在执行每条命令后都会打印执行的结果，比如对于alloc命令，会打印分配是否成功，以及分配的pid和size等信息
                //verbose==true,则进入调试模式；verbose==false,则进入正常模式，不打印每条命令的执行结果；在正常模式下，我们只关心最终的统计结果，而不需要看到每条命令的执行细节；在调试模式下，我们可以看到每条命令的执行结果，便于排查问题或者理解内存分配的过程
                std::cout << "[line " << lineNo << "] alloc " << pid << " " << sz
                          << (ok ? " -> OK" : " -> FAIL") << "\n";
            }
            result.executedOps++;
        } else if (cmd == "free") {
            int pid = -1;
            if (!(iss >> pid)) {//如果命令是free，那么需要从istringstream对象中读取一个参数pid，如果读取失败（比如参数缺失或者格式错误），就打印错误信息并标记执行结果为失败
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
        } else {//如果命令名称不匹配任何已知的命令类型，就打印错误信息并标记执行结果为失败
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

    std::cout << "\n=== Compare workload: " << file << " (memory=" << totalMemorySize << ") ===\n";//这个函数的作用是对同一个workload文件，使用三种不同的内存分配策略（首次适应、最佳适应、最差适应）来执行workload中的命令，并收集每种策略执行后的内存统计数据，最后将这些数据以表格的形式打印出来，方便比较不同策略在同一workload下的表现差异
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
