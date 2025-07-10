#ifndef ANX_RUST_H
#define ANX_RUST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Type Definitions
// =============================================================================

/// Opaque handle for configuration
typedef struct ConfigHandle ConfigHandle;

/// Opaque handle for HTTP request
typedef struct HttpRequestHandle HttpRequestHandle;

/// Opaque handle for HTTP response
typedef struct HttpResponseHandle HttpResponseHandle;

/// Opaque handle for cache
typedef struct CacheHandle CacheHandle;

/// Cache response structure
typedef struct {
    uint8_t *data;
    size_t data_len;
    char *content_type;
    char *etag;
    unsigned long last_modified;
    int is_compressed;
    int needs_validation;
} CacheResponseC;

/// Cache statistics structure
typedef struct {
    unsigned long hits;
    unsigned long misses;
    unsigned long puts;
    unsigned long evictions;
    size_t current_size;
    size_t current_entries;
    double hit_rate;
} CacheStatsC;

// =============================================================================
// Configuration Functions
// =============================================================================

/// Load configuration from TOML file
ConfigHandle* anx_config_load_toml(const char *path);

/// Load configuration from Nginx file
ConfigHandle* anx_config_load_nginx(const char *path);

/// Get server listen address by index
char* anx_config_get_listen(const ConfigHandle *handle, size_t index);

/// Get server root directory
char* anx_config_get_root(const ConfigHandle *handle);

/// Get server worker processes count
int anx_config_get_worker_processes(const ConfigHandle *handle);

/// Get server worker connections count
int anx_config_get_worker_connections(const ConfigHandle *handle);

/// Get number of locations
int anx_config_get_locations_count(const ConfigHandle *handle);

/// Get location path by index
char* anx_config_get_location_path(const ConfigHandle *handle, size_t index);

/// Validate configuration
int anx_config_validate(const ConfigHandle *handle);

/// Free configuration handle
void anx_config_free(ConfigHandle *handle);

// =============================================================================
// HTTP Parser Functions
// =============================================================================

/// Parse HTTP request from raw data
HttpRequestHandle* anx_http_parse_request(const uint8_t *data, size_t len);

/// Get HTTP request method
char* anx_http_get_method(const HttpRequestHandle *handle);

/// Get HTTP request URI
char* anx_http_get_uri(const HttpRequestHandle *handle);

/// Get HTTP request version
char* anx_http_get_version(const HttpRequestHandle *handle);

/// Get HTTP request header value
char* anx_http_get_header(const HttpRequestHandle *handle, const char *name);

/// Get HTTP request body
uint8_t* anx_http_get_body(const HttpRequestHandle *handle, size_t *len);

/// Check if request is keep-alive
int anx_http_is_keep_alive(const HttpRequestHandle *handle);

/// Free HTTP request handle
void anx_http_request_free(HttpRequestHandle *handle);

/// Create HTTP response
HttpResponseHandle* anx_http_response_new(unsigned int status_code, const char *reason_phrase);

/// Set HTTP response header
int anx_http_response_set_header(HttpResponseHandle *handle, const char *name, const char *value);

/// Set HTTP response body
int anx_http_response_set_body(HttpResponseHandle *handle, const uint8_t *data, size_t len);

/// Convert HTTP response to bytes
uint8_t* anx_http_response_to_bytes(const HttpResponseHandle *handle, size_t *len);

/// Free HTTP response handle
void anx_http_response_free(HttpResponseHandle *handle);

// =============================================================================
// Cache Functions
// =============================================================================

/// Cache strategies
#define ANX_CACHE_STRATEGY_LRU  0
#define ANX_CACHE_STRATEGY_LFU  1
#define ANX_CACHE_STRATEGY_FIFO 2

/// Create new cache with default configuration
CacheHandle* anx_cache_new(void);

/// Create new cache with custom configuration
CacheHandle* anx_cache_new_with_config(size_t max_size, size_t max_entries, 
                                      unsigned long default_ttl_secs, int strategy);

/// Get value from cache
CacheResponseC* anx_cache_get(const CacheHandle *handle, const char *key);

/// Get value from cache with conditional headers
CacheResponseC* anx_cache_get_conditional(const CacheHandle *handle, const char *key, 
                                         const char *if_none_match, unsigned long if_modified_since);

/// Put value into cache
int anx_cache_put(const CacheHandle *handle, const char *key, 
                  const uint8_t *data, size_t data_len, const char *content_type);

/// Put value into cache with metadata
int anx_cache_put_with_metadata(const CacheHandle *handle, const char *key, 
                                const uint8_t *data, size_t data_len, 
                                const char *content_type, const char *etag, 
                                unsigned long last_modified, unsigned long ttl_secs);

/// Remove value from cache
int anx_cache_remove(const CacheHandle *handle, const char *key);

/// Clear all entries from cache
void anx_cache_clear(const CacheHandle *handle);

/// Get cache statistics
CacheStatsC* anx_cache_get_stats(const CacheHandle *handle);

/// Clean up expired entries
void anx_cache_cleanup_expired(const CacheHandle *handle);

/// Generate ETag for content
char* anx_cache_generate_etag(const uint8_t *data, size_t len, unsigned long last_modified);

/// Free cache handle
void anx_cache_free(CacheHandle *handle);

/// Free cache response
void anx_cache_response_free(CacheResponseC *response);

/// Free cache statistics
void anx_cache_stats_free(CacheStatsC *stats);

// =============================================================================
// Utility Functions
// =============================================================================

/// Free C string allocated by Rust
void anx_free_string(char *ptr);

/// Free byte array allocated by Rust
void anx_free_bytes(uint8_t *ptr, size_t len);

// =============================================================================
// Library Initialization
// =============================================================================

/// Initialize the Rust modules
int anx_rust_init(void);

/// Cleanup the Rust modules
void anx_rust_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // ANX_RUST_H 