#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "bandwidth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <fnmatch.h>
#include <openssl/ssl.h>
#include "log.h"

#define DEFAULT_BURST_SIZE 65536      // 64KB 默认突发大小
#define DEFAULT_RATE_LIMIT 1048576    // 1MB/s 默认速率限制
#define MIN_SLEEP_USEC 1000           // 最小睡眠时间（微秒）
#define MAX_SLEEP_USEC 100000         // 最大睡眠时间（微秒）

// 全局带宽统计
static bandwidth_stats_t global_bandwidth_stats = {0};

// 创建带宽限制配置
bandwidth_config_t *bandwidth_config_create(void) {
    bandwidth_config_t *config = calloc(1, sizeof(bandwidth_config_t));
    if (!config) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for bandwidth config");
        return NULL;
    }
    
    config->enable_bandwidth_limit = false;
    config->default_rate_limit = DEFAULT_RATE_LIMIT;
    config->default_burst_size = DEFAULT_BURST_SIZE;
    config->min_file_size = 1024 * 1024; // 1MB
    config->rules = NULL;
    config->rules_count = 0;
    
    log_message(LOG_LEVEL_DEBUG, "Bandwidth config created with defaults");
    return config;
}

// 释放带宽限制配置
void bandwidth_config_free(bandwidth_config_t *config) {
    if (!config) return;
    
    bandwidth_rule_t *rule = config->rules;
    while (rule) {
        bandwidth_rule_t *next = rule->next;
        free(rule->path_pattern);
        free(rule->mime_type);
        free(rule->client_ip_pattern);
        free(rule);
        rule = next;
    }
    
    free(config);
    log_message(LOG_LEVEL_DEBUG, "Bandwidth config freed");
}

// 添加带宽限制规则
int bandwidth_config_add_rule(bandwidth_config_t *config, const char *path_pattern,
                              const char *mime_type, const char *client_ip_pattern,
                              size_t rate_limit, bandwidth_unit_t unit, size_t burst_size) {
    if (!config) return -1;
    
    bandwidth_rule_t *rule = calloc(1, sizeof(bandwidth_rule_t));
    if (!rule) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for bandwidth rule");
        return -1;
    }
    
    rule->path_pattern = path_pattern ? strdup(path_pattern) : NULL;
    rule->mime_type = mime_type ? strdup(mime_type) : NULL;
    rule->client_ip_pattern = client_ip_pattern ? strdup(client_ip_pattern) : NULL;
    rule->rate_limit = bandwidth_convert_to_bytes_per_second(rate_limit, unit);
    rule->unit = unit;
    rule->burst_size = burst_size > 0 ? burst_size : config->default_burst_size;
    rule->enabled = true;
    
    // 添加到链表头部
    rule->next = config->rules;
    config->rules = rule;
    config->rules_count++;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Added bandwidth rule: pattern=%s, rate=%zu %s, burst=%zu",
             path_pattern ? path_pattern : "*", rate_limit, bandwidth_unit_to_string(unit), burst_size);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    return 0;
}

// 查找匹配的带宽限制规则
bandwidth_rule_t *bandwidth_config_find_rule(bandwidth_config_t *config, 
                                            const char *path, const char *mime_type,
                                            const char *client_ip) {
    if (!config || !config->enable_bandwidth_limit) return NULL;
    
    bandwidth_rule_t *rule = config->rules;
    while (rule) {
        if (!rule->enabled) {
            rule = rule->next;
            continue;
        }
        
        // 检查路径模式
        if (rule->path_pattern && path) {
            if (!bandwidth_match_pattern(rule->path_pattern, path)) {
                rule = rule->next;
                continue;
            }
        }
        
        // 检查MIME类型
        if (rule->mime_type && mime_type) {
            if (!bandwidth_match_pattern(rule->mime_type, mime_type)) {
                rule = rule->next;
                continue;
            }
        }
        
        // 检查客户端IP
        if (rule->client_ip_pattern && client_ip) {
            if (!bandwidth_match_ip_pattern(rule->client_ip_pattern, client_ip)) {
                rule = rule->next;
                continue;
            }
        }
        
        return rule; // 找到匹配的规则
    }
    
    return NULL; // 没有找到匹配的规则
}

