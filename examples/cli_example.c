/**
 * 命令行参数解析示例
 * 展示如何使用CLI模块解析命令行参数
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "anx_rust.h"

void print_cli_config(const CliConfigHandle* config) {
    if (!config) {
        printf("配置为空\n");
        return;
    }
    
    printf("=== CLI配置信息 ===\n");
    
    // 基本配置
    printf("端口: %d\n", anx_cli_get_port(config));
    
    char* host = anx_cli_get_host(config);
    printf("主机: %s\n", host ? host : "N/A");
    if (host) anx_free_string(host);
    
    // 静态文件配置
    char* static_dir = anx_cli_get_static_dir(config);
    printf("静态文件目录: %s\n", static_dir ? static_dir : "N/A");
    if (static_dir) anx_free_string(static_dir);
    
    // 反向代理配置
    size_t proxy_count = anx_cli_get_proxy_count(config);
    printf("反向代理数量: %zu\n", proxy_count);
    
    for (size_t i = 0; i < proxy_count; i++) {
        char* proxy_url = anx_cli_get_proxy_url(config, i);
        char* proxy_prefix = anx_cli_get_proxy_path_prefix(config, i);
        
        printf("  代理 %zu:\n", i);
        printf("    URL: %s\n", proxy_url ? proxy_url : "N/A");
        printf("    路径前缀: %s\n", proxy_prefix ? proxy_prefix : "N/A");
        
        if (proxy_url) anx_free_string(proxy_url);
        if (proxy_prefix) anx_free_string(proxy_prefix);
    }
    
    // SSL配置
    int ssl_enabled = anx_cli_is_ssl_enabled(config);
    printf("SSL启用: %s\n", ssl_enabled ? "是" : "否");
    
    if (ssl_enabled) {
        char* cert_file = anx_cli_get_ssl_cert_file(config);
        char* key_file = anx_cli_get_ssl_key_file(config);
        
        printf("  SSL证书: %s\n", cert_file ? cert_file : "N/A");
        printf("  SSL私钥: %s\n", key_file ? key_file : "N/A");
        
        if (cert_file) anx_free_string(cert_file);
        if (key_file) anx_free_string(key_file);
    }
    
    // 日志配置
    char* log_level = anx_cli_get_log_level(config);
    char* log_file = anx_cli_get_log_file(config);
    
    printf("日志级别: %s\n", log_level ? log_level : "N/A");
    printf("日志文件: %s\n", log_file ? log_file : "N/A");
    
    if (log_level) anx_free_string(log_level);
    if (log_file) anx_free_string(log_file);
    
    // 缓存配置
    int cache_enabled = anx_cli_is_cache_enabled(config);
    size_t cache_size = anx_cli_get_cache_size(config);
    unsigned long cache_ttl = anx_cli_get_cache_ttl(config);
    
    printf("缓存启用: %s\n", cache_enabled ? "是" : "否");
    printf("缓存大小: %zu bytes\n", cache_size);
    printf("缓存TTL: %lu seconds\n", cache_ttl);
    
    // 性能配置
    size_t threads = anx_cli_get_threads(config);
    size_t max_connections = anx_cli_get_max_connections(config);
    
    printf("线程数: %zu\n", threads);
    printf("最大连接数: %zu\n", max_connections);
    
    // 其他配置
    int daemon = anx_cli_is_daemon(config);
    char* pid_file = anx_cli_get_pid_file(config);
    
    printf("守护进程: %s\n", daemon ? "是" : "否");
    printf("PID文件: %s\n", pid_file ? pid_file : "N/A");
    
    if (pid_file) anx_free_string(pid_file);
    
    printf("==================\n");
}

int main(int argc, char* argv[]) {
    printf("ASM HTTP Server CLI参数解析示例\n");
    printf("================================\n\n");
    
    // 创建CLI解析器
    CliParser* parser = anx_cli_parser_create();
    if (!parser) {
        printf("创建CLI解析器失败\n");
        return 1;
    }
    
    // 模拟命令行参数
    printf("模拟命令行参数:\n");
    printf("  ./asm_server -d /var/www/html -p 8080 --proxy http://api:8080 /api --ssl-cert cert.pem --ssl-key key.pem\n\n");
    
    // 这里我们直接测试默认配置
    // 在实际应用中，这些参数会从argv传递
    CliConfigHandle* config = anx_cli_parse_args(parser);
    if (!config) {
        printf("解析命令行参数失败\n");
        anx_cli_parser_free(parser);
        return 1;
    }
    
    // 打印配置信息
    print_cli_config(config);
    
    // 清理资源
    anx_cli_config_free(config);
    anx_cli_parser_free(parser);
    
    printf("\nCLI参数解析示例完成\n");
    return 0;
} 