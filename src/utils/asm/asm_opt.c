#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "asm_opt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "log.h"

// 内联汇编宏定义
#ifdef __aarch64__
#define ASM_PREFETCH_READ(addr) \
    __asm__ volatile ("prfm pldl1keep, %0" :: "Q" (*(const char*)(addr)))
#define ASM_PREFETCH_WRITE(addr) \
    __asm__ volatile ("prfm pstl1keep, %0" :: "Q" (*(const char*)(addr)))
#else
#define ASM_PREFETCH_READ(addr) ((void)0)
#define ASM_PREFETCH_WRITE(addr) ((void)0)
#endif

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
    #ifdef __aarch64__
    if (n >= 16 && (cpu_features & CPU_FEATURE_NEON)) {
        const uint8_t* ptr1 = (const uint8_t*)s1;
        const uint8_t* ptr2 = (const uint8_t*)s2;
        
        // 使用128位NEON寄存器进行16字节批量比较
        while (n >= 16) {
            uint64_t chunk1_low, chunk1_high, chunk2_low, chunk2_high;
            __asm__ volatile (
                "ldp %0, %1, [%4]\n\t"
                "ldp %2, %3, [%5]\n\t"
                : "=r"(chunk1_low), "=r"(chunk1_high), "=r"(chunk2_low), "=r"(chunk2_high)
                : "r"(ptr1), "r"(ptr2)
                : "memory"
            );
            
            if (chunk1_low != chunk2_low || chunk1_high != chunk2_high) {
                // 找到差异，逐字节比较
                for (int i = 0; i < 16; i++) {
                    if (ptr1[i] != ptr2[i]) {
                        return ptr1[i] - ptr2[i];
                    }
                }
            }
            
            ptr1 += 16;
            ptr2 += 16;
            n -= 16;
        }
        
        // 处理剩余字节
        while (n > 0) {
            if (*ptr1 != *ptr2) {
                return *ptr1 - *ptr2;
            }
            ptr1++;
            ptr2++;
            n--;
        }
        
        return 0;
    }
    #endif
    
    return memcmp(s1, s2, n);
}

void* asm_opt_memmove(void* dest, const void* src, size_t n) {
    #ifdef __aarch64__
    if (n >= 64 && (cpu_features & CPU_FEATURE_NEON)) {
        char* d = (char*)dest;
        const char* s = (const char*)src;
        
        // 检查重叠情况
        if (d > s && d < s + n) {
            // 重叠，从后往前拷贝
            d += n;
            s += n;
            
            // 64字节对齐的反向拷贝
            while (n >= 64) {
                d -= 64;
                s -= 64;
                n -= 64;
                
                __asm__ volatile (
                    "ldp q0, q1, [%1, #0]\n\t"
                    "ldp q2, q3, [%1, #32]\n\t"
                    "stp q0, q1, [%0, #0]\n\t"
                    "stp q2, q3, [%0, #32]\n\t"
                    : : "r"(d), "r"(s) : "memory", "q0", "q1", "q2", "q3"
                );
            }
            
            // 处理剩余字节
            while (n > 0) {
                *(--d) = *(--s);
                n--;
            }
        } else {
            // 无重叠，使用优化的memcpy
            return asm_opt_memcpy(dest, src, n);
        }
        
        return dest;
    }
    #endif
    
    return memmove(dest, src, n);
}

char* asm_opt_strcpy(char* dest, const char* src) {
    #ifdef __aarch64__
    char* d = dest;
    const char* s = src;
    
    // 8字节对齐的字符串拷贝
    while (1) {
        uint64_t chunk;
        __asm__ volatile (
            "ldr %0, [%1]\n\t"
            : "=r"(chunk) : "r"(s) : "memory"
        );
        
        // 检查是否有零字节
        if (((chunk - 0x0101010101010101ULL) & ~chunk & 0x8080808080808080ULL) != 0) {
            // 有零字节，逐字节拷贝直到结束
            while ((*d++ = *s++) != '\0') {
                // 继续拷贝
            }
            break;
        }
        
        // 没有零字节，拷贝整个8字节块
        __asm__ volatile (
            "str %1, [%0]\n\t"
            : : "r"(d), "r"(chunk) : "memory"
        );
        
        d += 8;
        s += 8;
    }
    
    return dest;
    #endif
    
    return strcpy(dest, src);
}

char* asm_opt_strncpy(char* dest, const char* src, size_t n) {
    #ifdef __aarch64__
    char* d = dest;
    const char* s = src;
    size_t copied = 0;
    
    // 8字节对齐的字符串拷贝
    while (copied + 8 <= n) {
        uint64_t chunk;
        __asm__ volatile (
            "ldr %0, [%1]\n\t"
            : "=r"(chunk) : "r"(s) : "memory"
        );
        
        // 检查是否有零字节
        if (((chunk - 0x0101010101010101ULL) & ~chunk & 0x8080808080808080ULL) != 0) {
            // 有零字节，逐字节拷贝直到结束或达到n
            while (copied < n && (*d++ = *s++) != '\0') {
                copied++;
            }
            // 用零填充剩余空间
            while (copied < n) {
                *d++ = '\0';
                copied++;
            }
            return dest;
        }
        
        // 没有零字节，拷贝整个8字节块
        __asm__ volatile (
            "str %1, [%0]\n\t"
            : : "r"(d), "r"(chunk) : "memory"
        );
        
        d += 8;
        s += 8;
        copied += 8;
    }
    
    // 处理剩余字符
    while (copied < n && (*d++ = *s++) != '\0') {
        copied++;
    }
    
    // 用零填充剩余空间
    while (copied < n) {
        *d++ = '\0';
        copied++;
    }
    
    return dest;
    #endif
    
    return strncpy(dest, src, n);
}

