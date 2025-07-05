#include "http.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "config.h"
#include "log.h"
#include "util.h"

#define BUFFER_SIZE 4096
#define TEMP_DEFAULT_PAGE "/index.html"
#define TEMP_NOT_FOUND_PAGE "/404.html"

void handle_http_request(int client_socket, const char *client_ip) {
  char buffer[BUFFER_SIZE] = {0};
  ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);

  if (bytes_read <= 0) {
    if (bytes_read == -1 && errno != EAGAIN) {
      log_message(LOG_LEVEL_ERROR, "read error on http socket");
    }
    close(client_socket);
    return;
  }

  char *method = strtok(buffer, " \t\r\n");
  char *req_path = strtok(NULL, " \t\r\n");

  if (!method || !req_path) {
    log_message(LOG_LEVEL_ERROR, "Malformed http request, closing connection.");
    close(client_socket);
    return;
  }

  char log_msg[BUFFER_SIZE];
  snprintf(log_msg, sizeof(log_msg), "HTTP Request from %s: %s %s", client_ip,
           method, req_path);
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

  char file_path[BUFFER_SIZE];
  if (strcmp(req_path, "/") == 0) {
    snprintf(file_path, sizeof(file_path), "%s%s", g_config->http->directives[1].value,
             TEMP_DEFAULT_PAGE);
  } else {
    snprintf(file_path, sizeof(file_path), "%s%s", g_config->http->directives[1].value, req_path);
  }

  struct stat file_stat;
  int file_fd = -1;
  int status_code = 200;

  if (stat(file_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
    status_code = 404;
    snprintf(file_path, sizeof(file_path), "%s%s", g_config->http->directives[1].value,
             TEMP_NOT_FOUND_PAGE);
    stat(file_path, &file_stat);  // Get stats for the 404 page
    snprintf(log_msg, sizeof(log_msg), "File not found: %s. Responding with 404.",
             req_path);
    log_message(LOG_LEVEL_INFO, log_msg);
  }

  file_fd = open(file_path, O_RDONLY);
  if (file_fd < 0) {
    // If even the 404 page can't be opened, something is very wrong.
    log_message(LOG_LEVEL_ERROR, "Could not open 404 page, closing connection.");
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
  close(client_socket);
} 