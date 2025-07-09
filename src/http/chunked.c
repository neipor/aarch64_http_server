#include "chunked.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include "log.h"

#define DEFAULT_CHUNK_SIZE 8192  // 8KB
#define CHUNK_HEADER_SIZE 16     // 最大分块头大小
#define BUFFER_SIZE 4096

// 创建分块传输编码上下文
chunked_context_t *chunked_context_create(int socket_fd, SSL *ssl) {
    chunked_context_t *ctx = malloc(sizeof(chunked_context_t));
    if (!ctx) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for chunked context");
        return NULL;
    }
    
    ctx->socket_fd = socket_fd;
    ctx->ssl = ssl;
    ctx->is_ssl = (ssl != NULL);
    ctx->finished = false;
    ctx->total_sent = 0;
    
    return ctx;
}

// 释放分块传输编码上下文
void chunked_context_free(chunked_context_t *ctx) {
    if (ctx) {
        free(ctx);
    }
}

// 发送数据到客户端
static int send_data(chunked_context_t *ctx, const char *data, size_t size) {
    if (!ctx || !data || size == 0) {
        return -1;
    }
    
    int bytes_sent;
    if (ctx->is_ssl) {
        bytes_sent = SSL_write(ctx->ssl, data, size);
    } else {
        bytes_sent = send(ctx->socket_fd, data, size, 0);
    }
    
    if (bytes_sent <= 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to send data to client");
        return -1;
    }
    
    ctx->total_sent += bytes_sent;
    return bytes_sent;
}

// 发送分块传输编码的响应头
int chunked_send_headers(chunked_context_t *ctx, int status_code, 
                        const char *content_type, const char *extra_headers) {
    if (!ctx || ctx->finished) {
        return -1;
    }
    
    char header[BUFFER_SIZE * 2];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Server: ANX HTTP Server/0.8.0\r\n",
        status_code, 
        (status_code == 200) ? "OK" : "Error",
        content_type ? content_type : "text/plain");
    
    // 添加额外头部
    if (extra_headers && strlen(extra_headers) > 0) {
        header_len += snprintf(header + header_len, sizeof(header) - header_len,
                              "%s", extra_headers);
    }
    
    // 添加连接头部和结束
    header_len += snprintf(header + header_len, sizeof(header) - header_len,
                          "Connection: close\r\n\r\n");
    
    if (send_data(ctx, header, strlen(header)) < 0) {
        return -1;
    }
    
    log_message(LOG_LEVEL_DEBUG, "Sent chunked transfer encoding headers");
    return 0;
}

// 发送一个数据块
int chunked_send_chunk(chunked_context_t *ctx, const char *data, size_t size) {
    if (!ctx || ctx->finished) {
        return -1;
    }
    
    if (size == 0) {
        return 0; // 不发送空块（除非是最后一块）
    }
    
    // 构建分块头部（十六进制大小 + \r\n）
    char chunk_header[CHUNK_HEADER_SIZE];
    int header_len = snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", size);
    
    // 发送分块头部
    if (send_data(ctx, chunk_header, header_len) < 0) {
        return -1;
    }
    
    // 发送数据
    if (send_data(ctx, data, size) < 0) {
        return -1;
    }
    
    // 发送分块结束符
    if (send_data(ctx, "\r\n", 2) < 0) {
        return -1;
    }
    
    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "Sent chunk: %zu bytes", size);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return 0;
}

// 发送最后的空块，结束分块传输
int chunked_send_final_chunk(chunked_context_t *ctx, const char *trailer_headers) {
    if (!ctx || ctx->finished) {
        return -1;
    }
    
    // 发送最后的空块
    if (send_data(ctx, "0\r\n", 3) < 0) {
        return -1;
    }
    
    // 发送trailer headers（如果有）
    if (trailer_headers && strlen(trailer_headers) > 0) {
        if (send_data(ctx, trailer_headers, strlen(trailer_headers)) < 0) {
            return -1;
        }
    }
    
    // 发送最终的\r\n
    if (send_data(ctx, "\r\n", 2) < 0) {
        return -1;
    }
    
    ctx->finished = true;
    
    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "Finished chunked transfer: %zu total bytes", ctx->total_sent);
    log_message(LOG_LEVEL_DEBUG, log_msg);
    
    return 0;
}

