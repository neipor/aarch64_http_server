#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "push.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "log.h"

#define DEFAULT_HEARTBEAT_INTERVAL 30
#define DEFAULT_CLIENT_TIMEOUT 300
#define DEFAULT_MAX_QUEUE_SIZE 100
#define DEFAULT_BUFFER_SIZE 8192
#define SSE_HEADERS "HTTP/1.1 200 OK\r\n" \
                   "Content-Type: text/event-stream\r\n" \
                   "Cache-Control: no-cache\r\n" \
                   "Connection: keep-alive\r\n" \
                   "Access-Control-Allow-Origin: *\r\n" \
                   "Access-Control-Allow-Headers: Cache-Control\r\n\r\n"

// 创建推送配置
push_config_t *push_config_create(void) {
    push_config_t *config = calloc(1, sizeof(push_config_t));
    if (!config) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for push config");
        return NULL;
    }
    
    // 设置默认值
    config->enabled = false;
    config->port = 8080;
    config->bind_address = strdup("0.0.0.0");
    config->max_clients = 1000;
    config->client_timeout = DEFAULT_CLIENT_TIMEOUT;
    config->heartbeat_interval = DEFAULT_HEARTBEAT_INTERVAL;
    config->max_queue_size = DEFAULT_MAX_QUEUE_SIZE;
    config->max_channels = 100;
    config->max_subscribers_per_channel = 1000;
    config->message_history_size = 50;
    config->require_origin_check = false;
    config->worker_threads = 4;
    config->buffer_size = DEFAULT_BUFFER_SIZE;
    config->enable_compression = false;
    config->access_log = strdup("push_access.log");
    config->error_log = strdup("push_error.log");
    config->enable_stats = true;
    
    log_message(LOG_LEVEL_DEBUG, "Push config created");
    return config;
}

// 释放推送配置
void push_config_free(push_config_t *config) {
    if (!config) return;
    
    free(config->bind_address);
    free(config->access_log);
    free(config->error_log);
    
    if (config->allowed_origins) {
        for (int i = 0; i < config->allowed_origins_count; i++) {
            free(config->allowed_origins[i]);
        }
        free(config->allowed_origins);
    }
    
    free(config);
}

// 创建推送管理器
push_manager_t *push_manager_create(push_config_t *config) {
    if (!config) {
        log_message(LOG_LEVEL_ERROR, "Invalid config for push manager creation");
        return NULL;
    }
    
    push_manager_t *manager = calloc(1, sizeof(push_manager_t));
    if (!manager) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for push manager");
        return NULL;
    }
    
    manager->config = config;
    manager->clients = NULL;
    manager->client_count = 0;
    manager->channels = NULL;
    manager->channel_count = 0;
    manager->server_socket = -1;
    manager->running = false;
    manager->start_time = time(NULL);
    
    // 初始化互斥锁
    if (pthread_mutex_init(&manager->clients_mutex, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize clients mutex");
        free(manager);
        return NULL;
    }
    
    if (pthread_rwlock_init(&manager->channels_lock, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize channels lock");
        pthread_mutex_destroy(&manager->clients_mutex);
        free(manager);
        return NULL;
    }
    
    // 分配工作线程数组
    manager->worker_threads = calloc(config->worker_threads, sizeof(pthread_t));
    if (!manager->worker_threads) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate worker threads array");
        pthread_mutex_destroy(&manager->clients_mutex);
        pthread_rwlock_destroy(&manager->channels_lock);
        free(manager);
        return NULL;
    }
    
    log_message(LOG_LEVEL_INFO, "Push manager created successfully");
    return manager;
}

// 释放推送管理器
void push_manager_free(push_manager_t *manager) {
    if (!manager) return;
    
    if (manager->running) {
        push_manager_stop(manager);
    }
    
    // 清理客户端
    pthread_mutex_lock(&manager->clients_mutex);
    push_client_t *client = manager->clients;
    while (client) {
        push_client_t *next = client->next;
        push_client_free(client);
        client = next;
    }
    pthread_mutex_unlock(&manager->clients_mutex);
    
    // 清理频道
    pthread_rwlock_wrlock(&manager->channels_lock);
    push_channel_t *channel = manager->channels;
    while (channel) {
        push_channel_t *next = channel->next;
        push_channel_free(channel);
        channel = next;
    }
    pthread_rwlock_unlock(&manager->channels_lock);
    
    // 销毁锁
    pthread_mutex_destroy(&manager->clients_mutex);
    pthread_rwlock_destroy(&manager->channels_lock);
    
    free(manager->worker_threads);
    free(manager);
    
    log_message(LOG_LEVEL_INFO, "Push manager freed");
}

