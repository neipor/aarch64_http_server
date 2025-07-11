# FFI示例编译Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I../src/include
LDFLAGS = -L../target/debug -lanx_core -lpthread -ldl

# 获取CPU核心数
CPU_CORES := $(shell nproc)
THREADS := $(shell echo $$(($(CPU_CORES) * 2)))

# 目标文件
TARGET = ffi_example
CLI_TARGET = cli_example
SOURCES = ffi_example.c
CLI_SOURCES = cli_example.c
OBJECTS = $(SOURCES:.c=.o)
CLI_OBJECTS = $(CLI_SOURCES:.c=.o)

# 默认目标
all: $(TARGET) $(CLI_TARGET)

# 编译FFI示例目标
$(TARGET): $(OBJECTS)
	@echo "链接FFI示例程序..."
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "FFI示例程序编译完成: $(TARGET)"

# 编译CLI示例目标
$(CLI_TARGET): $(CLI_OBJECTS)
	@echo "链接CLI示例程序..."
	$(CC) $(CLI_OBJECTS) -o $(CLI_TARGET) $(LDFLAGS)
	@echo "CLI示例程序编译完成: $(CLI_TARGET)"

# 编译对象文件
%.o: %.c
	@echo "编译 $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# 运行FFI示例
run: $(TARGET)
	@echo "运行FFI示例程序..."
	@echo "================================"
	./$(TARGET)

# 运行CLI示例
run-cli: $(CLI_TARGET)
	@echo "运行CLI示例程序..."
	@echo "================================"
	./$(CLI_TARGET)

# 清理
clean:
	@echo "清理编译文件..."
	rm -f $(OBJECTS) $(CLI_OBJECTS) $(TARGET) $(CLI_TARGET)
	@echo "清理完成"

# 重新编译Rust库
rebuild-lib:
	@echo "重新编译Rust库..."
	cd .. && cargo build
	@echo "Rust库编译完成"

# 完整重建
rebuild: clean rebuild-lib all

# 帮助信息
help:
	@echo "示例编译选项:"
	@echo "  all          - 编译所有示例程序"
	@echo "  run          - 编译并运行FFI示例"
	@echo "  run-cli      - 编译并运行CLI示例"
	@echo "  clean        - 清理编译文件"
	@echo "  rebuild-lib  - 重新编译Rust库"
	@echo "  rebuild      - 完整重建"
	@echo "  help         - 显示此帮助信息"
	@echo ""
	@echo "CPU核心数: $(CPU_CORES)"
	@echo "编译线程数: $(THREADS)"

.PHONY: all run clean rebuild-lib rebuild help 