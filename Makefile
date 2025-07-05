# Compiler
CC = gcc

# Compiler flags
# -g: Add debug info
# -Wall: Turn on all warnings
# -O2: Optimization level 2
CFLAGS = -g -Wall -O2

# Linker flags
LDFLAGS = -lssl -lcrypto

# Source files and Object files
# Find all .c files in the src directory
SRCS = src/config.c \
       src/http.c \
       src/https.c \
       src/log.c \
       src/main.c \
       src/net.c \
       src/util.c \
       src/core.c \
       src/proxy.c \
       src/headers.c

OBJS = $(SRCS:src/%.c=build/%.o)

# Target executable
TARGET = anx

.PHONY: all clean docker-build-prod docker-run-prod docker-build-dev docker-run-dev test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
	@echo "Build complete. Output: $(TARGET)"

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

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
test: all
	@echo "Starting container in background..."
	docker run -d --rm --name anx-test-container -v "$(PWD)":/app anx-dev-env ./anx -c server.conf
	@echo "Waiting for server to start..."
	sleep 3
	@echo "Getting container IP..."
	CONTAINER_IP=$$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' anx-test-container); \
	if [ -z "$$CONTAINER_IP" ]; then \
		echo "Failed to get container IP. Aborting test."; \
		docker logs anx-test-container; \
		docker stop anx-test-container; \
		exit 1; \
	fi; \
	echo "Container IP: $$CONTAINER_IP"; \
	echo ""; \
	echo "=== Testing HTTP (port 80) ==="; \
	curl -s --max-time 10 -w "Status: %{http_code}, Time: %{time_total}s\n" http://$$CONTAINER_IP/ || echo "HTTP test failed (timeout or other error)"; \
	echo ""; \
	echo "=== Testing HTTPS (port 443) ==="; \
	curl -s -k --max-time 10 -w "Status: %{http_code}, Time: %{time_total}s\n" https://$$CONTAINER_IP/ || echo "HTTPS test failed (timeout or other error)"; \
	echo ""; \
	echo "=== Server Logs ==="; \
	docker logs anx-test-container; \
	echo ""; \
	echo "=== Stopping container ==="; \
	docker stop anx-test-container

# Target for cleaning up the project
clean:
	rm -f $(TARGET) build/*.o 