# 将要删除的文件列表

## 📋 Git清理脚本将删除以下文件和目录：

### 🧪 测试脚本文件
- `test_load_balancer_demo.sh` - 负载均衡测试脚本
- `test_cache_demo.sh` - 缓存功能测试脚本
- `test_compression_demo.sh` - 压缩功能测试脚本
- `test_logging_demo.sh` - 日志功能测试脚本
- `test_headers_demo.sh` - 头部操作测试脚本
- `check_compression.sh` - 压缩检查脚本
- `check_status.sh` - 状态检查脚本

### 🐳 Docker文件
- `Dockerfile` - 生产环境Docker配置
- `Dockerfile.dev` - 开发环境Docker配置
- `.dockerignore` - Docker忽略文件

### ⚙️ 临时配置文件
- `anx.conf` - 临时配置文件

### 📁 临时目录
- `config/` - 配置目录（如果存在）
- `certs/` - 证书目录（如果存在）
- `tests/` - 测试目录（如果存在）

### 📄 临时文档
- `PHASE_1_3_SUMMARY.md` - Phase 1.3总结文档
- `PHASE_1_4_SUMMARY.md` - Phase 1.4总结文档
- `PHASE_2_1_SUMMARY.md` - Phase 2.1总结文档
- `PHASE_2_2_SUMMARY.md` - Phase 2.2总结文档
- `PROJECT_STATUS_REPORT.md` - 项目状态报告
- `GIT_CLEANUP_GUIDE.md` - Git清理指南

### 🏷️ 要删除的Git Tag
- `v0.1.0` - 旧版本标签
- `v0.2.0-c-version` - C版本标签
- `v0.3.0-epoll` - Epoll版本标签
- `v0.4.0-static-files` - 静态文件版本标签
- `v0.5.0-configurable` - 可配置版本标签
- `v0.6.0-multi-process` - 多进程版本标签

### 🌿 要删除的Git分支
- `feature/phase-1.4-compression` - 压缩功能分支（合并后删除）

---

## 📌 保留的重要文件

### 📚 核心文档
- `README.md` - 项目说明
- `CHANGELOG.md` - 更新日志
- `ROADMAP.md` - 发展路线图

### 🔧 核心配置
- `Makefile` - 构建配置
- `server.conf` - 服务器配置
- `.gitignore` - Git忽略文件
- `.clang-format` - 代码格式化配置

### 💻 源代码
- `src/` - 源代码目录及所有文件
- `build/` - 构建输出目录
- `www/` - 网站文件目录
- `logs/` - 日志目录
- `docs/` - 文档目录

### 🏷️ 新创建的Tag
- `v0.8.0` - 新版本标签（标记Phase 2.2完成）

---

## ⚠️ 重要提醒

1. **备份重要数据**：如果您有重要的测试数据或配置，请先备份
2. **不可恢复**：删除操作不可逆，请确认不需要这些文件
3. **Docker部署**：删除Docker文件后，如果需要容器化部署，需要重新创建
4. **测试脚本**：删除测试脚本后，需要重新创建用于Phase 2.3的测试

## 🎯 删除后的项目状态

删除完成后，您的项目将：
- ✅ 在master分支上
- ✅ 有清晰的tag体系（v0.8.0）
- ✅ 删除了开发期间的临时文件
- ✅ 项目结构干净整洁
- ✅ 准备好开始Phase 2.3开发

---

**执行前请确认：您确实不需要上述列出的文件和目录！** 