# ANX HTTP Server v1.1.0+ 快速启动指南

## 🚀 一键启动体验

ANX v1.1.0+ 引入了革命性的命令行参数系统，让启动HTTP服务器变得比nginx更简单！

### 基础用法

#### 1. 静态文件服务器
```bash
# 最简单的用法 - 启动静态文件服务器
./anx --static-dir /var/www/html --port 8080

# 指定主机绑定
./anx --static-dir /var/www/html --port 8080 --host 0.0.0.0
```

#### 2. 反向代理
```bash
# 静态文件 + 反向代理
./anx --static-dir /var/www/html --proxy /api http://backend:8080 --port 80

# 多个反向代理
./anx --static-dir /var/www/html \
      --proxy /api http://backend:8080 \
      --proxy /admin http://admin:3000 \
      --port 80
```

#### 3. SSL/HTTPS 服务器
```bash
# SSL 配置
./anx --static-dir /var/www/html \
      --ssl-cert /path/to/cert.pem \
      --ssl-key /path/to/key.pem \
      --port 443
```

#### 4. 高性能配置
```bash
# 启用缓存和优化
./anx --static-dir /var/www/html \
      --cache-size 500MB \
      --cache-ttl 3600 \
      --threads 8 \
      --max-connections 10000 \
      --port 8080
```

#### 5. 生产环境配置
```bash
# 守护进程模式
./anx --static-dir /var/www/html \
      --daemon \
      --pid-file /var/run/anx.pid \
      --log-level info \
      --log-file /var/log/anx.log \
      --port 80
```

## 📋 命令行参数参考

### 基础参数
| 参数 | 说明 | 示例 |
|------|------|------|
| `--static-dir` | 静态文件目录 | `--static-dir /var/www/html` |
| `--port` | 监听端口 | `--port 8080` |
| `--host` | 监听主机 | `--host 0.0.0.0` |

### 反向代理参数
| 参数 | 说明 | 示例 |
|------|------|------|
| `--proxy` | 反向代理配置 | `--proxy /api http://backend:8080` |

### SSL参数
| 参数 | 说明 | 示例 |
|------|------|------|
| `--ssl-cert` | SSL证书文件 | `--ssl-cert cert.pem` |
| `--ssl-key` | SSL私钥文件 | `--ssl-key key.pem` |

### 缓存参数
| 参数 | 说明 | 示例 |
|------|------|------|
| `--cache-size` | 缓存大小 | `--cache-size 100MB` |
| `--cache-ttl` | 缓存TTL(秒) | `--cache-ttl 3600` |

### 性能参数
| 参数 | 说明 | 示例 |
|------|------|------|
| `--threads` | 工作线程数 | `--threads 4` |
| `--max-connections` | 最大连接数 | `--max-connections 1000` |

### 日志参数
| 参数 | 说明 | 示例 |
|------|------|------|
| `--log-level` | 日志级别 | `--log-level info` |
| `--log-file` | 日志文件 | `--log-file access.log` |

### 进程参数
| 参数 | 说明 | 示例 |
|------|------|------|
| `--daemon` | 守护进程模式 | `--daemon` |
| `--pid-file` | PID文件路径 | `--pid-file /var/run/anx.pid` |

## 🔧 配置文件方式

虽然命令行参数更简单，但ANX仍然支持传统的配置文件方式：

### TOML配置文件
```toml
# configs/anx.toml
[server]
port = 8080
host = "0.0.0.0"
static_dir = "/var/www/html"

[[server.locations]]
path = "/api"
proxy_pass = "http://backend:8080"

[server.ssl]
cert_file = "cert.pem"
key_file = "key.pem"

[server.cache]
size = "500MB"
ttl = 3600

[server.logging]
level = "info"
file = "access.log"
```

### Nginx兼容配置
```nginx
# configs/nginx.conf
http {
    server {
        listen 8080;
        server_name localhost;
        
        root /var/www/html;
        
        location /api {
            proxy_pass http://backend:8080;
        }
    }
}
```

## 🚀 与nginx对比

### nginx启动方式
```bash
# nginx需要配置文件
sudo nginx -c /etc/nginx/nginx.conf

# 或者需要复杂的配置文件
cat > nginx.conf << EOF
http {
    server {
        listen 8080;
        root /var/www/html;
    }
}
EOF
nginx -c nginx.conf
```

### ANX启动方式
```bash
# 一行命令搞定！
./anx --static-dir /var/www/html --port 8080
```

## 📊 性能对比

| 特性 | nginx | ANX v1.1.0+ |
|------|-------|-------------|
| 启动复杂度 | 需要配置文件 | 一行命令 |
| 配置学习成本 | 高 | 低 |
| 开发调试 | 需要重启 | 实时生效 |
| 内存占用 | 中等 | 低 |
| 并发性能 | 高 | 高 |
| 架构 | C | C/Rust混合 |

## 🛠️ 开发调试

### 开发模式
```bash
# 开发时使用详细日志
./anx --static-dir ./www \
      --port 8080 \
      --log-level debug \
      --log-file -
```

### 测试反向代理
```bash
# 启动测试后端
python3 -m http.server 3000

# 启动ANX代理
./anx --static-dir ./www \
      --proxy /api http://localhost:3000 \
      --port 8080
```

### 性能测试
```bash
# 使用ab进行压力测试
ab -n 10000 -c 100 http://localhost:8080/

# 使用wrk进行基准测试
wrk -t12 -c400 -d30s http://localhost:8080/
```

## 🔍 故障排除

### 常见问题

1. **端口被占用**
   ```bash
   # 检查端口占用
   netstat -tlnp | grep :8080
   
   # 使用其他端口
   ./anx --static-dir /var/www/html --port 8081
   ```

2. **权限问题**
   ```bash
   # 确保有读取静态文件目录的权限
   chmod 755 /var/www/html
   ```

3. **SSL证书问题**
   ```bash
   # 检查证书文件权限
   chmod 600 cert.pem key.pem
   ```

### 调试技巧

1. **查看详细日志**
   ```bash
   ./anx --static-dir /var/www/html --port 8080 --log-level debug
   ```

2. **检查配置**
   ```bash
   # 验证配置文件
   ./anx -c configs/anx.toml --validate
   ```

3. **性能监控**
   ```bash
   # 监控进程
   top -p $(pgrep anx)
   
   # 监控网络连接
   netstat -an | grep :8080
   ```

## 📚 更多资源

- [API参考文档](API_REFERENCE.md)
- [配置文档](CONFIGURATION.md)
- [架构文档](ARCHITECTURE.md)
- [开发指南](DEVELOPMENT.md)

## 🤝 贡献

欢迎提交Issue和Pull Request！

- 报告Bug: [GitHub Issues](https://github.com/neipor/asm_http_server/issues)
- 功能建议: [GitHub Discussions](https://github.com/neipor/asm_http_server/discussions)
- 代码贡献: [GitHub Pull Requests](https://github.com/neipor/asm_http_server/pulls) 