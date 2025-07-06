#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "compress.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BUFFER_SIZE (64 * 1024)  // 64KB
#define DEFAULT_MIN_LENGTH 1024          // 1KB
#define MAX_MIME_TYPES 50               // 最大MIME类型数量

// 创建压缩配置
compress_config_t *compress_config_create(void) {
    compress_config_t *config = calloc(1, sizeof(compress_config_t));
    if (!config) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate compression config");
        return NULL;
    }
    
    // 设置默认值
    config->enable_compression = true;
    config->level = COMPRESS_LEVEL_DEFAULT;
    config->min_length = DEFAULT_MIN_LENGTH;
    config->compression_buffer_size = DEFAULT_BUFFER_SIZE;
    config->enable_vary = true;
    
    // 分配MIME类型数组
    config->mime_types = calloc(MAX_MIME_TYPES, sizeof(char *));
    if (!config->mime_types) {
        free(config);
        log_message(LOG_LEVEL_ERROR, "Failed to allocate MIME types array");
        return NULL;
    }
    
    // 添加默认MIME类型
    const char *default_types[] = {
        "text/html", "text/css", "text/plain", "text/javascript",
        "application/javascript", "application/json", "application/xml",
        "text/xml", "application/x-javascript"
    };
    
    for (size_t i = 0; i < sizeof(default_types) / sizeof(default_types[0]); i++) {
        compress_config_add_mime_type(config, default_types[i]);
    }
    
    return config;
}

// 释放压缩配置
void compress_config_free(compress_config_t *config) {
    if (!config) return;
    
    if (config->mime_types) {
        for (int i = 0; i < config->mime_types_count; i++) {
            free(config->mime_types[i]);
        }
        free(config->mime_types);
    }
    
    free(config);
}

// 添加MIME类型到配置
int compress_config_add_mime_type(compress_config_t *config, const char *mime_type) {
    if (!config || !mime_type || config->mime_types_count >= MAX_MIME_TYPES) {
        return -1;
    }
    
    // 检查是否已存在
    for (int i = 0; i < config->mime_types_count; i++) {
        if (strcmp(config->mime_types[i], mime_type) == 0) {
            return 0;  // 已存在
        }
    }
    
    // 添加新MIME类型
    config->mime_types[config->mime_types_count] = strdup(mime_type);
    if (!config->mime_types[config->mime_types_count]) {
        return -1;
    }
    
    config->mime_types_count++;
    return 0;
}

// 检查MIME类型是否应该压缩
bool should_compress_mime_type(compress_config_t *config, const char *mime_type) {
    if (!config || !mime_type) return false;
    
    for (int i = 0; i < config->mime_types_count; i++) {
        if (strncmp(config->mime_types[i], mime_type, strlen(config->mime_types[i])) == 0) {
            return true;
        }
    }
    
    return false;
}

// 创建压缩上下文
compress_context_t *compress_context_create(compress_config_t *config) {
    if (!config) return NULL;
    
    compress_context_t *ctx = calloc(1, sizeof(compress_context_t));
    if (!ctx) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate compression context");
        return NULL;
    }
    
    ctx->buffer_size = config->compression_buffer_size;
    
    // 分配缓冲区
    ctx->in_buffer = malloc(ctx->buffer_size);
    ctx->out_buffer = malloc(ctx->buffer_size);
    
    if (!ctx->in_buffer || !ctx->out_buffer) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate compression buffers");
        compress_context_free(ctx);
        return NULL;
    }
    
    // 初始化zlib流
    ctx->stream.zalloc = Z_NULL;
    ctx->stream.zfree = Z_NULL;
    ctx->stream.opaque = Z_NULL;
    
    if (deflateInit2(&ctx->stream, config->level, Z_DEFLATED, 
                     31, // 15 + 16 for gzip
                     8,  // 默认内存级别
                     Z_DEFAULT_STRATEGY) != Z_OK) {
        log_message(LOG_LEVEL_ERROR, "Failed to initialize zlib stream");
        compress_context_free(ctx);
        return NULL;
    }
    
    ctx->initialized = true;
    return ctx;
}

// 释放压缩上下文
void compress_context_free(compress_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->initialized) {
        deflateEnd(&ctx->stream);
    }
    
    free(ctx->in_buffer);
    free(ctx->out_buffer);
    free(ctx);
}

// 压缩数据块
int compress_data(compress_context_t *ctx, const void *in, size_t in_len,
                 void *out, size_t *out_len, int flush) {
    if (!ctx || !ctx->initialized) return -1;
    
    ctx->stream.next_in = (Bytef *)in;
    ctx->stream.avail_in = in_len;
    ctx->stream.next_out = out;
    ctx->stream.avail_out = *out_len;
    
    int ret = deflate(&ctx->stream, flush);
    if (ret < 0) {
        log_message(LOG_LEVEL_ERROR, "Compression failed");
        return ret;
    }
    
    *out_len = *out_len - ctx->stream.avail_out;
    return ret;
}

// 检查客户端是否支持压缩
bool client_accepts_compression(const char *accept_encoding) {
    if (!accept_encoding) return false;
    
    // 检查是否支持gzip
    return (strstr(accept_encoding, "gzip") != NULL);
}

// 获取压缩方法字符串
const char *get_compression_method(void) {
    return "gzip";
}

// 重置压缩上下文
void compress_context_reset(compress_context_t *ctx) {
    if (!ctx || !ctx->initialized) return;
    
    deflateReset(&ctx->stream);
} 