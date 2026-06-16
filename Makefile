KDBG_ENABLE ?= 1
KDBG_LEVEL ?= 1

GPP_PARAMS = -m32 -g -ffreestanding -Iinclude -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-common -fno-omit-frame-pointer -DKDBG_ENABLE=$(KDBG_ENABLE) -DKDBG_LEVEL=$(KDBG_LEVEL)
ASM_PARAMS = --32 -g
ASM_NASM_PARAMS = -f elf32
BUILD_DIR = build

# Source-relative .o paths under build/obj/
objects = \
	$(BUILD_DIR)/obj/asm/common_handler.o \
	$(BUILD_DIR)/obj/asm/load_gdt.o \
	$(BUILD_DIR)/obj/asm/load_tss.o \
	$(BUILD_DIR)/obj/asm/loader.o \
	$(BUILD_DIR)/obj/audio/wav.o \
	$(BUILD_DIR)/obj/core/driver.o \
	$(BUILD_DIR)/obj/core/drivers/ata.o \
	$(BUILD_DIR)/obj/core/drivers/AudioMixer.o \
	$(BUILD_DIR)/obj/core/drivers/GraphicsDriver.o \
	$(BUILD_DIR)/obj/core/drivers/keyboard.o \
	$(BUILD_DIR)/obj/core/drivers/ModuleLoader.o \
	$(BUILD_DIR)/obj/core/drivers/mouse.o \
	$(BUILD_DIR)/obj/core/drivers/SymbolTable.o \
	$(BUILD_DIR)/obj/core/drivers/vbe.o \
	$(BUILD_DIR)/obj/core/elf.o \
	$(BUILD_DIR)/obj/core/filesystem/FAT32.o \
	$(BUILD_DIR)/obj/core/filesystem/File.o \
	$(BUILD_DIR)/obj/core/filesystem/msdospart.o \
	$(BUILD_DIR)/obj/core/CrashReporter.o \
	$(BUILD_DIR)/obj/core/constructors.o \
	$(BUILD_DIR)/obj/core/gdt.o \
	$(BUILD_DIR)/obj/core/globals.o \
	$(BUILD_DIR)/obj/core/interrupts.o \
	$(BUILD_DIR)/obj/core/KernelSymbolResolver.o \
	$(BUILD_DIR)/obj/core/memory.o \
	$(BUILD_DIR)/obj/core/paging.o \
	$(BUILD_DIR)/obj/core/pci.o \
	$(BUILD_DIR)/obj/core/pmm.o \
	$(BUILD_DIR)/obj/core/ports.o \
	$(BUILD_DIR)/obj/core/scheduler.o \
	$(BUILD_DIR)/obj/core/syscalls.o \
	$(BUILD_DIR)/obj/debug.o \
	$(BUILD_DIR)/obj/gui/bmp.o \
	$(BUILD_DIR)/obj/gui/button.o \
	$(BUILD_DIR)/obj/gui/desktop.o \
	$(BUILD_DIR)/obj/gui/elements/window_action_button.o \
	$(BUILD_DIR)/obj/gui/elements/window_action_button_round.o \
	$(BUILD_DIR)/obj/gui/fonts/font.o \
	$(BUILD_DIR)/obj/gui/Hgui.o \
	$(BUILD_DIR)/obj/gui/infodialog.o \
	$(BUILD_DIR)/obj/gui/label.o \
	$(BUILD_DIR)/obj/gui/listview.o \
	$(BUILD_DIR)/obj/gui/progressbar.o \
	$(BUILD_DIR)/obj/gui/renderer/nina.o \
	$(BUILD_DIR)/obj/gui/terminalview.o \
	$(BUILD_DIR)/obj/gui/taskbar.o \
	$(BUILD_DIR)/obj/gui/widget.o \
	$(BUILD_DIR)/obj/gui/window.o \
	$(BUILD_DIR)/obj/kernel.o \
	$(BUILD_DIR)/obj/stdlib.o \
	$(BUILD_DIR)/obj/stdlib/math.o \
	$(BUILD_DIR)/obj/utils/string.o

