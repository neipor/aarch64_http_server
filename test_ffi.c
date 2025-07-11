#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/include/anx_rust.h"

int main() {
    printf("=== ASM HTTP Server FFI 接口测试 ===\n\n");
    
    // 测试HTTP解析器
    printf("1. 测试HTTP解析器...\n");
    test_http_parser();
    
    // 测试缓存模块
    printf("\n2. 测试缓存模块...\n");
    test_cache_module();
    
    printf("\n=== FFI 测试完成 ===\n");
    return 0;
}

void test_http_parser() {
    // 测试HTTP请求解析
    const char *request_data = "GET /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
    size_t len = strlen(request_data);
    
    HttpRequestHandle *request = anx_http_parse_request((const uint8_t*)request_data, len);
    if (request) {
        printf("✓ HTTP请求解析成功\n");
        
        char *method = anx_http_get_method(request);
        char *uri = anx_http_get_uri(request);
        char *version = anx_http_get_version(request);
        
        if (method) {
            printf("  方法: %s\n", method);
            anx_free_string(method);
        }
        if (uri) {
            printf("  URI: %s\n", uri);
            anx_free_string(uri);
        }
        if (version) {
            printf("  版本: %s\n", version);
            anx_free_string(version);
        }
        
        anx_http_request_free(request);
    } else {
        printf("✗ HTTP请求解析失败\n");
    }
    
    // 测试HTTP响应构建
    HttpResponseHandle *response = anx_http_response_new(200, "OK");
    if (response) {
        printf("✓ HTTP响应创建成功\n");
        
        anx_http_response_set_header(response, "Content-Type", "text/plain");
        anx_http_response_set_body(response, (const uint8_t*)"Hello, World!", 13);
        
        size_t response_len;
        uint8_t *response_data = anx_http_response_to_bytes(response, &response_len);
        if (response_data) {
            printf("  响应长度: %zu\n", response_len);
            anx_free_bytes(response_data, response_len);
        }
        
        anx_http_response_free(response);
    } else {
        printf("✗ HTTP响应创建失败\n");
    }
}

void test_cache_module() {
    // 创建缓存
    CacheHandle *cache = anx_cache_new();
    if (!cache) {
        printf("✗ 缓存创建失败\n");
        return;
    }
    printf("✓ 缓存创建成功\n");
    
    // 测试基本操作
    const char *key = "test_key";
    const char *data = "test data";
    size_t data_len = strlen(data);
    
    int result = anx_cache_put(cache, key, (const uint8_t*)data, data_len, "text/plain");
    if (result == 0) {
        printf("✓ 缓存PUT操作成功\n");
    } else {
        printf("✗ 缓存PUT操作失败\n");
    }
    
    CacheResponseC *response = anx_cache_get(cache, key);
    if (response) {
        printf("✓ 缓存GET操作成功\n");
        printf("  数据长度: %zu\n", response->data_len);
        printf("  内容类型: %s\n", response->content_type ? response->content_type : "NULL");
        
        anx_cache_response_free(response);
    } else {
        printf("✗ 缓存GET操作失败\n");
    }
    
    // 测试缓存未命中
    CacheResponseC *miss_response = anx_cache_get(cache, "nonexistent");
    if (miss_response) {
        printf("✗ 意外的缓存命中\n");
        anx_cache_response_free(miss_response);
    } else {
        printf("✓ 缓存未命中（预期）\n");
    }
    
    // 测试统计信息
    CacheStatsC *stats = anx_cache_get_stats(cache);
    if (stats) {
        printf("✓ 缓存统计信息:\n");
        printf("  PUT操作: %lu\n", stats->puts);
        printf("  命中次数: %lu\n", stats->hits);
        printf("  未命中次数: %lu\n", stats->misses);
        printf("  当前条目数: %zu\n", stats->current_entries);
        printf("  命中率: %.2f%%\n", stats->hit_rate * 100.0);
        
        anx_cache_stats_free(stats);
    }
    
    anx_cache_free(cache);
} 