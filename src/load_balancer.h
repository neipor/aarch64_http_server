#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include "core.h"

// 前向声明
typedef struct health_check_manager health_check_manager_t;

// 负载均衡策略枚举
typedef enum {
    LB_STRATEGY_ROUND_ROBIN,
    LB_STRATEGY_WEIGHTED_ROUND_ROBIN,
    LB_STRATEGY_LEAST_CONNECTIONS,
    LB_STRATEGY_IP_HASH,
    LB_STRATEGY_RANDOM,
    LB_STRATEGY_WEIGHTED_RANDOM
} lb_strategy_t;

// 服务器状态枚举
typedef enum {
    SERVER_STATUS_UP,
    SERVER_STATUS_DOWN,
    SERVER_STATUS_CHECKING,
    SERVER_STATUS_UNKNOWN
} server_status_t;

// 上游服务器结构
typedef struct upstream_server {
    char *host;                    // 服务器主机名或IP
    int port;                      // 服务器端口
    int weight;                    // 权重（默认为1）
    int max_fails;                 // 最大失败次数
    int fail_timeout;              // 失败超时时间（秒）
    int max_conns;                 // 最大连接数限制
    
    // 运行时状态
    server_status_t status;        // 服务器状态
    int current_connections;       // 当前连接数
    int total_requests;            // 总请求数
    int failed_requests;           // 失败请求数
    int consecutive_failures;      // 连续失败次数
    time_t last_failure_time;      // 最后失败时间
    time_t last_check_time;        // 最后检查时间
    
    // 健康检查相关
    int health_check_interval;     // 健康检查间隔（秒）
    char *health_check_uri;        // 健康检查URI
    int health_check_timeout;      // 健康检查超时时间
    
    // 统计信息
    double avg_response_time;      // 平均响应时间
    time_t last_response_time;     // 最后响应时间
    
    // 加权轮询相关
    int current_weight;            // 当前权重
    int effective_weight;          // 有效权重
    
    struct upstream_server *next;  // 链表指针
} upstream_server_t;

// 上游服务器组结构
typedef struct upstream_group {
    char *name;                    // 组名
    lb_strategy_t strategy;        // 负载均衡策略
    upstream_server_t *servers;    // 服务器列表
    int server_count;              // 服务器数量
    
    // 运行时状态
    int current_server_index;      // 当前服务器索引（轮询用）
    int total_weight;              // 总权重
    pthread_mutex_t mutex;         // 互斥锁
    
    // 会话保持
    int session_persistence;       // 是否启用会话保持
    int session_timeout;           // 会话超时时间
    
    // 健康检查配置
    int health_check_enabled;      // 是否启用健康检查
    int health_check_interval;     // 默认健康检查间隔
    int health_check_timeout;      // 默认健康检查超时
    char *health_check_uri;        // 默认健康检查URI
    
    // 健康检查管理器
    health_check_manager_t *health_manager; // 健康检查管理器
    
    // 统计信息
    int total_requests;            // 总请求数
    int successful_requests;       // 成功请求数
    int failed_requests;           // 失败请求数
    
    struct upstream_group *next;   // 链表指针
} upstream_group_t;

// 负载均衡配置结构
typedef struct lb_config {
    upstream_group_t *groups;      // 上游组列表
    int group_count;               // 组数量
    
    // 全局配置
    int default_max_fails;         // 默认最大失败次数
    int default_fail_timeout;      // 默认失败超时时间
    int default_health_check_interval; // 默认健康检查间隔
    int default_health_check_timeout;  // 默认健康检查超时
    
    pthread_mutex_t mutex;         // 全局互斥锁
} lb_config_t;

// 负载均衡选择结果
typedef struct lb_selection {
    upstream_server_t *server;     // 选中的服务器
    char *proxy_url;               // 代理URL
    int connection_id;             // 连接ID
} lb_selection_t;

// 会话信息结构
typedef struct session_info {
    char *client_ip;               // 客户端IP
    char *session_id;              // 会话ID
    upstream_server_t *server;     // 绑定的服务器
    time_t last_access;            // 最后访问时间
    struct session_info *next;     // 链表指针
} session_info_t;

// 负载均衡统计信息
typedef struct lb_stats {
    int total_requests;            // 总请求数
    int successful_requests;       // 成功请求数
    int failed_requests;           // 失败请求数
    int active_connections;        // 活跃连接数
    double avg_response_time;      // 平均响应时间
    time_t last_updated;           // 最后更新时间
} lb_stats_t;

// 函数原型声明

// 负载均衡配置管理
lb_config_t *lb_config_create(void);
void lb_config_free(lb_config_t *config);
int lb_config_add_group(lb_config_t *config, const char *name, lb_strategy_t strategy);
upstream_group_t *lb_config_get_group(lb_config_t *config, const char *name);

// 上游组管理
upstream_group_t *upstream_group_create(const char *name, lb_strategy_t strategy);
void upstream_group_free(upstream_group_t *group);
int upstream_group_add_server(upstream_group_t *group, const char *host, int port, int weight);
int upstream_group_remove_server(upstream_group_t *group, const char *host, int port);
upstream_server_t *upstream_group_get_server(upstream_group_t *group, const char *host, int port);

// 服务器管理
upstream_server_t *upstream_server_create(const char *host, int port, int weight);
void upstream_server_free(upstream_server_t *server);
void upstream_server_set_status(upstream_server_t *server, server_status_t status);
int upstream_server_is_available(upstream_server_t *server);

// 负载均衡算法实现
lb_selection_t *lb_select_server(upstream_group_t *group, const char *client_ip, const char *session_id);
upstream_server_t *lb_round_robin(upstream_group_t *group);
upstream_server_t *lb_weighted_round_robin(upstream_group_t *group);
upstream_server_t *lb_least_connections(upstream_group_t *group);
upstream_server_t *lb_ip_hash(upstream_group_t *group, const char *client_ip);
upstream_server_t *lb_random(upstream_group_t *group);
upstream_server_t *lb_weighted_random(upstream_group_t *group);

// 健康检查
int lb_health_check_server(upstream_server_t *server);
void lb_health_check_all(upstream_group_t *group);
void *lb_health_check_thread(void *arg);

// 会话管理
session_info_t *session_find(const char *client_ip, const char *session_id);
int session_bind(const char *client_ip, const char *session_id, upstream_server_t *server);
void session_cleanup_expired(int timeout);

// 统计信息
lb_stats_t *lb_get_stats(upstream_group_t *group);
void lb_update_stats(upstream_server_t *server, int success, double response_time);
void lb_print_stats(upstream_group_t *group);

// 连接管理
int lb_connect_to_server(upstream_server_t *server);
void lb_close_connection(upstream_server_t *server, int connection_id);
void lb_update_connection_count(upstream_server_t *server, int delta);

// 工具函数
char *lb_build_proxy_url(upstream_server_t *server);
unsigned int lb_hash_string(const char *str);
void lb_selection_free(lb_selection_t *selection);

// 配置解析
int lb_parse_upstream_block(const char *block_content, lb_config_t *config);
int lb_parse_server_directive(const char *directive, upstream_group_t *group);

// 健康检查管理器集成
int lb_start_health_check_manager(upstream_group_t *group, health_check_config_t *config);
int lb_stop_health_check_manager(upstream_group_t *group);
int lb_is_health_check_running(upstream_group_t *group);

// 策略转换函数
const char *lb_strategy_to_string(lb_strategy_t strategy);
lb_strategy_t lb_strategy_from_string(const char *strategy_str);

#endif // LOAD_BALANCER_H 