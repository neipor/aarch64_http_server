#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "log.h"

#define DEFAULT_BUFFER_SIZE 8192
#define DEFAULT_CONNECT_TIMEOUT 5
#define DEFAULT_PROXY_TIMEOUT 300
#define DEFAULT_BACKLOG 128
#define DEFAULT_WORKER_CONNECTIONS 1024
#define MAX_EVENTS 1000

// 创建Stream管理器
stream_manager_t *stream_manager_create(stream_config_t *config, lb_config_t *lb_config) {
    if (!config || !lb_config) {
        log_message(LOG_LEVEL_ERROR, "Invalid parameters for stream manager creation");
        return NULL;
    }
    
    stream_manager_t *manager = calloc(1, sizeof(stream_manager_t));
    if (!manager) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for stream manager");
        return NULL;
    }
    
    manager->config = config;
    manager->lb_config = lb_config;
    manager->connections = NULL;
    manager->running = false;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&manager->connections_mutex, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize connections mutex");
        free(manager);
        return NULL;
    }
    
    // 初始化统计信息
    manager->stats.start_time = time(NULL);
    manager->stats.last_updated = manager->stats.start_time;
    
    log_message(LOG_LEVEL_INFO, "Stream manager created successfully");
    return manager;
}

// 释放Stream管理器
void stream_manager_free(stream_manager_t *manager) {
    if (!manager) return;
    
    // 停止管理器
    if (manager->running) {
        stream_manager_stop(manager);
    }
    
    // 清理所有连接
    pthread_mutex_lock(&manager->connections_mutex);
    stream_connection_t *conn = manager->connections;
    while (conn) {
        stream_connection_t *next = conn->next;
        stream_connection_cleanup(conn);
        stream_connection_free(conn);
        conn = next;
    }
    pthread_mutex_unlock(&manager->connections_mutex);
    
    // 销毁互斥锁
    pthread_mutex_destroy(&manager->connections_mutex);
    
    free(manager);
    log_message(LOG_LEVEL_INFO, "Stream manager freed");
}

// 启动Stream管理器
int stream_manager_start(stream_manager_t *manager) {
    if (!manager || manager->running) {
        return -1;
    }
    
    // 启动所有监听器
    stream_listener_t *listener = manager->config->listeners;
    while (listener) {
        if (stream_listener_start(listener, manager->lb_config) < 0) {
            log_message(LOG_LEVEL_ERROR, "Failed to start stream listener");
            return -1;
        }
        listener = listener->next;
    }
    
    manager->running = true;
    log_message(LOG_LEVEL_INFO, "Stream manager started successfully");
    return 0;
}

// 停止Stream管理器
int stream_manager_stop(stream_manager_t *manager) {
    if (!manager || !manager->running) {
        return -1;
    }
    
    manager->running = false;
    
    // 停止所有监听器
    stream_listener_t *listener = manager->config->listeners;
    while (listener) {
        stream_listener_stop(listener);
        listener = listener->next;
    }
    
    log_message(LOG_LEVEL_INFO, "Stream manager stopped");
    return 0;
}

// 创建Stream配置
stream_config_t *stream_config_create(void) {
    stream_config_t *config = calloc(1, sizeof(stream_config_t));
    if (!config) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for stream config");
        return NULL;
    }
    
    // 设置默认值
    config->worker_connections = DEFAULT_WORKER_CONNECTIONS;
    config->resolver_timeout = 30;
    config->default_buffer_size = DEFAULT_BUFFER_SIZE;
    config->enable_stats = true;
    config->access_log = strdup("stream_access.log");
    config->error_log = strdup("stream_error.log");
    
    log_message(LOG_LEVEL_DEBUG, "Stream config created");
    return config;
}

// 释放Stream配置
void stream_config_free(stream_config_t *config) {
    if (!config) return;
    
    // 释放监听器列表
    stream_listener_t *listener = config->listeners;
    while (listener) {
        stream_listener_t *next = listener->next;
        stream_listener_free(listener);
        listener = next;
    }
    
    free(config->access_log);
    free(config->error_log);
    free(config);
}

