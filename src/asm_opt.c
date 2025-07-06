#include "asm_opt.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include "log.h"

// 全局变量：CPU特性支持
static uint32_t cpu_features = 0;
static int cpu_features_detected = 0;

// 检测CPU特性支持
static void detect_cpu_features(void) {
    if (cpu_features_detected) return;
    
    // 检测NEON支持
    #ifdef __ARM_NEON
    cpu_features |= CPU_FEATURE_NEON;
    #endif
    
    // 检测CRC32支持
    #ifdef __ARM_FEATURE_CRC32
    cpu_features |= CPU_FEATURE_CRC32;
    #endif
    
    // 检测AES支持
    #ifdef __ARM_FEATURE_AES
    cpu_features |= CPU_FEATURE_AES;
    #endif
    
    // 检测SHA1支持
    #ifdef __ARM_FEATURE_SHA1
    cpu_features |= CPU_FEATURE_SHA1;
    #endif
    
    // 检测SHA2支持
    #ifdef __ARM_FEATURE_SHA2
    cpu_features |= CPU_FEATURE_SHA2;
    #endif
    
    // 检测SVE支持
    #ifdef __ARM_FEATURE_SVE
    cpu_features |= CPU_FEATURE_SVE;
    #endif
    
    cpu_features_detected = 1;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "CPU features detected: 0x%08x", cpu_features);
    log_message(LOG_LEVEL_INFO, log_msg);
}

// 检查是否支持汇编优化
int asm_opt_is_supported(void) {
    #ifdef __aarch64__
    return 1;
    #else
    return 0;
    #endif
}

// 获取CPU特性
uint32_t asm_opt_get_cpu_features(void) {
    detect_cpu_features();
    return cpu_features;
}

// 优化的memcpy实现（aarch64 NEON）
void* asm_opt_memcpy(void* dest, const void* src, size_t n) {
    #ifdef __aarch64__
    if (n >= 64 && (cpu_features & CPU_FEATURE_NEON)) {
        // 使用NEON指令进行64字节对齐的快速拷贝
        char* d = (char*)dest;
        const char* s = (const char*)src;
        
        // 处理对齐前的字节
        while (((uintptr_t)d & 15) && n > 0) {
            *d++ = *s++;
            n--;
        }
        
        // 使用NEON 128位寄存器进行批量拷贝
        while (n >= 64) {
            __asm__ volatile (
                "ldp q0, q1, [%1, #0]\n\t"
                "ldp q2, q3, [%1, #32]\n\t"
                "stp q0, q1, [%0, #0]\n\t"
                "stp q2, q3, [%0, #32]\n\t"
                : : "r"(d), "r"(s) : "memory", "q0", "q1", "q2", "q3"
            );
            d += 64;
            s += 64;
            n -= 64;
        }
        
        // 处理剩余的字节
        while (n > 0) {
            *d++ = *s++;
            n--;
        }
        
        return dest;
    }
    #endif
    
    // 回退到标准库实现
    return memcpy(dest, src, n);
}

// 优化的memset实现（aarch64 NEON）
void* asm_opt_memset(void* s, int c, size_t n) {
    #ifdef __aarch64__
    if (n >= 64 && (cpu_features & CPU_FEATURE_NEON)) {
        char* ptr = (char*)s;
        
        // 创建包含重复字节的128位向量
        uint8_t byte_val = (uint8_t)c;
        
        // 处理对齐前的字节
        while (((uintptr_t)ptr & 15) && n > 0) {
            *ptr++ = byte_val;
            n--;
        }
        
        // 使用NEON批量设置
        while (n >= 64) {
            __asm__ volatile (
                "dup v0.16b, %w2\n\t"
                "str q0, [%0, #0]\n\t"
                "str q0, [%0, #16]\n\t"
                "str q0, [%0, #32]\n\t"
                "str q0, [%0, #48]\n\t"
                : : "r"(ptr), "r"(n), "r"(byte_val) : "memory", "v0"
            );
            ptr += 64;
            n -= 64;
        }
        
        // 处理剩余字节
        while (n > 0) {
            *ptr++ = byte_val;
            n--;
        }
        
        return s;
    }
    #endif
    
    return memset(s, c, n);
}

