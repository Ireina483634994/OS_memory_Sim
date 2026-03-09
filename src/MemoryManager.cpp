#include "../include/MemoryManager.h"
#include <iomanip>
#include <algorithm>  // std::max



// 构造函数（类的public成员函数，用于类初始化时）：初始化一个总大小为 totalSize 的内存空闲区域（从地址 0 开始）
MemoryManager::MemoryManager(int size) {
    totalSize = size;
    head = new MemoryBlock(0, size); // 在堆区上申请一个block，模拟初始只有一整块空闲状态的内存的情况，地址从0开始，大小为 totalSize
    pidMap.clear();
}//左边的Manager表示类名，右边的Manager表示这个类当中的构造函数的名称

// 析构函数（类的public成员函数，用于类销毁时）：释放链表中的所有块
// 析构函数会在对象生命周期结束时自动调用，负责清理资源，防止内存泄漏
//在构造函数前面加上~符号表示这是一个析构函数，函数名必须和类名相同，前面加上~表示这是一个析构函数
//由于是析构函数，因此在main.cpp当中一旦main()函数结束，或者实例化class的对象离开作用域，析构函数就会被自动调用，负责清理资源，防止内存泄漏，不用手动释放内存
MemoryManager::~MemoryManager() {
    clearAll();
}

// 释放整个链表，这是一个private成员函数，外部无法访问，只有在析构函数当中调用，或者在需要清理链表的时候调用
void MemoryManager::clearAll() {
    MemoryBlock* cur = head;
    while (cur) {
        MemoryBlock* tmp = cur;
        cur = cur->next;
        delete tmp;
    }
    head = nullptr;
    pidMap.clear();
}


// 分配函数，要从内存的空闲区域分配reqSize大小的内存给pid的进程使用，使用algo策略（成功返回true，失败返回false）
//这是一个public成员，外部可以使用
bool MemoryManager::allocate(int pid, int reqSize, Algorithm algo) {
    if (pidMap.count(pid) > 0) {
        std::cout << "Error: PID " << pid << " already allocated.\n";
        return false;
    }

    MemoryBlock* block = nullptr;
    switch (algo) {
        case Algorithm::FIRST_FIT:  block = findFirstFit(reqSize); break;
        case Algorithm::BEST_FIT:   block = findBestFit(reqSize); break;
        case Algorithm::WORST_FIT:  block = findWorstFit(reqSize); break;
    }

    if (!block) return false; // 没有找到可用块

    splitBlock(block, reqSize);
    block->processId = pid;
    block->isFree = false;

    pidMap[pid] = block;
    return true;
}


// 释放函数，要释放pid占用的内存，注意合并相邻空闲块
//public成员函数，外部可以使用
void MemoryManager::deallocate(int pid) {
    auto it = pidMap.find(pid);
    if (it == pidMap.end()) {
        std::cout << "Warning: PID " << pid << " not found.\n";
        return;
    }
    MemoryBlock* block = it->second;
    block->isFree = true;
    block->processId = -1;
    coalesce(block);
    pidMap.erase(it);
}


// 可视化函数，显示当前内存的分布情况，使用ASCII字符表示空闲块和占用块
//public成员函数，外部可以使用
void MemoryManager::showMemoryMap() const {
    std::cout << "Memory Map:\n";
    MemoryBlock* cur = head;
    while (cur) {
        std::cout << "[" << cur->startAddr << ", " << cur->startAddr + cur->size - 1 << "] "
                  << (cur->isFree ? "Free" : "PID " + std::to_string(cur->processId))
                  << " | Size: " << cur->size << "\n";
        cur = cur->next;
    }
}

// 统计函数，显示当前内存的碎片情况，包括空闲块数量、总空闲内存大小、最大空闲块大小等信息
//public成员函数，外部可以使用
void MemoryManager::showStats() const {
    int freeCount = 0, freeTotal = 0, maxFree = 0;
    MemoryBlock* cur = head;
    while (cur) {
        if (cur->isFree) {
            freeCount++;
            freeTotal += cur->size;
            maxFree = std::max(maxFree, cur->size);
        }
        cur = cur->next;
    }
    std::cout << "Free Blocks: " << freeCount
              << ", Total Free: " << freeTotal
              << ", Max Free Block: " << maxFree << "\n";
}



//接下来是private成员函数的实现，包括查找算法和切分/合并操作，这些函数只能在类的内部被调用，外部无法访问

// -------------------- 查找算法 --------------------
MemoryBlock* MemoryManager::findFirstFit(int req) const {
    MemoryBlock* cur = head;
    while (cur) {
        if (cur->isFree && cur->size >= req) return cur;
        cur = cur->next;
    }
    return nullptr;
}

MemoryBlock* MemoryManager::findBestFit(int req) const {
    MemoryBlock* cur = head;
    MemoryBlock* best = nullptr;
    int minSize = std::numeric_limits<int>::max();
    while (cur) {
        if (cur->isFree && cur->size >= req && cur->size < minSize) {
            best = cur;
            minSize = cur->size;
        }
        cur = cur->next;
    }
    return best;
}

MemoryBlock* MemoryManager::findWorstFit(int req) const {
    MemoryBlock* cur = head;
    MemoryBlock* worst = nullptr;
    int maxSize = -1;
    while (cur) {
        if (cur->isFree && cur->size >= req && cur->size > maxSize) {
            worst = cur;
            maxSize = cur->size;
        }
        cur = cur->next;
    }
    return worst;
}

// -------------------- 切分 / 合并 --------------------
void MemoryManager::splitBlock(MemoryBlock* block, int req) {
    if (!block || block->size <= req) return;

    // 新建剩余空闲块
    MemoryBlock* remainder = new MemoryBlock(block->startAddr + req, block->size - req);
    remainder->isFree = true;
    remainder->prev = block;
    remainder->next = block->next;
    if (block->next) block->next->prev = remainder;
    block->next = remainder;
    block->size = req;
}

void MemoryManager::coalesce(MemoryBlock* block) {
    if (!block) return;

    // 向前合并
    if (block->prev && block->prev->isFree) {
        MemoryBlock* L = block->prev;
        L->size += block->size;
        L->next = block->next;
        if (block->next) block->next->prev = L;
        delete block;
        block = L;
    }
    // 向后合并
    if (block->next && block->next->isFree) {
        MemoryBlock* R = block->next;
        block->size += R->size;
        block->next = R->next;
        if (R->next) R->next->prev = block;
        delete R;
    }
}

