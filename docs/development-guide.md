# ANX HTTP Server 开发指南

## 目录
1. [简介](#简介)
2. [开发环境搭建](#开发环境搭建)
3. [项目结构](#项目结构)
4. [构建系统](#构建系统)
5. [编码规范](#编码规范)
6. [测试](#测试)
7. [性能优化](#性能优化)
8. [调试](#调试)
9. [贡献代码](#贡献代码)

## 简介

本指南旨在帮助开发者了解ANX HTTP Server的开发流程、项目结构和编码规范。ANX是一个基于C语言和Rust的混合架构Web服务器，具有高性能和可扩展性。

## 开发环境搭建

### 系统要求

- **操作系统**: Linux (推荐Ubuntu 20.04+)
- **编译器**: GCC 9.0+ 和 Rust 1.75+
- **构建工具**: Make, CMake (可选)
- **依赖库**: OpenSSL 1.1.1+, Zlib

### 安装依赖

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y build-essential gcc rustc cargo libssl-dev zlib1g-dev clang-format cppcheck
```

#### 其他系统
请根据您的系统安装相应的包管理器和依赖。

### 克隆仓库
```bash
git clone https://github.com/neipor/asm_http_server.git
cd asm_http_server
```

## 项目结构

```
.
├── src/                    # 源代码目录
│   ├── core/              # 核心模块
│   ├── http/              # HTTP处理模块
│   ├── proxy/             # 代理模块
│   ├── stream/            # 流媒体模块
│   ├── utils/             # 工具模块
│   ├── rust_modules/      # Rust模块
│   └── include/           # 头文件
├── configs/               # 配置文件示例
├── examples/              # 示例代码
├── scripts/               # 脚本文件
├── docs/                  # 文档
├── tests/                 # 测试文件
├── www/                   # 静态文件目录
└── Makefile               # 构建文件
```

### 主要模块说明

1. **core/**: 核心模块，包含主事件循环、配置解析、网络I/O等
2. **http/**: HTTP协议处理模块
3. **proxy/**: 反向代理和负载均衡模块
4. **stream/**: 流媒体处理模块
5. **utils/**: 工具模块，包括缓存、压缩、日志等
6. **rust_modules/**: Rust实现的模块，如配置解析、HTTP解析、缓存等

## 构建系统

### 使用Makefile构建

#### 构建调试版本
```bash
make
```

#### 构建发布版本
```bash
make CFLAGS="-O3 -march=native -DNDEBUG"
```

#### 清理构建文件
```bash
make clean
```

#### 运行测试
```bash
make test
```

### 使用构建脚本

项目提供了统一的构建脚本`scripts/build.sh`：

```bash
# 构建发布版本
./scripts/build.sh release

# 构建调试版本（详细输出）
./scripts/build.sh -v debug

# 4线程构建发布版本，构建前格式化代码
./scripts/build.sh -j4 -f release

# 运行测试
./scripts/build.sh test
```

### 构建选项

| 选项 | 说明 |
|------|------|
| `debug` | 构建调试版本 (默认) |
| `release` | 构建发布版本 |
| `profile` | 构建性能分析版本 |
| `clean` | 清理构建文件 |
| `test` | 运行测试 |
| `package` | 打包发布 |
| `install` | 安装到系统 |
| `uninstall` | 从系统卸载 |

## 编码规范

### C语言编码规范

1. **命名规范**
   - 函数名: `snake_case`
   - 变量名: `snake_case`
   - 常量名: `UPPER_SNAKE_CASE`
   - 结构体名: `snake_case_t`
   - 枚举名: `snake_case_t`

2. **注释规范**
   - 使用Doxygen风格注释
   - 函数注释应包含参数说明和返回值说明

3. **代码格式**
   - 使用4个空格缩进
   - 每行不超过100个字符
   - 使用`clang-format`格式化代码

### Rust编码规范

1. **命名规范**
   - 遵循Rust官方命名规范
   - 模块名: `snake_case`
   - 结构体名: `PascalCase`
   - 函数名: `snake_case`

2. **错误处理**
   - 使用`thiserror` crate定义错误类型
   - 使用`Result`类型处理错误

3. **文档注释**
   - 使用Rust文档注释风格
   - 为公共API提供示例代码

## 测试

### 单元测试

#### C语言单元测试
C语言模块的测试主要通过集成测试进行。

#### Rust单元测试
Rust模块使用Cargo内置测试框架：

```bash
# 运行Rust单元测试
cargo test
```

### 集成测试

项目提供了多种集成测试：

```bash
# 运行所有测试
make test

# 运行Rust模块测试
make test-rust

# 运行FFI集成测试
make test-ffi

# 运行综合集成测试
make test-integration
```

### 性能测试

项目提供了快速性能测试脚本：

```bash
# 运行性能测试
./scripts/quick_benchmark.sh
```

## 性能优化

### 汇编优化

ANX在aarch64架构上使用汇编优化来提高性能：

1. **NEON SIMD指令集**: 用于加速内存操作
2. **CRC32硬件指令**: 用于加速哈希计算
3. **优化的字符串处理**: 提高字符串操作性能

### 内存管理

1. **内存池**: 使用高性能内存池减少系统调用
2. **零拷贝技术**: 使用`sendfile()`减少数据拷贝
3. **对象复用**: 复用HTTP请求/响应对象

### 并发模型

1. **多进程架构**: 充分利用多核CPU
2. **epoll非阻塞I/O**: 支持高并发连接
3. **线程池**: 用于处理计算密集型任务

## 调试

### 日志系统

ANX提供了详细的日志系统：

1. **日志级别**: DEBUG, INFO, WARNING, ERROR
2. **日志输出**: 支持文件和标准输出
3. **日志轮转**: 自动轮转日志文件

### 调试工具

1. **GDB**: 用于调试C代码
2. **Valgrind**: 用于检测内存泄漏
3. **Perf**: 用于性能分析

### 调试模式

构建调试版本以启用调试信息：

```bash
make CFLAGS="-g -O0 -DDEBUG"
```

## 贡献代码

### 提交Pull Request

1. Fork项目仓库
2. 创建功能分支
3. 提交代码更改
4. 编写测试用例
5. 提交Pull Request

### 代码审查

所有Pull Request都需要通过代码审查：

1. 代码符合编码规范
2. 通过所有测试
3. 提供充分的文档
4. 不引入新的安全问题

### 版本发布

项目使用语义化版本控制：

1. **主版本号**: 不兼容的API更改
2. **次版本号**: 向后兼容的功能新增
3. **修订号**: 向后兼容的问题修正