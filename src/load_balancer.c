#include "load_balancer.h"
#include "log.h"
#include "health_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>

// 全局会话表
static session_info_t *session_table = NULL;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

// 负载均衡配置管理
lb_config_t *lb_config_create(void) {
    lb_config_t *config = calloc(1, sizeof(lb_config_t));
    if (!config) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for lb_config_t");
        return NULL;
    }
    
    // 设置默认值
    config->default_max_fails = 3;
    config->default_fail_timeout = 30;
    config->default_health_check_interval = 30;
    config->default_health_check_timeout = 10;
    
    if (pthread_mutex_init(&config->mutex, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize mutex for lb_config_t");
        free(config);
        return NULL;
    }
    
    log_message(LOG_LEVEL_DEBUG, "Load balancer config created successfully");
    return config;
}

void lb_config_free(lb_config_t *config) {
    if (!config) return;
    
    pthread_mutex_lock(&config->mutex);
    
    upstream_group_t *group = config->groups;
    while (group) {
        upstream_group_t *next = group->next;
        upstream_group_free(group);
        group = next;
    }
    
    pthread_mutex_unlock(&config->mutex);
    pthread_mutex_destroy(&config->mutex);
    free(config);
    
    log_message(LOG_LEVEL_DEBUG, "Load balancer config freed");
}

int lb_config_add_group(lb_config_t *config, const char *name, lb_strategy_t strategy) {
    if (!config || !name) return -1;
    
    pthread_mutex_lock(&config->mutex);
    
    // 检查组名是否已存在
    upstream_group_t *existing = config->groups;
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            pthread_mutex_unlock(&config->mutex);
            log_message(LOG_LEVEL_WARNING, "Upstream group already exists");
            return -1;
        }
        existing = existing->next;
    }
    
    // 创建新组
    upstream_group_t *group = upstream_group_create(name, strategy);
    if (!group) {
        pthread_mutex_unlock(&config->mutex);
        return -1;
    }
    
    // 添加到链表头部
    group->next = config->groups;
    config->groups = group;
    config->group_count++;
    
    pthread_mutex_unlock(&config->mutex);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Added upstream group '%s' with strategy %d", name, strategy);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return 0;
}

upstream_group_t *lb_config_get_group(lb_config_t *config, const char *name) {
    if (!config || !name) return NULL;
    
    pthread_mutex_lock(&config->mutex);
    
    upstream_group_t *group = config->groups;
    while (group) {
        if (strcmp(group->name, name) == 0) {
            pthread_mutex_unlock(&config->mutex);
            return group;
        }
        group = group->next;
    }
    
    pthread_mutex_unlock(&config->mutex);
    return NULL;
}

// 上游组管理
upstream_group_t *upstream_group_create(const char *name, lb_strategy_t strategy) {
    if (!name) return NULL;
    
    upstream_group_t *group = calloc(1, sizeof(upstream_group_t));
    if (!group) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for upstream_group_t");
        return NULL;
    }
    
    group->name = strdup(name);
    group->strategy = strategy;
    group->current_server_index = 0;
    group->total_weight = 0;
    group->session_persistence = 0;
    group->session_timeout = 3600; // 1小时默认
    group->health_check_enabled = 1;
    group->health_check_interval = 30;
    group->health_check_timeout = 10;
    group->health_check_uri = strdup("/health");
    
    if (pthread_mutex_init(&group->mutex, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize mutex for upstream_group_t");
        free(group->name);
        free(group->health_check_uri);
        free(group);
        return NULL;
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Created upstream group '%s'", name);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return group;
}

void upstream_group_free(upstream_group_t *group) {
    if (!group) return;
    
    pthread_mutex_lock(&group->mutex);
    
    upstream_server_t *server = group->servers;
    while (server) {
        upstream_server_t *next = server->next;
        upstream_server_free(server);
        server = next;
    }
    
    free(group->name);
    free(group->health_check_uri);
    
    pthread_mutex_unlock(&group->mutex);
    pthread_mutex_destroy(&group->mutex);
    free(group);
    
    log_message(LOG_LEVEL_DEBUG, "Upstream group freed");
}

int upstream_group_add_server(upstream_group_t *group, const char *host, int port, int weight) {
    if (!group || !host || port <= 0 || weight <= 0) return -1;
    
    pthread_mutex_lock(&group->mutex);
    
    // 检查服务器是否已存在
    upstream_server_t *existing = group->servers;
    while (existing) {
        if (strcmp(existing->host, host) == 0 && existing->port == port) {
            pthread_mutex_unlock(&group->mutex);
            log_message(LOG_LEVEL_WARNING, "Server already exists in upstream group");
            return -1;
        }
        existing = existing->next;
    }
    
    // 创建新服务器
    upstream_server_t *server = upstream_server_create(host, port, weight);
    if (!server) {
        pthread_mutex_unlock(&group->mutex);
        return -1;
    }
    
    // 添加到链表头部
    server->next = group->servers;
    group->servers = server;
    group->server_count++;
    group->total_weight += weight;
    
    pthread_mutex_unlock(&group->mutex);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Added server %s:%d (weight=%d) to group '%s'", 
             host, port, weight, group->name);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return 0;
}

