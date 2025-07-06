#!/bin/bash

# ANX HTTP Server 状态检查脚本
# Quick status check for ANX HTTP Server

echo "========================================"
echo "🚀 ANX HTTP Server - 状态检查"
echo "========================================"
echo

# 版本信息
echo "📋 版本信息 (Version Info):"
echo "  当前版本: v0.5.0"
echo "  发布日期: 2025-01-05"
echo "  状态: Phase 1.3 COMPLETE ✅"
echo

# 编译状态
echo "🔧 编译状态 (Build Status):"
if [ -f "anx" ]; then
    echo "  ✅ 可执行文件存在"
    echo "  文件大小: $(ls -lh anx | awk '{print $5}')"
    echo "  修改时间: $(ls -l anx | awk '{print $6, $7, $8}')"
else
    echo "  ❌ 可执行文件不存在 - 请运行 'make' 编译"
fi
echo

# 配置文件
echo "📝 配置文件 (Configuration):"
if [ -f "server.conf" ]; then
    echo "  ✅ server.conf 存在"
    echo "  配置行数: $(wc -l < server.conf)"
else
    echo "  ❌ server.conf 不存在"
fi
echo

# 日志目录
echo "📊 日志系统 (Logging System):"
if [ -d "logs" ]; then
    echo "  ✅ logs/ 目录存在"
    if [ -f "logs/access.log" ]; then
        echo "  ✅ 访问日志: $(wc -l < logs/access.log) 行 ($(ls -lh logs/access.log | awk '{print $5}'))"
    else
        echo "  ⚪ 访问日志: 暂无"
    fi
    if [ -f "logs/error.log" ]; then
        echo "  ✅ 错误日志: $(wc -l < logs/error.log) 行 ($(ls -lh logs/error.log | awk '{print $5}'))"
    else
        echo "  ⚪ 错误日志: 暂无"
    fi
else
    echo "  ❌ logs/ 目录不存在"
fi
echo

# 已实现功能
echo "✨ 已实现功能 (Implemented Features):"
echo "  ✅ HTTP/HTTPS 基础服务"
echo "  ✅ 多进程架构"
echo "  ✅ 虚拟主机支持"
echo "  ✅ 静态文件服务"
echo "  ✅ 反向代理 (v0.3.0)"
echo "  ✅ HTTP 头部操作 (v0.4.0)"
echo "  ✅ 访问日志系统 (v0.5.0)"
echo "  ✅ 性能监控 (v0.5.0)"
echo "  ✅ 日志轮转 (v0.5.0)"
echo

# 测试脚本
echo "🧪 测试脚本 (Test Scripts):"
if [ -f "test_logging_demo.sh" ]; then
    echo "  ✅ test_logging_demo.sh - 日志系统测试"
fi
if [ -f "test_headers_demo.sh" ]; then
    echo "  ✅ test_headers_demo.sh - 头部操作测试"
fi
echo

# 快速命令
echo "⚡ 快速命令 (Quick Commands):"
echo "  编译: make clean && make"
echo "  启动: ./anx -c server.conf"
echo "  测试: ./test_logging_demo.sh"
echo "  日志: tail -f logs/access.log"
echo

# 下一步开发
echo "🎯 下一步开发 (Next Development):"
echo "  Phase 1.4: 压缩支持 (Compression Support)"
echo "  - Gzip 压缩"
echo "  - 压缩级别配置"
echo "  - MIME 类型过滤"
echo "  - 客户端协商"
echo

echo "========================================" 