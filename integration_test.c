#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "src/include/anx_rust.h"

void test_http_parser() {
    printf("=== HTTP Parser Test ===\n");
    
    // Test HTTP request parsing
    const char* request_data = 
        "GET /api/users HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: TestClient/1.0\r\n"
        "Accept: application/json\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    HttpRequestHandle* req = anx_http_parse_request(
        (const uint8_t*)request_data, 
        strlen(request_data)
    );
    
    if (req) {
        char* method = anx_http_get_method(req);
        char* uri = anx_http_get_uri(req);
        char* version = anx_http_get_version(req);
        char* host = anx_http_get_header(req, "Host");
        char* user_agent = anx_http_get_header(req, "User-Agent");
        
        printf("Method: %s\n", method ? method : "NULL");
        printf("URI: %s\n", uri ? uri : "NULL");
        printf("Version: %s\n", version ? version : "NULL");
        printf("Host: %s\n", host ? host : "NULL");
        printf("User-Agent: %s\n", user_agent ? user_agent : "NULL");
        printf("Keep-Alive: %s\n", anx_http_is_keep_alive(req) ? "Yes" : "No");
        
        anx_free_string(method);
        anx_free_string(uri);
        anx_free_string(version);
        anx_free_string(host);
        anx_free_string(user_agent);
        anx_http_request_free(req);
    } else {
        printf("Failed to parse HTTP request\n");
    }
    
    // Test HTTP response creation
    HttpResponseHandle* resp = anx_http_response_new(200, "OK");
    if (resp) {
        anx_http_response_set_header(resp, "Content-Type", "application/json");
        anx_http_response_set_header(resp, "Cache-Control", "max-age=3600");
        
        const char* body = "{\"status\":\"success\",\"data\":\"test\"}";
        anx_http_response_set_body(resp, (const uint8_t*)body, strlen(body));
        
        size_t response_len;
        uint8_t* response_data = anx_http_response_to_bytes(resp, &response_len);
        
        printf("Response length: %zu\n", response_len);
        printf("Response data: %.*s\n", (int)response_len, response_data);
        
        anx_free_bytes(response_data, response_len);
        anx_http_response_free(resp);
    }
    
    printf("\n");
}

void test_cache() {
    printf("=== Cache Test ===\n");
    
    // Create cache with custom configuration
    CacheHandle* cache = anx_cache_new_with_config(
        1024 * 1024,  // 1MB max size
        1000,          // 1000 max entries
        3600,          // 1 hour default TTL
        0              // LRU strategy
    );
    
    if (!cache) {
        printf("Failed to create cache\n");
        return;
    }
    
    // Test cache put
    const char* key = "test_key";
    const char* data = "Hello, World!";
    const char* content_type = "text/plain";
    
    int result = anx_cache_put(cache, key, (const uint8_t*)data, strlen(data), content_type);
    printf("Cache put result: %d\n", result);
    
    // Test cache get
    CacheResponseC* response = anx_cache_get(cache, key);
    if (response) {
        printf("Cache hit! Data: %.*s\n", (int)response->data_len, response->data);
        printf("Content-Type: %s\n", response->content_type ? response->content_type : "NULL");
        printf("ETag: %s\n", response->etag ? response->etag : "NULL");
        printf("Last-Modified: %lu\n", response->last_modified);
        printf("Is compressed: %s\n", response->is_compressed ? "Yes" : "No");
        printf("Needs validation: %s\n", response->needs_validation ? "Yes" : "No");
        
        anx_cache_response_free(response);
    } else {
        printf("Cache miss\n");
    }
    
    // Test cache miss
    CacheResponseC* miss_response = anx_cache_get(cache, "nonexistent_key");
    if (miss_response) {
        printf("Unexpected cache hit for nonexistent key\n");
        anx_cache_response_free(miss_response);
    } else {
        printf("Expected cache miss for nonexistent key\n");
    }
    
    // Test cache stats
    CacheStatsC* stats = anx_cache_get_stats(cache);
    if (stats) {
        printf("Cache Stats:\n");
        printf("  Hits: %lu\n", stats->hits);
        printf("  Misses: %lu\n", stats->misses);
        printf("  Puts: %lu\n", stats->puts);
        printf("  Evictions: %lu\n", stats->evictions);
        printf("  Current size: %zu\n", stats->current_size);
        printf("  Current entries: %zu\n", stats->current_entries);
        printf("  Hit rate: %.2f%%\n", stats->hit_rate * 100.0);
        
        anx_cache_stats_free(stats);
    }
    
    // Test ETag generation
    const char* etag_data = "test data for etag";
    char* etag = anx_cache_generate_etag(
        (const uint8_t*)etag_data, 
        strlen(etag_data), 
        time(NULL)
    );
    if (etag) {
        printf("Generated ETag: %s\n", etag);
        anx_free_string(etag);
    }
    
    anx_cache_free(cache);
    printf("\n");
}

