# Compiler
CC = gcc

# Compiler flags
# -g: Add debug info
# -Wall: Turn on all warnings
# -O2: Optimization level 2
CFLAGS = -g -Wall -O2

# Source files and Object files
# Find all .c files in the src directory
SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, build/%.o, $(SRCS))

# Target executable
TARGET = anx

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) build/*.o 