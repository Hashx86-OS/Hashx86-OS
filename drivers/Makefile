GXX = g++
GPP_PARAMS = -m32 -g -ffreestanding -I../include -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-common -c -fno-pic

BUILD_DIR = ../build/drivers

# List of drivers to build
DRIVERS = $(BUILD_DIR)/bga.sys \
	  $(BUILD_DIR)/ac97.sys

all: $(DRIVERS)

$(BUILD_DIR)/%.sys: %.cpp
	mkdir -p $(BUILD_DIR)
	$(GXX) $(GPP_PARAMS) -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