void test_cli() {
    printf("=== CLI Test ===\n");
    
    // Create CLI parser
    CliParser* parser = anx_cli_parser_create();
    if (!parser) {
        printf("Failed to create CLI parser\n");
        return;
    }
    
    // Test CLI parsing (simulate command line arguments)
    // Note: In a real scenario, this would be called with actual argc/argv
    CliConfigHandle* config = anx_cli_parse_args(parser);
    if (config) {
        printf("CLI Config:\n");
        printf("  Port: %u\n", anx_cli_get_port(config));
        
        char* host = anx_cli_get_host(config);
        printf("  Host: %s\n", host ? host : "NULL");
        anx_free_string(host);
        
        char* static_dir = anx_cli_get_static_dir(config);
        printf("  Static Directory: %s\n", static_dir ? static_dir : "NULL");
        anx_free_string(static_dir);
        
        printf("  Proxy Count: %zu\n", anx_cli_get_proxy_count(config));
        
        printf("  SSL Enabled: %s\n", anx_cli_is_ssl_enabled(config) ? "Yes" : "No");
        
        char* log_level = anx_cli_get_log_level(config);
        printf("  Log Level: %s\n", log_level ? log_level : "NULL");
        anx_free_string(log_level);
        
        printf("  Cache Enabled: %s\n", anx_cli_is_cache_enabled(config) ? "Yes" : "No");
        printf("  Cache Size: %zu\n", anx_cli_get_cache_size(config));
        printf("  Cache TTL: %lu\n", anx_cli_get_cache_ttl(config));
        printf("  Threads: %zu\n", anx_cli_get_threads(config));
        printf("  Max Connections: %zu\n", anx_cli_get_max_connections(config));
        printf("  Daemon Mode: %s\n", anx_cli_is_daemon(config) ? "Yes" : "No");
        
        anx_cli_config_free(config);
    } else {
        printf("Failed to parse CLI arguments\n");
    }
    
    anx_cli_parser_free(parser);
    printf("\n");
}

void test_config() {
    printf("=== Config Test ===\n");
    
    // Test TOML config loading
    ConfigHandle* config = anx_config_load_toml("configs/test.toml");
    if (config) {
        printf("TOML config loaded successfully\n");
        
        // Test config validation
        int valid = anx_config_validate(config);
        printf("Config valid: %s\n", valid == 0 ? "Yes" : "No");
        
        // Test config getters
        char* listen = anx_config_get_listen(config, 0);
        printf("Listen: %s\n", listen ? listen : "NULL");
        anx_free_string(listen);
        
        char* root = anx_config_get_root(config);
        printf("Root: %s\n", root ? root : "NULL");
        anx_free_string(root);
        
        printf("Worker Processes: %d\n", anx_config_get_worker_processes(config));
        printf("Worker Connections: %d\n", anx_config_get_worker_connections(config));
        printf("Locations Count: %d\n", anx_config_get_locations_count(config));
        
        anx_config_free(config);
    } else {
        printf("Failed to load TOML config\n");
    }
    
    // Test Nginx config loading
    ConfigHandle* nginx_config = anx_config_load_nginx("configs/nginx.conf");
    if (nginx_config) {
        printf("Nginx config loaded successfully\n");
        anx_config_free(nginx_config);
    } else {
        printf("Failed to load Nginx config\n");
    }
    
    printf("\n");
}

int main() {
    printf("ANX HTTP Server v1.1.0+ Integration Test\n");
    printf("==========================================\n\n");
    
    // Set library path for FFI
    setenv("LD_LIBRARY_PATH", ".", 1);
    
    test_http_parser();
    test_cache();
    test_cli();
    test_config();
    
    printf("Integration test completed!\n");
    return 0;
} 