#ifndef ANX_VERSION_H
#define ANX_VERSION_H

#define ANX_VERSION_MAJOR 0
#define ANX_VERSION_MINOR 8
#define ANX_VERSION_PATCH 0
#define ANX_VERSION_BUILD "development"

#define ANX_VERSION_STRING "0.8.0-development"
#define ANX_NAME "ANX HTTP Server"
#define ANX_AUTHOR "neipor"
#define ANX_EMAIL "neitherportal@proton.me"
#define ANX_DESCRIPTION "High-performance HTTP server with assembly optimizations"

#define ANX_BUILD_DATE __DATE__
#define ANX_BUILD_TIME __TIME__

// 功能特性宏
#define ANX_FEATURE_SSL 1
#define ANX_FEATURE_HTTP2 0
#define ANX_FEATURE_WEBSOCKET 0
#define ANX_FEATURE_COMPRESSION 1
#define ANX_FEATURE_CACHE 1
#define ANX_FEATURE_LOAD_BALANCER 1
#define ANX_FEATURE_HEALTH_CHECK 1
#define ANX_FEATURE_STREAM 1
#define ANX_FEATURE_PUSH 1
#define ANX_FEATURE_ASM_OPT 1

// 平台检测
#ifdef __aarch64__
#define ANX_ARCH_ARM64 1
#define ANX_ARCH_STRING "aarch64"
#elif defined(__x86_64__)
#define ANX_ARCH_X86_64 1
#define ANX_ARCH_STRING "x86_64"
#else
#define ANX_ARCH_UNKNOWN 1
#define ANX_ARCH_STRING "unknown"
#endif

// 函数声明
const char* anx_get_version(void);
const char* anx_get_build_info(void);
void anx_print_version(void);

#endif // ANX_VERSION_H 