# ANX HTTP Server 配置系统

本文档介绍了ANX HTTP Server的新一代配置系统，基于Rust实现，支持TOML和Nginx兼容配置格式。

## 概述

ANX v1.1.0+ 引入了全新的配置系统，具有以下特性：

- **类型安全**：基于Rust的类型系统，提供编译时配置验证
- **TOML支持**：使用现代的TOML格式，易于阅读和编写
- **Nginx兼容**：支持读取和转换Nginx风格的配置文件
- **FFI集成**：通过FFI接口与C代码无缝集成

## 配置格式

### TOML 配置格式

ANX使用TOML作为主要配置格式。示例配置文件：

```toml
# ANX HTTP Server Configuration

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

### Nginx 兼容配置

ANX也支持读取Nginx风格的配置文件：

```nginx
worker_processes auto;

events {
    worker_connections 1024;
}

http {
    server {
        listen 80;
        server_name example.com;
        root /var/www/html;
        index index.html index.htm;
        
        location / {
            try_files $uri $uri/ =404;
        }
        
        location /api {
            proxy_pass http://backend;
        }
    }
}
```

## 配置字段详解

### [server] 部分

- `listen`: 监听端口列表（必需）
- `server_name`: 服务器域名（可选）
- `root`: 文档根目录（必需）
- `index`: 默认索引文件列表（必需）
- `worker_processes`: 工作进程数（可选，默认为1）
- `worker_connections`: 每个工作进程的连接数（可选，默认为1024）
- `keepalive_timeout`: 保持连接超时时间（可选，默认为65秒）
- `client_max_body_size`: 客户端最大请求体大小（可选，默认为1M）

### [logging] 部分

- `access_log`: 访问日志文件路径（可选）
- `error_log`: 错误日志文件路径（可选）
- `log_level`: 日志级别（可选，默认为info）

### [ssl] 部分

- `certificate`: SSL证书文件路径（必需）
- `private_key`: SSL私钥文件路径（必需）
- `protocols`: 支持的SSL/TLS协议列表（可选）
- `ciphers`: 支持的加密套件（可选）

### [proxy] 部分

- `proxy_timeout`: 代理超时时间（可选，默认为60秒）
- `proxy_connect_timeout`: 代理连接超时时间（可选，默认为5秒）
- `upstream`: 上游服务器组配置

### [cache] 部分

- `enabled`: 是否启用缓存（可选，默认为false）
- `max_size`: 缓存最大大小（可选）
- `ttl`: 缓存生存时间（可选，默认为3600秒）
- `keys_zone`: 缓存密钥区域配置（可选）

### [[locations]] 部分

- `path`: 位置路径（必需）
- `root`: 该位置的根目录（可选）
- `index`: 该位置的索引文件（可选）
- `proxy_pass`: 代理转发地址（可选）
- `return_code`: 返回HTTP状态码（可选）
- `return_url`: 返回重定向URL（可选）
- `try_files`: 尝试文件列表（可选）
- `headers`: 自定义HTTP头部（可选）

## 配置文件使用

### 启动服务器

使用TOML配置文件：
```bash
./anx --config configs/anx.toml
```

使用Nginx配置文件：
```bash
./anx --config configs/nginx.conf
```

### 配置验证

验证配置文件语法：
```bash
./anx --test-config configs/anx.toml
```

### 配置转换

将Nginx配置转换为TOML格式：
```bash
./anx --convert-config nginx.conf > anx.toml
```

## 配置最佳实践

1. **使用TOML格式**：推荐使用TOML格式，它更易于阅读和维护
2. **分组配置**：将相关的配置项组织在一起
3. **使用注释**：为复杂的配置项添加注释
4. **环境分离**：为不同环境（开发、测试、生产）使用不同的配置文件
5. **配置验证**：在部署前验证配置文件的正确性

## 常见问题

### Q: 如何迁移现有的Nginx配置？

A: ANX提供了Nginx兼容层，可以直接使用现有的Nginx配置文件。也可以使用转换工具将Nginx配置转换为TOML格式。

### Q: 配置更改后是否需要重启服务？

A: 是的，配置更改后需要重启服务才能生效。未来版本将支持热重载配置。

### Q: 如何处理配置文件中的敏感信息？

A: 推荐使用环境变量或外部密钥管理系统来处理敏感信息，如SSL私钥路径等。

### Q: 配置文件的优先级是什么？

A: 命令行参数 > 配置文件 > 默认值

## 配置示例

更多配置示例请参考 `configs/` 目录下的示例文件：

- `configs/anx.toml` - 完整的TOML配置示例
- `configs/nginx.conf.example` - Nginx兼容配置示例
- `configs/minimal.toml` - 最小化配置示例
- `configs/ssl.toml` - SSL配置示例
- `configs/proxy.toml` - 代理配置示例

## 相关文档

- [架构文档](ARCHITECTURE.md)
- [开发路线图](../ROADMAP.md)
- [安装指南](INSTALLATION.md) 