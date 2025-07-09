#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "headers.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// 辅助函数：添加头部操作到集合
static void add_header_operation(header_operations_t *ops, header_operation_type_t type,
                                const char *name, const char *value, bool always) {
    if (!ops) return;
    
    // 扩容检查
    if (ops->count >= ops->capacity) {
        ops->capacity = ops->capacity == 0 ? 4 : ops->capacity * 2;
        ops->operations = realloc(ops->operations, sizeof(header_operation_t) * ops->capacity);
    }
    
    header_operation_t *op = &ops->operations[ops->count];
    op->type = type;
    op->name = strdup(name);
    op->value = value ? strdup(value) : NULL;
    op->always = always;
    ops->count++;
}

// 解析头部操作指令
header_operations_t *parse_header_operations(const directive_t *directives, int count) {
    header_operations_t *ops = calloc(1, sizeof(header_operations_t));
    if (!ops) return NULL;
    
    for (int i = 0; i < count; i++) {
        const directive_t *dir = &directives[i];
        
        if (strcmp(dir->key, "add_header") == 0) {
            // 格式: add_header name value [always]
            char *value_copy = strdup(dir->value);
            char *name = strtok(value_copy, " \t");
            char *value = strtok(NULL, " \t");
            char *always_flag = strtok(NULL, " \t");
            
            if (name && value) {
                bool always = (always_flag && strcmp(always_flag, "always") == 0);
                add_header_operation(ops, HEADER_ADD, name, value, always);
            }
            free(value_copy);
        }
        else if (strcmp(dir->key, "set_header") == 0) {
            // 格式: set_header name value [always]
            char *value_copy = strdup(dir->value);
            char *name = strtok(value_copy, " \t");
            char *value = strtok(NULL, " \t");
            char *always_flag = strtok(NULL, " \t");
            
            if (name && value) {
                bool always = (always_flag && strcmp(always_flag, "always") == 0);
                add_header_operation(ops, HEADER_SET, name, value, always);
            }
            free(value_copy);
        }
        else if (strcmp(dir->key, "remove_header") == 0) {
            // 格式: remove_header name [always]
            char *value_copy = strdup(dir->value);
            char *name = strtok(value_copy, " \t");
            char *always_flag = strtok(NULL, " \t");
            
            if (name) {
                bool always = (always_flag && strcmp(always_flag, "always") == 0);
                add_header_operation(ops, HEADER_REMOVE, name, NULL, always);
            }
            free(value_copy);
        }
    }
    
    return ops;
}

// 创建头部处理上下文
header_context_t *create_header_context(const directive_t *directives, int count) {
    header_context_t *ctx = calloc(1, sizeof(header_context_t));
    if (!ctx) return NULL;
    
    ctx->operations = parse_header_operations(directives, count);
    
    return ctx;
}

