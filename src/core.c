#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "core.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "config.h"
#include "log.h"
#include "net.h"
#include "health_check.h"

route_t find_route(const core_config_t *core_conf, const char *host,
                   const char *uri, int port) {
  route_t route = {NULL, NULL};
  if (!core_conf || !core_conf->raw_config || !core_conf->raw_config->http || !core_conf->raw_config->http->servers) {
    return route;
  }

  // 1. Find the matching server block by host AND port
  server_block_t *matched_server = NULL;
  server_block_t *default_server = NULL;
  
  for (server_block_t *srv = core_conf->raw_config->http->servers; srv; srv = srv->next) {
    // Check if this server listens on the requested port
    bool listens_on_port = false;
    for (int i = 0; i < srv->directive_count; i++) {
      if (strcmp(srv->directives[i].key, "listen") == 0) {
        char *value_copy = strdup(srv->directives[i].value);
        char *token = strtok(value_copy, " ");
        int listen_port = token ? atoi(token) : 0;
        
        if (listen_port == port) {
          listens_on_port = true;
          if (!default_server) {
            default_server = srv; // First server listening on this port is default
          }
        }
        free(value_copy);
      }
    }
    
    if (listens_on_port) {
      const char *server_name = get_directive_value("server_name", srv->directives, srv->directive_count);
      if (host && server_name && strcmp(host, server_name) == 0) {
        matched_server = srv;
        break;
      }
    }
  }

  if (!matched_server) {
    matched_server = default_server;
  }
  
  if (!matched_server) {
    // Fallback to the very first server if no port matches
    matched_server = core_conf->raw_config->http->servers;
  }
  
  route.server = matched_server;

  // 2. Find the best matching location block within that server
  if (matched_server) {
    location_block_t *best_match = NULL;
    size_t best_match_len = 0;

    // Debug: log the available locations
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "DEBUG: Looking for location match for URI: %s on port %d", uri, port);
    log_message(LOG_LEVEL_DEBUG, log_msg);

    for (location_block_t *loc = matched_server->locations; loc; loc = loc->next) {
      size_t loc_path_len = strlen(loc->path);
      snprintf(log_msg, sizeof(log_msg), "DEBUG: Checking location: %s (len=%zu)", loc->path, loc_path_len);
      log_message(LOG_LEVEL_DEBUG, log_msg);
      
      if (strncmp(uri, loc->path, loc_path_len) == 0) {
        // It's a prefix match. Is it the best one so far?
        snprintf(log_msg, sizeof(log_msg), "DEBUG: Location %s matches URI %s", loc->path, uri);
        log_message(LOG_LEVEL_DEBUG, log_msg);
        
        if (loc_path_len > best_match_len) {
          best_match = loc;
          best_match_len = loc_path_len;
          snprintf(log_msg, sizeof(log_msg), "DEBUG: New best match: %s (len=%zu)", loc->path, loc_path_len);
          log_message(LOG_LEVEL_DEBUG, log_msg);
        }
      }
    }
    
    if (best_match) {
      snprintf(log_msg, sizeof(log_msg), "DEBUG: Final location match: %s", best_match->path);
      log_message(LOG_LEVEL_DEBUG, log_msg);
    } else {
      snprintf(log_msg, sizeof(log_msg), "DEBUG: No location matched for URI: %s", uri);
      log_message(LOG_LEVEL_DEBUG, log_msg);
    }
    
    route.location = best_match;
  }

  return route;
}

