#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "http.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"
#include "core.h"
#include "log.h"
#include "util.h"
#include "proxy.h"
#include "lb_proxy.h"
#include "headers.h"
#include "compress.h"
#include "cache.h"
#include "health_api.h"
#include "chunked.h"
#include "bandwidth.h"

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

// Helper to extract User-Agent header from request
static char *get_user_agent(const char *buffer) {
    const char *ua_hdr = "User-Agent: ";
    char *ua_start = strcasestr(buffer, ua_hdr);
    if (!ua_start) return strdup("-");

    ua_start += strlen(ua_hdr);
    char *ua_end = strstr(ua_start, "\r\n");
    if (!ua_end) return strdup("-");

    return strndup(ua_start, ua_end - ua_start);
}

// Helper to extract Referer header from request
static char *get_referer(const char *buffer) {
    const char *ref_hdr = "Referer: ";
    char *ref_start = strcasestr(buffer, ref_hdr);
    if (!ref_start) return strdup("-");

    ref_start += strlen(ref_hdr);
    char *ref_end = strstr(ref_start, "\r\n");
    if (!ref_end) return strdup("-");

    return strndup(ref_start, ref_end - ref_start);
}

// Helper to get client IP address from socket
static char *get_client_ip(int client_socket) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    if (getpeername(client_socket, (struct sockaddr *)&client_addr, &addr_len) == 0) {
        return strdup(inet_ntoa(client_addr.sin_addr));
    }
    
    return strdup("127.0.0.1"); // 默认返回本地IP
}

// 提取HTTP头部信息
static char *extract_headers(const char *buffer) {
    const char *headers_start = strchr(buffer, '\n');
    if (!headers_start) return NULL;
    
    headers_start++; // 跳过第一行
    const char *headers_end = strstr(headers_start, "\r\n\r\n");
    if (!headers_end) return NULL;
    
    return strndup(headers_start, headers_end - headers_start);
}

// 提取特定头部的值
static char *extract_header_value(const char *buffer, const char *header_name) {
    if (!buffer || !header_name) return NULL;
    
    char search_header[64];
    snprintf(search_header, sizeof(search_header), "%s: ", header_name);
    
    char *header_start = strcasestr(buffer, search_header);
    if (!header_start) return NULL;
    
    header_start += strlen(search_header);
    char *header_end = strstr(header_start, "\r\n");
    if (!header_end) return NULL;
    
    return strndup(header_start, header_end - header_start);
}