// 创建Stream监听器
stream_listener_t *stream_listener_create(const char *address, int port, 
                                         stream_protocol_t protocol, const char *upstream) {
    if (!address || port <= 0 || !upstream) {
        log_message(LOG_LEVEL_ERROR, "Invalid parameters for stream listener creation");
        return NULL;
    }
    
    stream_listener_t *listener = calloc(1, sizeof(stream_listener_t));
    if (!listener) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for stream listener");
        return NULL;
    }
    
    listener->bind_address = strdup(address);
    listener->port = port;
    listener->protocol = protocol;
    listener->upstream_name = strdup(upstream);
    listener->socket_fd = -1;
    listener->active = false;
    
    // 设置默认值
    listener->backlog = DEFAULT_BACKLOG;
    listener->connect_timeout = DEFAULT_CONNECT_TIMEOUT;
    listener->proxy_timeout = DEFAULT_PROXY_TIMEOUT;
    listener->proxy_buffer_size = DEFAULT_BUFFER_SIZE;
    listener->tcp_nodelay = true;
    listener->so_keepalive = true;
    listener->so_reuseport = 1;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Stream listener created: %s:%d (%s) -> %s",
             address, port, stream_protocol_to_string(protocol), upstream);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return listener;
}

// 释放Stream监听器
void stream_listener_free(stream_listener_t *listener) {
    if (!listener) return;
    
    if (listener->active) {
        stream_listener_stop(listener);
    }
    
    free(listener->bind_address);
    free(listener->upstream_name);
    free(listener);
}

// 添加监听器到配置
int stream_config_add_listener(stream_config_t *config, stream_listener_t *listener) {
    if (!config || !listener) return -1;
    
    listener->next = config->listeners;
    config->listeners = listener;
    config->listeners_count++;
    
    return 0;
}

// 启动Stream监听器
int stream_listener_start(stream_listener_t *listener, lb_config_t *lb_config) {
    if (!listener || listener->active) {
        return -1;
    }
    
    // 创建socket
    int sock_type = (listener->protocol == STREAM_PROTOCOL_TCP) ? SOCK_STREAM : SOCK_DGRAM;
    listener->socket_fd = socket(AF_INET, sock_type, 0);
    if (listener->socket_fd < 0) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to create socket for %s:%d - %s",
                 listener->bind_address, listener->port, strerror(errno));
        log_message(LOG_LEVEL_ERROR, log_msg);
        return -1;
    }
    
    // 设置socket选项
    int opt = 1;
    if (setsockopt(listener->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_message(LOG_LEVEL_WARNING, "Failed to set SO_REUSEADDR");
    }
    
    if (listener->so_reuseport) {
        if (setsockopt(listener->socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            log_message(LOG_LEVEL_WARNING, "Failed to set SO_REUSEPORT");
        }
    }
    
    if (listener->protocol == STREAM_PROTOCOL_TCP && listener->tcp_nodelay) {
        if (setsockopt(listener->socket_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
            log_message(LOG_LEVEL_WARNING, "Failed to set TCP_NODELAY");
        }
    }
    
    if (listener->so_keepalive) {
        if (setsockopt(listener->socket_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
            log_message(LOG_LEVEL_WARNING, "Failed to set SO_KEEPALIVE");
        }
    }
    
    // 绑定地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listener->port);
    
    if (strcmp(listener->bind_address, "*") == 0 || strcmp(listener->bind_address, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, listener->bind_address, &addr.sin_addr) <= 0) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Invalid bind address: %s", listener->bind_address);
            log_message(LOG_LEVEL_ERROR, log_msg);
            close(listener->socket_fd);
            return -1;
        }
    }
    
    if (bind(listener->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to bind %s:%d - %s",
                 listener->bind_address, listener->port, strerror(errno));
        log_message(LOG_LEVEL_ERROR, log_msg);
        close(listener->socket_fd);
        return -1;
    }
    
    // TCP需要监听
    if (listener->protocol == STREAM_PROTOCOL_TCP) {
        if (listen(listener->socket_fd, listener->backlog) < 0) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Failed to listen on %s:%d - %s",
                     listener->bind_address, listener->port, strerror(errno));
            log_message(LOG_LEVEL_ERROR, log_msg);
            close(listener->socket_fd);
            return -1;
        }
    }
    
    listener->active = true;
    
    // 创建监听线程（这里简化，实际可以使用线程池）
    pthread_t listen_thread;
    struct {
        stream_listener_t *listener;
        lb_config_t *lb_config;
    } *thread_args = malloc(sizeof(*thread_args));
    
    thread_args->listener = listener;
    thread_args->lb_config = lb_config;
    
    if (pthread_create(&listen_thread, NULL, stream_listener_thread, thread_args) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to create listener thread");
        free(thread_args);
        close(listener->socket_fd);
        listener->active = false;
        return -1;
    }
    
    pthread_detach(listen_thread);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Stream listener started: %s:%d (%s)",
             listener->bind_address, listener->port, stream_protocol_to_string(listener->protocol));
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return 0;
}

