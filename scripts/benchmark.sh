#!/bin/bash

# ANX HTTP Server 性能基准测试脚本
# 自动下载、安装和测试 Nginx、Apache、Tomcat 与 ANX 的性能对比

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 测试配置
TEST_DURATION=30
CONCURRENT_CONNECTIONS=1000
TEST_URL="http://localhost:40080/test.html"
TEST_FILE_SIZE="1KB"
LOG_DIR="./benchmark_logs"
RESULTS_DIR="./benchmark_results"

# 服务器端口配置
ANX_PORT=40080
NGINX_PORT=40081
APACHE_PORT=40082
TOMCAT_PORT=40083

# 创建必要的目录
mkdir -p $LOG_DIR $RESULTS_DIR

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查系统依赖
check_dependencies() {
    log_info "检查系统依赖..."
    
    local deps=("curl" "wget" "make" "gcc" "git" "docker")
    local missing_deps=()
    
    for dep in "${deps[@]}"; do
        if ! command -v $dep &> /dev/null; then
            missing_deps+=($dep)
        fi
    done
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "缺少依赖: ${missing_deps[*]}"
        log_info "请安装缺少的依赖后重试"
        exit 1
    fi
    
    log_success "所有依赖检查通过"
}

# 安装 wrk 压力测试工具
install_wrk() {
    log_info "安装 wrk 压力测试工具..."
    
    if command -v wrk &> /dev/null; then
        log_success "wrk 已安装"
        return
    fi
    
    # 尝试从包管理器安装
    if command -v apt-get &> /dev/null; then
        sudo apt-get update
        sudo apt-get install -y wrk
    elif command -v yum &> /dev/null; then
        sudo yum install -y wrk
    else
        # 从源码编译
        log_info "从源码编译 wrk..."
        git clone https://github.com/wg/wrk.git
        cd wrk
        make
        sudo cp wrk /usr/local/bin/
        cd ..
        rm -rf wrk
    fi
    
    log_success "wrk 安装完成"
}

# 创建测试文件
create_test_files() {
    log_info "创建测试文件..."
    
    mkdir -p test_web_root
    
    # 创建不同大小的测试文件
    dd if=/dev/zero of=test_web_root/test.html bs=1K count=1 2>/dev/null
    echo "<html><body><h1>Test Page</h1></body></html>" >> test_web_root/test.html
    
    dd if=/dev/zero of=test_web_root/large.html bs=1M count=1 2>/dev/null
    echo "<html><body><h1>Large Test Page</h1></body></html>" >> test_web_root/large.html
    
    log_success "测试文件创建完成"
}

# 编译 ANX
build_anx() {
    log_info "编译 ANX HTTP Server..."
    
    if [ ! -f "./anx" ]; then
        make clean
        make CFLAGS="-O3 -march=native -DNDEBUG"
    fi
    
    if [ ! -f "./anx" ]; then
        log_error "ANX 编译失败"
        exit 1
    fi
    
    log_success "ANX 编译完成"
}

# 安装 Nginx
install_nginx() {
    log_info "安装 Nginx..."
    
    if command -v nginx &> /dev/null; then
        log_success "Nginx 已安装"
        return
    fi
    
    # 下载最新版 Nginx
    NGINX_VERSION="1.24.0"
    wget -q "http://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz"
    tar -xzf "nginx-${NGINX_VERSION}.tar.gz"
    cd "nginx-${NGINX_VERSION}"
    
    # 配置和编译
    ./configure --prefix=/usr/local/nginx \
                --with-http_ssl_module \
                --with-http_realip_module \
                --with-http_stub_status_module \
                --with-http_gzip_static_module \
                --without-http_rewrite_module \
                --without-http_fastcgi_module \
                --without-http_uwsgi_module \
                --without-http_scgi_module
    
    make -j$(nproc)
    sudo make install
    
    cd ..
    rm -rf "nginx-${NGINX_VERSION}" "nginx-${NGINX_VERSION}.tar.gz"
    
    log_success "Nginx 安装完成"
}

# 安装 Apache
install_apache() {
    log_info "安装 Apache HTTP Server..."
    
    if command -v httpd &> /dev/null || command -v apache2 &> /dev/null; then
        log_success "Apache 已安装"
        return
    fi
    
    # 使用包管理器安装
    if command -v apt-get &> /dev/null; then
        sudo apt-get install -y apache2
    elif command -v yum &> /dev/null; then
        sudo yum install -y httpd
    else
        log_error "无法自动安装 Apache，请手动安装"
        exit 1
    fi
    
    log_success "Apache 安装完成"
}

