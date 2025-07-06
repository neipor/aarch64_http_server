# 手动执行Git清理 - 分步指南

由于终端问题，请按照以下步骤手动执行git清理操作。

## 步骤1：检查当前状态
```bash
# 检查当前目录
pwd

# 检查当前分支
git branch --show-current

# 检查状态
git status
```

## 步骤2：提交当前修改
```bash
# 添加所有修改
git add -A

# 提交修改
git commit -m "feat: 完成Phase 2.2负载均衡和健康检查功能

- 实现负载均衡算法 (轮询、加权轮询、最少连接、IP哈希)
- 添加健康检查机制
- 实现缓存系统
- 优化性能和稳定性
- 更新文档和配置

版本: v0.8.0"
```

## 步骤3：查看和删除旧tag
```bash
# 查看现有tag
git tag -l

# 删除旧tag（一个个删除）
git tag -d v0.1.0
git tag -d v0.2.0-c-version
git tag -d v0.3.0-epoll
git tag -d v0.4.0-static-files
git tag -d v0.5.0-configurable
git tag -d v0.6.0-multi-process

# 从远程删除tag（可能有些不存在，忽略错误）
git push origin :refs/tags/v0.1.0
git push origin :refs/tags/v0.2.0-c-version
git push origin :refs/tags/v0.3.0-epoll
git push origin :refs/tags/v0.4.0-static-files
git push origin :refs/tags/v0.5.0-configurable
git push origin :refs/tags/v0.6.0-multi-process
```

## 步骤4：创建新tag
```bash
# 创建新的v0.8.0 tag
git tag -a v0.8.0 -m "ANX HTTP Server v0.8.0 - 负载均衡与健康检查

主要功能:
- 多种负载均衡算法支持
- 健康检查机制
- 缓存系统
- 性能优化
- 配置增强

发布日期: $(date '+%Y-%m-%d')
分支: feature/phase-1.4-compression"
```

## 步骤5：切换到master分支
```bash
# 切换到master
git checkout master

# 更新master分支
git pull origin master
```

## 步骤6：合并feature分支
```bash
# 合并feature分支
git merge feature/phase-1.4-compression --no-ff -m "Merge feature/phase-1.4-compression: 完成Phase 2.2负载均衡功能

合并功能:
- 负载均衡算法实现
- 健康检查机制
- 缓存系统
- 性能优化
- 文档更新

版本: v0.8.0"
```

## 步骤7：删除测试文件
```bash
# 删除测试脚本（如果存在）
rm -f test_load_balancer_demo.sh
rm -f test_cache_demo.sh
rm -f test_compression_demo.sh
rm -f test_logging_demo.sh
rm -f test_headers_demo.sh
rm -f check_compression.sh
rm -f check_status.sh
```

## 步骤8：删除临时文档
```bash
# 删除临时文档（如果存在）
rm -f PHASE_1_3_SUMMARY.md
rm -f PHASE_1_4_SUMMARY.md
rm -f PHASE_2_1_SUMMARY.md
rm -f PHASE_2_2_SUMMARY.md
rm -f PROJECT_STATUS_REPORT.md
rm -f GIT_CLEANUP_GUIDE.md
```

## 步骤9：提交删除操作
```bash
# 添加删除操作
git add -A

# 提交
git commit -m "cleanup: 删除无用测试文件和临时文档

- 删除测试脚本文件
- 删除临时文档文件
- 清理项目结构
- 为正式发布做准备"
```

## 步骤10：推送到远程
```bash
# 推送master分支
git push origin master

# 推送tag
git push origin --tags
```

## 步骤11：删除已合并的分支
```bash
# 删除本地分支
git branch -d feature/phase-1.4-compression

# 删除远程分支
git push origin --delete feature/phase-1.4-compression
```

## 步骤12：验证结果
```bash
# 检查当前分支
git branch --show-current

# 检查tag
git tag -l

# 检查最新提交
git log --oneline -5

# 检查文件
ls -la
```

## 步骤13：测试编译
```bash
# 清理并编译
make clean && make

# 如果编译成功，测试运行
./build/anx_server --help
```

---

## 注意事项：

1. **每个步骤执行前请仔细检查**
2. **如果某个命令失败，请检查错误信息**
3. **删除文件前请确认文件存在**
4. **如果有冲突，请手动解决**
5. **最后的编译测试很重要**

完成后您的项目将：
- 在master分支上
- 有清晰的tag体系（v0.8.0）
- 删除了无用的测试文件
- 准备好开始Phase 2.3 