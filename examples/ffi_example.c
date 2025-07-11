/**
 * FFI接口使用示例
 * 展示如何使用Rust模块的C接口进行HTTP解析和缓存操作
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "anx_rust.h"

// 测试HTTP解析器FFI接口
void test_http_parser_ffi() {
    printf("=== HTTP解析器FFI测试 ===\n");
    
    // 测试HTTP请求解析
    const char* request_data = 
        "GET /api/users HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: Mozilla/5.0\r\n"
        "Accept: application/json\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    
    HttpRequestHandle* request = anx_http_parse_request((const uint8_t*)request_data, strlen(request_data));
    if (request) {
        printf("HTTP请求解析成功:\n");
        
        char* method = anx_http_get_method(request);
        char* uri = anx_http_get_uri(request);
        char* version = anx_http_get_version(request);
        char* host = anx_http_get_header(request, "Host");
        char* user_agent = anx_http_get_header(request, "User-Agent");
        
        printf("  方法: %s\n", method ? method : "N/A");
        printf("  URI: %s\n", uri ? uri : "N/A");
        printf("  版本: %s\n", version ? version : "N/A");
        printf("  Host: %s\n", host ? host : "N/A");
        printf("  User-Agent: %s\n", user_agent ? user_agent : "N/A");
        
        // 释放字符串
        if (method) anx_free_string(method);
        if (uri) anx_free_string(uri);
        if (version) anx_free_string(version);
        if (host) anx_free_string(host);
        if (user_agent) anx_free_string(user_agent);
        
        anx_http_request_free(request);
    } else {
        printf("HTTP请求解析失败\n");
    }
    
    // 测试HTTP响应构建
    HttpResponseHandle* response = anx_http_response_new(200, "OK");
    if (response) {
        anx_http_response_set_header(response, "Content-Type", "application/json");
        anx_http_response_set_header(response, "Cache-Control", "max-age=3600");
        anx_http_response_set_body(response, (const uint8_t*)"{\"status\":\"success\"}", 21);
        
        size_t response_len;
        uint8_t* response_bytes = anx_http_response_to_bytes(response, &response_len);
        if (response_bytes) {
            printf("HTTP响应构建成功:\n%.*s\n", (int)response_len, response_bytes);
            anx_free_bytes(response_bytes, response_len);
        }
        
        anx_http_response_free(response);
    }
    
    printf("HTTP解析器测试完成\n\n");
}

// 测试缓存模块FFI接口
void test_cache_ffi() {
    printf("=== 缓存模块FFI测试 ===\n");
    
    // 创建缓存实例
    CacheHandle* cache = anx_cache_new();
    if (!cache) {
        printf("创建缓存实例失败\n");
        return;
    }
    printf("缓存实例创建成功\n");
    
    // 测试缓存PUT操作
    const char* key = "test_key";
    const char* data = "Hello, World!";
    const char* content_type = "text/plain";
    
    int put_result = anx_cache_put(cache, key, (const uint8_t*)data, strlen(data), content_type);
    if (put_result == 0) {
        printf("缓存PUT操作成功\n");
    } else {
        printf("缓存PUT操作失败\n");
    }
    
    // 测试缓存GET操作
    CacheResponseC* response = anx_cache_get(cache, key);
    if (response) {
        printf("缓存GET操作成功:\n");
        printf("  数据长度: %zu\n", response->data_len);
        printf("  内容类型: %s\n", response->content_type ? response->content_type : "N/A");
        printf("  ETag: %s\n", response->etag ? response->etag : "N/A");
        printf("  数据: %.*s\n", (int)response->data_len, response->data);
        
        anx_cache_response_free(response);
    } else {
        printf("缓存GET操作失败\n");
    }
    
    // 测试缓存未命中
    CacheResponseC* miss_response = anx_cache_get(cache, "nonexistent_key");
    if (!miss_response) {
        printf("缓存未命中测试通过\n");
    }
    
    // 测试缓存统计
    CacheStatsC* stats = anx_cache_get_stats(cache);
    if (stats) {
        printf("缓存统计:\n");
        printf("  命中次数: %lu\n", stats->hits);
        printf("  未命中次数: %lu\n", stats->misses);
        printf("  PUT操作次数: %lu\n", stats->puts);
        printf("  驱逐次数: %lu\n", stats->evictions);
        printf("  当前条目数: %zu\n", stats->current_entries);
        printf("  当前大小: %zu\n", stats->current_size);
        printf("  命中率: %.2f%%\n", stats->hit_rate * 100);
        
        anx_cache_stats_free(stats);
    }
    
    // 测试缓存清理
    anx_cache_clear(cache);
    printf("缓存清理完成\n");
    
    anx_cache_free(cache);
    printf("缓存模块测试完成\n\n");
}

// 测试配置系统FFI接口
void test_config_ffi() {
    printf("=== 配置系统FFI测试 ===\n");
    
    // 创建TOML配置内容
    const char* toml_content = 
        "[server]\n"
        "port = 8080\n"
        "host = \"0.0.0.0\"\n"
        "threads = 4\n"
        "\n"
        "[logging]\n"
        "level = \"info\"\n"
        "file = \"/var/log/server.log\"\n"
        "\n"
        "[cache]\n"
        "enabled = true\n"
        "max_size = 1048576\n"
        "strategy = \"lru\"\n";
    
    // 创建临时TOML文件
    FILE* temp_file = tmpfile();
    if (!temp_file) {
        printf("创建临时文件失败\n");
        return;
    }
    
    fwrite(toml_content, 1, strlen(toml_content), temp_file);
    fflush(temp_file);
    rewind(temp_file);
    
    // 获取临时文件路径
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/proc/self/fd/%d", fileno(temp_file));
    
    // 加载配置
    ConfigHandle* config = anx_config_load_toml(temp_path);
    if (config) {
        printf("配置加载成功\n");
        
        // 获取服务器配置
        char* listen = anx_config_get_listen(config, 0);
        char* root = anx_config_get_root(config);
        int worker_processes = anx_config_get_worker_processes(config);
        int worker_connections = anx_config_get_worker_connections(config);
        
        printf("服务器配置:\n");
        printf("  监听地址: %s\n", listen ? listen : "N/A");
        printf("  根目录: %s\n", root ? root : "N/A");
        printf("  工作进程数: %d\n", worker_processes);
        printf("  工作连接数: %d\n", worker_connections);
        
        // 获取位置配置
        int locations_count = anx_config_get_locations_count(config);
        printf("位置配置数量: %d\n", locations_count);
        
        for (int i = 0; i < locations_count; i++) {
            char* location_path = anx_config_get_location_path(config, i);
            printf("  位置 %d: %s\n", i, location_path ? location_path : "N/A");
            if (location_path) anx_free_string(location_path);
        }
        
        // 释放字符串
        if (listen) anx_free_string(listen);
        if (root) anx_free_string(root);
        
        anx_config_free(config);
    } else {
        printf("配置加载失败\n");
    }
    
    fclose(temp_file);
    printf("配置系统测试完成\n\n");
}

int main() {
    printf("ASM HTTP Server FFI接口测试\n");
    printf("========================\n\n");
    
    // 测试配置系统
    test_config_ffi();
    
    // 测试HTTP解析器
    test_http_parser_ffi();
    
    // 测试缓存模块
    test_cache_ffi();
    
    printf("所有FFI接口测试完成\n");
    return 0;
} 