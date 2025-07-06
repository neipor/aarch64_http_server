#ifndef HEALTH_API_H
#define HEALTH_API_H

#include "health_check.h"
#include "load_balancer.h"
#include <stdio.h>

// API响应格式
typedef enum {
    HEALTH_API_FORMAT_JSON,
    HEALTH_API_FORMAT_TEXT,
    HEALTH_API_FORMAT_XML
} health_api_format_t;

// API请求结构
typedef struct health_api_request {
    char *path;                    // 请求路径
    char *method;                  // HTTP方法
    char *query_string;            // 查询字符串
    health_api_format_t format;    // 响应格式
    int detailed;                  // 是否返回详细信息
    char *upstream_name;           // 指定的upstream名称
    char *server_host;             // 指定的服务器主机
    int server_port;               // 指定的服务器端口
} health_api_request_t;

// API响应结构
typedef struct health_api_response {
    int status_code;               // HTTP状态码
    char *content_type;            // 内容类型
    char *body;                    // 响应体
    size_t body_size;              // 响应体大小
    time_t timestamp;              // 响应时间戳
} health_api_response_t;

// 健康检查状态汇总
typedef struct health_status_summary {
    int total_servers;             // 总服务器数
    int healthy_servers;           // 健康服务器数
    int unhealthy_servers;         // 不健康服务器数
    int checking_servers;          // 检查中服务器数
    int unknown_servers;           // 未知状态服务器数
    double overall_uptime;         // 整体可用性
    time_t last_updated;           // 最后更新时间
} health_status_summary_t;

// 函数声明

// API请求处理
health_api_response_t *health_api_handle_request(health_api_request_t *request, lb_config_t *lb_config);
health_api_request_t *health_api_parse_request(const char *path, const char *method, const char *query_string);
void health_api_request_free(health_api_request_t *request);

// API响应管理
health_api_response_t *health_api_response_create(void);
void health_api_response_free(health_api_response_t *response);
int health_api_response_set_body(health_api_response_t *response, const char *body);

// 健康状态查询
health_api_response_t *health_api_get_overall_status(lb_config_t *lb_config, health_api_format_t format);
health_api_response_t *health_api_get_upstream_status(upstream_group_t *group, health_api_format_t format);
health_api_response_t *health_api_get_server_status(upstream_server_t *server, health_api_format_t format);
health_api_response_t *health_api_get_server_history(upstream_server_t *server, health_api_format_t format);

// 健康检查控制
health_api_response_t *health_api_force_check(upstream_server_t *server, health_api_format_t format);
health_api_response_t *health_api_enable_check(upstream_server_t *server, health_api_format_t format);
health_api_response_t *health_api_disable_check(upstream_server_t *server, health_api_format_t format);

// 状态汇总
health_status_summary_t *health_api_get_status_summary(lb_config_t *lb_config);
void health_status_summary_free(health_status_summary_t *summary);

// JSON格式化
char *health_api_format_json_status(upstream_group_t *group);
char *health_api_format_json_server(upstream_server_t *server);
char *health_api_format_json_summary(health_status_summary_t *summary);

// 文本格式化
char *health_api_format_text_status(upstream_group_t *group);
char *health_api_format_text_server(upstream_server_t *server);
char *health_api_format_text_summary(health_status_summary_t *summary);

// API路由
typedef struct health_api_route {
    char *path_pattern;            // 路径模式
    char *method;                  // HTTP方法
    health_api_response_t *(*handler)(health_api_request_t *, lb_config_t *); // 处理函数
} health_api_route_t;

// 路由管理
health_api_route_t *health_api_get_routes(void);
int health_api_get_route_count(void);
health_api_route_t *health_api_match_route(const char *path, const char *method);

// 工具函数
int health_api_path_matches(const char *pattern, const char *path);
char *health_api_extract_path_param(const char *pattern, const char *path, const char *param_name);
char *health_api_get_query_param(const char *query_string, const char *param_name);

// 处理器函数声明
health_api_response_t *health_api_get_overall_status_handler(health_api_request_t *request, lb_config_t *lb_config);
health_api_response_t *health_api_get_upstream_status_handler(health_api_request_t *request, lb_config_t *lb_config);
health_api_response_t *health_api_get_server_status_handler(health_api_request_t *request, lb_config_t *lb_config);
health_api_response_t *health_api_get_server_history_handler(health_api_request_t *request, lb_config_t *lb_config);
health_api_response_t *health_api_force_check_handler(health_api_request_t *request, lb_config_t *lb_config);
health_api_response_t *health_api_enable_check_handler(health_api_request_t *request, lb_config_t *lb_config);
health_api_response_t *health_api_disable_check_handler(health_api_request_t *request, lb_config_t *lb_config);

#endif // HEALTH_API_H 