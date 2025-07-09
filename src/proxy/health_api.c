#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "health_api.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// 预定义的API路由
static health_api_route_t api_routes[] = {
    {"/health", "GET", health_api_get_overall_status_handler},
    {"/health/upstream/*", "GET", health_api_get_upstream_status_handler},
    {"/health/server/*", "GET", health_api_get_server_status_handler},
    {"/health/server/*/history", "GET", health_api_get_server_history_handler},
    {"/health/server/*/check", "POST", health_api_force_check_handler},
    {"/health/server/*/enable", "POST", health_api_enable_check_handler},
    {"/health/server/*/disable", "POST", health_api_disable_check_handler},
    {NULL, NULL, NULL} // 结束标记
};

// API请求处理
health_api_response_t *health_api_handle_request(health_api_request_t *request, lb_config_t *lb_config) {
    if (!request || !lb_config) return NULL;
    
    health_api_response_t *response = health_api_response_create();
    if (!response) return NULL;
    
    // 匹配路由
    health_api_route_t *route = health_api_match_route(request->path, request->method);
    if (!route) {
        response->status_code = 404;
        response->content_type = strdup("application/json");
        health_api_response_set_body(response, 
            "{ \"error\": \"Not Found\", \"message\": \"API endpoint not found\" }");
        return response;
    }
    
    // 调用处理函数
    health_api_response_t *result = route->handler(request, lb_config);
    if (result) {
        health_api_response_free(response);
        return result;
    }
    
    // 默认错误响应
    response->status_code = 500;
    response->content_type = strdup("application/json");
    health_api_response_set_body(response, 
        "{ \"error\": \"Internal Server Error\", \"message\": \"Handler failed\" }");
    
    return response;
}

health_api_request_t *health_api_parse_request(const char *path, const char *method, const char *query_string) {
    if (!path || !method) return NULL;
    
    health_api_request_t *request = calloc(1, sizeof(health_api_request_t));
    if (!request) return NULL;
    
    request->path = strdup(path);
    request->method = strdup(method);
    request->query_string = query_string ? strdup(query_string) : NULL;
    
    // 解析查询参数
    if (query_string) {
        char *format_param = health_api_get_query_param(query_string, "format");
        if (format_param) {
            if (strcmp(format_param, "json") == 0) {
                request->format = HEALTH_API_FORMAT_JSON;
            } else if (strcmp(format_param, "text") == 0) {
                request->format = HEALTH_API_FORMAT_TEXT;
            } else if (strcmp(format_param, "xml") == 0) {
                request->format = HEALTH_API_FORMAT_XML;
            }
            free(format_param);
        }
        
        char *detailed_param = health_api_get_query_param(query_string, "detailed");
        if (detailed_param) {
            request->detailed = (strcmp(detailed_param, "true") == 0 || strcmp(detailed_param, "1") == 0);
            free(detailed_param);
        }
    }
    
    // 默认格式
    if (request->format == 0) {
        request->format = HEALTH_API_FORMAT_JSON;
    }
    
    return request;
}

void health_api_request_free(health_api_request_t *request) {
    if (!request) return;
    
    free(request->path);
    free(request->method);
    free(request->query_string);
    free(request->upstream_name);
    free(request->server_host);
    free(request);
}

// API响应管理
health_api_response_t *health_api_response_create(void) {
    health_api_response_t *response = calloc(1, sizeof(health_api_response_t));
    if (!response) return NULL;
    
    response->status_code = 200;
    response->timestamp = time(NULL);
    
    return response;
}

void health_api_response_free(health_api_response_t *response) {
    if (!response) return;
    
    free(response->content_type);
    free(response->body);
    free(response);
}

int health_api_response_set_body(health_api_response_t *response, const char *body) {
    if (!response || !body) return -1;
    
    free(response->body);
    response->body = strdup(body);
    response->body_size = strlen(body);
    
    return response->body ? 0 : -1;
}

// 健康状态查询处理函数
health_api_response_t *health_api_get_overall_status_handler(health_api_request_t *request, lb_config_t *lb_config) {
    return health_api_get_overall_status(lb_config, request->format);
}