// 创建带宽控制器
bandwidth_controller_t *bandwidth_controller_create(size_t rate_limit, size_t burst_size) {
    bandwidth_controller_t *controller = calloc(1, sizeof(bandwidth_controller_t));
    if (!controller) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for bandwidth controller");
        return NULL;
    }
    
    if (bandwidth_controller_init(controller, rate_limit, burst_size) < 0) {
        free(controller);
        return NULL;
    }
    
    return controller;
}

// 释放带宽控制器
void bandwidth_controller_free(bandwidth_controller_t *controller) {
    if (controller) {
        free(controller);
    }
}

// 初始化带宽控制器
int bandwidth_controller_init(bandwidth_controller_t *controller, size_t rate_limit, size_t burst_size) {
    if (!controller) return -1;
    
    controller->rate_limit = rate_limit > 0 ? rate_limit : DEFAULT_RATE_LIMIT;
    controller->burst_size = burst_size > 0 ? burst_size : DEFAULT_BURST_SIZE;
    controller->tokens = controller->burst_size; // 开始时令牌桶是满的
    controller->bytes_sent = 0;
    controller->active = true;
    
    gettimeofday(&controller->last_update, NULL);
    controller->start_time = controller->last_update;
    
    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "Bandwidth controller initialized: rate=%zu B/s, burst=%zu B",
             controller->rate_limit, controller->burst_size);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return 0;
}

// 更新令牌桶中的令牌数量
void bandwidth_controller_update_tokens(bandwidth_controller_t *controller) {
    if (!controller || !controller->active) return;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // 计算时间差（微秒）
    long time_diff_usec = (now.tv_sec - controller->last_update.tv_sec) * 1000000 +
                          (now.tv_usec - controller->last_update.tv_usec);
    
    if (time_diff_usec <= 0) return;
    
    // 计算应该添加的令牌数
    size_t tokens_to_add = (controller->rate_limit * time_diff_usec) / 1000000;
    
    // 更新令牌数，但不超过突发大小
    controller->tokens += tokens_to_add;
    if (controller->tokens > controller->burst_size) {
        controller->tokens = controller->burst_size;
    }
    
    controller->last_update = now;
}

// 获取令牌（令牌桶算法）
int bandwidth_controller_acquire(bandwidth_controller_t *controller, size_t bytes) {
    if (!controller || !controller->active || bytes == 0) return 0;
    
    bandwidth_controller_update_tokens(controller);
    
    if (controller->tokens >= bytes) {
        controller->tokens -= bytes;
        controller->bytes_sent += bytes;
        return 0; // 可以立即发送
    }
    
    // 需要等待的时间（微秒）
    size_t needed_tokens = bytes - controller->tokens;
    size_t wait_time_usec = (needed_tokens * 1000000) / controller->rate_limit;
    
    // 限制等待时间
    if (wait_time_usec < MIN_SLEEP_USEC) wait_time_usec = MIN_SLEEP_USEC;
    if (wait_time_usec > MAX_SLEEP_USEC) wait_time_usec = MAX_SLEEP_USEC;
    
    return wait_time_usec;
}

// 计算发送指定字节数需要的延迟时间
size_t bandwidth_controller_calculate_delay(bandwidth_controller_t *controller, size_t bytes) {
    if (!controller || !controller->active || bytes == 0) return 0;
    
    bandwidth_controller_update_tokens(controller);
    
    if (controller->tokens >= bytes) {
        return 0; // 不需要延迟
    }
    
    size_t needed_tokens = bytes - controller->tokens;
    return (needed_tokens * 1000000) / controller->rate_limit;
}

// 检查是否可以发送指定字节数
bool bandwidth_controller_can_send(bandwidth_controller_t *controller, size_t bytes) {
    if (!controller || !controller->active) return true;
    
    bandwidth_controller_update_tokens(controller);
    return controller->tokens >= bytes;
}

