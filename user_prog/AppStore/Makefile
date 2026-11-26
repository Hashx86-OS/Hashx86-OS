# user_prog/AppStore/Makefile

GPP = g++
LD = ld

BUILD_DIR = ../../build/user
LIBHX86_BUILD_DIR = ../../build/libhx86
LIBMBEDTLS_BUILD_DIR = ../../build/libmbedtls

INCLUDES = -I. -I../libhx86/include -I../libmbedtls/include -I../../include \
           -I../../third_party/mbedtls/include -I../../third_party/jsmn \
           -I../../third_party/stb_image

# Change this to the repository that hosts the catalog release. The release
# itself is discovered through GitHub's /releases/latest API; tags, asset IDs,
# URLs and hashes are not compiled into the store. App binaries are resolved
# from each catalog entry's own repository.
APPSTORE_CATALOG_REPO ?= Hashx86-OS/AppStore-Hx86
APPSTORE_CATALOG_ASSET ?= catalog.json
APPSTORE_OS_VERSION ?= 0.1.0

CPPFLAGS = -m32 -O2 -g -ffreestanding -fno-use-cxa-atexit -nostdlib -fno-builtin \
           -fno-rtti -fno-exceptions -fno-common -MMD -MP $(INCLUDES) \
           -DAPPSTORE_CATALOG_REPO=\"$(APPSTORE_CATALOG_REPO)\" \
           -DAPPSTORE_CATALOG_ASSET=\"$(APPSTORE_CATALOG_ASSET)\" \
           -DAPPSTORE_OS_VERSION=\"$(APPSTORE_OS_VERSION)\"

LDFLAGS = -melf_i386 -L$(LIBHX86_BUILD_DIR) -L$(LIBMBEDTLS_BUILD_DIR) \
          -lmbedtls -lhx86

APP_NAME = AppStore

CPP_SRCS = prog.cpp HttpClient.cpp Catalog.cpp StoreUtil.cpp ImageDecoder.cpp

CPP_OBJS = $(addprefix $(BUILD_DIR)/obj/$(APP_NAME)/, $(CPP_SRCS:.cpp=.o))
OBJS = $(CPP_OBJS)
TARGET = $(BUILD_DIR)/$(APP_NAME).bin

all: $(TARGET)

$(TARGET): $(OBJS) linker.ld $(LIBHX86_BUILD_DIR)/libhx86.a $(LIBMBEDTLS_BUILD_DIR)/libmbedtls.a
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

CONFIG_STAMP = $(BUILD_DIR)/obj/$(APP_NAME)/config.txt
export APPSTORE_CATALOG_REPO APPSTORE_CATALOG_ASSET APPSTORE_OS_VERSION
.PHONY: FORCE
$(CONFIG_STAMP): FORCE
	@mkdir -p $(dir $@)
	@printenv APPSTORE_CATALOG_REPO APPSTORE_CATALOG_ASSET APPSTORE_OS_VERSION > $@.tmp
	@cmp -s $@.tmp $@ || cp $@.tmp $@
	@rm -f $@.tmp
$(CPP_OBJS): $(CONFIG_STAMP)
