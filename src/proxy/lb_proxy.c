#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "lb_proxy.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <strings.h>

#define BUFFER_SIZE 4096

// 处理负载均衡的HTTP代理请求
int handle_lb_proxy_request(int client_socket, const char *method, const char *path, 
                           const char *http_version, const char *headers, 
                           const char *upstream_name, const char *client_ip,
                           core_config_t *core_config) {
    if (!core_config || !core_config->lb_config) {
        log_message(LOG_LEVEL_ERROR, "Load balancer config not available");
        return -1;
    }
    
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Load balancing HTTP request %s %s to upstream '%s'", 
             method, path, upstream_name);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    // 获取upstream组
    upstream_group_t *group = lb_config_get_group(core_config->lb_config, upstream_name);
    if (!group) {
        snprintf(log_msg, sizeof(log_msg), "Upstream group '%s' not found", upstream_name);
        log_message(LOG_LEVEL_ERROR, log_msg);
        return -1;
    }
    
    // 选择服务器
    lb_selection_t *selection = lb_select_server(group, client_ip, NULL);
    if (!selection || !selection->server) {
        snprintf(log_msg, sizeof(log_msg), "No available server in upstream '%s'", upstream_name);
        log_message(LOG_LEVEL_ERROR, log_msg);
        if (selection) lb_selection_free(selection);
        return -1;
    }
    
    upstream_server_t *server = selection->server;
    
    // 连接到选中的服务器
    int backend_fd = lb_connect_to_server(server);
    if (backend_fd < 0) {
        snprintf(log_msg, sizeof(log_msg), "Failed to connect to server %s:%d", 
                 server->host, server->port);
        log_message(LOG_LEVEL_ERROR, log_msg);
        lb_selection_free(selection);
        return -1;
    }
    
    // 构建代理请求
    char *proxy_request = build_lb_proxy_request(method, path, http_version, headers, 
                                                server, client_ip);
    if (!proxy_request) {
        log_message(LOG_LEVEL_ERROR, "Failed to build proxy request");
        lb_close_connection(server, backend_fd);
        lb_selection_free(selection);
        return -1;
    }
    
    // 发送请求到后端
    ssize_t bytes_sent = send(backend_fd, proxy_request, strlen(proxy_request), 0);
    if (bytes_sent < 0) {
        snprintf(log_msg, sizeof(log_msg), "Failed to send request to server %s:%d - %s", 
                 server->host, server->port, strerror(errno));
        log_message(LOG_LEVEL_ERROR, log_msg);
        free(proxy_request);
        lb_close_connection(server, backend_fd);
        lb_selection_free(selection);
        return -1;
    }
    
    // 转发响应
    int result = forward_lb_response(backend_fd, client_socket, server);
    
    // 计算响应时间
    gettimeofday(&end_time, NULL);
    double response_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                          (end_time.tv_usec - start_time.tv_usec) / 1000.0;
    
    // 更新统计信息
    update_server_stats(server, (result >= 0), response_time);
    
    // 记录日志
    log_lb_request(method, path, client_ip, upstream_name, server, 
                  (result >= 0) ? 200 : 502, response_time);
    
    // 清理资源
    free(proxy_request);
    lb_close_connection(server, backend_fd);
    lb_selection_free(selection);
    
    return result;
}

