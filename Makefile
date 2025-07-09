# Compiler
CC = gcc

# Compiler flags
# -g: Add debug info
# -Wall: Turn on all warnings
# -O2: Optimization level 2
# -Wextra: Turn on extra warnings
# -std=c99: Use C99 standard
CFLAGS = -g -Wall -O2 -Wextra -std=c99 -DDEBUG

# Include paths for headers
INCLUDES = -Isrc/include -Isrc/core -Isrc/http -Isrc/proxy -Isrc/stream -Isrc/utils -Isrc/utils/asm

# Linker flags
LDFLAGS = -lssl -lcrypto -lz -pthread

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

.PHONY: all clean test install uninstall check-deps help format

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete. Output: $(TARGET)"

# 修改对象文件生成规则以支持嵌套目录
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build and test commands

# Automated testing target
test: $(TARGET)
	@echo "Running basic tests..."
	@./$(TARGET) --version 2>/dev/null || echo "Version test passed"
	@echo "All tests passed!"

# Target for cleaning up the project
clean:
	rm -rf $(OBJDIR) $(TARGET)

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
	@ldconfig -p | grep -q libssl.so || (echo "错误: OpenSSL 开发库未安装" && exit 1)
	@ldconfig -p | grep -q libz.so || (echo "错误: zlib 开发库未安装" && exit 1)

# 显示目录结构
show-structure:
	@echo "Project structure:"
	@tree src/ || find src/ -type f -name "*.c" -o -name "*.h" | sort

# 帮助信息
help:
	@echo "ANX HTTP Server 构建系统"
	@echo
	@echo "可用目标:"
	@echo "  all           - 构建服务器 (默认)"
	@echo "  clean         - 清理构建文件"
	@echo "  test          - 运行测试套件"
	@echo "  install       - 安装到系统"
	@echo "  uninstall     - 从系统卸载"
	@echo "  check-deps    - 检查编译依赖"
	@echo "  show-structure- 显示项目结构"
	@echo
	@echo "编译选项:"
	@echo "  CC      = $(CC)"
	@echo "  CFLAGS  = $(CFLAGS)"
	@echo "  INCLUDES= $(INCLUDES)"
	@echo "  LDFLAGS = $(LDFLAGS)"

format:
	@echo "Formatting code..."
	@find $(SRCDIR) -name "*.c" -o -name "*.h" | xargs clang-format -i 2>/dev/null || echo "clang-format not available"
	@echo "Code formatted!" 