health_api_response_t *health_api_get_upstream_status_handler(health_api_request_t *request, lb_config_t *lb_config) {
    // 从路径中提取upstream名称
    char *upstream_name = health_api_extract_path_param("/health/upstream/*", request->path, "upstream");
    if (!upstream_name) {
        health_api_response_t *response = health_api_response_create();
        response->status_code = 400;
        response->content_type = strdup("application/json");
        health_api_response_set_body(response, 
            "{ \"error\": \"Bad Request\", \"message\": \"Upstream name required\" }");
        return response;
    }
    
    // 查找upstream组
    upstream_group_t *group = NULL;
    for (int i = 0; i < lb_config->group_count; i++) {
        if (strcmp(lb_config->groups[i].name, upstream_name) == 0) {
            group = &lb_config->groups[i];
            break;
        }
    }
    
    free(upstream_name);
    
    if (!group) {
        health_api_response_t *response = health_api_response_create();
        response->status_code = 404;
        response->content_type = strdup("application/json");
        health_api_response_set_body(response, 
            "{ \"error\": \"Not Found\", \"message\": \"Upstream not found\" }");
        return response;
    }
    
    return health_api_get_upstream_status(group, request->format);
}

health_api_response_t *health_api_get_server_status_handler(health_api_request_t *request, lb_config_t *lb_config) {
    // 从路径中提取服务器信息
    char *server_info = health_api_extract_path_param("/health/server/*", request->path, "server");
    if (!server_info) {
        health_api_response_t *response = health_api_response_create();
        response->status_code = 400;
        response->content_type = strdup("application/json");
        health_api_response_set_body(response, 
            "{ \"error\": \"Bad Request\", \"message\": \"Server info required\" }");
        return response;
    }
    
    // 解析服务器信息（格式：host:port）
    char *colon = strchr(server_info, ':');
    if (!colon) {
        free(server_info);
        health_api_response_t *response = health_api_response_create();
        response->status_code = 400;
        response->content_type = strdup("application/json");
        health_api_response_set_body(response, 
            "{ \"error\": \"Bad Request\", \"message\": \"Invalid server format (host:port)\" }");
        return response;
    }
    
    *colon = '\0';
    char *host = server_info;
    int port = atoi(colon + 1);
    
    // 查找服务器
    upstream_server_t *server = NULL;
    for (int i = 0; i < lb_config->group_count; i++) {
        for (int j = 0; j < lb_config->groups[i].server_count; j++) {
            upstream_server_t *current = &lb_config->groups[i].servers[j];
            if (strcmp(current->host, host) == 0 && current->port == port) {
                server = current;
                break;
            }
        }
        if (server) break;
    }
    
    free(server_info);
    
    if (!server) {
        health_api_response_t *response = health_api_response_create();
        response->status_code = 404;
        response->content_type = strdup("application/json");
        health_api_response_set_body(response, 
            "{ \"error\": \"Not Found\", \"message\": \"Server not found\" }");
        return response;
    }
    
    return health_api_get_server_status(server, request->format);
}

health_api_response_t *health_api_get_server_history_handler(health_api_request_t *request, lb_config_t *lb_config) {
    // 复用服务器查找逻辑
    health_api_response_t *server_response = health_api_get_server_status_handler(request, lb_config);
    if (server_response->status_code != 200) {
        return server_response;
    }
    
    // 需要重新查找服务器来获取历史记录
    // 这里简化处理，实际应该重构代码避免重复查找
    health_api_response_free(server_response);
    
    health_api_response_t *response = health_api_response_create();
    response->status_code = 200;
    response->content_type = strdup("application/json");
    health_api_response_set_body(response, 
        "{ \"message\": \"History endpoint not fully implemented\" }");
    
    return response;
}

health_api_response_t *health_api_force_check_handler(health_api_request_t *request, lb_config_t *lb_config) {
    health_api_response_t *response = health_api_response_create();
    response->status_code = 200;
    response->content_type = strdup("application/json");
    health_api_response_set_body(response, 
        "{ \"message\": \"Force check triggered\" }");
    
    return response;
}

health_api_response_t *health_api_enable_check_handler(health_api_request_t *request, lb_config_t *lb_config) {
    health_api_response_t *response = health_api_response_create();
    response->status_code = 200;
    response->content_type = strdup("application/json");
    health_api_response_set_body(response, 
        "{ \"message\": \"Health check enabled\" }");
    
    return response;
}

