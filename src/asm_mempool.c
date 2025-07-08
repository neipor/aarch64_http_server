#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "asm_mempool.h"
#include "asm_opt.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

// 内存块魔数
#define MEMPOOL_BLOCK_MAGIC 0xDEADBEEF
#define MEMPOOL_FREE_MAGIC  0xFEEDFACE

// 全局内存池管理器
mempool_manager_t* global_mempool_manager = NULL;

// 获取默认配置
mempool_config_t* mempool_get_default_config(void) {
    static mempool_config_t default_config = {
        .small_pool_size = 1024 * 1024,      // 1MB
        .medium_pool_size = 4 * 1024 * 1024, // 4MB
        .large_pool_size = 16 * 1024 * 1024, // 16MB
        .huge_pool_size = 64 * 1024 * 1024,  // 64MB
        .alignment = 16,                      // 16字节对齐
        .enable_gc = 1,
        .enable_asm_opt = 1,
        .gc_threshold = 1024 * 1024 * 1024   // 1GB
    };
    return &default_config;
}

// 获取块大小类型
static mempool_type_t get_block_type(size_t size) {
    if (size <= MEMPOOL_BLOCK_SIZE_SMALL) return MEMPOOL_TYPE_SMALL;
    if (size <= MEMPOOL_BLOCK_SIZE_MEDIUM) return MEMPOOL_TYPE_MEDIUM;
    if (size <= MEMPOOL_BLOCK_SIZE_LARGE) return MEMPOOL_TYPE_LARGE;
    return MEMPOOL_TYPE_HUGE;
}

// 获取实际块大小
size_t mempool_get_block_size(size_t requested_size) {
    mempool_type_t type = get_block_type(requested_size);
    switch (type) {
        case MEMPOOL_TYPE_SMALL: return MEMPOOL_BLOCK_SIZE_SMALL;
        case MEMPOOL_TYPE_MEDIUM: return MEMPOOL_BLOCK_SIZE_MEDIUM;
        case MEMPOOL_TYPE_LARGE: return MEMPOOL_BLOCK_SIZE_LARGE;
        case MEMPOOL_TYPE_HUGE: return MEMPOOL_BLOCK_SIZE_HUGE;
        default: return requested_size;
    }
}

// 汇编优化的内存对齐分配
static void* mempool_alloc_aligned_asm_impl(size_t size, size_t alignment) {
    #ifdef __aarch64__
    void* ptr = NULL;
    
    // 使用汇编优化的对齐分配
    __asm__ volatile (
        "mov x0, %1\n\t"           // size
        "mov x1, %2\n\t"           // alignment
        "add x0, x0, x1\n\t"       // size + alignment
        "sub x0, x0, #1\n\t"       // size + alignment - 1
        "mov x2, #0\n\t"           // flags
        "mov x3, #-1\n\t"          // fd
        "mov x4, #0\n\t"           // offset
        "mov x8, #222\n\t"         // mmap syscall
        "svc #0\n\t"               // system call
        "mov %0, x0\n\t"           // result
        : "=r"(ptr)
        : "r"(size), "r"(alignment)
        : "x0", "x1", "x2", "x3", "x4", "x8", "memory"
    );
    
    if (ptr == MAP_FAILED) {
        return NULL;
    }
    
    // 对齐指针
    uintptr_t aligned_ptr = ((uintptr_t)ptr + alignment - 1) & ~(alignment - 1);
    return (void*)aligned_ptr;
    #else
    return aligned_alloc(alignment, size);
    #endif
}

// 创建内存池
mempool_t* mempool_create(size_t initial_size) {
    mempool_config_t* config = mempool_get_default_config();
    return mempool_create_with_config(config);
}