// 获取当前日期时间字符串（用于Date头部）
char *get_current_date_string() {
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    
    char *date_str = malloc(64);
    if (!date_str) return NULL;
    
    strftime(date_str, 64, "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return date_str;
}

// 应用头部操作到HTTP响应
void apply_headers_to_response(char *response_buffer, size_t buffer_size, 
                              const header_context_t *context, 
                              int status_code, const char *content_type, 
                              long content_length) {
    if (!context || !context->operations) return;
    
    // 检查是否是错误状态码
    bool is_error = (status_code >= 400);
    
    // 应用自定义头部操作
    for (int i = 0; i < context->operations->count; i++) {
        const header_operation_t *op = &context->operations->operations[i];
        
        // 检查是否应该应用此操作
        if (is_error && !op->always) continue;
        
        // 简单实现：在响应头部结束前添加头部
        if (op->type == HEADER_ADD || op->type == HEADER_SET) {
            if (op->name && op->value) {
                char *headers_end = strstr(response_buffer, "\r\n\r\n");
                if (headers_end) {
                    char header_line[512];
                    snprintf(header_line, sizeof(header_line), "%s: %s\r\n", 
                            op->name, op->value);
                    
                    // 检查缓冲区空间
                    size_t current_len = strlen(response_buffer);
                    size_t header_len = strlen(header_line);
                    
                    if (current_len + header_len < buffer_size) {
                        // 找到插入位置：在第一个\r\n\r\n的第二个\r\n之前
                        char *insert_pos = headers_end + 2; // 跳过第一个\r\n
                        size_t remaining_len = strlen(insert_pos);
                        
                        // 移动从insert_pos到字符串末尾的所有内容，为新头部腾出空间
                        memmove(insert_pos + header_len, insert_pos, remaining_len + 1);
                        // 插入新头部
                        memcpy(insert_pos, header_line, header_len);
                        
                        // 调试信息
                        char log_msg[256];
                        snprintf(log_msg, sizeof(log_msg), "Added header: %s: %s", op->name, op->value);
                        log_message(LOG_LEVEL_DEBUG, log_msg);
                    }
                }
            }
        }
    }
}

// 应用头部操作到代理请求
void apply_headers_to_proxy_request(char *request_buffer, size_t buffer_size,
                                   const header_context_t *context,
                                   const char *method, const char *path) {
    if (!context || !context->operations) return;
    
    // 为代理请求应用头部操作
    for (int i = 0; i < context->operations->count; i++) {
        const header_operation_t *op = &context->operations->operations[i];
        
        // 对于代理请求，主要处理添加和设置操作
        if (op->type == HEADER_ADD || op->type == HEADER_SET) {
            if (op->name && op->value) {
                // 在请求结束标记前添加头部
                char *headers_end = strstr(request_buffer, "\r\n\r\n");
                if (headers_end) {
                    char header_line[512];
                    snprintf(header_line, sizeof(header_line), "%s: %s\r\n", 
                            op->name, op->value);
                    
                    // 检查缓冲区空间
                    size_t header_len = strlen(header_line);
                    if (strlen(request_buffer) + header_len < buffer_size) {
                        memmove(headers_end + header_len, headers_end, 
                               strlen(headers_end) + 1);
                        memcpy(headers_end, header_line, header_len);
                    }
                }
            }
        }
    }
}

// 处理响应头部（用于代理响应）
void process_proxy_response_headers(char *response_buffer, size_t buffer_size,
                                   const header_context_t *context) {
    if (!context) return;
    
    // 提取状态码
    int status_code = 200;
    if (strncmp(response_buffer, "HTTP/", 5) == 0) {
        sscanf(response_buffer, "HTTP/%*s %d", &status_code);
    }
    
    // 应用头部操作到代理响应
    apply_headers_to_response(response_buffer, buffer_size, context, 
                             status_code, NULL, 0);
}

// 解析安全头部配置
security_headers_t *parse_security_headers(const directive_t *directives, int count) {
    security_headers_t *sec = calloc(1, sizeof(security_headers_t));
    if (!sec) return NULL;
    
    for (int i = 0; i < count; i++) {
        const directive_t *dir = &directives[i];
        
        if (strcmp(dir->key, "enable_hsts") == 0) {
            sec->enable_hsts = (strcmp(dir->value, "on") == 0);
        }
        else if (strcmp(dir->key, "hsts_value") == 0) {
            sec->hsts_value = strdup(dir->value);
        }
        else if (strcmp(dir->key, "enable_xframe_options") == 0) {
            sec->enable_xframe_options = (strcmp(dir->value, "on") == 0);
        }
        else if (strcmp(dir->key, "xframe_options_value") == 0) {
            sec->xframe_options_value = strdup(dir->value);
        }
        else if (strcmp(dir->key, "enable_xcontent_type_options") == 0) {
            sec->enable_xcontent_type_options = (strcmp(dir->value, "on") == 0);
        }
        else if (strcmp(dir->key, "enable_xss_protection") == 0) {
            sec->enable_xss_protection = (strcmp(dir->value, "on") == 0);
        }
        else if (strcmp(dir->key, "enable_referrer_policy") == 0) {
            sec->enable_referrer_policy = (strcmp(dir->value, "on") == 0);
        }
        else if (strcmp(dir->key, "referrer_policy_value") == 0) {
            sec->referrer_policy_value = strdup(dir->value);
        }
        else if (strcmp(dir->key, "enable_content_security_policy") == 0) {
            sec->enable_content_security_policy = (strcmp(dir->value, "on") == 0);
        }
        else if (strcmp(dir->key, "csp_value") == 0) {
            sec->csp_value = strdup(dir->value);
        }
    }
    
    return sec;
}

// 解析标准头部配置
standard_headers_t *parse_standard_headers(const directive_t *directives, int count) {
    standard_headers_t *std = calloc(1, sizeof(standard_headers_t));
    if (!std) return NULL;
    
    // 默认启用
    std->enable_server_header = true;
    std->enable_date_header = true;
    std->enable_connection_header = true;
    
    for (int i = 0; i < count; i++) {
        const directive_t *dir = &directives[i];
        
        if (strcmp(dir->key, "enable_server_header") == 0) {
            std->enable_server_header = (strcmp(dir->value, "on") == 0);
        }
        else if (strcmp(dir->key, "server_value") == 0) {
            std->server_value = strdup(dir->value);
        }
        else if (strcmp(dir->key, "enable_date_header") == 0) {
            std->enable_date_header = (strcmp(dir->value, "on") == 0);
        }
        else if (strcmp(dir->key, "enable_connection_header") == 0) {
            std->enable_connection_header = (strcmp(dir->value, "on") == 0);
        }
        else if (strcmp(dir->key, "connection_value") == 0) {
            std->connection_value = strdup(dir->value);
        }
    }
    
    return std;
}

// 释放头部操作集合
void free_header_operations(header_operations_t *operations) {
    if (!operations) return;
    
    for (int i = 0; i < operations->count; i++) {
        free(operations->operations[i].name);
        free(operations->operations[i].value);
    }
    free(operations->operations);
    free(operations);
}

// 释放安全头部配置
void free_security_headers(security_headers_t *security) {
    if (!security) return;
    
    free(security->hsts_value);
    free(security->xframe_options_value);
    free(security->referrer_policy_value);
    free(security->csp_value);
    free(security);
}

// 释放标准头部配置
void free_standard_headers(standard_headers_t *standard) {
    if (!standard) return;
    
    free(standard->server_value);
    free(standard->connection_value);
    free(standard);
}

// 释放头部处理上下文
void free_header_context(header_context_t *context) {
    if (!context) return;
    
    free_header_operations(context->operations);
    free_security_headers(context->security);
    free_standard_headers(context->standard);
    free(context);
}