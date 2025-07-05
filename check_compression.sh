#!/bin/bash

# ANX HTTP Server 压缩功能检查脚本
# Compression Feature Check Script

echo "========================================"
echo "🔍 ANX HTTP Server - 压缩功能检查"
echo "========================================"
echo

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 检查zlib库
echo "📚 检查zlib依赖..."
if ldconfig -p | grep -q libz.so; then
    echo -e "${GREEN}✓ zlib库已安装${NC}"
    zlib_version=$(ldconfig -p | grep libz.so | head -n1 | grep -o 'libz.so.[0-9.]*' | cut -d. -f3-)
    echo "  版本: $zlib_version"
else
    echo -e "${RED}✗ 未找到zlib库${NC}"
    echo "  请安装zlib开发库:"
    echo "  Ubuntu/Debian: sudo apt-get install zlib1g-dev"
    echo "  CentOS/RHEL: sudo yum install zlib-devel"
fi
echo

# 检查压缩相关源文件
echo "📁 检查源代码文件..."
files_to_check=("src/compress.h" "src/compress.c")
all_files_present=true

for file in "${files_to_check[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${GREEN}✓ $file 存在${NC}"
        echo "  大小: $(ls -lh "$file" | awk '{print $5}')"
        echo "  修改时间: $(ls -l "$file" | awk '{print $6, $7, $8}')"
    else
        echo -e "${RED}✗ $file 不存在${NC}"
        all_files_present=false
    fi
done
echo

# 检查配置文件中的压缩设置
echo "⚙️ 检查压缩配置..."
if [ -f "server.conf" ]; then
    echo -e "${GREEN}✓ server.conf 存在${NC}"
    echo "压缩相关配置:"
    echo "----------------------------------------"
    grep -A 10 "gzip" server.conf | while read -r line; do
        if [[ $line == *"gzip"* ]]; then
            echo -e "${BLUE}$line${NC}"
        fi
    done
    echo "----------------------------------------"
else
    echo -e "${RED}✗ server.conf 不存在${NC}"
fi
echo

# 检查测试脚本
echo "🧪 检查测试脚本..."
if [ -f "test_compression_demo.sh" ]; then
    echo -e "${GREEN}✓ test_compression_demo.sh 存在${NC}"
    echo "  权限: $(ls -l test_compression_demo.sh | awk '{print $1}')"
    if [ ! -x "test_compression_demo.sh" ]; then
        echo "  提示: 脚本需要执行权限，运行: chmod +x test_compression_demo.sh"
    fi
else
    echo -e "${RED}✗ test_compression_demo.sh 不存在${NC}"
fi
echo

# 检查编译设置
echo "🔧 检查编译设置..."
if [ -f "Makefile" ]; then
    echo -e "${GREEN}✓ Makefile 存在${NC}"
    if grep -q "zlib" Makefile; then
        echo -e "${GREEN}✓ zlib编译配置已添加${NC}"
    else
        echo -e "${RED}✗ 未找到zlib编译配置${NC}"
        echo "  请确保Makefile中包含 -lz 链接选项"
    fi
else
    echo -e "${RED}✗ Makefile 不存在${NC}"
fi
echo

# 检查可执行文件
echo "📦 检查可执行文件..."
if [ -f "anx" ]; then
    echo -e "${GREEN}✓ anx 可执行文件存在${NC}"
    echo "  大小: $(ls -lh anx | awk '{print $5}')"
    echo "  修改时间: $(ls -l anx | awk '{print $6, $7, $8}')"
    
    # 检查zlib依赖
    if ldd anx | grep -q libz.so; then
        echo -e "${GREEN}✓ zlib依赖正确${NC}"
    else
        echo -e "${RED}✗ 未找到zlib依赖${NC}"
        echo "  请重新编译以包含zlib支持"
    fi
else
    echo -e "${RED}✗ anx 可执行文件不存在${NC}"
    echo "  请运行 make 编译项目"
fi
echo

# 总结
echo "📊 功能检查总结:"
if $all_files_present && [ -f "server.conf" ] && [ -f "test_compression_demo.sh" ]; then
    echo -e "${GREEN}✅ 压缩功能已正确安装${NC}"
    echo "运行以下命令测试压缩功能:"
    echo "  1. chmod +x test_compression_demo.sh"
    echo "  2. ./test_compression_demo.sh"
else
    echo -e "${RED}❌ 压缩功能安装不完整${NC}"
    echo "请检查上述错误并修复后重试"
fi

echo
echo "========================================" 