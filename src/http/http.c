#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "http.h"
#include "config.h"
#include "core.h"
#include "log.h"
#include "cache.h"
#include "proxy.h"
#include "compress.h"
#include "health_check.h"
#include "../utils/asm/asm_opt.h"
#include "../utils/asm/asm_mempool.h"
#include "../utils/asm/asm_integration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFER_SIZE 8192
#define TEMP_DEFAULT_PAGE "/test.html"
#define TEMP_NOT_FOUND_PAGE "/404.html"

// 全局内存池管理器
static mempool_manager_t* global_mempool = NULL;

// 初始化HTTP模块
void http_module_init(void) {
    // 初始化汇编优化
    asm_integration_init();
    
    // 初始化全局内存池
    global_mempool = mempool_manager_create();
    if (!global_mempool) {
        log_message(LOG_LEVEL_WARNING, "Failed to create global memory pool, using standard malloc");
    }
    
    log_message(LOG_LEVEL_INFO, "HTTP module initialized with assembly optimizations");
}

// 清理HTTP模块
void http_module_cleanup(void) {
    if (global_mempool) {
        mempool_manager_destroy(global_mempool);
        global_mempool = NULL;
    }
    
    asm_integration_cleanup();
    log_message(LOG_LEVEL_INFO, "HTTP module cleaned up");
}

// 优化的HTTP请求解析
static int parse_http_request_optimized(const char* buffer, size_t len, 
                                      char** method, char** path, char** version) {
    if (!buffer || len == 0) return -1;
    
    // 使用汇编优化的字符串搜索
    const char* method_end = asm_opt_strchr(buffer, ' ');
    if (!method_end || method_end >= buffer + len) return -1;
    
    const char* path_start = method_end + 1;
    const char* path_end = asm_opt_strchr(path_start, ' ');
    if (!path_end || path_end >= buffer + len) return -1;
    
    const char* version_start = path_end + 1;
    const char* version_end = asm_opt_strstr(version_start, "\r\n");
    if (!version_end || version_end >= buffer + len) return -1;
    
    // 计算长度
    size_t method_len = method_end - buffer;
    size_t path_len = path_end - path_start;
    size_t version_len = version_end - version_start;
    
    // 使用内存池分配
    if (global_mempool) {
        *method = mempool_manager_alloc(global_mempool, method_len + 1);
        *path = mempool_manager_alloc(global_mempool, path_len + 1);
        *version = mempool_manager_alloc(global_mempool, version_len + 1);
    } else {
        *method = malloc(method_len + 1);
        *path = malloc(path_len + 1);
        *version = malloc(version_len + 1);
    }
    
    if (!*method || !*path || !*version) {
        // 清理已分配的内存
        if (*method) mempool_manager_free(global_mempool, *method);
        if (*path) mempool_manager_free(global_mempool, *path);
        if (*version) mempool_manager_free(global_mempool, *version);
        return -1;
    }
    
    // 使用汇编优化的内存拷贝
    asm_opt_memcpy(*method, buffer, method_len);
    (*method)[method_len] = '\0';
    
    asm_opt_memcpy(*path, path_start, path_len);
    (*path)[path_len] = '\0';
    
    asm_opt_memcpy(*version, version_start, version_len);
    (*version)[version_len] = '\0';
    
    return 0;
}

// 优化的HTTP头部提取
static char* extract_header_value_optimized(const char* buffer, const char* header_name) {
    if (!buffer || !header_name) return NULL;
    
    size_t header_len = asm_opt_strlen(header_name);
    const char* ptr = buffer;
    
    while (*ptr) {
        // 使用汇编优化的字符串比较
        if (asm_opt_strncmp(ptr, header_name, header_len) == 0 && ptr[header_len] == ':') {
            ptr += header_len + 1;
            
            // 跳过空格
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            
            const char* value_start = ptr;
            while (*ptr && *ptr != '\r' && *ptr != '\n') ptr++;
            
            size_t value_len = ptr - value_start;
            if (value_len > 0) {
                char* value = global_mempool ? 
                    mempool_manager_alloc(global_mempool, value_len + 1) : 
                    malloc(value_len + 1);
                
                if (value) {
                    asm_opt_memcpy(value, value_start, value_len);
                    value[value_len] = '\0';
                    return value;
                }
            }
        }
        
        // 查找下一行
        ptr = asm_opt_strstr(ptr, "\r\n");
        if (!ptr) break;
        ptr += 2;
    }
    
    return NULL;
}