// 创建推送客户端
push_client_t *push_client_create(int socket_fd, SSL *ssl, const char *client_ip) {
    if (socket_fd < 0 || !client_ip) {
        return NULL;
    }
    
    push_client_t *client = calloc(1, sizeof(push_client_t));
    if (!client) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for push client");
        return NULL;
    }
    
    client->socket_fd = socket_fd;
    client->ssl = ssl;
    client->is_ssl = (ssl != NULL);
    client->type = PUSH_TYPE_SSE;
    client->state = PUSH_STATE_CONNECTING;
    client->client_ip = strdup(client_ip);
    client->connect_time = time(NULL);
    client->last_activity = client->connect_time;
    client->heartbeat_interval = DEFAULT_HEARTBEAT_INTERVAL;
    client->timeout = DEFAULT_CLIENT_TIMEOUT;
    client->auto_reconnect = true;
    client->max_queue_size = DEFAULT_MAX_QUEUE_SIZE;
    client->active = true;
    
    // 生成客户端ID
    client->id = push_generate_client_id(client_ip);
    
    // 初始化队列互斥锁
    if (pthread_mutex_init(&client->queue_mutex, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize client queue mutex");
        free(client->id);
        free(client->client_ip);
        free(client);
        return NULL;
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Push client created: %s (%s)", client->id, client_ip);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return client;
}

// 释放推送客户端
void push_client_free(push_client_t *client) {
    if (!client) return;
    
    client->active = false;
    
    // 清理消息队列
    pthread_mutex_lock(&client->queue_mutex);
    push_message_t *msg = client->message_queue;
    while (msg) {
        push_message_t *next = msg->next;
        push_message_free(msg);
        msg = next;
    }
    pthread_mutex_unlock(&client->queue_mutex);
    
    // 清理订阅
    push_subscription_t *sub = client->subscriptions;
    while (sub) {
        push_subscription_t *next = sub->next;
        push_subscription_free(sub);
        sub = next;
    }
    
    // 关闭连接
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
    }
    
    // 销毁互斥锁
    pthread_mutex_destroy(&client->queue_mutex);
    
    free(client->id);
    free(client->client_ip);
    free(client->user_agent);
    free(client->origin);
    free(client);
}

// 创建推送消息
push_message_t *push_message_create(const char *event, const char *data, const char *id) {
    push_message_t *message = calloc(1, sizeof(push_message_t));
    if (!message) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for push message");
        return NULL;
    }
    
    message->type = PUSH_MSG_DATA;
    message->timestamp = time(NULL);
    message->retry_interval = 3000; // 3秒默认重试间隔
    
    if (event) {
        message->event = strdup(event);
    }
    
    if (data) {
        message->data = strdup(data);
        message->data_length = strlen(data);
    }
    
    if (id) {
        message->id = strdup(id);
    } else {
        message->id = push_generate_message_id();
    }
    
    return message;
}

// 释放推送消息
void push_message_free(push_message_t *message) {
    if (!message) return;
    
    free(message->id);
    free(message->event);
    free(message->data);
    free(message->origin);
    free(message);
}

// 创建心跳消息
push_message_t *push_message_create_heartbeat(void) {
    push_message_t *message = push_message_create("heartbeat", "ping", NULL);
    if (message) {
        message->type = PUSH_MSG_HEARTBEAT;
    }
    return message;
}