// 处理负载均衡的HTTPS代理请求
int handle_lb_https_proxy_request(SSL *ssl, const char *method, const char *path, 
                                 const char *http_version, const char *headers,
                                 const char *upstream_name, const char *client_ip,
                                 core_config_t *core_config) {
    if (!core_config || !core_config->lb_config) {
        log_message(LOG_LEVEL_ERROR, "Load balancer config not available");
        return -1;
    }
    
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Load balancing HTTPS request %s %s to upstream '%s'", 
             method, path, upstream_name);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    // 获取upstream组
    upstream_group_t *group = lb_config_get_group(core_config->lb_config, upstream_name);
    if (!group) {
        snprintf(log_msg, sizeof(log_msg), "Upstream group '%s' not found", upstream_name);
        log_message(LOG_LEVEL_ERROR, log_msg);
        return -1;
    }
    
    // 选择服务器
    lb_selection_t *selection = lb_select_server(group, client_ip, NULL);
    if (!selection || !selection->server) {
        snprintf(log_msg, sizeof(log_msg), "No available server in upstream '%s'", upstream_name);
        log_message(LOG_LEVEL_ERROR, log_msg);
        if (selection) lb_selection_free(selection);
        return -1;
    }
    
    upstream_server_t *server = selection->server;
    
    // 连接到选中的服务器
    int backend_fd = lb_connect_to_server(server);
    if (backend_fd < 0) {
        snprintf(log_msg, sizeof(log_msg), "Failed to connect to server %s:%d", 
                 server->host, server->port);
        log_message(LOG_LEVEL_ERROR, log_msg);
        lb_selection_free(selection);
        return -1;
    }
    
    // 构建代理请求
    char *proxy_request = build_lb_proxy_request(method, path, http_version, headers, 
                                                server, client_ip);
    if (!proxy_request) {
        log_message(LOG_LEVEL_ERROR, "Failed to build proxy request");
        lb_close_connection(server, backend_fd);
        lb_selection_free(selection);
        return -1;
    }
    
    // 发送请求到后端
    ssize_t bytes_sent = send(backend_fd, proxy_request, strlen(proxy_request), 0);
    if (bytes_sent < 0) {
        snprintf(log_msg, sizeof(log_msg), "Failed to send request to server %s:%d - %s", 
                 server->host, server->port, strerror(errno));
        log_message(LOG_LEVEL_ERROR, log_msg);
        free(proxy_request);
        lb_close_connection(server, backend_fd);
        lb_selection_free(selection);
        return -1;
    }
    
    // 转发响应
    int result = forward_lb_https_response(backend_fd, ssl, server);
    
    // 计算响应时间
    gettimeofday(&end_time, NULL);
    double response_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                          (end_time.tv_usec - start_time.tv_usec) / 1000.0;
    
    // 更新统计信息
    update_server_stats(server, (result >= 0), response_time);
    
    // 记录日志
    log_lb_request(method, path, client_ip, upstream_name, server, 
                  (result >= 0) ? 200 : 502, response_time);
    
    // 清理资源
    free(proxy_request);
    lb_close_connection(server, backend_fd);
    lb_selection_free(selection);
    
    return result;
}

// 检查是否为upstream代理（proxy_pass指向upstream组）
int is_upstream_proxy(const char *proxy_pass_value) {
    if (!proxy_pass_value) return 0;
    
    // 检查是否以"http://"开头但不包含端口号或IP地址
    if (strncmp(proxy_pass_value, "http://", 7) == 0) {
        const char *after_protocol = proxy_pass_value + 7;
        
        // 如果包含点号或数字，可能是IP地址或域名
        if (strchr(after_protocol, '.') || strchr(after_protocol, ':')) {
            return 0;  // 不是upstream，是直接的URL
        }
        
        // 检查是否只包含字母、数字、下划线和连字符（upstream名称格式）
        for (const char *p = after_protocol; *p && *p != '/' && *p != '?'; p++) {
            if (!isalnum(*p) && *p != '_' && *p != '-') {
                return 0;
            }
        }
        
        return 1;  // 是upstream
    }
    
    // 检查是否直接是upstream名称（不带协议前缀）
    if (strncmp(proxy_pass_value, "upstream://", 11) == 0) {
        return 1;
    }
    
    return 0;
}

// 从proxy_pass值中提取upstream名称
char *extract_upstream_name(const char *proxy_pass_value) {
    if (!proxy_pass_value) return NULL;
    
    if (strncmp(proxy_pass_value, "http://", 7) == 0) {
        const char *after_protocol = proxy_pass_value + 7;
        const char *end = strchr(after_protocol, '/');
        if (end) {
            return strndup(after_protocol, end - after_protocol);
        } else {
            return strdup(after_protocol);
        }
    } else if (strncmp(proxy_pass_value, "upstream://", 11) == 0) {
        const char *after_protocol = proxy_pass_value + 11;
        const char *end = strchr(after_protocol, '/');
        if (end) {
            return strndup(after_protocol, end - after_protocol);
        } else {
            return strdup(after_protocol);
        }
    }
    
    return NULL;
}

