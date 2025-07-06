#include "asm_integration.h"
#include "asm_opt.h"
#include "asm_mempool.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// 全局汇编优化状态
static int asm_integration_initialized = 0;
static mempool_t* global_asm_pool = NULL;

// 初始化汇编优化集成
int asm_integration_init(void) {
    if (asm_integration_initialized) {
        return 1; // 已经初始化
    }
    
    log_message(LOG_LEVEL_INFO, "Initializing assembly optimization integration...");
    
    // 检查汇编优化支持
    if (!asm_opt_is_supported()) {
        log_message(LOG_LEVEL_WARNING, "Assembly optimizations not supported on this platform");
        return 0;
    }
    
    // 获取CPU特性
    uint32_t cpu_features = asm_opt_get_cpu_features();
    char features_msg[256];
    snprintf(features_msg, sizeof(features_msg), 
             "CPU features detected: NEON=%s, CRC32=%s, AES=%s",
             (cpu_features & CPU_FEATURE_NEON) ? "YES" : "NO",
             (cpu_features & CPU_FEATURE_CRC32) ? "YES" : "NO",
             (cpu_features & CPU_FEATURE_AES) ? "YES" : "NO");
    log_message(LOG_LEVEL_INFO, features_msg);
    
    // 初始化全局内存池
    global_asm_pool = mempool_create(4 * 1024 * 1024); // 4MB
    if (!global_asm_pool) {
        log_message(LOG_LEVEL_ERROR, "Failed to create global assembly optimized memory pool");
        return 0;
    }
    
    asm_integration_initialized = 1;
    log_message(LOG_LEVEL_INFO, "Assembly optimization integration initialized successfully");
    
    return 1;
}

// 清理汇编优化集成
void asm_integration_cleanup(void) {
    if (!asm_integration_initialized) {
        return;
    }
    
    if (global_asm_pool) {
        mempool_print_stats(global_asm_pool);
        mempool_destroy(global_asm_pool);
        global_asm_pool = NULL;
    }
    
    asm_integration_initialized = 0;
    log_message(LOG_LEVEL_INFO, "Assembly optimization integration cleaned up");
}

// 优化的HTTP头部解析
int asm_parse_http_request_line(const char* buffer, size_t len, 
                               http_request_info_t* request_info) {
    if (!buffer || !request_info || len == 0) {
        return -1;
    }
    
    asm_opt_perf_counter_t perf_counter = {0};
    asm_opt_perf_counter_start(&perf_counter);
    
    // 使用汇编优化的字符串搜索查找空格和换行
    const char* method_end = asm_opt_strchr(buffer, ' ');
    if (!method_end || method_end >= buffer + len) {
        return -1;
    }
    
    const char* path_start = method_end + 1;
    const char* path_end = asm_opt_strchr(path_start, ' ');
    if (!path_end || path_end >= buffer + len) {
        return -1;
    }
    
    const char* version_start = path_end + 1;
    const char* version_end = asm_opt_strstr(version_start, "\r\n");
    if (!version_end || version_end >= buffer + len) {
        return -1;
    }
    
    // 使用汇编优化的内存池分配
    size_t method_len = method_end - buffer;
    size_t path_len = path_end - path_start;
    size_t version_len = version_end - version_start;
    
    if (global_asm_pool) {
        request_info->method = mempool_alloc(global_asm_pool, method_len + 1);
        request_info->path = mempool_alloc(global_asm_pool, path_len + 1);
        request_info->version = mempool_alloc(global_asm_pool, version_len + 1);
    } else {
        request_info->method = malloc(method_len + 1);
        request_info->path = malloc(path_len + 1);
        request_info->version = malloc(version_len + 1);
    }
    
    if (!request_info->method || !request_info->path || !request_info->version) {
        return -1;
    }
    
    // 使用汇编优化的内存拷贝
    asm_opt_memcpy(request_info->method, buffer, method_len);
    request_info->method[method_len] = '\0';
    
    asm_opt_memcpy(request_info->path, path_start, path_len);
    request_info->path[path_len] = '\0';
    
    asm_opt_memcpy(request_info->version, version_start, version_len);
    request_info->version[version_len] = '\0';
    
    // 验证HTTP方法
    if (!asm_opt_validate_http_method(request_info->method, method_len)) {
        asm_free_http_request_info(request_info);
        return -1;
    }
    
    asm_opt_perf_counter_stop(&perf_counter);
    
    // 记录性能信息
    char perf_msg[128];
    snprintf(perf_msg, sizeof(perf_msg), 
             "HTTP request line parsed in %lu cycles", perf_counter.cycles);
    log_message(LOG_LEVEL_DEBUG, perf_msg);
    
    return 0;
}

