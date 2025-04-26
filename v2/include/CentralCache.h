#pragma once
#include "Common.h"
#include <mutex>

namespace myMemoryPool {

class CentralCache {
public:
    static CentralCache& getInstance() {
        static CentralCache instance;
        return instance;
    }

    //  CentralCache用于分配对应索引位置（映射到对应内存块大小的链表）的内存块给ThreadCache
    void* fetchMemory(size_t index);

    // CentralCache用来接受上层的ThreadCache释放的索引为index的内存块链表，并通过头插法插入到CentralCache对应index的空闲链表
    void returnMemory(void* start, size_t index);

private:
    // 初始化为链表全空，以及lock全为false
    CentralCache() {
        for(auto& ptr : centralFreeList_) {
            ptr.store(nullptr, std::memory_order_relaxed);
        }

        for(auto& lock : locks_) {
            lock.clear();
        }
    }

    // CentralCache内存块不够上层ThreadCache使用时，向下层的PageCache申请新的内存
    void* fetchFromPageCache(size_t size);

private:
    
    std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_; // 不同大小内存块对应的链表
    std::array<std::atomic_flag, FREE_LIST_SIZE> locks_; // 不同大小内存块链表对应的lock
};
} // namespace myMemoryPool