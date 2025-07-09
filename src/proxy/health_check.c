#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "health_check.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>

// 健康检查配置管理
health_check_config_t *health_check_config_create(void) {
    health_check_config_t *config = calloc(1, sizeof(health_check_config_t));
    if (!config) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for health_check_config_t");
        return NULL;
    }
    
    // 设置默认值
    config->type = HEALTH_CHECK_HTTP;
    config->uri = strdup("/health");
    config->method = strdup("GET");
    config->timeout = 10;
    config->interval = 30;
    config->retries = 3;
    config->rise = 2;
    config->fall = 3;
    config->enabled = true;
    config->user_agent = strdup("ANX-HealthCheck/1.0");
    config->port = 0; // 使用服务器端口
    
    log_message(LOG_LEVEL_DEBUG, "Health check config created with defaults");
    return config;
}

void health_check_config_free(health_check_config_t *config) {
    if (!config) return;
    
    free(config->uri);
    free(config->method);
    free(config->expected_response);
    free(config->headers);
    free(config->user_agent);
    free(config);
    
    log_message(LOG_LEVEL_DEBUG, "Health check config freed");
}

// 健康检查结果管理
health_check_result_t *health_check_result_create(void) {
    health_check_result_t *result = calloc(1, sizeof(health_check_result_t));
    if (!result) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for health_check_result_t");
        return NULL;
    }
    
    result->status = HEALTH_STATUS_UNKNOWN;
    result->check_time = time(NULL);
    result->response_time = -1.0;
    
    return result;
}

void health_check_result_free(health_check_result_t *result) {
    if (!result) return;
    
    free(result->error_message);
    free(result);
}

// 健康检查历史记录
health_check_history_t *health_check_history_create(int capacity) {
    if (capacity <= 0) capacity = 100; // 默认容量
    
    health_check_history_t *history = calloc(1, sizeof(health_check_history_t));
    if (!history) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for health_check_history_t");
        return NULL;
    }
    
    history->results = calloc(capacity, sizeof(health_check_result_t));
    if (!history->results) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for health check results");
        free(history);
        return NULL;
    }
    
    history->capacity = capacity;
    history->count = 0;
    history->index = 0;
    
    if (pthread_mutex_init(&history->mutex, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize mutex for health check history");
        free(history->results);
        free(history);
        return NULL;
    }
    
    log_message(LOG_LEVEL_DEBUG, "Health check history created");
    return history;
}

void health_check_history_free(health_check_history_t *history) {
    if (!history) return;
    
    pthread_mutex_lock(&history->mutex);
    
    for (int i = 0; i < history->count; i++) {
        free(history->results[i].error_message);
    }
    free(history->results);
    
    pthread_mutex_unlock(&history->mutex);
    pthread_mutex_destroy(&history->mutex);
    free(history);
    
    log_message(LOG_LEVEL_DEBUG, "Health check history freed");
}

// 健康检查执行
health_check_result_t *health_check_execute(upstream_server_t *server, health_check_config_t *config) {
    if (!server || !config) return NULL;
    
    health_check_result_t *result = NULL;
    
    switch (config->type) {
        case HEALTH_CHECK_HTTP:
            result = health_check_http(server, config);
            break;
        case HEALTH_CHECK_HTTPS:
            result = health_check_https(server, config);
            break;
        case HEALTH_CHECK_TCP:
            result = health_check_tcp(server, config);
            break;
        case HEALTH_CHECK_PING:
            result = health_check_ping(server, config);
            break;
        default:
            result = health_check_http(server, config);
            break;
    }
    
    return result;
}

