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

    void* fetchMemory(size_t index);
    void returnMemory(void* start, size_t index);

private:
    CentralCache() {
        for(auto& ptr : centralFreeList_) {
            ptr.store(nullptr, std::memory_order_relaxed);
        }

        for(auto& lock : locks_) {
            lock.clear();
        }
    }

    void* fetchFromPageCache(size_t size);

private:
    
    std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;
    std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
};
} // namespace myMemoryPool