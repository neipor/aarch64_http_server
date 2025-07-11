# ASM HTTP Server v1.1.0+

基于C语言和Rust混合架构的高性能HTTP服务器，v1.1.0+版本新增了完整的Rust模块支持和FFI集成。

## 🚀 新特性

### Rust模块集成
- **类型安全配置系统**: 基于Rust的配置解析器，支持TOML和Nginx兼容配置
- **高性能HTTP解析器**: 完整的HTTP请求/响应解析和构建功能
- **智能缓存系统**: 支持LRU、LFU、FIFO缓存策略，内置ETag和条件请求支持
- **FFI接口**: 完整的C/Rust互操作接口，支持从C代码调用Rust模块

### 架构改进
- **混合架构**: C语言核心 + Rust模块，兼顾性能和安全性
- **多线程编译**: 自动检测CPU核心数，使用双倍线程数进行并行编译
- **模块化设计**: 清晰的模块边界和接口定义

## 📦 模块结构

```
src/
├── rust_modules/          # Rust模块
│   ├── config/           # 配置系统
│   ├── http_parser/      # HTTP解析器
│   ├── cache/            # 缓存系统
│   └── ffi/              # FFI接口
├── core/                 # C语言核心
├── http/                 # HTTP处理
├── proxy/                # 代理功能
├── stream/               # 流处理
└── utils/                # 工具模块
```

## 🔧 编译和安装

### 系统要求
- Linux (推荐 Ubuntu 20.04+)
- GCC 9.0+
- Rust 1.70+
- Make

### 编译步骤

1. **克隆仓库**
   ```bash
   git clone <repository-url>
   cd asm_http_server
   ```

2. **编译Rust模块**
   ```bash
   cargo build
   ```

3. **编译C代码**
   ```bash
   make -j$(nproc)
   ```

4. **运行测试**
   ```bash
   # Rust模块测试
   cargo test
   
   # FFI接口测试
   cd examples && make run
   ```

## 📚 使用示例

### FFI接口使用

```c
#include "anx_rust.h"

// HTTP解析器
HttpRequestHandle* request = anx_http_parse_request(data, len);
char* method = anx_http_get_method(request);
char* uri = anx_http_get_uri(request);

// 缓存操作
CacheHandle* cache = anx_cache_new();
anx_cache_put(cache, "key", data, len, "text/plain");
CacheResponseC* response = anx_cache_get(cache, "key");

// 配置加载
ConfigHandle* config = anx_config_load_toml("config.toml");
char* listen = anx_config_get_listen(config, 0);
```

### Rust模块使用

```rust
use anx_core::http_parser::{HttpRequest, HttpResponse};
use anx_core::cache::{Cache, CacheConfig};
use anx_core::config::AnxConfig;

// HTTP解析
let request = HttpRequest::parse(raw_data)?;
println!("Method: {}", request.method);

// 缓存操作
let cache = Cache::new();
cache.put("key".to_string(), data, Some("text/plain".to_string()))?;
let response = cache.get("key")?;

// 配置加载
let config = AnxConfig::from_toml_file("config.toml")?;
```

## 🧪 测试状态

### ✅ 已完成
- HTTP解析器模块 (Rust) - 完整功能
- 缓存模块 (Rust) - 基本功能
- FFI接口 - HTTP解析器和缓存模块
- 多线程编译支持
- 配置系统基础功能

### 🔄 进行中
- 缓存模块高级测试
- 配置系统FFI接口调试
- 集成测试开发
- 文档完善

### 📋 计划中
- 性能基准测试
- 内存泄漏检测
- 并发压力测试
- 端到端集成测试

## 🏗️ 架构设计

### 混合架构优势
1. **性能**: C语言核心提供高性能基础
2. **安全性**: Rust模块提供内存安全和类型安全
3. **可维护性**: 模块化设计，清晰的接口边界
4. **扩展性**: FFI接口支持灵活的模块组合

### 模块职责
- **C语言核心**: 网络I/O、事件循环、基础HTTP处理
- **Rust配置模块**: 类型安全配置解析和管理
- **Rust HTTP解析器**: 高性能HTTP协议解析
- **Rust缓存模块**: 智能缓存策略和HTTP缓存支持
- **FFI接口**: C/Rust互操作桥梁

## 📈 性能特性

- **多线程编译**: 自动使用CPU核心数的2倍线程
- **零拷贝优化**: 减少内存分配和拷贝
- **智能缓存**: 支持多种缓存策略和HTTP缓存标准
- **类型安全**: Rust模块提供编译时错误检查

## 🔍 调试和监控

### 日志系统
- 结构化日志输出
- 可配置日志级别
- 性能指标记录

### 缓存统计
- 命中率统计
- 内存使用监控
- 驱逐策略分析

## 📄 许可证

本项目采用 [许可证名称] 许可证。

## 🤝 贡献

欢迎提交Issue和Pull Request！

### 开发环境设置
1. 安装Rust工具链
2. 安装C开发工具
3. 运行测试套件
4. 遵循代码规范

## 📞 联系方式

- 项目主页: [项目URL]
- 问题反馈: [Issues链接]
- 文档: [文档链接]

---

**ASM HTTP Server v1.1.0+** - 高性能混合架构HTTP服务器 