int upstream_group_remove_server(upstream_group_t *group, const char *host, int port) {
    if (!group || !host || port <= 0) return -1;
    
    pthread_mutex_lock(&group->mutex);
    
    upstream_server_t *prev = NULL;
    upstream_server_t *current = group->servers;
    
    while (current) {
        if (strcmp(current->host, host) == 0 && current->port == port) {
            if (prev) {
                prev->next = current->next;
            } else {
                group->servers = current->next;
            }
            
            group->server_count--;
            group->total_weight -= current->weight;
            
            upstream_server_free(current);
            
            pthread_mutex_unlock(&group->mutex);
            
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Removed server %s:%d from group '%s'", 
                     host, port, group->name);
            log_message(LOG_LEVEL_INFO, log_msg);
            
            return 0;
        }
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&group->mutex);
    return -1;
}

upstream_server_t *upstream_group_get_server(upstream_group_t *group, const char *host, int port) {
    if (!group || !host || port <= 0) return NULL;
    
    pthread_mutex_lock(&group->mutex);
    
    upstream_server_t *server = group->servers;
    while (server) {
        if (strcmp(server->host, host) == 0 && server->port == port) {
            pthread_mutex_unlock(&group->mutex);
            return server;
        }
        server = server->next;
    }
    
    pthread_mutex_unlock(&group->mutex);
    return NULL;
}

// 服务器管理
upstream_server_t *upstream_server_create(const char *host, int port, int weight) {
    if (!host || port <= 0 || weight <= 0) return NULL;
    
    upstream_server_t *server = calloc(1, sizeof(upstream_server_t));
    if (!server) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for upstream_server_t");
        return NULL;
    }
    
    server->host = strdup(host);
    server->port = port;
    server->weight = weight;
    server->max_fails = 3;
    server->fail_timeout = 30;
    server->max_conns = 1000;
    server->status = SERVER_STATUS_UNKNOWN;
    server->health_check_interval = 30;
    server->health_check_timeout = 10;
    server->health_check_uri = strdup("/health");
    server->current_weight = 0;
    server->effective_weight = weight;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Created upstream server %s:%d (weight=%d)", host, port, weight);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return server;
}

void upstream_server_free(upstream_server_t *server) {
    if (!server) return;
    
    free(server->host);
    free(server->health_check_uri);
    free(server);
    
    log_message(LOG_LEVEL_DEBUG, "Upstream server freed");
}

void upstream_server_set_status(upstream_server_t *server, server_status_t status) {
    if (!server) return;
    
    if (server->status != status) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Server %s:%d status changed from %d to %d", 
                 server->host, server->port, server->status, status);
        log_message(LOG_LEVEL_INFO, log_msg);
        
        server->status = status;
        server->last_check_time = time(NULL);
    }
}

