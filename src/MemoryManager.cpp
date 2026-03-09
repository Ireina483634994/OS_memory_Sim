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


// 参数校验：请求大小需大于 0，pid 需非负
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
bool MemoryManager::allocate(int pid, int reqSize, Algorithm algo) {
    if (!isValidRequest(pid, reqSize)) return false;

    if (pidMap.count(pid) > 0) {
        std::cout << "Error: PID " << pid << " already allocated.\n";
        return false;
    }//说明之前pid已经分配了内存，不能重复分配

    MemoryBlock* block = nullptr;
    switch (algo) {
        case Algorithm::FIRST_FIT:  block = findFirstFit(reqSize); break;
        case Algorithm::BEST_FIT:   block = findBestFit(reqSize); break;
        case Algorithm::WORST_FIT:  block = findWorstFit(reqSize); break;
    }//使得block按照算法指示的方式找到一个合适的空闲块，如果没有找到合适的块，block就会是nullptr

    if (!block) return false; // 没有找到可用块，比如当前内存碎片过多，无法满足reqSize的连续内存需求，或者本身reqsize过大

    splitBlock(block, reqSize);//对指定的block，在block当中切分出一个大小为reqSize的块来分配给pid（将block的前部分给进程，后半部分是空闲的），剩余的部分继续作为一个空闲块存在链表中；如果block的大小等于reqSize，就直接分配给pid，不需要切分
    block->processId = pid;//说明block的前半部分分配给了进程pid，因此这个block就不再是空闲的了，设置isFree为false，并且记录这个block是被哪个pid占用的
    block->isFree = false;

    pidMap[pid] = block;
    return true;
}


// 释放函数，要释放pid占用的内存，注意合并相邻空闲块
//public成员函数，外部可以使用
void MemoryManager::deallocate(int pid) {
    if (pid < 0) {
        std::cout << "Warning: PID must be non-negative.\n";
        return;
    }

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
    std::cout << "内存可视化:\n";
    MemoryBlock* cur = head;
    while (cur) {
        std::cout << "[" << cur->startAddr << ", " << cur->startAddr + cur->size - 1 << "] "
                  << (cur->isFree ? "Free" : "PID: " + std::to_string(cur->processId))
                  << " | Size: " << cur->size << "\n";
        cur = cur->next;
    }//打印这个内存块的起始地址和结束地址，以及这个内存块是空闲的还是被哪个pid占用的，以及这个内存块的大小
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
    double externalFrag = 0.0;
    if (freeTotal > 0) {
        externalFrag = 1.0 - static_cast<double>(maxFree) / static_cast<double>(freeTotal);
    }

    std::cout << "Free Blocks: " << freeCount
              << ", Total Free: " << freeTotal
              << ", Max Free Block: " << maxFree
              << ", External Fragmentation Ratio: " << std::fixed << std::setprecision(2)
              << externalFrag << "\n";
}



//接下来是private成员函数的实现，包括查找算法和切分/合并操作，这些函数只能在类的内部被调用，外部无法访问

// -------------------- 查找算法 --------------------
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

// -------------------- 切分 / 合并 --------------------

// 切分函数，将一个指定的空闲块block摘出req大小的前部分来分配给进程，剩余的部分继续作为一个空闲块存在链表中（额外创建一个block插入相应位置）；如果block的大小等于req，就直接将block自己分配给进程，不需要切分
void MemoryManager::splitBlock(MemoryBlock* block, int req) {
    if (!block || block->size <= req) return;

    // 新建剩余空闲块
    MemoryBlock* remainder = new MemoryBlock(block->startAddr + req, block->size - req);
    remainder->isFree = true;
    remainder->prev = block;
    remainder->next = block->next;//remainder插在block和block->next之间
    if (block->next) block->next->prev = remainder;
    block->next = remainder;
    block->size = req;
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

