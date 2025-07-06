#!/bin/bash

# 一键执行Git清理
echo "开始执行Git清理..."

# 提交当前修改
echo "1. 提交当前修改..."
git add -A && git commit -m "feat: 完成Phase 2.2负载均衡和健康检查功能

- 实现负载均衡算法 (轮询、加权轮询、最少连接、IP哈希)
- 添加健康检查机制
- 实现缓存系统
- 优化性能和稳定性
- 更新文档和配置

版本: v0.8.0"

# 删除旧tag
echo "2. 删除旧tag..."
git tag -d v0.1.0 2>/dev/null
git tag -d v0.2.0-c-version 2>/dev/null
git tag -d v0.3.0-epoll 2>/dev/null
git tag -d v0.4.0-static-files 2>/dev/null
git tag -d v0.5.0-configurable 2>/dev/null
git tag -d v0.6.0-multi-process 2>/dev/null

# 创建新tag
echo "3. 创建新tag..."
git tag -a v0.8.0 -m "ANX HTTP Server v0.8.0 - 负载均衡与健康检查"

# 切换到master
echo "4. 切换到master..."
git checkout master && git pull origin master

# 合并分支
echo "5. 合并分支..."
git merge feature/phase-1.4-compression --no-ff -m "Merge feature/phase-1.4-compression: 完成Phase 2.2负载均衡功能"

# 删除测试文件
echo "6. 删除测试文件..."
rm -f test_load_balancer_demo.sh test_cache_demo.sh test_compression_demo.sh test_logging_demo.sh test_headers_demo.sh check_compression.sh check_status.sh

# 删除临时文档
echo "7. 删除临时文档..."
rm -f PHASE_1_3_SUMMARY.md PHASE_1_4_SUMMARY.md PHASE_2_1_SUMMARY.md PHASE_2_2_SUMMARY.md PROJECT_STATUS_REPORT.md GIT_CLEANUP_GUIDE.md

# 提交删除操作
echo "8. 提交删除操作..."
git add -A && git commit -m "cleanup: 删除无用测试文件和临时文档"

# 推送到远程
echo "9. 推送到远程..."
git push origin master && git push origin --tags

# 删除已合并分支
echo "10. 删除已合并分支..."
git branch -d feature/phase-1.4-compression 2>/dev/null
git push origin --delete feature/phase-1.4-compression 2>/dev/null

echo "✅ Git清理完成！"
echo "当前分支: $(git branch --show-current)"
echo "现有tag: $(git tag -l | tr '\n' ' ')" 