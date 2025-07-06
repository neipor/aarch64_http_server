#ifndef CACHE_H
#define CACHE_H

#include <time.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

// 缓存策略枚举
typedef enum {
    CACHE_STRATEGY_LRU,    // 最近最少使用
    CACHE_STRATEGY_LFU,    // 最少使用频率
    CACHE_STRATEGY_FIFO    // 先进先出
} cache_strategy_t;

// 缓存条目结构
typedef struct cache_entry {
    char *key;                    // 缓存键（通常是文件路径）
    char *etag;                   // ETag值
    time_t last_modified;         // 最后修改时间
    time_t expires;               // 过期时间
    time_t last_access;           // 最后访问时间
    size_t access_count;          // 访问次数
    size_t content_length;        // 内容长度
    char *content_type;           // 内容类型
    char *content;                // 缓存内容
    bool is_compressed;           // 是否已压缩
    struct cache_entry *lru_next; // LRU链表指针
    struct cache_entry *lru_prev; // LRU双向链表指针
    struct cache_entry *hash_next; // 哈希表链表指针
} cache_entry_t;

// 缓存配置结构
typedef struct {
    bool enable_cache;            // 是否启用缓存
    size_t max_size;              // 最大缓存大小（字节）
    size_t max_entries;           // 最大缓存条目数
    int default_ttl;              // 默认TTL（秒）
    cache_strategy_t strategy;    // 缓存策略
    char **cacheable_types;       // 可缓存的MIME类型
    int cacheable_types_count;    // 可缓存类型数量
    size_t min_file_size;         // 最小缓存文件大小
    size_t max_file_size;         // 最大缓存文件大小
    bool enable_etag;             // 是否启用ETag
    bool enable_last_modified;    // 是否启用Last-Modified
} cache_config_t;

// 缓存统计结构
typedef struct {
    size_t hits;                  // 缓存命中次数
    size_t misses;                // 缓存未命中次数
    size_t evictions;             // 缓存驱逐次数
    size_t current_size;          // 当前缓存大小
    size_t current_entries;       // 当前缓存条目数
    double hit_ratio;             // 命中率
} cache_stats_t;

// 缓存管理器结构
typedef struct {
    cache_config_t *config;       // 缓存配置
    cache_entry_t *head;          // LRU链表头
    cache_entry_t *tail;          // LRU链表尾
    cache_entry_t **hash_table;   // 哈希表
    size_t hash_size;             // 哈希表大小
    cache_stats_t stats;          // 缓存统计
    pthread_mutex_t mutex;        // 线程安全锁
} cache_manager_t;

// 缓存响应结构
typedef struct {
    bool is_cached;               // 是否来自缓存
    bool is_fresh;                // 是否新鲜
    bool needs_validation;        // 是否需要验证
    char *etag;                   // ETag值
    time_t last_modified;         // 最后修改时间
    char *content;                // 内容
    size_t content_length;        // 内容长度
    char *content_type;           // 内容类型
    bool is_compressed;           // 是否已压缩
} cache_response_t;

// 缓存配置函数
cache_config_t *cache_config_create(void);
void cache_config_free(cache_config_t *config);
int cache_config_add_type(cache_config_t *config, const char *mime_type);
bool cache_config_is_cacheable(cache_config_t *config, const char *mime_type, size_t size);

// 缓存管理器函数
cache_manager_t *cache_manager_create(cache_config_t *config);
void cache_manager_free(cache_manager_t *manager);
void cache_manager_clear(cache_manager_t *manager);

// 缓存操作函数
cache_response_t *cache_get(cache_manager_t *manager, const char *key, 
                           const char *if_none_match, time_t if_modified_since);
int cache_put(cache_manager_t *manager, const char *key, const char *content,
              size_t content_length, const char *content_type, 
              time_t last_modified, int ttl, bool is_compressed);
int cache_remove(cache_manager_t *manager, const char *key);
bool cache_is_fresh(cache_entry_t *entry);

// 缓存验证函数
char *cache_generate_etag(const char *path, time_t mtime, size_t size);
bool cache_validate_etag(const char *etag1, const char *etag2);
bool cache_validate_modified_since(time_t last_modified, time_t if_modified_since);

// 缓存统计函数
cache_stats_t *cache_get_stats(cache_manager_t *manager);
void cache_print_stats(cache_manager_t *manager);
void cache_reset_stats(cache_manager_t *manager);

// 缓存响应函数
cache_response_t *cache_response_create(void);
void cache_response_free(cache_response_t *response);

// 缓存清理函数
void cache_cleanup_expired(cache_manager_t *manager);
void cache_evict_lru(cache_manager_t *manager);
void cache_evict_lfu(cache_manager_t *manager);

#endif // CACHE_H 