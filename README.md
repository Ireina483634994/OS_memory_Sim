# OS Memory Simulator (CLI)

一个面向初学者的连续内存分配模拟器：
- 支持 First Fit / Best Fit / Worst Fit 三种分配策略
- 支持内存块分割与相邻空闲块合并
- 支持交互式 CLI、workload 脚本执行和三策略对比

## 目录结构

- `include/MemoryManager.h`：核心数据结构与接口
- `src/MemoryManager.cpp`：分配/释放/合并/统计实现
- `src/main.cpp`：CLI 与 workload/compare 框架
- `workloads/*.txt`：演示脚本（含 first/best/worst/compare）

## 编译

```bash
g++ -std=c++17 -Iinclude src/main.cpp src/MemoryManager.cpp -o sim
```

## 运行

```bash
./sim
# 或指定总内存
./sim 1024
```

## CLI 命令

- `help`
- `exit`
- `show`
- `stats`
- `alloc <pid> <size>`
- `free <pid>`
- `set strategy first|best|worst`
- `runworkload <file>`
- `compare <file>`
- `reset <memory_size>`

## workload 文件格式

支持以下行（`#` 开头表示注释）：

- `alloc <pid> <size>`
- `free <pid>`
- `show`
- `stats`

示例：

```bash
runworkload workloads/demo_first.txt
runworkload workloads/demo_worst.txt
compare workloads/demo_compare.txt
```