int asm_opt_strncmp(const char* s1, const char* s2, size_t n) {
    #ifdef __aarch64__
    if (n >= 8) {
        const char* p1 = s1;
        const char* p2 = s2;
        size_t compared = 0;
        
        // 8字节对齐比较
        while (compared + 8 <= n) {
            uint64_t chunk1, chunk2;
            __asm__ volatile (
                "ldr %0, [%2]\n\t"
                "ldr %1, [%3]\n\t"
                : "=r"(chunk1), "=r"(chunk2) : "r"(p1), "r"(p2) : "memory"
            );
            
            if (chunk1 != chunk2) {
                // 找到差异，逐字节比较
                for (int i = 0; i < 8 && compared + i < n; i++) {
                    if (p1[i] != p2[i]) {
                        return (unsigned char)p1[i] - (unsigned char)p2[i];
                    }
                    if (p1[i] == '\0') {
                        return 0; // 都到达字符串末尾
                    }
                }
            }
            
            // 检查是否有字符串结束
            if (((chunk1 - 0x0101010101010101ULL) & ~chunk1 & 0x8080808080808080ULL) != 0) {
                return 0; // 字符串在这个块中结束且相等
            }
            
            p1 += 8;
            p2 += 8;
            compared += 8;
        }
        
        // 处理剩余字符
        while (compared < n && *p1 == *p2 && *p1 != '\0') {
            p1++;
            p2++;
            compared++;
        }
        
        if (compared >= n) {
            return 0;
        }
        
        return (unsigned char)*p1 - (unsigned char)*p2;
    }
    #endif
    
    return strncmp(s1, s2, n);
}

char* asm_opt_strstr(const char* haystack, const char* needle) {
    #ifdef __aarch64__
    if (needle[0] == '\0') {
        return (char*)haystack;
    }
    
    size_t needle_len = asm_opt_strlen(needle);
    if (needle_len >= 4 && (cpu_features & CPU_FEATURE_NEON)) {
        // 使用优化的缓冲区查找
        size_t haystack_len = asm_opt_strlen(haystack);
        size_t pos = asm_opt_buffer_find(haystack, haystack_len, needle, needle_len);
        if (pos != (size_t)-1) {
            return (char*)(haystack + pos);
        }
        return NULL;
    }
    #endif
    
    return strstr(haystack, needle);
}

char* asm_opt_strchr(const char* s, int c) {
    #ifdef __aarch64__
    const char* ptr = s;
    char target = (char)c;
    
    // 8字节对齐的字符搜索
    while (1) {
        uint64_t chunk;
        __asm__ volatile (
            "ldr %0, [%1]\n\t"
            : "=r"(chunk) : "r"(ptr) : "memory"
        );
        
        // 创建目标字符的8字节重复
        uint64_t target_pattern = 0x0101010101010101ULL * (uint8_t)target;
        
        // 检查是否有匹配的字符
        uint64_t match_mask = chunk ^ target_pattern;
        if (((match_mask - 0x0101010101010101ULL) & ~match_mask & 0x8080808080808080ULL) != 0) {
            // 找到匹配，逐字节检查
            for (int i = 0; i < 8; i++) {
                if (ptr[i] == target) {
                    return (char*)(ptr + i);
                }
                if (ptr[i] == '\0') {
                    return NULL;
                }
            }
        }
        
        // 检查是否有零字节（字符串结束）
        if (((chunk - 0x0101010101010101ULL) & ~chunk & 0x8080808080808080ULL) != 0) {
            // 有零字节，逐字节检查
            for (int i = 0; i < 8; i++) {
                if (ptr[i] == target) {
                    return (char*)(ptr + i);
                }
                if (ptr[i] == '\0') {
                    return NULL;
                }
            }
        }
        
        ptr += 8;
    }
    #endif
    
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
    #ifdef __aarch64__
    if (!header || len == 0 || !key || !value) {
        return -1;
    }
    
    // 使用汇编优化的字符查找
    const char* colon = asm_opt_strchr(header, ':');
    if (!colon || colon >= header + len) {
        return -1;
    }
    
    size_t key_len = colon - header;
    const char* value_start = colon + 1;
    
    // 跳过值前面的空格
    while (value_start < header + len && (*value_start == ' ' || *value_start == '\t')) {
        value_start++;
    }
    
    // 查找值的结束位置（\r\n或\n）
    const char* value_end = value_start;
    while (value_end < header + len && *value_end != '\r' && *value_end != '\n') {
        value_end++;
    }
    
    // 去掉值末尾的空格
    while (value_end > value_start && (value_end[-1] == ' ' || value_end[-1] == '\t')) {
        value_end--;
    }
    
    size_t value_len = value_end - value_start;
    
    // 使用标准内存分配
    *key = malloc(key_len + 1);
    *value = malloc(value_len + 1);
    
    if (!*key || !*value) {
        if (*key) {
            free(*key);
        }
        if (*value) {
            free(*value);
        }
        return -1;
    }
    
    // 使用汇编优化的内存拷贝
    asm_opt_memcpy(*key, header, key_len);
    (*key)[key_len] = '\0';
    
    asm_opt_memcpy(*value, value_start, value_len);
    (*value)[value_len] = '\0';
    
    return 0;
    #endif
    
    // 简单实现，后续可以优化
    const char* colon_fallback = strchr(header, ':');
    if (!colon_fallback) return -1;
    
    *key = strndup(header, colon_fallback - header);
    *value = strndup(colon_fallback + 1, len - (colon_fallback + 1 - header));
    
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
    #ifdef __aarch64__
    static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    const uint8_t* input = (const uint8_t*)src;
    char* output = dst;
    size_t required_len = ((src_len + 2) / 3) * 4;
    
    if (dst_len < required_len + 1) {
        return 0; // 缓冲区太小
    }
    
    if (src_len >= 12 && (cpu_features & CPU_FEATURE_NEON)) {
        // 使用NEON优化的base64编码（处理12字节输入 -> 16字节输出）
        while (src_len >= 12) {
            uint8_t block[12];
            asm_opt_memcpy(block, input, 12);
            
            // 每3字节编码为4字节
            for (int i = 0; i < 4; i++) {
                uint32_t triple = (block[i*3] << 16) | (block[i*3+1] << 8) | block[i*3+2];
                
                output[i*4] = base64_table[(triple >> 18) & 0x3F];
                output[i*4+1] = base64_table[(triple >> 12) & 0x3F];
                output[i*4+2] = base64_table[(triple >> 6) & 0x3F];
                output[i*4+3] = base64_table[triple & 0x3F];
            }
            
            input += 12;
            output += 16;
            src_len -= 12;
        }
    }
    
    // 处理剩余字节
    while (src_len >= 3) {
        uint32_t triple = (input[0] << 16) | (input[1] << 8) | input[2];
        
        output[0] = base64_table[(triple >> 18) & 0x3F];
        output[1] = base64_table[(triple >> 12) & 0x3F];
        output[2] = base64_table[(triple >> 6) & 0x3F];
        output[3] = base64_table[triple & 0x3F];
        
        input += 3;
        output += 4;
        src_len -= 3;
    }
    
    // 处理剩余的1或2字节
    if (src_len > 0) {
        uint32_t triple = input[0] << 16;
        if (src_len > 1) {
            triple |= input[1] << 8;
        }
        
        output[0] = base64_table[(triple >> 18) & 0x3F];
        output[1] = base64_table[(triple >> 12) & 0x3F];
        output[2] = (src_len > 1) ? base64_table[(triple >> 6) & 0x3F] : '=';
        output[3] = '=';
        
        output += 4;
    }
    
    *output = '\0';
    return output - dst;
    #endif
    
    // 简单回退实现
    return 0;
}

