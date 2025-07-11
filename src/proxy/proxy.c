#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "proxy.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <strings.h>

#include "log.h"

#define BUFFER_SIZE 4096
#define PROXY_TIMEOUT 30  // 30秒超时

// 解析proxy_pass URL
proxy_url_t *parse_proxy_url(const char *url) {
    if (!url) return NULL;
    
    proxy_url_t *proxy_url = calloc(1, sizeof(proxy_url_t));
    if (!proxy_url) return NULL;
    
    // 默认值
    proxy_url->port = 80;
    proxy_url->path = strdup("/");
    
    // 复制URL以便解析
    char *url_copy = strdup(url);
    char *p = url_copy;
    
    // 解析协议
    if (strncmp(p, "http://", 7) == 0) {
        proxy_url->protocol = strdup("http");
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        proxy_url->protocol = strdup("https");
        proxy_url->port = 443;  // HTTPS默认端口
        p += 8;
    } else {
        // 默认HTTP
        proxy_url->protocol = strdup("http");
    }
    
    // 查找主机名结束位置
    char *host_end = strchr(p, ':');
    char *path_start = strchr(p, '/');
    
    if (host_end && (!path_start || host_end < path_start)) {
        // 有端口号
        proxy_url->host = strndup(p, host_end - p);
        p = host_end + 1;
        
        // 解析端口
        char *port_end = strchr(p, '/');
        if (port_end) {
            proxy_url->port = atoi(strndup(p, port_end - p));
            p = port_end;
        } else {
            proxy_url->port = atoi(p);
            p = p + strlen(p);
        }
    } else if (path_start) {
        // 没有端口号，但有路径
        proxy_url->host = strndup(p, path_start - p);
        p = path_start;
    } else {
        // 只有主机名
        proxy_url->host = strdup(p);
        p = p + strlen(p);
    }
    
    // 解析路径
    if (*p == '/') {
        free(proxy_url->path);
        proxy_url->path = strdup(p);
    }
    
    free(url_copy);
    return proxy_url;
}

// 释放proxy_url_t结构
void free_proxy_url(proxy_url_t *url) {
    if (!url) return;
    free(url->protocol);
    free(url->host);
    free(url->path);
    free(url);
}

// 建立到后端服务器的连接
static int connect_to_backend(const char *host, int port) {
    struct sockaddr_in server_addr;
    int sock_fd;
    
    // 创建socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to create socket for backend connection");
        return -1;
    }
    
    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = PROXY_TIMEOUT;
    timeout.tv_usec = 0;
    
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        log_message(LOG_LEVEL_WARNING, "Failed to set socket receive timeout");
    }
    
    if (setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        log_message(LOG_LEVEL_WARNING, "Failed to set socket send timeout");
    }
    
    // 解析主机名
    struct hostent *he = gethostbyname(host);
    if (!he) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to resolve hostname: %s", host);
        log_message(LOG_LEVEL_ERROR, log_msg);
        close(sock_fd);
        return -1;
    }
    
    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    // 连接到后端服务器
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to connect to backend %s:%d - %s", 
                host, port, strerror(errno));
        log_message(LOG_LEVEL_ERROR, log_msg);
        close(sock_fd);
        return -1;
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Successfully connected to backend %s:%d", host, port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return sock_fd;
}