// 释放HTTP请求信息
void asm_free_http_request_info(http_request_info_t* request_info) {
    if (!request_info) return;
    
    if (global_asm_pool) {
        if (request_info->method) mempool_free(global_asm_pool, request_info->method);
        if (request_info->path) mempool_free(global_asm_pool, request_info->path);
        if (request_info->version) mempool_free(global_asm_pool, request_info->version);
    } else {
        free(request_info->method);
        free(request_info->path);
        free(request_info->version);
    }
    
    request_info->method = NULL;
    request_info->path = NULL;
    request_info->version = NULL;
}

// 优化的缓冲区数据传输
ssize_t asm_optimized_send(int socket_fd, const void* buffer, size_t len, int flags) {
    if (!buffer || len == 0) {
        return 0;
    }
    
    asm_opt_perf_counter_t perf_counter = {0};
    asm_opt_perf_counter_start(&perf_counter);
    
    // 对于大数据包，使用汇编优化的发送
    if (len >= 4096 && asm_opt_is_supported()) {
        // 分块发送，每次发送64KB
        const char* data = (const char*)buffer;
        size_t remaining = len;
        ssize_t total_sent = 0;
        
        while (remaining > 0) {
            size_t chunk_size = (remaining > 65536) ? 65536 : remaining;
            
            ssize_t sent = send(socket_fd, data, chunk_size, flags);
            if (sent <= 0) {
                if (total_sent > 0) {
                    break; // 部分发送成功
                }
                return sent; // 发送失败
            }
            
            total_sent += sent;
            data += sent;
            remaining -= sent;
            
            // 如果发送的数据少于请求的，可能是缓冲区满了
            if (sent < chunk_size) {
                break;
            }
        }
        
        asm_opt_perf_counter_stop(&perf_counter);
        
        char perf_msg[128];
        snprintf(perf_msg, sizeof(perf_msg), 
                 "Optimized send: %zd bytes in %lu cycles", total_sent, perf_counter.cycles);
        log_message(LOG_LEVEL_DEBUG, perf_msg);
        
        return total_sent;
    } else {
        // 小数据包使用标准发送
        ssize_t result = send(socket_fd, buffer, len, flags);
        
        asm_opt_perf_counter_stop(&perf_counter);
        return result;
    }
}

// 优化的字符串哈希（用于缓存键）
uint32_t asm_compute_cache_key_hash(const char* url, const char* headers) {
    if (!url) return 0;
    
    uint32_t url_hash = asm_opt_hash_string(url);
    uint32_t headers_hash = headers ? asm_opt_hash_string(headers) : 0;
    
    // 组合哈希值
    return url_hash ^ (headers_hash << 16) ^ (headers_hash >> 16);
}

// 优化的数据压缩预处理
int asm_prepare_compression_data(const void* data, size_t len, uint32_t* checksum) {
    if (!data || len == 0 || !checksum) {
        return -1;
    }
    
    asm_opt_perf_counter_t perf_counter = {0};
    asm_opt_perf_counter_start(&perf_counter);
    
    // 使用汇编优化的CRC32计算校验和
    *checksum = asm_opt_crc32(data, len);
    
    asm_opt_perf_counter_stop(&perf_counter);
    
    char perf_msg[128];
    snprintf(perf_msg, sizeof(perf_msg), 
             "Compression prep: %zu bytes, CRC32=0x%08x in %lu cycles", 
             len, *checksum, perf_counter.cycles);
    log_message(LOG_LEVEL_DEBUG, perf_msg);
    
    return 0;
}

// 优化的网络字节序转换
void asm_convert_network_headers(http_response_header_t* header) {
    if (!header) return;
    
    // 转换状态码和内容长度为网络字节序
    header->status_code = asm_opt_htons(header->status_code);
    header->content_length = asm_opt_htonl(header->content_length);
    
    // 转换时间戳
    header->timestamp = asm_opt_get_timestamp_us();
}