int upstream_server_is_available(upstream_server_t *server) {
    if (!server) return 0;
    
    // 检查服务器状态
    if (server->status == SERVER_STATUS_DOWN) {
        return 0;
    }
    
    // 检查连接数限制
    if (server->max_conns > 0 && server->current_connections >= server->max_conns) {
        return 0;
    }
    
    // 检查失败次数和超时
    if (server->consecutive_failures >= server->max_fails) {
        time_t now = time(NULL);
        if (now - server->last_failure_time < server->fail_timeout) {
            return 0;
        } else {
            // 重置失败计数
            server->consecutive_failures = 0;
        }
    }
    
    return 1;
}

// 负载均衡算法实现
lb_selection_t *lb_select_server(upstream_group_t *group, const char *client_ip, const char *session_id) {
    if (!group || group->server_count == 0) return NULL;
    
    upstream_server_t *selected = NULL;
    
    // 检查会话保持
    if (group->session_persistence && (client_ip || session_id)) {
        session_info_t *session = session_find(client_ip, session_id);
        if (session && upstream_server_is_available(session->server)) {
            selected = session->server;
        }
    }
    
    // 如果没有会话绑定，使用负载均衡算法选择
    if (!selected) {
        switch (group->strategy) {
            case LB_STRATEGY_ROUND_ROBIN:
                selected = lb_round_robin(group);
                break;
            case LB_STRATEGY_WEIGHTED_ROUND_ROBIN:
                selected = lb_weighted_round_robin(group);
                break;
            case LB_STRATEGY_LEAST_CONNECTIONS:
                selected = lb_least_connections(group);
                break;
            case LB_STRATEGY_IP_HASH:
                selected = lb_ip_hash(group, client_ip);
                break;
            case LB_STRATEGY_RANDOM:
                selected = lb_random(group);
                break;
            case LB_STRATEGY_WEIGHTED_RANDOM:
                selected = lb_weighted_random(group);
                break;
            default:
                selected = lb_round_robin(group);
                break;
        }
    }
    
    if (!selected) return NULL;
    
    // 创建选择结果
    lb_selection_t *selection = calloc(1, sizeof(lb_selection_t));
    if (!selection) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for lb_selection_t");
        return NULL;
    }
    
    selection->server = selected;
    selection->proxy_url = lb_build_proxy_url(selected);
    selection->connection_id = 0; // 暂时不实现连接池
    
    // 更新统计信息
    selected->total_requests++;
    group->total_requests++;
    
    // 绑定会话（如果启用）
    if (group->session_persistence && (client_ip || session_id)) {
        session_bind(client_ip, session_id, selected);
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Selected server %s:%d for request (strategy=%d)", 
             selected->host, selected->port, group->strategy);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return selection;
}

upstream_server_t *lb_round_robin(upstream_group_t *group) {
    if (!group || group->server_count == 0) return NULL;
    
    pthread_mutex_lock(&group->mutex);
    
    upstream_server_t *server = group->servers;
    int current_index = 0;
    
    // 找到当前索引对应的服务器
    while (server && current_index < group->current_server_index) {
        server = server->next;
        current_index++;
    }
    
    // 从当前位置开始查找可用服务器
    upstream_server_t *start_server = server;
    int start_index = current_index;
    
    do {
        if (server && upstream_server_is_available(server)) {
            group->current_server_index = (current_index + 1) % group->server_count;
            pthread_mutex_unlock(&group->mutex);
            return server;
        }
        
        server = server ? server->next : group->servers;
        current_index = server ? (current_index + 1) % group->server_count : 0;
        
    } while (server != start_server || current_index != start_index);
    
    pthread_mutex_unlock(&group->mutex);
    return NULL;
}

