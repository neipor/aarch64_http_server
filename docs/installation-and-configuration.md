# ANX HTTP Server 安装和配置指南

## 目录
1. [系统要求](#系统要求)
2. [安装步骤](#安装步骤)
3. [配置文件](#配置文件)
4. [启动服务器](#启动服务器)
5. [常见问题](#常见问题)

## 系统要求

ANX HTTP Server 需要以下依赖：

- **GCC** (推荐 9.0+)
- **Rust** (推荐 1.75+) 和 **Cargo**
- **OpenSSL** (推荐 1.1.1+)
- **Zlib**
- **Make**

## 安装步骤

### 1. 克隆仓库

```bash
git clone https://github.com/neipor/asm_http_server.git
cd asm_http_server
```

### 2. 检查依赖

```bash
make check-deps
```

如果缺少任何依赖，请先安装它们。

### 3. 编译

ANX 支持两种编译模式：

- **调试模式** (默认)：
  ```bash
  make
  ```

- **生产模式** (推荐)：
  ```bash
  make CFLAGS="-O3 -march=native -DNDEBUG"
  ```

### 4. 安装

```bash
sudo make install
```

这将把 `anx` 可执行文件安装到 `/usr/local/bin/` 目录。

### 5. 卸载

```bash
sudo make uninstall
```

## 配置文件

ANX 支持多种配置方式：

### 1. 命令行参数 (推荐)

ANX 支持直接通过命令行参数启动，无需配置文件：

```bash
# 启动静态文件服务器
./anx --static-dir /var/www/html --port 8080

# 反向代理 + 静态文件
./anx --static-dir /var/www/html --proxy /api http://backend:8080 --port 80

# SSL加密服务器
./anx --static-dir /var/www/html --ssl-cert cert.pem --ssl-key key.pem --port 443
```

### 2. TOML格式配置文件

ANX 使用 TOML 格式的配置文件，示例如下：

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

### 3. Nginx兼容配置文件

ANX 也支持 Nginx 风格的配置文件：

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

## 启动服务器

### 1. 使用命令行参数启动

```bash
# 启动静态文件服务器
./anx --static-dir /var/www/html --port 8080

# 启动反向代理服务器
./anx --static-dir /var/www/html --proxy /api http://backend:8080 --port 80

# 启动SSL服务器
./anx --static-dir /var/www/html --ssl-cert cert.pem --ssl-key key.pem --port 443
```

### 2. 使用配置文件启动

```bash
# 使用 TOML 配置文件
./anx -c configs/anx.toml

# 使用 Nginx 配置文件
./anx -c configs/nginx.conf.example
```

### 3. 守护进程模式

```bash
# 以守护进程模式启动
./anx --static-dir /var/www/html --port 8080 --daemon
```

## 常见问题

### 1. 如何检查依赖是否安装？

```bash
make check-deps
```

### 2. 如何运行测试？

```bash
make test
```

### 3. 如何清理构建文件？

```bash
make clean
```

### 4. 如何查看并行编译信息？

```bash
make parallel-info
```

### 5. 如何格式化代码？

```bash
make format