// 流式发送文件内容（支持分块传输编码）
int chunked_send_file_stream(chunked_context_t *ctx, int file_fd, 
                            size_t file_size, chunked_config_t *config) {
    if (!ctx || file_fd < 0 || ctx->finished) {
        return -1;
    }
    
    size_t chunk_size = config ? config->chunk_size : DEFAULT_CHUNK_SIZE;
    char *buffer = malloc(chunk_size);
    if (!buffer) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate buffer for file streaming");
        return -1;
    }
    
    ssize_t bytes_read;
    int result = 0;
    
    // 重置文件指针
    lseek(file_fd, 0, SEEK_SET);
    
    while ((bytes_read = read(file_fd, buffer, chunk_size)) > 0) {
        if (chunked_send_chunk(ctx, buffer, bytes_read) < 0) {
            log_message(LOG_LEVEL_ERROR, "Failed to send chunk during file streaming");
            result = -1;
            break;
        }
    }
    
    if (bytes_read < 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to read from file during streaming");
        result = -1;
    }
    
    free(buffer);
    
    // 发送最后的空块
    if (result == 0) {
        const char *trailer = config && config->enable_trailer ? 
                             "X-Stream-Source: file\r\n" : NULL;
        if (chunked_send_final_chunk(ctx, trailer) < 0) {
            result = -1;
        }
    }
    
    return result;
}

// 流式发送动态内容（支持分块传输编码）
int chunked_send_stream(chunked_context_t *ctx, stream_data_callback_t callback,
                       void *user_data, chunked_config_t *config) {
    if (!ctx || !callback || ctx->finished) {
        return -1;
    }
    
    size_t chunk_size = config ? config->chunk_size : DEFAULT_CHUNK_SIZE;
    char *buffer = malloc(chunk_size);
    if (!buffer) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate buffer for streaming");
        return -1;
    }
    
    int result = 0;
    int bytes_generated;
    
    while ((bytes_generated = callback(user_data, buffer, chunk_size)) > 0) {
        if (chunked_send_chunk(ctx, buffer, bytes_generated) < 0) {
            log_message(LOG_LEVEL_ERROR, "Failed to send chunk during streaming");
            result = -1;
            break;
        }
    }
    
    if (bytes_generated < 0) {
        log_message(LOG_LEVEL_ERROR, "Stream data callback returned error");
        result = -1;
    }
    
    free(buffer);
    
    // 发送最后的空块
    if (result == 0) {
        const char *trailer = config && config->enable_trailer ? 
                             "X-Stream-Source: dynamic\r\n" : NULL;
        if (chunked_send_final_chunk(ctx, trailer) < 0) {
            result = -1;
        }
    }
    
    return result;
}

// 检查请求是否支持分块传输编码
bool chunked_is_supported(const char *request_headers) {
    if (!request_headers) {
        return false;
    }
    
    // 检查HTTP版本是否支持分块传输编码（HTTP/1.1及以上）
    if (strstr(request_headers, "HTTP/1.1") || strstr(request_headers, "HTTP/2")) {
        return true;
    }
    
    return false;
}

// 检查内容是否应该使用分块传输编码
bool chunked_should_use(const char *content_type, size_t content_length) {
    // 如果内容长度未知或者是动态内容，建议使用分块传输编码
    if (content_length == 0 || content_length == (size_t)-1) {
        return true;
    }
    
    // 对于某些内容类型，优先使用分块传输编码
    if (content_type) {
        if (strstr(content_type, "text/html") ||
            strstr(content_type, "application/json") ||
            strstr(content_type, "text/event-stream") ||
            strstr(content_type, "application/x-ndjson")) {
            return true;
        }
    }
    
    // 对于大文件，使用分块传输编码
    if (content_length > 1024 * 1024) { // 1MB
        return true;
    }
    
    return false;
}

// 获取默认分块传输编码配置
chunked_config_t *chunked_get_default_config(void) {
    chunked_config_t *config = malloc(sizeof(chunked_config_t));
    if (!config) {
        return NULL;
    }
    
    config->chunk_size = DEFAULT_CHUNK_SIZE;
    config->enable_trailer = false;
    
    return config;
}

// 释放分块传输编码配置
void chunked_config_free(chunked_config_t *config) {
    if (config) {
        free(config);
    }
} 