LD_PARAMS = -melf_i386
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
KERNEL_MAP = $(BUILD_DIR)/kernel.map
KERNEL_ISO = $(BUILD_DIR)/kernel.iso
QEMU_DISK = $(BUILD_DIR)/HDD.vdi
RUNQ_DELAY ?= 1

# Generic pattern rules: any file under build/obj/ maps to source tree
$(BUILD_DIR)/obj/%.o: %.cpp
	mkdir -p $(dir $@)
	g++ $(GPP_PARAMS) -o $@ -c $<

$(BUILD_DIR)/obj/%.o: %.c
	mkdir -p $(dir $@)
	gcc $(GPP_PARAMS) -x c -o $@ -c $<

$(BUILD_DIR)/obj/%.o: %.s
	mkdir -p $(dir $@)
	as $(ASM_PARAMS) -o $@ $<

$(BUILD_DIR)/obj/%.o: %.asm
	mkdir -p $(dir $@)
	nasm $(ASM_NASM_PARAMS) -o $@ $<

# Linking the kernel binary
$(KERNEL_BIN): linker.ld $(objects)
	mkdir -p $(BUILD_DIR)
	ld $(LD_PARAMS) -Map $(KERNEL_MAP) -T $< -o $@ $(objects)

# Install the kernel binary (updated path)
install: $(KERNEL_BIN)
	sudo cp $(KERNEL_BIN) /boot/kernel.bin

# Clean rule
clean:
	-test -d $(BUILD_DIR) && find $(BUILD_DIR) -mindepth 1 -maxdepth 1 ! -name 'HDD.vdi' -exec rm -rf {} +

build:
	make clean
	make
	make -C user_prog clean
	make -C user_prog
	make -C drivers clean
	make -C drivers
	make iso
	make hdd
	@echo "[BUILD] Waiting $(RUNQ_DELAY)s to release the HDD file..."
	@sleep $(RUNQ_DELAY)
	make runq

runq: iso
	qemu-system-i386 -cdrom $(KERNEL_ISO) -boot d -vga std -serial stdio -m 1G \
	-drive file=$(QEMU_DISK),format=vdi

run:
	make clean
	make
	make iso
	make runq

hdd:
# 	Cleanup from previous mounts
	-sudo umount /mnt/vdi_p1
	-sudo qemu-nbd --disconnect /dev/nbd0
# 	1. Load the NBD module
	sudo modprobe nbd max_part=16
# 	2. Connect VHD to /dev/nbd0
	sudo qemu-nbd --connect=/dev/nbd0 $(QEMU_DISK)
# 	3. Mount Partition 1 (p1)
	sudo mkdir -p /mnt/vdi_p1
	sudo mount /dev/nbd0p1 /mnt/vdi_p1

# 	4. Copy Files
	-sudo mkdir -p /mnt/vdi_p1/bin
	-sudo mkdir -p /mnt/vdi_p1/fonts
	-sudo mkdir -p /mnt/vdi_p1/bitmaps
	-sudo mkdir -p /mnt/vdi_p1/drivers
	-sudo mkdir -p /mnt/vdi_p1/audio
	-sudo mkdir -p /mnt/vdi_p1/SYS32
	-sudo mkdir -p /mnt/vdi_p1/ProgFile/Game3D

	-sudo cp $(KERNEL_MAP) /mnt/vdi_p1/kernel.map

	-sudo cp bin/audio/boot.wav /mnt/vdi_p1/audio/boot.wav
	-sudo cp bin/bitmaps/boot.bmp /mnt/vdi_p1/bitmaps/boot.bmp
	-sudo cp bin/bitmaps/icon.bmp /mnt/vdi_p1/bitmaps/icon.bmp
	-sudo cp bin/bitmaps/cursor.bmp /mnt/vdi_p1/bitmaps/cursor.bmp
	-sudo cp bin/bitmaps/desktop.bmp /mnt/vdi_p1/bitmaps/desktop.bmp
	-sudo cp bin/bitmaps/panic.bmp /mnt/vdi_p1/bitmaps/panic.bmp

	-sudo cp bin/ProgFile/Game3D/obj.obj /mnt/vdi_p1/ProgFile/Game3D/obj.obj
	-sudo cp bin/ProgFile/Game3D/floor.obj /mnt/vdi_p1/ProgFile/Game3D/floor.obj
	-sudo cp bin/ProgFile/Game3D/map.bmp /mnt/vdi_p1/ProgFile/Game3D/map.bmp
	-sudo cp bin/ProgFile/Game3D/sky.bmp /mnt/vdi_p1/ProgFile/Game3D/sky.bmp