// 序列化SSE消息
char *push_message_serialize_sse(push_message_t *message) {
    if (!message) return NULL;
    
    size_t buffer_size = 1024;
    if (message->data_length > 0) {
        buffer_size += message->data_length * 2; // 预留空间
    }
    
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for SSE message");
        return NULL;
    }
    
    int offset = 0;
    
    // 添加ID
    if (message->id) {
        offset += snprintf(buffer + offset, buffer_size - offset, "id: %s\n", message->id);
    }
    
    // 添加事件类型
    if (message->event) {
        offset += snprintf(buffer + offset, buffer_size - offset, "event: %s\n", message->event);
    }
    
    // 添加重试间隔
    if (message->retry_interval > 0) {
        offset += snprintf(buffer + offset, buffer_size - offset, "retry: %d\n", message->retry_interval);
    }
    
    // 添加数据
    if (message->data) {
        // 处理多行数据
        char *data_copy = strdup(message->data);
        char *line = strtok(data_copy, "\n");
        while (line && (size_t)offset < buffer_size - 20) {
            offset += snprintf(buffer + offset, buffer_size - offset, "data: %s\n", line);
            line = strtok(NULL, "\n");
        }
        free(data_copy);
    }
    
    // 添加结束标记
    offset += snprintf(buffer + offset, buffer_size - offset, "\n");
    
    return buffer;
}

// 发送SSE头部
int push_send_sse_headers(push_client_t *client) {
    if (!client || client->socket_fd < 0) {
        return -1;
    }
    
    const char *headers = SSE_HEADERS;
    ssize_t bytes_sent;
    
    if (client->is_ssl && client->ssl) {
        bytes_sent = SSL_write(client->ssl, headers, strlen(headers));
    } else {
        bytes_sent = send(client->socket_fd, headers, strlen(headers), 0);
    }
    
    if (bytes_sent < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to send SSE headers");
        return -1;
    }
    
    client->state = PUSH_STATE_CONNECTED;
    log_message(LOG_LEVEL_DEBUG, "SSE headers sent successfully");
    
    return 0;
}

// 发送SSE消息
int push_send_sse_message(push_client_t *client, push_message_t *message) {
    if (!client || !message || client->socket_fd < 0) {
        return -1;
    }
    
    char *sse_data = push_message_serialize_sse(message);
    if (!sse_data) {
        return -1;
    }
    
    ssize_t bytes_sent;
    size_t data_len = strlen(sse_data);
    
    if (client->is_ssl && client->ssl) {
        bytes_sent = SSL_write(client->ssl, sse_data, data_len);
    } else {
        bytes_sent = send(client->socket_fd, sse_data, data_len, 0);
    }
    
    free(sse_data);
    
    if (bytes_sent < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to send SSE message");
        return -1;
    }
    
    // 更新统计信息
    client->messages_sent++;
    client->bytes_sent += bytes_sent;
    client->last_message_time = time(NULL);
    client->last_activity = client->last_message_time;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "SSE message sent to client %s: %s", 
             client->id, message->event ? message->event : "data");
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return 0;
}

// 处理SSE请求
int push_handle_sse_request(push_manager_t *manager, int client_socket, SSL *ssl, 
                           const char *request_headers, const char *client_ip) {
    if (!manager || client_socket < 0 || !client_ip) {
        return -1;
    }
    
    // 检查是否达到最大客户端数
    pthread_mutex_lock(&manager->clients_mutex);
    if (manager->client_count >= manager->config->max_clients) {
        pthread_mutex_unlock(&manager->clients_mutex);
        log_message(LOG_LEVEL_WARNING, "Max clients limit reached");
        
        const char *error_response = "HTTP/1.1 503 Service Unavailable\r\n"
                                   "Content-Type: text/plain\r\n"
                                   "Content-Length: 21\r\n\r\n"
                                   "Service Unavailable\n";
        
        if (ssl) {
            SSL_write(ssl, error_response, strlen(error_response));
        } else {
            send(client_socket, error_response, strlen(error_response), 0);
        }
        return -1;
    }
    pthread_mutex_unlock(&manager->clients_mutex);
    
    // 创建客户端
    push_client_t *client = push_client_create(client_socket, ssl, client_ip);
    if (!client) {
        return -1;
    }
    
    // 解析请求头（简化版）
    if (request_headers) {
        // 提取User-Agent
        const char *ua_header = strstr(request_headers, "User-Agent:");
        if (ua_header) {
            const char *ua_start = ua_header + 11;
            const char *ua_end = strstr(ua_start, "\r\n");
            if (ua_end) {
                client->user_agent = strndup(ua_start, ua_end - ua_start);
            }
        }
        
        // 提取Origin
        const char *origin_header = strstr(request_headers, "Origin:");
        if (origin_header) {
            const char *origin_start = origin_header + 7;
            const char *origin_end = strstr(origin_start, "\r\n");
            if (origin_end) {
                client->origin = strndup(origin_start, origin_end - origin_start);
            }
        }
    }
    
    // 发送SSE头部
    if (push_send_sse_headers(client) < 0) {
        push_client_free(client);
        return -1;
    }
    
    // 添加到客户端列表
    pthread_mutex_lock(&manager->clients_mutex);
    client->next = manager->clients;
    manager->clients = client;
    manager->client_count++;
    manager->total_connections++;
    pthread_mutex_unlock(&manager->clients_mutex);
    
    // 发送欢迎消息
    push_message_t *welcome = push_message_create("connected", 
                                                "Welcome to ANX Push Service", NULL);
    if (welcome) {
        push_send_sse_message(client, welcome);
        push_message_free(welcome);
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "SSE client connected: %s from %s", 
             client->id, client_ip);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return 0;
}