// 构建负载均衡的代理请求
char *build_lb_proxy_request(const char *method, const char *original_path, 
                            const char *http_version, const char *headers,
                            upstream_server_t *server, const char *client_ip) {
    if (!method || !original_path || !http_version || !server) return NULL;
    
    char *request_buffer = malloc(BUFFER_SIZE * 2);
    if (!request_buffer) return NULL;
    
    // 构建请求行
    int offset = snprintf(request_buffer, BUFFER_SIZE * 2, 
                         "%s %s %s\r\n", method, original_path, http_version);
    
    // 添加Host头
    offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset,
                      "Host: %s:%d\r\n", server->host, server->port);
    
    // 添加代理相关头
    if (client_ip) {
        offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset,
                          "X-Forwarded-For: %s\r\n", client_ip);
    }
    
    offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset,
                      "X-Forwarded-Proto: http\r\n");
    
    offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset,
                      "X-Real-IP: %s\r\n", client_ip ? client_ip : "unknown");
    
    // 添加负载均衡器标识
    offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset,
                      "X-Load-Balancer: ANX-LB/1.0\r\n");
    
    // 添加其他头信息（跳过原始的Host头和Connection头）
    if (headers) {
        char *headers_copy = strdup(headers);
        char *line = strtok(headers_copy, "\r\n");
        while (line) {
            // 跳过Host头、Connection头和代理相关头
            if (strncasecmp(line, "Host:", 5) != 0 && 
                strncasecmp(line, "Connection:", 11) != 0 &&
                strncasecmp(line, "X-Forwarded-", 12) != 0 &&
                strncasecmp(line, "X-Real-IP:", 10) != 0) {
                offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset,
                                  "%s\r\n", line);
            }
            line = strtok(NULL, "\r\n");
        }
        free(headers_copy);
    }
    
    // 添加Connection: close头
    offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset,
                      "Connection: close\r\n");
    
    // 结束头部分
    offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset, "\r\n");
    
    return request_buffer;
}

// 处理负载均衡代理响应
int forward_lb_response(int backend_fd, int client_fd, upstream_server_t *server) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    int total_bytes = 0;
    
    while ((bytes_read = read(backend_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(client_fd, buffer, bytes_read);
        if (bytes_written < 0) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Failed to write response to client from server %s:%d", 
                     server->host, server->port);
            log_message(LOG_LEVEL_ERROR, log_msg);
            return -1;
        }
        total_bytes += bytes_written;
    }
    
    if (bytes_read < 0) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to read response from server %s:%d", 
                 server->host, server->port);
        log_message(LOG_LEVEL_ERROR, log_msg);
        return -1;
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Forwarded %d bytes from server %s:%d to client", 
             total_bytes, server->host, server->port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return total_bytes;
}

// 处理负载均衡HTTPS代理响应
int forward_lb_https_response(int backend_fd, SSL *ssl, upstream_server_t *server) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int total_bytes = 0;
    
    while ((bytes_read = read(backend_fd, buffer, sizeof(buffer))) > 0) {
        if (SSL_write(ssl, buffer, bytes_read) <= 0) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Failed to write response to SSL client from server %s:%d", 
                     server->host, server->port);
            log_message(LOG_LEVEL_ERROR, log_msg);
            return -1;
        }
        total_bytes += bytes_read;
    }
    
    if (bytes_read < 0) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to read response from server %s:%d", 
                 server->host, server->port);
        log_message(LOG_LEVEL_ERROR, log_msg);
        return -1;
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Forwarded %d bytes from server %s:%d to HTTPS client", 
             total_bytes, server->host, server->port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return total_bytes;
}

// 更新服务器统计信息
void update_server_stats(upstream_server_t *server, int success, double response_time) {
    if (!server) return;
    
    lb_update_stats(server, success, response_time);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Updated stats for server %s:%d - Success: %s, Response time: %.2f ms", 
             server->host, server->port, success ? "Yes" : "No", response_time);
    log_message(LOG_LEVEL_DEBUG, log_msg);
}

// 记录负载均衡日志
void log_lb_request(const char *method, const char *path, const char *client_ip,
                   const char *upstream_name, upstream_server_t *server, 
                   int status_code, double response_time) {
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "LB Request: %s %s from %s -> upstream '%s' -> server %s:%d - Status: %d, Time: %.2f ms", 
             method ? method : "unknown", 
             path ? path : "unknown", 
             client_ip ? client_ip : "unknown",
             upstream_name ? upstream_name : "unknown",
             server ? server->host : "unknown",
             server ? server->port : 0,
             status_code, response_time);
    log_message(LOG_LEVEL_INFO, log_msg);
} 