#ifndef LB_PROXY_H
#define LB_PROXY_H

#include "core.h"
#include "load_balancer.h"

// 处理负载均衡的HTTP代理请求
int handle_lb_proxy_request(int client_socket, const char *method, const char *path, 
                           const char *http_version, const char *headers, 
                           const char *upstream_name, const char *client_ip,
                           core_config_t *core_config);

// 处理负载均衡的HTTPS代理请求
int handle_lb_https_proxy_request(SSL *ssl, const char *method, const char *path, 
                                 const char *http_version, const char *headers,
                                 const char *upstream_name, const char *client_ip,
                                 core_config_t *core_config);

// 检查是否为upstream代理（proxy_pass指向upstream组）
int is_upstream_proxy(const char *proxy_pass_value);

// 从proxy_pass值中提取upstream名称
char *extract_upstream_name(const char *proxy_pass_value);

// 构建负载均衡的代理请求
char *build_lb_proxy_request(const char *method, const char *original_path, 
                            const char *http_version, const char *headers,
                            upstream_server_t *server, const char *client_ip);

// 处理负载均衡代理响应
int forward_lb_response(int backend_fd, int client_fd, upstream_server_t *server);

// 处理负载均衡HTTPS代理响应
int forward_lb_https_response(int backend_fd, SSL *ssl, upstream_server_t *server);

// 更新服务器统计信息
void update_server_stats(upstream_server_t *server, int success, double response_time);

// 记录负载均衡日志
void log_lb_request(const char *method, const char *path, const char *client_ip,
                   const char *upstream_name, upstream_server_t *server, 
                   int status_code, double response_time);

#endif // LB_PROXY_H 