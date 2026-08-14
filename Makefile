KDBG_ENABLE ?= 1
KDBG_LEVEL ?= 1

GPP_PARAMS = -m32 -g -ffreestanding -Iinclude -I. -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-common -fno-omit-frame-pointer -DKDBG_ENABLE=$(KDBG_ENABLE) -DKDBG_LEVEL=$(KDBG_LEVEL)
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
	$(BUILD_DIR)/obj/core/filesystem/File.o \
	$(BUILD_DIR)/obj/core/filesystem/msdospart.o \
	$(BUILD_DIR)/obj/core/filesystem/FatFs/ff.o \
	$(BUILD_DIR)/obj/core/filesystem/FatFs/ffunicode.o \
	$(BUILD_DIR)/obj/core/filesystem/FatFs/diskio.o \
	$(BUILD_DIR)/obj/core/filesystem/FatFs/FatFsWrapper.o \
	$(BUILD_DIR)/obj/core/CrashReporter.o \
	$(BUILD_DIR)/obj/core/constructors.o \
	$(BUILD_DIR)/obj/core/gdt.o \
	$(BUILD_DIR)/obj/core/globals.o \
	$(BUILD_DIR)/obj/core/interrupts.o \
	$(BUILD_DIR)/obj/core/KernelSymbolResolver.o \
	$(BUILD_DIR)/obj/core/kstack.o \
	$(BUILD_DIR)/obj/core/memory.o \
	$(BUILD_DIR)/obj/core/tlsf/tlsf.o \
	$(BUILD_DIR)/obj/core/paging.o \
	$(BUILD_DIR)/obj/core/pci.o \
	$(BUILD_DIR)/obj/core/pmm.o \
	$(BUILD_DIR)/obj/core/ports.o \
	$(BUILD_DIR)/obj/core/scheduler.o \
	$(BUILD_DIR)/obj/core/syscalls.o \
	$(BUILD_DIR)/obj/debug.o \
	$(BUILD_DIR)/obj/gui/bmp.o \
	$(BUILD_DIR)/obj/gui/button.o \
	$(BUILD_DIR)/obj/gui/iconbutton.o \
	$(BUILD_DIR)/obj/gui/desktop.o \
	$(BUILD_DIR)/obj/gui/elements/window_action_button.o \
	$(BUILD_DIR)/obj/gui/elements/window_action_button_round.o \
	$(BUILD_DIR)/obj/gui/fonts/font.o \
	$(BUILD_DIR)/obj/core/fonts/ttf_render.o \
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
	$(BUILD_DIR)/obj/stdlib/math/w_sqrt.o \
	$(BUILD_DIR)/obj/stdlib/math/e_sqrt.o \
	$(BUILD_DIR)/obj/stdlib/math/e_atan2.o \
	$(BUILD_DIR)/obj/stdlib/math/e_pow.o \
	$(BUILD_DIR)/obj/stdlib/math/w_fmod.o \
	$(BUILD_DIR)/obj/stdlib/math/w_pow.o \
	$(BUILD_DIR)/obj/stdlib/math/w_cos.o \
	$(BUILD_DIR)/obj/stdlib/math/w_acos.o \
	$(BUILD_DIR)/obj/stdlib/math/e_fmod.o \
	$(BUILD_DIR)/obj/stdlib/math/k_rem_pio2.o \
	$(BUILD_DIR)/obj/stdlib/math/k_sin.o \
	$(BUILD_DIR)/obj/stdlib/math/k_cos.o \
	$(BUILD_DIR)/obj/stdlib/math/k_tan.o \
	$(BUILD_DIR)/obj/stdlib/math/s_scalbn.o \
	$(BUILD_DIR)/obj/stdlib/math/s_copysign.o \
	$(BUILD_DIR)/obj/stdlib/math/s_fabs.o \
	$(BUILD_DIR)/obj/stdlib/math/s_atan.o \
	$(BUILD_DIR)/obj/stdlib/math/s_floor.o \
	$(BUILD_DIR)/obj/stdlib/string/strlen.o \
	$(BUILD_DIR)/obj/stdlib/string/strcmp.o \
	$(BUILD_DIR)/obj/stdlib/string/strncmp.o \
	$(BUILD_DIR)/obj/stdlib/string/strcpy.o \
	$(BUILD_DIR)/obj/stdlib/string/strncpy.o \
	$(BUILD_DIR)/obj/stdlib/string/strcat.o \
	$(BUILD_DIR)/obj/stdlib/string/strncat.o \
	$(BUILD_DIR)/obj/stdlib/string/strchr.o \
	$(BUILD_DIR)/obj/stdlib/string/strrchr.o \
	$(BUILD_DIR)/obj/stdlib/string/strstr.o \
	$(BUILD_DIR)/obj/stdlib/string/strtok.o \
	$(BUILD_DIR)/obj/stdlib/string/strspn.o \
	$(BUILD_DIR)/obj/stdlib/string/strcspn.o \
	$(BUILD_DIR)/obj/stdlib/string/strpbrk.o \
	$(BUILD_DIR)/obj/stdlib/string/memmove.o \
	$(BUILD_DIR)/obj/stdlib/string/memchr.o \
	$(BUILD_DIR)/obj/stdlib/string/HexStrToInt.o \

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