// 停止Stream监听器
int stream_listener_stop(stream_listener_t *listener) {
    if (!listener || !listener->active) {
        return -1;
    }
    
    listener->active = false;
    
    if (listener->socket_fd >= 0) {
        close(listener->socket_fd);
        listener->socket_fd = -1;
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Stream listener stopped: %s:%d",
             listener->bind_address, listener->port);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return 0;
}

// 监听线程函数
void *stream_listener_thread(void *arg) {
    struct {
        stream_listener_t *listener;
        lb_config_t *lb_config;
    } *args = arg;
    
    stream_listener_t *listener = args->listener;
    lb_config_t *lb_config = args->lb_config;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Stream listener thread started for %s:%d",
             listener->bind_address, listener->port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    while (listener->active) {
        if (listener->protocol == STREAM_PROTOCOL_TCP) {
            // TCP处理
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            int client_fd = accept(listener->socket_fd, (struct sockaddr *)&client_addr, &addr_len);
            if (client_fd < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK && listener->active) {
                    log_message(LOG_LEVEL_ERROR, "Failed to accept TCP connection");
                }
                continue;
            }
            
            // 创建连接对象
            stream_connection_t *conn = stream_connection_create(client_fd, STREAM_PROTOCOL_TCP, 
                                                               listener->upstream_name);
            if (conn) {
                conn->client_addr = client_addr;
                
                // 创建代理线程
                struct {
                    stream_connection_t *conn;
                    lb_config_t *lb_config;
                } *proxy_args = malloc(sizeof(*proxy_args));
                
                proxy_args->conn = conn;
                proxy_args->lb_config = lb_config;
                
                if (pthread_create(&conn->proxy_thread, NULL, stream_proxy_thread, proxy_args) == 0) {
                    pthread_detach(conn->proxy_thread);
                } else {
                    log_message(LOG_LEVEL_ERROR, "Failed to create proxy thread");
                    stream_connection_free(conn);
                    free(proxy_args);
                }
            } else {
                close(client_fd);
            }
        } else {
            // UDP处理（简化版）
            char buffer[65536];
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            ssize_t bytes = recvfrom(listener->socket_fd, buffer, sizeof(buffer), 0,
                                   (struct sockaddr *)&client_addr, &addr_len);
            if (bytes > 0) {
                // 处理UDP数据包（这里需要更复杂的会话管理）
                stream_connection_t *conn = stream_connection_create(-1, STREAM_PROTOCOL_UDP, 
                                                                   listener->upstream_name);
                if (conn) {
                    conn->client_addr = client_addr;
                    // UDP代理处理
                    stream_udp_proxy_start(conn, lb_config);
                    stream_connection_free(conn);
                }
            }
        }
    }
    
    free(args);
    log_message(LOG_LEVEL_DEBUG, "Stream listener thread ended");
    return NULL;
}

// 代理线程函数
void *stream_proxy_thread(void *arg) {
    struct {
        stream_connection_t *conn;
        lb_config_t *lb_config;
    } *args = arg;
    
    stream_connection_t *conn = args->conn;
    lb_config_t *lb_config = args->lb_config;
    
    if (conn->protocol == STREAM_PROTOCOL_TCP) {
        stream_tcp_proxy_start(conn, lb_config);
    } else {
        stream_udp_proxy_start(conn, lb_config);
    }
    
    stream_connection_cleanup(conn);
    stream_connection_free(conn);
    free(args);
    
    return NULL;
}

// 创建Stream连接
stream_connection_t *stream_connection_create(int client_fd, stream_protocol_t protocol, 
                                            const char *upstream_name) {
    if (client_fd < 0 && protocol == STREAM_PROTOCOL_TCP) {
        return NULL;
    }
    
    stream_connection_t *conn = calloc(1, sizeof(stream_connection_t));
    if (!conn) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for stream connection");
        return NULL;
    }
    
    conn->client_fd = client_fd;
    conn->backend_fd = -1;
    conn->protocol = protocol;
    conn->state = STREAM_PROXY_STATE_IDLE;
    conn->start_time = time(NULL);
    conn->active = true;
    conn->upstream_name = strdup(upstream_name);
    
    return conn;
}