upstream_server_t *lb_weighted_round_robin(upstream_group_t *group) {
    if (!group || group->server_count == 0) return NULL;
    
    pthread_mutex_lock(&group->mutex);
    
    upstream_server_t *best = NULL;
    int total = 0;
    
    upstream_server_t *server = group->servers;
    while (server) {
        if (upstream_server_is_available(server)) {
            server->current_weight += server->effective_weight;
            total += server->effective_weight;
            
            if (server->effective_weight < server->weight) {
                server->effective_weight++;
            }
            
            if (!best || server->current_weight > best->current_weight) {
                best = server;
            }
        }
        server = server->next;
    }
    
    if (best) {
        best->current_weight -= total;
    }
    
    pthread_mutex_unlock(&group->mutex);
    return best;
}

upstream_server_t *lb_least_connections(upstream_group_t *group) {
    if (!group || group->server_count == 0) return NULL;
    
    pthread_mutex_lock(&group->mutex);
    
    upstream_server_t *best = NULL;
    int min_connections = INT_MAX;
    
    upstream_server_t *server = group->servers;
    while (server) {
        if (upstream_server_is_available(server)) {
            if (server->current_connections < min_connections) {
                min_connections = server->current_connections;
                best = server;
            }
        }
        server = server->next;
    }
    
    pthread_mutex_unlock(&group->mutex);
    return best;
}

upstream_server_t *lb_ip_hash(upstream_group_t *group, const char *client_ip) {
    if (!group || group->server_count == 0 || !client_ip) return NULL;
    
    pthread_mutex_lock(&group->mutex);
    
    // 计算IP哈希值
    unsigned int hash = lb_hash_string(client_ip);
    int target_index = hash % group->server_count;
    
    upstream_server_t *server = group->servers;
    int current_index = 0;
    
    // 找到目标索引的服务器
    while (server && current_index < target_index) {
        server = server->next;
        current_index++;
    }
    
    // 从目标位置开始查找可用服务器
    upstream_server_t *start_server = server;
    int start_index = current_index;
    
    do {
        if (server && upstream_server_is_available(server)) {
            pthread_mutex_unlock(&group->mutex);
            return server;
        }
        
        server = server ? server->next : group->servers;
        current_index = server ? (current_index + 1) % group->server_count : 0;
        
    } while (server != start_server || current_index != start_index);
    
    pthread_mutex_unlock(&group->mutex);
    return NULL;
}

upstream_server_t *lb_random(upstream_group_t *group) {
    if (!group || group->server_count == 0) return NULL;
    
    pthread_mutex_lock(&group->mutex);
    
    // 统计可用服务器数量
    int available_count = 0;
    upstream_server_t *server = group->servers;
    while (server) {
        if (upstream_server_is_available(server)) {
            available_count++;
        }
        server = server->next;
    }
    
    if (available_count == 0) {
        pthread_mutex_unlock(&group->mutex);
        return NULL;
    }
    
    // 随机选择一个可用服务器
    int target_index = rand() % available_count;
    int current_index = 0;
    
    server = group->servers;
    while (server) {
        if (upstream_server_is_available(server)) {
            if (current_index == target_index) {
                pthread_mutex_unlock(&group->mutex);
                return server;
            }
            current_index++;
        }
        server = server->next;
    }
    
    pthread_mutex_unlock(&group->mutex);
    return NULL;
}

upstream_server_t *lb_weighted_random(upstream_group_t *group) {
    if (!group || group->server_count == 0) return NULL;
    
    pthread_mutex_lock(&group->mutex);
    
    // 计算总权重
    int total_weight = 0;
    upstream_server_t *server = group->servers;
    while (server) {
        if (upstream_server_is_available(server)) {
            total_weight += server->weight;
        }
        server = server->next;
    }
    
    if (total_weight == 0) {
        pthread_mutex_unlock(&group->mutex);
        return NULL;
    }
    
    // 随机选择权重点
    int random_weight = rand() % total_weight;
    int current_weight = 0;
    
    server = group->servers;
    while (server) {
        if (upstream_server_is_available(server)) {
            current_weight += server->weight;
            if (current_weight > random_weight) {
                pthread_mutex_unlock(&group->mutex);
                return server;
            }
        }
        server = server->next;
    }
    
    pthread_mutex_unlock(&group->mutex);
    return NULL;
}