#	-sudo cp bin/sound.wav /mnt/vdi_p1/sound.wav
#	-sudo rm -f /mnt/vdi_p1/drivers/ac97.sys

	-sudo cp $(BUILD_DIR)/drivers/bga.sys /mnt/vdi_p1/drivers/bga.sys
	-sudo cp $(BUILD_DIR)/drivers/ac97.sys /mnt/vdi_p1/drivers/ac97.sys

	-sudo cp bin/fonts/segoeui.bin /mnt/vdi_p1/fonts/segoeui.bin

	-sudo cp $(BUILD_DIR)/user/MeMView.bin /mnt/vdi_p1/SYS32/MeMView.bin
	-sudo cp $(BUILD_DIR)/user/test.bin /mnt/vdi_p1/SYS32/test.bin
	-sudo cp $(BUILD_DIR)/user/Explorer.bin /mnt/vdi_p1/SYS32/Explorer.bin
	-sudo cp $(BUILD_DIR)/user/Terminal.bin /mnt/vdi_p1/SYS32/TERMINAL.BIN
	-sudo cp $(BUILD_DIR)/user/CLIHello.bin /mnt/vdi_p1/SYS32/CLIHello.bin
	-sudo cp $(BUILD_DIR)/user/Game3D.bin /mnt/vdi_p1/ProgFile/Game3D/Game3D.bin

# 	5. Cleanup
	sudo umount /mnt/vdi_p1
	sudo qemu-nbd --disconnect /dev/nbd0

runvb: iso
	(killall VirtualBox && sleep 1) || true
	VirtualBox --startvm 'My Operating System' &

iso: $(KERNEL_BIN)
	mkdir -p $(BUILD_DIR)/iso/boot/grub
	mkdir -p $(BUILD_DIR)/iso/boot/fonts
	cp $(KERNEL_BIN) $(BUILD_DIR)/iso/boot/kernel.bin
#	cp bin/fonts/segoeui.bin $(BUILD_DIR)/iso/boot/fonts/segoeui.bin
	echo 'set timeout=0' > $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo 'set default=0' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
#	echo 'set gfxmode=1152x864x32' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
#	echo 'set gfxpayload=keep' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo 'terminal_output gfxterm' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo 'menuentry "My Operating System" {' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '  multiboot /boot/kernel.bin' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
#	echo '  module /boot/fonts/segoeui.bin' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '  boot' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '}' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue --output=$(KERNEL_ISO) --modules="video gfxterm video_bochs video_cirrus" $(BUILD_DIR)/iso
	rm -rf $(BUILD_DIR)/iso

prog:
	make hdd
	@echo "[PROG] Waiting $(RUNQ_DELAY)s before runq..."
	@sleep $(RUNQ_DELAY)
	make runq

.PHONY: clean build hdd check runq run prog runvb iso check-style check-bugs check-headers check-eof fix-style install

# -----------------------------------
# CODE QUALITY TOOLS
# Check style (Report Only)
check-style:
	@echo "--- Checking Code Formatting ---"
	@find . -name "*.cpp" -o -name "*.c" -o -name "*.h" | xargs clang-format --dry-run -Werror
	@echo "Style check passed."

# Check Logic (Report Only)
check-bugs:
	@echo "--- Static Analysis ---"
	@cppcheck --enable=warning,performance,portability \
		--suppress=missingIncludeSystem \
		--inline-suppr \
		--quiet \
		.

# Check Headers (Report Only)
check-headers:
	@./check_headers.sh

# Check EOF (Report Only)
check-eof:
	@./check_eof.sh

# Master Check
check: check-style check-bugs check-eof check-headers

# Auto-Fix-Style
fix-style:
	@echo "--- Applying Auto-Formatting ---"
	@find . -name "*.cpp" -o -name "*.c" -o -name "*.h" | xargs clang-format -i -style=file
	@echo "Code formatted."