// 优化的MIME类型检测
static const char* get_mime_type_optimized(const char* path) {
    if (!path) return "application/octet-stream";
    
    // 使用汇编优化的字符串搜索
    const char* ext = asm_opt_strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    ext++; // 跳过点号
    
    // 使用汇编优化的字符串比较
    if (asm_opt_strcmp(ext, "html") == 0 || asm_opt_strcmp(ext, "htm") == 0)
        return "text/html";
    if (asm_opt_strcmp(ext, "css") == 0)
        return "text/css";
    if (asm_opt_strcmp(ext, "js") == 0)
        return "application/javascript";
    if (asm_opt_strcmp(ext, "json") == 0)
        return "application/json";
    if (asm_opt_strcmp(ext, "png") == 0)
        return "image/png";
    if (asm_opt_strcmp(ext, "jpg") == 0 || asm_opt_strcmp(ext, "jpeg") == 0)
        return "image/jpeg";
    if (asm_opt_strcmp(ext, "gif") == 0)
        return "image/gif";
    if (asm_opt_strcmp(ext, "ico") == 0)
        return "image/x-icon";
    if (asm_opt_strcmp(ext, "svg") == 0)
        return "image/svg+xml";
    if (asm_opt_strcmp(ext, "woff") == 0)
        return "font/woff";
    if (asm_opt_strcmp(ext, "woff2") == 0)
        return "font/woff2";
    if (asm_opt_strcmp(ext, "ttf") == 0)
        return "font/ttf";
    if (asm_opt_strcmp(ext, "eot") == 0)
        return "application/vnd.ms-fontobject";
    if (asm_opt_strcmp(ext, "otf") == 0)
        return "font/otf";
    if (asm_opt_strcmp(ext, "pdf") == 0)
        return "application/pdf";
    if (asm_opt_strcmp(ext, "zip") == 0)
        return "application/zip";
    if (asm_opt_strcmp(ext, "gz") == 0)
        return "application/gzip";
    if (asm_opt_strcmp(ext, "tar") == 0)
        return "application/x-tar";
    if (asm_opt_strcmp(ext, "xml") == 0)
        return "application/xml";
    if (asm_opt_strcmp(ext, "txt") == 0)
        return "text/plain";
    if (asm_opt_strcmp(ext, "md") == 0)
        return "text/markdown";
    if (asm_opt_strcmp(ext, "mp4") == 0)
        return "video/mp4";
    if (asm_opt_strcmp(ext, "webm") == 0)
        return "video/webm";
    if (asm_opt_strcmp(ext, "mp3") == 0)
        return "audio/mpeg";
    if (asm_opt_strcmp(ext, "wav") == 0)
        return "audio/wav";
    if (asm_opt_strcmp(ext, "ogg") == 0)
        return "audio/ogg";
    
    return "application/octet-stream";
}

// 优化的零拷贝文件发送
static int send_file_optimized(int client_socket, const char* file_path, 
                              const char* mime_type, size_t file_size) {
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) return -1;
    
    // 构建HTTP响应头
    char header[BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Server: ANX HTTP Server/1.1.0+\r\n"
        "Accept-Ranges: bytes\r\n"
        "Connection: close\r\n\r\n",
        mime_type, file_size);
    
    // 发送头部
    if (write(client_socket, header, header_len) < 0) {
        close(file_fd);
        return -1;
    }
    
    // 使用sendfile进行零拷贝传输
    off_t offset = 0;
    ssize_t sent = sendfile(client_socket, file_fd, &offset, file_size);
    
    close(file_fd);
    return (sent == (ssize_t)file_size) ? 0 : -1;
}