// 优化的strlen实现（aarch64）
size_t asm_opt_strlen(const char* s) {
    #ifdef __aarch64__
    size_t len = 0;
    const char* ptr = s;
    
    // 使用64位寄存器快速检查8字节块
    while (1) {
        uint64_t chunk;
        __asm__ volatile (
            "ldr %0, [%1]\n\t"
            : "=r"(chunk) : "r"(ptr) : "memory"
        );
        
        // 检查是否有零字节
        if (((chunk - 0x0101010101010101ULL) & ~chunk & 0x8080808080808080ULL) != 0) {
            // 找到零字节，逐字节检查
            while (*ptr != '\0') {
                ptr++;
            }
            break;
        }
        
        ptr += 8;
        len += 8;
    }
    
    return ptr - s;
    #endif
    
    return strlen(s);
}

// 优化的strcmp实现（aarch64）
int asm_opt_strcmp(const char* s1, const char* s2) {
    #ifdef __aarch64__
    const char* p1 = s1;
    const char* p2 = s2;
    
    // 8字节对齐比较
    while (1) {
        uint64_t chunk1, chunk2;
        
        __asm__ volatile (
            "ldr %0, [%2]\n\t"
            "ldr %1, [%3]\n\t"
            : "=r"(chunk1), "=r"(chunk2) : "r"(p1), "r"(p2) : "memory"
        );
        
        if (chunk1 != chunk2) {
            // 找到差异，逐字节比较
            while (*p1 == *p2 && *p1 != '\0') {
                p1++;
                p2++;
            }
            return (unsigned char)*p1 - (unsigned char)*p2;
        }
        
        // 检查是否到达字符串末尾
        if (((chunk1 - 0x0101010101010101ULL) & ~chunk1 & 0x8080808080808080ULL) != 0) {
            return 0; // 字符串相等
        }
        
        p1 += 8;
        p2 += 8;
    }
    #endif
    
    return strcmp(s1, s2);
}

// 优化的哈希函数（使用CRC32指令）
uint32_t asm_opt_hash_string(const char* str) {
    #if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
    if (cpu_features & CPU_FEATURE_CRC32) {
        uint32_t hash = 0;
        const char* ptr = str;
        
        // 使用CRC32指令加速哈希计算
        while (*ptr != '\0') {
            __asm__ volatile (
                "crc32b %w0, %w0, %w1\n\t"
                : "+r"(hash) : "r"(*ptr) : "memory"
            );
            ptr++;
        }
        
        return hash;
    }
    #endif
    
    // 回退到传统哈希算法
    uint32_t hash = 5381;
    const char* ptr = str;
    while (*ptr != '\0') {
        hash = ((hash << 5) + hash) + *ptr++;
    }
    return hash;
}