# Linking the main kernel binary
$(KERNEL_BIN): linker.ld $(objects)
	mkdir -p $(BUILD_DIR)
	ld $(LD_PARAMS) -Map $(KERNEL_MAP) -T $< -o $@ $(objects)

# Installer kernel — minimal heap-free build (VGA text mode, no GUI/scheduler/paging)
# Uses stub headers under tools/installer/include/ to bypass the bloated kernel includes.
INSTALLER_INCLUDES = -Itools/installer/include -Iinclude -I.
INSTALLER_CFLAGS = -m32 -g -ffreestanding $(INSTALLER_INCLUDES) -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-common -fno-omit-frame-pointer -DKDBG_ENABLE=0 -DKDBG_LEVEL=0
INSTALLER_ASMFLAGS = $(ASM_NASM_PARAMS)

INSTALLER_OBJECTS = \
	$(BUILD_DIR)/obj/installer/loader.o \
	$(BUILD_DIR)/obj/installer/main.o \
	$(BUILD_DIR)/obj/installer/ata.o \
	$(BUILD_DIR)/obj/installer/diskio.o \
	$(BUILD_DIR)/obj/core/ports.o \
	$(BUILD_DIR)/obj/core/filesystem/FatFs/ff.o \
	$(BUILD_DIR)/obj/core/filesystem/FatFs/ffunicode.o \
	$(BUILD_DIR)/obj/core/constructors.o \
	$(BUILD_DIR)/obj/console.o \
	$(BUILD_DIR)/obj/stdlib.o \
	$(BUILD_DIR)/obj/stdlib/string/strlen.o \
	$(BUILD_DIR)/obj/stdlib/string/strcmp.o \
	$(BUILD_DIR)/obj/stdlib/string/strncmp.o \
	$(BUILD_DIR)/obj/stdlib/string/strcpy.o \
	$(BUILD_DIR)/obj/stdlib/string/strncpy.o \
	$(BUILD_DIR)/obj/stdlib/string/strcat.o \
	$(BUILD_DIR)/obj/stdlib/string/strncat.o \
	$(BUILD_DIR)/obj/stdlib/string/strchr.o \
	$(BUILD_DIR)/obj/stdlib/string/strrchr.o \
	$(BUILD_DIR)/obj/stdlib/string/strstr.o \
	$(BUILD_DIR)/obj/stdlib/string/strtok.o \
	$(BUILD_DIR)/obj/stdlib/string/strspn.o \
	$(BUILD_DIR)/obj/stdlib/string/strcspn.o \
	$(BUILD_DIR)/obj/stdlib/string/strpbrk.o \
	$(BUILD_DIR)/obj/stdlib/string/memmove.o \
	$(BUILD_DIR)/obj/stdlib/string/memchr.o \
	$(BUILD_DIR)/obj/stdlib/string/memcmp.o \
	$(BUILD_DIR)/obj/stdlib/string/memcpy.o \
	$(BUILD_DIR)/obj/stdlib/string/memset.o \
	$(BUILD_DIR)/obj/stdlib/string/HexStrToInt.o

# Installer-specific compile rules (with stub header override)
$(BUILD_DIR)/obj/installer/loader.o: tools/installer/loader.asm
	mkdir -p $(dir $@)
	nasm $(INSTALLER_ASMFLAGS) -o $@ $<

$(BUILD_DIR)/obj/installer/main.o: tools/installer/main.cpp
	mkdir -p $(dir $@)
	g++ $(INSTALLER_CFLAGS) -o $@ -c $<

$(BUILD_DIR)/obj/installer/ata.o: core/drivers/ata.cpp
	mkdir -p $(dir $@)
	g++ $(INSTALLER_CFLAGS) -o $@ -c $<

