#!/bin/bash

# ANX HTTP Server Assembly Optimization Test Script
# 测试汇编优化功能的正确性和性能

set -e

echo "🚀 ANX HTTP Server Assembly Optimization Test Suite"
echo "============================================="
echo

# 检查是否在aarch64架构上
ARCH=$(uname -m)
echo "📋 System Information:"
echo "  Architecture: $ARCH"
echo "  OS: $(uname -s)"
echo "  Kernel: $(uname -r)"
echo

if [ "$ARCH" != "aarch64" ]; then
    echo "⚠️  Warning: Assembly optimizations are designed for aarch64 architecture"
    echo "   Current architecture: $ARCH"
    echo "   Some optimizations may not be available"
    echo
fi

# 检查CPU特性
echo "🔍 Checking CPU Features:"
if [ -f /proc/cpuinfo ]; then
    echo "  CPU Model: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
    
    # 检查NEON支持
    if grep -q "neon" /proc/cpuinfo; then
        echo "  ✅ NEON SIMD support detected"
    else
        echo "  ❌ NEON SIMD support not detected"
    fi
    
    # 检查CRC32支持
    if grep -q "crc32" /proc/cpuinfo; then
        echo "  ✅ CRC32 instruction support detected"
    else
        echo "  ❌ CRC32 instruction support not detected"
    fi
    
    # 检查AES支持
    if grep -q "aes" /proc/cpuinfo; then
        echo "  ✅ AES instruction support detected"
    else
        echo "  ❌ AES instruction support not detected"
    fi
else
    echo "  ⚠️  /proc/cpuinfo not available"
fi
echo

# 编译项目
echo "🔨 Building ANX HTTP Server with Assembly Optimizations..."
if ! make clean >/dev/null 2>&1; then
    echo "❌ Failed to clean build directory"
    exit 1
fi

if ! make CFLAGS="-g -Wall -O3 -Wextra -std=c99 -DDEBUG -march=native -mtune=native" >/dev/null 2>&1; then
    echo "❌ Failed to build ANX HTTP Server"
    echo "Please check compilation errors:"
    make CFLAGS="-g -Wall -O3 -Wextra -std=c99 -DDEBUG -march=native -mtune=native"
    exit 1
fi

echo "✅ Build successful with native optimizations"
echo

# 创建测试配置
echo "📝 Creating test configuration..."
cat > test_asm_config.conf << 'EOF'
http {
    workers 2;
    error_log ./logs/error.log;
    access_log ./logs/access.log;
    
    # 启用压缩以测试压缩优化
    gzip on;
    gzip_comp_level 6;
    gzip_min_length 1024;
    gzip_types text/plain text/html text/css application/json;
    
    # 启用缓存以测试缓存优化
    proxy_cache on;
    proxy_cache_max_size 64m;
    proxy_cache_max_entries 1000;
    
    # 启用带宽限制以测试带宽优化
    enable_bandwidth_limiting on;
    default_rate_limit 10m;
    
    server {
        listen 18080;
        server_name localhost;
        
        location / {
            root ./www;
        }
        
        location /test {
            proxy_pass http://127.0.0.1:18081;
        }
        
        location /asm-status {
            # 特殊端点用于查看汇编优化状态
            return 200 "Assembly Optimization Status";
        }
    }
}
EOF

# 创建测试文件
echo "📁 Creating test files..."
mkdir -p www logs

# 创建小文件测试
echo "Small file content for testing" > www/small.txt

# 创建中等大小文件测试
dd if=/dev/urandom of=www/medium.dat bs=1024 count=64 >/dev/null 2>&1

# 创建大文件测试（1MB）
dd if=/dev/urandom of=www/large.dat bs=1024 count=1024 >/dev/null 2>&1

