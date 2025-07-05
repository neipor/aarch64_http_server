#include "http.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

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

void handle_http_request(int client_socket, const char *client_ip, core_config_t *core_conf) {
  char buffer[BUFFER_SIZE];
  ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);

  if (bytes_read <= 0) {
    close(client_socket);
    return;
  }
  buffer[bytes_read] = '\0';

  char *buffer_copy = strdup(buffer);
  char *method = strtok(buffer_copy, " ");
  char *req_path = strtok(NULL, " ");
  char *http_version = strtok(NULL, "\r\n");
  char *host = get_host(buffer);

  if (!method || !req_path) {
    close(client_socket);
    return;
  }

  char log_msg[BUFFER_SIZE];
  snprintf(log_msg, sizeof(log_msg), "\"%s %s %s\" from %s (Host: %s)", method,
           req_path, http_version, client_ip, host ? host : "none");
  log_message(LOG_LEVEL_INFO, log_msg);

  // Security: prevent directory traversal
  if (strstr(req_path, "..")) {
    // For simplicity, just close connection on malicious-looking paths
    snprintf(log_msg, sizeof(log_msg),
             "Directory traversal attempt from %s blocked.", client_ip);
    log_message(LOG_LEVEL_ERROR, log_msg);
    close(client_socket);
    return;
  }

  // --- Routing ---
  route_t route = find_route(core_conf, host, req_path);
  if (!route.server) {
      // Send a 500 internal server error
      const char *response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
      write(client_socket, response, strlen(response));
      free(host);
      free(buffer_copy);
      return;
  }

  // Determine the root directory from the matched route
  const char *root = get_directive_value("root", route.server->directives, route.server->directive_count);
  if (route.location) {
      const char* loc_root = get_directive_value("root", route.location->directives, route.location->directive_count);
      if (loc_root) root = loc_root; // Location root overrides server root
  }
  if (!root) {
      // Fallback to a default if no root is specified anywhere
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
  int file_fd = -1;
  int status_code = 200;

  if (stat(file_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
    status_code = 404;
    snprintf(file_path, sizeof(file_path), "%s%s", root,
             TEMP_NOT_FOUND_PAGE);
    stat(file_path, &file_stat);  // Get stats for the 404 page
    snprintf(log_msg, sizeof(log_msg), "File not found: %s. Responding with 404.",
             req_path);
    log_message(LOG_LEVEL_INFO, log_msg);
  }

  file_fd = open(file_path, O_RDONLY);
  if (file_fd < 0) {
    log_message(LOG_LEVEL_ERROR, "Could not open requested file.");
    // Send a 500 error response instead of just closing
    const char *response = "HTTP/1.1 500 Internal Server Error\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: 21\r\n"
                          "Connection: close\r\n\r\n"
                          "Internal Server Error";
    write(client_socket, response, strlen(response));
    free(host);
    free(buffer_copy);
    close(client_socket);
    return;
  }

  const char *mime_type = get_mime_type(file_path);
  char header[BUFFER_SIZE];
  snprintf(header, sizeof(header),
           "HTTP/1.1 %d %s\r\n"
           "Content-Type: %s\r\n"
           "Content-Length: %ld\r\n"
           "Server: ANX-Static/0.5.0\r\n"
           "Connection: close\r\n\r\n",
           status_code, (status_code == 200) ? "OK" : "Not Found", mime_type,
           file_stat.st_size);

  write(client_socket, header, strlen(header));
  sendfile(client_socket, file_fd, NULL, file_stat.st_size);
  close(file_fd);
  // Clean up
  free(host);
  free(buffer_copy);
  close(client_socket);
} 