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

### 短期目标 (v0.9.0 - v1.0.0) - 性能与稳定性

-   [ ] **性能基准测试框架**:
    -   [ ] 建立标准化的性能测试场景 (wrk, ab)。
    -   [ ] 持续追踪关键性能指标 (RPS, 延迟, 内存占用)。
-   [ ] **稳定性增强**:
    -   [ ] 完整的单元测试和集成测试覆盖。
    -   [ ] 引入静态代码分析工具 (Clang Static Analyzer, Cppcheck)。
    -   [ ] 修复所有已知的内存泄漏和并发问题。
-   [ ] **HTTPS性能优化**:
    -   [ ] 利用OpenSSL 3.0新特性，提升TLS握手性能。
    -   [ ] 实现会话复用 (Session Resumption)。
-   [ ] **汇编优化扩展**:
    -   [ ] 支持更多aarch64指令 (如SVE)。
    -   [ ] 为x86_64架构添加SSE/AVX优化。

### 中期目标 (v1.1.0 - v1.5.0) - 功能完善与生态

-   [ ] **HTTP/2 支持**:
    -   [ ] 实现HTTP/2协议栈。
    -   [ ] 支持头部压缩 (HPACK)。
    -   [ ] 实现服务器推送 (Server Push)。
-   [ ] **动态模块系统**:
    -   [ ] 允许在不重新编译服务器的情况下加载功能模块 (.so)。
    -   [ ] 提供清晰的模块开发API。
-   [ ] **高级缓存策略**:
    -   [ ] 引入基于磁盘的持久化缓存。
    -   [ ] 支持缓存清除 (Cache Purge) API。
    -   [ ] 实现Stale-while-revalidate和Stale-if-error。
-   [ ] **WebSockets 支持**:
    -   [ ] 实现完整的WebSocket协议代理。
-   [ ] **URL重写与重定向**:
    -   [ ] 实现强大的URL Rewrite模块，支持正则表达式。

### 长期目标 (v2.0.0+) - 前沿技术与智能化

-   [ ] **HTTP/3 & QUIC 支持**:
    -   [ ] 集成QUIC协议栈，实现HTTP/3。
-   [ ] **智能化运维**:
    -   [ ] 实现基于运行时数据的**自动性能调优**。
    -   [ ] 集成Prometheus，提供丰富的监控指标。
-   [ ] **Web应用防火墙 (WAF)**:
    -   [ ] 开发内置的WAF模块，防御常见Web攻击 (SQL注入, XSS)。
-   [ ] **脚本语言集成**:
    -   [ ] 支持嵌入Lua或mruby，实现更灵活的请求处理逻辑。
-   [ ] **多平台原生支持**:
    -   [ ] 优化对FreeBSD (kqueue) 和 Windows (IOCP) 的支持。

---

## 📜 已完成里程碑

-   **v0.8.0**: aarch64汇编优化 & 流媒体
-   [ ] **v0.7.0**: 缓存系统
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

### Short-Term Goals (v0.9.0 - v1.0.0) - Performance & Stability

-   [ ] **Performance Benchmarking Framework**:
    -   [ ] Establish standardized performance testing scenarios (wrk, ab).
    -   [ ] Continuously track key performance indicators (RPS, latency, memory usage).
-   [ ] **Stability Enhancements**:
    -   [ ] Complete unit and integration test coverage.
    -   [ ] Introduce static code analysis tools (Clang Static Analyzer, Cppcheck).
    -   [ ] Fix all known memory leaks and concurrency issues.
-   [ ] **HTTPS Performance Optimization**:
    -   [ ] Leverage new features in OpenSSL 3.0 to improve TLS handshake performance.
    -   [ ] Implement session resumption.
-   [ ] **Assembly Optimization Expansion**:
    -   [ ] Support for more aarch64 instructions (e.g., SVE).
    -   [ ] Add SSE/AVX optimizations for the x86_64 architecture.

### Mid-Term Goals (v1.1.0 - v1.5.0) - Feature Completion & Ecosystem

-   [ ] **HTTP/2 Support**:
    -   [ ] Implement the HTTP/2 protocol stack.
    -   [ ] Support header compression (HPACK).
    -   [ ] Implement Server Push.
-   [ ] **Dynamic Module System**:
    -   [ ] Allow loading functional modules (.so) without recompiling the server.
    -   [ ] Provide a clear module development API.
-   [ ] **Advanced Caching Strategies**:
    -   [ ] Introduce disk-based persistent caching.
    -   [ ] Support a Cache Purge API.
    -   [ ] Implement Stale-while-revalidate and Stale-if-error.
-   [ ] **WebSockets Support**:
    -   [ ] Implement a full WebSocket protocol proxy.
-   [ ] **URL Rewriting and Redirection**:
    -   [ ] Implement a powerful URL Rewrite module with regex support.

### Long-Term Goals (v2.0.0+) - Cutting-Edge Technology & Intelligence

-   [ ] **HTTP/3 & QUIC Support**:
    -   [ ] Integrate a QUIC protocol stack to implement HTTP/3.
-   [ ] **Intelligent Operations**:
    -   [ ] Implement **automatic performance tuning** based on runtime data.
    -   [ ] Integrate with Prometheus to provide rich monitoring metrics.
-   [ ] **Web Application Firewall (WAF)**:
    -   [ ] Develop a built-in WAF module to defend against common web attacks (SQL injection, XSS).
-   [ ] **Scripting Language Integration**:
    -   [ ] Support embedded Lua or mruby for more flexible request handling logic.
-   [ ] **Native Multi-platform Support**:
    -   [ ] Optimize support for FreeBSD (kqueue) and Windows (IOCP).

---

## 📜 Completed Milestones

-   **v0.8.0**: AArch64 assembly optimizations & streaming media.
-   [ ] **v0.7.0**: Caching system.
-   [ ] **v0.6.0**: Load balancing system.
-   [ ] **v0.5.0**: Content compression.
-   [ ] **v0.4.0**: Access logging system & HTTP header manipulation.
-   [ ] **v0.3.0**: Reverse proxy.
-   [ ] **v0.2.0**: HTTPS support & multi-process architecture.
-   [ ] **v0.1.0**: Basic HTTP server & epoll event-driven model.

---

> This roadmap will be updated periodically based on community feedback and technological trends. Your valuable suggestions are welcome! 