// 健康检查
int lb_health_check_server(upstream_server_t *server) {
    if (!server) return -1;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Starting health check for server %s:%d", 
             server->host, server->port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    // 创建socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to create socket for health check");
        return -1;
    }
    
    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = server->health_check_timeout;
    timeout.tv_usec = 0;
    
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message(LOG_LEVEL_WARNING, "Failed to set socket timeout for health check");
    }
    
    // 解析主机名
    struct hostent *he = gethostbyname(server->host);
    if (!he) {
        snprintf(log_msg, sizeof(log_msg), "Failed to resolve hostname %s for health check", server->host);
        log_message(LOG_LEVEL_ERROR, log_msg);
        close(sock_fd);
        return -1;
    }
    
    // 连接到服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server->port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        snprintf(log_msg, sizeof(log_msg), "Health check failed for server %s:%d - connection failed", 
                 server->host, server->port);
        log_message(LOG_LEVEL_WARNING, log_msg);
        close(sock_fd);
        return -1;
    }
    
    // 发送HTTP健康检查请求
    char request[512];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "User-Agent: ANX-LoadBalancer/1.0\r\n"
             "Connection: close\r\n"
             "\r\n",
             server->health_check_uri, server->host, server->port);
    
    if (send(sock_fd, request, strlen(request), 0) < 0) {
        snprintf(log_msg, sizeof(log_msg), "Health check failed for server %s:%d - send failed", 
                 server->host, server->port);
        log_message(LOG_LEVEL_WARNING, log_msg);
        close(sock_fd);
        return -1;
    }
    
    // 接收响应
    char response[1024];
    ssize_t bytes_received = recv(sock_fd, response, sizeof(response) - 1, 0);
    close(sock_fd);
    
    if (bytes_received <= 0) {
        snprintf(log_msg, sizeof(log_msg), "Health check failed for server %s:%d - no response", 
                 server->host, server->port);
        log_message(LOG_LEVEL_WARNING, log_msg);
        return -1;
    }
    
    response[bytes_received] = '\0';
    
    // 检查HTTP状态码
    if (strncmp(response, "HTTP/1.1 200", 12) == 0 || strncmp(response, "HTTP/1.0 200", 12) == 0) {
        snprintf(log_msg, sizeof(log_msg), "Health check passed for server %s:%d", 
                 server->host, server->port);
        log_message(LOG_LEVEL_DEBUG, log_msg);
        return 0;
    } else {
        snprintf(log_msg, sizeof(log_msg), "Health check failed for server %s:%d - bad status code", 
                 server->host, server->port);
        log_message(LOG_LEVEL_WARNING, log_msg);
        return -1;
    }
}

void lb_health_check_all(upstream_group_t *group) {
    if (!group || !group->health_check_enabled) return;
    
    pthread_mutex_lock(&group->mutex);
    
    upstream_server_t *server = group->servers;
    while (server) {
        time_t now = time(NULL);
        
        // 检查是否需要进行健康检查
        if (now - server->last_check_time >= server->health_check_interval) {
            server_status_t old_status = server->status;
            
            if (lb_health_check_server(server) == 0) {
                // 健康检查通过
                if (old_status != SERVER_STATUS_UP) {
                    upstream_server_set_status(server, SERVER_STATUS_UP);
                    server->consecutive_failures = 0;
                }
            } else {
                // 健康检查失败
                server->consecutive_failures++;
                server->last_failure_time = now;
                
                if (server->consecutive_failures >= server->max_fails) {
                    upstream_server_set_status(server, SERVER_STATUS_DOWN);
                }
            }
            
            server->last_check_time = now;
        }
        
        server = server->next;
    }
    
    pthread_mutex_unlock(&group->mutex);
}

