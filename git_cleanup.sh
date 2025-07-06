#!/bin/bash

# ANX HTTP Server Git 清理脚本
# 功能：重新命名tag，清理无用tag，合并分支，删除测试文件

echo "=== ANX HTTP Server Git 清理脚本 ==="
echo "当前目录: $(pwd)"
echo "当前分支: $(git branch --show-current)"
echo ""

# 1. 显示当前状态
echo "1. 显示当前git状态"
git status --short
echo ""

# 2. 显示现有的tag
echo "2. 现有的tag:"
git tag -l
echo ""

# 3. 显示所有分支
echo "3. 现有的分支:"
git branch -a
echo ""

# 4. 确认是否继续
read -p "是否要继续清理? (y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "取消清理操作"
    exit 1
fi

# 5. 提交当前修改
echo "5. 提交当前修改..."
git add -A
git commit -m "feat: 完成Phase 2.2负载均衡和健康检查功能

- 实现负载均衡算法 (轮询、加权轮询、最少连接、IP哈希)
- 添加健康检查机制
- 实现缓存系统
- 优化性能和稳定性
- 更新文档和配置

版本: v0.8.0"

# 6. 删除旧的无用tag
echo "6. 删除旧的无用tag..."
OLD_TAGS=(
    "v0.1.0"
    "v0.2.0-c-version"
    "v0.3.0-epoll"
    "v0.4.0-static-files"
    "v0.5.0-configurable"
    "v0.6.0-multi-process"
)

for tag in "${OLD_TAGS[@]}"; do
    if git tag -l | grep -q "^$tag$"; then
        echo "删除tag: $tag"
        git tag -d "$tag"
        git push origin :refs/tags/"$tag" 2>/dev/null || echo "远程tag $tag 不存在或已删除"
    fi
done

# 7. 重新创建有意义的tag
echo "7. 创建新的tag..."
git tag -a "v0.8.0" -m "ANX HTTP Server v0.8.0 - 负载均衡与健康检查

主要功能:
- 多种负载均衡算法支持
- 健康检查机制
- 缓存系统
- 性能优化
- 配置增强

发布日期: $(date '+%Y-%m-%d')
分支: feature/phase-1.4-compression"

# 8. 切换到master分支
echo "8. 切换到master分支..."
git checkout master
git pull origin master

# 9. 合并feature分支
echo "9. 合并feature/phase-1.4-compression分支..."
git merge feature/phase-1.4-compression --no-ff -m "Merge feature/phase-1.4-compression: 完成Phase 2.2负载均衡功能

合并功能:
- 负载均衡算法实现
- 健康检查机制
- 缓存系统
- 性能优化
- 文档更新

版本: v0.8.0"

# 10. 删除无用的测试文件
echo "10. 删除无用的测试文件..."
TEST_FILES=(
    "test_load_balancer_demo.sh"
    "test_cache_demo.sh"
    "test_compression_demo.sh"
    "test_logging_demo.sh"
    "test_headers_demo.sh"
    "check_compression.sh"
    "check_status.sh"
)

for file in "${TEST_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "删除测试文件: $file"
        rm "$file"
    fi
done

# 11. 删除临时文档文件
echo "11. 删除临时文档文件..."
TEMP_DOCS=(
    "PHASE_1_3_SUMMARY.md"
    "PHASE_1_4_SUMMARY.md"
    "PHASE_2_1_SUMMARY.md"
    "PHASE_2_2_SUMMARY.md"
    "PROJECT_STATUS_REPORT.md"
    "GIT_CLEANUP_GUIDE.md"
)

for file in "${TEMP_DOCS[@]}"; do
    if [ -f "$file" ]; then
        echo "删除临时文档: $file"
        rm "$file"
    fi
done

# 12. 提交删除操作
echo "12. 提交清理操作..."
git add -A
git commit -m "cleanup: 删除无用测试文件和临时文档

- 删除测试脚本文件
- 删除临时文档文件
- 清理项目结构
- 为正式发布做准备"

# 13. 推送到远程仓库
echo "13. 推送到远程仓库..."
git push origin master
git push origin --tags

# 14. 删除已合并的feature分支
echo "14. 删除已合并的feature分支..."
git branch -d feature/phase-1.4-compression
git push origin --delete feature/phase-1.4-compression

# 15. 清理无用的分支
echo "15. 检查其他分支..."
git branch -a

echo ""
echo "=== 清理完成 ==="
echo "最终状态:"
echo "- 当前分支: $(git branch --show-current)"
echo "- 最新提交: $(git log --oneline -1)"
echo "- 现有tag: $(git tag -l | tr '\n' ' ')"
echo ""
echo "建议下一步:"
echo "1. 检查项目功能是否正常"
echo "2. 运行编译测试: make clean && make"
echo "3. 开始Phase 2.3开发"
echo "" 