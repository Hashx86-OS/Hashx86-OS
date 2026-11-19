# userspace mbedTLS build for Hashx86.

GCC = gcc
GPP = g++
AR = ar
ROOT = ../..

INCLUDES = -Iinclude -I$(ROOT)/third_party/mbedtls/include -I../libhx86/include -I$(ROOT)/include
CONFIG_DEFINE = -DMBEDTLS_CONFIG_FILE='"hashx86_mbedtls_config.h"'
CPPFLAGS = -m32 -O2 -g -ffreestanding -fno-use-cxa-atexit -nostdlib -fno-builtin \
           -fno-exceptions -fno-common $(INCLUDES) $(CONFIG_DEFINE) -MMD -MP

BUILD_DIR = $(ROOT)/build/libmbedtls
MBEDTLS_LIB_DIR = $(ROOT)/third_party/mbedtls/library
MBEDTLS_SRCS := $(wildcard $(MBEDTLS_LIB_DIR)/*.c)
MBEDTLS_OBJS := $(patsubst $(MBEDTLS_LIB_DIR)/%.c,$(BUILD_DIR)/%.o,$(MBEDTLS_SRCS))
PORT_OBJ = $(BUILD_DIR)/mbedtls_port.o
DIV_OBJ = $(BUILD_DIR)/64bit.o
TARGET = $(BUILD_DIR)/libmbedtls.a

all: $(TARGET)

$(TARGET): $(MBEDTLS_OBJS) $(PORT_OBJ) $(DIV_OBJ)
	@echo "[TLS] Archiving $@"
	@$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: $(MBEDTLS_LIB_DIR)/%.c
	@echo "[TLS] Compiling $<"
	@mkdir -p $(dir $@)
	@$(GCC) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/mbedtls_port.o: mbedtls_port.cpp
	@echo "[TLS] Compiling $<"
	@mkdir -p $(dir $@)
	@$(GPP) $(CPPFLAGS) -fno-rtti -c $< -o $@

$(BUILD_DIR)/64bit.o: 64bit.cpp
	@echo "[TLS] Compiling $<"
	@mkdir -p $(dir $@)
	@$(GPP) $(CPPFLAGS) -fno-rtti -c $< -o $@

-include $(MBEDTLS_OBJS:.o=.d) $(PORT_OBJ:.o=.d) $(DIV_OBJ:.o=.d)

clean:
	rm -rf $(BUILD_DIR)