// 使用配置创建内存池
mempool_t* mempool_create_with_config(mempool_config_t* config) {
    mempool_t* pool = calloc(1, sizeof(mempool_t));
    if (!pool) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory pool");
        return NULL;
    }
    
    // 初始化基本属性
    pool->alignment = config->alignment;
    pool->use_asm_opt = config->enable_asm_opt;
    pool->cpu_features = asm_opt_get_cpu_features();
    pool->gc_threshold = config->gc_threshold;
    
    // 设置块大小
    pool->block_sizes[MEMPOOL_TYPE_SMALL] = MEMPOOL_BLOCK_SIZE_SMALL;
    pool->block_sizes[MEMPOOL_TYPE_MEDIUM] = MEMPOOL_BLOCK_SIZE_MEDIUM;
    pool->block_sizes[MEMPOOL_TYPE_LARGE] = MEMPOOL_BLOCK_SIZE_LARGE;
    pool->block_sizes[MEMPOOL_TYPE_HUGE] = MEMPOOL_BLOCK_SIZE_HUGE;
    
    // 设置池大小
    pool->pool_sizes[MEMPOOL_TYPE_SMALL] = config->small_pool_size;
    pool->pool_sizes[MEMPOOL_TYPE_MEDIUM] = config->medium_pool_size;
    pool->pool_sizes[MEMPOOL_TYPE_LARGE] = config->large_pool_size;
    pool->pool_sizes[MEMPOOL_TYPE_HUGE] = config->huge_pool_size;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize mutex");
        free(pool);
        return NULL;
    }
    
    // 预分配内存池
    for (int i = 0; i < MEMPOOL_TYPE_MAX; i++) {
        if (mempool_prealloc(pool, i, pool->pool_sizes[i] / pool->block_sizes[i]) < 0) {
            log_message(LOG_LEVEL_WARNING, "Failed to preallocate memory pool");
        }
    }
    
    pool->last_gc_time = asm_opt_get_timestamp_us();
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Memory pool created (ASM opt: %s)", 
             pool->use_asm_opt ? "enabled" : "disabled");
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return pool;
}

