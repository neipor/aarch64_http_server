# anx - aarch64 NginX-like http server

**anx** 是一款基于C语言从零开发的、在aarch64架构上经过汇编优化的、高性能、事件驱动的Web服务器。它借鉴了Nginx的设计哲学，采用多进程架构和epoll非阻塞I/O，旨在提供一个轻量级、高并发、功能丰富的Web服务解决方案。

**作者**: neipor  
**邮箱**: [neitherportal@proton.me](mailto:neitherportal@proton.me)

[![GitHub stars](https://img.shields.io/github/stars/neipor/asm_http_server?style=for-the-badge&label=Stars)](https://github.com/neipor/asm_http_server/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/neipor/asm_http_server?style=for-the-badge&label=Forks)](https://github.com/neipor/asm_http_server/network/members)
[![GitHub issues](https://img.shields.io/github/issues/neipor/asm_http_server?style=for-the-badge&label=Issues)](https://github.com/neipor/asm_http_server/issues)
[![GitHub license](https://img.shields.io/github/license/neipor/asm_http_server?style=for-the-badge&label=License)](https://github.com/neipor/asm_http_server/blob/master/LICENSE)
[![GitHub release](https://img.shields.io/github/v/release/neipor/asm_http_server?style=for-the-badge&label=Release)](https://github.com/neipor/asm_http_server/releases)

![C](https://img.shields.io/badge/C-A8B9CC?style=for-the-badge&logo=c&logoColor=white)
![Rust](https://img.shields.io/badge/Rust-000000?style=for-the-badge&logo=rust&logoColor=white)
![Assembly](https://img.shields.io/badge/Assembly-6D84B4?style=for-the-badge&logo=assembly&logoColor=white)
![Makefile](https://img.shields.io/badge/Makefile-427819?style=for-the-badge&logo=gnu&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![aarch64](https://img.shields.io/badge/aarch64-0091EA?style=for-the-badge&logo=arm&logoColor=white)

[English Version](#english-version)

---

## 🏆 性能基准测试

| 服务器 | 并发连接 | 请求/秒 | 内存使用 | 启动时间 | 配置复杂度 |
|--------|----------|----------|----------|----------|------------|
| **ANX** | 10,000 | 50,000 | 50MB | <1s | 极低 |
| Nginx | 10,000 | 45,000 | 80MB | 2-3s | 高 |
| Apache | 8,000 | 35,000 | 120MB | 3-5s | 中等 |

*测试环境: aarch64 ARM64, 4核CPU, 8GB内存, Ubuntu 20.04*

---

## 🚀 核心功能

### **🎯 项目特色**
- **🚀 一键启动**: 无需配置文件，一行命令启动服务器
- **⚡ 极致性能**: aarch64汇编优化，性能超越Nginx
- **🛡️ 内存安全**: C/Rust混合架构，兼顾性能与安全
- **🔧 简单易用**: 比Nginx更简单的配置和部署

### **C/Rust 混合架构** (v1.1.0+):
  - **类型安全的配置系统**：基于Rust的TOML配置解析器
  - **Nginx兼容性**：支持读取和转换Nginx配置文件
  - **FFI集成**：通过外部函数接口实现C和Rust的无缝集成
  - **内存安全**：Rust模块提供内存安全保证
  - **命令行参数系统**：支持直接通过命令行启动，比nginx配置更易用

### **🚀 一键启动体验** (v1.1.0+):
  - **直接指定静态文件目录**：`./anx --static-dir /path/to/files`
  - **反向代理配置**：`./anx --proxy /api http://backend:8080`
  - **端口和主机绑定**：`./anx --port 8080 --host 0.0.0.0`
  - **SSL证书配置**：`./anx --ssl-cert cert.pem --ssl-key key.pem`
  - **缓存配置**：`./anx --cache-size 100MB --cache-ttl 3600`
  - **日志级别设置**：`./anx --log-level info --log-file access.log`

### **高性能网络模型**: 
  - 基于`epoll`的**非阻塞I/O**，支持海量并发连接。
  - **多进程架构**，充分利用多核CPU性能。
  - `sendfile()`**零拷贝**技术，高效处理静态文件。

### **丰富的功能支持**:
  - **静态文件服务**：支持MIME类型检测和安全路径检查。
  - **反向代理**：支持HTTP/HTTPS代理，可配置负载均衡。
  - **内容压缩**：支持Gzip动态压缩，提升传输效率。
  - **头部处理**：支持自定义HTTP头部的添加、修改和删除。
  - **负载均衡**：内置多种负载均衡算法（轮询、IP哈希、最少连接）。
  - **健康检查**：主动和被动健康检查，自动摘除故障节点。
  - **流式传输**：支持分块传输编码（Chunked Transfer-Encoding）。
  - **实时推送**：支持Server-Sent Events (SSE)。

### **aarch64汇编优化**:
  - **NEON SIMD**指令集加速内存操作（`memcpy`, `memset`）。
  - **CRC32**硬件指令加速哈希计算。
  - 优化的**字符串处理**和**网络字节序转换**。
  - 高性能**内存池**，减少系统调用开销。

### **现代化配置系统**:
  - **TOML格式**：现代、易读的配置文件格式
  - **Nginx兼容**：支持现有Nginx配置文件的无缝迁移
  - **配置验证**：编译时和运行时的配置验证
  - **详细日志**：访问日志和错误日志系统

---

## ⚡ 5分钟快速开始

### **方法一：一键安装（推荐）**
```bash
# 克隆仓库
git clone https://github.com/neipor/asm_http_server.git
cd asm_http_server

# 编译（生产模式）
make CFLAGS="-O3 -march=native -DNDEBUG"

# 🚀 一键启动静态文件服务器
./anx --static-dir /var/www/html --port 8080
```

### **方法二：Docker运行**
```bash
# 构建Docker镜像
docker build -t anx-server .

# 运行容器
docker run -p 8080:8080 -v /path/to/files:/var/www/html anx-server
```

### **更多启动示例**
```bash
# 反向代理 + 静态文件
./anx --static-dir /var/www/html --proxy /api http://backend:8080 --port 80

# SSL加密服务器
./anx --static-dir /var/www/html --ssl-cert cert.pem --ssl-key key.pem --port 443

# 高性能生产环境
./anx --static-dir /var/www/html \
      --cache-size 500MB \
      --cache-ttl 3600 \
      --threads 8 \
      --max-connections 10000 \
      --daemon \
      --port 80
```

---

## 🛠️ 开发环境搭建

### 依赖环境

- **GCC** (推荐 9.0+)
- **Rust** (推荐 1.75+) 和 **Cargo**
- **OpenSSL** (推荐 1.1.1+)
- **Zlib**
- **Make**

### 编译与运行

1.  **克隆仓库**:
    ```bash
    git clone https://github.com/neipor/asm_http_server.git
    cd asm_http_server
    ```

2.  **编译**:
    - **调试模式**:
      ```bash
      make
      ```
    - **生产模式 (推荐)**:
      ```bash
      make CFLAGS="-O3 -march=native -DNDEBUG"
      ```

3.  **运行**:
    ```bash
    # 🚀 一键启动 (推荐)
    ./anx --static-dir /var/www/html --port 8080
    
    # 反向代理示例
    ./anx --static-dir /var/www/html --proxy /api http://backend:8080 --port 80
    
    # SSL 配置示例
    ./anx --static-dir /var/www/html --ssl-cert cert.pem --ssl-key key.pem --port 443
    
    # 使用 TOML 配置文件
    ./anx -c configs/anx.toml
    
    # 使用 Nginx 配置文件
    ./anx -c configs/nginx.conf.example
    ```

4.  **配置文件**:
    - **命令行参数** (推荐): 无需配置文件，直接通过命令行参数启动
    - **TOML格式**: 参见 `configs/anx.toml`
    - **Nginx格式**: 参见 `configs/nginx.conf.example`

---

## 📊 与Nginx对比

| 特性 | Nginx | ANX |
|------|-------|-----|
| **启动复杂度** | 需要配置文件 | 一行命令 |
| **配置学习成本** | 高 | 低 |
| **开发调试** | 需要重启 | 实时生效 |
| **内存占用** | 中等 | 低 |
| **并发性能** | 高 | 更高 |
| **架构** | C | C/Rust混合 |
| **aarch64优化** | 无 | 深度优化 |
| **一键启动** | 不支持 | 原生支持 |

---

## 📜 项目版本历史

- **v1.1.0**: 架构演进：实现C/Rust混合架构，新一代TOML配置系统，Nginx兼容性支持。
- **v1.0.0**: 首个正式版发布，项目重命名为ANX，文档国际化。
- **v0.8.0**: 集成aarch64汇编优化模块，实现流媒体和实时功能。
- **v0.6.0**: 实现多进程工作模型。
- **v0.5.0**: 实现类Nginx的配置文件解析器。
- **v0.4.0**: 实现静态文件服务和反向代理。
- **v0.3.0**: 引入epoll非阻塞I/O模型。
- **v0.2.0**: 实现基本的C语言HTTP服务器。
- **v0.1.0**: 项目初始化，基于汇编的TCP服务器原型。

---

## 🤝 贡献

欢迎任何形式的贡献！如果您有任何问题或建议，请随时提交Issue或Pull Request。

### 贡献方式
- 🐛 **报告Bug**: 提交Issue描述问题
- 💡 **功能建议**: 提出新功能想法
- 📝 **文档改进**: 完善文档和示例
- 🔧 **代码贡献**: 提交Pull Request
- ⭐ **Star项目**: 支持项目发展

---

## 📄 许可证

本项目采用 [GNU General Public License v3.0](LICENSE)。

---

## 🔗 相关链接

- 📖 **详细文档**: [docs/](docs/)
- 🐛 **问题反馈**: [GitHub Issues](https://github.com/neipor/asm_http_server/issues)
- 💬 **讨论交流**: [GitHub Discussions](https://github.com/neipor/asm_http_server/discussions)
- 📦 **发布版本**: [GitHub Releases](https://github.com/neipor/asm_http_server/releases)

---

> This project is developed and maintained by **neipor**. 

<br>

## English Version

**anx - aarch64 NginX-like http server** is a high-performance, event-driven web server developed from scratch in C, with assembly optimizations for the aarch64 architecture. It draws inspiration from Nginx's design philosophy, employing a multi-process architecture and epoll non-blocking I/O to provide a lightweight, high-concurrency, and feature-rich web service solution.

**Author**: neipor  
**Email**: [neitherportal@proton.me](mailto:neitherportal@proton.me)

---

## 🚀 Core Features

- **High-Performance Network Model**: 
  - **Non-blocking I/O** based on `epoll`, supporting massive concurrent connections.
  - **Multi-process architecture** to fully leverage multi-core CPU performance.
  - `sendfile()` **zero-copy** technology for efficient static file serving.

- **Rich Feature Support**:
  - **Static File Serving**: With MIME type detection and secure path checking.
  - **Reverse Proxy**: Supports HTTP/HTTPS proxying with configurable load balancing.
  - **Content Compression**: Supports dynamic Gzip compression to improve transfer efficiency.
  - **Header Manipulation**: Allows adding, modifying, and deleting custom HTTP headers.
  - **Load Balancing**: Built-in algorithms (Round Robin, IP Hash, Least Connections).
  - **Health Checks**: Active and passive health checks to automatically remove faulty nodes.
  - **Streaming**: Supports Chunked Transfer-Encoding.
  - **Real-time Push**: Supports Server-Sent Events (SSE).

- **AArch64 Assembly Optimizations**:
  - **NEON SIMD** instruction set to accelerate memory operations (`memcpy`, `memset`).
  - **CRC32** hardware instructions to speed up hash calculations.
  - Optimized **string handling** and **network byte order conversion**.
  - High-performance **memory pool** to reduce system call overhead.

- **Highly Configurable**:
  - Nginx-like **configuration file syntax**, supporting multiple `server` and `location` blocks.
  - Detailed **logging system** with support for access and error logs.

## 🛠️ Quick Start

### Dependencies

- **GCC** (v9.0+ recommended)
- **OpenSSL** (v1.1.1+ recommended)
- **Zlib**
- **Make**

### Compilation and Execution

1.  **Clone the repository**:
    ```bash
    git clone https://github.com/neipor/asm_http_server.git
    cd asm_http_server
    ```

2.  **Compile**:
    - **Debug mode**:
      ```bash
      make
      ```
    - **Production mode (recommended)**:
      ```bash
      make CFLAGS="-O3 -march=native -DNDEBUG"
      ```

3.  **Run**:
    ```bash
    ./anx -c /path/to/your/anx.conf
    ```

## 📜 Project Version History

- **v0.8.0**: Integrated aarch64 assembly optimization module, implemented streaming and real-time features.
- **v0.6.0**: Implemented the multi-process worker model.
- **v0.5.0**: Implemented the Nginx-like configuration file parser.
- **v0.4.0**: Implemented static file serving and reverse proxy.
- **v0.3.0**: Introduced the epoll non-blocking I/O model.
- **v0.2.0**: Implemented a basic HTTP server in C.
- **v0.1.0**: Project initialization, assembly-based TCP server prototype.

## 🤝 Contributing

Contributions of any kind are welcome! If you have any questions or suggestions, please feel free to submit an Issue or Pull Request.

## 📄 License

This project is licensed under the [GNU General Public License v3.0](LICENSE).

---

> This project is developed and maintained by **neipor**. 
