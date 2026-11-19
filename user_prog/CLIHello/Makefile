# user_prog/CLIHello/Makefile

GPP = g++
LD = ld
BUILD_DIR ?= ../../build/user
LIBHX86_BUILD_DIR ?= ../../build/libhx86

INCLUDES = -Iinclude -I../libhx86/include -I../../include

CPPFLAGS = -m32 -O2 -g -ffreestanding -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-common $(INCLUDES)

LDFLAGS = -melf_i386 -L$(LIBHX86_BUILD_DIR) -lhx86

APP_NAME = CLIHello
SRCS = prog.cpp
OBJS = $(addprefix $(BUILD_DIR)/obj/$(APP_NAME)/, $(SRCS:.cpp=.o))
TARGET = $(BUILD_DIR)/CLIHello.bin

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "[APP] Linking $@"
	@mkdir -p $(dir $@)
	@$(LD) -T linker.ld -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/obj/$(APP_NAME)/%.o: %.cpp
	@echo "[APP] Compiling $<"
	@mkdir -p $(dir $@)
	@$(GPP) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)/obj/$(APP_NAME) $(TARGET)