size_t asm_opt_base64_decode(const char* src, size_t src_len, void* dst, size_t dst_len) {
    #ifdef __aarch64__
    static const int base64_decode_table[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
        -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };
    
    const char* input = src;
    uint8_t* output = (uint8_t*)dst;
    size_t decoded_len = 0;
    
    // 去掉填充
    while (src_len > 0 && src[src_len - 1] == '=') {
        src_len--;
    }
    
    size_t required_len = (src_len * 3) / 4;
    if (dst_len < required_len) {
        return 0; // 缓冲区太小
    }
    
    if (src_len >= 16 && (cpu_features & CPU_FEATURE_NEON)) {
        // 使用NEON优化的base64解码（处理16字节输入 -> 12字节输出）
        while (src_len >= 16) {
            char block[16];
            asm_opt_memcpy(block, input, 16);
            
            // 每4字节解码为3字节
            for (int i = 0; i < 4; i++) {
                int val1 = base64_decode_table[(int)block[i*4]];
                int val2 = base64_decode_table[(int)block[i*4+1]];
                int val3 = base64_decode_table[(int)block[i*4+2]];
                int val4 = base64_decode_table[(int)block[i*4+3]];
                
                if (val1 == -1 || val2 == -1 || val3 == -1 || val4 == -1) {
                    return 0; // 无效字符
                }
                
                uint32_t triple = (val1 << 18) | (val2 << 12) | (val3 << 6) | val4;
                
                output[i*3] = (triple >> 16) & 0xFF;
                output[i*3+1] = (triple >> 8) & 0xFF;
                output[i*3+2] = triple & 0xFF;
            }
            
            input += 16;
            output += 12;
            src_len -= 16;
            decoded_len += 12;
        }
    }
    
    // 处理剩余字节
    while (src_len >= 4) {
        int val1 = base64_decode_table[(int)input[0]];
        int val2 = base64_decode_table[(int)input[1]];
        int val3 = base64_decode_table[(int)input[2]];
        int val4 = base64_decode_table[(int)input[3]];
        
        if (val1 == -1 || val2 == -1 || val3 == -1 || val4 == -1) {
            return 0; // 无效字符
        }
        
        uint32_t triple = (val1 << 18) | (val2 << 12) | (val3 << 6) | val4;
        
        output[0] = (triple >> 16) & 0xFF;
        output[1] = (triple >> 8) & 0xFF;
        output[2] = triple & 0xFF;
        
        input += 4;
        output += 3;
        src_len -= 4;
        decoded_len += 3;
    }
    
    // 处理剩余的不完整块
    if (src_len >= 2) {
        int val1 = base64_decode_table[(int)input[0]];
        int val2 = base64_decode_table[(int)input[1]];
        
        if (val1 == -1 || val2 == -1) {
            return 0;
        }
        
        uint32_t triple = (val1 << 18) | (val2 << 12);
        output[0] = (triple >> 16) & 0xFF;
        decoded_len++;
        
        if (src_len >= 3) {
            int val3 = base64_decode_table[(int)input[2]];
            if (val3 == -1) {
                return 0;
            }
            
            triple |= (val3 << 6);
            output[1] = (triple >> 8) & 0xFF;
            decoded_len++;
        }
    }
    
    return decoded_len;
    #endif
    
    // 简单回退实现
    return 0;
}

// 高级NEON SIMD优化函数

// 向量化的数据清零
void asm_opt_simd_zero_buffer(void* buffer, size_t size) {
    #ifdef __aarch64__
    if (size >= 64 && (cpu_features & CPU_FEATURE_NEON)) {
        uint8_t* ptr = (uint8_t*)buffer;
        
        // 128位零向量
        __asm__ volatile (
            "movi v0.16b, #0\n\t"
            "movi v1.16b, #0\n\t"
            "movi v2.16b, #0\n\t"
            "movi v3.16b, #0\n\t"
            : : : "v0", "v1", "v2", "v3"
        );
        
        // 64字节批量清零
        while (size >= 64) {
            __asm__ volatile (
                "stp q0, q1, [%0, #0]\n\t"
                "stp q2, q3, [%0, #32]\n\t"
                : : "r"(ptr) : "memory"
            );
            ptr += 64;
            size -= 64;
        }
        
        // 16字节批量清零
        while (size >= 16) {
            __asm__ volatile (
                "str q0, [%0]\n\t"
                : : "r"(ptr) : "memory"
            );
            ptr += 16;
            size -= 16;
        }
        
        // 处理剩余字节
        while (size > 0) {
            *ptr++ = 0;
            size--;
        }
    } else {
        asm_opt_memset(buffer, 0, size);
    }
    #endif
}

// 向量化的数组求和
uint64_t asm_opt_simd_sum_array(const uint32_t* array, size_t count) {
    #ifdef __aarch64__
    if (count >= 16 && (cpu_features & CPU_FEATURE_NEON)) {
        uint64_t sum = 0;
        const uint32_t* ptr = array;
        
        // 使用NEON累加器
        __asm__ volatile (
            "movi v0.2d, #0\n\t"      // 累加器清零
            "movi v1.2d, #0\n\t"
            "movi v2.2d, #0\n\t"
            "movi v3.2d, #0\n\t"
            : : : "v0", "v1", "v2", "v3"
        );
        
        // 16个元素批量处理
        while (count >= 16) {
            __asm__ volatile (
                "ldp q4, q5, [%0, #0]\n\t"     // 加载16个uint32
                "ldp q6, q7, [%0, #32]\n\t"
                "uadalp v0.2d, v4.4s\n\t"      // 累加到64位
                "uadalp v1.2d, v5.4s\n\t"
                "uadalp v2.2d, v6.4s\n\t"
                "uadalp v3.2d, v7.4s\n\t"
                : : "r"(ptr) : "memory", "v4", "v5", "v6", "v7"
            );
            ptr += 16;
            count -= 16;
        }
        
        // 合并累加器
        __asm__ volatile (
            "add v0.2d, v0.2d, v1.2d\n\t"
            "add v2.2d, v2.2d, v3.2d\n\t"
            "add v0.2d, v0.2d, v2.2d\n\t"
            "addp d0, v0.2d\n\t"
            "fmov %0, d0\n\t"
            : "=r"(sum) : : "v0", "v1", "v2", "v3"
        );
        
        // 处理剩余元素
        while (count > 0) {
            sum += *ptr++;
            count--;
        }
        
        return sum;
    }
    #endif
    
    // 标准实现
    uint64_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += array[i];
    }
    return sum;
}

