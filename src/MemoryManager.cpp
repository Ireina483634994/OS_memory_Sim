#include "../include/MemoryManager.h"
#include <iomanip>
#include <algorithm>  // 我们使用了std::max这个函数



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


// 对输入参数的校验：请求大小需大于 0，pid 需非负
bool MemoryManager::isValidRequest(int pid, int reqSize) const {
    if (pid < 0) {
        std::cout << "Error: PID must be non-negative.\n";
        return false;
    }
    if (reqSize <= 0) {
        std::cout << "Error: requested size must be positive.\n";
        return false;
    }
    if (reqSize > totalSize) {
        std::cout << "Error: requested size exceeds total memory size.\n";
        return false;
    }
    return true;
}

// 分配函数，要从内存的空闲区域分配reqSize大小的内存给pid的进程使用，使用algo策略（成功返回true，失败返回false）
//这是一个public成员，外部可以使用
bool MemoryManager::allocate(int pid, int reqSize, Algorithm algo, std::string* errMsg) {
    if (!isValidRequest(pid, reqSize)) {
        if (errMsg) *errMsg = "invalid request";
        return false;
    }//首先对输入参数进行校验，如果不合法就直接返回false

    if (pidMap.count(pid) > 0) {
        if (errMsg) *errMsg = "PID already allocated";
        std::cout << "Error: PID " << pid << " already allocated.\n";
        return false;
    }//说明之前pid已经分配了内存，不能重复分配（基于一个进程最多分配一块内存的前提）

    MemoryBlock* block = nullptr;
    switch (algo) {
        case Algorithm::FIRST_FIT:  block = findFirstFit(reqSize); break;
        case Algorithm::BEST_FIT:   block = findBestFit(reqSize); break;
        case Algorithm::WORST_FIT:  block = findWorstFit(reqSize); break;
    }//使得block按照算法指示的方式找到一个合适的空闲块，如果没有找到合适的块，block就会是nullptr

    if (!block) {
        if (errMsg) *errMsg = "no suitable free block (insufficient memory or fragmentation)";
        return false; // 没有找到可用块，比如当前内存碎片过多，无法满足reqSize的连续内存需求，或者本身reqsize过大
    }

    splitBlock(block, reqSize);

    block->processId = pid;
    block->isFree = false;
    pidMap[pid] = block;
    return true;
}


// 释放函数，要释放pid占用的内存，注意合并相邻空闲块
//public成员函数，外部可以使用
bool MemoryManager::deallocate(int pid, bool verbose) {
    if (pid < 0) {
        if (verbose) {
            std::cout << "Warning: PID must be non-negative.\n";
        }
        return false;
    }

    auto it = pidMap.find(pid);
    if (it == pidMap.end()) {
        if (verbose) {
            std::cout << "Warning: PID " << pid << " not found.\n";
        }
        return false;
    }
    //说明pidMap当中找到了这个pid对应的block，我们需要将这个block标记为空闲，并且尝试与相邻的空闲块进行合并，最后从pidMap当中删除这个pid的记录
    MemoryBlock* block = it->second;
    block->isFree = true;
    block->processId = -1;
    coalesce(block);//尝试与相邻的空闲块进行合并
    pidMap.erase(it);
    return true;
}


// 可视化函数，显示当前内存的分布情况，使用ASCII字符表示空闲块和占用块
//public成员函数，外部可以使用
void MemoryManager::showMemoryMap() const {
    std::cout << "内存可视化:\n";
    MemoryBlock* cur = head;
    while (cur) {
        // use printf-style formatting to avoid stream state issues
        printf("[%04d, %04d] ", cur->startAddr, cur->startAddr + cur->size - 1);
        
        // switch back to cout for the rest
        if (cur->isFree) {
            std::cout << std::left << std::setw(12) << "Free";
        } else {
            std::cout << std::left << std::setw(12) << ("PID: " + std::to_string(cur->processId));
        }
        std::cout << "| Size: " << cur->size << "\n";
        
        cur = cur->next;
    }//打印这个内存块的起始地址和结束地址，以及这个内存块是空闲的还是被哪个pid占用的，以及这个内存块的大小
}

