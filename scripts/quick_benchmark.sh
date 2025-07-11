#!/bin/bash

# ANX HTTP Server 快速性能测试脚本
# 快速验证 ANX 与 Nginx 的性能对比

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 测试配置
TEST_DURATION=10
CONCURRENT_CONNECTIONS=100
TEST_URL="http://localhost:40080/test.html"

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查依赖
check_dependencies() {
    log_info "检查依赖..."
    
    if ! command -v curl &> /dev/null; then
        log_error "需要 curl"
        exit 1
    fi
    
    if ! command -v wrk &> /dev/null; then
        log_info "安装 wrk..."
        if command -v apt-get &> /dev/null; then
            sudo apt-get update && sudo apt-get install -y wrk
        else
            log_error "请手动安装 wrk"
            exit 1
        fi
    fi
    
    log_success "依赖检查完成"
}

# 创建测试文件
create_test_files() {
    log_info "创建测试文件..."
    
    mkdir -p test_web_root
    echo "<html><body><h1>ANX Test Page</h1><p>Performance test page</p></body></html>" > test_web_root/test.html
    
    log_success "测试文件创建完成"
}

# 编译 ANX
build_anx() {
    log_info "编译 ANX..."
    
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
    
    if command -v apt-get &> /dev/null; then
        sudo apt-get install -y nginx
    else
        log_error "请手动安装 Nginx"
        exit 1
    fi
    
    log_success "Nginx 安装完成"
}

# 启动服务器
start_server() {
    local server_name=$1
    local port=$2
    
    log_info "启动 $server_name (端口: $port)..."
    
    case $server_name in
        "anx")
            ./anx --static-dir ./test_web_root --port $port --daemon --pid-file /tmp/anx.pid
            sleep 2
            ;;
        "nginx")
            # 创建临时配置
            cat > nginx_temp.conf << EOF
events {
    worker_connections 1024;
}
http {
    server {
        listen $port;
        root $(pwd)/test_web_root;
        index test.html;
    }
}
EOF
            sudo nginx -c $(pwd)/nginx_temp.conf
            sleep 2
            ;;
    esac
    
    # 检查服务器是否启动
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
    
    log_info "停止 $server_name..."
    
    case $server_name in
        "anx")
            if [ -f "/tmp/anx.pid" ]; then
                kill $(cat /tmp/anx.pid) 2>/dev/null || true
                rm -f /tmp/anx.pid
            fi
            ;;
        "nginx")
            sudo nginx -s stop 2>/dev/null || true
            rm -f nginx_temp.conf
            ;;
    esac
    
    sleep 2
}

# 运行性能测试
run_benchmark() {
    local server_name=$1
    local port=$2
    local test_url="http://localhost:$port/test.html"
    
    log_info "测试 $server_name..."
    
    # 运行 wrk 测试
    local result=$(wrk -t2 -c$CONCURRENT_CONNECTIONS -d${TEST_DURATION}s $test_url 2>/dev/null)
    
    # 提取关键指标
    local requests=$(echo "$result" | grep "Requests/sec" | awk '{print $2}' || echo "0")
    local latency=$(echo "$result" | grep "Latency" | awk '{print $2}' || echo "0")
    
    # 记录内存使用
    local memory_usage=$(ps aux | grep $server_name | grep -v grep | awk '{sum+=$6} END {print sum/1024 "MB"}' || echo "0MB")
    
    echo "=== $server_name 测试结果 ==="
    echo "请求/秒: $requests"
    echo "延迟: $latency"
    echo "内存使用: $memory_usage"
    echo ""
    
    # 保存结果
    echo "$server_name,$requests,$latency,$memory_usage" >> benchmark_results.csv
}

# 生成简单报告
generate_report() {
    log_info "生成测试报告..."
    
    echo "=== ANX vs Nginx 性能对比 ===" > benchmark_report.txt
    echo "测试时间: $(date)" >> benchmark_report.txt
    echo "测试时长: ${TEST_DURATION}秒" >> benchmark_report.txt
    echo "并发连接: ${CONCURRENT_CONNECTIONS}" >> benchmark_report.txt
    echo "" >> benchmark_report.txt
    
    if [ -f "benchmark_results.csv" ]; then
        echo "服务器,请求/秒,延迟,内存使用" >> benchmark_report.txt
        cat benchmark_results.csv >> benchmark_report.txt
    fi
    
    log_success "报告生成完成: benchmark_report.txt"
}

# 清理
cleanup() {
    log_info "清理环境..."
    stop_server "anx"
    stop_server "nginx"
    rm -f /tmp/anx.pid nginx_temp.conf
    log_success "清理完成"
}

# 主函数
main() {
    log_info "开始快速性能测试..."
    
    # 初始化
    check_dependencies
    create_test_files
    build_anx
    install_nginx
    
    # 清空结果文件
    > benchmark_results.csv
    
    # 测试 ANX
    if start_server "anx" 40080; then
        run_benchmark "anx" 40080
        stop_server "anx"
    fi
    
    sleep 3
    
    # 测试 Nginx
    if start_server "nginx" 40081; then
        run_benchmark "nginx" 40081
        stop_server "nginx"
    fi
    
    # 生成报告
    generate_report
    
    # 清理
    cleanup
    
    log_success "快速性能测试完成！"
    log_info "查看结果: benchmark_report.txt"
}

# 信号处理
trap cleanup EXIT

# 运行主函数
main "$@" 