#ifndef ANX_H
#define ANX_H

// 包含通用定义和版本信息
#include "common.h"
#include "version.h"

// 核心模块头文件
#include "../core/core.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../core/net.h"

// HTTP模块头文件
#include "../http/http.h"
#include "../http/https.h"
#include "../http/headers.h"
#include "../http/chunked.h"

// 代理模块头文件
#include "../proxy/proxy.h"
#include "../proxy/lb_proxy.h"
#include "../proxy/load_balancer.h"
#include "../proxy/health_check.h"
#include "../proxy/health_api.h"

// 流媒体模块头文件
#include "../stream/stream.h"
#include "../stream/push.h"

// 工具模块头文件
#include "../utils/util.h"
#include "../utils/cache.h"
#include "../utils/compress.h"
#include "../utils/bandwidth.h"

// 汇编优化模块（条件编译）
#ifdef ANX_FEATURE_ASM_OPT
#include "../utils/asm/asm_core.h"
#include "../utils/asm/asm_opt.h"
#include "../utils/asm/asm_mempool.h"
#include "../utils/asm/asm_integration.h"
#endif

// 主要API函数声明

/**
 * 初始化ANX服务器
 * @param config_file 配置文件路径
 * @return 成功返回ANX_OK，失败返回错误码
 */
anx_int_t anx_init(const char *config_file);

/**
 * 启动ANX服务器
 * @return 成功返回ANX_OK，失败返回错误码
 */
anx_int_t anx_start(void);

/**
 * 停止ANX服务器
 * @return 成功返回ANX_OK，失败返回错误码
 */
anx_int_t anx_stop(void);

/**
 * 重载ANX服务器配置
 * @param config_file 新的配置文件路径
 * @return 成功返回ANX_OK，失败返回错误码
 */
anx_int_t anx_reload(const char *config_file);

/**
 * 清理ANX服务器资源
 */
void anx_cleanup(void);

/**
 * 获取服务器状态信息
 * @return 状态信息字符串
 */
const char* anx_get_status(void);

#endif // ANX_H 