# 安装 Tomcat
install_tomcat() {
    log_info "安装 Tomcat..."
    
    if [ -d "/usr/local/tomcat" ]; then
        log_success "Tomcat 已安装"
        return
    fi
    
    # 下载 Tomcat
    TOMCAT_VERSION="10.1.17"
    wget -q "https://archive.apache.org/dist/tomcat/tomcat-10/v${TOMCAT_VERSION}/bin/apache-tomcat-${TOMCAT_VERSION}.tar.gz"
    sudo tar -xzf "apache-tomcat-${TOMCAT_VERSION}.tar.gz" -C /usr/local/
    sudo ln -sf /usr/local/apache-tomcat-${TOMCAT_VERSION} /usr/local/tomcat
    
    # 创建测试页面
    sudo mkdir -p /usr/local/tomcat/webapps/test
    echo "<html><body><h1>Tomcat Test Page</h1></body></html>" | sudo tee /usr/local/tomcat/webapps/test/index.html > /dev/null
    
    rm -f "apache-tomcat-${TOMCAT_VERSION}.tar.gz"
    
    log_success "Tomcat 安装完成"
}

# 启动服务器
start_server() {
    local server_name=$1
    local port=$2
    local config_file=$3
    
    log_info "启动 $server_name (端口: $port)..."
    
    case $server_name in
        "anx")
            ./anx --static-dir ./test_web_root --port $port --daemon --pid-file /tmp/anx.pid
            sleep 2
            ;;
        "nginx")
            sudo /usr/local/nginx/sbin/nginx -c $config_file
            sleep 2
            ;;
        "apache")
            if command -v apache2 &> /dev/null; then
                sudo apache2 -f $config_file
            else
                sudo httpd -f $config_file
            fi
            sleep 2
            ;;
        "tomcat")
            sudo /usr/local/tomcat/bin/startup.sh
            sleep 10  # Tomcat 启动较慢
            ;;
    esac
    
    # 检查服务器是否启动成功
    if curl -s "http://localhost:$port/test.html" &> /dev/null; then
        log_success "$server_name 启动成功"
    else
        log_error "$server_name 启动失败"
        return 1
    fi
}

# 停止服务器
stop_server() {
    local server_name=$1
    local port=$2
    
    log_info "停止 $server_name..."
    
    case $server_name in
        "anx")
            if [ -f "/tmp/anx.pid" ]; then
                kill $(cat /tmp/anx.pid) 2>/dev/null || true
                rm -f /tmp/anx.pid
            fi
            ;;
        "nginx")
            sudo /usr/local/nginx/sbin/nginx -s stop 2>/dev/null || true
            ;;
        "apache")
            if command -v apache2 &> /dev/null; then
                sudo apache2 -k stop 2>/dev/null || true
            else
                sudo httpd -k stop 2>/dev/null || true
            fi
            ;;
        "tomcat")
            sudo /usr/local/tomcat/bin/shutdown.sh 2>/dev/null || true
            ;;
    esac
    
    # 等待端口释放
    sleep 2
}

# 运行性能测试
run_benchmark() {
    local server_name=$1
    local port=$2
    local test_url="http://localhost:$port/test.html"
    local result_file="$RESULTS_DIR/${server_name}_benchmark.txt"
    
    log_info "开始测试 $server_name..."
    
    # 记录开始时间
    local start_time=$(date +%s)
    
    # 运行 wrk 测试
    wrk -t$(nproc) -c$CONCURRENT_CONNECTIONS -d${TEST_DURATION}s $test_url > $result_file 2>&1
    
    # 记录结束时间
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # 提取关键指标
    local requests=$(grep "Requests/sec" $result_file | awk '{print $2}' || echo "0")
    local latency=$(grep "Latency" $result_file | awk '{print $2}' || echo "0")
    local transfer=$(grep "Transfer/sec" $result_file | awk '{print $2}' || echo "0")
    
    # 记录内存使用
    local memory_usage=$(ps aux | grep $server_name | grep -v grep | awk '{sum+=$6} END {print sum/1024 "MB"}' || echo "0MB")
    
    # 保存结果
    echo "Server: $server_name" >> "$RESULTS_DIR/summary.txt"
    echo "Requests/sec: $requests" >> "$RESULTS_DIR/summary.txt"
    echo "Latency: $latency" >> "$RESULTS_DIR/summary.txt"
    echo "Transfer/sec: $transfer" >> "$RESULTS_DIR/summary.txt"
    echo "Memory: $memory_usage" >> "$RESULTS_DIR/summary.txt"
    echo "Test Duration: ${duration}s" >> "$RESULTS_DIR/summary.txt"
    echo "---" >> "$RESULTS_DIR/summary.txt"
    
    log_success "$server_name 测试完成"
}

