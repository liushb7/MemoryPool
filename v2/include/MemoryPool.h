#pragma once
#include "ThreadCache.h"

namespace myMemoryPool {

class MemoryPool {
public:
    static void* allocate(size_t size) {
        return ThreadCache::getInstance()->allocate(size);
    }

    static void release(void* ptr, size_t size) {
        ThreadCache::getInstance()->release(ptr, size);
    }
};

} // namespace myMemoryPool