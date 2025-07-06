#!/bin/bash

# ANX HTTP Server Git 清理脚本 - 安全版本
# 功能：分步执行，每步都需要确认

echo "=== ANX HTTP Server Git 清理脚本 (安全版本) ==="
echo "这个脚本将分步执行，每步都会询问是否继续"
echo ""

# 函数：询问是否继续
confirm() {
    read -p "$1 (y/n): " -n 1 -r
    echo
    [[ $REPLY =~ ^[Yy]$ ]]
}

# 函数：显示当前状态
show_status() {
    echo "当前状态:"
    echo "- 目录: $(pwd)"
    echo "- 分支: $(git branch --show-current)"
    echo "- 最新提交: $(git log --oneline -1)"
    echo ""
}

show_status

# 步骤1：显示当前修改
echo "步骤1: 显示当前修改"
git status
echo ""
if confirm "是否要提交当前修改？"; then
    git add -A
    git commit -m "feat: 完成Phase 2.2负载均衡和健康检查功能

- 实现负载均衡算法 (轮询、加权轮询、最少连接、IP哈希)
- 添加健康检查机制
- 实现缓存系统
- 优化性能和稳定性
- 更新文档和配置

版本: v0.8.0"
    echo "✅ 提交完成"
else
    echo "⏭️  跳过提交"
fi
echo ""

# 步骤2：显示现有tag
echo "步骤2: 显示现有tag"
git tag -l
echo ""
if confirm "是否要清理旧的tag？"; then
    OLD_TAGS=("v0.1.0" "v0.2.0-c-version" "v0.3.0-epoll" "v0.4.0-static-files" "v0.5.0-configurable" "v0.6.0-multi-process")
    
    for tag in "${OLD_TAGS[@]}"; do
        if git tag -l | grep -q "^$tag$"; then
            echo "删除tag: $tag"
            git tag -d "$tag"
            git push origin :refs/tags/"$tag" 2>/dev/null || echo "远程tag $tag 不存在或已删除"
        fi
    done
    echo "✅ 旧tag清理完成"
else
    echo "⏭️  跳过tag清理"
fi
echo ""

# 步骤3：创建新tag
echo "步骤3: 创建新tag"
if confirm "是否要创建新的v0.8.0 tag？"; then
    git tag -a "v0.8.0" -m "ANX HTTP Server v0.8.0 - 负载均衡与健康检查

主要功能:
- 多种负载均衡算法支持
- 健康检查机制
- 缓存系统
- 性能优化
- 配置增强

发布日期: $(date '+%Y-%m-%d')
分支: feature/phase-1.4-compression"
    echo "✅ 创建tag v0.8.0成功"
else
    echo "⏭️  跳过创建tag"
fi
echo ""

# 步骤4：切换到master分支
echo "步骤4: 切换到master分支"
if confirm "是否要切换到master分支？"; then
    git checkout master
    git pull origin master
    echo "✅ 切换到master分支完成"
else
    echo "⏭️  跳过切换分支"
fi
echo ""

# 步骤5：合并feature分支
echo "步骤5: 合并feature分支"
if confirm "是否要合并feature/phase-1.4-compression分支？"; then
    git merge feature/phase-1.4-compression --no-ff -m "Merge feature/phase-1.4-compression: 完成Phase 2.2负载均衡功能

合并功能:
- 负载均衡算法实现
- 健康检查机制
- 缓存系统
- 性能优化
- 文档更新

版本: v0.8.0"
    echo "✅ 分支合并完成"
else
    echo "⏭️  跳过分支合并"
fi
echo ""

# 步骤6：删除测试文件
echo "步骤6: 删除测试文件"
TEST_FILES=("test_load_balancer_demo.sh" "test_cache_demo.sh" "test_compression_demo.sh" "test_logging_demo.sh" "test_headers_demo.sh" "check_compression.sh" "check_status.sh")

echo "将要删除的测试文件:"
for file in "${TEST_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "  - $file"
    fi
done
echo ""

if confirm "是否要删除这些测试文件？"; then
    for file in "${TEST_FILES[@]}"; do
        if [ -f "$file" ]; then
            echo "删除: $file"
            rm "$file"
        fi
    done
    echo "✅ 测试文件删除完成"
else
    echo "⏭️  跳过删除测试文件"
fi
echo ""

# 步骤7：删除临时文档
echo "步骤7: 删除临时文档"
TEMP_DOCS=("PHASE_1_3_SUMMARY.md" "PHASE_1_4_SUMMARY.md" "PHASE_2_1_SUMMARY.md" "PHASE_2_2_SUMMARY.md" "PROJECT_STATUS_REPORT.md" "GIT_CLEANUP_GUIDE.md")

echo "将要删除的临时文档:"
for file in "${TEMP_DOCS[@]}"; do
    if [ -f "$file" ]; then
        echo "  - $file"
    fi
done
echo ""

if confirm "是否要删除这些临时文档？"; then
    for file in "${TEMP_DOCS[@]}"; do
        if [ -f "$file" ]; then
            echo "删除: $file"
            rm "$file"
        fi
    done
    echo "✅ 临时文档删除完成"
else
    echo "⏭️  跳过删除临时文档"
fi
echo ""

# 步骤8：提交删除操作
echo "步骤8: 提交删除操作"
if confirm "是否要提交删除操作？"; then
    git add -A
    git commit -m "cleanup: 删除无用测试文件和临时文档

- 删除测试脚本文件
- 删除临时文档文件
- 清理项目结构
- 为正式发布做准备"
    echo "✅ 删除操作提交完成"
else
    echo "⏭️  跳过提交删除操作"
fi
echo ""

# 步骤9：推送到远程仓库
echo "步骤9: 推送到远程仓库"
if confirm "是否要推送到远程仓库？"; then
    git push origin master
    git push origin --tags
    echo "✅ 推送完成"
else
    echo "⏭️  跳过推送"
fi
echo ""

# 步骤10：清理分支
echo "步骤10: 清理已合并的分支"
if confirm "是否要删除已合并的feature分支？"; then
    git branch -d feature/phase-1.4-compression
    git push origin --delete feature/phase-1.4-compression
    echo "✅ 分支清理完成"
else
    echo "⏭️  跳过分支清理"
fi
echo ""

# 最终状态
echo "=== 清理完成 ==="
show_status
echo "现有tag: $(git tag -l | tr '\n' ' ')"
echo ""
echo "建议下一步:"
echo "1. 检查项目功能: make clean && make"
echo "2. 测试服务器: ./build/anx_server"
echo "3. 开始Phase 2.3开发"
echo "" 