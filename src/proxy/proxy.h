#ifndef PROXY_H
#define PROXY_H

#include "core.h"

// 处理HTTP反向代理请求
int handle_proxy_request(int client_socket, const char *method, const char *path, 
                        const char *http_version, const char *headers, 
                        const char *proxy_pass_url, const char *client_ip);

// 处理HTTPS反向代理请求
int handle_https_proxy_request(SSL *ssl, const char *method, const char *path, 
                              const char *http_version, const char *headers,
                              const char *proxy_pass_url, const char *client_ip);

// 解析proxy_pass URL
typedef struct {
    char *protocol;  // http or https
    char *host;      // 主机名或IP
    int port;        // 端口号
    char *path;      // 路径前缀
} proxy_url_t;

// 解析proxy_pass URL字符串
proxy_url_t *parse_proxy_url(const char *url);

// 释放proxy_url_t结构
void free_proxy_url(proxy_url_t *url);

#endif // PROXY_H 