$(BUILD_DIR)/obj/installer/diskio.o: core/filesystem/FatFs/diskio.cpp
	mkdir -p $(dir $@)
	g++ $(INSTALLER_CFLAGS) -o $@ -c $<

# console.o is not part of the main kernel build, so compile it here
$(BUILD_DIR)/obj/console.o: console.cpp
	mkdir -p $(dir $@)
	g++ $(INSTALLER_CFLAGS) -o $@ -c $<

KERNEL_INSTALLER_BIN = $(BUILD_DIR)/kernel_installer.bin
KERNEL_INSTALLER_MAP = $(BUILD_DIR)/kernel_installer.map

$(KERNEL_INSTALLER_BIN): linker.ld $(INSTALLER_OBJECTS)
	mkdir -p $(BUILD_DIR)
	ld $(LD_PARAMS) -Map $(KERNEL_INSTALLER_MAP) -T $< -o $@ $(INSTALLER_OBJECTS)

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

build-run:
	make build
	@echo "[BUILD] Waiting $(RUNQ_DELAY)s to release the HDD file..."
	@sleep $(RUNQ_DELAY)
	make runq

runq: iso
	qemu-system-i386 -cdrom $(KERNEL_ISO) -boot d -vga std -serial stdio -m 1G \
	-drive file=$(QEMU_DISK),format=vdi

# Run with GDB debug
rungdb: iso
	qemu-system-i386 -cdrom $(KERNEL_ISO) -boot d -vga std -serial stdio -m 1G \
	-drive file=$(QEMU_DISK),format=vdi -s -S

# Connect GDB to the running QEMU instance
gdb:
	gdb -ex "target remote localhost:1234" -ex "continue" $(KERNEL_BIN)

run:
	make clean
	make
	make iso
	make runq

HDD_RAW = /tmp/hdd_raw.img

