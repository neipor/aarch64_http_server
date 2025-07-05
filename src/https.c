#include "https.h"

#include <errno.h>
#include <fcntl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <stdlib.h>

#include "config.h"
#include "core.h"
#include "log.h"
#include "util.h"

#define BUFFER_SIZE 4096
#define TEMP_DEFAULT_PAGE "/index.html"
#define TEMP_NOT_FOUND_PAGE "/404.html"

// Helper to parse Host header from request
static char *get_host(const char *buffer) {
    const char *host_hdr = "Host: ";
    char *host_start = strcasestr(buffer, host_hdr);
    if (!host_start) return NULL;

    host_start += strlen(host_hdr);
    char *host_end = strstr(host_start, "\r\n");
    if (!host_end) return NULL;

    return strndup(host_start, host_end - host_start);
}

void handle_https_request(SSL *ssl, const char *client_ip, core_config_t *core_conf) {
  char buffer[BUFFER_SIZE];
  int bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1);

  if (bytes_read <= 0) {
    // Handle SSL read error or closed connection
    return;
  }
  buffer[bytes_read] = '\0';

  char *buffer_copy = strdup(buffer);
  char *method = strtok(buffer_copy, " ");
  char *req_path = strtok(NULL, " ");
  char *http_version = strtok(NULL, "\r\n");
  char *host = get_host(buffer);

  if (!method || !req_path) {
    // Invalid request format
    free(buffer_copy);
    return;
  }

  char log_msg[BUFFER_SIZE];
  snprintf(log_msg, sizeof(log_msg), "HTTPS Request from %s: %s %s (Host: %s)",
           client_ip, method, req_path, host ? host : "none");
  log_message(LOG_LEVEL_INFO, log_msg);

  // --- Routing ---
  route_t route = find_route(core_conf, host, req_path);
  if (!route.server) {
      log_message(LOG_LEVEL_ERROR, "Could not find a server block for the request.");
      const char *response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
      SSL_write(ssl, response, strlen(response));
      free(host);
      free(buffer_copy);
      return;
  }

  const char *root = get_directive_value("root", route.server->directives, route.server->directive_count);
  if (route.location) {
      const char* loc_root = get_directive_value("root", route.location->directives, route.location->directive_count);
      if (loc_root) root = loc_root;
  }
  if (!root) {
      root = "./www";
  }

  char file_path[BUFFER_SIZE];
  if (strcmp(req_path, "/") == 0) {
    snprintf(file_path, sizeof(file_path), "%s%s", root,
             TEMP_DEFAULT_PAGE);
  } else {
    snprintf(file_path, sizeof(file_path), "%s%s", root, req_path);
  }

  struct stat file_stat;
  int status_code = 200;

  if (stat(file_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
    status_code = 404;
    snprintf(file_path, sizeof(file_path), "%s%s", root,
             TEMP_NOT_FOUND_PAGE);
    stat(file_path, &file_stat);
  }

  const char *mime_type = get_mime_type(file_path);
  char header[BUFFER_SIZE];
  snprintf(header, sizeof(header),
           "HTTP/1.1 %d OK\r\nContent-Type: %s\r\nContent-Length: "
           "%ld\r\nConnection: close\r\n\r\n",
           status_code, mime_type, file_stat.st_size);

  SSL_write(ssl, header, strlen(header));

  int file_fd = open(file_path, O_RDONLY);
  if (file_fd >= 0) {
    // This is not ideal for SSL, as sendfile doesn't encrypt.
    // A better approach would be to read from file and SSL_write in a loop.
    // For now, this is a placeholder.
    char file_buffer[BUFFER_SIZE];
    ssize_t bytes;
    while((bytes = read(file_fd, file_buffer, BUFFER_SIZE)) > 0) {
        SSL_write(ssl, file_buffer, bytes);
    }
    close(file_fd);
  } else {
    log_message(LOG_LEVEL_ERROR, "Could not open requested file for HTTPS.");
    // Send error response body
    const char *error_body = "Internal Server Error";
    SSL_write(ssl, error_body, strlen(error_body));
  }

  free(host);
  free(buffer_copy);
} 