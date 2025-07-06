#ifndef PUSH_H
#define PUSH_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <openssl/ssl.h>

// 推送连接类型
typedef enum {
    PUSH_TYPE_SSE,           // Server-Sent Events
    PUSH_TYPE_WEBSOCKET,     // WebSocket (基础支持)
    PUSH_TYPE_LONG_POLLING   // 长轮询
} push_type_t;

// 推送连接状态
typedef enum {
    PUSH_STATE_CONNECTING,
    PUSH_STATE_CONNECTED,
    PUSH_STATE_SUBSCRIBING,
    PUSH_STATE_ACTIVE,
    PUSH_STATE_ERROR,
    PUSH_STATE_CLOSED
} push_state_t;

// 推送消息类型
typedef enum {
    PUSH_MSG_DATA,           // 数据消息
    PUSH_MSG_EVENT,          // 事件消息
    PUSH_MSG_HEARTBEAT,      // 心跳消息
    PUSH_MSG_SYSTEM,         // 系统消息
    PUSH_MSG_ERROR           // 错误消息
} push_message_type_t;

// 推送消息结构
typedef struct push_message {
    char *id;                        // 消息ID
    push_message_type_t type;        // 消息类型
    char *event;                     // 事件名称
    char *data;                      // 消息数据
    size_t data_length;              // 数据长度
    time_t timestamp;                // 时间戳
    int retry_interval;              // 重试间隔（毫秒）
    char *origin;                    // 消息来源
    struct push_message *next;       // 链表指针
} push_message_t;

// 推送订阅信息
typedef struct push_subscription {
    char *id;                        // 订阅ID
    char *channel;                   // 频道名称
    char *event_filter;              // 事件过滤器（可选）
    bool active;                     // 是否激活
    time_t created_time;             // 创建时间
    time_t last_message_time;        // 最后消息时间
    int message_count;               // 消息计数
    struct push_subscription *next;  // 链表指针
} push_subscription_t;

// 推送客户端连接
typedef struct push_client {
    char *id;                        // 客户端ID
    int socket_fd;                   // socket文件描述符
    SSL *ssl;                        // SSL连接（如果是HTTPS）
    bool is_ssl;                     // 是否是SSL连接
    struct sockaddr_in client_addr;  // 客户端地址
    
    push_type_t type;                // 连接类型
    push_state_t state;              // 连接状态
    
    // 连接信息
    char *user_agent;                // 用户代理
    char *origin;                    // 来源
    char *client_ip;                 // 客户端IP
    time_t connect_time;             // 连接时间
    time_t last_activity;            // 最后活动时间
    
    // 订阅管理
    push_subscription_t *subscriptions; // 订阅列表
    int subscription_count;          // 订阅数量
    
    // 消息队列
    push_message_t *message_queue;   // 消息队列
    int queue_size;                  // 队列大小
    int max_queue_size;              // 最大队列大小
    pthread_mutex_t queue_mutex;     // 队列互斥锁
    
    // 配置参数
    int heartbeat_interval;          // 心跳间隔（秒）
    int timeout;                     // 超时时间（秒）
    bool auto_reconnect;             // 自动重连
    
    // 统计信息
    size_t messages_sent;            // 已发送消息数
    size_t bytes_sent;               // 已发送字节数
    time_t last_message_time;        // 最后消息时间
    
    pthread_t worker_thread;         // 工作线程
    bool active;                     // 是否激活
    struct push_client *next;        // 链表指针
} push_client_t;

// 推送频道
typedef struct push_channel {
    char *name;                      // 频道名称
    char *description;               // 频道描述
    bool active;                     // 是否激活
    
    // 订阅者管理
    push_client_t **subscribers;     // 订阅者数组
    int subscriber_count;            // 订阅者数量
    int max_subscribers;             // 最大订阅者数量
    pthread_rwlock_t subscribers_lock; // 订阅者读写锁
    
    // 消息历史
    push_message_t *message_history; // 消息历史
    int history_size;                // 历史大小
    int max_history_size;            // 最大历史大小
    
    // 统计信息
    size_t total_messages;           // 总消息数
    size_t total_subscribers;        // 总订阅者数
    time_t created_time;             // 创建时间
    time_t last_message_time;        // 最后消息时间
    
    struct push_channel *next;       // 链表指针
} push_channel_t;

// 推送服务器配置
typedef struct {
    bool enabled;                    // 是否启用推送服务
    int port;                        // 推送服务端口
    char *bind_address;              // 绑定地址
    
    // 连接设置
    int max_clients;                 // 最大客户端数
    int client_timeout;              // 客户端超时（秒）
    int heartbeat_interval;          // 心跳间隔（秒）
    int max_queue_size;              // 最大消息队列大小
    
    // 频道设置
    int max_channels;                // 最大频道数
    int max_subscribers_per_channel; // 每个频道最大订阅者数
    int message_history_size;        // 消息历史大小
    
    // 安全设置
    bool require_origin_check;       // 是否检查Origin
    char **allowed_origins;          // 允许的来源列表
    int allowed_origins_count;       // 允许来源数量
    
    // 性能设置
    int worker_threads;              // 工作线程数
    size_t buffer_size;              // 缓冲区大小
    bool enable_compression;         // 是否启用压缩
    
    // 日志设置
    char *access_log;                // 访问日志
    char *error_log;                 // 错误日志
    bool enable_stats;               // 是否启用统计
} push_config_t;