# Create a fresh disk image with two 512 MB FAT32 partitions
hddinit:
	sudo -v
	-sudo qemu-nbd --disconnect /dev/nbd0
	-rm -f $(HDD_RAW)
	qemu-img create -f raw $(HDD_RAW) 1G
	sudo modprobe nbd max_part=8
	sudo qemu-nbd -c /dev/nbd0 -f raw $(HDD_RAW)
	sudo parted -s /dev/nbd0 mklabel msdos
	sudo parted -s /dev/nbd0 mkpart primary fat32 1MiB 513MiB
	sudo parted -s /dev/nbd0 mkpart primary fat32 513MiB 100%
	sudo partprobe /dev/nbd0
	-sudo udevadm settle
	sudo mkfs.fat -F32 /dev/nbd0p1
	sudo mkfs.fat -F32 /dev/nbd0p2
	sudo qemu-nbd -d /dev/nbd0
	rm -f $(QEMU_DISK)
	qemu-img convert -f raw -O vdi $(HDD_RAW) $(QEMU_DISK)
	rm -f $(HDD_RAW)

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
	-sudo mkdir -p /mnt/vdi_p1/Hashx86/apps
	-sudo mkdir -p /mnt/vdi_p1/Hashx86/gfx
	-sudo mkdir -p /mnt/vdi_p1/Hashx86/fonts
	-sudo mkdir -p /mnt/vdi_p1/Hashx86/audio
	-sudo mkdir -p /mnt/vdi_p1/Hashx86/drivers

	-sudo cp $(KERNEL_MAP) /mnt/vdi_p1/kernel.map

	-sudo cp bin/audio/boot.wav /mnt/vdi_p1/Hashx86/audio/boot.wav
	-sudo cp bin/bitmaps/boot.bmp /mnt/vdi_p1/Hashx86/gfx/boot.bmp
	-sudo cp bin/bitmaps/icon.bmp /mnt/vdi_p1/Hashx86/gfx/icon.bmp
	-sudo cp bin/bitmaps/cursor.bmp /mnt/vdi_p1/Hashx86/gfx/cursor.bmp
	-sudo cp bin/bitmaps/desktop.bmp /mnt/vdi_p1/Hashx86/gfx/desktop.bmp
	-sudo cp bin/bitmaps/panic.bmp /mnt/vdi_p1/Hashx86/gfx/panic.bmp

	-sudo mkdir -p /mnt/vdi_p1/Apps/Game3D
	-sudo cp bin/ProgFile/Game3D/obj.obj /mnt/vdi_p1/Apps/Game3D/obj.obj
	-sudo cp bin/ProgFile/Game3D/floor.obj /mnt/vdi_p1/Apps/Game3D/floor.obj
	-sudo cp bin/ProgFile/Game3D/map.bmp /mnt/vdi_p1/Apps/Game3D/map.bmp
	-sudo cp bin/ProgFile/Game3D/sky.bmp /mnt/vdi_p1/Apps/Game3D/sky.bmp

	-sudo cp $(BUILD_DIR)/drivers/bga.sys /mnt/vdi_p1/Hashx86/drivers/bga.sys
	-sudo cp $(BUILD_DIR)/drivers/ac97.sys /mnt/vdi_p1/Hashx86/drivers/ac97.sys

	-sudo cp bin/fonts/segoeui.ttf /mnt/vdi_p1/Hashx86/fonts/segoeui.ttf
	-sudo cp bin/fonts/segoeuib.ttf /mnt/vdi_p1/Hashx86/fonts/segoeuib.ttf
	-sudo cp bin/fonts/segoeuii.ttf /mnt/vdi_p1/Hashx86/fonts/segoeuii.ttf
	-sudo cp bin/fonts/segoeuiz.ttf /mnt/vdi_p1/Hashx86/fonts/segoeuiz.ttf
	-sudo cp bin/fonts/CascadiaMono-Regular.ttf /mnt/vdi_p1/Hashx86/fonts/CascadiaMono-Regular.ttf
	-sudo cp bin/fonts/CascadiaMono-Bold.ttf /mnt/vdi_p1/Hashx86/fonts/CascadiaMono-Bold.ttf
	-sudo cp bin/fonts/CascadiaMono-Italic.ttf /mnt/vdi_p1/Hashx86/fonts/CascadiaMono-Italic.ttf
	-sudo cp bin/fonts/CascadiaMono-BoldItalic.ttf /mnt/vdi_p1/Hashx86/fonts/CascadiaMono-BoldItalic.ttf
	-sudo cp bin/fonts/fa-solid-900.ttf /mnt/vdi_p1/Hashx86/fonts/fa-solid-900.ttf

	-sudo cp $(BUILD_DIR)/user/MeMView.bin /mnt/vdi_p1/Hashx86/apps/MeMView.bin
	-sudo cp $(BUILD_DIR)/user/test.bin /mnt/vdi_p1/Hashx86/apps/test.bin
	-sudo cp $(BUILD_DIR)/user/Explorer.bin /mnt/vdi_p1/Hashx86/apps/Explorer.bin
	-sudo cp $(BUILD_DIR)/user/Terminal.bin /mnt/vdi_p1/Hashx86/apps/Terminal.bin
	-sudo cp $(BUILD_DIR)/user/CLIHello.bin /mnt/vdi_p1/Hashx86/apps/CLIHello.bin
	-sudo cp $(BUILD_DIR)/user/Game3D.bin /mnt/vdi_p1/Apps/Game3D/Game3D.bin
	-sudo cp $(BUILD_DIR)/user/Notepad.bin /mnt/vdi_p1/Hashx86/apps/Notepad.bin

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

INSTALLER_PAK = $(BUILD_DIR)/installer.pak
INSTALLER_ISO = $(BUILD_DIR)/installer.iso
INSTALLER_CORE_IMG = $(BUILD_DIR)/installer_core.img
INSTALLER_PAK_DIR = $(BUILD_DIR)/installer_data
INSTALLER_GRUB_PLATFORM_DIR ?= /usr/lib/grub/i386-pc

# Build GRUB core.img for the installer (fat + biosdisk + part_msdos)
$(INSTALLER_CORE_IMG):
	grub-mkimage -O i386-pc -o $@ --prefix='(hd0,msdos1)/boot/grub' fat part_msdos biosdisk

# Always-regenerated prerequisite used by the installer manifest rule.
FORCE:

# Build user programs and drivers (the sub-makes are themselves incremental) so
# the installer manifest scans the freshly built binaries in build/user and
# build/drivers. Declared output lists mirror user_prog/Makefile (SUBDIRS) and
# drivers/Makefile (DRIVERS); stale binaries not on those lists are pruned
# before the incremental sub-make runs so obsolete files never reach the package.
USER_APPS = MeMView test Explorer Terminal Game3D CLIHello
USER_BINS = $(addprefix $(BUILD_DIR)/user/,$(addsuffix .bin,$(USER_APPS)))
DRIVER_SYS = $(addprefix $(BUILD_DIR)/drivers/,bga.sys ac97.sys)

