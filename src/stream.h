#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "load_balancer.h"

// Stream协议类型
typedef enum {
    STREAM_PROTOCOL_TCP,
    STREAM_PROTOCOL_UDP
} stream_protocol_t;

// Stream代理状态
typedef enum {
    STREAM_PROXY_STATE_IDLE,
    STREAM_PROXY_STATE_CONNECTING,
    STREAM_PROXY_STATE_CONNECTED,
    STREAM_PROXY_STATE_FORWARDING,
    STREAM_PROXY_STATE_ERROR,
    STREAM_PROXY_STATE_CLOSED
} stream_proxy_state_t;

// Stream连接信息
typedef struct stream_connection {
    int client_fd;                    // 客户端socket
    int backend_fd;                   // 后端socket
    struct sockaddr_in client_addr;   // 客户端地址
    struct sockaddr_in backend_addr;  // 后端地址
    stream_protocol_t protocol;       // 协议类型
    stream_proxy_state_t state;       // 连接状态
    time_t start_time;               // 连接开始时间
    size_t bytes_sent;               // 已发送字节数
    size_t bytes_received;           // 已接收字节数
    pthread_t proxy_thread;          // 代理线程
    bool active;                     // 是否激活
    char *upstream_name;             // upstream名称
    upstream_server_t *backend_server; // 后端服务器
    struct stream_connection *next;   // 链表指针
} stream_connection_t;

// Stream监听器配置
typedef struct stream_listener {
    char *bind_address;              // 监听地址
    int port;                        // 监听端口
    stream_protocol_t protocol;      // 协议类型
    char *upstream_name;             // upstream名称
    int socket_fd;                   // 监听socket
    bool active;                     // 是否激活
    
    // 连接设置
    int backlog;                     // TCP监听队列长度
    int connect_timeout;             // 连接超时（秒）
    int proxy_timeout;               // 代理超时（秒）
    size_t proxy_buffer_size;        // 代理缓冲区大小
    
    // 性能设置
    bool tcp_nodelay;                // TCP_NODELAY选项
    bool so_keepalive;               // SO_KEEPALIVE选项
    int so_reuseport;                // SO_REUSEPORT选项
    
    struct stream_listener *next;    // 链表指针
} stream_listener_t;

// Stream配置
typedef struct {
    stream_listener_t *listeners;    // 监听器列表
    int listeners_count;             // 监听器数量
    
    // 全局设置
    int worker_connections;          // 工作连接数
    int resolver_timeout;            // 解析超时
    size_t default_buffer_size;      // 默认缓冲区大小
    
    // 错误处理
    char *access_log;                // 访问日志文件
    char *error_log;                 // 错误日志文件
    
    // 统计设置
    bool enable_stats;               // 是否启用统计
} stream_config_t;

// Stream统计信息
typedef struct {
    size_t total_connections;        // 总连接数
    size_t active_connections;       // 活跃连接数
    size_t failed_connections;       // 失败连接数
    size_t bytes_transferred;        // 传输字节数
    time_t start_time;              // 统计开始时间
    time_t last_updated;            // 最后更新时间
} stream_stats_t;

// Stream管理器
typedef struct {
    stream_config_t *config;         // 配置
    lb_config_t *lb_config;          // 负载均衡配置
    stream_connection_t *connections; // 连接列表
    pthread_mutex_t connections_mutex; // 连接列表互斥锁
    stream_stats_t stats;            // 统计信息
    bool running;                    // 是否运行中
} stream_manager_t;

// Stream模块初始化和清理
stream_manager_t *stream_manager_create(stream_config_t *config, lb_config_t *lb_config);
void stream_manager_free(stream_manager_t *manager);
int stream_manager_start(stream_manager_t *manager);
int stream_manager_stop(stream_manager_t *manager);

// Stream配置管理
stream_config_t *stream_config_create(void);
void stream_config_free(stream_config_t *config);
stream_listener_t *stream_listener_create(const char *address, int port, 
                                         stream_protocol_t protocol, const char *upstream);
void stream_listener_free(stream_listener_t *listener);
int stream_config_add_listener(stream_config_t *config, stream_listener_t *listener);

// Stream监听和代理
int stream_listener_start(stream_listener_t *listener, lb_config_t *lb_config);
int stream_listener_stop(stream_listener_t *listener);
void *stream_listener_thread(void *arg);
void *stream_proxy_thread(void *arg);

// TCP代理函数
int stream_tcp_proxy_start(stream_connection_t *conn, lb_config_t *lb_config);
int stream_tcp_forward_data(int source_fd, int dest_fd, char *buffer, size_t buffer_size);
int stream_tcp_connect_backend(stream_connection_t *conn, upstream_server_t *server);

// UDP代理函数
int stream_udp_proxy_start(stream_connection_t *conn, lb_config_t *lb_config);
int stream_udp_forward_packet(stream_connection_t *conn, lb_config_t *lb_config);

// 连接管理
stream_connection_t *stream_connection_create(int client_fd, stream_protocol_t protocol, 
                                            const char *upstream_name);
void stream_connection_free(stream_connection_t *conn);
int stream_connection_add(stream_manager_t *manager, stream_connection_t *conn);
int stream_connection_remove(stream_manager_t *manager, stream_connection_t *conn);
void stream_connection_cleanup(stream_connection_t *conn);

// 负载均衡集成
upstream_server_t *stream_select_backend(lb_config_t *lb_config, const char *upstream_name,
                                        const char *client_ip);
int stream_update_backend_stats(upstream_server_t *server, bool success, double response_time);

// 统计和监控
stream_stats_t *stream_get_stats(stream_manager_t *manager);
void stream_update_stats(stream_manager_t *manager, stream_connection_t *conn, bool success);
int stream_log_connection(stream_connection_t *conn, int status_code, const char *message);

// 工具函数
const char *stream_protocol_to_string(stream_protocol_t protocol);
stream_protocol_t stream_protocol_from_string(const char *protocol_str);
const char *stream_state_to_string(stream_proxy_state_t state);
bool stream_is_address_valid(const char *address);
int stream_parse_address(const char *address, char **host, int *port);

// 配置解析
int stream_parse_config_block(const char *block_content, stream_config_t *config, 
                             lb_config_t *lb_config);
int stream_parse_listener_directive(stream_listener_t *listener, const char *key, 
                                   const char *value);

#endif // STREAM_H 