// 带宽控制的发送函数
int bandwidth_controlled_send(int socket_fd, const char *data, size_t size, 
                             bandwidth_controller_t *controller) {
    if (!data || size == 0) return 0;
    
    size_t total_sent = 0;
    size_t chunk_size = 8192; // 8KB 块大小
    
    while (total_sent < size) {
        size_t to_send = (size - total_sent > chunk_size) ? chunk_size : (size - total_sent);
        
        // 获取令牌
        if (controller) {
            int delay = bandwidth_controller_acquire(controller, to_send);
            if (delay > 0) {
                usleep(delay);
                // 重新获取令牌
                bandwidth_controller_acquire(controller, to_send);
            }
        }
        
        // 发送数据
        ssize_t sent = send(socket_fd, data + total_sent, to_send, 0);
        if (sent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000); // 1ms
                continue;
            }
            return -1;
        }
        
        total_sent += sent;
        
        // 更新统计
        bandwidth_update_stats(sent, controller != NULL);
    }
    
    return total_sent;
}

// 带宽控制的sendfile函数
int bandwidth_controlled_sendfile(int socket_fd, int file_fd, off_t *offset, 
                                 size_t count, bandwidth_controller_t *controller) {
    if (count == 0) return 0;
    
    size_t total_sent = 0;
    size_t chunk_size = 65536; // 64KB 块大小
    off_t current_offset = offset ? *offset : 0;
    
    while (total_sent < count) {
        size_t to_send = (count - total_sent > chunk_size) ? chunk_size : (count - total_sent);
        
        // 获取令牌
        if (controller) {
            int delay = bandwidth_controller_acquire(controller, to_send);
            if (delay > 0) {
                usleep(delay);
                // 重新获取令牌
                bandwidth_controller_acquire(controller, to_send);
            }
        }
        
        // 发送文件数据
        off_t current_pos = current_offset + total_sent;
        ssize_t sent = sendfile(socket_fd, file_fd, &current_pos, to_send);
        if (sent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000); // 1ms
                continue;
            }
            return -1;
        }
        
        total_sent += sent;
        
        // 更新统计
        bandwidth_update_stats(sent, controller != NULL);
    }
    
    if (offset) *offset += total_sent;
    return total_sent;
}

// 带宽控制的SSL发送函数
int bandwidth_controlled_ssl_send(void *ssl_ptr, const char *data, size_t size,
                                 bandwidth_controller_t *controller) {
    if (!ssl_ptr || !data || size == 0) return 0;
    
    SSL *ssl = (SSL *)ssl_ptr;
    size_t total_sent = 0;
    size_t chunk_size = 8192; // 8KB 块大小
    
    while (total_sent < size) {
        size_t to_send = (size - total_sent > chunk_size) ? chunk_size : (size - total_sent);
        
        // 获取令牌
        if (controller) {
            int delay = bandwidth_controller_acquire(controller, to_send);
            if (delay > 0) {
                usleep(delay);
                // 重新获取令牌
                bandwidth_controller_acquire(controller, to_send);
            }
        }
        
        // 发送SSL数据
        int sent = SSL_write(ssl, data + total_sent, to_send);
        if (sent <= 0) {
            int ssl_error = SSL_get_error(ssl, sent);
            if (ssl_error == SSL_ERROR_WANT_WRITE || ssl_error == SSL_ERROR_WANT_READ) {
                usleep(1000); // 1ms
                continue;
            }
            return -1;
        }
        
        total_sent += sent;
        
        // 更新统计
        bandwidth_update_stats(sent, controller != NULL);
    }
    
    return total_sent;
}

// 单位转换函数
size_t bandwidth_convert_to_bytes_per_second(size_t value, bandwidth_unit_t unit) {
    switch (unit) {
        case BANDWIDTH_UNIT_BPS:
            return value;
        case BANDWIDTH_UNIT_KBPS:
            return value * 1024;
        case BANDWIDTH_UNIT_MBPS:
            return value * 1024 * 1024;
        default:
            return value;
    }
}

// 单位转换为字符串
const char *bandwidth_unit_to_string(bandwidth_unit_t unit) {
    switch (unit) {
        case BANDWIDTH_UNIT_BPS:  return "B/s";
        case BANDWIDTH_UNIT_KBPS: return "KB/s";
        case BANDWIDTH_UNIT_MBPS: return "MB/s";
        default: return "B/s";
    }
}

