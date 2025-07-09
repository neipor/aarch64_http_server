# ANX HTTP Server - 架构设计文档

**作者**: neipor  
**邮箱**: [neitherportal@proton.me](mailto:neitherportal@proton.me)

## 1. 核心设计哲学

ANX的核心设计借鉴了Nginx，旨在实现**高并发**、**高性能**和**高可用性**。其关键架构决策包括：

-   **事件驱动模型**: 采用`epoll`非阻塞I/O，实现单线程处理大量并发连接。
-   **多进程架构**: 利用多核CPU，通过Master-Worker模式实现真正的并行处理。
-   **模块化设计**: 功能高度解耦，每个核心功能（如日志、缓存、代理）都是独立的模块，易于维护和扩展。

---

## 2. 整体架构图

```mermaid
graph TD
    subgraph "客户端请求"
        direction LR
        C1[Client 1]
        C2[Client 2]
        C3[Client ...]
    end

    subgraph "ANX HTTP Server"
        direction TB
        Master[Master Process] -- "fork()" --> W1[Worker 1<br/>(epoll loop)]
        Master -- "fork()" --> W2[Worker 2<br/>(epoll loop)]
        Master -- "fork()" --> W3[Worker ...<br/>(epoll loop)]
        
        W1 -- "处理请求" --> M[功能模块]
        W2 -- "处理请求" --> M
        W3 -- "处理请求" --> M
    end

    subgraph "功能模块 (在Worker中执行)"
        direction LR
        M --> HTTP[HTTP解析]
        M --> Static[静态文件]
        M --> Proxy[反向代理]
        M --> Cache[缓存]
        M --> LB[负载均衡]
        M --> ASM[汇编优化]
    end

    C1 --> W1
    C2 --> W2
    C3 --> W3
```

---

## 3. 核心组件详解

### 3.1. Master进程

Master进程是整个服务器的入口和管理者，但不处理任何客户端请求。其主要职责包括：

-   **解析配置**: 读取并验证`anx.conf`配置文件。
-   **绑定端口**: 创建监听Socket并绑定到指定端口。
-   **管理Worker**: 
    -   根据配置`fork()`出指定数量的Worker进程。
    -   监控Worker进程的健康状况，并在Worker意外退出时重新启动它。
-   **信号处理**: 优雅地处理`SIGINT`, `SIGTERM`等信号，确保服务器平滑关闭或重启。

### 3.2. Worker进程

Worker进程是实际处理客户端请求的主体。每个Worker进程都包含一个独立的**epoll事件循环**。

1.  **接受连接**: 内核将新的客户端连接有效地分发给空闲的Worker进程（`accept`惊群问题已由现代内核解决）。
2.  **事件循环**:
    -   通过`epoll_wait()`等待网络事件。
    -   **读事件**: 读取客户端请求数据，并进行HTTP解析。
    -   **写事件**: 当响应数据准备好且Socket可写时，发送响应数据。
3.  **请求处理**: 根据解析出的请求和`location`配置，将请求分发给相应的功能模块进行处理。

### 3.3. 功能模块

-   **HTTP模块**: 负责解析HTTP请求行、头部和主体。
-   **静态文件模块**: 高效地提供静态资源，使用`sendfile()`实现零拷贝。
-   **反向代理模块**: 将请求转发给上游服务器，并返回其响应。
-   **负载均衡模块**: 当配置了多个上游服务器时，根据指定策略（如轮询、IP哈希）选择一个后端。
-   **缓存模块**: 缓存上游服务器的响应，减少后端压力。
-   **汇编优化模块**: 在关键路径（如内存操作、哈希计算）上使用aarch64汇编指令提升性能。

## 4. 内存管理

-   **内存池**: 为了减少频繁的`malloc`/`free`带来的性能开销和内存碎片，ANX实现了一个高性能的内存池 (`asm_mempool.c`)。HTTP请求生命周期内的大部分小块内存分配都通过内存池进行。
-   **大块内存**: 对于大文件等需要大块连续内存的场景，仍然使用标准的`malloc`或`mmap`。
-   **生命周期管理**: 所有内存在请求处理完成后都会被释放，防止内存泄漏。

## 5. 数据流

1.  客户端请求到达服务器端口。
2.  一个Worker进程的`accept()`被唤醒，建立连接。
3.  Worker将新的Socket描述符添加到其epoll实例中。
4.  当Socket可读时，Worker读取数据，HTTP模块解析请求。
5.  根据`location`匹配，请求被分发到**静态文件**或**反向代理**模块。
6.  模块处理请求，生成响应数据。
7.  当Socket可写时，Worker将响应数据发送给客户端。
8.  连接关闭，相关资源被释放。 