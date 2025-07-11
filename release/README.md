# ANX HTTP Server v1.1.0+ 发布包

## 🚀 一键启动体验

ANX v1.1.0+ 引入了革命性的命令行参数系统，让启动HTTP服务器变得比nginx更简单！

### 快速开始

```bash
# 最简单的用法 - 启动静态文件服务器
./anx --static-dir /var/www/html --port 8080

# 反向代理示例
./anx --static-dir /var/www/html --proxy /api http://backend:8080 --port 80

# SSL 配置示例
./anx --static-dir /var/www/html --ssl-cert cert.pem --ssl-key key.pem --port 443
```

### 主要特性

- **🚀 命令行参数系统**: 无需配置文件，一行命令启动
- **C/Rust 混合架构**: 类型安全和内存安全
- **高性能网络模型**: 基于epoll的非阻塞I/O
- **丰富的功能支持**: 静态文件、反向代理、SSL、缓存
- **aarch64汇编优化**: NEON SIMD指令集加速

### 文件结构

```
release/
├── anx                    # 可执行文件
├── configs/              # 配置文件示例
│   ├── anx.toml         # TOML配置示例
│   └── nginx.conf.example # Nginx兼容配置示例
└── docs/                # 文档
    ├── API_REFERENCE.md # API参考文档
    ├── QUICK_START.md   # 快速启动指南
    └── ...
```

### 系统要求

- **操作系统**: Linux (推荐 Ubuntu 20.04+)
- **架构**: aarch64 (ARM64)
- **依赖**: OpenSSL, Zlib
- **内存**: 最小 64MB RAM
- **磁盘**: 最小 10MB 可用空间

### 安装说明

1. **解压发布包**
   ```bash
   tar -xzf anx-http-server-v1.1.0+.tar.gz
   cd anx-http-server-v1.1.0+
   ```

2. **设置权限**
   ```bash
   chmod +x anx
   ```

3. **启动服务器**
   ```bash
   # 开发测试
   ./anx --static-dir ./www --port 8080
   
   # 生产环境
   ./anx --static-dir /var/www/html --port 80 --daemon
   ```

### 配置示例

#### 开发环境
```bash
./anx --static-dir ./www \
      --port 8080 \
      --log-level debug \
      --log-file -
```

#### 生产环境
```bash
./anx --static-dir /var/www/html \
      --proxy /api http://backend:8080 \
      --cache-size 500MB \
      --cache-ttl 3600 \
      --threads 8 \
      --max-connections 10000 \
      --daemon \
      --pid-file /var/run/anx.pid \
      --log-level info \
      --log-file /var/log/anx.log \
      --port 80
```

### 性能特点

- **高并发**: 支持数万并发连接
- **低延迟**: 毫秒级响应时间
- **内存高效**: 优化的内存管理
- **CPU友好**: 汇编优化和零拷贝技术

### 与nginx对比

| 特性 | nginx | ANX v1.1.0+ |
|------|-------|-------------|
| 启动复杂度 | 需要配置文件 | 一行命令 |
| 配置学习成本 | 高 | 低 |
| 开发调试 | 需要重启 | 实时生效 |
| 内存占用 | 中等 | 低 |
| 并发性能 | 高 | 高 |
| 架构 | C | C/Rust混合 |

### 技术支持

- **文档**: 详见 `docs/` 目录
- **API参考**: `docs/API_REFERENCE.md`
- **快速启动**: `docs/QUICK_START.md`
- **问题反馈**: GitHub Issues

### 许可证

本项目采用 [GNU General Public License v3.0](LICENSE)。

---

**ANX HTTP Server v1.1.0+** - 让HTTP服务器启动变得简单！ 