#include "http_module.h"
#include "http.h"
#include "proxy.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>

// 代理请求处理函数 - 转发到proxy模块
int proxy_request(int client_socket, const char *req_path, const char *proxy_pass, 
                 const char *client_ip, core_config_t *core_conf) {
    if (!proxy_pass || !req_path) {
        return -1;
    }
    
    // 记录代理请求
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Proxying request %s to %s", req_path, proxy_pass);
    log_message(LOG_LEVEL_INFO, log_msg);
    
    // 调用代理处理函数
    return handle_proxy_request(client_socket, req_path, proxy_pass, client_ip, core_conf);
}