#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <thread>

namespace myMemoryPool {

// 每次从PageCache获取的Span页数最小值(单位为页)
static const size_t SPAN_PAGES = 8;

void* CentralCache::fetchMemory(size_t index) {
    if(index >= FREE_LIST_SIZE) return nullptr;

    // test_and_set尝试获取锁，如果锁被占用，返回true，一直在while等
    while(locks_[index].test_and_set(std::memory_order_acquire)) {
        // 让当前线程主动放弃CPU执行权，避免忙等待
        std::this_thread::yield();
    }

    void* result = nullptr;
    
    try {
        // atomic变量获取值用load + std::memory_order_acquire
        result = centralFreeList_[index].load(std::memory_order_acquire);
        
        // CentralCache没有对应大小的内存块，就向PageCache申请
        if(!result) {
            // 由于index从0开始，因此要加1
            size_t size = (index + 1) * ALIGNMENT;
            result = fetchFromPageCache(size);
            
            if(!result) {
                locks_[index].clear(std::memory_order_release);
                return nullptr;
            }

            char* start = static_cast<char*>(result);
            
            // 当size 大于8*4KB即32KB的时候，blockNum = 0
            size_t blockNum = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;

            // blockNum = 0/1 的时候直接return result
            if(blockNum > 1) {
                // 创建链表
                for(size_t i = 1; i < blockNum; i ++) {
                    void* cur = start + (i - 1) * size;
                    void* next = start + i * size;
                    // 相当于结构体链表中的cur->next
                    *reinterpret_cast<void**>(cur) = next;
                }
                // 最后一个节点的next为nullptr
                *reinterpret_cast<void**>(start + (blockNum - 1) * size) = nullptr;

                // 把头结点断开，下一个节点next作为链表头结点
                void* next = *reinterpret_cast<void**>(result);
                *reinterpret_cast<void**>(result) = nullptr;
                centralFreeList_[index].store(next, std::memory_order_release);
            }
        } else {
            // CentralCache有对应大小的内存，直接把头结点返回，next作为新链表的头节点
            void* next = *reinterpret_cast<void**>(result);
            *reinterpret_cast<void**>(result) = nullptr;
            centralFreeList_[index].store(next, std::memory_order_release);
        }

    } catch(...) {
        // 解锁使用clear + std::memeory_order_release
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    // 解锁使用clear + std::memeory_order_release
    locks_[index].clear(std::memory_order_release);
    return result;
}

void CentralCache::returnMemory(void* start, size_t index) {
    if(!start || index >= FREE_LIST_SIZE) return;

    while(locks_[index].test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    try {

        // 由于要归还的是一条链表，因此进行头插法的话要找到这条链表的尾节点
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
    // 向上取整
    size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

    // 申请内存小于等于8页，一律按照8页进行分配（多余的可以进行分块，保存在CentralCache）
    if(size <= SPAN_PAGES * PageCache::PAGE_SIZE) {
        return PageCache::getInstance().allocateSpan(SPAN_PAGES);
    }else {
    // 否则按照实际需要内存大小进行分配
        return PageCache::getInstance().allocateSpan(numPages);
    }
}

} // namespace myMemoryPool