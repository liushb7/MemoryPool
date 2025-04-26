#pragma once
#include "Common.h"

namespace myMemoryPool {

class ThreadCache {
public:
    static ThreadCache* getInstance() {
        static thread_local ThreadCache instance;
        return &instance;
    }

    // 提供的对外接口，分配size大小的内存
    void* allocate(size_t size);
    // 释放ptr开始的size大小的内存
    void release(void* ptr, size_t size);
private:
    // 线程本地的链表和链表长度数组分别初始化为全nullptr以及全0
    ThreadCache() {
        freeList_.fill(nullptr);
        freeListSize_.fill(0);
    }

    // ThreadCache本地size大小对应的链表内存块不够，向CentralCache申请
    void* fetchFromCentralCache(size_t size);
    // ThreadCache向CentralCache归还size大小对应的线程本地内存块（当线程本地size大小对应的链表内存块大于一定数量(threshold)时触发）
    void returnToCentralCache(void* start, size_t size);

private:
    // 下面的两个变量没有使用原子结构，和CentralCache中不一样，因为这是线程本地的，不存在线程之间的竞争，无需使用原子结构和互斥锁/自旋锁
    std::array<void*, FREE_LIST_SIZE> freeList_; //线程本地内存块链表数组，每一个freeList_[i]对应一个链表的头节点   
    std::array<size_t, FREE_LIST_SIZE> freeListSize_; //线程本地内存块长度数组
};

}// namespace myMemoryPool