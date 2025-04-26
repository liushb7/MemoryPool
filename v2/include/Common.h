#pragma once
#include <cstddef>
#include <atomic>
#include <array>

namespace myMemoryPool {

// 对齐值为8，即内存地址为8的整数倍
constexpr size_t ALIGNMENT = 8;
constexpr size_t MAX_BYTES = 256 * 1024;
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;

class SizeClass {
public:
    // 不是8的整数倍的size被填充至整数倍
    static size_t roundUp(size_t bytes) {
        return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    // 获取bytes字节大小的内存映射到的索引

    // FreeList_中索引与内存块的对应关系：
    // 索引                 内存块大小
    // 0                        8
    // 1                        16
    // 2                        24
    // ...
    // FREE_LIST_SIZE       MAX_BYTES
    static size_t getIndex(size_t bytes) {
        bytes = std::max(bytes, ALIGNMENT);
        // 0-based
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }
};

}// namespace myMemoryPool