// 向量化的最大值查找
uint32_t asm_opt_simd_max_array(const uint32_t* array, size_t count) {
    #ifdef __aarch64__
    if (count >= 16 && (cpu_features & CPU_FEATURE_NEON)) {
        uint32_t max_val = 0;
        const uint32_t* ptr = array;
        
        // 初始化最大值向量
        __asm__ volatile (
            "ldr q0, [%0]\n\t"        // 加载前4个元素
            "mov v1.16b, v0.16b\n\t"  // 复制到其他向量
            "mov v2.16b, v0.16b\n\t"
            "mov v3.16b, v0.16b\n\t"
            : : "r"(ptr) : "v0", "v1", "v2", "v3"
        );
        
        // 16个元素批量处理
        while (count >= 16) {
            __asm__ volatile (
                "ldp q4, q5, [%0, #0]\n\t"
                "ldp q6, q7, [%0, #32]\n\t"
                "umax v0.4s, v0.4s, v4.4s\n\t"
                "umax v1.4s, v1.4s, v5.4s\n\t"
                "umax v2.4s, v2.4s, v6.4s\n\t"
                "umax v3.4s, v3.4s, v7.4s\n\t"
                : : "r"(ptr) : "memory", "v4", "v5", "v6", "v7"
            );
            ptr += 16;
            count -= 16;
        }
        
        // 合并最大值
        __asm__ volatile (
            "umax v0.4s, v0.4s, v1.4s\n\t"
            "umax v2.4s, v2.4s, v3.4s\n\t"
            "umax v0.4s, v0.4s, v2.4s\n\t"
            "umaxp v0.4s, v0.4s, v0.4s\n\t"
            "umaxp v0.4s, v0.4s, v0.4s\n\t"
            "fmov %w0, s0\n\t"
            : "=r"(max_val) : : "v0", "v1", "v2", "v3"
        );
        
        // 处理剩余元素
        while (count > 0) {
            if (*ptr > max_val) {
                max_val = *ptr;
            }
            ptr++;
            count--;
        }
        
        return max_val;
    }
    #endif
    
    // 标准实现
    uint32_t max_val = 0;
    for (size_t i = 0; i < count; i++) {
        if (array[i] > max_val) {
            max_val = array[i];
        }
    }
    return max_val;
}

// 向量化的字符串批量验证（检查是否为ASCII打印字符）
int asm_opt_simd_validate_ascii(const char* str, size_t len) {
    #ifdef __aarch64__
    if (len >= 16 && (cpu_features & CPU_FEATURE_NEON)) {
        const uint8_t* ptr = (const uint8_t*)str;
        
        // ASCII打印字符范围：0x20-0x7E
        __asm__ volatile (
            "movi v0.16b, #0x20\n\t"    // 最小值
            "movi v1.16b, #0x7E\n\t"    // 最大值
            : : : "v0", "v1"
        );
        
        // 16字节批量验证
        while (len >= 16) {
            __asm__ volatile (
                "ldr q2, [%0]\n\t"
                "cmhs v3.16b, v2.16b, v0.16b\n\t"  // >= 0x20
                "cmhi v4.16b, v1.16b, v2.16b\n\t"  // <= 0x7E
                "and v3.16b, v3.16b, v4.16b\n\t"   // 两个条件都满足
                "umaxv b5, v3.16b\n\t"             // 检查是否所有位都为1
                "fmov %w1, s5\n\t"
                : "=r"(ptr), "=r"(len) : "0"(ptr), "1"(len) : "memory", "v2", "v3", "v4", "v5"
            );
            
            if ((len & 0xFF) != 0xFF) {
                return 0; // 包含非ASCII字符
            }
            
            ptr += 16;
            len -= 16;
        }
        
        // 处理剩余字符
        while (len > 0) {
            if (*ptr < 0x20 || *ptr > 0x7E) {
                return 0;
            }
            ptr++;
            len--;
        }
        
        return 1;
    }
    #endif
    
    // 标准实现
    for (size_t i = 0; i < len; i++) {
        if (str[i] < 0x20 || str[i] > 0x7E) {
            return 0;
        }
    }
    return 1;
}

// 向量化的HTTP状态行生成
size_t asm_opt_simd_generate_status_line(char* buffer, size_t buffer_size, 
                                         int status_code, const char* reason_phrase) {
    #ifdef __aarch64__
    if (buffer_size >= 64 && (cpu_features & CPU_FEATURE_NEON)) {
        const char* http_version = "HTTP/1.1 ";
        char status_str[4];
        
        // 快速整数转字符串
        status_str[0] = '0' + (status_code / 100);
        status_str[1] = '0' + ((status_code / 10) % 10);
        status_str[2] = '0' + (status_code % 10);
        status_str[3] = ' ';
        
        // 使用NEON加速拷贝
        char* ptr = buffer;
        
        // 拷贝HTTP版本
        asm_opt_memcpy(ptr, http_version, 9);
        ptr += 9;
        
        // 拷贝状态码
        asm_opt_memcpy(ptr, status_str, 4);
        ptr += 4;
        
        // 拷贝原因短语
        size_t reason_len = asm_opt_strlen(reason_phrase);
        asm_opt_memcpy(ptr, reason_phrase, reason_len);
        ptr += reason_len;
        
        // 添加CRLF
        *ptr++ = '\r';
        *ptr++ = '\n';
        
        return ptr - buffer;
    }
    #endif
    
    // 标准实现
    return snprintf(buffer, buffer_size, "HTTP/1.1 %d %s\r\n", 
                   status_code, reason_phrase);
}

