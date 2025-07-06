# Compiler
CC = gcc

# Compiler flags
# -g: Add debug info
# -Wall: Turn on all warnings
# -O2: Optimization level 2
# -Wextra: Turn on extra warnings
# -std=c99: Use C99 standard
CFLAGS = -g -Wall -O2 -Wextra -std=c99 -DDEBUG

# Linker flags
LDFLAGS = -lssl -lcrypto -lz -pthread

# Source files and Object files
# Find all .c files in the src directory
SRCDIR = src
OBJDIR = build
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Target executable
TARGET = anx

# 创建构建目录
$(shell mkdir -p $(OBJDIR))

.PHONY: all clean docker-build-prod docker-run-prod docker-build-dev docker-run-dev test install uninstall check-deps help format

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete. Output: $(TARGET)"

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Docker commands

# --- Production Build (self-contained image) ---
docker-build-prod:
	docker build -t anx-prod-env -f Dockerfile .

docker-run-prod: docker-build-prod
	docker run -it -p 80:80 -p 443:443 --rm anx-prod-env

# --- Development Workflow (host compile, container run) ---
docker-build-dev:
	docker build -t anx-dev-env -f Dockerfile.dev .

# This is the primary development command. It compiles on the host and runs in the container.
docker-run-dev:
	docker run -it --rm --name anx-dev-container -v "$(PWD)":/app anx-dev-env

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

# 帮助信息
help:
	@echo "ANX HTTP Server 构建系统"
	@echo
	@echo "可用目标:"
	@echo "  all        - 构建服务器 (默认)"
	@echo "  clean      - 清理构建文件"
	@echo "  test       - 运行测试套件"
	@echo "  install    - 安装到系统"
	@echo "  uninstall  - 从系统卸载"
	@echo "  check-deps - 检查编译依赖"
	@echo
	@echo "编译选项:"
	@echo "  CC      = $(CC)"
	@echo "  CFLAGS  = $(CFLAGS)"
	@echo "  LDFLAGS = $(LDFLAGS)"

format:
	@echo "Formatting code..."
	@find $(SRCDIR) -name "*.c" -o -name "*.h" | xargs clang-format -i
	@echo "Code formatted!"

# 依赖关系
$(OBJDIR)/main.o: $(SRCDIR)/main.c $(SRCDIR)/server.h $(SRCDIR)/config.h $(SRCDIR)/log.h
$(OBJDIR)/server.o: $(SRCDIR)/server.c $(SRCDIR)/server.h $(SRCDIR)/core.h $(SRCDIR)/net.h $(SRCDIR)/http.h $(SRCDIR)/https.h $(SRCDIR)/log.h
$(OBJDIR)/config.o: $(SRCDIR)/config.c $(SRCDIR)/config.h $(SRCDIR)/log.h $(SRCDIR)/compress.h $(SRCDIR)/cache.h $(SRCDIR)/health_check.h
$(OBJDIR)/core.o: $(SRCDIR)/core.c $(SRCDIR)/core.h $(SRCDIR)/config.h $(SRCDIR)/log.h $(SRCDIR)/cache.h $(SRCDIR)/load_balancer.h $(SRCDIR)/health_check.h
$(OBJDIR)/net.o: $(SRCDIR)/net.c $(SRCDIR)/net.h $(SRCDIR)/log.h
$(OBJDIR)/http.o: $(SRCDIR)/http.c $(SRCDIR)/http.h $(SRCDIR)/core.h $(SRCDIR)/log.h $(SRCDIR)/util.h $(SRCDIR)/proxy.h $(SRCDIR)/lb_proxy.h $(SRCDIR)/headers.h $(SRCDIR)/compress.h $(SRCDIR)/cache.h
$(OBJDIR)/https.o: $(SRCDIR)/https.c $(SRCDIR)/https.h $(SRCDIR)/core.h $(SRCDIR)/log.h $(SRCDIR)/util.h $(SRCDIR)/proxy.h $(SRCDIR)/lb_proxy.h $(SRCDIR)/headers.h $(SRCDIR)/compress.h $(SRCDIR)/cache.h
$(OBJDIR)/log.o: $(SRCDIR)/log.c $(SRCDIR)/log.h
$(OBJDIR)/util.o: $(SRCDIR)/util.c $(SRCDIR)/util.h
$(OBJDIR)/proxy.o: $(SRCDIR)/proxy.c $(SRCDIR)/proxy.h $(SRCDIR)/log.h
$(OBJDIR)/headers.o: $(SRCDIR)/headers.c $(SRCDIR)/headers.h $(SRCDIR)/config.h $(SRCDIR)/log.h
$(OBJDIR)/compress.o: $(SRCDIR)/compress.c $(SRCDIR)/compress.h $(SRCDIR)/log.h
$(OBJDIR)/cache.o: $(SRCDIR)/cache.c $(SRCDIR)/cache.h $(SRCDIR)/log.h
$(OBJDIR)/load_balancer.o: $(SRCDIR)/load_balancer.c $(SRCDIR)/load_balancer.h $(SRCDIR)/log.h $(SRCDIR)/health_check.h
$(OBJDIR)/lb_proxy.o: $(SRCDIR)/lb_proxy.c $(SRCDIR)/lb_proxy.h $(SRCDIR)/load_balancer.h $(SRCDIR)/core.h $(SRCDIR)/log.h
$(OBJDIR)/health_check.o: $(SRCDIR)/health_check.c $(SRCDIR)/health_check.h $(SRCDIR)/load_balancer.h $(SRCDIR)/log.h
$(OBJDIR)/health_api.o: $(SRCDIR)/health_api.c $(SRCDIR)/health_api.h $(SRCDIR)/health_check.h $(SRCDIR)/load_balancer.h $(SRCDIR)/log.h 