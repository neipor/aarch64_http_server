#ifndef CHUNKED_H
#define CHUNKED_H

#include <stddef.h>
#include <stdbool.h>
#include <openssl/ssl.h>

// 分块传输编码上下文
typedef struct {
    int socket_fd;          // 客户端socket
    SSL *ssl;               // SSL连接（如果是HTTPS）
    bool is_ssl;            // 是否是SSL连接
    bool finished;          // 是否已完成传输
    size_t total_sent;      // 已发送的总字节数
} chunked_context_t;

// 分块传输编码配置
typedef struct {
    size_t chunk_size;      // 分块大小（默认8KB）
    bool enable_trailer;    // 是否启用trailer headers
} chunked_config_t;

// 流式响应处理函数类型
typedef int (*stream_data_callback_t)(void *user_data, char *buffer, size_t max_size);

// 创建分块传输编码上下文
chunked_context_t *chunked_context_create(int socket_fd, SSL *ssl);

// 释放分块传输编码上下文
void chunked_context_free(chunked_context_t *ctx);

// 发送分块传输编码的响应头
int chunked_send_headers(chunked_context_t *ctx, int status_code, 
                        const char *content_type, const char *extra_headers);

// 发送一个数据块
int chunked_send_chunk(chunked_context_t *ctx, const char *data, size_t size);

// 发送最后的空块，结束分块传输
int chunked_send_final_chunk(chunked_context_t *ctx, const char *trailer_headers);

// 流式发送文件内容（支持分块传输编码）
int chunked_send_file_stream(chunked_context_t *ctx, int file_fd, 
                            size_t file_size, chunked_config_t *config);

// 流式发送动态内容（支持分块传输编码）
int chunked_send_stream(chunked_context_t *ctx, stream_data_callback_t callback,
                       void *user_data, chunked_config_t *config);

// 检查请求是否支持分块传输编码
bool chunked_is_supported(const char *request_headers);

// 检查内容是否应该使用分块传输编码
bool chunked_should_use(const char *content_type, size_t content_length);

// 获取默认分块传输编码配置
chunked_config_t *chunked_get_default_config(void);

// 释放分块传输编码配置
void chunked_config_free(chunked_config_t *config);

#endif // CHUNKED_H 