health_check_result_t *health_check_http(upstream_server_t *server, health_check_config_t *config) {
    if (!server || !config) return NULL;
    
    health_check_result_t *result = health_check_result_create();
    if (!result) return NULL;
    
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Starting HTTP health check for %s:%d", 
             server->host, server->port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    // 创建socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        result->error_message = strdup("Failed to create socket");
        log_message(LOG_LEVEL_ERROR, "Failed to create socket for health check");
        return result;
    }
    
    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = config->timeout;
    timeout.tv_usec = 0;
    
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message(LOG_LEVEL_WARNING, "Failed to set socket timeout");
    }
    
    // 解析主机名
    struct hostent *he = gethostbyname(server->host);
    if (!he) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        result->error_message = strdup("Failed to resolve hostname");
        close(sock_fd);
        return result;
    }
    
    // 连接到服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config->port > 0 ? config->port : server->port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        result->error_message = strdup("Connection failed");
        close(sock_fd);
        return result;
    }
    
    // 构建HTTP请求
    char request[2048];
    snprintf(request, sizeof(request),
             "%s %s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "User-Agent: %s\r\n"
             "Connection: close\r\n"
             "%s"
             "\r\n",
             config->method, config->uri, server->host, server->port,
             config->user_agent,
             config->headers ? config->headers : "");
    
    // 发送请求
    if (send(sock_fd, request, strlen(request), 0) < 0) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        result->error_message = strdup("Failed to send request");
        close(sock_fd);
        return result;
    }
    
    // 接收响应
    char response[4096];
    ssize_t bytes_received = recv(sock_fd, response, sizeof(response) - 1, 0);
    close(sock_fd);
    
    gettimeofday(&end_time, NULL);
    result->response_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                           (end_time.tv_usec - start_time.tv_usec) / 1000.0;
    
    if (bytes_received <= 0) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        result->error_message = strdup("No response received");
        return result;
    }
    
    response[bytes_received] = '\0';
    result->response_size = bytes_received;
    
    // 解析HTTP状态码
    if (sscanf(response, "HTTP/1.%*d %d", &result->response_code) != 1) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        result->error_message = strdup("Invalid HTTP response");
        return result;
    }
    
    // 检查状态码
    if (result->response_code >= 200 && result->response_code < 300) {
        // 检查期望的响应内容
        if (config->expected_response) {
            if (strstr(response, config->expected_response)) {
                result->status = HEALTH_STATUS_HEALTHY;
            } else {
                result->status = HEALTH_STATUS_UNHEALTHY;
                result->error_message = strdup("Response content mismatch");
            }
        } else {
            result->status = HEALTH_STATUS_HEALTHY;
        }
    } else {
        result->status = HEALTH_STATUS_UNHEALTHY;
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "HTTP error: %d", result->response_code);
        result->error_message = strdup(error_msg);
    }
    
    snprintf(log_msg, sizeof(log_msg), "HTTP health check completed for %s:%d - Status: %s, Response time: %.2fms", 
             server->host, server->port, 
             health_status_to_string(result->status), result->response_time);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return result;
}

health_check_result_t *health_check_tcp(upstream_server_t *server, health_check_config_t *config) {
    if (!server || !config) return NULL;
    
    health_check_result_t *result = health_check_result_create();
    if (!result) return NULL;
    
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Starting TCP health check for %s:%d", 
             server->host, server->port);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    // 创建socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        result->error_message = strdup("Failed to create socket");
        return result;
    }
    
    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = config->timeout;
    timeout.tv_usec = 0;
    
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message(LOG_LEVEL_WARNING, "Failed to set socket timeout");
    }
    
    // 解析主机名
    struct hostent *he = gethostbyname(server->host);
    if (!he) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        result->error_message = strdup("Failed to resolve hostname");
        close(sock_fd);
        return result;
    }
    
    // 连接到服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config->port > 0 ? config->port : server->port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        result->error_message = strdup("TCP connection failed");
        close(sock_fd);
        return result;
    }
    
    close(sock_fd);
    
    gettimeofday(&end_time, NULL);
    result->response_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                           (end_time.tv_usec - start_time.tv_usec) / 1000.0;
    
    result->status = HEALTH_STATUS_HEALTHY;
    result->response_code = 0; // TCP连接成功
    
    snprintf(log_msg, sizeof(log_msg), "TCP health check completed for %s:%d - Status: %s, Response time: %.2fms", 
             server->host, server->port, 
             health_status_to_string(result->status), result->response_time);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return result;
}