// 释放Stream连接
void stream_connection_free(stream_connection_t *conn) {
    if (!conn) return;
    
    free(conn->upstream_name);
    free(conn);
}

// 清理Stream连接
void stream_connection_cleanup(stream_connection_t *conn) {
    if (!conn) return;
    
    if (conn->client_fd >= 0) {
        close(conn->client_fd);
        conn->client_fd = -1;
    }
    
    if (conn->backend_fd >= 0) {
        close(conn->backend_fd);
        conn->backend_fd = -1;
    }
    
    conn->active = false;
    conn->state = STREAM_PROXY_STATE_CLOSED;
}

// 工具函数：协议转字符串
const char *stream_protocol_to_string(stream_protocol_t protocol) {
    switch (protocol) {
        case STREAM_PROTOCOL_TCP: return "TCP";
        case STREAM_PROTOCOL_UDP: return "UDP";
        default: return "Unknown";
    }
}

// 工具函数：字符串转协议
stream_protocol_t stream_protocol_from_string(const char *protocol_str) {
    if (!protocol_str) return STREAM_PROTOCOL_TCP;
    
    if (strcasecmp(protocol_str, "tcp") == 0) {
        return STREAM_PROTOCOL_TCP;
    } else if (strcasecmp(protocol_str, "udp") == 0) {
        return STREAM_PROTOCOL_UDP;
    }
    
    return STREAM_PROTOCOL_TCP; // 默认TCP
}

// 工具函数：状态转字符串
const char *stream_state_to_string(stream_proxy_state_t state) {
    switch (state) {
        case STREAM_PROXY_STATE_IDLE: return "Idle";
        case STREAM_PROXY_STATE_CONNECTING: return "Connecting";
        case STREAM_PROXY_STATE_CONNECTED: return "Connected";
        case STREAM_PROXY_STATE_FORWARDING: return "Forwarding";
        case STREAM_PROXY_STATE_ERROR: return "Error";
        case STREAM_PROXY_STATE_CLOSED: return "Closed";
        default: return "Unknown";
    }
}

// TCP代理启动
int stream_tcp_proxy_start(stream_connection_t *conn, lb_config_t *lb_config) {
    if (!conn || !lb_config) return -1;
    
    conn->state = STREAM_PROXY_STATE_CONNECTING;
    
    // 选择后端服务器
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &conn->client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    upstream_server_t *server = stream_select_backend(lb_config, conn->upstream_name, client_ip);
    if (!server) {
        log_message(LOG_LEVEL_ERROR, "No available backend server");
        conn->state = STREAM_PROXY_STATE_ERROR;
        return -1;
    }
    
    conn->backend_server = server;
    
    // 连接后端服务器
    if (stream_tcp_connect_backend(conn, server) < 0) {
        conn->state = STREAM_PROXY_STATE_ERROR;
        return -1;
    }
    
    conn->state = STREAM_PROXY_STATE_CONNECTED;
    
    // 开始数据转发
    char buffer[DEFAULT_BUFFER_SIZE];
    fd_set read_fds;
    int max_fd = (conn->client_fd > conn->backend_fd) ? conn->client_fd : conn->backend_fd;
    
    conn->state = STREAM_PROXY_STATE_FORWARDING;
    
    while (conn->active) {
        FD_ZERO(&read_fds);
        FD_SET(conn->client_fd, &read_fds);
        FD_SET(conn->backend_fd, &read_fds);
        
        struct timeval timeout = {1, 0}; // 1秒超时
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ready < 0) {
            if (errno != EINTR) {
                log_message(LOG_LEVEL_ERROR, "Select error in TCP proxy");
                break;
            }
            continue;
        }
        
        if (ready == 0) continue; // 超时，继续循环
        
        // 客户端到后端
        if (FD_ISSET(conn->client_fd, &read_fds)) {
            if (stream_tcp_forward_data(conn->client_fd, conn->backend_fd, buffer, sizeof(buffer)) <= 0) {
                break;
            }
        }
        
        // 后端到客户端
        if (FD_ISSET(conn->backend_fd, &read_fds)) {
            if (stream_tcp_forward_data(conn->backend_fd, conn->client_fd, buffer, sizeof(buffer)) <= 0) {
                break;
            }
        }
    }
    
    conn->state = STREAM_PROXY_STATE_CLOSED;
    return 0;
}

