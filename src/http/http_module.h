#ifndef HTTP_MODULE_H
#define HTTP_MODULE_H

#include "core.h"

// HTTP模块初始化函数 - 在http.c中实现
void http_module_init(void);

// HTTP模块清理函数 - 在http.c中实现
void http_module_cleanup(void);

// 代理请求处理函数 - 在http_module.c中实现
int proxy_request(int client_socket, const char *req_path, const char *proxy_pass, 
                 const char *client_ip, core_config_t *core_conf);

// 处理代理请求的函数 - 在proxy.c中实现
// 注意: 这个函数在proxy.h中声明

#endif // HTTP_MODULE_H