// 工具函数
const char *health_status_to_string(health_status_t status) {
    switch (status) {
        case HEALTH_STATUS_HEALTHY:   return "HEALTHY";
        case HEALTH_STATUS_UNHEALTHY: return "UNHEALTHY";
        case HEALTH_STATUS_CHECKING:  return "CHECKING";
        case HEALTH_STATUS_UNKNOWN:   return "UNKNOWN";
        case HEALTH_STATUS_DEGRADED:  return "DEGRADED";
        default:                      return "UNKNOWN";
    }
}

const char *health_check_type_to_string(health_check_type_t type) {
    switch (type) {
        case HEALTH_CHECK_HTTP:    return "HTTP";
        case HEALTH_CHECK_HTTPS:   return "HTTPS";
        case HEALTH_CHECK_TCP:     return "TCP";
        case HEALTH_CHECK_PING:    return "PING";
        case HEALTH_CHECK_CUSTOM:  return "CUSTOM";
        default:                   return "UNKNOWN";
    }
}

// 健康检查HTTPS实现（简化版）
health_check_result_t *health_check_https(upstream_server_t *server, health_check_config_t *config) {
    // 简化实现，实际应该使用SSL/TLS
    return health_check_tcp(server, config);
}

// 健康检查PING实现（简化版）
health_check_result_t *health_check_ping(upstream_server_t *server, health_check_config_t *config) {
    // 简化实现，实际应该使用ICMP
    return health_check_tcp(server, config);
}

// 健康检查配置设置函数
int health_check_config_set_uri(health_check_config_t *config, const char *uri) {
    if (!config || !uri) return -1;
    
    free(config->uri);
    config->uri = strdup(uri);
    return config->uri ? 0 : -1;
}

int health_check_config_set_method(health_check_config_t *config, const char *method) {
    if (!config || !method) return -1;
    
    free(config->method);
    config->method = strdup(method);
    return config->method ? 0 : -1;
}

int health_check_config_set_headers(health_check_config_t *config, const char *headers) {
    if (!config || !headers) return -1;
    
    free(config->headers);
    config->headers = strdup(headers);
    return config->headers ? 0 : -1;
}

int health_check_config_set_expected_response(health_check_config_t *config, const char *response) {
    if (!config || !response) return -1;
    
    free(config->expected_response);
    config->expected_response = strdup(response);
    return config->expected_response ? 0 : -1;
}

// 健康检查历史记录管理
int health_check_history_add(health_check_history_t *history, const health_check_result_t *result) {
    if (!history || !result) return -1;
    
    pthread_mutex_lock(&history->mutex);
    
    // 复制结果到历史记录
    health_check_result_t *stored_result = &history->results[history->index];
    
    // 释放旧的错误消息
    if (stored_result->error_message) {
        free(stored_result->error_message);
    }
    
    // 复制新结果
    *stored_result = *result;
    stored_result->error_message = result->error_message ? strdup(result->error_message) : NULL;
    
    // 更新索引和计数
    history->index = (history->index + 1) % history->capacity;
    if (history->count < history->capacity) {
        history->count++;
    }
    
    pthread_mutex_unlock(&history->mutex);
    
    log_message(LOG_LEVEL_DEBUG, "Health check result added to history");
    return 0;
}

health_check_result_t *health_check_history_get_latest(health_check_history_t *history) {
    if (!history || history->count == 0) return NULL;
    
    pthread_mutex_lock(&history->mutex);
    
    int latest_index = (history->index - 1 + history->capacity) % history->capacity;
    health_check_result_t *latest = &history->results[latest_index];
    
    pthread_mutex_unlock(&history->mutex);
    
    return latest;
}