// 优化的HTTP请求处理主函数
void handle_http_request(int client_socket, const char* client_ip, core_config_t *core_conf) {
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    // 使用汇编优化的缓冲区
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = asm_opt_socket_recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    buffer[bytes_read] = '\0';

    // 使用优化的HTTP请求解析
    char *method = NULL, *req_path = NULL, *http_version = NULL;
    if (parse_http_request_optimized(buffer, bytes_read, &method, &req_path, &http_version) < 0) {
        close(client_socket);
        return;
    }

    // 使用优化的头部提取
    char *host = extract_header_value_optimized(buffer, "Host");
    char *user_agent = extract_header_value_optimized(buffer, "User-Agent");
    char *referer = extract_header_value_optimized(buffer, "Referer");
    char *if_none_match = extract_header_value_optimized(buffer, "If-None-Match");
    char *if_modified_since_str = extract_header_value_optimized(buffer, "If-Modified-Since");

    // 创建访问日志条目
    access_log_entry_t *access_entry = create_access_log_entry();
    if (access_entry) {
        access_entry->client_ip = strdup(client_ip);
        access_entry->method = strdup(method);
        access_entry->request_uri = strdup(req_path);
        access_entry->user_agent = user_agent ? strdup(user_agent) : NULL;
        access_entry->referer = referer ? strdup(referer) : NULL;
        access_entry->timestamp = time(NULL);
    }

    // 安全检查：防止目录遍历
    if (strstr(req_path, "..")) {
        log_message(LOG_LEVEL_ERROR, "Directory traversal attempt blocked");
        
        const char *response = "HTTP/1.1 403 Forbidden\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: 13\r\n"
                              "Connection: close\r\n\r\n"
                              "Access Denied";
        write(client_socket, response, strlen(response));
        
        if (access_entry) {
            access_entry->status_code = 403;
            access_entry->response_size = strlen(response);
            struct timeval end_time;
            gettimeofday(&end_time, NULL);
            access_entry->request_duration_ms = 
                (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                (end_time.tv_usec - start_time.tv_usec) / 1000.0;
            log_access_entry(access_entry);
            free_access_log_entry(access_entry);
        }
        
        close(client_socket);
        goto cleanup;
    }

    // 路由查找
    route_t route = find_route(core_conf, host, req_path, 80);
    
    // 检查是否是代理请求
    if (route.location && route.location->proxy_pass) {
        // 处理代理请求（保持原有逻辑）
        char *headers = NULL;
        int result = proxy_request(client_socket, req_path, route.location->proxy_pass, 
                                 buffer, bytes_read, &headers);
        
        if (result >= 0) {
            if (access_entry) {
                access_entry->status_code = 200;
                access_entry->response_size = result;
                struct timeval end_time;
                gettimeofday(&end_time, NULL);
                access_entry->request_duration_ms = 
                    (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                    (end_time.tv_usec - start_time.tv_usec) / 1000.0;
                log_access_entry(access_entry);
                free_access_log_entry(access_entry);
            }
        }
        
        if (headers) free(headers);
        close(client_socket);
        goto cleanup;
    }

    // 静态文件服务优化
    const char *root = get_directive_value("root", route.server->directives, route.server->directive_count);
    if (route.location) {
        const char* loc_root = get_directive_value("root", route.location->directives, route.location->directive_count);
        if (loc_root) root = loc_root;
    }
    if (!root) root = "./www";

    char file_path[BUFFER_SIZE];
    if (strcmp(req_path, "/") == 0) {
        // 处理根目录请求
        const char *index_directive = NULL;
        if (route.location) {
            index_directive = get_directive_value("index", route.location->directives, route.location->directive_count);
        }
        if (!index_directive && route.server) {
            index_directive = get_directive_value("index", route.server->directives, route.server->directive_count);
        }
        
        bool index_found = false;
        if (index_directive) {
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
            snprintf(file_path, sizeof(file_path), "%s%s", root, TEMP_DEFAULT_PAGE);
        }
    } else {
        snprintf(file_path, sizeof(file_path), "%s%s", root, req_path);
    }

    struct stat file_stat;
    if (stat(file_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
        // 文件不存在，返回404
        snprintf(file_path, sizeof(file_path), "%s%s", root, TEMP_NOT_FOUND_PAGE);
        stat(file_path, &file_stat);
        
        if (access_entry) {
            access_entry->status_code = 404;
        }
    }

    // 使用优化的MIME类型检测
    const char *mime_type = get_mime_type_optimized(file_path);
    
    // 使用零拷贝文件发送
    if (send_file_optimized(client_socket, file_path, mime_type, file_stat.st_size) == 0) {
        if (access_entry) {
            access_entry->status_code = 200;
            access_entry->response_size = file_stat.st_size;
            struct timeval end_time;
            gettimeofday(&end_time, NULL);
            access_entry->request_duration_ms = 
                (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                (end_time.tv_usec - start_time.tv_usec) / 1000.0;
            log_access_entry(access_entry);
            free_access_log_entry(access_entry);
        }
    } else {
        // 发送错误响应
        const char *response = "HTTP/1.1 500 Internal Server Error\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: 21\r\n"
                              "Connection: close\r\n\r\n"
                              "Internal Server Error";
        write(client_socket, response, strlen(response));
        
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
    }

cleanup:
    // 使用内存池释放内存
    if (method) mempool_manager_free(global_mempool, method);
    if (req_path) mempool_manager_free(global_mempool, req_path);
    if (http_version) mempool_manager_free(global_mempool, http_version);
    if (host) mempool_manager_free(global_mempool, host);
    if (user_agent) mempool_manager_free(global_mempool, user_agent);
    if (referer) mempool_manager_free(global_mempool, referer);
    if (if_none_match) mempool_manager_free(global_mempool, if_none_match);
    if (if_modified_since_str) mempool_manager_free(global_mempool, if_modified_since_str);
    
    close(client_socket);
} 