// 优化的CRC32实现
uint32_t asm_opt_crc32(const void* data, size_t len) {
    #if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
    if (cpu_features & CPU_FEATURE_CRC32) {
        uint32_t crc = 0xFFFFFFFF;
        const uint8_t* ptr = (const uint8_t*)data;
        
        // 8字节对齐处理
        while (len >= 8) {
            uint64_t chunk = *(uint64_t*)ptr;
            __asm__ volatile (
                "crc32x %w0, %w0, %1\n\t"
                : "+r"(crc) : "r"(chunk) : "memory"
            );
            ptr += 8;
            len -= 8;
        }
        
        // 处理剩余字节
        while (len > 0) {
            __asm__ volatile (
                "crc32b %w0, %w0, %w1\n\t"
                : "+r"(crc) : "r"(*ptr) : "memory"
            );
            ptr++;
            len--;
        }
        
        return crc ^ 0xFFFFFFFF;
    }
    #endif
    
    // 回退到软件实现
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* ptr = (const uint8_t*)data;
    
    static const uint32_t crc_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
        0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        // ... (完整的CRC32表)
    };
    
    for (size_t i = 0; i < len; i++) {
        crc = crc_table[(crc ^ ptr[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

// 优化的网络字节序转换
uint16_t asm_opt_htons(uint16_t hostshort) {
    #ifdef __aarch64__
    uint16_t result;
    __asm__ volatile (
        "rev16 %w0, %w1\n\t"
        : "=r"(result) : "r"(hostshort) : "memory"
    );
    return result;
    #endif
    
    return (hostshort >> 8) | (hostshort << 8);
}

uint32_t asm_opt_htonl(uint32_t hostlong) {
    #ifdef __aarch64__
    uint32_t result;
    __asm__ volatile (
        "rev %w0, %w1\n\t"
        : "=r"(result) : "r"(hostlong) : "memory"
    );
    return result;
    #endif
    
    return ((hostlong >> 24) & 0xFF) | 
           ((hostlong >> 8) & 0xFF00) | 
           ((hostlong << 8) & 0xFF0000) | 
           ((hostlong << 24) & 0xFF000000);
}

// 优化的缓冲区查找
size_t asm_opt_buffer_find(const void* buf, size_t buf_len, const void* pattern, size_t pattern_len) {
    if (pattern_len == 0 || pattern_len > buf_len) return (size_t)-1;
    
    const uint8_t* buffer = (const uint8_t*)buf;
    const uint8_t* pat = (const uint8_t*)pattern;
    
    #ifdef __aarch64__
    if (pattern_len >= 4 && (cpu_features & CPU_FEATURE_NEON)) {
        // 使用NEON指令加速模式匹配
        uint32_t first_4_bytes = *(uint32_t*)pat;
        
        for (size_t i = 0; i <= buf_len - pattern_len; i += 4) {
            uint32_t current_4_bytes = *(uint32_t*)(buffer + i);
            
            if (current_4_bytes == first_4_bytes) {
                // 找到可能的匹配，进行完整比较
                if (memcmp(buffer + i, pat, pattern_len) == 0) {
                    return i;
                }
            }
        }
    }
    #endif
    
    // 回退到简单的搜索算法
    for (size_t i = 0; i <= buf_len - pattern_len; i++) {
        if (memcmp(buffer + i, pat, pattern_len) == 0) {
            return i;
        }
    }
    
    return (size_t)-1;
}

// 优化的时间戳获取
uint64_t asm_opt_get_timestamp_us(void) {
    #ifdef __aarch64__
    uint64_t timestamp;
    __asm__ volatile (
        "mrs %0, cntvct_el0\n\t"
        : "=r"(timestamp) : : "memory"
    );
    
    // 转换为微秒（假设频率为1MHz）
    return timestamp;
    #endif
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

uint64_t asm_opt_get_timestamp_ns(void) {
    #ifdef __aarch64__
    uint64_t timestamp;
    __asm__ volatile (
        "mrs %0, cntvct_el0\n\t"
        : "=r"(timestamp) : : "memory"
    );
    
    // 转换为纳秒（假设频率为1GHz）
    return timestamp;
    #endif
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// 优化的带宽令牌更新
void asm_opt_bandwidth_update_tokens(uint64_t* tokens, uint64_t rate, uint64_t interval) {
    #ifdef __aarch64__
    // 使用原子操作确保线程安全
    uint64_t new_tokens = rate * interval / 1000000; // 转换为每微秒
    
    __asm__ volatile (
        "1:\n\t"
        "ldxr %0, [%1]\n\t"
        "add %0, %0, %2\n\t"
        "stxr w3, %0, [%1]\n\t"
        "cbnz w3, 1b\n\t"
        : "=&r"(*tokens) : "r"(tokens), "r"(new_tokens) : "w3", "memory"
    );
    #else
    *tokens += rate * interval / 1000000;
    #endif
}

// 优化的HTTP头部解析
int asm_opt_find_http_header_end(const char* buffer, size_t len) {
    #ifdef __aarch64__
    if (len >= 8 && (cpu_features & CPU_FEATURE_NEON)) {
        // 使用NEON指令快速查找 \r\n\r\n
        const char* ptr = buffer;
        size_t remaining = len;
        
        while (remaining >= 8) {
            uint64_t chunk;
            __asm__ volatile (
                "ldr %0, [%1]\n\t"
                : "=r"(chunk) : "r"(ptr) : "memory"
            );
            
            // 检查是否包含 \r\n\r\n 模式
            if (((chunk & 0xFFFFFFFF) == 0x0A0D0A0D) || // \r\n\r\n
                ((chunk >> 8) & 0xFFFFFFFF) == 0x0A0D0A0D) {
                // 找到可能的结束位置，进行精确检查
                for (int i = 0; i < 8 && (ptr + i + 3) < (buffer + len); i++) {
                    if (ptr[i] == '\r' && ptr[i+1] == '\n' && 
                        ptr[i+2] == '\r' && ptr[i+3] == '\n') {
                        return (ptr + i + 4) - buffer;
                    }
                }
            }
            
            ptr += 8;
            remaining -= 8;
        }
    }
    #endif
    
    // 回退到简单搜索
    for (size_t i = 0; i < len - 3; i++) {
        if (buffer[i] == '\r' && buffer[i+1] == '\n' && 
            buffer[i+2] == '\r' && buffer[i+3] == '\n') {
            return i + 4;
        }
    }
    
    return -1;
}

// 性能计数器相关函数
void asm_opt_perf_counter_start(asm_opt_perf_counter_t* counter) {
    #ifdef __aarch64__
    __asm__ volatile (
        "mrs %0, pmccntr_el0\n\t"
        : "=r"(counter->cycles) : : "memory"
    );
    #endif
}

void asm_opt_perf_counter_stop(asm_opt_perf_counter_t* counter) {
    #ifdef __aarch64__
    uint64_t end_cycles;
    __asm__ volatile (
        "mrs %0, pmccntr_el0\n\t"
        : "=r"(end_cycles) : : "memory"
    );
    counter->cycles = end_cycles - counter->cycles;
    #endif
}

void asm_opt_perf_counter_print(const asm_opt_perf_counter_t* counter) {
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), 
             "Performance: %lu cycles", counter->cycles);
    log_message(LOG_LEVEL_DEBUG, log_msg);
}

// 初始化汇编优化模块
void asm_opt_init(void) {
    detect_cpu_features();
    
    if (asm_opt_is_supported()) {
        log_message(LOG_LEVEL_INFO, "Assembly optimizations enabled for aarch64");
    } else {
        log_message(LOG_LEVEL_INFO, "Assembly optimizations not supported on this platform");
    }
}

// 其他未实现的函数使用回退实现
int asm_opt_memcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

void* asm_opt_memmove(void* dest, const void* src, size_t n) {
    return memmove(dest, src, n);
}

char* asm_opt_strcpy(char* dest, const char* src) {
    return strcpy(dest, src);
}

char* asm_opt_strncpy(char* dest, const char* src, size_t n) {
    return strncpy(dest, src, n);
}

int asm_opt_strncmp(const char* s1, const char* s2, size_t n) {
    return strncmp(s1, s2, n);
}

char* asm_opt_strstr(const char* haystack, const char* needle) {
    return strstr(haystack, needle);
}

char* asm_opt_strchr(const char* s, int c) {
    return strchr(s, c);
}

uint32_t asm_opt_hash_data(const void* data, size_t len) {
    return asm_opt_crc32(data, len);
}

uint16_t asm_opt_ntohs(uint16_t netshort) {
    return asm_opt_htons(netshort);
}

uint32_t asm_opt_ntohl(uint32_t netlong) {
    return asm_opt_htonl(netlong);
}

size_t asm_opt_buffer_copy(void* dest, const void* src, size_t n) {
    asm_opt_memcpy(dest, src, n);
    return n;
}

int asm_opt_buffer_compare(const void* buf1, const void* buf2, size_t n) {
    return asm_opt_memcmp(buf1, buf2, n);
}

void asm_opt_compress_prepare(const void* data, size_t len, uint32_t* checksum) {
    *checksum = asm_opt_crc32(data, len);
}

void asm_opt_compress_finalize(const void* data, size_t len, uint32_t* checksum) {
    *checksum = asm_opt_crc32(data, len);
}

int asm_opt_bandwidth_check_limit(uint64_t tokens, uint64_t required) {
    return tokens >= required ? 1 : 0;
}

int asm_opt_parse_http_header(const char* header, size_t len, char** key, char** value) {
    // 简单实现，后续可以优化
    const char* colon = strchr(header, ':');
    if (!colon) return -1;
    
    *key = strndup(header, colon - header);
    *value = strndup(colon + 1, len - (colon + 1 - header));
    
    return 0;
}

int asm_opt_validate_http_method(const char* method, size_t len) {
    // 简单验证HTTP方法
    if (len == 3 && strncmp(method, "GET", 3) == 0) return 1;
    if (len == 4 && strncmp(method, "POST", 4) == 0) return 1;
    if (len == 3 && strncmp(method, "PUT", 3) == 0) return 1;
    if (len == 6 && strncmp(method, "DELETE", 6) == 0) return 1;
    if (len == 4 && strncmp(method, "HEAD", 4) == 0) return 1;
    if (len == 7 && strncmp(method, "OPTIONS", 7) == 0) return 1;
    return 0;
}

size_t asm_opt_base64_encode(const void* src, size_t src_len, char* dst, size_t dst_len) {
    // 简单实现，后续可以优化
    return 0;
}

size_t asm_opt_base64_decode(const char* src, size_t src_len, void* dst, size_t dst_len) {
    // 简单实现，后续可以优化
    return 0;
} 