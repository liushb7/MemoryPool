#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"
#include <cstdlib>

namespace myMemoryPool {

const size_t threshold = 64;

void* ThreadCache::allocate(size_t size) {
    if(size == 0) {
        size = ALIGNMENT;
    }

    if(size > MAX_BYTES) {
        return malloc(size);
    }

    size_t index = SizeClass::getIndex(size);

    freeListSize_[index]--;
    if(void* ptr = freeList_[index]) {
        freeList_[index] = *reinterpret_cast<void**>(ptr);
        return ptr;
    }

    return fetchFromCentralCache(index);
}

void ThreadCache::release(void* ptr, size_t size) {
    if(size > MAX_BYTES) {
        free(ptr);
        return;
    }

    size_t index = SizeClass::getIndex(size);

    *reinterpret_cast<void**>(ptr) = freeList_[index];
    freeList_[index] = ptr;
    freeListSize_[index]++;

    if(freeListSize_[index] >= threshold) {
        returnToCentralCache(freeList_[index], size);
    }    
}

void* ThreadCache::fetchFromCentralCache(size_t index) {
    void* start = CentralCache::getInstance().fetchMemory(index);
    if(!start) return nullptr;

    void* result = start;
    freeList_[index] = *reinterpret_cast<void**>(start);

    size_t batchNum = 0;
    void* cur = start;

    while(cur != nullptr) {
        batchNum ++;
        cur = *reinterpret_cast<void**>(cur);
    }

    freeListSize_[index] += batchNum;

    return result;
}

void ThreadCache::returnToCentralCache(void* start, size_t size) {
    size_t index = SizeClass::getIndex(size);

    size_t batchNum = freeListSize_[index];
    
    size_t keepNum = batchNum / 4;
    // size_t returnNum = batchNum - keepNum;

    void* cur = start;
    for(size_t i = 0; i < keepNum - 1; i ++) {
        cur = *reinterpret_cast<void**>(cur);
    }

    if(cur != nullptr) {
        void* next = *reinterpret_cast<void**>(cur);
        *reinterpret_cast<void**>(cur) = nullptr;

        freeList_[index] = start;
        freeListSize_[index] = keepNum;

        CentralCache::getInstance().returnMemory(next, index);
    }
}

} // namespace myMemoryPool

