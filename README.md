# ANX HTTP Server

**ANX HTTP Server** 是一款基于C语言从零开发的、高性能、事件驱动的Web服务器。它借鉴了Nginx的设计哲学，采用多进程架构和epoll非阻塞I/O，旨在提供一个轻量级、高并发、功能丰富的Web服务解决方案。

**作者**: neipor  
**邮箱**: [neitherportal@proton.me](mailto:neitherportal@proton.me)

![C](https://img.shields.io/badge/C-A8B9CC?style=for-the-badge&logo=c&logoColor=white)
![Assembly](https://img.shields.io/badge/Assembly-6D84B4?style=for-the-badge&logo=assembly&logoColor=white)
![Docker](https://img.shields.io/badge/Docker-2496ED?style=for-the-badge&logo=docker&logoColor=white)
![Shell Script](https://img.shields.io/badge/Shell_Script-121011?style=for-the-badge&logo=gnu-bash&logoColor=white)
![Makefile](https://img.shields.io/badge/Makefile-427819?style=for-the-badge&logo=gnu&logoColor=white)

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
    git clone <repository_url>
    cd anx-http-server
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
    *默认配置文件位于 `test-configs/anx.conf`*

### Docker环境

项目提供了完整的Docker测试环境，可以轻松在隔离环境中运行和测试。

1.  **启动测试环境**:
    ```bash
    docker-compose up --build
    ```

2.  **运行测试套件**:
    ```bash
    ./run-docker-tests.sh
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

> 该项目由 **neipor** 开发和维护。 