void *lb_health_check_thread(void *arg) {
    upstream_group_t *group = (upstream_group_t *)arg;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Health check thread started for group '%s'", group->name);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    while (1) {
        lb_health_check_all(group);
        sleep(group->health_check_interval);
    }
    
    return NULL;
}

// 会话管理
session_info_t *session_find(const char *client_ip, const char *session_id) {
    if (!client_ip && !session_id) return NULL;
    
    pthread_mutex_lock(&session_mutex);
    
    session_info_t *session = session_table;
    while (session) {
        int match = 0;
        
        if (client_ip && session->client_ip && strcmp(session->client_ip, client_ip) == 0) {
            match = 1;
        }
        
        if (session_id && session->session_id && strcmp(session->session_id, session_id) == 0) {
            match = 1;
        }
        
        if (match) {
            session->last_access = time(NULL);
            pthread_mutex_unlock(&session_mutex);
            return session;
        }
        
        session = session->next;
    }
    
    pthread_mutex_unlock(&session_mutex);
    return NULL;
}

int session_bind(const char *client_ip, const char *session_id, upstream_server_t *server) {
    if ((!client_ip && !session_id) || !server) return -1;
    
    pthread_mutex_lock(&session_mutex);
    
    // 检查会话是否已存在
    session_info_t *existing = session_find(client_ip, session_id);
    if (existing) {
        existing->server = server;
        existing->last_access = time(NULL);
        pthread_mutex_unlock(&session_mutex);
        return 0;
    }
    
    // 创建新会话
    session_info_t *session = calloc(1, sizeof(session_info_t));
    if (!session) {
        pthread_mutex_unlock(&session_mutex);
        return -1;
    }
    
    if (client_ip) {
        session->client_ip = strdup(client_ip);
    }
    if (session_id) {
        session->session_id = strdup(session_id);
    }
    
    session->server = server;
    session->last_access = time(NULL);
    
    // 添加到会话表
    session->next = session_table;
    session_table = session;
    
    pthread_mutex_unlock(&session_mutex);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Session bound: client=%s, server=%s:%d", 
             client_ip ? client_ip : "unknown", server->host, server->port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return 0;
}

void session_cleanup_expired(int timeout) {
    pthread_mutex_lock(&session_mutex);
    
    time_t now = time(NULL);
    session_info_t *prev = NULL;
    session_info_t *current = session_table;
    
    while (current) {
        if (now - current->last_access > timeout) {
            if (prev) {
                prev->next = current->next;
            } else {
                session_table = current->next;
            }
            
            session_info_t *to_free = current;
            current = current->next;
            
            free(to_free->client_ip);
            free(to_free->session_id);
            free(to_free);
        } else {
            prev = current;
            current = current->next;
        }
    }
    
    pthread_mutex_unlock(&session_mutex);
}

// 统计信息
lb_stats_t *lb_get_stats(upstream_group_t *group) {
    if (!group) return NULL;
    
    lb_stats_t *stats = calloc(1, sizeof(lb_stats_t));
    if (!stats) return NULL;
    
    pthread_mutex_lock(&group->mutex);
    
    stats->total_requests = group->total_requests;
    stats->successful_requests = group->successful_requests;
    stats->failed_requests = group->failed_requests;
    stats->last_updated = time(NULL);
    
    // 计算活跃连接数和平均响应时间
    int active_connections = 0;
    double total_response_time = 0.0;
    int server_count = 0;
    
    upstream_server_t *server = group->servers;
    while (server) {
        active_connections += server->current_connections;
        total_response_time += server->avg_response_time;
        server_count++;
        server = server->next;
    }
    
    stats->active_connections = active_connections;
    stats->avg_response_time = server_count > 0 ? total_response_time / server_count : 0.0;
    
    pthread_mutex_unlock(&group->mutex);
    
    return stats;
}