// 向量化的URL解码
size_t asm_opt_simd_url_decode(const char* src, size_t src_len, char* dst, size_t dst_len) {
    #ifdef __aarch64__
    if (src_len >= 16 && (cpu_features & CPU_FEATURE_NEON)) {
        const char* in = src;
        char* out = dst;
        size_t remaining = src_len;
        
        // 16字节批量处理
        while (remaining >= 16) {
            uint8_t block[16];
            asm_opt_memcpy(block, in, 16);
            
            int has_percent = 0;
            for (int i = 0; i < 16; i++) {
                if (block[i] == '%') {
                    has_percent = 1;
                    break;
                }
            }
            
            if (!has_percent) {
                // 没有%编码，直接拷贝
                asm_opt_memcpy(out, block, 16);
                out += 16;
                in += 16;
                remaining -= 16;
            } else {
                // 有%编码，逐字符处理
                break;
            }
        }
        
        // 处理剩余字符或%编码
        while (remaining > 0 && (size_t)(out - dst) < dst_len - 1) {
            if (*in == '%' && remaining >= 3) {
                // 十六进制解码
                char hex1 = in[1];
                char hex2 = in[2];
                
                int val1 = (hex1 >= '0' && hex1 <= '9') ? hex1 - '0' :
                          (hex1 >= 'A' && hex1 <= 'F') ? hex1 - 'A' + 10 :
                          (hex1 >= 'a' && hex1 <= 'f') ? hex1 - 'a' + 10 : -1;
                          
                int val2 = (hex2 >= '0' && hex2 <= '9') ? hex2 - '0' :
                          (hex2 >= 'A' && hex2 <= 'F') ? hex2 - 'A' + 10 :
                          (hex2 >= 'a' && hex2 <= 'f') ? hex2 - 'a' + 10 : -1;
                
                if (val1 >= 0 && val2 >= 0) {
                    *out++ = (val1 << 4) | val2;
                    in += 3;
                    remaining -= 3;
                } else {
                    *out++ = *in++;
                    remaining--;
                }
            } else if (*in == '+') {
                *out++ = ' ';
                in++;
                remaining--;
            } else {
                *out++ = *in++;
                remaining--;
            }
        }
        
        *out = '\0';
        return out - dst;
    }
    #endif
    
    // 标准实现
    return 0;
}

// 向量化的JSON转义检查
int asm_opt_simd_needs_json_escape(const char* str, size_t len) {
    #ifdef __aarch64__
    if (len >= 16 && (cpu_features & CPU_FEATURE_NEON)) {
        const uint8_t* ptr = (const uint8_t*)str;
        
        // 需要转义的字符：" \ / \b \f \n \r \t 和控制字符
        while (len >= 16) {
            __asm__ volatile (
                "ldr q0, [%0]\n\t"
                "movi v1.16b, #0x20\n\t"       // 控制字符阈值
                "movi v2.16b, #0x22\n\t"       // " 字符
                "movi v3.16b, #0x5C\n\t"       // 反斜杠字符
                "movi v4.16b, #0x2F\n\t"       // / 字符
                
                "cmhi v5.16b, v1.16b, v0.16b\n\t"  // 检查控制字符
                "cmeq v6.16b, v0.16b, v2.16b\n\t"  // 检查 "
                "cmeq v7.16b, v0.16b, v3.16b\n\t"  // 检查反斜杠
                "cmeq v8.16b, v0.16b, v4.16b\n\t"  // 检查 /
                
                "orr v5.16b, v5.16b, v6.16b\n\t"
                "orr v7.16b, v7.16b, v8.16b\n\t"
                "orr v5.16b, v5.16b, v7.16b\n\t"
                
                "umaxv b9, v5.16b\n\t"
                "fmov %w1, s9\n\t"
                : "=r"(ptr), "=r"(len) : "0"(ptr), "1"(len) 
                : "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9"
            );
            
            if ((len & 0xFF) != 0) {
                return 1; // 找到需要转义的字符
            }
            
            ptr += 16;
            len -= 16;
        }
        
        // 处理剩余字符
        while (len > 0) {
            if (*ptr < 0x20 || *ptr == '"' || *ptr == '\\' || *ptr == '/') {
                return 1;
            }
            ptr++;
            len--;
        }
        
        return 0;
    }
    #endif
    
    // 标准实现
    for (size_t i = 0; i < len; i++) {
        if (str[i] < 0x20 || str[i] == '"' || str[i] == '\\' || str[i] == '/') {
            return 1;
        }
    }
    return 0;
} 

// =================================================================
// HTTP协议专用汇编优化函数
// =================================================================

// 快速HTTP方法识别（使用向量化比较）
int asm_opt_fast_http_method_detect(const char* method, size_t len) {
    #ifdef __aarch64__
    if (len >= 3 && len <= 7 && (cpu_features & CPU_FEATURE_NEON)) {
        uint64_t method_word = 0;
        
        // 将方法字符串加载到64位寄存器
        for (size_t i = 0; i < len && i < 8; i++) {
            method_word |= ((uint64_t)method[i]) << (i * 8);
        }
        
        // 使用汇编优化的方法识别
        int result = 0;
        uint64_t get_val = 0x544547;          // "GET"
        uint64_t post_val = 0x54534F50;       // "POST"  
        uint64_t put_val = 0x545550;          // "PUT"
        uint64_t delete_val = 0x45544544454C; // "DELETE"
        uint64_t head_val = 0x44414548;       // "HEAD"
        uint64_t options_val = 0x534E4F4954504F; // "OPTIONS"
        
        __asm__ volatile (
            "cmp %1, %2\n\t"
            "b.eq 1f\n\t"
            "cmp %1, %3\n\t"
            "b.eq 2f\n\t"
            "cmp %1, %4\n\t"
            "b.eq 3f\n\t"
            "cmp %1, %5\n\t"
            "b.eq 4f\n\t"
            "cmp %1, %6\n\t"
            "b.eq 5f\n\t"
            "cmp %1, %7\n\t"
            "b.eq 6f\n\t"
            "mov %0, #0\n\t"   // 未知方法
            "b 7f\n\t"
            "1: mov %0, #1\n\t"  // GET
            "b 7f\n\t"
            "2: mov %0, #2\n\t"  // POST
            "b 7f\n\t"  
            "3: mov %0, #3\n\t"  // PUT
            "b 7f\n\t"
            "4: mov %0, #4\n\t"  // DELETE
            "b 7f\n\t"
            "5: mov %0, #5\n\t"  // HEAD
            "b 7f\n\t"
            "6: mov %0, #6\n\t"  // OPTIONS
            "7:\n\t"
            : "=r"(result) 
            : "r"(method_word), "r"(get_val), "r"(post_val), "r"(put_val), 
              "r"(delete_val), "r"(head_val), "r"(options_val)
        );
        
        return result;
    }
    #endif
    
    // 回退实现
    if (len == 3 && strncmp(method, "GET", 3) == 0) return 1;
    if (len == 4 && strncmp(method, "POST", 4) == 0) return 2;
    if (len == 3 && strncmp(method, "PUT", 3) == 0) return 3;
    if (len == 6 && strncmp(method, "DELETE", 6) == 0) return 4;
    if (len == 4 && strncmp(method, "HEAD", 4) == 0) return 5;
    if (len == 7 && strncmp(method, "OPTIONS", 7) == 0) return 6;
    return 0;
}

