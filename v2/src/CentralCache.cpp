#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <thread>

namespace myMemoryPool {

// 每次从PageCache获取的Span页数最小值(单位为页)
static const size_t SPAN_PAGES = 8;

void* CentralCache::fetchMemory(size_t index) {
    if(index >= FREE_LIST_SIZE) return nullptr;

    while(locks_[index].test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    void* result = nullptr;
    
    try {
        result = centralFreeList_[index].load(std::memory_order_acquire);

        if(!result) {
            size_t size = (index + 1) * ALIGNMENT;
            result = fetchFromPageCache(size);
            
            if(!result) {
                locks_[index].clear(std::memory_order_release);
                return nullptr;
            }

            char* start = static_cast<char*>(result);
            
            size_t blockNum = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;

            // blockNum = 0/1 的时候直接return result
            if(blockNum > 1) {
                for(size_t i = 1; i < blockNum; i ++) {
                    void* cur = start + (i - 1) * size;
                    void* next = start + i * size;
                    // 相当于结构体链表中的cur->next
                    *reinterpret_cast<void**>(cur) = next;
                }

                *reinterpret_cast<void**>(start + (blockNum - 1) * size) = nullptr;

                void* next = *reinterpret_cast<void**>(result);
                *reinterpret_cast<void**>(result) = nullptr;

                centralFreeList_[index].store(next, std::memory_order_release);
            }
        } else {
            void* next = *reinterpret_cast<void**>(result);
            *reinterpret_cast<void**>(result) = nullptr;

            centralFreeList_[index].store(next, std::memory_order_release);
        }

    } catch(...) {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    locks_[index].clear(std::memory_order_release);
    return result;
}

void CentralCache::returnMemory(void* start, size_t index) {
    if(!start || index >= FREE_LIST_SIZE) return;

    while(locks_[index].test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    try {

        // 找到要归还的链表的尾节点
        void* end = start;
        
        while(*reinterpret_cast<void**>(end) != nullptr) {
            end = *reinterpret_cast<void**>(end);
        }

        // 头插法
        void* cur = centralFreeList_[index].load(std::memory_order_acquire);
        *reinterpret_cast<void**>(end) = cur;
        centralFreeList_[index].store(start, std::memory_order_release);
    } catch (...) {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    locks_[index].clear(std::memory_order_release);
}

void* CentralCache::fetchFromPageCache(size_t size) {
    size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

    if(size <= SPAN_PAGES * PageCache::PAGE_SIZE) {
        return PageCache::getInstance().allocateSpan(SPAN_PAGES);
    }else {
        return PageCache::getInstance().allocateSpan(numPages);
    }
}

} // namespace myMemoryPool