user:
	@for bin in $(BUILD_DIR)/user/*.bin; do \
		[ -e "$$bin" ] || continue; \
		case " $(USER_BINS) " in *" $$bin "*) ;; *) \
			echo "Removing stale $$bin"; rm -f "$$bin";; \
		esac; \
	done
	@$(MAKE) -C user_prog

drivers:
	@for sys in $(BUILD_DIR)/drivers/*.sys; do \
		[ -e "$$sys" ] || continue; \
		case " $(DRIVER_SYS) " in *" $$sys "*) ;; *) \
			echo "Removing stale $$sys"; rm -f "$$sys";; \
		esac; \
	done
	@$(MAKE) -C drivers

# Build host-side packer tool
$(BUILD_DIR)/packer: tools/installer/packer.cpp include/core/pak.h
	mkdir -p $(BUILD_DIR)
	g++ -std=c++11 -o $@ $<

# Prepare installer data directory (mirrors the HDD layout)
# A stamp file (rather than the directory itself) is the target so that
# changes to binaries, fonts, and assets reliably trigger a repack.
INSTALLER_PAK_STAMP = $(BUILD_DIR)/.installer_data.stamp
# Content manifest covering every staged installer path (kernels, binaries,
# drivers, fonts, bitmaps, audio, Game3D assets, the map, and this Makefile).
# It is regenerated on every invocation but only rewritten when its contents
# change, so the stamp below rebuilds the package exactly when inputs change.
INSTALLER_PAK_MANIFEST = $(BUILD_DIR)/.installer_pak.manifest
$(INSTALLER_PAK_MANIFEST): FORCE $(KERNEL_BIN) $(INSTALLER_CORE_IMG) $(KERNEL_MAP) user drivers
	@find $(KERNEL_BIN) $(INSTALLER_CORE_IMG) $(KERNEL_MAP) Makefile \
	  $(BUILD_DIR)/user $(BUILD_DIR)/drivers bin/fonts bin/bitmaps bin/audio \
	  bin/ProgFile/Game3D $(INSTALLER_GRUB_PLATFORM_DIR) -type f -printf '%p %s %T@\n' 2>/dev/null | sort > $@.tmp
	@cmp -s $@.tmp $@ || cp $@.tmp $@
	@rm -f $@.tmp
$(INSTALLER_PAK_STAMP): $(INSTALLER_PAK_MANIFEST) $(KERNEL_BIN) $(INSTALLER_CORE_IMG) $(KERNEL_MAP) Makefile
	# Invalidate the stamp first so a failed or interrupted rebuild cannot
	# leave a stale stamp that lets later make invocations skip rebuilding.
	rm -f $(INSTALLER_PAK_STAMP)
	rm -rf $(INSTALLER_PAK_DIR)
	mkdir -p $(INSTALLER_PAK_DIR)
	# Validate the GRUB platform directory exists before copying any GRUB files
	test -d $(INSTALLER_GRUB_PLATFORM_DIR) || \
		(echo "GRUB platform directory missing: $(INSTALLER_GRUB_PLATFORM_DIR)"; \
		 echo "Install grub-pc-bin or set INSTALLER_GRUB_PLATFORM_DIR"; exit 1)
	# GRUB boot files
	mkdir -p $(INSTALLER_PAK_DIR)/boot/grub
	cp $(INSTALLER_GRUB_PLATFORM_DIR)/boot.img $(INSTALLER_PAK_DIR)/boot/
	cp $(INSTALLER_CORE_IMG) $(INSTALLER_PAK_DIR)/boot/core.img
	# grub.cfg for the installed system
	echo 'set timeout=0' > $(INSTALLER_PAK_DIR)/boot/grub/grub.cfg
	echo 'set default=0' >> $(INSTALLER_PAK_DIR)/boot/grub/grub.cfg
	echo 'terminal_output gfxterm' >> $(INSTALLER_PAK_DIR)/boot/grub/grub.cfg
	echo '' >> $(INSTALLER_PAK_DIR)/boot/grub/grub.cfg
	echo 'menuentry "Hashx86-OS" {' >> $(INSTALLER_PAK_DIR)/boot/grub/grub.cfg
	echo '  multiboot /boot/kernel.bin' >> $(INSTALLER_PAK_DIR)/boot/grub/grub.cfg
	echo '  boot' >> $(INSTALLER_PAK_DIR)/boot/grub/grub.cfg
	echo '}' >> $(INSTALLER_PAK_DIR)/boot/grub/grub.cfg
	# All GRUB platform modules (needed by core.img + grub.cfg)
	mkdir -p $(INSTALLER_PAK_DIR)/boot/grub/i386-pc
	cp $(INSTALLER_GRUB_PLATFORM_DIR)/*.mod $(INSTALLER_PAK_DIR)/boot/grub/i386-pc/
	cp $(INSTALLER_GRUB_PLATFORM_DIR)/*.lst $(INSTALLER_PAK_DIR)/boot/grub/i386-pc/ 2>/dev/null || true
	# Kernel (for booting after install — normal kernel, not the installer)
	mkdir -p $(INSTALLER_PAK_DIR)/boot
	cp $(KERNEL_BIN) $(INSTALLER_PAK_DIR)/boot/kernel.bin
	cp $(KERNEL_MAP) $(INSTALLER_PAK_DIR)/
	# Apps (all except Game3D which goes to its own directory)
	mkdir -p $(INSTALLER_PAK_DIR)/Hashx86/apps
	for app in $(BUILD_DIR)/user/*.bin; do \
		base=$$(basename $$app); \
		if [ "$$base" != "Game3D.bin" ]; then \
			cp $$app $(INSTALLER_PAK_DIR)/Hashx86/apps/ || exit 1; \
		fi; \
	done
	# Game3D — binary + data together
	mkdir -p $(INSTALLER_PAK_DIR)/Apps/Game3D
	cp $(BUILD_DIR)/user/Game3D.bin $(INSTALLER_PAK_DIR)/Apps/Game3D/
	cp bin/ProgFile/Game3D/* $(INSTALLER_PAK_DIR)/Apps/Game3D/
	# Graphics
	mkdir -p $(INSTALLER_PAK_DIR)/Hashx86/gfx
	cp bin/bitmaps/*.bmp $(INSTALLER_PAK_DIR)/Hashx86/gfx/
	# Fonts
	mkdir -p $(INSTALLER_PAK_DIR)/Hashx86/fonts
	cp bin/fonts/*.ttf $(INSTALLER_PAK_DIR)/Hashx86/fonts/
	# Audio
	mkdir -p $(INSTALLER_PAK_DIR)/Hashx86/audio
	cp bin/audio/boot.wav $(INSTALLER_PAK_DIR)/Hashx86/audio/
	# Drivers
	mkdir -p $(INSTALLER_PAK_DIR)/Hashx86/drivers
	cp $(BUILD_DIR)/drivers/*.sys $(INSTALLER_PAK_DIR)/Hashx86/drivers/
	touch $@

# Build installer.pak from the prepared data directory
$(INSTALLER_PAK): $(BUILD_DIR)/packer $(INSTALLER_PAK_STAMP)
	$(BUILD_DIR)/packer $(INSTALLER_PAK_DIR) $(INSTALLER_PAK)

# Build the installer ISO (uses kernel_installer.bin, not the main kernel)
build-installer: $(KERNEL_INSTALLER_BIN) $(INSTALLER_PAK)
	mkdir -p $(BUILD_DIR)/iso/boot/grub
	cp $(KERNEL_INSTALLER_BIN) $(BUILD_DIR)/iso/boot/kernel.bin
	cp $(INSTALLER_PAK) $(BUILD_DIR)/iso/installer.pak
	echo 'set timeout=0' > $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo 'set default=0' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo 'terminal_output gfxterm' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo 'menuentry "Hashx86-OS Installer" {' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '  multiboot /boot/kernel.bin' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '  module /installer.pak installer.pak' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '  boot' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '}' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue --output=$(INSTALLER_ISO) --modules="video gfxterm video_bochs video_cirrus" $(BUILD_DIR)/iso
	rm -rf $(BUILD_DIR)/iso

# Run installer in QEMU (uses a blank disk image; created only when absent so
# re-runs with an existing disk keep the previous installation).
run-installer: build-installer
	test -f $(BUILD_DIR)/install_disk.qcow2 || qemu-img create -f qcow2 $(BUILD_DIR)/install_disk.qcow2 1G
	qemu-system-i386 -cdrom $(INSTALLER_ISO) -boot d -vga std -serial stdio -m 1G \
	  -drive file=$(BUILD_DIR)/install_disk.qcow2,format=qcow2

prog:
	make hdd
	@echo "[PROG] Waiting $(RUNQ_DELAY)s before runq..."
	@sleep $(RUNQ_DELAY)
	make runq

.PHONY: clean build build-run hdd hddinit check runq run prog runvb iso build-installer run-installer check-style check-bugs check-headers check-eof fix-style install newapp FORCE user drivers

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

newapp:
	@bash newapp.sh
