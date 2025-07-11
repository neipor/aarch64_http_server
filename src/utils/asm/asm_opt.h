#ifndef ASM_OPT_H
#define ASM_OPT_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

// 检查是否支持汇编优化
int asm_opt_is_supported(void);

// 获取CPU特性
uint32_t asm_opt_get_cpu_features(void);

// CPU特性标志（aarch64）
#define CPU_FEATURE_NEON    (1 << 0)
#define CPU_FEATURE_CRC32   (1 << 1)
#define CPU_FEATURE_AES     (1 << 2)
#define CPU_FEATURE_SHA1    (1 << 3)
#define CPU_FEATURE_SHA2    (1 << 4)
#define CPU_FEATURE_SVE     (1 << 5)

// 内存操作优化
void* asm_opt_memcpy(void* dest, const void* src, size_t n);
void* asm_opt_memset(void* s, int c, size_t n);
int asm_opt_memcmp(const void* s1, const void* s2, size_t n);
void* asm_opt_memmove(void* dest, const void* src, size_t n);

// 字符串操作优化
size_t asm_opt_strlen(const char* s);
char* asm_opt_strcpy(char* dest, const char* src);
char* asm_opt_strncpy(char* dest, const char* src, size_t n);
int asm_opt_strcmp(const char* s1, const char* s2);
int asm_opt_strncmp(const char* s1, const char* s2, size_t n);
char* asm_opt_strstr(const char* haystack, const char* needle);
char* asm_opt_strchr(const char* s, int c);
char* asm_opt_strrchr(const char* s, int c);  // 添加strrchr函数声明

// 哈希函数优化
uint32_t asm_opt_hash_string(const char* str);
uint32_t asm_opt_hash_data(const void* data, size_t len);
uint32_t asm_opt_crc32(const void* data, size_t len);

// 网络字节序转换优化
uint16_t asm_opt_htons(uint16_t hostshort);
uint32_t asm_opt_htonl(uint32_t hostlong);
uint16_t asm_opt_ntohs(uint16_t netshort);
uint32_t asm_opt_ntohl(uint32_t netlong);

// 缓冲区操作优化
size_t asm_opt_buffer_copy(void* dest, const void* src, size_t n);
size_t asm_opt_buffer_find(const void* buf, size_t buf_len, const void* pattern, size_t pattern_len);
int asm_opt_buffer_compare(const void* buf1, const void* buf2, size_t n);

// 数据压缩辅助优化
void asm_opt_compress_prepare(const void* data, size_t len, uint32_t* checksum);
void asm_opt_compress_finalize(const void* data, size_t len, uint32_t* checksum);

// 带宽控制优化
void asm_opt_bandwidth_update_tokens(uint64_t* tokens, uint64_t rate, uint64_t interval);
int asm_opt_bandwidth_check_limit(uint64_t tokens, uint64_t required);

// HTTP头部解析优化
int asm_opt_parse_http_header(const char* header, size_t len, char** key, char** value);
int asm_opt_find_http_header_end(const char* buffer, size_t len);
int asm_opt_validate_http_method(const char* method, size_t len);

// base64编码/解码优化
size_t asm_opt_base64_encode(const void* src, size_t src_len, char* dst, size_t dst_len);
size_t asm_opt_base64_decode(const char* src, size_t src_len, void* dst, size_t dst_len);

// 时间戳优化
uint64_t asm_opt_get_timestamp_us(void);
uint64_t asm_opt_get_timestamp_ns(void);

// 性能计数器
typedef struct {
    uint64_t cycles;
    uint64_t instructions;
    uint32_t cache_misses;
    uint32_t branch_misses;
} asm_opt_perf_counter_t;

void asm_opt_perf_counter_start(asm_opt_perf_counter_t* counter);
void asm_opt_perf_counter_stop(asm_opt_perf_counter_t* counter);
void asm_opt_perf_counter_print(const asm_opt_perf_counter_t* counter);

// 初始化汇编优化模块
void asm_opt_init(void);

// HTTP协议专用汇编优化函数
int asm_opt_fast_http_method_detect(const char* method, size_t len);
size_t asm_opt_generate_status_response(char* buffer, size_t buffer_size, 
                                       int status_code, const char* reason);
size_t asm_opt_write_http_header(char* buffer, size_t buffer_size,
                                const char* name, const char* value);
size_t asm_opt_generate_content_length_header(char* buffer, size_t buffer_size, 
                                             size_t content_length);

// 网络I/O汇编优化函数
ssize_t asm_opt_socket_send(int sockfd, const void* buffer, size_t len, int flags);
ssize_t asm_opt_socket_recv(int sockfd, void* buffer, size_t len, int flags);
size_t asm_opt_network_buffer_copy(void* dest, const void* src, size_t len);

// 高级性能分析和调试汇编函数
void asm_opt_get_cpu_performance_counters(asm_opt_perf_counter_t* counters);
uint64_t asm_opt_memory_bandwidth_test(void* buffer, size_t size, int iterations);
uint64_t asm_opt_latency_test(void* ptr, int iterations);

// SSL/TLS加密相关汇编优化
void asm_opt_aes_encrypt_block(const uint8_t* plaintext, uint8_t* ciphertext, 
                              const uint8_t* round_keys, int rounds);
void asm_opt_sha256_hash(const uint8_t* data, size_t len, uint8_t* hash);

// 调试和诊断汇编函数
uint64_t asm_opt_get_cpu_frequency(void);
void asm_opt_get_cache_info(uint32_t* l1_cache_size, uint32_t* l2_cache_size, 
                           uint32_t* l3_cache_size);
void asm_opt_print_statistics(void);

// 高级NEON SIMD优化函数
void asm_opt_simd_zero_buffer(void* buffer, size_t size);
uint64_t asm_opt_simd_sum_array(const uint32_t* array, size_t count);
uint32_t asm_opt_simd_max_array(const uint32_t* array, size_t count);
int asm_opt_simd_validate_ascii(const char* str, size_t len);
size_t asm_opt_simd_generate_status_line(char* buffer, size_t buffer_size, 
                                       int status_code, const char* reason_phrase);
size_t asm_opt_simd_url_decode(const char* src, size_t src_len, char* dst, size_t dst_len);
int asm_opt_simd_needs_json_escape(const char* str, size_t len);

#endif // ASM_OPT_H 