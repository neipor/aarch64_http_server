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
       src/core.c

OBJS = $(SRCS:src/%.c=build/%.o)

# Target executable
TARGET = anx

.PHONY: all clean docker-build docker-run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

# Docker commands
docker-build:
	docker build -t anx-server .

docker-run:
	docker run -it -p 8080:80 -p 8443:443 -p 9090:8080 --rm anx-server

# Target for cleaning up the project
clean:
	rm -f $(TARGET) build/*.o 