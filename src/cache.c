#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cache.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <openssl/md5.h>

#define DEFAULT_HASH_SIZE 1024
#define DEFAULT_MAX_SIZE (64 * 1024 * 1024)  // 64MB
#define DEFAULT_MAX_ENTRIES 10000
#define DEFAULT_TTL 3600  // 1小时
#define MAX_CACHEABLE_TYPES 50

// 哈希函数
static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// 创建缓存配置
cache_config_t *cache_config_create(void) {
    cache_config_t *config = calloc(1, sizeof(cache_config_t));
    if (!config) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate cache config");
        return NULL;
    }
    
    // 设置默认值
    config->enable_cache = true;
    config->max_size = DEFAULT_MAX_SIZE;
    config->max_entries = DEFAULT_MAX_ENTRIES;
    config->default_ttl = DEFAULT_TTL;
    config->strategy = CACHE_STRATEGY_LRU;
    config->min_file_size = 1024;      // 1KB
    config->max_file_size = 10 * 1024 * 1024;  // 10MB
    config->enable_etag = true;
    config->enable_last_modified = true;
    
    // 分配MIME类型数组
    config->cacheable_types = calloc(MAX_CACHEABLE_TYPES, sizeof(char *));
    if (!config->cacheable_types) {
        free(config);
        log_message(LOG_LEVEL_ERROR, "Failed to allocate cacheable types array");
        return NULL;
    }
    
    // 添加默认可缓存类型
    const char *default_types[] = {
        "text/html", "text/css", "text/javascript", "text/plain",
        "application/javascript", "application/json", "application/xml",
        "text/xml", "image/jpeg", "image/png", "image/gif", "image/webp",
        "image/svg+xml", "application/pdf", "font/woff", "font/woff2"
    };
    
    for (size_t i = 0; i < sizeof(default_types) / sizeof(default_types[0]); i++) {
        cache_config_add_type(config, default_types[i]);
    }
    
    return config;
}

// 释放缓存配置
void cache_config_free(cache_config_t *config) {
    if (!config) return;
    
    if (config->cacheable_types) {
        for (int i = 0; i < config->cacheable_types_count; i++) {
            free(config->cacheable_types[i]);
        }
        free(config->cacheable_types);
    }
    
    free(config);
}

// 添加可缓存类型
int cache_config_add_type(cache_config_t *config, const char *mime_type) {
    if (!config || !mime_type || config->cacheable_types_count >= MAX_CACHEABLE_TYPES) {
        return -1;
    }
    
    // 检查是否已存在
    for (int i = 0; i < config->cacheable_types_count; i++) {
        if (strcmp(config->cacheable_types[i], mime_type) == 0) {
            return 0; // 已存在
        }
    }
    
    config->cacheable_types[config->cacheable_types_count] = strdup(mime_type);
    if (!config->cacheable_types[config->cacheable_types_count]) {
        return -1;
    }
    
    config->cacheable_types_count++;
    return 0;
}

// 检查是否可缓存
bool cache_config_is_cacheable(cache_config_t *config, const char *mime_type, size_t size) {
    if (!config || !config->enable_cache || !mime_type) {
        return false;
    }
    
    // 检查文件大小
    if (size < config->min_file_size || size > config->max_file_size) {
        return false;
    }
    
    // 检查MIME类型
    for (int i = 0; i < config->cacheable_types_count; i++) {
        if (strcmp(config->cacheable_types[i], mime_type) == 0) {
            return true;
        }
    }
    
    return false;
}

// 创建缓存管理器
cache_manager_t *cache_manager_create(cache_config_t *config) {
    if (!config) return NULL;
    
    cache_manager_t *manager = calloc(1, sizeof(cache_manager_t));
    if (!manager) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate cache manager");
        return NULL;
    }
    
    manager->config = config;
    manager->hash_size = DEFAULT_HASH_SIZE;
    
    // 分配哈希表
    manager->hash_table = calloc(manager->hash_size, sizeof(cache_entry_t *));
    if (!manager->hash_table) {
        free(manager);
        log_message(LOG_LEVEL_ERROR, "Failed to allocate cache hash table");
        return NULL;
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        free(manager->hash_table);
        free(manager);
        log_message(LOG_LEVEL_ERROR, "Failed to initialize cache mutex");
        return NULL;
    }
    
    // 初始化统计
    memset(&manager->stats, 0, sizeof(cache_stats_t));
    
    log_message(LOG_LEVEL_INFO, "Cache manager created successfully");
    return manager;
}

// 释放缓存条目
static void cache_entry_free(cache_entry_t *entry) {
    if (!entry) return;
    
    free(entry->key);
    free(entry->etag);
    free(entry->content_type);
    free(entry->content);
    free(entry);
}

// 释放缓存管理器
void cache_manager_free(cache_manager_t *manager) {
    if (!manager) return;
    
    pthread_mutex_lock(&manager->mutex);
    
    // 清理所有缓存条目
    cache_entry_t *current = manager->head;
    while (current) {
        cache_entry_t *next = current->lru_next;
        cache_entry_free(current);
        current = next;
    }
    
    free(manager->hash_table);
    pthread_mutex_unlock(&manager->mutex);
    pthread_mutex_destroy(&manager->mutex);
    free(manager);
}

