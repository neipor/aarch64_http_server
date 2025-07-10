# anx - aarch64 NginX-like http server - 项目开发路线图

本路线图规划了 anx 的未来发展方向，旨在将其打造为一款功能全面、性能卓越、安全可靠的现代化Web服务器。

**作者**: neipor  
**邮箱**: [neitherportal@proton.me](mailto:neitherportal@proton.me)

[English Version](#english-version)

---

## 🎯 总体目标

- **性能**: 成为同类C语言服务器中的性能佼佼者。
- **功能**: 对标Nginx，提供丰富且实用的功能。
- **安全**: 提供强大的安全防护机制。
- **易用**: 保持简单的配置和部署方式。

---

## 🗺️ 版本规划

### 战略调整说明 (2025年7月)

为了项目的长期健康、安全性和最终的成功，我们做出了一个关键的战略决策：**在实现HTTP/2等新功能之前，优先完成向C/Rust混合架构的演进。**

我们相信，通过首先用Rust构建一个更安全、更现代化的核心平台，我们可以用更高的质量和更快的速度交付未来的复杂功能（如HTTP/2），从而避免陷入C语言的“技术债泥潭”。这是一个“磨刀不误砍柴工”的战略选择。

因此，当前的路线图将优先聚焦于 **架构演进**。

---

### **当前核心目标 (v1.1.0+) - 架构演进：迈向C/Rust混合系统** 🚧 *进行中*

这是项目下一阶段的**最重要目标**，旨在通过引入Rust来从根本上提升服务器的安全性、稳定性和可维护性，同时保持其顶尖性能。详细计划请参阅 `ARCHITECTURE.md`。

-   [ ] **阶段一：新一代配置系统**
    -   [ ] **原生配置支持**: 使用Rust, TOML和`serde`库，实现一个全新的、类型安全的TOML配置解析器 (`anx.toml`)。
    -   [ ] **FFI集成**: 将新的Rust配置模块编译为静态库，并集成到现有的C语言核心中。
    -   [ ] **Nginx兼容层**: 开发一个兼容层，用于读取常见的`nginx.conf`配置，方便用户平滑迁移。
-   [ ] **阶段二：核心逻辑现代化**
    -   [ ] **HTTP解析器重构**: 逐步将C语言实现的、逻辑复杂的HTTP头部和请求解析逻辑，迁移到新的、更安全的Rust模块中。
    -   [ ] **缓存系统重构**: 利用Rust的并发安全特性和现有生态库，重构缓存系统，以消除潜在的并发bug。
-   [ ] **阶段三：构建系统升级**
    -   [ ] 引入`cargo`来管理Rust部分，并调整`Makefile`以支持混合编译流程。

### **未来目标 (v2.0.0+) - 新架构上的功能完善**

在混合架构平台稳定后，我们将在此基础上，以更高的效率和质量实现以下功能。

-   [ ] **HTTP/2 支持**
-   [ ] **WebSockets 支持**
-   [ ] **HTTP/3 & QUIC 支持**
-   [ ] **动态模块系统**
-   [ ] **Web应用防火墙 (WAF)**
-   [ ] **智能化运维与Prometheus集成**
-   [ ] **URL重写与重定向**
-   [ ] **脚本语言集成 (Lua/mruby)**

---

## 📜 已完成里程碑

-   **v1.0.0**: 首个正式版发布，项目重命名，文档国际化。
-   **v0.8.0**: aarch64汇编优化 & 流媒体
-   **v0.7.0**: 缓存系统
-   [ ] **v0.6.0**: 负载均衡系统
-   [ ] **v0.5.0**: 内容压缩
-   [ ] **v0.4.0**: 访问日志系统 & HTTP头部处理
-   [ ] **v0.3.0**: 反向代理
-   [ ] **v0.2.0**: HTTPS支持 & 多进程架构
-   [ ] **v0.1.0**: 基础HTTP服务器 & epoll事件驱动模型

---
<br>

# anx - aarch64 NginX-like http server - Project Development Roadmap

This roadmap outlines the future development direction for anx, aiming to build it into a fully-featured, high-performance, secure, and reliable modern web server.

**Author**: neipor  
**Email**: [neitherportal@proton.me](mailto:neitherportal@proton.me)

---

## 🎯 Overall Goals

- **Performance**: To be a performance leader among similar C-based servers.
- **Functionality**: To match Nginx by providing a rich and practical feature set.
- **Security**: To offer robust security protection mechanisms.
- **Ease of Use**: To maintain simple configuration and deployment methods.

---

## 🗺️ Version Planning

### Strategic Adjustment Note (July 2024)

For the long-term health, security, and ultimate success of the project, we have made a critical strategic decision: **Prioritize the evolution to a C/Rust hybrid architecture before implementing new features like HTTP/2.**

We believe that by first building a safer, more modern core platform with Rust, we can deliver future complex features (like HTTP/2) with higher quality and greater velocity, thus avoiding the "technical debt trap" of C. This is a strategic choice to "sharpen the axe before cutting the tree."

Therefore, the current roadmap will focus primarily on **Architectural Evolution**.

---

### **Current Core Goal (v1.1.0+) - Architectural Evolution: Towards a C/Rust Hybrid System** 🚧 *In Progress*

This is the **most critical goal** for the next phase of the project. It aims to fundamentally enhance the server's security, stability, and maintainability by introducing Rust, while retaining its top-tier performance. For a detailed strategy, see `ARCHITECTURE.md`.

-   [ ] **Phase 1: Next-Generation Configuration System**
    -   [ ] **Native Configuration Support**: Implement a new, type-safe TOML configuration parser (`anx.toml`) using Rust, TOML, and the `serde` library.
    -   [ ] **FFI Integration**: Compile the new Rust configuration module as a static library and integrate it into the existing C core.
    -   [ ] **Nginx Compatibility Layer**: Develop a compatibility layer to read common `nginx.conf` configurations, facilitating smooth migration for users.
-   [ ] **Phase 2: Core Logic Modernization**
    -   [ ] **HTTP Parser Refactoring**: Gradually migrate the complex HTTP header and request parsing logic from C to a new, safer Rust module.
    -   [ ] **Cache System Refactoring**: Leverage Rust's concurrency safety and ecosystem libraries to refactor the cache system, eliminating potential concurrency bugs.
-   [ ] **Phase 3: Build System Upgrade**
    -   [ ] Introduce `cargo` to manage the Rust parts and adapt the `Makefile` to support the hybrid compilation process.

### **Future Goals (v2.0.0+) - Feature Completion on the New Architecture**

After the hybrid architecture platform is stable, we will implement the following features on this new foundation with greater efficiency and quality.

-   [ ] **HTTP/2 Support**
-   [ ] **WebSockets Support**
-   [ ] **HTTP/3 & QUIC Support**
-   [ ] **Dynamic Module System**
-   [ ] **Web Application Firewall (WAF)**
-   [ ] **Intelligent Operations & Prometheus Integration**
-   [ ] **URL Rewriting and Redirection**
-   [ ] **Scripting Language Integration (Lua/mruby)**

---

## 📜 Completed Milestones

-   **v1.0.0**: First official release, project renaming, and documentation internationalization.
-   **v0.8.0**: AArch64 assembly optimizations & streaming media.
-   **v0.7.0**: Caching system
-   [ ] **v0.6.0**: Load balancing system.
-   [ ] **v0.5.0**: Content compression.
-   [ ] **v0.4.0**: Access logging system & HTTP header manipulation.
-   [ ] **v0.3.0**: Reverse proxy.
-   [ ] **v0.2.0**: HTTPS support & multi-process architecture.
-   [ ] **v0.1.0**: Basic HTTP server & epoll event-driven model.

---

> This roadmap will be updated periodically based on community feedback and technological trends. Your valuable suggestions are welcome! 