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
# 使用 g++ 编译（也可用 workspace 提供的 VS Code 任务）
# 输出可执行文件 sim
g++ -std=c++17 -Iinclude src/main.cpp src/MemoryManager.cpp -o sim
```

## 运行

```bash
# 默认总内存 1024 个单位
./sim
# 或通过命令行参数指定总内存
./sim 2048
```

## CLI 命令

- `help`：显示帮助菜单
- `exit` / `quit`：退出程序
- `show`：打印当前内存块的分布（地址区间 + PID/Free + 大小）
- `stats`：输出碎片统计信息（空闲块数、总空闲、最大/最小/平均块、利用率、外部碎片率）
- `alloc <pid> <size>`：按当前策略为进程分配指定大小
- `free <pid>`：释放该 PID 占用的内存
- `set strategy first|best|worst`：切换分配策略
- `runworkload <file>`：执行脚本（会在 `workloads/` 中查找，并自动追加 `.txt`）
- `compare <file> [memory_size]`：对比三种策略运行同一脚本；`memory_size` 可选，用于覆写当前总内存原值
- `reset <memory_size>`：重置管理器并设置新的总内存（会清除之前的所有分配）

## 示例输出

```
mm> show
[0000, 1023] Free      | Size: 1024
mm> alloc 1 100
allocate success
mm> show
[0000, 0099] PID: 1    | Size: 100
[0100, 1023] Free      | Size: 924
mm> stats
Free Blocks: 1, Total Free: 924, Max Free Block: 924, Min Free Block: 924, Avg Free Block: 924.00, External Fragmentation Ratio: 0.00, Utilization: 0.10
```

`reset` 会重新初始化内存管理器并清除之前的分配，`compare` 命令在表格中输出每种策略的统计值。

## workload 文件格式

支持以下行（`#` 开头表示注释）：

- `alloc <pid> <size>`
- `free <pid>`
- `show`
- `stats`

示例：

```bash
# workload 文件名可省略路径和扩展名
runworkload demo_first          # 等同于 workloads/demo_first.txt
runworkload some_script         # 如果存在 workloads/some_script.txt

# compare 可以指定内存大小
compare demo_compare            # 使用当前设置的总内存
compare demo_compare 2048       # 使用 2048 单位内存进行三策略对比

# 在交互式 shell 中也可以直接运行这些命令
```