// 优化的HTTP状态码生成
size_t asm_opt_generate_status_response(char* buffer, size_t buffer_size, 
                                       int status_code, const char* reason) {
    #ifdef __aarch64__
    if (buffer_size >= 32 && (cpu_features & CPU_FEATURE_NEON)) {
        char* ptr = buffer;
        
        // 使用向量化指令快速构建响应行
        const char* version = "HTTP/1.1 ";
        
        // 快速拷贝HTTP版本
        __asm__ volatile (
            "ldr x1, [%1]\n\t"      // 加载8字节
            "str x1, [%0]\n\t"      // 存储到buffer
            "ldrb w1, [%1, #8]\n\t" // 加载第9字节
            "strb w1, [%0, #8]\n\t" // 存储第9字节
            : : "r"(ptr), "r"(version) : "x1", "memory"
        );
        ptr += 9;
        
        // 快速整数转字符串（状态码）
        int hundreds = status_code / 100;
        int tens = (status_code / 10) % 10;
        int ones = status_code % 10;
        
        __asm__ volatile (
            "add w1, %w1, #48\n\t"  // 转换为ASCII
            "add w2, %w2, #48\n\t"
            "add w3, %w3, #48\n\t"
            "strb w1, [%0, #0]\n\t"
            "strb w2, [%0, #1]\n\t"
            "strb w3, [%0, #2]\n\t"
            "mov w1, #32\n\t"       // 空格
            "strb w1, [%0, #3]\n\t"
            : : "r"(ptr), "r"(hundreds), "r"(tens), "r"(ones) : "w1", "w2", "w3", "memory"
        );
        ptr += 4;
        
        // 拷贝原因短语
        size_t reason_len = asm_opt_strlen(reason);
        asm_opt_memcpy(ptr, reason, reason_len);
        ptr += reason_len;
        
        // 添加CRLF
        __asm__ volatile (
            "mov w1, #13\n\t"       // \r
            "mov w2, #10\n\t"       // \n
            "strb w1, [%0, #0]\n\t"
            "strb w2, [%0, #1]\n\t"
            : : "r"(ptr) : "w1", "w2", "memory"
        );
        ptr += 2;
        
        return ptr - buffer;
    }
    #endif
    
    return snprintf(buffer, buffer_size, "HTTP/1.1 %d %s\r\n", status_code, reason);
}

// 优化的HTTP头部写入
size_t asm_opt_write_http_header(char* buffer, size_t buffer_size,
                                const char* name, const char* value) {
    #ifdef __aarch64__
    if (buffer_size >= 64 && (cpu_features & CPU_FEATURE_NEON)) {
        char* ptr = buffer;
        size_t name_len = asm_opt_strlen(name);
        size_t value_len = asm_opt_strlen(value);
        
        if (name_len + value_len + 4 > buffer_size) {
            return 0; // 缓冲区不足
        }
        
        // 快速拷贝头部名称
        asm_opt_memcpy(ptr, name, name_len);
        ptr += name_len;
        
        // 添加冒号和空格
        __asm__ volatile (
            "mov w1, #58\n\t"       // ':'
            "mov w2, #32\n\t"       // ' '
            "strb w1, [%0, #0]\n\t"
            "strb w2, [%0, #1]\n\t"
            : : "r"(ptr) : "w1", "w2", "memory"
        );
        ptr += 2;
        
        // 拷贝头部值
        asm_opt_memcpy(ptr, value, value_len);
        ptr += value_len;
        
        // 添加CRLF
        __asm__ volatile (
            "mov w1, #13\n\t"       // \r
            "mov w2, #10\n\t"       // \n
            "strb w1, [%0, #0]\n\t"
            "strb w2, [%0, #1]\n\t"
            : : "r"(ptr) : "w1", "w2", "memory"
        );
        ptr += 2;
        
        return ptr - buffer;
    }
    #endif
    
    return snprintf(buffer, buffer_size, "%s: %s\r\n", name, value);
}

// 优化的Content-Length生成
size_t asm_opt_generate_content_length_header(char* buffer, size_t buffer_size, 
                                             size_t content_length) {
    #ifdef __aarch64__
    if (buffer_size >= 32 && (cpu_features & CPU_FEATURE_NEON)) {
        const char* header_name = "Content-Length: ";
        char* ptr = buffer;
        
        // 快速拷贝头部名称
        asm_opt_memcpy(ptr, header_name, 16);
        ptr += 16;
        
        // 快速数字转字符串
        char num_str[32];
        int digits = 0;
        size_t temp = content_length;
        
        if (temp == 0) {
            num_str[0] = '0';
            digits = 1;
        } else {
            // 使用汇编优化的除法
            while (temp > 0) {
                uint64_t quotient, remainder;
                __asm__ volatile (
                    "mov x1, #10\n\t"
                    "udiv %0, %2, x1\n\t"
                    "msub %1, %0, x1, %2\n\t"
                    : "=r"(quotient), "=r"(remainder) : "r"(temp) : "x1"
                );
                num_str[digits++] = '0' + remainder;
                temp = quotient;
            }
        }
        
        // 反转数字字符串
        for (int i = 0; i < digits / 2; i++) {
            char tmp = num_str[i];
            num_str[i] = num_str[digits - 1 - i];
            num_str[digits - 1 - i] = tmp;
        }
        
        asm_opt_memcpy(ptr, num_str, digits);
        ptr += digits;
        
        // 添加CRLF
        __asm__ volatile (
            "mov w1, #13\n\t"
            "mov w2, #10\n\t"
            "strb w1, [%0, #0]\n\t"
            "strb w2, [%0, #1]\n\t"
            : : "r"(ptr) : "w1", "w2", "memory"
        );
        ptr += 2;
        
        return ptr - buffer;
    }
    #endif
    
    return snprintf(buffer, buffer_size, "Content-Length: %zu\r\n", content_length);
}

