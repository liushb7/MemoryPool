#pragma once
#include "Common.h"
#include <map>
#include <mutex>

namespace myMemoryPool {

class PageCache {
public:
    static const size_t PAGE_SIZE = 4 * 1024;

    static PageCache& getInstance() {
        static PageCache instance;
        return instance;
    }

    void* allocateSpan(size_t numPages);
    void releaseSpan(void* ptr, size_t numPages);
private:
    PageCache() = default;

    void* systemAllocate(size_t numPages);
private:
    struct Span {
        void* pageAddr;
        size_t numPages;
        Span* next;
    };

    std::map<size_t, Span*> pageNumToSpan_;
    std::map<void*, Span*> addressToSpan_;
    std::mutex mutex_;
};

}// namespace myMemoryPool