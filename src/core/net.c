#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/time.h>

#include "config.h"
#include "core.h"
#include "http.h"
#include "https.h"
#include "log.h"
#include "../utils/asm/asm_opt.h"
#include "../utils/asm/asm_mempool.h"

#define MAX_EVENTS 256  // 增加事件处理数量
#define MAX_ACCEPT_PER_ROUND 32  // 每轮最多接受的连接数
#define EPOLL_TIMEOUT_MS 1  // 1ms超时，减少阻塞

// Connection state structure
typedef struct connection_t {
    int fd;
    char client_ip[INET_ADDRSTRLEN];
    time_t last_activity;
    size_t buffer_size;
    char buffer[8192];
    int is_https;
    SSL* ssl;
} connection_t;

// 优化的连接池
static connection_t* connection_pool = NULL;
static int connection_pool_size = 0;
static int connection_pool_capacity = 0;

// 前向声明
static int make_socket_non_blocking(int fd);
void error_and_exit(const char *msg);

// 初始化连接池
static int init_connection_pool(int capacity) {
    connection_pool_capacity = capacity;
    connection_pool = calloc(capacity, sizeof(connection_t));
    if (!connection_pool) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate connection pool");
    return -1;
  }

    // 初始化连接池
    for (int i = 0; i < capacity; i++) {
        connection_pool[i].fd = -1;
        connection_pool[i].ssl = NULL;
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Connection pool initialized with capacity %d", capacity);
    log_message(LOG_LEVEL_INFO, log_msg);
  return 0;
}

// 获取空闲连接槽
static connection_t* get_free_connection(void) {
    for (int i = 0; i < connection_pool_capacity; i++) {
        if (connection_pool[i].fd == -1) {
            return &connection_pool[i];
        }
    }
    return NULL;
}

// 释放连接槽
static void free_connection(connection_t* conn) {
    if (conn) {
        if (conn->ssl) {
            SSL_free(conn->ssl);
            conn->ssl = NULL;
        }
        conn->fd = -1;
        conn->is_https = 0;
        connection_pool_size--;
    }
}

// 优化的socket创建
int create_server_socket(int port) {
  int server_fd;
  struct sockaddr_in server_addr;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    error_and_exit("socket failed");
  }

    // 设置socket选项
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        error_and_exit("setsockopt SO_REUSEADDR failed");
    }
    
    // 设置TCP_NODELAY减少延迟
    if (setsockopt(server_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt))) {
        log_message(LOG_LEVEL_WARNING, "Failed to set TCP_NODELAY");
    }
    
    // 设置SO_KEEPALIVE
    if (setsockopt(server_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt))) {
        log_message(LOG_LEVEL_WARNING, "Failed to set SO_KEEPALIVE");
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    char msg[100];
    snprintf(msg, sizeof(msg), "bind to port %d failed", port);
    error_and_exit(msg);
  }

    if (listen(server_fd, 1024) < 0) {  // 增加backlog
    error_and_exit("listen failed");
  }

  if (make_socket_non_blocking(server_fd) == -1) {
    error_and_exit("make_socket_non_blocking failed");
  }

  return server_fd;
}

// 优化的批量连接接受
int accept_connections_batch(int epoll_fd, int server_fd, int is_https, 
                           struct epoll_event* events, int* event_count,
                           SSL_CTX* ssl_ctx, core_config_t* core_config) {
    (void)core_config; // 未使用的参数
    int accepted = 0;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // 批量接受连接
    for (int i = 0; i < MAX_ACCEPT_PER_ROUND && accepted < MAX_EVENTS - *event_count; i++) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // 没有更多连接
            }
            log_message(LOG_LEVEL_ERROR, "accept failed");
            continue;
        }
        
        // 获取空闲连接槽
        connection_t* conn = get_free_connection();
        if (!conn) {
            log_message(LOG_LEVEL_WARNING, "Connection pool full, closing connection");
            close(client_fd);
            continue;
        }
        
        // 设置连接信息
        conn->fd = client_fd;
        conn->is_https = is_https;
        conn->last_activity = time(NULL);
        conn->buffer_size = 0;
        
        inet_ntop(AF_INET, &client_addr.sin_addr, conn->client_ip, INET_ADDRSTRLEN);
        
        // 设置客户端socket为非阻塞
        if (make_socket_non_blocking(client_fd) == -1) {
            log_message(LOG_LEVEL_ERROR, "Failed to set client socket non-blocking");
            free_connection(conn);
            close(client_fd);
            continue;
        }
        
        // 处理HTTPS连接
        if (is_https && ssl_ctx) {
            conn->ssl = SSL_new(ssl_ctx);
            if (!conn->ssl) {
                log_message(LOG_LEVEL_ERROR, "Failed to create SSL context");
                free_connection(conn);
                close(client_fd);
                continue;
            }
            
            SSL_set_fd(conn->ssl, client_fd);
            
            // 执行SSL握手
            int ssl_result = SSL_accept(conn->ssl);
            if (ssl_result <= 0) {
                int ssl_error = SSL_get_error(conn->ssl, ssl_result);
                if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
                    // 需要更多数据，继续等待
                    log_message(LOG_LEVEL_DEBUG, "SSL handshake in progress");
                } else {
                    log_message(LOG_LEVEL_ERROR, "SSL handshake failed");
                    free_connection(conn);
                    close(client_fd);
                    continue;
                }
            }
        }
        
        // 添加到epoll事件
        struct epoll_event event;
        event.data.ptr = conn;
        event.events = EPOLLIN | EPOLLET;  // 使用边缘触发
        
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
            log_message(LOG_LEVEL_ERROR, "Failed to add client fd to epoll");
            free_connection(conn);
            close(client_fd);
            continue;
        }
        
        connection_pool_size++;
        accepted++;
    }
    
    return accepted;
}