// 构建代理请求
static char *build_proxy_request(const char *method, const char *original_path, 
                               const char *http_version, const char *headers,
                               const char *backend_host, int backend_port,
                               const char *proxy_path) {
    char *request_buffer = malloc(BUFFER_SIZE * 2);
    if (!request_buffer) return NULL;
    
    // 构建新的路径
    char new_path[512];
    if (strcmp(proxy_path, "/") == 0) {
        // 如果proxy_path是根路径，直接使用原始路径
        snprintf(new_path, sizeof(new_path), "%s", original_path);
    } else {
        // 否则替换路径前缀
        snprintf(new_path, sizeof(new_path), "%s", original_path);
    }
    
    // 构建请求行
    int offset = snprintf(request_buffer, BUFFER_SIZE * 2, 
                         "%s %s %s\r\n", method, new_path, http_version);
    
    // 添加或修改Host头
    offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset,
                      "Host: %s:%d\r\n", backend_host, backend_port);
    
    // 添加代理相关头
    offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset,
                      "X-Forwarded-For: %s\r\n", "127.0.0.1");  // 这里应该是客户端IP
    
    offset += snprintf(request_buffer + offset, BUFFER_SIZE * 2 - offset,
                      "X-Forwarded-Proto: http\r\n");
    
    // 添加其他头信息（跳过原始的Host头）
    if (headers) {
        char *headers_copy = strdup(headers);
        char *line = strtok(headers_copy, "\r\n");
        while (line) {
            // 跳过Host头和Connection头
            if (strncasecmp(line, "Host:", 5) != 0 && 
                strncasecmp(line, "Connection:", 11) != 0) {
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

// 转发响应数据
static int forward_response(int backend_fd, int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    int total_bytes = 0;
    
    while ((bytes_read = read(backend_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(client_fd, buffer, bytes_read);
        if (bytes_written < 0) {
            log_message(LOG_LEVEL_ERROR, "Failed to write response to client");
            return -1;
        }
        total_bytes += bytes_written;
    }
    
    if (bytes_read < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to read response from backend");
        return -1;
    }
    
    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "Forwarded %d bytes from backend to client", total_bytes);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return total_bytes;
}

// 处理HTTP反向代理请求
int handle_proxy_request(int client_socket, const char *req_path, const char *proxy_pass,
                        const char *client_ip, core_config_t *core_conf) {
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Proxying request %s to %s", 
             req_path, proxy_pass);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    // 解析proxy_pass URL
    proxy_url_t *proxy_url = parse_proxy_url(proxy_pass);
    if (!proxy_url) {
        log_message(LOG_LEVEL_ERROR, "Failed to parse proxy_pass URL");
        return -1;
    }
    
    // 连接到后端服务器
    int backend_fd = connect_to_backend(proxy_url->host, proxy_url->port);
    if (backend_fd < 0) {
        free_proxy_url(proxy_url);
        return -1;
    }
    
    // 构建代理请求
    // 注意: 这里简化了请求构建，实际实现中应该从HTTP请求中提取method、http_version和headers
    const char *method = "GET";  // 从请求中提取
    const char *http_version = "HTTP/1.1";  // 默认使用HTTP/1.1
    const char *headers = "";  // 应该从原始请求中提取并过滤敏感头
    
    char *proxy_request = build_proxy_request(method, req_path, http_version, headers,
                                             proxy_url->host, proxy_url->port,
                                             proxy_url->path);
    if (!proxy_request) {
        log_message(LOG_LEVEL_ERROR, "Failed to build proxy request");
        close(backend_fd);
        free_proxy_url(proxy_url);
        return -1;
    }
    
    // 发送请求到后端
    ssize_t bytes_sent = send(backend_fd, proxy_request, strlen(proxy_request), 0);
    if (bytes_sent < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to send request to backend");
        close(backend_fd);
        free(proxy_request);
        free_proxy_url(proxy_url);
        return -1;
    }
    
    // 转发响应
    int result = forward_response(backend_fd, client_socket);
    
    // 清理资源
    close(backend_fd);
    free(proxy_request);
    free_proxy_url(proxy_url);
    
    return result;
}

// 处理HTTPS反向代理请求
int handle_https_proxy_request(SSL *ssl, const char *method, const char *path, 
                             const char *http_version, const char *headers,
                             const char *proxy_pass_url, const char *client_ip) {
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "HTTPS Proxying request %s %s to %s", 
             method, path, proxy_pass_url);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    // 解析proxy_pass URL
    proxy_url_t *proxy_url = parse_proxy_url(proxy_pass_url);
    if (!proxy_url) {
        log_message(LOG_LEVEL_ERROR, "Failed to parse proxy_pass URL");
        return -1;
    }
    
    // 连接到后端服务器
    int backend_fd = connect_to_backend(proxy_url->host, proxy_url->port);
    if (backend_fd < 0) {
        free_proxy_url(proxy_url);
        return -1;
    }
    
    // 构建代理请求
    char *proxy_request = build_proxy_request(method, path, http_version, headers,
                                            proxy_url->host, proxy_url->port,
                                            proxy_url->path);
    if (!proxy_request) {
        log_message(LOG_LEVEL_ERROR, "Failed to build proxy request");
        close(backend_fd);
        free_proxy_url(proxy_url);
        return -1;
    }
    
    // 发送请求到后端
    ssize_t bytes_sent = send(backend_fd, proxy_request, strlen(proxy_request), 0);
    if (bytes_sent < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to send request to backend");
        close(backend_fd);
        free(proxy_request);
        free_proxy_url(proxy_url);
        return -1;
    }
    
    // 转发响应
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int total_bytes = 0;
    int in_headers = 1;  // 标记是否在处理响应头
    int content_length = -1;  // 响应体长度
    int chunked = 0;  // 是否为分块传输
    
    // 读取并解析响应头
    char *header_buffer = malloc(BUFFER_SIZE);
    int header_len = 0;
    
    while (in_headers && (bytes_read = read(backend_fd, buffer, sizeof(buffer))) > 0) {
        // 寻找响应头结束标记
        char *header_end = memmem(buffer, bytes_read, "\r\n\r\n", 4);
        if (header_end) {
            // 计算头部长度
            int headers_size = header_end - buffer + 4;
            memcpy(header_buffer + header_len, buffer, headers_size);
            header_len += headers_size;
            
            // 解析Content-Length
            char *cl_start = strcasestr(header_buffer, "Content-Length:");
            if (cl_start) {
                content_length = atoi(cl_start + 15);
            }
            
            // 检查是否为分块传输
            char *te_start = strcasestr(header_buffer, "Transfer-Encoding:");
            if (te_start && strstr(te_start, "chunked")) {
                chunked = 1;
            }
            
            // 发送响应头到客户端
            if (SSL_write(ssl, header_buffer, header_len) <= 0) {
                log_message(LOG_LEVEL_ERROR, "Failed to write response headers to SSL client");
                free(header_buffer);
                close(backend_fd);
                free(proxy_request);
                free_proxy_url(proxy_url);
                return -1;
            }
            
            total_bytes += header_len;
            
            // 处理剩余的响应体部分
            int remaining = bytes_read - headers_size;
            if (remaining > 0) {
                if (SSL_write(ssl, header_end + 4, remaining) <= 0) {
                    log_message(LOG_LEVEL_ERROR, "Failed to write response body to SSL client");
                    free(header_buffer);
                    close(backend_fd);
                    free(proxy_request);
                    free_proxy_url(proxy_url);
                    return -1;
                }
                total_bytes += remaining;
            }
            
            in_headers = 0;
        } else {
            // 继续累积头部数据
            memcpy(header_buffer + header_len, buffer, bytes_read);
            header_len += bytes_read;
        }
    }
    
    free(header_buffer);
    
    // 继续转发响应体
    while ((bytes_read = read(backend_fd, buffer, sizeof(buffer))) > 0) {
        if (SSL_write(ssl, buffer, bytes_read) <= 0) {
            log_message(LOG_LEVEL_ERROR, "Failed to write response to SSL client");
            close(backend_fd);
            free(proxy_request);
            free_proxy_url(proxy_url);
            return -1;
        }
        total_bytes += bytes_read;
    }
    
    if (bytes_read < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to read response from backend");
        close(backend_fd);
        free(proxy_request);
        free_proxy_url(proxy_url);
        return -1;
    }
    
    snprintf(log_msg, sizeof(log_msg), "Forwarded %d bytes from backend to SSL client", 
             total_bytes);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    // 清理资源
    close(backend_fd);
    free(proxy_request);
    free_proxy_url(proxy_url);
    
    return total_bytes;
}
