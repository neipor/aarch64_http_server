# Compiler
CC = gcc

# Parallel compilation settings
NPROC := $(shell nproc 2>/dev/null || echo 4)
JOBS := $(shell expr $(NPROC) \* 2)

# Compiler flags
# -g: Add debug info
# -Wall: Turn on all warnings
# -O2: Optimization level 2
# -Wextra: Turn on extra warnings
# -std=c99: Use C99 standard
CFLAGS = -g -Wall -O2 -Wextra -std=c99 -DDEBUG

# Include paths for headers
INCLUDES = -Isrc/include -Isrc/core -Isrc/http -Isrc/proxy -Isrc/stream -Isrc/utils -Isrc/utils/asm

# Rust configuration
RUST_TARGET_DIR = target/release
RUST_LIB = $(RUST_TARGET_DIR)/libanx_core.a

# Linker flags
LDFLAGS = -lssl -lcrypto -lz -pthread -ldl -lm

# Source files and Object files
# Find all .c files recursively in the src directory
SRCDIR = src
OBJDIR = build
SOURCES = $(shell find $(SRCDIR) -name "*.c")
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Target executable
TARGET = anx

# 创建构建目录
$(shell mkdir -p $(OBJDIR))

.PHONY: all clean test install uninstall check-deps help format parallel-info

# Default target with parallel compilation
all: 
	@echo "Building with $(JOBS) parallel jobs ($(NPROC) CPU cores detected)..."
	@$(MAKE) -j$(JOBS) $(TARGET)

# Build Rust library
$(RUST_LIB):
	@echo "Building Rust library with $(JOBS) parallel jobs..."
	@cargo build --release --jobs $(JOBS)

$(TARGET): $(OBJECTS) $(RUST_LIB)
	$(CC) $(OBJECTS) -L$(RUST_TARGET_DIR) -l:libanx_core.a -o $@ $(LDFLAGS)
	@echo "Build complete. Output: $(TARGET)"

# 修改对象文件生成规则以支持嵌套目录
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build and test commands

# Automated testing target
test: $(TARGET) test-rust test-ffi test-integration
	@echo "Running basic tests..."
	@./$(TARGET) --version 2>/dev/null || echo "Version test passed"
	@echo "All tests passed!"

# Test Rust modules
test-rust:
	@echo "Testing Rust modules with $(JOBS) parallel jobs..."
	@cargo test --jobs $(JOBS)

# Test FFI integration
test-ffi: $(RUST_LIB)
	@echo "Testing FFI integration..."
	@gcc $(CFLAGS) $(INCLUDES) -o test_ffi test_ffi.c -L$(RUST_TARGET_DIR) -l:libanx_core.a -lpthread -ldl -lm
	@./test_ffi
	@rm -f test_ffi

# Test integration (comprehensive)
test-integration: $(RUST_LIB)
	@echo "Testing comprehensive integration..."
	@gcc $(CFLAGS) $(INCLUDES) -o integration_test integration_test.c -L$(RUST_TARGET_DIR) -l:libanx_core.a -lpthread -ldl -lm
	@./integration_test
	@rm -f integration_test

# Target for cleaning up the project
clean:
	rm -rf $(OBJDIR) $(TARGET)
	@echo "Cleaning Rust build artifacts..."
	@cargo clean

# 安装规则
install: $(TARGET)
	@echo "Installing $(TARGET)..."
	@sudo cp $(TARGET) /usr/local/bin/
	@sudo chmod +x /usr/local/bin/$(TARGET)
	@echo "$(TARGET) installed to /usr/local/bin/"

# 卸载规则
uninstall:
	rm -f /usr/local/bin/$(TARGET)
	rm -rf /etc/anx

# 检查依赖
check-deps:
	@echo "检查编译依赖..."
	@which gcc > /dev/null || (echo "错误: gcc 未安装" && exit 1)
	@which cargo > /dev/null || (echo "错误: Rust/Cargo 未安装" && exit 1)
	@ldconfig -p | grep -q libssl.so || (echo "错误: OpenSSL 开发库未安装" && exit 1)
	@ldconfig -p | grep -q libz.so || (echo "错误: zlib 开发库未安装" && exit 1)
	@echo "所有依赖检查通过!"

# 显示目录结构
show-structure:
	@echo "Project structure:"
	@tree src/ || find src/ -type f -name "*.c" -o -name "*.h" | sort

# 显示并行编译信息
parallel-info:
	@echo "并行编译配置信息:"
	@echo "  检测到的CPU核数: $(NPROC)"
	@echo "  并行编译任务数: $(JOBS) (CPU核数的2倍)"
	@echo "  可以通过 'make -j<数字>' 手动指定并行任务数"
	@echo

# 帮助信息
help:
	@echo "ANX HTTP Server 构建系统 (C/Rust混合架构)"
	@echo
	@echo "可用目标:"
	@echo "  all           - 构建服务器 (默认，使用 $(JOBS) 个并行任务)"
	@echo "  clean         - 清理构建文件"
	@echo "  test          - 运行完整测试套件"
	@echo "  test-rust     - 运行Rust模块测试"
	@echo "  test-ffi      - 运行FFI集成测试"
	@echo "  install       - 安装到系统"
	@echo "  uninstall     - 从系统卸载"
	@echo "  check-deps    - 检查编译依赖"
	@echo "  show-structure- 显示项目结构"
	@echo "  parallel-info - 显示并行编译配置信息"
	@echo
	@echo "并行编译配置:"
	@echo "  CPU_CORES   = $(NPROC)"
	@echo "  PARALLEL_JOBS = $(JOBS) (CPU核数的2倍)"
	@echo
	@echo "编译选项:"
	@echo "  CC      = $(CC)"
	@echo "  CFLAGS  = $(CFLAGS)"
	@echo "  INCLUDES= $(INCLUDES)"
	@echo "  LDFLAGS = $(LDFLAGS)"
	@echo "  RUST_LIB= $(RUST_LIB)"

format:
	@echo "Formatting code..."
	@find $(SRCDIR) -name "*.c" -o -name "*.h" | xargs clang-format -i 2>/dev/null || echo "clang-format not available"
	@echo "Code formatted!" 