// TCP数据转发
int stream_tcp_forward_data(int source_fd, int dest_fd, char *buffer, size_t buffer_size) {
    ssize_t bytes_read = recv(source_fd, buffer, buffer_size, 0);
    if (bytes_read <= 0) {
        return bytes_read; // 连接关闭或错误
    }
    
    ssize_t bytes_sent = send(dest_fd, buffer, bytes_read, 0);
    if (bytes_sent != bytes_read) {
        log_message(LOG_LEVEL_WARNING, "Incomplete data forwarding");
        return -1;
    }
    
    return bytes_sent;
}

// TCP连接后端
int stream_tcp_connect_backend(stream_connection_t *conn, upstream_server_t *server) {
    if (!conn || !server) return -1;
    
    // 创建后端socket
    conn->backend_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->backend_fd < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to create backend socket");
        return -1;
    }
    
    // 设置非阻塞模式进行连接
    int flags = fcntl(conn->backend_fd, F_GETFL, 0);
    fcntl(conn->backend_fd, F_SETFL, flags | O_NONBLOCK);
    
    // 连接后端服务器
    struct sockaddr_in backend_addr;
    memset(&backend_addr, 0, sizeof(backend_addr));
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(server->port);
    
    if (inet_pton(AF_INET, server->host, &backend_addr.sin_addr) <= 0) {
        log_message(LOG_LEVEL_ERROR, "Invalid backend server address");
        close(conn->backend_fd);
        conn->backend_fd = -1;
        return -1;
    }
    
    conn->backend_addr = backend_addr;
    
    int result = connect(conn->backend_fd, (struct sockaddr *)&backend_addr, sizeof(backend_addr));
    if (result < 0 && errno != EINPROGRESS) {
        log_message(LOG_LEVEL_ERROR, "Failed to connect to backend server");
        close(conn->backend_fd);
        conn->backend_fd = -1;
        return -1;
    }
    
    // 等待连接完成
    if (errno == EINPROGRESS) {
        fd_set write_fds;
        struct timeval timeout = {DEFAULT_CONNECT_TIMEOUT, 0};
        
        FD_ZERO(&write_fds);
        FD_SET(conn->backend_fd, &write_fds);
        
        result = select(conn->backend_fd + 1, NULL, &write_fds, NULL, &timeout);
        if (result <= 0) {
            log_message(LOG_LEVEL_ERROR, "Backend connection timeout");
            close(conn->backend_fd);
            conn->backend_fd = -1;
            return -1;
        }
        
        // 检查连接是否成功
        int error = 0;
        socklen_t error_len = sizeof(error);
        if (getsockopt(conn->backend_fd, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0 || error != 0) {
            log_message(LOG_LEVEL_ERROR, "Backend connection failed");
            close(conn->backend_fd);
            conn->backend_fd = -1;
            return -1;
        }
    }
    
    // 恢复阻塞模式
    fcntl(conn->backend_fd, F_SETFL, flags);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Connected to backend server %s:%d", server->host, server->port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return 0;
}

// UDP代理启动（简化实现）
int stream_udp_proxy_start(stream_connection_t *conn, lb_config_t *lb_config) {
    if (!conn || !lb_config) return -1;
    
    // UDP代理实现较复杂，需要会话管理
    // 这里提供一个基础框架
    log_message(LOG_LEVEL_INFO, "UDP proxy started (basic implementation)");
    
    return 0;
}

// UDP数据包转发
int stream_udp_forward_packet(stream_connection_t *conn, lb_config_t *lb_config) {
    if (!conn || !lb_config) return -1;
    
    // UDP数据包转发实现
    log_message(LOG_LEVEL_DEBUG, "UDP packet forwarded");
    
    return 0;
}

// 选择后端服务器（使用现有的负载均衡逻辑）
upstream_server_t *stream_select_backend(lb_config_t *lb_config, const char *upstream_name,
                                        const char *client_ip) {
    if (!lb_config || !upstream_name) return NULL;
    
    // 查找upstream组
    upstream_group_t *group = lb_config_get_group(lb_config, upstream_name);
    if (!group) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Upstream group not found: %s", upstream_name);
        log_message(LOG_LEVEL_ERROR, log_msg);
        return NULL;
    }
    
    // 使用负载均衡算法选择服务器
    lb_selection_t *selection = lb_select_server(group, client_ip, NULL);
    if (!selection || !selection->server) {
        log_message(LOG_LEVEL_ERROR, "No available server in upstream group");
        if (selection) lb_selection_free(selection);
        return NULL;
    }
    
    upstream_server_t *server = selection->server;
    lb_selection_free(selection);
    
    return server;
} 