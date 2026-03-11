#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <iostream>
#include <cstddef>        // 为了使用 size_t
#include <limits>
#include <unordered_map>

// 内存块结构
struct MemoryBlock {
    int startAddr;      // 起始地址
    int size;           // 块大小
    int processId;      // 占用该块的进程ID（-1 表示空闲）
    bool isFree;        // 是否空闲
    MemoryBlock* next;  // 指向下一个块（按物理地址单调递增）
    MemoryBlock* prev;  // 指向前一个块（方便合并）

    MemoryBlock(int addr, int s)
        : startAddr(addr), size(s), processId(-1), isFree(true), next(nullptr), prev(nullptr) {}
};

enum class Algorithm { FIRST_FIT, BEST_FIT, WORST_FIT };


struct MemoryStats {// 内存统计数据结构
    int freeBlocks;          // 空闲块数量
    int totalFree;           // 总空闲内存大小
    int maxFreeBlock;        // 最大空闲块大小
    double externalFragmentation; // 外部碎片率 = (总空闲内存大小 - 最大空闲块大小) / 总空闲内存大小
    int minFreeBlock;        // 最小空闲块大小（如果没有块保持0）
    double avgFreeBlock;     // 平均空闲块大小
    double utilization;      // 内存利用率 = (totalSize - totalFree) / totalSize
};

class MemoryManager {
public:
    // 构造函数：初始化一个总大小为 totalSize 的内存空闲区域（从地址 0 开始）没有返回值，函数名必须和class类名相同，这是一个public成员函数，外部可以访问
    explicit MemoryManager(int totalSize);// 初始化一个总大小为 totalSize 的内存空闲区域（从地址 0 开始）
    ~MemoryManager();// 释放链表中的所有块

    // 禁止拷贝构造和赋值操作，防止误用
    //现在如果写：MemoryManager m2 = m1;或者MemoryManager m3(m1);或者m3 = m1;都会编译错误，提示拷贝构造函数或赋值操作被禁用
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;


    // 核心接口
    // 分配 reqSize 大小的内存给 pid，使用 algo 策略（成功返回 true，失败返回 false）
    // 如果传入非空的 errMsg 指针，则在失败时填充错误原因
    bool allocate(int pid, int reqSize, Algorithm algo, std::string* errMsg = nullptr);

  
    bool deallocate(int pid, bool verbose = true);// 释放 pid 占用的内存，注意合并相邻空闲块

    // 可视化 / 统计
    void showMemoryMap() const;//ASCII可视化
    void showStats() const;//碎片统计
    MemoryStats getStats() const;// 统计数据（便于 compare 等功能）


private:
    MemoryBlock* head;            // 按 startAddr 单调递增的链表头（覆盖整个管理区域）
    int totalSize;// 管理的总内存大小

    
    std::unordered_map<int, MemoryBlock*> pidMap;// 在一个进程释放内存时，为了快速释放，用 进程的pid -> block 映射
    // 3种算法的查找合适的空闲内存块的策略（遍历链表），返回合适的空闲内存块的指针（如果没有合适的块，返回 nullptr）
    //函数的参数req进程需要的内存大小，返回值是指向合适的MemoryBlock的指针；
    MemoryBlock* findFirstFit(int req) const;
    MemoryBlock* findBestFit(int req) const;
    MemoryBlock* findWorstFit(int req) const;
    //函数头后面加上const表示这个函数不会修改类的成员变量，可以在const对象上调用，防止误修改成员变量；也就是只读函数


    // 辅助链表/块操作
    void splitBlock(MemoryBlock* block, int req);    // 将 一个空闲块block 切分出 req 的前/后部分
    //这里的splitBlock函数是将合适的空闲block进行切分，block的大小可能大于等于req，如果大于req，就切分出一个大小为req的块来分配给进程，剩余的部分继续作为一个空闲块存在链表中；如果block的大小等于req，就直接分配给进程，不需要切分。
    void coalesce(MemoryBlock* block);               // 合并 block 与相邻空闲块（prev/next）
    void clearAll();                                 // 析构时清理链表
    bool isValidRequest(int pid, int reqSize) const; // 校验输入参数是否合法
};

#endif // MEMORY_MANAGER_H
