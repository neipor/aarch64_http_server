#ifndef ASM_MEMPOOL_H
#define ASM_MEMPOOL_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

// 内存池块大小定义
#define MEMPOOL_BLOCK_SIZE_SMALL    128     // 128B
#define MEMPOOL_BLOCK_SIZE_MEDIUM   1024    // 1KB
#define MEMPOOL_BLOCK_SIZE_LARGE    8192    // 8KB
#define MEMPOOL_BLOCK_SIZE_HUGE     65536   // 64KB

// 内存池类型
typedef enum {
    MEMPOOL_TYPE_SMALL = 0,
    MEMPOOL_TYPE_MEDIUM,
    MEMPOOL_TYPE_LARGE,
    MEMPOOL_TYPE_HUGE,
    MEMPOOL_TYPE_MAX
} mempool_type_t;

// 内存块结构
typedef struct mempool_block {
    struct mempool_block* next;
    uint32_t size;
    uint32_t magic;
    uint8_t data[0];
} mempool_block_t;

// 内存池统计信息
typedef struct {
    uint64_t total_allocated;
    uint64_t total_freed;
    uint64_t current_usage;
    uint64_t peak_usage;
    uint64_t allocation_count;
    uint64_t free_count;
    uint64_t cache_hits;
    uint64_t cache_misses;
} mempool_stats_t;

// 内存池结构
typedef struct {
    mempool_block_t* free_blocks[MEMPOOL_TYPE_MAX];
    size_t block_sizes[MEMPOOL_TYPE_MAX];
    size_t pool_sizes[MEMPOOL_TYPE_MAX];
    
    pthread_mutex_t mutex;
    mempool_stats_t stats;
    
    // 汇编优化标志
    int use_asm_opt;
    uint32_t cpu_features;
    
    // 内存对齐
    size_t alignment;
    
    // 预分配池
    void* preallocated_pools[MEMPOOL_TYPE_MAX];
    size_t preallocated_sizes[MEMPOOL_TYPE_MAX];
    
    // 垃圾回收
    uint64_t gc_threshold;
    uint64_t last_gc_time;
    
} mempool_t;

// 内存池创建和销毁
mempool_t* mempool_create(size_t initial_size);
void mempool_destroy(mempool_t* pool);

// 内存分配和释放
void* mempool_alloc(mempool_t* pool, size_t size);
void* mempool_calloc(mempool_t* pool, size_t nmemb, size_t size);
void* mempool_realloc(mempool_t* pool, void* ptr, size_t new_size);
void mempool_free(mempool_t* pool, void* ptr);

// 汇编优化的内存操作
void* mempool_alloc_asm(mempool_t* pool, size_t size);
void mempool_free_asm(mempool_t* pool, void* ptr);

// 内存池管理
int mempool_prealloc(mempool_t* pool, mempool_type_t type, size_t count);
void mempool_gc(mempool_t* pool);
void mempool_reset(mempool_t* pool);

// 统计信息
mempool_stats_t* mempool_get_stats(mempool_t* pool);
void mempool_print_stats(mempool_t* pool);
void mempool_reset_stats(mempool_t* pool);

// 内存对齐
void* mempool_alloc_aligned(mempool_t* pool, size_t size, size_t alignment);
void* mempool_alloc_aligned_asm(mempool_t* pool, size_t size, size_t alignment);

// 内存池配置
void mempool_set_gc_threshold(mempool_t* pool, uint64_t threshold);
void mempool_enable_asm_opt(mempool_t* pool, int enable);
size_t mempool_get_block_size(size_t requested_size);

// 内存池验证
int mempool_validate(mempool_t* pool);
int mempool_check_block(mempool_t* pool, void* ptr);

// 线程安全版本
typedef struct {
    mempool_t* pools[MEMPOOL_TYPE_MAX];
    pthread_key_t thread_key;
    pthread_mutex_t global_mutex;
} mempool_manager_t;

mempool_manager_t* mempool_manager_create(void);
void mempool_manager_destroy(mempool_manager_t* manager);
void* mempool_manager_alloc(mempool_manager_t* manager, size_t size);
void mempool_manager_free(mempool_manager_t* manager, void* ptr);

// 全局内存池（单例）
extern mempool_manager_t* global_mempool_manager;

// 便捷宏定义
#define MEMPOOL_ALLOC(size) mempool_manager_alloc(global_mempool_manager, size)
#define MEMPOOL_FREE(ptr) mempool_manager_free(global_mempool_manager, ptr)
#define MEMPOOL_INIT() (global_mempool_manager = mempool_manager_create())
#define MEMPOOL_CLEANUP() mempool_manager_destroy(global_mempool_manager)

// 内存池配置
typedef struct {
    size_t small_pool_size;
    size_t medium_pool_size;
    size_t large_pool_size;
    size_t huge_pool_size;
    size_t alignment;
    int enable_gc;
    int enable_asm_opt;
    uint64_t gc_threshold;
} mempool_config_t;

mempool_config_t* mempool_get_default_config(void);
mempool_t* mempool_create_with_config(mempool_config_t* config);

// 内存池调试
void mempool_dump_blocks(mempool_t* pool);
void mempool_check_leaks(mempool_t* pool);

#endif // ASM_MEMPOOL_H 