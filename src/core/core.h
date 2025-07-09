#ifndef CORE_H
#define CORE_H

#include <openssl/ssl.h>
#include "config.h"
#include "cache.h"
#include "load_balancer.h"

// 前向声明
typedef struct lb_config lb_config_t;

// Represents a single listening endpoint (a socket)
typedef struct {
  int fd;
  int port;
  int is_ssl;
  char *ssl_certificate;
  char *ssl_certificate_key;
} listening_socket_t;

// Contains the core, processed configuration needed for the server to run.
typedef struct {
  int worker_processes;
  listening_socket_t *listening_sockets;
  int listening_socket_count;
  // A pointer back to the raw parsed config tree
  config_t *raw_config;
  // We can add pointers to processed virtual host configs here later
  
  // 缓存管理器
  cache_manager_t *cache_manager;
  
  // 负载均衡配置
  lb_config_t *lb_config;
} core_config_t;

// This struct holds the result of a routing decision.
typedef struct {
  server_block_t *server;
  location_block_t *location; // NULL if no specific location matches
} route_t;

// Finds the best matching server and location block for a given request.
route_t find_route(const core_config_t *core_conf, const char *host,
                   const char *uri, int port);

// Processes the raw, parsed config tree (g_config) and populates a core_config_t
core_config_t *create_core_config(config_t *parsed_config);

// Frees all resources associated with the core config
void free_core_config(core_config_t *core_config);

#endif  // CORE_H 