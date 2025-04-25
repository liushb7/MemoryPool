#include "PageCache.h"
#include <sys/mman.h>
#include <cstring>

namespace myMemoryPool {

void* PageCache::allocateSpan(size_t numPages) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pageNumToSpan_.lower_bound(numPages);
    if(it != pageNumToSpan_.end()) {
        Span* span = it->second;
        
        if(span->next) {
            pageNumToSpan_[it->first] = span->next;
        }else {
            pageNumToSpan_.erase(it);
        }
        
        if(span->numPages > numPages) {
            Span* newSpan = new Span;
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) + numPages * PAGE_SIZE;
            newSpan->numPages = span->numPages - numPages;
            newSpan->next = nullptr;

            auto& list = pageNumToSpan_[newSpan->numPages];
            newSpan->next = list;
            list = newSpan;

            span->numPages = numPages;
        }

        addressToSpan_[span->pageAddr] = span;
        return span->pageAddr;
    }

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
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
    auto nextIt = addressToSpan_.find(nextAddr);
    
    if(nextIt != addressToSpan_.end()) {
        Span* nextSpan = nextIt->second;

        bool found = false;
        auto& nextList = pageNumToSpan_[nextSpan->numPages];

        if(nextList == nextSpan) {
            nextList = nextSpan->next;
            found = true;
        }else if(nextList) {
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

        if(found) {
            span->numPages += nextSpan->numPages;
            addressToSpan_.erase(nextAddr);
            delete nextSpan;
        }
    }

    auto& list = pageNumToSpan_[span->numPages];
    span->next = list;
    list = span;
}

void* PageCache::systemAllocate(size_t numPages) {
    size_t size = numPages * PAGE_SIZE;

    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(ptr == MAP_FAILED) return nullptr;

    memset(ptr, 0, size);
    return ptr;
}

} // namespace myMemoryPool