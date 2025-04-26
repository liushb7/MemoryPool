#pragma once
#include "Common.h"
#include <map>
#include <mutex>

namespace myMemoryPool {

class PageCache {
public:
    // 固定页大小为4KB
    static const size_t PAGE_SIZE = 4 * 1024;

    static PageCache& getInstance() {
        static PageCache instance;
        return instance;
    }

    // PageCache分配Span(若干个Page)
    void* allocateSpan(size_t numPages);
    // PageCache回收Span
    void releaseSpan(void* ptr, size_t numPages);
private:
    // 默认构造函数，即：
    // PageCache() {}
    PageCache() = default;

    // 系统内存申请
    void* systemAllocate(size_t numPages);
private:

    // Span结构体定义，用于构建链表
    struct Span {
        void* pageAddr;  //Span内存开始地址
        size_t numPages; //Span包含的Page数量
        Span* next;      //next指针指向下一个Span
    };

    std::map<size_t, Span*> pageNumToSpan_; //不同的pageNum映射到不同的Span链表，每一个pageNumToSpan_[pageNum]表示对应Span链表的头节点
    std::map<void*, Span*> addressToSpan_; //地址（指针）到Span的映射，在合并相邻空闲Span的时候会用上
    std::mutex mutex_; // 互斥锁，用于对PageCache的互斥访问
};

}// namespace myMemoryPool