#ifndef ANX_COMMON_H
#define ANX_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

// 通用常量定义
#define ANX_MAX_PATH_LENGTH 4096
#define ANX_MAX_HOST_LENGTH 256
#define ANX_MAX_BUFFER_SIZE 65536
#define ANX_MAX_HEADER_SIZE 8192
#define ANX_MAX_URI_LENGTH 2048
#define ANX_MAX_WORKERS 128
#define ANX_MAX_CONNECTIONS 10000

// 错误代码定义
typedef enum {
    ANX_OK = 0,
    ANX_ERROR = -1,
    ANX_AGAIN = -2,
    ANX_BUSY = -3,
    ANX_DONE = -4,
    ANX_DECLINED = -5,
    ANX_ABORT = -6
} anx_int_t;

// 常用宏定义
#define ANX_UNUSED(x) ((void)(x))
#define ANX_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ANX_MIN(a, b) ((a) < (b) ? (a) : (b))
#define ANX_MAX(a, b) ((a) > (b) ? (a) : (b))
#define ANX_CLAMP(val, min, max) ANX_MAX(min, ANX_MIN(max, val))

// 内存对齐宏
#define ANX_ALIGN(size, alignment) (((size) + (alignment) - 1) & ~((alignment) - 1))
#define ANX_ALIGN_PTR(ptr, alignment) \
    ((void *)(((uintptr_t)(ptr) + (alignment) - 1) & ~((alignment) - 1)))

// 字符串操作宏
#define ANX_STR_EQUAL(s1, s2) (strcmp((s1), (s2)) == 0)
#define ANX_STR_EMPTY(s) ((s) == NULL || *(s) == '\0')

// 日志级别定义
typedef enum {
    ANX_LOG_EMERG = 0,
    ANX_LOG_ALERT = 1,
    ANX_LOG_CRIT = 2,
    ANX_LOG_ERR = 3,
    ANX_LOG_WARN = 4,
    ANX_LOG_NOTICE = 5,
    ANX_LOG_INFO = 6,
    ANX_LOG_DEBUG = 7
} anx_log_level_t;

// 时间相关宏
#define ANX_SEC_TO_MSEC(sec) ((sec) * 1000)
#define ANX_MSEC_TO_SEC(msec) ((msec) / 1000)
#define ANX_USEC_TO_MSEC(usec) ((usec) / 1000)

// 网络相关常量
#define ANX_INVALID_SOCKET -1
#define ANX_LISTEN_BACKLOG 511

// 编译器属性宏
#ifdef __GNUC__
#define ANX_LIKELY(x) __builtin_expect(!!(x), 1)
#define ANX_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ANX_INLINE __inline__
#define ANX_FORCE_INLINE __attribute__((always_inline)) inline
#define ANX_NO_INLINE __attribute__((noinline))
#define ANX_PACKED __attribute__((packed))
#define ANX_ALIGNED(n) __attribute__((aligned(n)))
#else
#define ANX_LIKELY(x) (x)
#define ANX_UNLIKELY(x) (x)
#define ANX_INLINE inline
#define ANX_FORCE_INLINE inline
#define ANX_NO_INLINE
#define ANX_PACKED
#define ANX_ALIGNED(n)
#endif

// 调试宏
#ifdef DEBUG
#define ANX_DEBUG_PRINT(fmt, ...) \
    fprintf(stderr, "[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define ANX_DEBUG_PRINT(fmt, ...) ((void)0)
#endif

#endif // ANX_COMMON_H 