// 销毁内存池
void mempool_destroy(mempool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    // 释放所有预分配的内存
    for (int i = 0; i < MEMPOOL_TYPE_MAX; i++) {
        if (pool->preallocated_pools[i]) {
            munmap(pool->preallocated_pools[i], pool->preallocated_sizes[i]);
        }
    }
    
    // 释放剩余的块
    for (int i = 0; i < MEMPOOL_TYPE_MAX; i++) {
        mempool_block_t* block = pool->free_blocks[i];
        while (block) {
            mempool_block_t* next = block->next;
            free(block);
            block = next;
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Memory pool destroyed (peak usage: %lu bytes)", 
             pool->stats.peak_usage);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    free(pool);
}

// 预分配内存池
int mempool_prealloc(mempool_t* pool, mempool_type_t type, size_t count) {
    if (!pool || type >= MEMPOOL_TYPE_MAX) return -1;
    
    size_t block_size = pool->block_sizes[type];
    size_t total_size = block_size * count;
    
    // 使用mmap分配大块内存
    void* memory = mmap(NULL, total_size, PROT_READ | PROT_WRITE, 
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        log_message(LOG_LEVEL_ERROR, "Failed to mmap memory pool");
        return -1;
    }
    
    pool->preallocated_pools[type] = memory;
    pool->preallocated_sizes[type] = total_size;
    
    // 将内存分割为块并加入空闲链表
    char* ptr = (char*)memory;
    for (size_t i = 0; i < count; i++) {
        mempool_block_t* block = (mempool_block_t*)ptr;
        block->next = pool->free_blocks[type];
        block->size = block_size;
        block->magic = MEMPOOL_FREE_MAGIC;
        pool->free_blocks[type] = block;
        ptr += block_size;
    }
    
    return 0;
}

// 汇编优化的内存分配
void* mempool_alloc_asm(mempool_t* pool, size_t size) {
    if (!pool || size == 0) return NULL;
    
    mempool_type_t type = get_block_type(size);
    
    pthread_mutex_lock(&pool->mutex);
    
    mempool_block_t* block = pool->free_blocks[type];
    if (block) {
        // 从空闲链表中取出
        pool->free_blocks[type] = block->next;
        block->magic = MEMPOOL_BLOCK_MAGIC;
        
        // 更新统计信息
        pool->stats.cache_hits++;
        pool->stats.allocation_count++;
        pool->stats.current_usage += block->size;
        if (pool->stats.current_usage > pool->stats.peak_usage) {
            pool->stats.peak_usage = pool->stats.current_usage;
        }
        
        pthread_mutex_unlock(&pool->mutex);
        
        #ifdef __aarch64__
        if (pool->use_asm_opt) {
            // 使用汇编优化的内存清零
            __asm__ volatile (
                "mov x0, %0\n\t"
                "mov x1, #0\n\t"
                "mov x2, %1\n\t"
                "1:\n\t"
                "str x1, [x0], #8\n\t"
                "subs x2, x2, #8\n\t"
                "bgt 1b\n\t"
                : : "r"(block->data), "r"(block->size) : "x0", "x1", "x2", "memory"
            );
        }
        #endif
        
        return block->data;
    } else {
        // 缓存未命中，分配新块
        pool->stats.cache_misses++;
        pthread_mutex_unlock(&pool->mutex);
        
        size_t alloc_size = pool->block_sizes[type] + sizeof(mempool_block_t);
        
        if (pool->use_asm_opt) {
            block = (mempool_block_t*)mempool_alloc_aligned_asm_impl(alloc_size, pool->alignment);
        } else {
            block = (mempool_block_t*)aligned_alloc(pool->alignment, alloc_size);
        }
        
        if (!block) {
            log_message(LOG_LEVEL_ERROR, "Failed to allocate memory block");
            return NULL;
        }
        
        block->next = NULL;
        block->size = pool->block_sizes[type];
        block->magic = MEMPOOL_BLOCK_MAGIC;
        
        // 更新统计信息
        pthread_mutex_lock(&pool->mutex);
        pool->stats.allocation_count++;
        pool->stats.total_allocated += block->size;
        pool->stats.current_usage += block->size;
        if (pool->stats.current_usage > pool->stats.peak_usage) {
            pool->stats.peak_usage = pool->stats.current_usage;
        }
        pthread_mutex_unlock(&pool->mutex);
        
        return block->data;
    }
}

// 标准内存分配
void* mempool_alloc(mempool_t* pool, size_t size) {
    if (pool && pool->use_asm_opt) {
        return mempool_alloc_asm(pool, size);
    }
    
    return malloc(size);
}

// 汇编优化的内存释放
void mempool_free_asm(mempool_t* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    // 从数据指针回推到块头
    mempool_block_t* block = (mempool_block_t*)((char*)ptr - sizeof(mempool_block_t));
    
    // 验证魔数
    if (block->magic != MEMPOOL_BLOCK_MAGIC) {
        log_message(LOG_LEVEL_ERROR, "Invalid memory block magic number");
        return;
    }
    
    mempool_type_t type = get_block_type(block->size);
    
    pthread_mutex_lock(&pool->mutex);
    
    // 将块加入空闲链表
    block->next = pool->free_blocks[type];
    block->magic = MEMPOOL_FREE_MAGIC;
    pool->free_blocks[type] = block;
    
    // 更新统计信息
    pool->stats.free_count++;
    pool->stats.current_usage -= block->size;
    pool->stats.total_freed += block->size;
    
    pthread_mutex_unlock(&pool->mutex);
    
    // 检查是否需要垃圾回收
    if (pool->stats.total_freed > pool->gc_threshold) {
        mempool_gc(pool);
    }
}

// 标准内存释放
void mempool_free(mempool_t* pool, void* ptr) {
    if (pool && pool->use_asm_opt) {
        mempool_free_asm(pool, ptr);
    } else {
        free(ptr);
    }
}

// 垃圾回收
void mempool_gc(mempool_t* pool) {
    if (!pool) return;
    
    uint64_t current_time = asm_opt_get_timestamp_us();
    if (current_time - pool->last_gc_time < 1000000) { // 1秒间隔
        return;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    size_t freed_memory = 0;
    
    // 释放过多的空闲块
    for (int type = 0; type < MEMPOOL_TYPE_MAX; type++) {
        mempool_block_t* block = pool->free_blocks[type];
        mempool_block_t* prev = NULL;
        int free_count = 0;
        
        // 计算空闲块数量
        mempool_block_t* temp = block;
        while (temp) {
            free_count++;
            temp = temp->next;
        }
        
        // 保留一半的空闲块
        int keep_count = free_count / 2;
        int release_count = free_count - keep_count;
        
        // 释放多余的块
        while (release_count > 0 && block) {
            mempool_block_t* next = block->next;
            if (block->magic == MEMPOOL_FREE_MAGIC) {
                freed_memory += block->size;
                free(block);
                release_count--;
            }
            block = next;
        }
        
        // 重新链接剩余的块
        pool->free_blocks[type] = block;
    }
    
    pool->last_gc_time = current_time;
    pthread_mutex_unlock(&pool->mutex);
    
    if (freed_memory > 0) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Memory pool GC freed %zu bytes", freed_memory);
        log_message(LOG_LEVEL_DEBUG, log_msg);
    }
}

// 获取统计信息
mempool_stats_t* mempool_get_stats(mempool_t* pool) {
    if (!pool) return NULL;
    
    pthread_mutex_lock(&pool->mutex);
    mempool_stats_t* stats = malloc(sizeof(mempool_stats_t));
    if (stats) {
        memcpy(stats, &pool->stats, sizeof(mempool_stats_t));
    }
    pthread_mutex_unlock(&pool->mutex);
    
    return stats;
}

// 打印统计信息
void mempool_print_stats(mempool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    printf("\n=== Memory Pool Statistics ===\n");
    printf("Total allocated: %lu bytes\n", pool->stats.total_allocated);
    printf("Total freed: %lu bytes\n", pool->stats.total_freed);
    printf("Current usage: %lu bytes\n", pool->stats.current_usage);
    printf("Peak usage: %lu bytes\n", pool->stats.peak_usage);
    printf("Allocation count: %lu\n", pool->stats.allocation_count);
    printf("Free count: %lu\n", pool->stats.free_count);
    printf("Cache hits: %lu\n", pool->stats.cache_hits);
    printf("Cache misses: %lu\n", pool->stats.cache_misses);
    
    if (pool->stats.cache_hits + pool->stats.cache_misses > 0) {
        double hit_rate = (double)pool->stats.cache_hits / 
                         (pool->stats.cache_hits + pool->stats.cache_misses) * 100.0;
        printf("Cache hit rate: %.2f%%\n", hit_rate);
    }
    
    printf("ASM optimization: %s\n", pool->use_asm_opt ? "enabled" : "disabled");
    printf("==============================\n\n");
    
    pthread_mutex_unlock(&pool->mutex);
}

// 内存池管理器
mempool_manager_t* mempool_manager_create(void) {
    mempool_manager_t* manager = calloc(1, sizeof(mempool_manager_t));
    if (!manager) {
        log_message(LOG_LEVEL_ERROR, "Failed to create memory pool manager");
        return NULL;
    }
    
    // 初始化全局互斥锁
    if (pthread_mutex_init(&manager->global_mutex, NULL) != 0) {
        free(manager);
        return NULL;
    }
    
    // 创建线程本地存储
    if (pthread_key_create(&manager->thread_key, NULL) != 0) {
        pthread_mutex_destroy(&manager->global_mutex);
        free(manager);
        return NULL;
    }
    
    // 创建全局内存池
    for (int i = 0; i < MEMPOOL_TYPE_MAX; i++) {
        manager->pools[i] = mempool_create(1024 * 1024); // 1MB初始大小
        if (!manager->pools[i]) {
            log_message(LOG_LEVEL_ERROR, "Failed to create memory pool");
            mempool_manager_destroy(manager);
            return NULL;
        }
    }
    
    log_message(LOG_LEVEL_INFO, "Memory pool manager created");
    return manager;
}

// 销毁内存池管理器
void mempool_manager_destroy(mempool_manager_t* manager) {
    if (!manager) return;
    
    // 销毁所有内存池
    for (int i = 0; i < MEMPOOL_TYPE_MAX; i++) {
        if (manager->pools[i]) {
            mempool_destroy(manager->pools[i]);
        }
    }
    
    pthread_key_delete(manager->thread_key);
    pthread_mutex_destroy(&manager->global_mutex);
    free(manager);
    
    log_message(LOG_LEVEL_INFO, "Memory pool manager destroyed");
}

// 管理器分配内存
void* mempool_manager_alloc(mempool_manager_t* manager, size_t size) {
    if (!manager) return malloc(size);
    
    mempool_type_t type = get_block_type(size);
    return mempool_alloc(manager->pools[type], size);
}

// 管理器释放内存
void mempool_manager_free(mempool_manager_t* manager, void* ptr) {
    if (!manager) {
        free(ptr);
        return;
    }
    
    // 简单实现：尝试在所有池中释放
    for (int i = 0; i < MEMPOOL_TYPE_MAX; i++) {
        if (mempool_check_block(manager->pools[i], ptr)) {
            mempool_free(manager->pools[i], ptr);
            return;
        }
    }
    
    // 如果不在任何池中，使用标准free
    free(ptr);
}

// 检查内存块是否属于池
int mempool_check_block(mempool_t* pool, void* ptr) {
    if (!pool || !ptr) return 0;
    
    // 简单检查：验证魔数
    mempool_block_t* block = (mempool_block_t*)((char*)ptr - sizeof(mempool_block_t));
    return (block->magic == MEMPOOL_BLOCK_MAGIC);
}

// 其他函数的简单实现
void* mempool_calloc(mempool_t* pool, size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void* ptr = mempool_alloc(pool, total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void* mempool_realloc(mempool_t* pool, void* ptr, size_t new_size) {
    if (!ptr) return mempool_alloc(pool, new_size);
    if (new_size == 0) {
        mempool_free(pool, ptr);
        return NULL;
    }
    
    void* new_ptr = mempool_alloc(pool, new_size);
    if (new_ptr) {
        // 简单实现：假设原大小
        memcpy(new_ptr, ptr, new_size);
        mempool_free(pool, ptr);
    }
    return new_ptr;
}

void* mempool_alloc_aligned(mempool_t* pool, size_t size, size_t alignment) {
    if (pool && pool->use_asm_opt) {
        return mempool_alloc_aligned_asm(pool, size, alignment);
    }
    return aligned_alloc(alignment, size);
}

void* mempool_alloc_aligned_asm(mempool_t* pool, size_t size, size_t alignment) {
    return mempool_alloc_aligned_asm_impl(size, alignment);
}

void mempool_reset(mempool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    // 清空所有空闲链表
    for (int i = 0; i < MEMPOOL_TYPE_MAX; i++) {
        pool->free_blocks[i] = NULL;
    }
    
    // 重置统计信息
    memset(&pool->stats, 0, sizeof(mempool_stats_t));
    
    pthread_mutex_unlock(&pool->mutex);
}

void mempool_reset_stats(mempool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    memset(&pool->stats, 0, sizeof(mempool_stats_t));
    pthread_mutex_unlock(&pool->mutex);
}

void mempool_set_gc_threshold(mempool_t* pool, uint64_t threshold) {
    if (pool) {
        pool->gc_threshold = threshold;
    }
}

void mempool_enable_asm_opt(mempool_t* pool, int enable) {
    if (pool) {
        pool->use_asm_opt = enable;
    }
}

int mempool_validate(mempool_t* pool) {
    if (!pool) return 0;
    
    pthread_mutex_lock(&pool->mutex);
    
    // 验证空闲链表
    for (int i = 0; i < MEMPOOL_TYPE_MAX; i++) {
        mempool_block_t* block = pool->free_blocks[i];
        while (block) {
            if (block->magic != MEMPOOL_FREE_MAGIC) {
                pthread_mutex_unlock(&pool->mutex);
                return 0;
            }
            block = block->next;
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
    return 1;
}

void mempool_dump_blocks(mempool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    printf("=== Memory Pool Block Dump ===\n");
    for (int i = 0; i < MEMPOOL_TYPE_MAX; i++) {
        printf("Type %d (size %zu):\n", i, pool->block_sizes[i]);
        
        mempool_block_t* block = pool->free_blocks[i];
        int count = 0;
        while (block) {
            printf("  Block %d: %p (magic: 0x%x)\n", count++, block, block->magic);
            block = block->next;
        }
        printf("  Total free blocks: %d\n", count);
    }
    printf("===============================\n");
    
    pthread_mutex_unlock(&pool->mutex);
}

void mempool_check_leaks(mempool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    if (pool->stats.allocation_count > pool->stats.free_count) {
        uint64_t leaked = pool->stats.allocation_count - pool->stats.free_count;
        printf("Memory leak detected: %lu blocks not freed\n", leaked);
        printf("Current usage: %lu bytes\n", pool->stats.current_usage);
    } else {
        printf("No memory leaks detected\n");
    }
    
    pthread_mutex_unlock(&pool->mutex);
} 