// 清空缓存
void cache_manager_clear(cache_manager_t *manager) {
    if (!manager) return;
    
    pthread_mutex_lock(&manager->mutex);
    
    // 清理所有条目
    cache_entry_t *current = manager->head;
    while (current) {
        cache_entry_t *next = current->lru_next;
        cache_entry_free(current);
        current = next;
    }
    
    // 重置链表和哈希表
    manager->head = NULL;
    manager->tail = NULL;
    memset(manager->hash_table, 0, manager->hash_size * sizeof(cache_entry_t *));
    
    // 重置统计
    manager->stats.current_size = 0;
    manager->stats.current_entries = 0;
    
    pthread_mutex_unlock(&manager->mutex);
    log_message(LOG_LEVEL_INFO, "Cache cleared");
}

// 生成ETag
char *cache_generate_etag(const char *path, time_t mtime, size_t size) {
    if (!path) return NULL;
    
    char input[1024];
    snprintf(input, sizeof(input), "%s-%ld-%zu", path, mtime, size);
    
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5((unsigned char *)input, strlen(input), hash);
    
    char *etag = malloc(64);
    if (!etag) return NULL;
    
    snprintf(etag, 64, "\"%02x%02x%02x%02x%02x%02x%02x%02x\"",
             hash[0], hash[1], hash[2], hash[3],
             hash[4], hash[5], hash[6], hash[7]);
    
    return etag;
}

// 验证ETag
bool cache_validate_etag(const char *etag1, const char *etag2) {
    if (!etag1 || !etag2) return false;
    return strcmp(etag1, etag2) == 0;
}

// 验证修改时间
bool cache_validate_modified_since(time_t last_modified, time_t if_modified_since) {
    return last_modified <= if_modified_since;
}

// 检查缓存是否新鲜
bool cache_is_fresh(cache_entry_t *entry) {
    if (!entry) return false;
    return time(NULL) < entry->expires;
}

// 从LRU链表中移除条目
static void cache_remove_from_list(cache_manager_t *manager, cache_entry_t *entry) {
    if (entry->lru_prev) {
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        manager->head = entry->lru_next;
    }
    
    if (entry->lru_next) {
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        manager->tail = entry->lru_prev;
    }
}

// 将条目移到LRU链表头部
static void cache_move_to_head(cache_manager_t *manager, cache_entry_t *entry) {
    if (manager->head == entry) return;
    
    // 从当前位置移除
    cache_remove_from_list(manager, entry);
    
    // 添加到头部
    entry->lru_prev = NULL;
    entry->lru_next = manager->head;
    
    if (manager->head) {
        manager->head->lru_prev = entry;
    }
    manager->head = entry;
    
    if (!manager->tail) {
        manager->tail = entry;
    }
}

// 查找缓存条目
static cache_entry_t *cache_find_entry(cache_manager_t *manager, const char *key) {
    unsigned int hash = hash_string(key) % manager->hash_size;
    cache_entry_t *entry = manager->hash_table[hash];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->hash_next;
    }
    
    return NULL;
}