// 生成客户端ID
char *push_generate_client_id(const char *client_ip) {
    char *id = malloc(64);
    if (!id) return NULL;
    
    static int client_counter = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
        snprintf(id, 64, "client_%s_%ld_%ld_%d",
             client_ip, tv.tv_sec, tv.tv_usec, ++client_counter);
    
    return id;
}

// 生成消息ID
char *push_generate_message_id(void) {
    char *id = malloc(64);
    if (!id) return NULL;
    
    static int msg_counter = 0;
    snprintf(id, 64, "msg_%ld_%d", time(NULL), ++msg_counter);
    
    return id;
}

// 工具函数：推送类型转字符串
const char *push_type_to_string(push_type_t type) {
    switch (type) {
        case PUSH_TYPE_SSE: return "SSE";
        case PUSH_TYPE_WEBSOCKET: return "WebSocket";
        case PUSH_TYPE_LONG_POLLING: return "Long Polling";
        default: return "Unknown";
    }
}

// 工具函数：字符串转推送类型
push_type_t push_type_from_string(const char *type_str) {
    if (!type_str) return PUSH_TYPE_SSE;
    
    if (strcasecmp(type_str, "sse") == 0 || strcasecmp(type_str, "server-sent-events") == 0) {
        return PUSH_TYPE_SSE;
    } else if (strcasecmp(type_str, "websocket") == 0 || strcasecmp(type_str, "ws") == 0) {
        return PUSH_TYPE_WEBSOCKET;
    } else if (strcasecmp(type_str, "long-polling") == 0 || strcasecmp(type_str, "polling") == 0) {
        return PUSH_TYPE_LONG_POLLING;
    }
    
    return PUSH_TYPE_SSE; // 默认SSE
}

// 工具函数：状态转字符串
const char *push_state_to_string(push_state_t state) {
    switch (state) {
        case PUSH_STATE_CONNECTING: return "Connecting";
        case PUSH_STATE_CONNECTED: return "Connected";
        case PUSH_STATE_SUBSCRIBING: return "Subscribing";
        case PUSH_STATE_ACTIVE: return "Active";
        case PUSH_STATE_ERROR: return "Error";
        case PUSH_STATE_CLOSED: return "Closed";
        default: return "Unknown";
    }
}

// 启动推送管理器
int push_manager_start(push_manager_t *manager) {
    if (!manager || manager->running) {
        return -1;
    }
    
    if (!manager->config->enabled) {
        log_message(LOG_LEVEL_INFO, "Push service is disabled");
        return 0;
    }
    
    // 创建服务器socket
    manager->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (manager->server_socket < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to create push server socket");
        return -1;
    }
    
    // 设置socket选项
    int opt = 1;
    if (setsockopt(manager->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_message(LOG_LEVEL_WARNING, "Failed to set SO_REUSEADDR for push server");
    }
    
    // 绑定地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(manager->config->port);
    
    if (inet_pton(AF_INET, manager->config->bind_address, &addr.sin_addr) <= 0) {
        log_message(LOG_LEVEL_ERROR, "Invalid push server bind address");
        close(manager->server_socket);
        return -1;
    }
    
    if (bind(manager->server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to bind push server to %s:%d - %s",
                 manager->config->bind_address, manager->config->port, strerror(errno));
        log_message(LOG_LEVEL_ERROR, log_msg);
        close(manager->server_socket);
        return -1;
    }
    
    // 监听
    if (listen(manager->server_socket, 128) < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to listen on push server socket");
        close(manager->server_socket);
        return -1;
    }
    
    manager->running = true;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Push server started on %s:%d",
             manager->config->bind_address, manager->config->port);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return 0;
}

