#include "core.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "net.h"

// Helper to find a directive's value in a directive array
static const char *get_directive_value(const char *key,
                                       const directive_t *directives,
                                       int count) {
  for (int i = 0; i < count; i++) {
    if (strcmp(directives[i].key, key) == 0) {
      return directives[i].value;
    }
  }
  return NULL;
}

core_config_t *create_core_config(config_t *parsed_config) {
  if (!parsed_config || !parsed_config->http) {
    log_message(LOG_LEVEL_ERROR, "No http block found in configuration.");
    return NULL;
  }

  core_config_t *core_conf = calloc(1, sizeof(core_config_t));
  if (!core_conf) return NULL;

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
            if (cert_path) sock->ssl_certificate = strdup(cert_path);
            if (key_path) sock->ssl_certificate_key = strdup(key_path);
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
    free(core_config);
} 