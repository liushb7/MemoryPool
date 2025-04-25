#pragma once
#include "Common.h"

namespace myMemoryPool {

class ThreadCache {
public:
    static ThreadCache* getInstance() {
        static thread_local ThreadCache instance;
        return &instance;
    }

    void* allocate(size_t size);
    void release(void* ptr, size_t size);
private:
    ThreadCache() {
        freeList_.fill(nullptr);
        freeListSize_.fill(0);
    }

    void* fetchFromCentralCache(size_t size);
    void returnToCentralCache(void* start, size_t size);

private:
    std::array<void*, FREE_LIST_SIZE> freeList_;
    std::array<size_t, FREE_LIST_SIZE> freeListSize_;
};

}// namespace myMemoryPool