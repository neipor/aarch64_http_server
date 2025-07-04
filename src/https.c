#include "https.h"

#include <errno.h>
#include <fcntl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "util.h"

#define BUFFER_SIZE 4096

void handle_https_request(SSL *ssl, const char *client_ip) {
  char buffer[BUFFER_SIZE] = {0};
  int client_socket = SSL_get_fd(ssl);

  // Perform SSL handshake
  if (SSL_accept(ssl) <= 0) {
    ERR_print_errors_fp(stderr);
    log_message(LOG_LEVEL_ERROR, "SSL handshake failed.");
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    return;
  }

  ssize_t bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1);

  if (bytes_read <= 0) {
    int ssl_error = SSL_get_error(ssl, bytes_read);
    if (ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE) {
      log_message(LOG_LEVEL_ERROR, "SSL_read error");
      ERR_print_errors_fp(stderr);
    }
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    return;
  }

  char *method = strtok(buffer, " \t\r\n");
  char *req_path = strtok(NULL, " \t\r\n");

  if (!method || !req_path) {
    log_message(LOG_LEVEL_ERROR, "Malformed request, closing connection.");
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    return;
  }

  char log_msg[BUFFER_SIZE];
  snprintf(log_msg, sizeof(log_msg), "HTTPS Request from %s: %s %s", client_ip,
           method, req_path);
  log_message(LOG_LEVEL_INFO, log_msg);

  // Security: prevent directory traversal
  if (strstr(req_path, "..")) {
    // For simplicity, just close connection on malicious-looking paths
    snprintf(log_msg, sizeof(log_msg),
             "Directory traversal attempt from %s blocked.", client_ip);
    log_message(LOG_LEVEL_ERROR, log_msg);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    return;
  }

  char file_path[BUFFER_SIZE];
  if (strcmp(req_path, "/") == 0) {
    snprintf(file_path, sizeof(file_path), "%s%s", config.web_root,
             DEFAULT_PAGE);
  } else {
    snprintf(file_path, sizeof(file_path), "%s%s", config.web_root, req_path);
  }

  struct stat file_stat;
  int file_fd = -1;
  int status_code = 200;

  if (stat(file_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
    status_code = 404;
    snprintf(file_path, sizeof(file_path), "%s%s", config.web_root,
             NOT_FOUND_PAGE);
    stat(file_path, &file_stat);  // Get stats for the 404 page
    snprintf(log_msg, sizeof(log_msg), "File not found: %s. Responding with 404.",
             req_path);
    log_message(LOG_LEVEL_INFO, log_msg);
  }

  file_fd = open(file_path, O_RDONLY);
  if (file_fd < 0) {
    // If even the 404 page can't be opened, something is very wrong.
    log_message(LOG_LEVEL_ERROR, "Could not open 404 page, closing connection.");
    SSL_shutdown(ssl);
    SSL_free(ssl);
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

  SSL_write(ssl, header, strlen(header));

  // sendfile doesn't work with SSL sockets directly, so we need to read and
  // write.
  char file_buffer[BUFFER_SIZE];
  ssize_t bytes_sent;
  while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
    bytes_sent = SSL_write(ssl, file_buffer, bytes_read);
    if (bytes_sent <= 0) {
      int ssl_error = SSL_get_error(ssl, bytes_sent);
      if (ssl_error != SSL_ERROR_WANT_WRITE) {
        log_message(LOG_LEVEL_ERROR,
                    "SSL_write error during sendfile emulation.");
        ERR_print_errors_fp(stderr);
        break;
      }
    }
  }

  close(file_fd);
  SSL_shutdown(ssl);
  SSL_free(ssl);
  close(client_socket);
} 