health_check_result_t **health_check_history_get_all(health_check_history_t *history, int *count) {
    if (!history || !count) return NULL;
    
    pthread_mutex_lock(&history->mutex);
    
    *count = history->count;
    health_check_result_t **results = malloc(sizeof(health_check_result_t *) * history->count);
    
    if (!results) {
        pthread_mutex_unlock(&history->mutex);
        return NULL;
    }
    
    for (int i = 0; i < history->count; i++) {
        int index = (history->index - history->count + i + history->capacity) % history->capacity;
        results[i] = &history->results[index];
    }
    
    pthread_mutex_unlock(&history->mutex);
    
    return results;
}

// 健康检查结果复制
health_check_result_t *health_check_result_copy(const health_check_result_t *src) {
    if (!src) return NULL;
    
    health_check_result_t *copy = health_check_result_create();
    if (!copy) return NULL;
    
    *copy = *src;
    copy->error_message = src->error_message ? strdup(src->error_message) : NULL;
    
    return copy;
}

// 健康检查管理器
health_check_manager_t *health_check_manager_create(upstream_group_t *group, health_check_config_t *config) {
    if (!group || !config) return NULL;
    
    health_check_manager_t *manager = calloc(1, sizeof(health_check_manager_t));
    if (!manager) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for health_check_manager_t");
        return NULL;
    }
    
    manager->group = group;
    manager->config = config;
    manager->running = false;
    manager->consecutive_successes = 0;
    manager->consecutive_failures = 0;
    manager->last_check_time = 0;
    manager->next_check_time = time(NULL) + config->interval;
    
    // 创建历史记录
    manager->history = health_check_history_create(100);
    if (!manager->history) {
        free(manager);
        return NULL;
    }
    
    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize mutex for health check manager");
        health_check_history_free(manager->history);
        free(manager);
        return NULL;
    }
    
    log_message(LOG_LEVEL_INFO, "Health check manager created");
    return manager;
}

void health_check_manager_free(health_check_manager_t *manager) {
    if (!manager) return;
    
    // 停止检查线程
    health_check_manager_stop(manager);
    
    pthread_mutex_destroy(&manager->mutex);
    health_check_history_free(manager->history);
    free(manager);
    
    log_message(LOG_LEVEL_INFO, "Health check manager freed");
}

int health_check_manager_start(health_check_manager_t *manager) {
    if (!manager) return -1;
    
    pthread_mutex_lock(&manager->mutex);
    
    if (manager->running) {
        pthread_mutex_unlock(&manager->mutex);
        return 0; // 已经在运行
    }
    
    manager->running = true;
    
    if (pthread_create(&manager->check_thread, NULL, health_check_thread_worker, manager) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to create health check thread");
        manager->running = false;
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    pthread_mutex_unlock(&manager->mutex);
    
    log_message(LOG_LEVEL_INFO, "Health check manager started");
    return 0;
}

int health_check_manager_stop(health_check_manager_t *manager) {
    if (!manager) return -1;
    
    pthread_mutex_lock(&manager->mutex);
    
    if (!manager->running) {
        pthread_mutex_unlock(&manager->mutex);
        return 0; // 已经停止
    }
    
    manager->running = false;
    
    pthread_mutex_unlock(&manager->mutex);
    
    // 等待线程结束
    pthread_join(manager->check_thread, NULL);
    
    log_message(LOG_LEVEL_INFO, "Health check manager stopped");
    return 0;
}

bool health_check_manager_is_running(health_check_manager_t *manager) {
    if (!manager) return false;
    
    pthread_mutex_lock(&manager->mutex);
    bool running = manager->running;
    pthread_mutex_unlock(&manager->mutex);
    
    return running;
}

// 健康检查线程工作函数
void *health_check_thread_worker(void *arg) {
    health_check_manager_t *manager = (health_check_manager_t *)arg;
    if (!manager) return NULL;
    
    log_message(LOG_LEVEL_INFO, "Health check thread started");
    
    while (health_check_manager_is_running(manager)) {
        time_t current_time = time(NULL);
        
        pthread_mutex_lock(&manager->mutex);
        
        // 检查是否到了检查时间
        if (current_time >= manager->next_check_time) {
            // 对每个服务器进行健康检查
            for (int i = 0; i < manager->group->server_count; i++) {
                upstream_server_t *server = &manager->group->servers[i];
                
                // 执行健康检查
                health_check_result_t *result = health_check_execute(server, manager->config);
                if (result) {
                    health_check_process_result(manager, result);
                    health_check_update_server_status(server, result, manager->config);
                    health_check_history_add(manager->history, result);
                    health_check_result_free(result);
                }
            }
            
            // 更新检查时间
            manager->last_check_time = current_time;
            manager->next_check_time = current_time + manager->config->interval;
        }
        
        pthread_mutex_unlock(&manager->mutex);
        
        // 短暂休眠，避免过度占用CPU
        usleep(100000); // 100ms
    }
    
    log_message(LOG_LEVEL_INFO, "Health check thread stopped");
    return NULL;
}

// 处理健康检查结果
void health_check_process_result(health_check_manager_t *manager, health_check_result_t *result) {
    if (!manager || !result) return;
    
    if (result->status == HEALTH_STATUS_HEALTHY) {
        manager->consecutive_successes++;
        manager->consecutive_failures = 0;
    } else {
        manager->consecutive_failures++;
        manager->consecutive_successes = 0;
    }
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "Health check result processed - Status: %s, Consecutive successes: %d, failures: %d",
             health_status_to_string(result->status),
             manager->consecutive_successes,
             manager->consecutive_failures);
    log_message(LOG_LEVEL_DEBUG, log_msg);
}

