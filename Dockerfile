# ANX HTTP Server Docker Image
FROM ubuntu:24.04

# 设置环境变量
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# 安装必要的运行时依赖
RUN apt-get update && apt-get install -y \
    libssl3 \
    libz1 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# 创建运行用户
RUN useradd -r -s /bin/false -d /var/www anxuser

# 创建必要的目录
RUN mkdir -p /var/www/html \
    && mkdir -p /var/log/anx \
    && mkdir -p /etc/anx \
    && chown -R anxuser:anxuser /var/www \
    && chown -R anxuser:anxuser /var/log/anx

# 复制二进制文件
COPY anx /usr/local/bin/anx
RUN chmod +x /usr/local/bin/anx

# 创建默认配置
RUN cat > /etc/anx/anx.conf << 'EOF'
# ANX HTTP Server配置
worker_processes 4;
error_log /var/log/anx/error.log;
access_log /var/log/anx/access.log;

http {
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    keepalive_timeout 65;
    
    gzip on;
    gzip_vary on;
    gzip_min_length 1000;
    gzip_comp_level 6;
    gzip_types text/plain text/css text/xml text/javascript application/json application/javascript;
    
    server {
        listen 80;
        server_name _;
        root /var/www/html;
        index index.html index.htm;
        
        location / {
            try_files $uri $uri/ =404;
        }
    }
}
EOF

# 创建默认主页
RUN cat > /var/www/html/index.html << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>ANX HTTP Server - Performance Test</title>
    <meta charset="utf-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .highlight { color: #e74c3c; font-weight: bold; }
        .feature { background: #f8f9fa; padding: 10px; margin: 10px 0; border-radius: 5px; }
    </style>
</head>
<body>
    <h1>🚀 ANX HTTP Server</h1>
    <p class="highlight">High-Performance HTTP Server with Assembly Optimizations</p>
    
    <div class="feature">
        <h3>⚡ Assembly Optimizations</h3>
        <ul>
            <li>NEON SIMD vectorization</li>
            <li>AES hardware acceleration</li>
            <li>CRC32 hardware checksum</li>
            <li>SHA1/SHA2 crypto acceleration</li>
        </ul>
    </div>
    
    <div class="feature">
        <h3>🏗️ Architecture</h3>
        <ul>
            <li>Event-driven I/O</li>
            <li>Multi-process workers</li>
            <li>Zero-copy optimizations</li>
            <li>Memory pool management</li>
        </ul>
    </div>
    
    <p><a href="/status">Server Status</a></p>
</body>
</html>
EOF

# 设置权限
RUN chown -R anxuser:anxuser /var/www/html

# 暴露端口
EXPOSE 80

# 设置用户
USER anxuser

# 启动命令
CMD ["/usr/local/bin/anx", "-c", "/etc/anx/anx.conf"] 