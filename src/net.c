#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "http.h"
#include "https.h"
#include "log.h"

#define MAX_EVENTS 64

// Forward declaration for static helper function
static int make_socket_non_blocking(int fd);
void error_and_exit(const char *msg);  // This should probably be in a more general place

// This is a temporary solution. Ideally, this should be in a shared utility module.
void error_and_exit(const char *msg) {
  perror(msg);
  _exit(1); // Use _exit in worker processes to avoid calling exit handlers
}


// Helper function to set a socket to non-blocking mode
static int make_socket_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL");
    return -1;
  }

  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1) {
    perror("fcntl F_SETFL");
    return -1;
  }
  return 0;
}

int create_server_socket(int port) {
  int server_fd;
  struct sockaddr_in server_addr;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    error_and_exit("socket failed");
  }

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    error_and_exit("setsockopt failed");
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    char msg[100];
    snprintf(msg, sizeof(msg), "bind to port %d failed", port);
    error_and_exit(msg);
  }

  if (listen(server_fd, 128) < 0) {
    error_and_exit("listen failed");
  }

  if (make_socket_non_blocking(server_fd) == -1) {
    error_and_exit("make_socket_non_blocking failed");
  }

  return server_fd;
}


void worker_loop(int server_fd, int https_server_fd, SSL_CTX *ssl_ctx) {
  int epoll_fd;
  struct epoll_event event;

  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) error_and_exit("epoll_create1 (worker)");

  if (server_fd != -1) {
    event.data.fd = server_fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        log_message(LOG_LEVEL_ERROR, "Failed to add http_fd to epoll");
        close(epoll_fd);
        return;
    }
  }
  if (https_server_fd != -1) {
    event.data.fd = https_server_fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, https_server_fd, &event) == -1) {
        log_message(LOG_LEVEL_ERROR, "Failed to add https_fd to epoll");
        close(epoll_fd);
        return;
    }
  }

  struct epoll_event events[MAX_EVENTS];
  char client_ip[INET_ADDRSTRLEN];

  log_message(LOG_LEVEL_INFO, "Worker process started.");

  while (1) {
    int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    for (int i = 0; i < n; i++) {
      if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg), "epoll error on fd %d",
                 events[i].data.fd);
        log_message(LOG_LEVEL_ERROR, log_msg);
        close(events[i].data.fd);
        continue;
      }

      int current_server_fd = events[i].data.fd;
      int is_https = (current_server_fd == https_server_fd);

      while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd =
            accept(current_server_fd, (struct sockaddr *)&client_addr,
                   &client_len);
        if (client_fd == -1) {
          if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
            log_message(LOG_LEVEL_ERROR, "accept failed");
          }
          break;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        if (is_https) {
          SSL *ssl = SSL_new(ssl_ctx);
          SSL_set_fd(ssl, client_fd);
          handle_https_request(ssl, client_ip);
        } else {
          handle_http_request(client_fd, client_ip);
        }
      }
    }
  }
} 