// 模式匹配函数
bool bandwidth_match_pattern(const char *pattern, const char *string) {
    if (!pattern || !string) return false;
    return fnmatch(pattern, string, FNM_PATHNAME) == 0;
}

// IP模式匹配函数
bool bandwidth_match_ip_pattern(const char *pattern, const char *ip) {
    if (!pattern || !ip) return false;
    
    // 简单的通配符匹配
    if (strcmp(pattern, "*") == 0) return true;
    
    // 精确匹配
    if (strcmp(pattern, ip) == 0) return true;
    
    // 子网匹配（简化版，只支持 xxx.xxx.xxx.* 格式）
    if (strstr(pattern, "*")) {
        size_t pattern_len = strlen(pattern);
        if (pattern[pattern_len - 1] == '*') {
            return strncmp(pattern, ip, pattern_len - 1) == 0;
        }
    }
    
    return false;
}

// 获取带宽统计信息
bandwidth_stats_t *bandwidth_get_stats(void) {
    global_bandwidth_stats.last_updated = time(NULL);
    
    // 计算平均传输速率
    if (global_bandwidth_stats.total_connections > 0) {
        global_bandwidth_stats.avg_transfer_rate = 
            (double)global_bandwidth_stats.total_bytes_sent / global_bandwidth_stats.total_connections;
    }
    
    return &global_bandwidth_stats;
}

// 更新带宽统计信息
void bandwidth_update_stats(size_t bytes_sent, bool limited) {
    global_bandwidth_stats.total_bytes_sent += bytes_sent;
    
    if (limited) {
        global_bandwidth_stats.total_bytes_limited += bytes_sent;
        global_bandwidth_stats.limited_connections++;
    }
    
    global_bandwidth_stats.total_connections++;
}

// 重置带宽统计信息
void bandwidth_reset_stats(void) {
    memset(&global_bandwidth_stats, 0, sizeof(global_bandwidth_stats));
}

// 解析配置指令
int bandwidth_parse_config_directive(bandwidth_config_t *config, const char *key, const char *value) {
    if (!config || !key || !value) return -1;
    
    if (strcmp(key, "enable_bandwidth_limit") == 0) {
        config->enable_bandwidth_limit = (strcmp(value, "on") == 0);
        return 0;
    }
    
    if (strcmp(key, "default_rate_limit") == 0) {
        char *endptr;
        long rate = strtol(value, &endptr, 10);
        if (rate > 0) {
            bandwidth_unit_t unit = bandwidth_parse_unit(endptr);
            config->default_rate_limit = bandwidth_convert_to_bytes_per_second(rate, unit);
            return 0;
        }
    }
    
    if (strcmp(key, "default_burst_size") == 0) {
        long burst = strtol(value, NULL, 10);
        if (burst > 0) {
            config->default_burst_size = burst;
            return 0;
        }
    }
    
    if (strcmp(key, "min_file_size") == 0) {
        long size = strtol(value, NULL, 10);
        if (size > 0) {
            config->min_file_size = size;
            return 0;
        }
    }
    
    return -1; // 未知指令
}

// 解析单位字符串
bandwidth_unit_t bandwidth_parse_unit(const char *unit_str) {
    if (!unit_str || strlen(unit_str) == 0) return BANDWIDTH_UNIT_BPS;
    
    // 跳过空白字符
    while (*unit_str == ' ' || *unit_str == '\t') unit_str++;
    
    if (strcasecmp(unit_str, "k") == 0 || strcasecmp(unit_str, "kb") == 0 || 
        strcasecmp(unit_str, "kbps") == 0) {
        return BANDWIDTH_UNIT_KBPS;
    }
    
    if (strcasecmp(unit_str, "m") == 0 || strcasecmp(unit_str, "mb") == 0 || 
        strcasecmp(unit_str, "mbps") == 0) {
        return BANDWIDTH_UNIT_MBPS;
    }
    
    return BANDWIDTH_UNIT_BPS;
} 