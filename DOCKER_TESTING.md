# ANX HTTP Server - Docker测试环境指南

**作者**: neipor  
**邮箱**: [neitherportal@proton.me](mailto:neitherportal@proton.me)

本文档提供了在Docker容器中对ANX HTTP Server进行构建、测试和调试的详细指南。

---

## 🚀 快速开始

### 依赖

- **Docker**: 最新版本
- **Docker Compose**: 最新版本

### 核心文件

- `docker-compose.yml`: 定义测试服务和环境。
- `Dockerfile.test`: 用于构建包含所有依赖项的测试镜像。
- `run-docker-tests.sh`: 在容器内执行完整测试套件的入口脚本。

---

## 🛠️ 构建与运行

### 1. 构建测试镜像

镜像会自动构建所有必需的依赖项，包括`gcc`, `openssl`, `zlib`等。

```bash
docker-compose build
```

### 2. 启动测试环境

此命令将启动一个后台运行的ANX服务器容器，用于手动测试和交互。

```bash
docker-compose up -d
```

### 3. 执行测试套件

这是最常用的命令，它会启动一个临时容器，在其中编译ANX并执行完整的自动化测试脚本。

```bash
./run-docker-tests.sh
```

---

## 🔬 测试流程详解

`run-docker-tests.sh`脚本执行以下操作：

1.  **启动容器**: 使用`docker-compose`启动一个测试实例。
2.  **执行测试**: 在容器内运行`docker-test-suite.sh`。
    -   **编译ANX**: 使用推荐的生产环境优化选项进行编译。
    -   **环境准备**: 创建测试所需的文件和目录。
    -   **启动服务器**: 在后台启动ANX服务器。
    -   **运行测试**:
        -   基础功能测试 (小文件、大文件传输)。
        -   压缩功能测试。
        -   缓存功能测试。
        -   并发连接测试。
        -   汇编优化功能验证。
    -   **生成报告**: 输出测试结果和性能指标。
3.  **清理环境**: 测试完成后自动关闭并移除容器。

---

## 🐛 调试指南

### 1. 进入正在运行的容器

如果`docker-compose up -d`正在运行，你可以进入容器进行调试。

```bash
docker-compose exec anx-test /bin/bash
```

### 2. 查看实时日志

你可以实时查看ANX服务器的日志输出。

```bash
docker-compose logs -f
```

### 3. 在容器内手动编译

在通过`exec`进入容器后，你可以手动编译和运行服务器，以便进行更详细的调试。

```bash
# 在容器内执行
make clean && make CFLAGS="-g -O0"  # 使用调试标记编译
gdb ./anx
```

---

> 使用Docker环境可以确保在一致、可复现的环境中进行测试，避免了本地环境差异带来的问题。 