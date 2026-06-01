# user_prog/Explorer/Makefile

GPP = g++
LD = ld

# Includes: Look in local include AND libhx86 include
INCLUDES = -Iinclude -I../libhx86/include

# Compiler Flags
CPPFLAGS = -m32 -g -ffreestanding -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-common $(INCLUDES)

# Linker Flags: Link against the static library in build/libhx86
LDFLAGS = -melf_i386 -L../../build/libhx86 -lhx86

BUILD_DIR = ../../build
APP_NAME = Explorer

# Source files for THIS program only
SRCS = prog.cpp
OBJS = $(addprefix $(BUILD_DIR)/obj/$(APP_NAME)/, $(SRCS:.cpp=.o))
TARGET = $(BUILD_DIR)/user/$(APP_NAME).bin

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p $(dir $(TARGET))
	@echo "[APP] Linking $@"
	@$(LD) -T linker.ld -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/obj/$(APP_NAME)/%.o: %.cpp
	mkdir -p $(dir $@)
	@echo "[APP] Compiling $<"
	@$(GPP) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