// 统计数据接口，便于 compare 等逻辑复用
//public成员函数，外部可以使用
MemoryStats MemoryManager::getStats() const {
        // compute free block statistics
    MemoryStats stats{0, 0, 0, 0.0, 0, 0.0, 0.0};
    MemoryBlock* cur = head;
    while (cur) {
        if (cur->isFree) {
            stats.freeBlocks++;
            stats.totalFree += cur->size;
            stats.maxFreeBlock = std::max(stats.maxFreeBlock, cur->size);
            if (stats.minFreeBlock == 0 || cur->size < stats.minFreeBlock) {
                stats.minFreeBlock = cur->size;
            }
        }
        cur = cur->next;
    }

    if (stats.totalFree > 0) {
        stats.externalFragmentation = static_cast<double>(stats.totalFree - stats.maxFreeBlock) / static_cast<double>(stats.totalFree);
        stats.avgFreeBlock = static_cast<double>(stats.totalFree) / stats.freeBlocks;
    }
    // utilization uses totalSize which is member
    stats.utilization = static_cast<double>(totalSize - stats.totalFree) / static_cast<double>(totalSize);
    return stats;
}

// 统计函数，显示当前内存的碎片情况，包括空闲块数量、总空闲内存大小、最大空闲块大小等信息
//public成员函数，外部可以使用
void MemoryManager::showStats() const {
    MemoryStats stats = getStats();
    std::cout << "Free Blocks: " << stats.freeBlocks
              << ", Total Free: " << stats.totalFree
              << ", Max Free Block: " << stats.maxFreeBlock
              << ", Min Free Block: " << stats.minFreeBlock
              << ", Avg Free Block: " << std::fixed << std::setprecision(2) << stats.avgFreeBlock
              << ", External Fragmentation Ratio: " << std::fixed << std::setprecision(2)
              << stats.externalFragmentation
              << ", Utilization: " << std::fixed << std::setprecision(2) << stats.utilization
              << "\n";
    //我们调用了getStats()函数来获取当前内存的统计数据，然后将这些数据格式化输出到控制台上，显示空闲块数量、总空闲内存大小、最大空闲块大小以及外部碎片率等信息
}



//接下来是private成员函数的实现，包括查找算法和切分/合并操作，这些函数只能在类的内部被调用，外部无法访问


//首次适应算法，只需从头开始遍历Block链表，找到第一个空闲的且size>=req的块就返回这个块的指针，如果没有找到合适的块就返回nullptr
MemoryBlock* MemoryManager::findFirstFit(int req) const {
    MemoryBlock* cur = head;
    while (cur) {
        if (cur->isFree && cur->size >= req) return cur;
        cur = cur->next;
    }
    return nullptr;
}
//最佳适应算法，我们需要遍历整个Block链表，找到所有空闲的且size>=req的块，比较它们的size，找到size最小的那个块返回它的指针，如果没有找到合适的块就返回nullptr
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
//最坏适应算法，我们需要遍历整个Block链表，找到所有空闲的且size>=req的块，比较它们的size，找到size最大的那个块返回它的指针，如果没有找到合适的块就返回nullptr
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


// 切分函数，将一个指定的空闲块block摘出req大小的前部分来分配给进程，剩余的部分继续作为一个空闲块存在链表中
void MemoryManager::splitBlock(MemoryBlock* block, int req) {
    if (!block || block->size <= req) return;  // 无需切分
    
    // 创建新块代表剩余的空闲部分
    MemoryBlock* newBlock = new MemoryBlock(block->startAddr + req, block->size - req);
    
    // 更新原块的大小
    block->size = req;
    
    // 插入新块到链表
    newBlock->next = block->next;
    newBlock->prev = block;
    if (block->next) block->next->prev = newBlock;
    block->next = newBlock;
}

void MemoryManager::coalesce(MemoryBlock* block) {
    if (!block) return;

    //对于传入的block，已经是个空闲块，我们需要看看它的前驱和后继是否也是空闲的，从而进行合并
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