// 更新服务器状态
void health_check_update_server_status(upstream_server_t *server, health_check_result_t *result, 
                                     health_check_config_t *config) {
    if (!server || !result || !config) return;
    
    // 根据连续成功/失败次数更新服务器状态
    if (result->status == HEALTH_STATUS_HEALTHY) {
        if (server->status == SERVER_STATUS_DOWN || server->status == SERVER_STATUS_UNKNOWN) {
            // 需要连续成功才能恢复
            static int consecutive_successes = 0;
            consecutive_successes++;
            
            if (consecutive_successes >= config->rise) {
                server->status = SERVER_STATUS_UP;
                consecutive_successes = 0;
                log_message(LOG_LEVEL_INFO, "Server recovered and marked as UP");
            }
        }
    } else {
        if (server->status == SERVER_STATUS_UP) {
            // 需要连续失败才能标记为DOWN
            static int consecutive_failures = 0;
            consecutive_failures++;
            
            if (consecutive_failures >= config->fall) {
                server->status = SERVER_STATUS_DOWN;
                consecutive_failures = 0;
                log_message(LOG_LEVEL_WARNING, "Server marked as DOWN due to health check failures");
            }
        }
    }
}

// 健康检查统计
health_check_stats_t *health_check_get_stats(health_check_manager_t *manager) {
    if (!manager) return NULL;
    
    health_check_stats_t *stats = calloc(1, sizeof(health_check_stats_t));
    if (!stats) return NULL;
    
    pthread_mutex_lock(&manager->mutex);
    
    // 从历史记录计算统计信息
    double total_response_time = 0.0;
    stats->min_response_time = -1.0;
    stats->max_response_time = -1.0;
    
    for (int i = 0; i < manager->history->count; i++) {
        health_check_result_t *result = &manager->history->results[i];
        
        stats->total_checks++;
        
        if (result->status == HEALTH_STATUS_HEALTHY) {
            stats->successful_checks++;
            if (stats->last_success_time < result->check_time) {
                stats->last_success_time = result->check_time;
            }
        } else {
            stats->failed_checks++;
            if (stats->last_failure_time < result->check_time) {
                stats->last_failure_time = result->check_time;
            }
        }
        
        if (result->is_timeout) {
            stats->timeout_checks++;
        }
        
        if (result->response_time > 0) {
            total_response_time += result->response_time;
            
            if (stats->min_response_time < 0 || result->response_time < stats->min_response_time) {
                stats->min_response_time = result->response_time;
            }
            
            if (stats->max_response_time < 0 || result->response_time > stats->max_response_time) {
                stats->max_response_time = result->response_time;
            }
        }
    }
    
    if (stats->total_checks > 0) {
        stats->avg_response_time = total_response_time / stats->total_checks;
        stats->uptime_percentage = (stats->successful_checks * 100) / stats->total_checks;
    }
    
    pthread_mutex_unlock(&manager->mutex);
    
    return stats;
}