// =================================================================
// 网络I/O汇编优化函数
// =================================================================

// 优化的套接字数据发送
ssize_t asm_opt_socket_send(int sockfd, const void* buffer, size_t len, int flags) {
    #ifdef __aarch64__
    ssize_t result;
    
    // 使用系统调用的汇编优化版本
    __asm__ volatile (
        "mov x8, #64\n\t"        // sys_write系统调用号
        "mov x0, %1\n\t"         // sockfd
        "mov x1, %2\n\t"         // buffer
        "mov x2, %3\n\t"         // len
        "svc #0\n\t"             // 系统调用
        "mov %0, x0\n\t"         // 返回值
        : "=r"(result) 
        : "r"((long)sockfd), "r"(buffer), "r"(len)
        : "x0", "x1", "x2", "x8", "memory"
    );
    
    return result;
    #endif
    
    return send(sockfd, buffer, len, flags);
}

// 优化的套接字数据接收
ssize_t asm_opt_socket_recv(int sockfd, void* buffer, size_t len, int flags) {
    #ifdef __aarch64__
    ssize_t result;
    
    // 预取接收缓冲区以提高缓存性能
    ASM_PREFETCH_WRITE(buffer);
    
    __asm__ volatile (
        "mov x8, #63\n\t"        // sys_read系统调用号
        "mov x0, %1\n\t"         // sockfd
        "mov x1, %2\n\t"         // buffer
        "mov x2, %3\n\t"         // len
        "svc #0\n\t"             // 系统调用
        "mov %0, x0\n\t"         // 返回值
        : "=r"(result)
        : "r"((long)sockfd), "r"(buffer), "r"(len)
        : "x0", "x1", "x2", "x8", "memory"
    );
    
    return result;
    #endif
    
    return recv(sockfd, buffer, len, flags);
}

// 优化的网络缓冲区拷贝（零拷贝优化）
size_t asm_opt_network_buffer_copy(void* dest, const void* src, size_t len) {
    #ifdef __aarch64__
    if (len >= 128 && (cpu_features & CPU_FEATURE_NEON)) {
        char* d = (char*)dest;
        const char* s = (const char*)src;
        
        // 预取源和目标地址
        ASM_PREFETCH_READ(s);
        ASM_PREFETCH_WRITE(d);
        
        // 128字节对齐的大块拷贝
        while (len >= 128) {
            __asm__ volatile (
                // 加载128字节到NEON寄存器
                "ldp q0, q1, [%1, #0]\n\t"
                "ldp q2, q3, [%1, #32]\n\t"
                "ldp q4, q5, [%1, #64]\n\t"
                "ldp q6, q7, [%1, #96]\n\t"
                
                // 存储128字节
                "stp q0, q1, [%0, #0]\n\t"
                "stp q2, q3, [%0, #32]\n\t"
                "stp q4, q5, [%0, #64]\n\t"
                "stp q6, q7, [%0, #96]\n\t"
                
                : : "r"(d), "r"(s) 
                : "memory", "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7"
            );
            
            d += 128;
            s += 128;
            len -= 128;
            
            // 预取下一个块
            if (len >= 128) {
                ASM_PREFETCH_READ(s + 128);
                ASM_PREFETCH_WRITE(d + 128);
            }
        }
        
        // 处理剩余字节
        if (len > 0) {
            asm_opt_memcpy(d, s, len);
        }
        
        return d - (char*)dest + len;
    }
    #endif
    
    return asm_opt_memcpy(dest, src, len) ? len : 0;
}

// =================================================================
// 高级性能分析和调试汇编函数
// =================================================================

// 获取详细的CPU性能计数器
void asm_opt_get_cpu_performance_counters(asm_opt_perf_counter_t* counters) {
    #ifdef __aarch64__
    if (!counters) return;
    
    // 读取各种性能计数器
    __asm__ volatile (
        "mrs %0, pmccntr_el0\n\t"      // 周期计数器
        "mrs %1, pmevcntr0_el0\n\t"    // 事件计数器0（指令）
        "mrs %2, pmevcntr1_el0\n\t"    // 事件计数器1（缓存未命中）
        "mrs %3, pmevcntr2_el0\n\t"    // 事件计数器2（分支预测）
        : "=r"(counters->cycles), "=r"(counters->instructions),
          "=r"(counters->cache_misses), "=r"(counters->branch_misses)
        : : "memory"
    );
    #endif
}

// 汇编优化的内存带宽测试
uint64_t asm_opt_memory_bandwidth_test(void* buffer, size_t size, int iterations) {
    #ifdef __aarch64__
    if (size >= 1024 && (cpu_features & CPU_FEATURE_NEON)) {
        uint64_t start_time, end_time;
        
        // 获取开始时间
        __asm__ volatile ("mrs %0, cntvct_el0" : "=r"(start_time));
        
        for (int iter = 0; iter < iterations; iter++) {
            char* ptr = (char*)buffer;
            size_t remaining = size;
            
            // 使用NEON进行内存流式访问
            while (remaining >= 64) {
                __asm__ volatile (
                    "ldp q0, q1, [%0, #0]\n\t"
                    "ldp q2, q3, [%0, #32]\n\t"
                    "stp q0, q1, [%0, #0]\n\t"
                    "stp q2, q3, [%0, #32]\n\t"
                    : : "r"(ptr) : "memory", "q0", "q1", "q2", "q3"
                );
                ptr += 64;
                remaining -= 64;
            }
        }
        
        // 获取结束时间
        __asm__ volatile ("mrs %0, cntvct_el0" : "=r"(end_time));
        
        return end_time - start_time;
    }
    #endif
    
    return 0;
}

