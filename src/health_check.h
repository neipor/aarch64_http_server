#ifndef HEALTH_CHECK_H
#define HEALTH_CHECK_H

#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include "load_balancer.h"

// 健康检查类型枚举
typedef enum {
    HEALTH_CHECK_HTTP,          // HTTP健康检查
    HEALTH_CHECK_HTTPS,         // HTTPS健康检查
    HEALTH_CHECK_TCP,           // TCP连接检查
    HEALTH_CHECK_PING,          // ICMP ping检查
    HEALTH_CHECK_CUSTOM         // 自定义检查
} health_check_type_t;

// 健康检查状态枚举
typedef enum {
    HEALTH_STATUS_HEALTHY,      // 健康
    HEALTH_STATUS_UNHEALTHY,    // 不健康
    HEALTH_STATUS_CHECKING,     // 检查中
    HEALTH_STATUS_UNKNOWN,      // 未知状态
    HEALTH_STATUS_DEGRADED      // 降级状态
} health_status_t;

// 健康检查结果
typedef struct health_check_result {
    health_status_t status;     // 检查结果状态
    int response_code;          // HTTP响应码
    double response_time;       // 响应时间（毫秒）
    char *error_message;        // 错误信息
    time_t check_time;          // 检查时间
    bool is_timeout;            // 是否超时
    size_t response_size;       // 响应大小
} health_check_result_t;

// 健康检查配置
typedef struct health_check_config {
    health_check_type_t type;   // 检查类型
    char *uri;                  // 检查URI
    char *method;               // HTTP方法
    char *expected_response;    // 期望的响应内容
    char *headers;              // 自定义头部
    int timeout;                // 超时时间（秒）
    int interval;               // 检查间隔（秒）
    int retries;                // 重试次数
    int rise;                   // 连续成功次数后标记为健康
    int fall;                   // 连续失败次数后标记为不健康
    bool enabled;               // 是否启用
    char *user_agent;           // User-Agent
    int port;                   // 检查端口（如果与服务端口不同）
} health_check_config_t;

// 健康检查历史记录
typedef struct health_check_history {
    health_check_result_t *results;  // 历史结果数组
    int capacity;                    // 数组容量
    int count;                       // 当前记录数
    int index;                       // 当前索引（循环缓冲区）
    pthread_mutex_t mutex;           // 互斥锁
} health_check_history_t;

// 健康检查管理器
typedef struct health_check_manager {
    upstream_group_t *group;         // 关联的upstream组
    health_check_config_t *config;   // 健康检查配置
    health_check_history_t *history; // 历史记录
    pthread_t check_thread;          // 检查线程
    bool running;                    // 是否运行中
    int consecutive_successes;       // 连续成功次数
    int consecutive_failures;        // 连续失败次数
    time_t last_check_time;         // 最后检查时间
    time_t next_check_time;         // 下次检查时间
    pthread_mutex_t mutex;          // 互斥锁
} health_check_manager_t;

// 健康检查统计信息
typedef struct health_check_stats {
    int total_checks;               // 总检查次数
    int successful_checks;          // 成功检查次数
    int failed_checks;              // 失败检查次数
    int timeout_checks;             // 超时检查次数
    double avg_response_time;       // 平均响应时间
    double min_response_time;       // 最小响应时间
    double max_response_time;       // 最大响应时间
    time_t last_success_time;       // 最后成功时间
    time_t last_failure_time;       // 最后失败时间
    health_status_t current_status; // 当前状态
    int uptime_percentage;          // 可用性百分比
} health_check_stats_t;

// 函数声明

// 健康检查配置管理
health_check_config_t *health_check_config_create(void);
void health_check_config_free(health_check_config_t *config);
int health_check_config_set_uri(health_check_config_t *config, const char *uri);
int health_check_config_set_method(health_check_config_t *config, const char *method);
int health_check_config_set_headers(health_check_config_t *config, const char *headers);
int health_check_config_set_expected_response(health_check_config_t *config, const char *response);

// 健康检查结果管理
health_check_result_t *health_check_result_create(void);
void health_check_result_free(health_check_result_t *result);
health_check_result_t *health_check_result_copy(const health_check_result_t *src);

// 健康检查历史记录
health_check_history_t *health_check_history_create(int capacity);
void health_check_history_free(health_check_history_t *history);
int health_check_history_add(health_check_history_t *history, const health_check_result_t *result);
health_check_result_t *health_check_history_get_latest(health_check_history_t *history);
health_check_result_t **health_check_history_get_all(health_check_history_t *history, int *count);

// 健康检查管理器
health_check_manager_t *health_check_manager_create(upstream_group_t *group, health_check_config_t *config);
void health_check_manager_free(health_check_manager_t *manager);
int health_check_manager_start(health_check_manager_t *manager);
int health_check_manager_stop(health_check_manager_t *manager);
bool health_check_manager_is_running(health_check_manager_t *manager);

// 健康检查执行
health_check_result_t *health_check_execute(upstream_server_t *server, health_check_config_t *config);
health_check_result_t *health_check_http(upstream_server_t *server, health_check_config_t *config);
health_check_result_t *health_check_https(upstream_server_t *server, health_check_config_t *config);
health_check_result_t *health_check_tcp(upstream_server_t *server, health_check_config_t *config);
health_check_result_t *health_check_ping(upstream_server_t *server, health_check_config_t *config);

// 健康检查线程
void *health_check_thread_worker(void *arg);
void health_check_process_result(health_check_manager_t *manager, health_check_result_t *result);
void health_check_update_server_status(upstream_server_t *server, health_check_result_t *result, 
                                     health_check_config_t *config);

// 健康检查统计
health_check_stats_t *health_check_get_stats(health_check_manager_t *manager);
void health_check_stats_free(health_check_stats_t *stats);
void health_check_print_stats(health_check_manager_t *manager);

// 健康检查API
int health_check_api_get_status(upstream_group_t *group, char *response, size_t response_size);
int health_check_api_get_history(upstream_server_t *server, char *response, size_t response_size);
int health_check_api_force_check(upstream_server_t *server);

// 工具函数
const char *health_status_to_string(health_status_t status);
const char *health_check_type_to_string(health_check_type_t type);
double health_check_calculate_uptime(health_check_history_t *history, time_t duration);
bool health_check_is_response_valid(const char *response, const char *expected);

// 配置解析
int health_check_parse_config(const char *config_str, health_check_config_t *config);
int health_check_parse_upstream_health_config(const char *block_content, upstream_group_t *group);

#endif // HEALTH_CHECK_H 