void health_check_stats_free(health_check_stats_t *stats) {
    if (stats) {
        free(stats);
    }
}

void health_check_print_stats(health_check_manager_t *manager) {
    if (!manager) return;
    
    health_check_stats_t *stats = health_check_get_stats(manager);
    if (!stats) return;
    
    log_message(LOG_LEVEL_INFO, "=== Health Check Statistics ===");
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Total checks: %d", stats->total_checks);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    snprintf(log_msg, sizeof(log_msg), "Successful checks: %d", stats->successful_checks);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    snprintf(log_msg, sizeof(log_msg), "Failed checks: %d", stats->failed_checks);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    snprintf(log_msg, sizeof(log_msg), "Timeout checks: %d", stats->timeout_checks);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    snprintf(log_msg, sizeof(log_msg), "Uptime: %d%%", stats->uptime_percentage);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    if (stats->avg_response_time > 0) {
        snprintf(log_msg, sizeof(log_msg), "Average response time: %.2fms", stats->avg_response_time);
        log_message(LOG_LEVEL_INFO, log_msg);
    }
    
    health_check_stats_free(stats);
}

// 工具函数
double health_check_calculate_uptime(health_check_history_t *history, time_t duration) {
    if (!history || history->count == 0) return 0.0;
    
    pthread_mutex_lock(&history->mutex);
    
    int healthy_count = 0;
    time_t cutoff_time = time(NULL) - duration;
    
    for (int i = 0; i < history->count; i++) {
        health_check_result_t *result = &history->results[i];
        
        if (result->check_time >= cutoff_time) {
            if (result->status == HEALTH_STATUS_HEALTHY) {
                healthy_count++;
            }
        }
    }
    
    pthread_mutex_unlock(&history->mutex);
    
    return (double)healthy_count / history->count * 100.0;
}

bool health_check_is_response_valid(const char *response, const char *expected) {
    if (!response || !expected) return false;
    
    return strstr(response, expected) != NULL;
}

// 简化的配置解析函数
int health_check_parse_config(const char *config_str, health_check_config_t *config) {
    if (!config_str || !config) return -1;
    
    // 这里应该实现完整的配置解析逻辑
    // 现在只是一个简化版本
    
    log_message(LOG_LEVEL_DEBUG, "Health check config parsed");
    return 0;
}

int health_check_parse_upstream_health_config(const char *block_content, upstream_group_t *group) {
    if (!block_content || !group) return -1;
    
    // 这里应该实现完整的upstream健康检查配置解析
    // 现在只是一个简化版本
    
    log_message(LOG_LEVEL_DEBUG, "Upstream health check config parsed");
    return 0;
}

// API函数实现
int health_check_api_get_status(upstream_group_t *group, char *response, size_t response_size) {
    if (!group || !response) return -1;
    
    // 简化的状态响应
    snprintf(response, response_size, 
             "{ \"group\": \"%s\", \"servers\": %d, \"status\": \"active\" }",
             group->name, group->server_count);
    
    return 0;
}

int health_check_api_get_history(upstream_server_t *server, char *response, size_t response_size) {
    if (!server || !response) return -1;
    
    // 简化的历史响应
    snprintf(response, response_size,
             "{ \"server\": \"%s:%d\", \"status\": \"%s\" }",
             server->host, server->port, 
             server->status == SERVER_STATUS_UP ? "UP" : "DOWN");
    
    return 0;
}

int health_check_api_force_check(upstream_server_t *server) {
    if (!server) return -1;
    
    log_message(LOG_LEVEL_INFO, "Force health check requested for server");
    
    // 这里应该触发立即健康检查
    return 0;
} 