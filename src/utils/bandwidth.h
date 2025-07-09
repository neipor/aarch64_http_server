#ifndef BANDWIDTH_H
#define BANDWIDTH_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

// 带宽限制单位
typedef enum {
    BANDWIDTH_UNIT_BPS,    // 字节每秒
    BANDWIDTH_UNIT_KBPS,   // KB每秒
    BANDWIDTH_UNIT_MBPS    // MB每秒
} bandwidth_unit_t;

// 带宽限制规则
typedef struct bandwidth_rule {
    char *path_pattern;           // 路径模式（支持通配符）
    char *mime_type;              // MIME类型（可选）
    char *client_ip_pattern;      // 客户端IP模式（可选）
    size_t rate_limit;            // 速率限制值
    bandwidth_unit_t unit;        // 单位
    size_t burst_size;            // 突发大小（字节）
    bool enabled;                 // 是否启用
    struct bandwidth_rule *next;  // 链表指针
} bandwidth_rule_t;

// 带宽限制配置
typedef struct {
    bool enable_bandwidth_limit;  // 是否启用带宽限制
    size_t default_rate_limit;    // 默认速率限制（字节每秒）
    size_t default_burst_size;    // 默认突发大小
    size_t min_file_size;         // 启用限制的最小文件大小
    bandwidth_rule_t *rules;      // 带宽限制规则链表
    int rules_count;              // 规则数量
} bandwidth_config_t;

// 带宽控制器（针对单个连接）
typedef struct {
    size_t rate_limit;            // 速率限制（字节每秒）
    size_t burst_size;            // 突发大小
    size_t tokens;                // 当前令牌数
    struct timeval last_update;   // 上次更新时间
    size_t bytes_sent;            // 已发送字节数
    struct timeval start_time;    // 开始时间
    bool active;                  // 是否激活
} bandwidth_controller_t;

// 带宽统计信息
typedef struct {
    size_t total_connections;     // 总连接数
    size_t limited_connections;   // 受限连接数
    size_t total_bytes_sent;      // 总发送字节数
    size_t total_bytes_limited;   // 被限制的字节数
    double avg_transfer_rate;     // 平均传输速率
    time_t last_updated;          // 最后更新时间
} bandwidth_stats_t;

// 配置管理函数
bandwidth_config_t *bandwidth_config_create(void);
void bandwidth_config_free(bandwidth_config_t *config);
int bandwidth_config_add_rule(bandwidth_config_t *config, const char *path_pattern,
                              const char *mime_type, const char *client_ip_pattern,
                              size_t rate_limit, bandwidth_unit_t unit, size_t burst_size);
bandwidth_rule_t *bandwidth_config_find_rule(bandwidth_config_t *config, 
                                            const char *path, const char *mime_type,
                                            const char *client_ip);

// 控制器管理函数
bandwidth_controller_t *bandwidth_controller_create(size_t rate_limit, size_t burst_size);
void bandwidth_controller_free(bandwidth_controller_t *controller);
int bandwidth_controller_init(bandwidth_controller_t *controller, size_t rate_limit, size_t burst_size);

// 速率控制函数
int bandwidth_controller_acquire(bandwidth_controller_t *controller, size_t bytes);
size_t bandwidth_controller_calculate_delay(bandwidth_controller_t *controller, size_t bytes);
void bandwidth_controller_update_tokens(bandwidth_controller_t *controller);
bool bandwidth_controller_can_send(bandwidth_controller_t *controller, size_t bytes);

// 传输控制函数
int bandwidth_controlled_send(int socket_fd, const char *data, size_t size, 
                             bandwidth_controller_t *controller);
int bandwidth_controlled_sendfile(int socket_fd, int file_fd, off_t *offset, 
                                 size_t count, bandwidth_controller_t *controller);
int bandwidth_controlled_ssl_send(void *ssl, const char *data, size_t size,
                                 bandwidth_controller_t *controller);

// 工具函数
size_t bandwidth_convert_to_bytes_per_second(size_t value, bandwidth_unit_t unit);
const char *bandwidth_unit_to_string(bandwidth_unit_t unit);
bool bandwidth_match_pattern(const char *pattern, const char *string);
bool bandwidth_match_ip_pattern(const char *pattern, const char *ip);

// 统计函数
bandwidth_stats_t *bandwidth_get_stats(void);
void bandwidth_update_stats(size_t bytes_sent, bool limited);
void bandwidth_reset_stats(void);

// 配置解析函数
int bandwidth_parse_config_directive(bandwidth_config_t *config, const char *key, const char *value);
bandwidth_unit_t bandwidth_parse_unit(const char *unit_str);

#endif // BANDWIDTH_H 