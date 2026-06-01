# user_prog/shared/Makefile

# Compiler Settings
GPP = g++
AR = ar

INCLUDES = -Iinclude

# Flags: Note the include path points to the local 'include' folder
CPPFLAGS = -m32 -g -ffreestanding -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-common $(INCLUDES)

BUILD_DIR = ../../build/libhx86

# Automatically find all .cpp files inside Hx86/ and subdirectories
SRCS := $(shell find Hx86 -name '*.cpp')
OBJS := $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))

# The Output Library
TARGET = $(BUILD_DIR)/libhx86.a

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "[LIB] Archiving $@"
	@$(AR) rcs $@ $(OBJS)

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	@echo "[LIB] Compiling $<"
	@$(GPP) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
