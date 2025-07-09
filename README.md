# ANX HTTP Server

**ANX HTTP Server** 是一款基于C语言从零开发的、高性能、事件驱动的Web服务器。它借鉴了Nginx的设计哲学，采用多进程架构和epoll非阻塞I/O，旨在提供一个轻量级、高并发、功能丰富的Web服务解决方案。

**作者**: neipor  
**邮箱**: [neitherportal@proton.me](mailto:neitherportal@proton.me)

![C](https://img.shields.io/badge/C-A8B9CC?style=for-the-badge&logo=c&logoColor=white)
![Assembly](https://img.shields.io/badge/Assembly-6D84B4?style=for-the-badge&logo=assembly&logoColor=white)
![Makefile](https://img.shields.io/badge/Makefile-427819?style=for-the-badge&logo=gnu&logoColor=white)

[English Version](#english-version)

---

## 🚀 核心功能

- **高性能网络模型**: 
  - 基于`epoll`的**非阻塞I/O**，支持海量并发连接。
  - **多进程架构**，充分利用多核CPU性能。
  - `sendfile()`**零拷贝**技术，高效处理静态文件。

- **丰富的功能支持**:
  - **静态文件服务**：支持MIME类型检测和安全路径检查。
  - **反向代理**：支持HTTP/HTTPS代理，可配置负载均衡。
  - **内容压缩**：支持Gzip动态压缩，提升传输效率。
  - **头部处理**：支持自定义HTTP头部的添加、修改和删除。
  - **负载均衡**：内置多种负载均衡算法（轮询、IP哈希、最少连接）。
  - **健康检查**：主动和被动健康检查，自动摘除故障节点。
  - **流式传输**：支持分块传输编码（Chunked Transfer-Encoding）。
  - **实时推送**：支持Server-Sent Events (SSE)。

- **aarch64汇编优化**:
  - **NEON SIMD**指令集加速内存操作（`memcpy`, `memset`）。
  - **CRC32**硬件指令加速哈希计算。
  - 优化的**字符串处理**和**网络字节序转换**。
  - 高性能**内存池**，减少系统调用开销。

- **高度可配置**:
  - 类Nginx的**配置文件语法**，支持多`server`和`location`块。
  - 详细的**日志系统**，支持访问日志和错误日志。

## 🛠️ 快速开始

### 依赖环境

- **GCC** (推荐 9.0+)
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
    ./anx -c /path/to/your/anx.conf
    ```

## 📜 项目版本历史

- **v0.8.0**: 集成aarch64汇编优化模块，实现流媒体和实时功能。
- **v0.6.0**: 实现多进程工作模型。
- **v0.5.0**: 实现类Nginx的配置文件解析器。
- **v0.4.0**: 实现静态文件服务和反向代理。
- **v0.3.0**: 引入epoll非阻塞I/O模型。
- **v0.2.0**: 实现基本的C语言HTTP服务器。
- **v0.1.0**: 项目初始化，基于汇编的TCP服务器原型。

## 🤝 贡献

欢迎任何形式的贡献！如果您有任何问题或建议，请随时提交Issue或Pull Request。

---
<br>

## English Version

**ANX HTTP Server** is a high-performance, event-driven web server developed from scratch in C. It draws inspiration from Nginx's design philosophy, employing a multi-process architecture and epoll non-blocking I/O to provide a lightweight, high-concurrency, and feature-rich web service solution.

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

---

> This project is developed and maintained by **neipor**. 
