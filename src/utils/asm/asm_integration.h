#ifndef ASM_INTEGRATION_H
#define ASM_INTEGRATION_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "asm_mempool.h"

// HTTP请求信息结构
typedef struct {
    char* method;
    char* path;
    char* version;
} http_request_info_t;

// HTTP响应头结构
typedef struct {
    uint16_t status_code;
    uint32_t content_length;
    uint64_t timestamp;
    char content_type[64];
    char server_name[32];
} http_response_header_t;

// 汇编优化状态结构
typedef struct {
    int is_supported;
    int is_initialized;
    uint32_t cpu_features;
    int mempool_available;
    mempool_stats_t mempool_stats;
} asm_optimization_status_t;

// 初始化和清理
int asm_integration_init(void);
void asm_integration_cleanup(void);

// HTTP处理优化
int asm_parse_http_request_line(const char* buffer, size_t len, 
                               http_request_info_t* request_info);
void asm_free_http_request_info(http_request_info_t* request_info);

// 网络I/O优化
ssize_t asm_optimized_send(int socket_fd, const void* buffer, size_t len, int flags);

// 缓存和哈希优化
uint32_t asm_compute_cache_key_hash(const char* url, const char* headers);

// 压缩优化
int asm_prepare_compression_data(const void* data, size_t len, uint32_t* checksum);

// 网络字节序优化
void asm_convert_network_headers(http_response_header_t* header);

// 性能基准测试
void asm_benchmark_operations(void);

// 状态查询
asm_optimization_status_t* asm_get_optimization_status(void);
void asm_print_status_report(void);

#endif // ASM_INTEGRATION_H 