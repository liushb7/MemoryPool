#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"
#include <cstdlib>

namespace myMemoryPool {

const size_t threshold = 64;

void* ThreadCache::allocate(size_t size) {
    // size为0补到对齐值
    if(size == 0) {
        size = ALIGNMENT;
    }

    // size超过最大分配内存256KB，使用系统malloc
    if(size > MAX_BYTES) {
        return malloc(size);
    }

    // 计算size对齐之后映射到的index
    size_t index = SizeClass::getIndex(size);

    // 对应链表长度减一，即使可能为空一开始，但是后面的fetchFromCentralCache会增加链表长度
    freeListSize_[index]--;
    // 由于ptr有可能为nullptr，所以要用if
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

    // 链表长度超过阈值，向CentralCache归还部分内存
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

    // 当前的fetchMemory设计的是只返回一个内存块，因此batchNum应该为1
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
    
    // 保留一部分内存，剩下的返回给给CentralCache
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