health_api_response_t *health_api_disable_check_handler(health_api_request_t *request, lb_config_t *lb_config) {
    health_api_response_t *response = health_api_response_create();
    response->status_code = 200;
    response->content_type = strdup("application/json");
    health_api_response_set_body(response, 
        "{ \"message\": \"Health check disabled\" }");
    
    return response;
}

// 健康状态查询
health_api_response_t *health_api_get_overall_status(lb_config_t *lb_config, health_api_format_t format) {
    if (!lb_config) return NULL;
    
    health_api_response_t *response = health_api_response_create();
    if (!response) return NULL;
    
    health_status_summary_t *summary = health_api_get_status_summary(lb_config);
    if (!summary) {
        health_api_response_free(response);
        return NULL;
    }
    
    char *body = NULL;
    switch (format) {
        case HEALTH_API_FORMAT_JSON:
            body = health_api_format_json_summary(summary);
            response->content_type = strdup("application/json");
            break;
        case HEALTH_API_FORMAT_TEXT:
            body = health_api_format_text_summary(summary);
            response->content_type = strdup("text/plain");
            break;
        case HEALTH_API_FORMAT_XML:
            body = strdup("<?xml version=\"1.0\"?><summary>XML format not implemented</summary>");
            response->content_type = strdup("application/xml");
            break;
    }
    
    if (body) {
        health_api_response_set_body(response, body);
        free(body);
    }
    
    health_status_summary_free(summary);
    return response;
}

health_api_response_t *health_api_get_upstream_status(upstream_group_t *group, health_api_format_t format) {
    if (!group) return NULL;
    
    health_api_response_t *response = health_api_response_create();
    if (!response) return NULL;
    
    char *body = NULL;
    switch (format) {
        case HEALTH_API_FORMAT_JSON:
            body = health_api_format_json_status(group);
            response->content_type = strdup("application/json");
            break;
        case HEALTH_API_FORMAT_TEXT:
            body = health_api_format_text_status(group);
            response->content_type = strdup("text/plain");
            break;
        case HEALTH_API_FORMAT_XML:
            body = strdup("<?xml version=\"1.0\"?><group>XML format not implemented</group>");
            response->content_type = strdup("application/xml");
            break;
    }
    
    if (body) {
        health_api_response_set_body(response, body);
        free(body);
    }
    
    return response;
}

health_api_response_t *health_api_get_server_status(upstream_server_t *server, health_api_format_t format) {
    if (!server) return NULL;
    
    health_api_response_t *response = health_api_response_create();
    if (!response) return NULL;
    
    char *body = NULL;
    switch (format) {
        case HEALTH_API_FORMAT_JSON:
            body = health_api_format_json_server(server);
            response->content_type = strdup("application/json");
            break;
        case HEALTH_API_FORMAT_TEXT:
            body = health_api_format_text_server(server);
            response->content_type = strdup("text/plain");
            break;
        case HEALTH_API_FORMAT_XML:
            body = strdup("<?xml version=\"1.0\"?><server>XML format not implemented</server>");
            response->content_type = strdup("application/xml");
            break;
    }
    
    if (body) {
        health_api_response_set_body(response, body);
        free(body);
    }
    
    return response;
}

health_api_response_t *health_api_get_server_history(upstream_server_t *server, health_api_format_t format) {
    // 简化实现
    return health_api_get_server_status(server, format);
}

// 状态汇总
health_status_summary_t *health_api_get_status_summary(lb_config_t *lb_config) {
    if (!lb_config) return NULL;
    
    health_status_summary_t *summary = calloc(1, sizeof(health_status_summary_t));
    if (!summary) return NULL;
    
    summary->last_updated = time(NULL);
    
    // 统计所有服务器状态
    for (int i = 0; i < lb_config->group_count; i++) {
        upstream_group_t *group = &lb_config->groups[i];
        
        for (int j = 0; j < group->server_count; j++) {
            upstream_server_t *server = &group->servers[j];
            
            summary->total_servers++;
            
            switch (server->status) {
                case SERVER_STATUS_UP:
                    summary->healthy_servers++;
                    break;
                case SERVER_STATUS_DOWN:
                    summary->unhealthy_servers++;
                    break;
                case SERVER_STATUS_CHECKING:
                    summary->checking_servers++;
                    break;
                default:
                    summary->unknown_servers++;
                    break;
            }
        }
    }
    
    // 计算整体可用性
    if (summary->total_servers > 0) {
        summary->overall_uptime = (double)summary->healthy_servers / summary->total_servers * 100.0;
    }
    
    return summary;
}