void lb_update_stats(upstream_server_t *server, int success, double response_time) {
    if (!server) return;
    
    if (success) {
        server->total_requests++;
        server->consecutive_failures = 0;
        
        // 更新平均响应时间
        if (server->avg_response_time == 0.0) {
            server->avg_response_time = response_time;
        } else {
            server->avg_response_time = (server->avg_response_time + response_time) / 2.0;
        }
        
        server->last_response_time = time(NULL);
    } else {
        server->failed_requests++;
        server->consecutive_failures++;
        server->last_failure_time = time(NULL);
    }
}

void lb_print_stats(upstream_group_t *group) {
    if (!group) return;
    
    lb_stats_t *stats = lb_get_stats(group);
    if (!stats) return;
    
    printf("Load Balancer Statistics for group '%s':\n", group->name);
    printf("  Strategy: %d\n", group->strategy);
    printf("  Total Requests: %d\n", stats->total_requests);
    printf("  Successful Requests: %d\n", stats->successful_requests);
    printf("  Failed Requests: %d\n", stats->failed_requests);
    printf("  Active Connections: %d\n", stats->active_connections);
    printf("  Average Response Time: %.2f ms\n", stats->avg_response_time);
    printf("  Servers:\n");
    
    pthread_mutex_lock(&group->mutex);
    upstream_server_t *server = group->servers;
    while (server) {
        printf("    %s:%d - Status: %d, Connections: %d, Requests: %d, Failures: %d\n",
               server->host, server->port, server->status, server->current_connections,
               server->total_requests, server->failed_requests);
        server = server->next;
    }
    pthread_mutex_unlock(&group->mutex);
    
    free(stats);
}

// 连接管理
int lb_connect_to_server(upstream_server_t *server) {
    if (!server || !upstream_server_is_available(server)) return -1;
    
    // 创建socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to create socket for server connection");
        return -1;
    }
    
    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message(LOG_LEVEL_WARNING, "Failed to set socket timeout");
    }
    
    // 解析主机名
    struct hostent *he = gethostbyname(server->host);
    if (!he) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to resolve hostname: %s", server->host);
        log_message(LOG_LEVEL_ERROR, log_msg);
        close(sock_fd);
        return -1;
    }
    
    // 连接到服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server->port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to connect to server %s:%d - %s", 
                 server->host, server->port, strerror(errno));
        log_message(LOG_LEVEL_ERROR, log_msg);
        close(sock_fd);
        return -1;
    }
    
    // 更新连接计数
    lb_update_connection_count(server, 1);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Successfully connected to server %s:%d", 
             server->host, server->port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return sock_fd;
}

void lb_close_connection(upstream_server_t *server, int connection_id) {
    if (!server || connection_id < 0) return;
    
    close(connection_id);
    lb_update_connection_count(server, -1);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Closed connection to server %s:%d", 
             server->host, server->port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
}

void lb_update_connection_count(upstream_server_t *server, int delta) {
    if (!server) return;
    
    server->current_connections += delta;
    if (server->current_connections < 0) {
        server->current_connections = 0;
    }
}

// 工具函数
char *lb_build_proxy_url(upstream_server_t *server) {
    if (!server) return NULL;
    
    char *url = malloc(256);
    if (!url) return NULL;
    
    snprintf(url, 256, "http://%s:%d", server->host, server->port);
    return url;
}

unsigned int lb_hash_string(const char *str) {
    if (!str) return 0;
    
    unsigned int hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash;
}

void lb_selection_free(lb_selection_t *selection) {
    if (!selection) return;
    
    free(selection->proxy_url);
    free(selection);
}

// 配置解析
int lb_parse_upstream_block(const char *block_content, lb_config_t *config) {
    // 简化的配置解析实现
    // 实际实现需要更复杂的解析逻辑
    return 0;
}

int lb_parse_server_directive(const char *directive, upstream_group_t *group) {
    // 简化的解析实现
    if (!directive || !group) return -1;
    
    // 这里应该实现完整的服务器指令解析逻辑
    log_message(LOG_LEVEL_DEBUG, "Server directive parsed");
    return 0;
}