// 优化的HTTP请求处理
static int handle_http_request_optimized(connection_t* conn, core_config_t* core_config) {
    if (!conn || conn->fd == -1) return -1;
    
    // 读取请求数据
    ssize_t bytes_read = 0;
    if (conn->is_https && conn->ssl) {
        bytes_read = SSL_read(conn->ssl, conn->buffer + conn->buffer_size, 
                             sizeof(conn->buffer) - conn->buffer_size - 1);
    } else {
        bytes_read = recv(conn->fd, conn->buffer + conn->buffer_size, 
                         sizeof(conn->buffer) - conn->buffer_size - 1, 0);
    }
    
    if (bytes_read <= 0) {
        // 连接关闭或错误
        free_connection(conn);
        return -1;
    }
    
    conn->buffer_size += bytes_read;
    conn->buffer[conn->buffer_size] = '\0';
    conn->last_activity = time(NULL);
    
    // 检查是否收到完整的HTTP请求
    char* header_end = asm_opt_strstr(conn->buffer, "\r\n\r\n");
    if (!header_end) {
        // 请求不完整，继续等待
        return 0;
    }
    
    // 处理HTTP请求
    if (conn->is_https && conn->ssl) {
        handle_https_request(conn->ssl, conn->client_ip, core_config);
    } else {
        handle_http_request(conn->fd, conn->client_ip, core_config);
    }
    
    // 检查是否应该保持连接 (HTTP/1.1 默认保持连接，除非指定 Connection: close)
    int keep_alive = 1;  // 默认保持连接
    char* connection_hdr = strcasestr(conn->buffer, "Connection:");
    if (connection_hdr) {
        char* end = asm_opt_strstr(connection_hdr, "\r\n");
        if (end) {
            *end = '\0';
            if (strcasestr(connection_hdr, "close")) {
                keep_alive = 0;
            }
            *end = '\r';
        }
    }
    
    if (keep_alive) {
        // 重置连接状态以处理下一个请求
        conn->buffer_size = 0;
        memset(conn->buffer, 0, sizeof(conn->buffer));
        return 0;
    } else {
        // 关闭连接
        free_connection(conn);
        return -1;
    }
}

// 优化的worker循环
void worker_loop(int server_fd, int https_server_fd, core_config_t *core_config, SSL_CTX *ssl_ctx) {
  int epoll_fd;
  struct epoll_event event;

  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) error_and_exit("epoll_create1 (worker)");

    // 初始化连接池
    if (init_connection_pool(1000) < 0) {
        close(epoll_fd);
        return;
    }

    // 添加服务器socket到epoll
  if (server_fd != -1) {
    event.data.fd = server_fd;
        event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        log_message(LOG_LEVEL_ERROR, "Failed to add http_fd to epoll");
        close(epoll_fd);
        return;
    }
  }
    
  if (https_server_fd != -1) {
    event.data.fd = https_server_fd;
        event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, https_server_fd, &event) == -1) {
        log_message(LOG_LEVEL_ERROR, "Failed to add https_fd to epoll");
        close(epoll_fd);
        return;
    }
  }

  struct epoll_event events[MAX_EVENTS];

    log_message(LOG_LEVEL_INFO, "Optimized worker process started.");

  while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);
        
    for (int i = 0; i < n; i++) {
      if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                // 处理错误连接
                if (events[i].data.ptr) {
                    connection_t* conn = (connection_t*)events[i].data.ptr;
                    free_connection(conn);
                } else {
        close(events[i].data.fd);
                }
        continue;
      }

            // 检查是否是服务器socket（新连接）
            if (events[i].data.fd == server_fd || events[i].data.fd == https_server_fd) {
                int is_https = (events[i].data.fd == https_server_fd);
                accept_connections_batch(epoll_fd, events[i].data.fd, is_https, events, &i, ssl_ctx, core_config);
            } else {
                // 处理客户端连接
                connection_t* conn = (connection_t*)events[i].data.ptr;
                if (conn) {
                    int result = handle_http_request_optimized(conn, core_config);
                    if (result == -1) {
                        // 如果处理失败，从epoll中移除并关闭连接
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL) == -1) {
                            log_message(LOG_LEVEL_ERROR, "Failed to remove client fd from epoll");
                        }
                        free_connection(conn);
                    }
                }
            }
        }
        
        // 定期清理超时连接
        static time_t last_cleanup = 0;
        time_t current_time = time(NULL);
        if (current_time - last_cleanup > 30) {  // 每30秒清理一次
            for (int i = 0; i < connection_pool_capacity; i++) {
                if (connection_pool[i].fd != -1 && 
                    current_time - connection_pool[i].last_activity > 300) {  // 5分钟超时
                    free_connection(&connection_pool[i]);
                }
            }
            last_cleanup = current_time;
        }
    }
    
    // 清理资源
    if (connection_pool) {
        for (int i = 0; i < connection_pool_capacity; i++) {
            if (connection_pool[i].fd != -1) {
                close(connection_pool[i].fd);
                if (connection_pool[i].ssl) {
                    SSL_free(connection_pool[i].ssl);
                }
            }
        }
        free(connection_pool);
    }
    
    close(epoll_fd);
}

// 其他辅助函数保持不变
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

void error_and_exit(const char *msg) {
    perror(msg);
    _exit(1);
} 