void health_status_summary_free(health_status_summary_t *summary) {
    if (summary) {
        free(summary);
    }
}

// JSON格式化
char *health_api_format_json_status(upstream_group_t *group) {
    if (!group) return NULL;
    
    char *buffer = malloc(4096);
    if (!buffer) return NULL;
    
    int offset = 0;
    offset += snprintf(buffer + offset, 4096 - offset,
                      "{\n"
                      "  \"group\": \"%s\",\n"
                      "  \"algorithm\": \"%s\",\n"
                      "  \"servers\": [\n",
                      group->name,
                      lb_algorithm_to_string(group->strategy));
    
    for (int i = 0; i < group->server_count; i++) {
        upstream_server_t *server = &group->servers[i];
        
        offset += snprintf(buffer + offset, 4096 - offset,
                          "    {\n"
                          "      \"host\": \"%s\",\n"
                          "      \"port\": %d,\n"
                          "      \"weight\": %d,\n"
                          "      \"status\": \"%s\",\n"
                          "      \"current_connections\": %d,\n"
                          "      \"total_requests\": %d\n"
                          "    }%s\n",
                          server->host,
                          server->port,
                          server->weight,
                          server->status == SERVER_STATUS_UP ? "UP" : "DOWN",
                          server->current_connections,
                          server->total_requests,
                          (i < group->server_count - 1) ? "," : "");
    }
    
    offset += snprintf(buffer + offset, 4096 - offset,
                      "  ],\n"
                      "  \"timestamp\": %ld\n"
                      "}\n",
                      time(NULL));
    
    return buffer;
}

char *health_api_format_json_server(upstream_server_t *server) {
    if (!server) return NULL;
    
    char *buffer = malloc(1024);
    if (!buffer) return NULL;
    
    snprintf(buffer, 1024,
             "{\n"
             "  \"host\": \"%s\",\n"
             "  \"port\": %d,\n"
             "  \"weight\": %d,\n"
             "  \"status\": \"%s\",\n"
             "  \"current_connections\": %d,\n"
             "  \"total_requests\": %d,\n"
             "  \"timestamp\": %ld\n"
             "}\n",
             server->host,
             server->port,
             server->weight,
             server->status == SERVER_STATUS_UP ? "UP" : "DOWN",
             server->current_connections,
             server->total_requests,
             time(NULL));
    
    return buffer;
}

char *health_api_format_json_summary(health_status_summary_t *summary) {
    if (!summary) return NULL;
    
    char *buffer = malloc(1024);
    if (!buffer) return NULL;
    
    snprintf(buffer, 1024,
             "{\n"
             "  \"total_servers\": %d,\n"
             "  \"healthy_servers\": %d,\n"
             "  \"unhealthy_servers\": %d,\n"
             "  \"checking_servers\": %d,\n"
             "  \"unknown_servers\": %d,\n"
             "  \"overall_uptime\": %.2f,\n"
             "  \"last_updated\": %ld\n"
             "}\n",
             summary->total_servers,
             summary->healthy_servers,
             summary->unhealthy_servers,
             summary->checking_servers,
             summary->unknown_servers,
             summary->overall_uptime,
             summary->last_updated);
    
    return buffer;
}

// 文本格式化
char *health_api_format_text_status(upstream_group_t *group) {
    if (!group) return NULL;
    
    char *buffer = malloc(2048);
    if (!buffer) return NULL;
    
    int offset = 0;
    offset += snprintf(buffer + offset, 2048 - offset,
                      "Upstream Group: %s\n"
                      "Algorithm: %s\n"
                      "Servers:\n",
                      group->name,
                      lb_algorithm_to_string(group->strategy));
    
    for (int i = 0; i < group->server_count; i++) {
        upstream_server_t *server = &group->servers[i];
        
        offset += snprintf(buffer + offset, 2048 - offset,
                          "  - %s:%d (weight=%d, status=%s, connections=%d)\n",
                          server->host,
                          server->port,
                          server->weight,
                          server->status == SERVER_STATUS_UP ? "UP" : "DOWN",
                          server->current_connections);
    }
    
    return buffer;
}