// 性能基准测试
void asm_benchmark_operations(void) {
    const size_t test_size = 1024 * 1024; // 1MB测试数据
    const int iterations = 1000;
    
    log_message(LOG_LEVEL_INFO, "Starting assembly optimization benchmarks...");
    
    // 分配测试数据
    void* test_data1 = malloc(test_size);
    void* test_data2 = malloc(test_size);
    
    if (!test_data1 || !test_data2) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate benchmark test data");
        free(test_data1);
        free(test_data2);
        return;
    }
    
    // 填充测试数据
    memset(test_data1, 0xAA, test_size);
    memset(test_data2, 0x55, test_size);
    
    // 基准测试：内存拷贝
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < iterations; i++) {
        asm_opt_memcpy(test_data2, test_data1, test_size);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double asm_memcpy_time = (end.tv_sec - start.tv_sec) + 
                            (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // 对比标准memcpy
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < iterations; i++) {
        memcpy(test_data2, test_data1, test_size);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double std_memcpy_time = (end.tv_sec - start.tv_sec) + 
                            (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // 基准测试：哈希计算
    const char* test_string = "This is a test string for hash calculation benchmark";
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    uint32_t asm_hash = 0;
    for (int i = 0; i < iterations * 1000; i++) {
        asm_hash ^= asm_opt_hash_string(test_string);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double asm_hash_time = (end.tv_sec - start.tv_sec) + 
                          (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // 打印基准测试结果
    char benchmark_msg[512];
    snprintf(benchmark_msg, sizeof(benchmark_msg),
             "Benchmark results:\n"
             "  ASM memcpy: %.3f seconds (%.2f MB/s)\n"
             "  STD memcpy: %.3f seconds (%.2f MB/s)\n"
             "  Speedup: %.2fx\n"
             "  ASM hash: %.3f seconds\n"
             "  Hash result: 0x%08x",
             asm_memcpy_time, 
             (test_size * iterations / 1024.0 / 1024.0) / asm_memcpy_time,
             std_memcpy_time,
             (test_size * iterations / 1024.0 / 1024.0) / std_memcpy_time,
             std_memcpy_time / asm_memcpy_time,
             asm_hash_time,
             asm_hash);
    
    log_message(LOG_LEVEL_INFO, benchmark_msg);
    
    free(test_data1);
    free(test_data2);
}

// 获取汇编优化状态
asm_optimization_status_t* asm_get_optimization_status(void) {
    static asm_optimization_status_t status;
    
    status.is_supported = asm_opt_is_supported();
    status.is_initialized = asm_integration_initialized;
    status.cpu_features = asm_opt_get_cpu_features();
    status.mempool_available = (global_asm_pool != NULL);
    
    if (global_asm_pool) {
        mempool_stats_t* stats = mempool_get_stats(global_asm_pool);
        if (stats) {
            status.mempool_stats = *stats;
            free(stats);
        }
    }
    
    return &status;
}

// 打印汇编优化状态报告
void asm_print_status_report(void) {
    asm_optimization_status_t* status = asm_get_optimization_status();
    
    printf("\n=== Assembly Optimization Status ===\n");
    printf("Platform support: %s\n", status->is_supported ? "YES" : "NO");
    printf("Initialization: %s\n", status->is_initialized ? "INITIALIZED" : "NOT INITIALIZED");
    
    if (status->is_supported) {
        printf("CPU Features:\n");
        printf("  NEON: %s\n", (status->cpu_features & CPU_FEATURE_NEON) ? "YES" : "NO");
        printf("  CRC32: %s\n", (status->cpu_features & CPU_FEATURE_CRC32) ? "YES" : "NO");
        printf("  AES: %s\n", (status->cpu_features & CPU_FEATURE_AES) ? "YES" : "NO");
        printf("  SHA1: %s\n", (status->cpu_features & CPU_FEATURE_SHA1) ? "YES" : "NO");
        printf("  SHA2: %s\n", (status->cpu_features & CPU_FEATURE_SHA2) ? "YES" : "NO");
        printf("  SVE: %s\n", (status->cpu_features & CPU_FEATURE_SVE) ? "YES" : "NO");
    }
    
    printf("Memory Pool: %s\n", status->mempool_available ? "AVAILABLE" : "NOT AVAILABLE");
    
    if (status->mempool_available) {
        printf("  Current usage: %lu bytes\n", status->mempool_stats.current_usage);
        printf("  Peak usage: %lu bytes\n", status->mempool_stats.peak_usage);
        printf("  Allocations: %lu\n", status->mempool_stats.allocation_count);
        printf("  Cache hit rate: %.2f%%\n", 
               status->mempool_stats.cache_hits * 100.0 / 
               (status->mempool_stats.cache_hits + status->mempool_stats.cache_misses));
    }
    
    printf("=====================================\n\n");
} 