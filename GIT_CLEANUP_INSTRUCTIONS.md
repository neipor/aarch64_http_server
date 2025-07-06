# Git 清理操作指南

## 概述

为了整理ANX HTTP Server项目的git仓库，我创建了两个脚本来帮助您：

1. **git_cleanup.sh** - 自动化脚本，一次性执行所有清理操作
2. **git_cleanup_safe.sh** - 安全版本，分步执行，每步都需要确认

## 将要执行的操作

### 1. Tag管理
- **删除旧的tag**：
  - `v0.1.0`
  - `v0.2.0-c-version`
  - `v0.3.0-epoll`
  - `v0.4.0-static-files`
  - `v0.5.0-configurable`
  - `v0.6.0-multi-process`

- **创建新的tag**：
  - `v0.8.0` - 标记当前完成的Phase 2.2功能

### 2. 分支管理
- 提交当前修改到 `feature/phase-1.4-compression` 分支
- 切换到 `master` 分支
- 合并 `feature/phase-1.4-compression` 分支到 `master`
- 删除已合并的 `feature/phase-1.4-compression` 分支

### 3. 文件清理
- **删除测试脚本**：
  - `test_load_balancer_demo.sh`
  - `test_cache_demo.sh`
  - `test_compression_demo.sh`
  - `test_logging_demo.sh`
  - `test_headers_demo.sh`
  - `check_compression.sh`
  - `check_status.sh`

- **删除临时文档**：
  - `PHASE_1_3_SUMMARY.md`
  - `PHASE_1_4_SUMMARY.md`
  - `PHASE_2_1_SUMMARY.md`
  - `PHASE_2_2_SUMMARY.md`
  - `PROJECT_STATUS_REPORT.md`
  - `GIT_CLEANUP_GUIDE.md`

## 使用方法

### 方法1：使用安全版本（推荐）

```bash
# 给脚本执行权限
chmod +x git_cleanup_safe.sh

# 运行脚本
./git_cleanup_safe.sh
```

这个版本会：
- 分步显示每个操作
- 每步都询问是否继续
- 允许跳过任何步骤
- 显示详细的状态信息

### 方法2：使用自动化版本

```bash
# 给脚本执行权限
chmod +x git_cleanup.sh

# 运行脚本
./git_cleanup.sh
```

这个版本会：
- 一次性执行所有操作
- 只在开始时询问一次确认
- 适合有经验的用户

## 执行前的准备

1. **确保工作区是干净的**：
   ```bash
   git status
   ```

2. **确认当前分支**：
   ```bash
   git branch --show-current
   ```

3. **备份重要文件**（如果有的话）：
   ```bash
   cp important_file.txt important_file.txt.backup
   ```

## 执行后的验证

1. **检查当前分支**：
   ```bash
   git branch --show-current
   # 应该显示: master
   ```

2. **检查tag**：
   ```bash
   git tag -l
   # 应该显示: v0.2.0 v0.3.0 v0.4.0 v0.8.0
   ```

3. **检查最新提交**：
   ```bash
   git log --oneline -5
   ```

4. **编译测试**：
   ```bash
   make clean && make
   ```

## 可能出现的问题

### 1. 权限问题
```bash
# 解决方案
chmod +x git_cleanup_safe.sh
```

### 2. 网络问题
如果推送到远程仓库失败，可以稍后手动推送：
```bash
git push origin master
git push origin --tags
```

### 3. 分支冲突
如果合并时出现冲突，脚本会停止。需要手动解决冲突：
```bash
# 查看冲突文件
git status

# 编辑冲突文件后
git add .
git commit -m "解决合并冲突"
```

## 清理后的项目状态

执行完成后，您的项目将：
- 在 `master` 分支上
- 包含所有Phase 2.2的功能
- 有清晰的tag体系
- 删除了临时测试文件
- 准备好开始Phase 2.3开发

## 下一步建议

1. 运行编译测试确保功能正常
2. 测试服务器基本功能
3. 开始Phase 2.3的开发规划
4. 更新README.md文档

---

**注意**：这些脚本会永久删除文件和tag，请确保在执行前备份重要数据。 