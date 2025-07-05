#include "core.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "net.h"

route_t find_route(const core_config_t *core_conf, const char *host,
                   const char *uri) {
  route_t route = {NULL, NULL};
  if (!core_conf || !core_conf->raw_config || !core_conf->raw_config->http || !core_conf->raw_config->http->servers) {
    return route;
  }

  // 1. Find the matching server block
  server_block_t *matched_server = NULL;
  // First server is default
  server_block_t *default_server = core_conf->raw_config->http->servers; 

  for (server_block_t *srv = core_conf->raw_config->http->servers; srv; srv = srv->next) {
    const char *server_name = get_directive_value("server_name", srv->directives, srv->directive_count);
    if (host && server_name && strcmp(host, server_name) == 0) {
      matched_server = srv;
      break;
    }
  }

  if (!matched_server) {
    matched_server = default_server;
  }
  route.server = matched_server;


  // 2. Find the best matching location block within that server
  if (matched_server) {
    location_block_t *best_match = NULL;
    size_t best_match_len = 0;

    for (location_block_t *loc = matched_server->locations; loc; loc = loc->next) {
      size_t loc_path_len = strlen(loc->path);
      if (strncmp(uri, loc->path, loc_path_len) == 0) {
        // It's a prefix match. Is it the best one so far?
        if (loc_path_len > best_match_len) {
          best_match = loc;
          best_match_len = loc_path_len;
        }
      }
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
    if (core_config->raw_config) {
        free_config(core_config->raw_config);
    }
    free(core_config);
} 