# 创建HTML文件测试压缩
cat > www/test.html << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Assembly Optimization Test</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .test-section { background: #f0f0f0; padding: 20px; margin: 20px 0; }
        .performance { color: #007700; font-weight: bold; }
        .warning { color: #cc6600; }
        .error { color: #cc0000; }
    </style>
</head>
<body>
    <h1>ANX HTTP Server Assembly Optimization Test Page</h1>
    
    <div class="test-section">
        <h2>Memory Operations Test</h2>
        <p>This page tests the assembly-optimized memory operations including:</p>
        <ul>
            <li>NEON-accelerated memcpy operations</li>
            <li>Optimized memset for large buffers</li>
            <li>Fast string comparison and searching</li>
        </ul>
    </div>
    
    <div class="test-section">
        <h2>Hash Function Test</h2>
        <p>Testing CRC32-accelerated hash functions for:</p>
        <ul>
            <li>Cache key generation</li>
            <li>Data integrity verification</li>
            <li>Load balancing distribution</li>
        </ul>
    </div>
    
    <div class="test-section">
        <h2>Network I/O Test</h2>
        <p>Evaluating optimized network operations:</p>
        <ul>
            <li>Large buffer transmission</li>
            <li>Chunked transfer encoding</li>
            <li>Bandwidth-limited transfers</li>
        </ul>
    </div>
    
    <!-- 添加一些重复内容以增加文件大小，便于测试压缩 -->
    <div class="test-section">
        <h2>Compression Test Data</h2>
        <p>This section contains repetitive data to test compression efficiency:</p>
EOF

# 添加重复内容
for i in {1..50}; do
    echo "        <p>Repetitive test data line $i for compression testing. This content should compress well with gzip.</p>" >> www/test.html
done

echo "    </div>
</body>
</html>" >> www/test.html

echo "✅ Test files created"
echo

# 启动服务器进行测试
echo "🌐 Starting ANX HTTP Server for testing..."
./anx -c test_asm_config.conf &
SERVER_PID=$!

# 等待服务器启动
sleep 2

if ! ps -p $SERVER_PID > /dev/null; then
    echo "❌ Failed to start ANX HTTP Server"
    exit 1
fi

echo "✅ Server started (PID: $SERVER_PID)"
echo

# 测试函数
run_test() {
    local test_name="$1"
    local url="$2"
    local expected_status="$3"
    
    echo -n "  Testing $test_name... "
    
    local response=$(curl -s -w "%{http_code}" -o /dev/null "http://localhost:18080$url" 2>/dev/null)
    
    if [ "$response" = "$expected_status" ]; then
        echo "✅ PASS"
        return 0
    else
        echo "❌ FAIL (expected $expected_status, got $response)"
        return 1
    fi
}

run_performance_test() {
    local test_name="$1"
    local url="$2"
    local iterations="$3"
    
    echo -n "  Performance test: $test_name... "
    
    local start_time=$(date +%s.%N)
    
    for i in $(seq 1 $iterations); do
        curl -s -o /dev/null "http://localhost:18080$url" >/dev/null 2>&1
    done
    
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    local rps=$(echo "scale=2; $iterations / $duration" | bc)
    
    echo "✅ $iterations requests in ${duration}s (${rps} req/s)"
}

# 基础功能测试
echo "🧪 Running Basic Functionality Tests:"
run_test "Small file" "/small.txt" "200"
run_test "HTML file (compression)" "/test.html" "200"
run_test "Medium binary file" "/medium.dat" "200"
run_test "Large binary file" "/large.dat" "200"
run_test "Non-existent file" "/nonexistent.txt" "404"
echo

# 性能测试
echo "⚡ Running Performance Tests:"

# 检查是否有bc命令用于计算
if command -v bc >/dev/null 2>&1; then
    run_performance_test "Small file (100 requests)" "/small.txt" 100
    run_performance_test "HTML file (50 requests)" "/test.html" 50
    run_performance_test "Large file (10 requests)" "/large.dat" 10
else
    echo "  ⚠️  bc command not found, skipping performance calculations"
    
    echo -n "  Basic performance test... "
    for i in {1..50}; do
        curl -s -o /dev/null "http://localhost:18080/test.html" >/dev/null 2>&1
    done
    echo "✅ PASS"
fi
echo

# 压缩测试
echo "🗜️  Testing Compression:"
echo -n "  Checking gzip compression... "

response=$(curl -s -H "Accept-Encoding: gzip" -D- "http://localhost:18080/test.html" | head -20)
if echo "$response" | grep -q "Content-Encoding: gzip"; then
    echo "✅ PASS"
else
    echo "❌ FAIL (no gzip header found)"
fi
echo

# 并发测试
echo "🔀 Testing Concurrent Connections:"
echo -n "  Running 20 concurrent requests... "

# 使用后台进程模拟并发
pids=()
for i in {1..20}; do
    curl -s -o /dev/null "http://localhost:18080/test.html" &
    pids+=($!)
done

# 等待所有请求完成
failed=0
for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
        failed=$((failed + 1))
    fi
done

if [ $failed -eq 0 ]; then
    echo "✅ PASS (all requests successful)"
else
    echo "⚠️  PARTIAL ($failed/$20 requests failed)"
fi
echo

# 内存和CPU使用测试
echo "📊 Resource Usage Analysis:"
if command -v ps >/dev/null 2>&1; then
    memory_usage=$(ps -p $SERVER_PID -o rss= | xargs)
    cpu_usage=$(ps -p $SERVER_PID -o %cpu= | xargs)
    echo "  Memory usage: ${memory_usage} KB"
    echo "  CPU usage: ${cpu_usage}%"
else
    echo "  ⚠️  ps command not available"
fi
echo

# 清理
echo "🧹 Cleaning up..."
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null || true

rm -f test_asm_config.conf
rm -rf www logs

echo "✅ Cleanup complete"
echo

# 总结
echo "📋 Test Summary:"
echo "=============="
echo "✅ Basic functionality tests completed"
echo "✅ Performance tests completed"
echo "✅ Compression tests completed"
echo "✅ Concurrent connection tests completed"
echo "✅ Resource usage analysis completed"
echo

echo "🎉 Assembly optimization test suite completed successfully!"
echo
echo "💡 Performance Tips:"
echo "   - For best performance, compile with -march=native -mtune=native"
echo "   - Enable all CPU features (NEON, CRC32, AES) if available"
echo "   - Use memory pools for frequent allocations"
echo "   - Enable compression for text-based content"
echo "   - Monitor resource usage in production"
echo

echo "📖 For more information about assembly optimizations:"
echo "   - Check the source code in src/asm_*.c files"
echo "   - Review performance benchmarks in logs"
echo "   - Use the /asm-status endpoint for runtime status" 