# user_prog/AppStore/Makefile

GPP = g++
LD = ld

BUILD_DIR = ../../build/user
LIBHX86_BUILD_DIR = ../../build/libhx86
LIBMBEDTLS_BUILD_DIR = ../../build/libmbedtls

INCLUDES = -I. -I../libhx86/include -I../libmbedtls/include -I../../include \
           -I../../third_party/mbedtls/include -I../../third_party/jsmn

CPPFLAGS = -m32 -O2 -g -ffreestanding -fno-use-cxa-atexit -nostdlib -fno-builtin \
           -fno-rtti -fno-exceptions -fno-common -MMD -MP $(INCLUDES)

LDFLAGS = -melf_i386 -L$(LIBHX86_BUILD_DIR) -L$(LIBMBEDTLS_BUILD_DIR) \
          -lmbedtls -lhx86

APP_NAME = AppStore

CPP_SRCS = prog.cpp HttpClient.cpp GitHub.cpp StoreUtil.cpp

CPP_OBJS = $(addprefix $(BUILD_DIR)/obj/$(APP_NAME)/, $(CPP_SRCS:.cpp=.o))
OBJS = $(CPP_OBJS)
TARGET = $(BUILD_DIR)/$(APP_NAME).bin

all: $(TARGET)

$(TARGET): $(OBJS) linker.ld
	@echo "[APP] Linking $@"
	@mkdir -p $(dir $@)
	@$(LD) -T linker.ld -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/obj/$(APP_NAME)/%.o: %.cpp
	@echo "[APP] Compiling $<"
	@mkdir -p $(dir $@)
	@$(GPP) $(CPPFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

clean:
	rm -rf $(BUILD_DIR)/obj/$(APP_NAME) $(TARGET)
