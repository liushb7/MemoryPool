# MemoryPool
还有一些地方可以优化：
1. CentralCache::fetchMemory可以进一步修改为批量返回内存
2. 内存释放还没有完善，可以在析构函数中释放内存