#include "PageCache.h"
#include <sys/mman.h>
#include <cstring>

namespace myMemoryPool {

void* PageCache::allocateSpan(size_t numPages) {
    // 进入函数自动lock，离开函数自动unlock
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找第一个大于等于要求的numPages的项，多余的页可以重新插入到新的链表中
    auto it = pageNumToSpan_.lower_bound(numPages);
    if(it != pageNumToSpan_.end()) {
        Span* span = it->second;
        
        // span->next：相同页大小的链表的下一个节点
        if(span->next) {
            pageNumToSpan_[it->first] = span->next;
        }else {
            // 如果这个页数对应的链表只有一个节点（即头节点），那么从map中直接删除
            pageNumToSpan_.erase(it);
        }
        
        // 查找的页数过多，需要把多余的重新插入到新的链表中
        if(span->numPages > numPages) {
            Span* newSpan = new Span;
            // span->pageAddr进行加法之前要转换成char*类型
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) + numPages * PAGE_SIZE;
            newSpan->numPages = span->numPages - numPages;
            newSpan->next = nullptr;
            
            // 把newSpan插入到剩余页数对应的链表的头部（头插法）
            auto& list = pageNumToSpan_[newSpan->numPages];
            newSpan->next = list;
            list = newSpan;

            span->numPages = numPages;
        }
        // 记录地址到Span的映射
        addressToSpan_[span->pageAddr] = span;
        return span->pageAddr;
    }
    // 向系统申请内存
    void* sysMemory = systemAllocate(numPages);
    if(!sysMemory) return nullptr;

    Span* span = new Span;
    span->pageAddr = sysMemory;
    span->numPages = numPages;
    span->next = nullptr;

    addressToSpan_[span->pageAddr] = span;
    return sysMemory;
}

void PageCache::releaseSpan(void* ptr, size_t numPages) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = addressToSpan_.find(ptr);
    // 按道理，通过PageCache申请的内存也是通过PageCache来释放，下面这句可能用不上
    if(it == addressToSpan_.end()) return;

    Span* span = it->second;
    // 尝试合并相邻的下一个Span，如果下一个Span也是空闲的话（即在Span链表中）
    // 注意这里的合并相邻Span只考虑了当前Span和右边的空闲Span合并，没有考虑和左边的Span进行合并
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
    auto nextIt = addressToSpan_.find(nextAddr);
    
    if(nextIt != addressToSpan_.end()) {
        Span* nextSpan = nextIt->second;

        bool found = false;
        auto& nextList = pageNumToSpan_[nextSpan->numPages];

        if(nextList == nextSpan) {
            nextList = nextSpan->next;
            found = true;
            // 删除map中的映射？
            // pageNumToSpan_.erase(nextSpan->numPages);
        }else if(nextList) {
            // 找到nextSpan并删除
            Span* prev = nextList;
            while(prev->next) {
                if(prev->next == nextSpan) {
                    prev->next = nextSpan->next;
                    found = true;
                    break;
                }
                prev = prev->next;
            }
        }
        // 如果找到了就进行合并，并把原来的nextAddr从addressToSpan_中删除，并delete nextSpan
        if(found) {
            span->numPages += nextSpan->numPages;
            addressToSpan_.erase(nextAddr);
            delete nextSpan;
        }
    }

    // 把归还的Span（可能和右边相邻的空闲Span进行了合并）插入到对应页数的链表中
    auto& list = pageNumToSpan_[span->numPages];
    span->next = list;
    list = span;
}

void* PageCache::systemAllocate(size_t numPages) {
    size_t size = numPages * PAGE_SIZE;
    
    // 使用mmap进行系统大块内存申请更高效
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(ptr == MAP_FAILED) return nullptr;

    memset(ptr, 0, size);
    return ptr;
}

} // namespace myMemoryPool