void handle_http_request(int client_socket, const char *client_ip, core_config_t *core_conf) {
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

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
    char *user_agent = get_user_agent(buffer);
    char *referer = get_referer(buffer);

    // 提取条件请求头部
    char *if_none_match = extract_header_value(buffer, "If-None-Match");
    char *if_modified_since_str = extract_header_value(buffer, "If-Modified-Since");
    time_t if_modified_since = 0;
    if (if_modified_since_str) {
        // 简单的时间解析，实际应该使用更完整的HTTP日期解析
        if_modified_since = atol(if_modified_since_str);
    }

    // Create access log entry
    access_log_entry_t *access_entry = create_access_log_entry();
    if (access_entry) {
        // Set basic request info
        free(access_entry->client_ip);
        access_entry->client_ip = strdup(client_ip ? client_ip : "-");
        
        if (method) {
            free(access_entry->method);
            access_entry->method = strdup(method);
        }
        
        if (req_path) {
            free(access_entry->uri);
            access_entry->uri = strdup(req_path);
        }
        
        if (http_version) {
            free(access_entry->protocol);
            access_entry->protocol = strdup(http_version);
        }
        
        if (user_agent) {
            free(access_entry->user_agent);
            access_entry->user_agent = strdup(user_agent);
        }
        
        if (referer) {
            free(access_entry->referer);
            access_entry->referer = strdup(referer);
        }
        
        access_entry->request_time = start_time;
        access_entry->server_port = 80; // Default HTTP port
    }

    if (!method || !req_path) {
        if (access_entry) {
            access_entry->status_code = 400;
            access_entry->response_size = 0;
            
            struct timeval end_time;
            gettimeofday(&end_time, NULL);
            access_entry->request_duration_ms = 
                (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                (end_time.tv_usec - start_time.tv_usec) / 1000.0;
            
            log_access_entry(access_entry);
            free_access_log_entry(access_entry);
        }
        close(client_socket);
        free(buffer_copy);
        free(host);
        free(user_agent);
        free(referer);
        if (if_none_match) free(if_none_match);
        if (if_modified_since_str) free(if_modified_since_str);
        return;
    }

    // 检查是否是健康检查API请求
    if (strncmp(req_path, "/health", 7) == 0 && core_conf && core_conf->lb_config) {
        // 提取查询字符串
        char *query_string = strchr(req_path, '?');
        if (query_string) {
            *query_string = '\0';
            query_string++;
        }
        
        // 解析API请求
        health_api_request_t *api_request = health_api_parse_request(req_path, method, query_string);
        if (api_request) {
            // 处理API请求
            health_api_response_t *api_response = health_api_handle_request(api_request, core_conf->lb_config);
            if (api_response) {
                // 发送API响应
                char response_header[1024];
                snprintf(response_header, sizeof(response_header),
                         "HTTP/1.1 %d %s\r\n"
                         "Content-Type: %s\r\n"
                         "Content-Length: %zu\r\n"
                         "Connection: close\r\n"
                         "X-Powered-By: ANX-HealthCheck/1.0\r\n"
                         "\r\n",
                         api_response->status_code,
                         api_response->status_code == 200 ? "OK" : "Error",
                         api_response->content_type ? api_response->content_type : "application/json",
                         api_response->body_size);
                
                send(client_socket, response_header, strlen(response_header), 0);
                if (api_response->body) {
                    send(client_socket, api_response->body, api_response->body_size, 0);
                }
                
                // 记录访问日志
                if (access_entry) {
                    access_entry->status_code = api_response->status_code;
                    access_entry->response_size = strlen(response_header) + api_response->body_size;
                    
                    struct timeval end_time;
                    gettimeofday(&end_time, NULL);
                    access_entry->request_duration_ms = 
                        (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_usec - start_time.tv_usec) / 1000.0;
                    
                    log_access_entry(access_entry);
                    free_access_log_entry(access_entry);
                }
                
                health_api_response_free(api_response);
            }
            health_api_request_free(api_request);
        }
        
        close(client_socket);
        free(buffer_copy);
        free(host);
        free(user_agent);
        free(referer);
        free(if_none_match);
        free(if_modified_since_str);
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
        
        if (access_entry) {
            access_entry->status_code = 403;
            access_entry->response_size = 0;
            
            struct timeval end_time;
            gettimeofday(&end_time, NULL);
            access_entry->request_duration_ms = 
                (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                (end_time.tv_usec - start_time.tv_usec) / 1000.0;
            
            log_access_entry(access_entry);
            free_access_log_entry(access_entry);
        }
        
        close(client_socket);
        free(buffer_copy);
        free(host);
        free(user_agent);
        free(referer);
        if (if_none_match) free(if_none_match);
        if (if_modified_since_str) free(if_modified_since_str);
        return;
    }

    // 检查缓存
    cache_response_t *cached_response = NULL;
    if (core_conf->cache_manager && method && strcmp(method, "GET") == 0) {
        cached_response = cache_get(core_conf->cache_manager, req_path, 
                                   if_none_match, if_modified_since);
        
        if (cached_response) {
            if (cached_response->needs_validation) {
                // 304 Not Modified
                const char *response = "HTTP/1.1 304 Not Modified\r\n"
                                      "Server: ANX HTTP Server/0.6.0\r\n"
                                      "Connection: close\r\n\r\n";
                if (write(client_socket, response, strlen(response)) < 0) {
                    log_message(LOG_LEVEL_ERROR, "Failed to write response to client");
                }
                
                if (access_entry) {
                    access_entry->status_code = 304;
                    access_entry->response_size = strlen(response);
                    
                    struct timeval end_time;
                    gettimeofday(&end_time, NULL);
                    access_entry->request_duration_ms = 
                        (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_usec - start_time.tv_usec) / 1000.0;
                    
                    log_access_entry(access_entry);
                    free_access_log_entry(access_entry);
                }
                
                cache_response_free(cached_response);
                free(host);
                free(buffer_copy);
                free(user_agent);
                free(referer);
                if (if_none_match) free(if_none_match);
                if (if_modified_since_str) free(if_modified_since_str);
                close(client_socket);
                return;
            }
            
            if (cached_response->is_cached && cached_response->content) {
                // 从缓存返回内容
                const char *mime_type = cached_response->content_type ? 
                                       cached_response->content_type : "application/octet-stream";
                
                char header[BUFFER_SIZE * 2];
                int header_len = snprintf(header, sizeof(header),
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: %s\r\n"
                         "Content-Length: %zu\r\n"
                         "Server: ANX HTTP Server/0.6.0\r\n"
                         "X-Cache: HIT\r\n",
                         mime_type, cached_response->content_length);
                
                if (cached_response->etag) {
                    header_len += snprintf(header + header_len, sizeof(header) - header_len,
                                          "ETag: %s\r\n", cached_response->etag);
                }
                
                if (cached_response->last_modified > 0) {
                    header_len += snprintf(header + header_len, sizeof(header) - header_len,
                                          "Last-Modified: %ld\r\n", cached_response->last_modified);
                }
                
                if (cached_response->is_compressed) {
                    header_len += snprintf(header + header_len, sizeof(header) - header_len,
                                          "Content-Encoding: gzip\r\n"
                                          "Vary: Accept-Encoding\r\n");
                }
                
                header_len += snprintf(header + header_len, sizeof(header) - header_len,
                                      "Connection: close\r\n\r\n");
                
                if (write(client_socket, header, strlen(header)) < 0) {
                    log_message(LOG_LEVEL_ERROR, "Failed to write header to client");
                }
                if (write(client_socket, cached_response->content, cached_response->content_length) < 0) {
                    log_message(LOG_LEVEL_ERROR, "Failed to write cached content to client");
                }
                
                if (access_entry) {
                    access_entry->status_code = 200;
                    access_entry->response_size = strlen(header) + cached_response->content_length;
                    
                    struct timeval end_time;
                    gettimeofday(&end_time, NULL);
                    access_entry->request_duration_ms = 
                        (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_usec - start_time.tv_usec) / 1000.0;
                    
                    log_access_entry(access_entry);
                    free_access_log_entry(access_entry);
                }
                
                cache_response_free(cached_response);
                free(host);
                free(buffer_copy);
                free(user_agent);
                free(referer);
                if (if_none_match) free(if_none_match);
                if (if_modified_since_str) free(if_modified_since_str);
                close(client_socket);
                return;
            }
        }
    }

    // --- Routing ---
    route_t route = find_route(core_conf, host, req_path, 8080);
    if (!route.server) {
        // Send a 500 internal server error
        const char *response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
        if (write(client_socket, response, strlen(response)) < 0) {
            log_message(LOG_LEVEL_ERROR, "Failed to write 500 response to client");
        }
        
        if (access_entry) {
            access_entry->status_code = 500;
            access_entry->response_size = strlen(response);
            
            struct timeval end_time;
            gettimeofday(&end_time, NULL);
            access_entry->request_duration_ms = 
                (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                (end_time.tv_usec - start_time.tv_usec) / 1000.0;
            
            log_access_entry(access_entry);
            free_access_log_entry(access_entry);
        }
        
        free(host);
        free(buffer_copy);
        free(user_agent);
        free(referer);
        if (if_none_match) free(if_none_match);
        if (if_modified_since_str) free(if_modified_since_str);
        close(client_socket);
        return;
    }

    // Set server info in access log
    if (access_entry) {
        const char *server_name = get_directive_value("server_name", route.server->directives, route.server->directive_count);
        if (server_name) {
            free(access_entry->server_name);
            access_entry->server_name = strdup(server_name);
        }
        
        const char *listen_port = get_directive_value("listen", route.server->directives, route.server->directive_count);
        if (listen_port) {
            access_entry->server_port = atoi(listen_port);
        }
    }

    // 检查是否有proxy_pass指令
    const char *proxy_pass = NULL;
    if (route.location) {
        proxy_pass = get_directive_value("proxy_pass", route.location->directives, route.location->directive_count);
    }

    // 如果配置了proxy_pass，执行反向代理
    if (proxy_pass) {
        char *headers = extract_headers(buffer);
        int result = -1;
        
        // 检查是否为upstream代理
        if (is_upstream_proxy(proxy_pass)) {
            char *upstream_name = extract_upstream_name(proxy_pass);
            if (upstream_name) {
                result = handle_lb_proxy_request(client_socket, method, req_path, http_version, 
                                               headers, upstream_name, client_ip, core_conf);
                free(upstream_name);
            }
        } else {
            // 传统的直接代理
            result = handle_proxy_request(client_socket, method, req_path, http_version, 
                                         headers, proxy_pass, client_ip);
        }
        
        if (access_entry) {
            free(access_entry->upstream_addr);
            access_entry->upstream_addr = strdup(proxy_pass);
            
            if (result < 0) {
                access_entry->status_code = 502;
                access_entry->response_size = 15; // "Bad Gateway" length
                access_entry->upstream_status = 502;
                
                snprintf(log_msg, sizeof(log_msg), "Proxy request failed for %s", req_path);
                log_message(LOG_LEVEL_ERROR, log_msg);
            } else {
                access_entry->status_code = 200;
                access_entry->upstream_status = 200;
                
                snprintf(log_msg, sizeof(log_msg), "Proxy request completed for %s", req_path);
                log_message(LOG_LEVEL_INFO, log_msg);
            }
            
            struct timeval end_time;
            gettimeofday(&end_time, NULL);
            access_entry->request_duration_ms = 
                (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                (end_time.tv_usec - start_time.tv_usec) / 1000.0;
            
            log_access_entry(access_entry);
            free_access_log_entry(access_entry);
        }
        
        if (result < 0) {
            // 代理失败，返回502错误
            const char *response = "HTTP/1.1 502 Bad Gateway\r\n"
                                  "Content-Type: text/plain\r\n"
                                  "Content-Length: 15\r\n"
                                  "Connection: close\r\n\r\n"
                                  "Bad Gateway";
            if (write(client_socket, response, strlen(response)) < 0) {
                log_message(LOG_LEVEL_ERROR, "Failed to write proxy error response to client");
            }
        }
        
        free(headers);
        free(host);
        free(buffer_copy);
        free(user_agent);
        free(referer);
        close(client_socket);
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
        // 处理根目录请求，查找index文件
        const char *index_directive = NULL;
        if (route.location) {
            index_directive = get_directive_value("index", route.location->directives, route.location->directive_count);
        }
        if (!index_directive && route.server) {
            index_directive = get_directive_value("index", route.server->directives, route.server->directive_count);
        }
        
        bool index_found = false;
        if (index_directive) {
            // 解析index指令，尝试多个文件
            char *index_copy = strdup(index_directive);
            char *index_file = strtok(index_copy, " \t");
            
            while (index_file && !index_found) {
                snprintf(file_path, sizeof(file_path), "%s/%s", root, index_file);
                struct stat temp_stat;
                if (stat(file_path, &temp_stat) == 0 && S_ISREG(temp_stat.st_mode)) {
                    index_found = true;
                }
                if (!index_found) {
                    index_file = strtok(NULL, " \t");
                }
            }
            free(index_copy);
        }
        
        if (!index_found) {
            // 如果没有找到配置的index文件，使用默认的
            snprintf(file_path, sizeof(file_path), "%s%s", root, TEMP_DEFAULT_PAGE);
        }
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
        if (write(client_socket, response, strlen(response)) < 0) {
            log_message(LOG_LEVEL_ERROR, "Failed to write 500 error response to client");
        }
        
        if (access_entry) {
            access_entry->status_code = 500;
            access_entry->response_size = strlen(response);
            
            struct timeval end_time;
            gettimeofday(&end_time, NULL);
            access_entry->request_duration_ms = 
                (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                (end_time.tv_usec - start_time.tv_usec) / 1000.0;
            
            log_access_entry(access_entry);
            free_access_log_entry(access_entry);
        }
        
        free(host);
        free(buffer_copy);
        free(user_agent);
        free(referer);
        if (if_none_match) free(if_none_match);
        if (if_modified_since_str) free(if_modified_since_str);
        close(client_socket);
        return;
    }

    const char *mime_type = get_mime_type(file_path);
    
    // 检查是否需要使用分块传输编码
    bool use_chunked_encoding = false;
    if (http_version && chunked_is_supported(buffer)) {
        use_chunked_encoding = chunked_should_use(mime_type, file_stat.st_size);
    }
    
    // 检查是否需要压缩
    char *accept_encoding = extract_header_value(buffer, "Accept-Encoding");
    bool should_compress = false;
    compress_context_t *compress_ctx = NULL;
    unsigned char *compressed_data = NULL;
    size_t compressed_size = 0;
    long final_content_length = file_stat.st_size;
    
    // 获取压缩配置
    compress_config_t *compress_config = NULL;
    if (core_conf && core_conf->raw_config && core_conf->raw_config->compress) {
        compress_config = core_conf->raw_config->compress;
    }
    
    if (compress_config && compress_config->enable_compression && 
        accept_encoding && client_accepts_compression(accept_encoding) &&
        should_compress_mime_type(compress_config, mime_type) &&
        (size_t)file_stat.st_size >= compress_config->min_length) {
        
        should_compress = true;
        compress_ctx = compress_context_create(compress_config);
        
        if (compress_ctx) {
            // 读取文件内容
            char *file_content = malloc(file_stat.st_size);
            if (file_content) {
                ssize_t bytes_read = read(file_fd, file_content, file_stat.st_size);
                if (bytes_read == file_stat.st_size) {
                    // 压缩数据
                    compressed_data = malloc(file_stat.st_size + 1024); // 预留空间
                    compressed_size = file_stat.st_size + 1024;
                    
                    if (compress_data(compress_ctx, file_content, file_stat.st_size, 
                                    compressed_data, &compressed_size, Z_FINISH) == Z_STREAM_END) {
                        final_content_length = compressed_size;
                        log_message(LOG_LEVEL_DEBUG, "File compressed successfully");
                    } else {
                        should_compress = false;
                        free(compressed_data);
                        compressed_data = NULL;
                        log_message(LOG_LEVEL_WARNING, "Compression failed, sending uncompressed");
                    }
                } else {
                    should_compress = false;
                    log_message(LOG_LEVEL_WARNING, "Failed to read file for compression");
                }
                free(file_content);
            } else {
                should_compress = false;
            }
        } else {
            should_compress = false;
        }
    }
    


    // 构建响应头
    char header[BUFFER_SIZE * 2];
    int header_len;
    
    if (use_chunked_encoding) {
        // 使用分块传输编码
        header_len = snprintf(header, sizeof(header),
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: %s\r\n"
                 "Transfer-Encoding: chunked\r\n"
                 "Server: ANX HTTP Server/0.8.0\r\n",
                 status_code, (status_code == 200) ? "OK" : "Not Found", mime_type);
    } else {
        // 使用Content-Length
        header_len = snprintf(header, sizeof(header),
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %ld\r\n"
                 "Server: ANX HTTP Server/0.8.0\r\n",
                 status_code, (status_code == 200) ? "OK" : "Not Found", mime_type,
                 final_content_length);
    }
    
    // 添加压缩相关头部
    if (should_compress) {
        header_len += snprintf(header + header_len, sizeof(header) - header_len,
                              "Content-Encoding: gzip\r\n");
        if (compress_config && compress_config->enable_vary) {
            header_len += snprintf(header + header_len, sizeof(header) - header_len,
                                  "Vary: Accept-Encoding\r\n");
        }
    }
    
    header_len += snprintf(header + header_len, sizeof(header) - header_len,
                          "Connection: close\r\n\r\n");

    // 创建头部处理上下文
    header_context_t *header_ctx = NULL;
    if (route.location) {
        header_ctx = create_header_context(route.location->directives, route.location->directive_count);
    }
    if (!header_ctx && route.server) {
        header_ctx = create_header_context(route.server->directives, route.server->directive_count);
    }

    // 检查是否需要带宽限制
    bandwidth_controller_t *bandwidth_ctrl = NULL;
    bandwidth_config_t *bandwidth_config = NULL;
    if (core_conf && core_conf->raw_config && core_conf->raw_config->bandwidth) {
        bandwidth_config = core_conf->raw_config->bandwidth;
        
        // 检查是否启用带宽限制并且文件大小满足条件
        if (bandwidth_config->enable_bandwidth_limit && 
            (size_t)file_stat.st_size >= bandwidth_config->min_file_size) {
            
            // 查找匹配的带宽限制规则
            bandwidth_rule_t *rule = bandwidth_config_find_rule(bandwidth_config, 
                                                              file_path, mime_type, 
                                                              get_client_ip(client_socket));
            
            size_t rate_limit = bandwidth_config->default_rate_limit;
            size_t burst_size = bandwidth_config->default_burst_size;
            
            if (rule) {
                rate_limit = rule->rate_limit;
                burst_size = rule->burst_size;
            }
            
            // 创建带宽控制器
            bandwidth_ctrl = bandwidth_controller_create(rate_limit, burst_size);
            if (bandwidth_ctrl) {
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg), "Applying bandwidth limit: %zu B/s, burst: %zu B", 
                         rate_limit, burst_size);
                log_message(LOG_LEVEL_DEBUG, log_msg);
            }
        }
    }

    // 发送响应
    if (use_chunked_encoding) {
        // 使用分块传输编码
        chunked_context_t *chunked_ctx = chunked_context_create(client_socket, NULL);
        if (chunked_ctx) {
            // 应用头部操作
            if (header_ctx) {
                apply_headers_to_response(header, sizeof(header), header_ctx, status_code, mime_type, final_content_length);
                free_header_context(header_ctx);
                header_ctx = NULL; // 避免重复释放
            }
            
            // 发送头部（不包含Transfer-Encoding，因为chunked模块会处理）
            if (bandwidth_ctrl) {
                bandwidth_controlled_send(client_socket, header, strlen(header), bandwidth_ctrl);
            } else {
                if (write(client_socket, header, strlen(header)) < 0) {
                    log_message(LOG_LEVEL_ERROR, "Failed to write chunked header to client");
                }
            }
            
            // 创建分块传输编码配置
            chunked_config_t *chunked_config = chunked_get_default_config();
            if (chunked_config) {
                chunked_config->enable_trailer = true;
                
                if (should_compress && compressed_data) {
                    // 发送压缩后的数据（分块方式）
                    const char *trailer = "X-Content-Encoding: gzip\r\n";
                    chunked_send_chunk(chunked_ctx, (const char *)compressed_data, compressed_size);
                    chunked_send_final_chunk(chunked_ctx, trailer);
                    free(compressed_data);
                } else {
                    // 发送原始文件（分块方式）
                    chunked_send_file_stream(chunked_ctx, file_fd, file_stat.st_size, chunked_config);
                }
                
                chunked_config_free(chunked_config);
            }
            
            chunked_context_free(chunked_ctx);
        }
    } else {
        // 使用传统的Content-Length方式
        
        // 应用头部操作
        if (header_ctx) {
            apply_headers_to_response(header, sizeof(header), header_ctx, status_code, mime_type, final_content_length);
            free_header_context(header_ctx);
            header_ctx = NULL; // 避免重复释放
        }
        
        // 发送头部
        if (bandwidth_ctrl) {
            bandwidth_controlled_send(client_socket, header, strlen(header), bandwidth_ctrl);
        } else {
            if (write(client_socket, header, strlen(header)) < 0) {
                log_message(LOG_LEVEL_ERROR, "Failed to write header to client");
            }
        }
        
        if (should_compress && compressed_data) {
            // 发送压缩后的数据
            if (bandwidth_ctrl) {
                bandwidth_controlled_send(client_socket, (const char *)compressed_data, compressed_size, bandwidth_ctrl);
            } else {
                if (write(client_socket, compressed_data, compressed_size) < 0) {
                    log_message(LOG_LEVEL_ERROR, "Failed to write compressed data to client");
                }
            }
            free(compressed_data);
        } else {
            // 发送原始文件
            lseek(file_fd, 0, SEEK_SET); // 重置文件指针
            if (bandwidth_ctrl) {
                bandwidth_controlled_sendfile(client_socket, file_fd, NULL, file_stat.st_size, bandwidth_ctrl);
            } else {
                sendfile(client_socket, file_fd, NULL, file_stat.st_size);
            }
        }
    }
    
    // 清理带宽控制器
    if (bandwidth_ctrl) {
        bandwidth_controller_free(bandwidth_ctrl);
    }
    
    // 将内容添加到缓存
    if (core_conf->cache_manager && method && strcmp(method, "GET") == 0 && 
        status_code == 200 && file_stat.st_size > 0) {
        
        if (cache_config_is_cacheable(core_conf->raw_config->cache, mime_type, file_stat.st_size)) {
            // 读取文件内容用于缓存
            char *file_content_for_cache = malloc(file_stat.st_size);
            if (file_content_for_cache) {
                lseek(file_fd, 0, SEEK_SET);
                if (read(file_fd, file_content_for_cache, file_stat.st_size) == file_stat.st_size) {
                    // 存储到缓存（如果已压缩则存储压缩版本）
                    if (should_compress && compressed_data) {
                        cache_put(core_conf->cache_manager, req_path, 
                                 (char *)compressed_data, compressed_size, 
                                 mime_type, file_stat.st_mtime, 0, true);
                    } else {
                        cache_put(core_conf->cache_manager, req_path, 
                                 file_content_for_cache, file_stat.st_size, 
                                 mime_type, file_stat.st_mtime, 0, false);
                    }
                }
                free(file_content_for_cache);
            }
        }
    }
    
    // 清理资源
    if (compress_ctx) {
        compress_context_free(compress_ctx);
    }
    if (accept_encoding) {
        free(accept_encoding);
    }
    if (header_ctx) {
        free_header_context(header_ctx);
    }
    close(file_fd);
    
    // Log the access entry
    if (access_entry) {
        access_entry->status_code = status_code;
        access_entry->response_size = strlen(header) + file_stat.st_size;
        
        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        access_entry->request_duration_ms = 
            (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
            (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        
        log_access_entry(access_entry);
        free_access_log_entry(access_entry);
    }
    
    // Clean up
    free(host);
    free(buffer_copy);
    free(user_agent);
    free(referer);
    if (if_none_match) free(if_none_match);
    if (if_modified_since_str) free(if_modified_since_str);
    if (cached_response) cache_response_free(cached_response);
    close(client_socket);
} 