# ANX HTTP Server 用户使用手册

## 目录
1. [简介](#简介)
2. [快速开始](#快速开始)
3. [命令行参数](#命令行参数)
4. [配置文件](#配置文件)
5. [静态文件服务](#静态文件服务)
6. [反向代理](#反向代理)
7. [负载均衡](#负载均衡)
8. [SSL/TLS配置](#ssltls配置)
9. [缓存](#缓存)
10. [日志](#日志)
11. [性能调优](#性能调优)
12. [故障排除](#故障排除)

## 简介

ANX HTTP Server 是一款基于C语言从零开发的、在aarch64架构上经过汇编优化的、高性能、事件驱动的Web服务器。它借鉴了Nginx的设计哲学，采用多进程架构和epoll非阻塞I/O，旨在提供一个轻量级、高并发、功能丰富的Web服务解决方案。

## 快速开始

### 5分钟快速启动

#### 方法一：一键安装（推荐）
```bash
# 克隆仓库
git clone https://github.com/neipor/asm_http_server.git
cd asm_http_server

# 编译（生产模式）
make CFLAGS="-O3 -march=native -DNDEBUG"

# 🚀 一键启动静态文件服务器
./anx --static-dir /var/www/html --port 8080
```

#### 方法二：Docker运行
```bash
# 构建Docker镜像
docker build -t anx-server .

# 运行容器
docker run -p 8080:8080 -v /path/to/files:/var/www/html anx-server
```

#### 更多启动示例
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

## 命令行参数

ANX 支持丰富的命令行参数，可以直接通过命令行启动服务器，无需配置文件：

| 参数 | 说明 | 示例 |
|------|------|------|
| `--static-dir DIR` | 指定静态文件目录 | `--static-dir /var/www/html` |
| `--proxy PATH URL` | 配置反向代理 | `--proxy /api http://backend:8080` |
| `--port PORT` | 监听端口 (默认8080) | `--port 80` |
| `--host HOST` | 监听主机 (默认0.0.0.0) | `--host 127.0.0.1` |
| `--ssl-cert FILE` | SSL证书 | `--ssl-cert cert.pem` |
| `--ssl-key FILE` | SSL私钥 | `--ssl-key key.pem` |
| `--cache-size SIZE` | 缓存大小 | `--cache-size 100MB` |
| `--cache-ttl SECS` | 缓存TTL | `--cache-ttl 3600` |
| `--log-level LEVEL` | 日志级别 (info/debug/warning/error) | `--log-level debug` |
| `--log-file FILE` | 日志文件 | `--log-file access.log` |
| `--daemon` | 守护进程模式 | `--daemon` |
| `--pid-file FILE` | PID文件 | `--pid-file /var/run/anx.pid` |
| `--dry-run` | 仅打印解析结果不启动服务 | `--dry-run` |
| `--help` | 显示帮助 | `--help` |
| `--version` | 显示版本 | `--version` |
| `-c config_file` | 使用配置文件 | `-c configs/anx.toml` |

## 配置文件

ANX 支持两种配置文件格式：

### TOML格式配置文件

```toml
# anx.toml - 新的原生配置文件

[server]
listen = ["80", "443"]
server_name = "example.com"
root = "/var/www/html"
index = ["index.html", "index.htm"]
worker_processes = 4
worker_connections = 1024
keepalive_timeout = 65
client_max_body_size = "10M"

[logging]
access_log = "/var/log/anx/access.log"
error_log = "/var/log/anx/error.log"
log_level = "info"

[ssl]
certificate = "/etc/ssl/certs/example.com.crt"
private_key = "/etc/ssl/private/example.com.key"
protocols = ["TLSv1.2", "TLSv1.3"]
ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384"

[proxy]
proxy_timeout = 60
proxy_connect_timeout = 5

[proxy.upstream]
backend = ["127.0.0.1:8001", "127.0.0.1:8002"]
api = ["127.0.0.1:9001"]

[cache]
enabled = true
max_size = "100M"
ttl = 3600
keys_zone = "main:10m"

[[locations]]
path = "/"
root = "/var/www/html"
index = ["index.html"]

[[locations]]
path = "/api"
proxy_pass = "http://backend"

[[locations]]
path = "/static"
root = "/var/www/static"
try_files = ["$uri", "$uri/", "@fallback"]

[locations.headers]
"X-Frame-Options" = "SAMEORIGIN"
"X-Content-Type-Options" = "nosniff"
```

### Nginx兼容配置文件

```nginx
worker_processes 4;
events {
    worker_connections 1024;
    use epoll;
    multi_accept on;
}

http {
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    keepalive_timeout 65;
    keepalive_requests 100;
    
    gzip on;
    gzip_vary on;
    gzip_min_length 1000;
    gzip_proxied any;
    gzip_comp_level 6;
    gzip_types text/plain text/css application/json application/javascript text/xml application/xml application/xml+rss text/javascript;
    
    server {
        listen 80;
        server_name _;
        root /usr/share/nginx/html;
        index index.html;
        
        location / {
            try_files $uri $uri/ =404;
        }
        
        location = /status {
            return 200 "Nginx Server\nStatus: Running\nWorkers: 4\n";
            add_header Content-Type text/plain;
        }
    }
}
```

## 静态文件服务

ANX 提供高性能的静态文件服务，支持MIME类型检测和安全路径检查。

### 基本用法

```bash
# 启动静态文件服务器
./anx --static-dir /var/www/html --port 8080
```

### 配置示例

```toml
[server]
root = "/var/www/html"
index = ["index.html", "index.htm"]

[[locations]]
path = "/"
root = "/var/www/html"
index = ["index.html"]

[[locations]]
path = "/images"
root = "/var/www/images"
```

## 反向代理

ANX 支持HTTP/HTTPS反向代理，可配置负载均衡。

### 基本用法

```bash
# 启动反向代理服务器
./anx --static-dir /var/www/html --proxy /api http://backend:8080 --port 80
```

### 配置示例

```toml
[proxy]
proxy_timeout = 60
proxy_connect_timeout = 5

[proxy.upstream]
backend = ["127.0.0.1:8001", "127.0.0.1:8002"]
api = ["127.0.0.1:9001"]

[[locations]]
path = "/api"
proxy_pass = "http://backend"
```

## 负载均衡

ANX 内置多种负载均衡算法（轮询、IP哈希、最少连接）。

### 配置示例

```toml
[proxy.upstream]
# 轮询算法（默认）
backend = ["127.0.0.1:8001", "127.0.0.1:8002"]

# IP哈希算法
api = ["127.0.0.1:9001", "127.0.0.1:9002"]
# 在location中指定算法
# ip_hash = true

# 最少连接算法
service = ["127.0.0.1:7001", "127.0.0.1:7002"]
# 在location中指定算法
# least_conn = true
```

## SSL/TLS配置

ANX 支持SSL/TLS加密，提供安全的HTTPS服务。

### 基本用法

```bash
# 启动SSL服务器
./anx --static-dir /var/www/html --ssl-cert cert.pem --ssl-key key.pem --port 443
```

### 配置示例

```toml
[ssl]
certificate = "/etc/ssl/certs/example.com.crt"
private_key = "/etc/ssl/private/example.com.key"
protocols = ["TLSv1.2", "TLSv1.3"]
ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384"

[server]
listen = ["443 ssl"]
```

## 缓存

ANX 提供基于Rust的缓存模块，支持多种缓存策略。

### 基本用法

```bash
# 启用缓存
./anx --static-dir /var/www/html --cache-size 100MB --cache-ttl 3600 --port 8080
```

### 配置示例

```toml
[cache]
enabled = true
max_size = "100M"
ttl = 3600
keys_zone = "main:10m"
strategy = "lru"  # 可选: lru, lfu, fifo

[[locations]]
path = "/api"
proxy_pass = "http://backend"
# 启用该location的缓存
cache = true
cache_valid = "200 1h"
cache_valid = "404 1m"
```

## 日志

ANX 提供详细的访问日志和错误日志系统。

### 配置示例

```toml
[logging]
access_log = "/var/log/anx/access.log"
error_log = "/var/log/anx/error.log"
log_level = "info"
log_format = "combined"  # 可选: common, combined, json
log_rotation_size_mb = 100
log_rotation_days = 7
```

## 性能调优

### 工作进程调优

```toml
[server]
worker_processes = 4  # 建议设置为CPU核心数
worker_connections = 1024  # 每个工作进程的最大连接数
```

### 内容压缩

```toml
[http]
gzip = true
gzip_comp_level = 6
gzip_min_length = 1000
gzip_types = ["text/plain", "text/css", "application/json", "application/javascript"]
```

### 带宽限制

```toml
[bandwidth]
enable_bandwidth_limit = true
default_rate_limit = "100k"  # 默认速率限制
default_burst_size = "1m"    # 默认突发大小

[[bandwidth.rules]]
pattern = "*.mp4"
rate = "50k"
burst = "500k"
```

## 故障排除

### 常见问题

1. **端口被占用**
   ```
   Port 8080 is already in use
   ```
   解决方案：更改端口号或停止占用端口的进程。

2. **SSL证书加载失败**
   ```
   Failed to load SSL certificate/key
   ```
   解决方案：检查证书和私钥文件路径是否正确，文件权限是否合适。

3. **配置文件解析错误**
   ```
   Failed to parse configuration
   ```
   解决方案：检查配置文件语法是否正确，参考示例配置文件。

### 日志分析

通过查看错误日志可以快速定位问题：
```bash
tail -f /var/log/anx/error.log
```

### 性能监控

ANX 提供性能日志功能，可以监控服务器性能：
```toml
[logging]
enable_performance_logging = true