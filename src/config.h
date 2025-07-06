#ifndef CONFIG_H
#define CONFIG_H

#include <openssl/ssl.h>
#include <stdbool.h>
#include "log.h"
#include "compress.h"
#include "cache.h"

// 前向声明
typedef struct health_check_config health_check_config_t;

// Represents a single "key value;" directive in nginx config
typedef struct {
  char *key;
  char *value;
} directive_t;

// Represents a location /path { ... } block
typedef struct location_block {
  char *path;
  directive_t *directives;
  int directive_count;
  struct location_block *next;
} location_block_t;

// Represents an upstream server entry
typedef struct upstream_server_entry {
  char *host;
  int port;
  int weight;
  int max_fails;
  int fail_timeout;
  int max_conns;
  // 健康检查配置
  health_check_config_t *health_config;
  struct upstream_server_entry *next;
} upstream_server_entry_t;

// Represents an upstream { ... } block
typedef struct upstream_block {
  char *name;
  directive_t *directives;
  int directive_count;
  upstream_server_entry_t *servers;
  // 全局健康检查配置
  health_check_config_t *default_health_config;
  struct upstream_block *next;
} upstream_block_t;

// Represents a server { ... } block
typedef struct server_block {
  directive_t *directives;
  int directive_count;
  location_block_t *locations;
  struct server_block *next;
} server_block_t;

// Represents the main http { ... } block
typedef struct {
  directive_t *directives;
  int directive_count;
  server_block_t *servers;
  upstream_block_t *upstreams;
} http_block_t;

// The root of our entire configuration
typedef struct {
  http_block_t *http;
  // We can add other top-level blocks like 'events' here later
  int worker_processes;
  char *error_log;
  char *access_log;
  log_level_t log_level;
  access_log_format_t log_format;
  int log_rotation_size;
  int log_rotation_days;
  bool enable_performance_logging;
  
  // 压缩配置
  compress_config_t *compress;
  
  // 缓存配置
  cache_config_t *cache;
} config_t;

// Helper function to find the value of a directive within an array.
const char *get_directive_value(const char *key, const directive_t *directives, int count);

// Function to parse the configuration file and populate the global config struct.
config_t *parse_config(const char *filename);

// Function to free the memory allocated for the configuration
void free_config(config_t *config);

// Helper function to resolve relative paths based on config file location
char* resolve_config_path(const char* path);

// Extract logging configuration from parsed config
log_config_t *extract_log_config(const config_t *config);

// Parse log format string to enum
access_log_format_t parse_log_format(const char *format_str);

// Get default log configuration
log_config_t *get_default_log_config(void);

// 健康检查配置解析函数
health_check_config_t *parse_health_check_config(const directive_t *directives, int count);
void free_health_check_config(health_check_config_t *config);

#endif  // CONFIG_H 