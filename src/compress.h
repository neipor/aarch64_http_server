#ifndef COMPRESS_H
#define COMPRESS_H

#include <zlib.h>
#include <stdbool.h>

// 压缩级别枚举
typedef enum {
    COMPRESS_LEVEL_OFF = 0,    // 不压缩
    COMPRESS_LEVEL_FAST = 1,   // 快速压缩
    COMPRESS_LEVEL_DEFAULT = 6, // 默认压缩级别
    COMPRESS_LEVEL_BEST = 9    // 最佳压缩
} compress_level_t;

// 压缩配置结构
typedef struct {
    bool enable_compression;           // 是否启用压缩
    compress_level_t level;           // 压缩级别
    size_t min_length;                // 最小压缩长度（字节）
    char **mime_types;                // 要压缩的MIME类型列表
    int mime_types_count;             // MIME类型数量
    bool enable_vary;                 // 是否添加Vary头
    size_t compression_buffer_size;   // 压缩缓冲区大小
} compress_config_t;

// 压缩上下文结构
typedef struct {
    z_stream stream;           // zlib流
    unsigned char *in_buffer;  // 输入缓冲区
    unsigned char *out_buffer; // 输出缓冲区
    size_t buffer_size;       // 缓冲区大小
    bool initialized;         // 是否已初始化
} compress_context_t;

// 初始化压缩配置
compress_config_t *compress_config_create(void);

// 释放压缩配置
void compress_config_free(compress_config_t *config);

// 添加MIME类型到配置
int compress_config_add_mime_type(compress_config_t *config, const char *mime_type);

// 检查MIME类型是否应该压缩
bool should_compress_mime_type(compress_config_t *config, const char *mime_type);

// 创建压缩上下文
compress_context_t *compress_context_create(compress_config_t *config);

// 释放压缩上下文
void compress_context_free(compress_context_t *ctx);

// 压缩数据块
int compress_data(compress_context_t *ctx, const void *in, size_t in_len,
                 void *out, size_t *out_len, int flush);

// 检查客户端是否支持压缩
bool client_accepts_compression(const char *accept_encoding);

// 获取压缩方法字符串
const char *get_compression_method(void);

// 重置压缩上下文
void compress_context_reset(compress_context_t *ctx);

#endif // COMPRESS_H 