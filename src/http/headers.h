#ifndef HEADERS_H
#define HEADERS_H

#include <stdbool.h>
#include <openssl/ssl.h>
#include "core.h"

// 头部操作类型
typedef enum {
    HEADER_ADD,      // 添加头部
    HEADER_SET,      // 设置头部（覆盖现有值）
    HEADER_REMOVE    // 移除头部
} header_operation_type_t;

// 头部操作结构
typedef struct {
    header_operation_type_t type;
    char *name;      // 头部名称
    char *value;     // 头部值（对于REMOVE操作为NULL）
    bool always;     // 是否总是应用（即使在错误响应中）
} header_operation_t;

// 头部操作集合
typedef struct {
    header_operation_t *operations;
    int count;
    int capacity;
} header_operations_t;

// 安全头部配置
typedef struct {
    bool enable_hsts;
    char *hsts_value;
    bool enable_xframe_options;
    char *xframe_options_value;
    bool enable_xcontent_type_options;
    bool enable_xss_protection;
    bool enable_referrer_policy;
    char *referrer_policy_value;
    bool enable_content_security_policy;
    char *csp_value;
} security_headers_t;

// 标准头部配置
typedef struct {
    bool enable_server_header;
    char *server_value;
    bool enable_date_header;
    bool enable_connection_header;
    char *connection_value;
} standard_headers_t;

// 头部处理上下文
typedef struct {
    header_operations_t *operations;
    security_headers_t *security;
    standard_headers_t *standard;
} header_context_t;

// 解析头部操作指令
header_operations_t *parse_header_operations(const directive_t *directives, int count);

// 解析安全头部配置
security_headers_t *parse_security_headers(const directive_t *directives, int count);

// 解析标准头部配置
standard_headers_t *parse_standard_headers(const directive_t *directives, int count);

// 创建头部处理上下文
header_context_t *create_header_context(const directive_t *directives, int count);

// 应用头部操作到HTTP响应
void apply_headers_to_response(char *response_buffer, size_t buffer_size, 
                              const header_context_t *context, 
                              int status_code, const char *content_type, 
                              long content_length);

// 应用头部操作到代理请求
void apply_headers_to_proxy_request(char *request_buffer, size_t buffer_size,
                                   const header_context_t *context,
                                   const char *method, const char *path);

// 处理响应头部（用于代理响应）
void process_proxy_response_headers(char *response_buffer, size_t buffer_size,
                                   const header_context_t *context);

// 获取当前日期时间字符串（用于Date头部）
char *get_current_date_string();

// 释放头部操作集合
void free_header_operations(header_operations_t *operations);

// 释放安全头部配置
void free_security_headers(security_headers_t *security);

// 释放标准头部配置
void free_standard_headers(standard_headers_t *standard);

// 释放头部处理上下文
void free_header_context(header_context_t *context);

#endif // HEADERS_H 