// 停止推送管理器
int push_manager_stop(push_manager_t *manager) {
    if (!manager || !manager->running) {
        return -1;
    }
    
    manager->running = false;
    
    if (manager->server_socket >= 0) {
        close(manager->server_socket);
        manager->server_socket = -1;
    }
    
    log_message(LOG_LEVEL_INFO, "Push server stopped");
    return 0;
}

// 释放推送订阅
void push_subscription_free(push_subscription_t *subscription) {
    if (!subscription) return;
    
    free(subscription->id);
    free(subscription->channel);
    free(subscription->event_filter);
    free(subscription);
}

// 创建推送频道
push_channel_t *push_channel_create(const char *name, const char *description) {
    if (!name) return NULL;
    
    push_channel_t *channel = calloc(1, sizeof(push_channel_t));
    if (!channel) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for push channel");
        return NULL;
    }
    
    channel->name = strdup(name);
    if (description) {
        channel->description = strdup(description);
    }
    channel->active = true;
    channel->max_subscribers = 1000;
    channel->max_history_size = 50;
    channel->created_time = time(NULL);
    
    // 初始化读写锁
    if (pthread_rwlock_init(&channel->subscribers_lock, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize channel subscribers lock");
        free(channel->name);
        free(channel->description);
        free(channel);
        return NULL;
    }
    
    // 分配订阅者数组
    channel->subscribers = calloc(channel->max_subscribers, sizeof(push_client_t *));
    if (!channel->subscribers) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate subscribers array");
        pthread_rwlock_destroy(&channel->subscribers_lock);
        free(channel->name);
        free(channel->description);
        free(channel);
        return NULL;
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Push channel created: %s", name);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return channel;
}

// 释放推送频道
void push_channel_free(push_channel_t *channel) {
    if (!channel) return;
    
    // 清理消息历史
    push_message_t *msg = channel->message_history;
    while (msg) {
        push_message_t *next = msg->next;
        push_message_free(msg);
        msg = next;
    }
    
    // 销毁读写锁
    pthread_rwlock_destroy(&channel->subscribers_lock);
    
    free(channel->subscribers);
    free(channel->name);
    free(channel->description);
    free(channel);
}

// 创建推送订阅
push_subscription_t *push_subscription_create(const char *channel, const char *event_filter) {
    if (!channel) return NULL;
    
    push_subscription_t *subscription = calloc(1, sizeof(push_subscription_t));
    if (!subscription) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for push subscription");
        return NULL;
    }
    
    subscription->channel = strdup(channel);
    if (event_filter) {
        subscription->event_filter = strdup(event_filter);
    }
    subscription->active = true;
    subscription->created_time = time(NULL);
    
    // 生成订阅ID
    static int sub_counter = 0;
    subscription->id = malloc(32);
    if (subscription->id) {
        snprintf(subscription->id, 32, "sub_%ld_%d", time(NULL), ++sub_counter);
    }
    
    return subscription;
}

// 获取推送统计信息
push_stats_t *push_get_stats(push_manager_t *manager) {
    if (!manager) return NULL;
    
    push_stats_t *stats = malloc(sizeof(push_stats_t));
    if (!stats) return NULL;
    
    pthread_mutex_lock(&manager->clients_mutex);
    stats->active_clients = manager->client_count;
    stats->total_connections = manager->total_connections;
    stats->total_messages = manager->total_messages;
    pthread_mutex_unlock(&manager->clients_mutex);
    
    pthread_rwlock_rdlock(&manager->channels_lock);
    stats->total_channels = manager->channel_count;
    pthread_rwlock_unlock(&manager->channels_lock);
    
    stats->messages_per_second = 0; // 需要更复杂的计算
    stats->avg_response_time = 0.0; // 需要更复杂的计算
    stats->last_updated = time(NULL);
    
    return stats;
} 