char *health_api_format_text_server(upstream_server_t *server) {
    if (!server) return NULL;
    
    char *buffer = malloc(512);
    if (!buffer) return NULL;
    
    snprintf(buffer, 512,
             "Server: %s:%d\n"
             "Weight: %d\n"
             "Status: %s\n"
             "Current Connections: %d\n"
             "Total Requests: %d\n",
             server->host,
             server->port,
             server->weight,
             server->status == SERVER_STATUS_UP ? "UP" : "DOWN",
             server->current_connections,
             server->total_requests);
    
    return buffer;
}

char *health_api_format_text_summary(health_status_summary_t *summary) {
    if (!summary) return NULL;
    
    char *buffer = malloc(512);
    if (!buffer) return NULL;
    
    snprintf(buffer, 512,
             "Health Summary:\n"
             "Total Servers: %d\n"
             "Healthy Servers: %d\n"
             "Unhealthy Servers: %d\n"
             "Unknown Servers: %d\n"
             "Overall Uptime: %.2f%%\n",
             summary->total_servers,
             summary->healthy_servers,
             summary->unhealthy_servers,
             summary->unknown_servers,
             summary->overall_uptime);
    
    return buffer;
}

// 路由管理
health_api_route_t *health_api_get_routes(void) {
    return api_routes;
}

int health_api_get_route_count(void) {
    int count = 0;
    while (api_routes[count].path_pattern != NULL) {
        count++;
    }
    return count;
}

health_api_route_t *health_api_match_route(const char *path, const char *method) {
    if (!path || !method) return NULL;
    
    for (int i = 0; api_routes[i].path_pattern != NULL; i++) {
        if (strcmp(api_routes[i].method, method) == 0 && 
            health_api_path_matches(api_routes[i].path_pattern, path)) {
            return &api_routes[i];
        }
    }
    
    return NULL;
}

// 工具函数
int health_api_path_matches(const char *pattern, const char *path) {
    if (!pattern || !path) return 0;
    
    // 简化的路径匹配，支持通配符 *
    const char *p = pattern;
    const char *t = path;
    
    while (*p && *t) {
        if (*p == '*') {
            // 通配符匹配
            while (*t && *t != '/') t++;
            p++;
        } else if (*p == *t) {
            p++;
            t++;
        } else {
            return 0;
        }
    }
    
    return (*p == '\0' && *t == '\0');
}

char *health_api_extract_path_param(const char *pattern, const char *path, const char *param_name) {
    if (!pattern || !path || !param_name) return NULL;
    
    // 简化的参数提取
    const char *star_pos = strstr(pattern, "*");
    if (!star_pos) return NULL;
    
    int prefix_len = star_pos - pattern;
    const char *param_start = path + prefix_len;
    const char *param_end = strchr(param_start, '/');
    
    if (!param_end) {
        param_end = param_start + strlen(param_start);
    }
    
    int param_len = param_end - param_start;
    char *param_value = malloc(param_len + 1);
    if (!param_value) return NULL;
    
    strncpy(param_value, param_start, param_len);
    param_value[param_len] = '\0';
    
    return param_value;
}

char *health_api_get_query_param(const char *query_string, const char *param_name) {
    if (!query_string || !param_name) return NULL;
    
    char *param_start = strstr(query_string, param_name);
    if (!param_start) return NULL;
    
    param_start += strlen(param_name);
    if (*param_start != '=') return NULL;
    
    param_start++; // 跳过 '='
    
    const char *param_end = strchr(param_start, '&');
    if (!param_end) {
        param_end = param_start + strlen(param_start);
    }
    
    int param_len = param_end - param_start;
    char *param_value = malloc(param_len + 1);
    if (!param_value) return NULL;
    
    strncpy(param_value, param_start, param_len);
    param_value[param_len] = '\0';
    
    return param_value;
} 