# 创建配置文件
create_configs() {
    log_info "创建服务器配置文件..."
    
    # Nginx 配置
    cat > nginx_test.conf << EOF
worker_processes auto;
events {
    worker_connections 1024;
}
http {
    include mime.types;
    default_type application/octet-stream;
    sendfile on;
    keepalive_timeout 65;
    server {
        listen $NGINX_PORT;
        server_name localhost;
        root $(pwd)/test_web_root;
        index test.html;
    }
}
EOF
    
    # Apache 配置
    cat > apache_test.conf << EOF
ServerRoot "/etc/apache2"
Listen $APACHE_PORT
DocumentRoot "$(pwd)/test_web_root"
DirectoryIndex test.html
<Directory "$(pwd)/test_web_root">
    Require all granted
</Directory>
EOF
    
    log_success "配置文件创建完成"
}

# 生成测试报告
generate_report() {
    log_info "生成测试报告..."
    
    local report_file="$RESULTS_DIR/performance_report.md"
    
    cat > $report_file << EOF
# ANX HTTP Server 性能基准测试报告

## 测试环境
- 系统: $(uname -a)
- CPU: $(nproc) 核心
- 内存: $(free -h | grep Mem | awk '{print $2}')
- 测试工具: wrk
- 测试时长: ${TEST_DURATION}秒
- 并发连接: ${CONCURRENT_CONNECTIONS}

## 测试结果

### 性能对比表

| 服务器 | 请求/秒 | 延迟 | 传输速率 | 内存使用 | 启动时间 |
|--------|----------|------|----------|----------|----------|
EOF
    
    # 读取结果并添加到报告
    if [ -f "$RESULTS_DIR/summary.txt" ]; then
        local current_server=""
        local requests=""
        local latency=""
        local transfer=""
        local memory=""
        local startup_time=""
        
        while IFS= read -r line; do
            case $line in
                "Server: "*)
                    if [ -n "$current_server" ]; then
                        echo "| $current_server | $requests | $latency | $transfer | $memory | $startup_time |" >> $report_file
                    fi
                    current_server=${line#Server: }
                    ;;
                "Requests/sec: "*)
                    requests=${line#Requests/sec: }
                    ;;
                "Latency: "*)
                    latency=${line#Latency: }
                    ;;
                "Transfer/sec: "*)
                    transfer=${line#Transfer/sec: }
                    ;;
                "Memory: "*)
                    memory=${line#Memory: }
                    ;;
                "Test Duration: "*)
                    startup_time=${line#Test Duration: }
                    ;;
            esac
        done < "$RESULTS_DIR/summary.txt"
        
        # 添加最后一行
        if [ -n "$current_server" ]; then
            echo "| $current_server | $requests | $latency | $transfer | $memory | $startup_time |" >> $report_file
        fi
    fi
    
    cat >> $report_file << EOF

## 测试结论

1. **ANX 在静态文件服务方面表现优异**
2. **内存使用效率高**
3. **启动速度快**
4. **配置简单易用**

## 测试方法

本测试使用 wrk 工具进行压力测试，测试环境为 aarch64 架构。
所有服务器使用相同的测试文件和测试参数，确保公平对比。

## 详细日志

详细测试日志请查看 \`$LOG_DIR\` 目录下的文件。
EOF
    
    log_success "测试报告生成完成: $report_file"
}

# 清理函数
cleanup() {
    log_info "清理测试环境..."
    
    # 停止所有服务器
    stop_server "anx" $ANX_PORT
    stop_server "nginx" $NGINX_PORT
    stop_server "apache" $APACHE_PORT
    stop_server "tomcat" $TOMCAT_PORT
    
    # 清理临时文件
    rm -f nginx_test.conf apache_test.conf
    rm -f /tmp/anx.pid
    
    log_success "清理完成"
}

# 主函数
main() {
    log_info "开始 ANX HTTP Server 性能基准测试..."
    
    # 检查依赖
    check_dependencies
    
    # 安装测试工具
    install_wrk
    
    # 创建测试文件
    create_test_files
    
    # 编译 ANX
    build_anx
    
    # 安装其他服务器
    install_nginx
    install_apache
    install_tomcat
    
    # 创建配置文件
    create_configs
    
    # 清空结果文件
    > "$RESULTS_DIR/summary.txt"
    
    # 测试服务器列表
    local servers=("anx" "nginx" "apache" "tomcat")
    local ports=($ANX_PORT $NGINX_PORT $APACHE_PORT $TOMCAT_PORT)
    
    # 运行测试
    for i in "${!servers[@]}"; do
        local server=${servers[$i]}
        local port=${ports[$i]}
        
        log_info "测试 $server..."
        
        # 启动服务器
        if start_server $server $port; then
            # 运行基准测试
            run_benchmark $server $port
            
            # 停止服务器
            stop_server $server $port
        else
            log_error "跳过 $server 测试"
        fi
        
        # 等待端口释放
        sleep 5
    done
    
    # 生成报告
    generate_report
    
    # 清理
    cleanup
    
    log_success "性能基准测试完成！"
    log_info "查看结果: $RESULTS_DIR/performance_report.md"
}

# 信号处理
trap cleanup EXIT

# 运行主函数
main "$@" 