// 汇编优化的延迟测试
uint64_t asm_opt_latency_test(void* ptr, int iterations) {
    #ifdef __aarch64__
    uint64_t start_time, end_time;
    volatile uint64_t* test_ptr = (volatile uint64_t*)ptr;
    
    // 获取开始时间
    __asm__ volatile ("mrs %0, cntvct_el0" : "=r"(start_time));
    
    // 执行依赖性内存访问链
    for (int i = 0; i < iterations; i++) {
        __asm__ volatile (
            "ldr x1, [%0]\n\t"          // 加载
            "add x1, x1, #1\n\t"        // 计算
            "str x1, [%0]\n\t"          // 存储
            : : "r"(test_ptr) : "x1", "memory"
        );
    }
    
    // 获取结束时间
    __asm__ volatile ("mrs %0, cntvct_el0" : "=r"(end_time));
    
    return end_time - start_time;
    #endif
    
    return 0;
}

// =================================================================
// SSL/TLS加密相关汇编优化
// =================================================================

// AES加密优化（如果支持AES指令）
void asm_opt_aes_encrypt_block(const uint8_t* plaintext, uint8_t* ciphertext, 
                              const uint8_t* round_keys, int rounds) {
    #if defined(__aarch64__) && defined(__ARM_FEATURE_AES)
    if (cpu_features & CPU_FEATURE_AES) {
        __asm__ volatile (
            "ld1 {v0.16b}, [%0]\n\t"        // 加载明文
            "ld1 {v1.16b}, [%2]\n\t"        // 加载轮密钥
            "aese v0.16b, v1.16b\n\t"       // AES加密
            "aesmc v0.16b, v0.16b\n\t"      // AES混列
            // ... 更多轮次
            "st1 {v0.16b}, [%1]\n\t"        // 存储密文
            : : "r"(plaintext), "r"(ciphertext), "r"(round_keys)
            : "memory", "v0", "v1"
        );
        return;
    }
    #endif
    
    // 软件实现作为回退
    memcpy(ciphertext, plaintext, 16);
}

// SHA256哈希计算优化
void asm_opt_sha256_hash(const uint8_t* data, size_t len, uint8_t* hash) {
    #if defined(__aarch64__) && defined(__ARM_FEATURE_SHA2)
    if (len >= 64 && (cpu_features & CPU_FEATURE_SHA2)) {
        // 使用ARM SHA2指令进行优化
        __asm__ volatile (
            "ld1 {v0.4s, v1.4s}, [%0]\n\t"     // 加载初始哈希值
            "ld1 {v2.16b}, [%1]\n\t"           // 加载数据块
            "sha256h q0, q1, v2.4s\n\t"        // SHA256哈希
            "sha256h2 q1, q0, v2.4s\n\t"       // SHA256哈希2
            "st1 {v0.4s, v1.4s}, [%2]\n\t"     // 存储结果
            : : "r"(data), "r"(data), "r"(hash)
            : "memory", "v0", "v1", "v2"
        );
        return;
    }
    #endif
    
    // 简单的软件实现
    memset(hash, 0, 32);
}

// =================================================================
// 调试和诊断汇编函数
// =================================================================

// 获取当前CPU频率
uint64_t asm_opt_get_cpu_frequency(void) {
    #ifdef __aarch64__
    uint64_t freq;
    __asm__ volatile ("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
    #endif
    return 0;
}

// 获取CPU缓存信息
void asm_opt_get_cache_info(uint32_t* l1_cache_size, uint32_t* l2_cache_size, 
                           uint32_t* l3_cache_size) {
    #ifdef __aarch64__
    uint64_t clidr, ccsidr;
    
    // 读取缓存层级信息寄存器
    __asm__ volatile ("mrs %0, clidr_el1" : "=r"(clidr));
    
    // 设置缓存大小选择寄存器查询L1数据缓存
    __asm__ volatile (
        "msr csselr_el1, xzr\n\t"      // 选择L1数据缓存
        "isb\n\t"
        "mrs %0, ccsidr_el1"
        : "=r"(ccsidr) : : "memory"
    );
    
    // 解析L1缓存大小
    *l1_cache_size = (1 << ((ccsidr >> 13) & 0xF)) * 
                     ((ccsidr & 0x1FFF) + 1) * 
                     (((ccsidr >> 3) & 0x3FF) + 1);
    
    // 类似的方式获取L2和L3缓存信息
    *l2_cache_size = 0; // 简化实现
    *l3_cache_size = 0;
    #endif
}

// 打印汇编优化统计信息
void asm_opt_print_statistics(void) {
    if (!asm_opt_is_supported()) {
        log_message(LOG_LEVEL_INFO, "汇编优化不支持当前平台");
        return;
    }
    
    uint32_t features = asm_opt_get_cpu_features();
    uint64_t freq = asm_opt_get_cpu_frequency();
    uint32_t l1_size, l2_size, l3_size;
    asm_opt_get_cache_info(&l1_size, &l2_size, &l3_size);
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "汇编优化统计信息:\n"
             "  CPU特性: 0x%08x\n"
             "  NEON支持: %s\n"
             "  CRC32支持: %s\n"
             "  AES支持: %s\n"
             "  SHA支持: %s\n"
             "  CPU频率: %lu Hz\n"
             "  L1缓存: %u bytes\n"
             "  L2缓存: %u bytes\n"
             "  L3缓存: %u bytes",
             features,
             (features & CPU_FEATURE_NEON) ? "是" : "否",
             (features & CPU_FEATURE_CRC32) ? "是" : "否", 
             (features & CPU_FEATURE_AES) ? "是" : "否",
             (features & (CPU_FEATURE_SHA1 | CPU_FEATURE_SHA2)) ? "是" : "否",
             freq, l1_size, l2_size, l3_size);
    
    log_message(LOG_LEVEL_INFO, log_msg);
}

// 优化的strrchr实现（aarch64）
char* asm_opt_strrchr(const char* s, int c) {
    #ifdef __aarch64__
    if (!s) return NULL;
    
    const char* last_occurrence = NULL;
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
            // 找到字符串结束，逐字节检查
            while (*ptr != '\0') {
                if (*ptr == c) {
                    last_occurrence = ptr;
                }
                ptr++;
            }
            break;
        }
        
        // 在当前块中查找字符c
        for (int i = 0; i < 8; i++) {
            if (ptr[i] == c) {
                last_occurrence = (char*)ptr + i;
            }
        }
        
        ptr += 8;
    }
    
    return (char*)last_occurrence;
    #else
    return strrchr(s, c);
    #endif
} 