// 获取缓存
cache_response_t *cache_get(cache_manager_t *manager, const char *key, 
                           const char *if_none_match, time_t if_modified_since) {
    if (!manager || !key) return NULL;
    
    pthread_mutex_lock(&manager->mutex);
    
    cache_entry_t *entry = cache_find_entry(manager, key);
    if (!entry) {
        manager->stats.misses++;
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    // 检查是否过期
    if (!cache_is_fresh(entry)) {
        // 过期，从缓存中移除
        cache_remove(manager, key);
        manager->stats.misses++;
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    // 更新访问信息
    entry->last_access = time(NULL);
    entry->access_count++;
    cache_move_to_head(manager, entry);
    
    // 创建响应
    cache_response_t *response = cache_response_create();
    if (!response) {
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    response->is_cached = true;
    response->is_fresh = true;
    
    // 检查条件请求
    if (if_none_match && entry->etag) {
        if (cache_validate_etag(entry->etag, if_none_match)) {
            response->needs_validation = true;
            manager->stats.hits++;
            pthread_mutex_unlock(&manager->mutex);
            return response;
        }
    }
    
    if (if_modified_since > 0) {
        if (cache_validate_modified_since(entry->last_modified, if_modified_since)) {
            response->needs_validation = true;
            manager->stats.hits++;
            pthread_mutex_unlock(&manager->mutex);
            return response;
        }
    }
    
    // 复制缓存数据
    response->content = malloc(entry->content_length);
    if (response->content) {
        memcpy(response->content, entry->content, entry->content_length);
        response->content_length = entry->content_length;
    }
    
    response->content_type = entry->content_type ? strdup(entry->content_type) : NULL;
    response->etag = entry->etag ? strdup(entry->etag) : NULL;
    response->last_modified = entry->last_modified;
    response->is_compressed = entry->is_compressed;
    
    manager->stats.hits++;
    manager->stats.hit_ratio = (double)manager->stats.hits / 
                              (manager->stats.hits + manager->stats.misses);
    
    pthread_mutex_unlock(&manager->mutex);
    return response;
}

// LRU驱逐
void cache_evict_lru(cache_manager_t *manager) {
    if (!manager || !manager->tail) return;
    
    cache_entry_t *victim = manager->tail;
    cache_remove(manager, victim->key);
    manager->stats.evictions++;
}

// 存储到缓存
int cache_put(cache_manager_t *manager, const char *key, const char *content,
              size_t content_length, const char *content_type, 
              time_t last_modified, int ttl, bool is_compressed) {
    if (!manager || !key || !content) return -1;
    
    pthread_mutex_lock(&manager->mutex);
    
    // 检查是否需要驱逐
    while (manager->stats.current_entries >= manager->config->max_entries ||
           manager->stats.current_size + content_length > manager->config->max_size) {
        cache_evict_lru(manager);
    }
    
    // 创建新条目
    cache_entry_t *entry = calloc(1, sizeof(cache_entry_t));
    if (!entry) {
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    entry->key = strdup(key);
    entry->content = malloc(content_length);
    if (!entry->content) {
        cache_entry_free(entry);
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    memcpy(entry->content, content, content_length);
    entry->content_length = content_length;
    entry->content_type = content_type ? strdup(content_type) : NULL;
    entry->last_modified = last_modified;
    entry->last_access = time(NULL);
    entry->expires = time(NULL) + (ttl > 0 ? ttl : manager->config->default_ttl);
    entry->access_count = 1;
    entry->is_compressed = is_compressed;
    
    // 生成ETag
    if (manager->config->enable_etag) {
        entry->etag = cache_generate_etag(key, last_modified, content_length);
    }
    
    // 添加到哈希表
    unsigned int hash = hash_string(key) % manager->hash_size;
    entry->hash_next = manager->hash_table[hash];
    manager->hash_table[hash] = entry;
    
    // 添加到LRU链表头部
    cache_move_to_head(manager, entry);
    
    // 更新统计
    manager->stats.current_entries++;
    manager->stats.current_size += content_length;
    
    pthread_mutex_unlock(&manager->mutex);
    return 0;
}

// 移除缓存条目
int cache_remove(cache_manager_t *manager, const char *key) {
    if (!manager || !key) return -1;
    
    pthread_mutex_lock(&manager->mutex);
    
    unsigned int hash = hash_string(key) % manager->hash_size;
    cache_entry_t *entry = manager->hash_table[hash];
    cache_entry_t *prev = NULL;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            // 从哈希表中移除
            if (prev) {
                prev->hash_next = entry->hash_next;
            } else {
                manager->hash_table[hash] = entry->hash_next;
            }
            
            // 从LRU链表中移除
            cache_remove_from_list(manager, entry);
            
            // 更新统计
            manager->stats.current_entries--;
            manager->stats.current_size -= entry->content_length;
            
            cache_entry_free(entry);
            pthread_mutex_unlock(&manager->mutex);
            return 0;
        }
        prev = entry;
        entry = entry->hash_next;
    }
    
    pthread_mutex_unlock(&manager->mutex);
    return -1;
}

// 创建缓存响应
cache_response_t *cache_response_create(void) {
    cache_response_t *response = calloc(1, sizeof(cache_response_t));
    return response;
}

// 释放缓存响应
void cache_response_free(cache_response_t *response) {
    if (!response) return;
    
    free(response->etag);
    free(response->content);
    free(response->content_type);
    free(response);
}

// 获取缓存统计
cache_stats_t *cache_get_stats(cache_manager_t *manager) {
    if (!manager) return NULL;
    
    pthread_mutex_lock(&manager->mutex);
    cache_stats_t *stats = malloc(sizeof(cache_stats_t));
    if (stats) {
        memcpy(stats, &manager->stats, sizeof(cache_stats_t));
    }
    pthread_mutex_unlock(&manager->mutex);
    
    return stats;
}

// 打印缓存统计
void cache_print_stats(cache_manager_t *manager) {
    if (!manager) return;
    
    pthread_mutex_lock(&manager->mutex);
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "Cache Stats: Hits=%zu, Misses=%zu, Hit Ratio=%.2f%%, "
             "Entries=%zu/%zu, Size=%zu/%zu MB, Evictions=%zu",
             manager->stats.hits, manager->stats.misses,
             manager->stats.hit_ratio * 100,
             manager->stats.current_entries, manager->config->max_entries,
             manager->stats.current_size / (1024 * 1024),
             manager->config->max_size / (1024 * 1024),
             manager->stats.evictions);
    
    log_message(LOG_LEVEL_INFO, log_msg);
    pthread_mutex_unlock(&manager->mutex);
}

// 清理过期缓存
void cache_cleanup_expired(cache_manager_t *manager) {
    if (!manager) return;
    
    pthread_mutex_lock(&manager->mutex);
    
    time_t now = time(NULL);
    cache_entry_t *current = manager->head;
    
    while (current) {
        cache_entry_t *next = current->lru_next;
        if (now >= current->expires) {
            cache_remove(manager, current->key);
        }
        current = next;
    }
    
    pthread_mutex_unlock(&manager->mutex);
} 