core_config_t *create_core_config(config_t *parsed_config) {
  if (!parsed_config || !parsed_config->http) {
    log_message(LOG_LEVEL_ERROR, "No http block found in configuration.");
    return NULL;
  }

  core_config_t *core_conf = calloc(1, sizeof(core_config_t));
  if (!core_conf) return NULL;

  core_conf->raw_config = parsed_config;

  // 初始化缓存管理器
  if (parsed_config->cache && parsed_config->cache->enable_cache) {
    core_conf->cache_manager = cache_manager_create(parsed_config->cache);
    if (!core_conf->cache_manager) {
      log_message(LOG_LEVEL_WARNING, "Failed to create cache manager");
    } else {
      log_message(LOG_LEVEL_INFO, "Cache manager initialized successfully");
    }
  }
  
  // 初始化负载均衡配置
  core_conf->lb_config = lb_config_create();
  if (!core_conf->lb_config) {
    log_message(LOG_LEVEL_WARNING, "Failed to create load balancer config");
  } else {
    log_message(LOG_LEVEL_INFO, "Load balancer config initialized successfully");
    
    // 从配置文件加载upstream组
    if (parsed_config->http && parsed_config->http->upstreams) {
      upstream_block_t *upstream = parsed_config->http->upstreams;
      while (upstream) {
        // 创建upstream组
        if (lb_config_add_group(core_conf->lb_config, upstream->name, LB_STRATEGY_ROUND_ROBIN) == 0) {
          upstream_group_t *group = lb_config_get_group(core_conf->lb_config, upstream->name);
          if (group) {
            // 添加服务器到组
            upstream_server_entry_t *server_entry = upstream->servers;
            while (server_entry) {
              if (upstream_group_add_server(group, server_entry->host, server_entry->port, 
                                          server_entry->weight) == 0) {
                // 设置服务器参数
                upstream_server_t *server = upstream_group_get_server(group, server_entry->host, 
                                                                    server_entry->port);
                if (server) {
                  server->max_fails = server_entry->max_fails;
                  server->fail_timeout = server_entry->fail_timeout;
                  server->max_conns = server_entry->max_conns;
                }
              }
              server_entry = server_entry->next;
            }
            
            // 解析upstream指令
            for (int i = 0; i < upstream->directive_count; i++) {
              const directive_t *dir = &upstream->directives[i];
              if (strcmp(dir->key, "least_conn") == 0) {
                group->strategy = LB_STRATEGY_LEAST_CONNECTIONS;
              } else if (strcmp(dir->key, "ip_hash") == 0) {
                group->strategy = LB_STRATEGY_IP_HASH;
              } else if (strcmp(dir->key, "random") == 0) {
                group->strategy = LB_STRATEGY_RANDOM;
              } else if (strcmp(dir->key, "keepalive") == 0) {
                // 暂时忽略，未来可以实现连接池
              }
            }
            
            // 初始化健康检查
            if (upstream->default_health_config || group->health_check_enabled) {
              health_check_config_t *health_config = upstream->default_health_config;
              
              // 如果没有特定的健康检查配置，使用默认配置
              if (!health_config) {
                health_config = health_check_config_create();
                if (health_config) {
                  health_config->enabled = true;
                  health_config->type = HEALTH_CHECK_HTTP;
                  health_config->interval = group->health_check_interval;
                  health_config->timeout = group->health_check_timeout;
                  health_check_config_set_uri(health_config, group->health_check_uri);
                }
              }
              
              if (health_config && health_config->enabled) {
                if (lb_start_health_check_manager(group, health_config) == 0) {
                  char log_msg[256];
                  snprintf(log_msg, sizeof(log_msg), "Health check started for upstream group '%s'", group->name);
                  log_message(LOG_LEVEL_INFO, log_msg);
                } else {
                  log_message(LOG_LEVEL_WARNING, "Failed to start health check for upstream group");
                }
                
                // 如果是临时创建的配置，释放它
                if (health_config != upstream->default_health_config) {
                  health_check_config_free(health_config);
                }
              }
            }
          }
        }
        upstream = upstream->next;
      }
    }
  }

  // 1. Get global settings from http block
  const char *workers_val = get_directive_value(
      "workers", parsed_config->http->directives,
      parsed_config->http->directive_count);
  core_conf->worker_processes = workers_val ? atoi(workers_val) : 2; // Default 2

  // 2. Iterate over server blocks to find all 'listen' directives
  server_block_t *srv = parsed_config->http->servers;
  while (srv) {
    for (int i = 0; i < srv->directive_count; i++) {
      if (strcmp(srv->directives[i].key, "listen") == 0) {
        core_conf->listening_socket_count++;
        core_conf->listening_sockets = realloc(
            core_conf->listening_sockets,
            core_conf->listening_socket_count * sizeof(listening_socket_t));
        
        listening_socket_t *sock = &core_conf->listening_sockets[core_conf->listening_socket_count - 1];
        
        // Properly parse "listen" directive, e.g., "80" or "443 ssl"
        char *value_copy = strdup(srv->directives[i].value);
        char *token = strtok(value_copy, " ");
        
        sock->port = token ? atoi(token) : 0;
        sock->is_ssl = 0;
        sock->ssl_certificate = NULL;
        sock->ssl_certificate_key = NULL;

        token = strtok(NULL, " ");
        if(token && strcmp(token, "ssl") == 0) {
            sock->is_ssl = 1;
            // If it's an SSL socket, find the certs in the same server block
            const char* cert_path = get_directive_value("ssl_certificate", srv->directives, srv->directive_count);
            const char* key_path = get_directive_value("ssl_certificate_key", srv->directives, srv->directive_count);
            if (cert_path) sock->ssl_certificate = resolve_config_path(cert_path);
            if (key_path) sock->ssl_certificate_key = resolve_config_path(key_path);
        }

        free(value_copy);
        
        // We create the actual socket FD later
        sock->fd = -1;
      }
    }
    srv = srv->next;
  }

  return core_conf;
}

void free_core_config(core_config_t *core_config) {
    if (!core_config) return;
    
    if (core_config->listening_sockets) {
        for (int i = 0; i < core_config->listening_socket_count; i++) {
            free(core_config->listening_sockets[i].ssl_certificate);
            free(core_config->listening_sockets[i].ssl_certificate_key);
        }
        free(core_config->listening_sockets);
    }
    
    if (core_config->cache_manager) {
        cache_manager_free(core_config->cache_manager);
    }
    
    if (core_config->lb_config) {
        // 停止所有健康检查管理器
        upstream_group_t *group = core_config->lb_config->groups;
        while (group) {
            if (lb_is_health_check_running(group)) {
                lb_stop_health_check_manager(group);
                log_message(LOG_LEVEL_INFO, "Stopped health check manager for upstream group");
            }
            group = group->next;
        }
        
        lb_config_free(core_config->lb_config);
    }
    
    if (core_config->raw_config) {
        free_config(core_config->raw_config);
    }
    
    free(core_config);
} 