// 健康检查管理器集成
int lb_start_health_check_manager(upstream_group_t *group, health_check_config_t *config) {
    if (!group || !config) return -1;
    
    pthread_mutex_lock(&group->mutex);
    
    // 如果已经有健康检查管理器在运行，先停止它
    if (group->health_manager) {
        health_check_manager_stop(group->health_manager);
        health_check_manager_free(group->health_manager);
    }
    
    // 创建新的健康检查管理器
    group->health_manager = health_check_manager_create(group, config);
    if (!group->health_manager) {
        pthread_mutex_unlock(&group->mutex);
        log_message(LOG_LEVEL_ERROR, "Failed to create health check manager");
        return -1;
    }
    
    // 启动健康检查管理器
    int result = health_check_manager_start(group->health_manager);
    if (result != 0) {
        health_check_manager_free(group->health_manager);
        group->health_manager = NULL;
        pthread_mutex_unlock(&group->mutex);
        log_message(LOG_LEVEL_ERROR, "Failed to start health check manager");
        return -1;
    }
    
    pthread_mutex_unlock(&group->mutex);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Health check manager started for group '%s'", group->name);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return 0;
}

int lb_stop_health_check_manager(upstream_group_t *group) {
    if (!group) return -1;
    
    pthread_mutex_lock(&group->mutex);
    
    if (!group->health_manager) {
        pthread_mutex_unlock(&group->mutex);
        return 0; // 没有运行的管理器
    }
    
    // 停止健康检查管理器
    health_check_manager_stop(group->health_manager);
    health_check_manager_free(group->health_manager);
    group->health_manager = NULL;
    
    pthread_mutex_unlock(&group->mutex);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Health check manager stopped for group '%s'", group->name);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return 0;
}

int lb_is_health_check_running(upstream_group_t *group) {
    if (!group) return 0;
    
    pthread_mutex_lock(&group->mutex);
    
    int running = 0;
    if (group->health_manager) {
        running = health_check_manager_is_running(group->health_manager);
    }
    
    pthread_mutex_unlock(&group->mutex);
    
    return running;
}

// 策略转换函数
const char *lb_strategy_to_string(lb_strategy_t strategy) {
    switch (strategy) {
        case LB_STRATEGY_ROUND_ROBIN:
            return "round_robin";
        case LB_STRATEGY_WEIGHTED_ROUND_ROBIN:
            return "weighted_round_robin";
        case LB_STRATEGY_LEAST_CONNECTIONS:
            return "least_conn";
        case LB_STRATEGY_IP_HASH:
            return "ip_hash";
        case LB_STRATEGY_RANDOM:
            return "random";
        case LB_STRATEGY_WEIGHTED_RANDOM:
            return "weighted_random";
        default:
            return "unknown";
    }
}

lb_strategy_t lb_strategy_from_string(const char *strategy_str) {
    if (!strategy_str) return LB_STRATEGY_ROUND_ROBIN;
    
    if (strcmp(strategy_str, "round_robin") == 0) {
        return LB_STRATEGY_ROUND_ROBIN;
    } else if (strcmp(strategy_str, "weighted_round_robin") == 0) {
        return LB_STRATEGY_WEIGHTED_ROUND_ROBIN;
    } else if (strcmp(strategy_str, "least_conn") == 0) {
        return LB_STRATEGY_LEAST_CONNECTIONS;
    } else if (strcmp(strategy_str, "ip_hash") == 0) {
        return LB_STRATEGY_IP_HASH;
    } else if (strcmp(strategy_str, "random") == 0) {
        return LB_STRATEGY_RANDOM;
    } else if (strcmp(strategy_str, "weighted_random") == 0) {
        return LB_STRATEGY_WEIGHTED_RANDOM;
    } else {
        return LB_STRATEGY_ROUND_ROBIN; // 默认策略
    }
}

// 负载均衡算法名称转换（为了与health_api.c的兼容性）
const char *lb_algorithm_to_string(lb_strategy_t algorithm) {
    return lb_strategy_to_string(algorithm);
} 