// 推送服务器管理器
typedef struct {
    push_config_t *config;           // 配置
    
    // 客户端管理
    push_client_t *clients;          // 客户端列表
    int client_count;                // 客户端数量
    pthread_mutex_t clients_mutex;   // 客户端互斥锁
    
    // 频道管理
    push_channel_t *channels;        // 频道列表
    int channel_count;               // 频道数量
    pthread_rwlock_t channels_lock;  // 频道读写锁
    
    // 服务器状态
    int server_socket;               // 服务器socket
    bool running;                    // 是否运行中
    pthread_t *worker_threads;       // 工作线程池
    
    // 统计信息
    size_t total_connections;        // 总连接数
    size_t total_messages;           // 总消息数
    size_t total_bytes_sent;         // 总发送字节数
    time_t start_time;               // 启动时间
} push_manager_t;

// 推送统计信息
typedef struct {
    size_t active_clients;           // 活跃客户端数
    size_t total_connections;        // 总连接数
    size_t total_messages;           // 总消息数
    size_t total_channels;           // 总频道数
    size_t messages_per_second;      // 每秒消息数
    double avg_response_time;        // 平均响应时间
    time_t last_updated;             // 最后更新时间
} push_stats_t;

// 推送服务器管理
push_manager_t *push_manager_create(push_config_t *config);
void push_manager_free(push_manager_t *manager);
int push_manager_start(push_manager_t *manager);
int push_manager_stop(push_manager_t *manager);

// 推送配置管理
push_config_t *push_config_create(void);
void push_config_free(push_config_t *config);
int push_config_set_port(push_config_t *config, int port);
int push_config_add_allowed_origin(push_config_t *config, const char *origin);

// 客户端连接管理
push_client_t *push_client_create(int socket_fd, SSL *ssl, const char *client_ip);
void push_client_free(push_client_t *client);
int push_client_add(push_manager_t *manager, push_client_t *client);
int push_client_remove(push_manager_t *manager, const char *client_id);
push_client_t *push_client_find(push_manager_t *manager, const char *client_id);
int push_client_send_message(push_client_t *client, push_message_t *message);

// 频道管理
push_channel_t *push_channel_create(const char *name, const char *description);
void push_channel_free(push_channel_t *channel);
int push_channel_add(push_manager_t *manager, push_channel_t *channel);
int push_channel_remove(push_manager_t *manager, const char *channel_name);
push_channel_t *push_channel_find(push_manager_t *manager, const char *channel_name);
int push_channel_subscribe(push_channel_t *channel, push_client_t *client);
int push_channel_unsubscribe(push_channel_t *channel, push_client_t *client);
int push_channel_broadcast(push_channel_t *channel, push_message_t *message);

// 消息管理
push_message_t *push_message_create(const char *event, const char *data, const char *id);
void push_message_free(push_message_t *message);
push_message_t *push_message_create_heartbeat(void);
push_message_t *push_message_create_system(const char *message);
char *push_message_serialize_sse(push_message_t *message);

// 订阅管理
push_subscription_t *push_subscription_create(const char *channel, const char *event_filter);
void push_subscription_free(push_subscription_t *subscription);
int push_client_add_subscription(push_client_t *client, push_subscription_t *subscription);
int push_client_remove_subscription(push_client_t *client, const char *subscription_id);

// Server-Sent Events处理
int push_handle_sse_request(push_manager_t *manager, int client_socket, SSL *ssl, 
                           const char *request_headers, const char *client_ip);
int push_send_sse_headers(push_client_t *client);
int push_send_sse_message(push_client_t *client, push_message_t *message);

// 长轮询处理
int push_handle_long_polling_request(push_manager_t *manager, int client_socket, SSL *ssl,
                                    const char *request_headers, const char *client_ip);
int push_send_long_polling_response(push_client_t *client, push_message_t *message);

// 工作线程
void *push_client_worker_thread(void *arg);
void *push_heartbeat_thread(void *arg);
void *push_cleanup_thread(void *arg);

// 统计和监控
push_stats_t *push_get_stats(push_manager_t *manager);
void push_update_stats(push_manager_t *manager);
int push_log_connection(push_client_t *client, const char *action, const char *details);

// 工具函数
const char *push_type_to_string(push_type_t type);
push_type_t push_type_from_string(const char *type_str);
const char *push_state_to_string(push_state_t state);
bool push_is_origin_allowed(push_config_t *config, const char *origin);
char *push_generate_client_id(const char *client_ip);
char *push_generate_message_id(void);

// 配置解析
int push_parse_config_directive(push_config_t *config, const char *key, const char *value);

#endif // PUSH_H 