# Compiler and Linker
AS = as
LD = ld

# Source files and Object files
SRCS = $(wildcard src/*.s)
OBJS = $(patsubst src/%.s, build/%.o, $(SRCS))

# Target executable
TARGET = anx

# Flags
ASFLAGS = -g # -g adds debug information
LDFLAGS = 

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS)

build/%.o: src/%.s
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) build/*.o 