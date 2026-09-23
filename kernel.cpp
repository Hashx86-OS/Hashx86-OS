/**
 * @file        kernel.cpp
 * @brief       Main Kernel of #x86
 *
 * @date        24/02/2025
 * @version     1.2.0-beta
 */

#define KDBG_COMPONENT "KERNEL"
#include <kernel.h>
#include <core/filesystem/Paths.h>
#include <core/kstack.h>
#include <gui/bootanim.h>

#define DEBUG_ENABLED TRUE;
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

KERNEL_MEMORY_MAP g_kmap;

extern "C" uint32_t pci_find_bar0(uint16_t vendor, uint16_t device);
extern "C" void __cxa_pure_virtual() {
    HALT("Pure Virtual Function Called! System Halted.");
}

int get_kernel_memory_map(KERNEL_MEMORY_MAP* kmap, MultibootInfo* mboot_info) {
    if (kmap == NULL) return -1;
    if (!(mboot_info->flags & (1 << 6))) return -1;  // memory map not present in multiboot info
    kmap->kernel.k_start_addr = (uint32_t)&__kernel_section_start;
    kmap->kernel.k_end_addr = (uint32_t)&__kernel_section_end;
    kmap->kernel.k_len = ((uint32_t)&__kernel_section_end - (uint32_t)&__kernel_section_start);

    kmap->kernel.text_start_addr = (uint32_t)&__kernel_text_section_start;
    kmap->kernel.text_end_addr = (uint32_t)&__kernel_text_section_end;
    kmap->kernel.text_len =
        ((uint32_t)&__kernel_text_section_end - (uint32_t)&__kernel_text_section_start);

    kmap->kernel.data_start_addr = (uint32_t)&__kernel_data_section_start;
    kmap->kernel.data_end_addr = (uint32_t)&__kernel_data_section_end;
    kmap->kernel.data_len =
        ((uint32_t)&__kernel_data_section_end - (uint32_t)&__kernel_data_section_start);

    kmap->kernel.rodata_start_addr = (uint32_t)&__kernel_rodata_section_start;
    kmap->kernel.rodata_end_addr = (uint32_t)&__kernel_rodata_section_end;
    kmap->kernel.rodata_len =
        ((uint32_t)&__kernel_rodata_section_end - (uint32_t)&__kernel_rodata_section_start);

    kmap->kernel.bss_start_addr = (uint32_t)&__kernel_bss_section_start;
    kmap->kernel.bss_end_addr = (uint32_t)&__kernel_bss_section_end;
    kmap->kernel.bss_len =
        ((uint32_t)&__kernel_bss_section_end - (uint32_t)&__kernel_bss_section_start);

    kmap->system.total_memory = mboot_info->mem_lower + mboot_info->mem_upper;

    for (int offset = 0; offset < (int)mboot_info->mmap_length;) {
        MULTIBOOT_MEMORY_MAP* mmap =
            (MULTIBOOT_MEMORY_MAP*)((uint32_t)mboot_info->mmap_addr + offset);
        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) {
            offset += mmap->size + sizeof(mmap->size);
            continue;
        }
        // Check that this available range contains the kernel,
        // not just starts at the same address
        if (mmap->addr_low <= kmap->kernel.text_start_addr &&
            mmap->addr_low + mmap->len_low >= kmap->kernel.k_end_addr) {
            // set available memory starting from end of our kernel, leaving 1MB size for functions
            // execution
            uint32_t suggested_start = kmap->kernel.k_end_addr + 1024 * 1024;
            kmap->available.start_addr = (suggested_start <= mmap->addr_low + mmap->len_low)
                                             ? suggested_start
                                             : kmap->kernel.k_end_addr;
            kmap->available.end_addr = mmap->addr_low + mmap->len_low;
            // get available memory in bytes
            kmap->available.size = kmap->available.end_addr - kmap->available.start_addr;
            return 0;
        }
        offset += mmap->size + sizeof(mmap->size);
    }

    return -1;
}

void display_kernel_memory_map(KERNEL_MEMORY_MAP* kmap) {
    KDBG2("MemoryMap section=KERNEL start=0x%x end=0x%x len=%d", kmap->kernel.k_start_addr,
          kmap->kernel.k_end_addr, kmap->kernel.k_len);
    KDBG2("MemoryMap section=TEXT   start=0x%x end=0x%x len=%d", kmap->kernel.text_start_addr,
          kmap->kernel.text_end_addr, kmap->kernel.text_len);
    KDBG2("MemoryMap section=DATA   start=0x%x end=0x%x len=%d", kmap->kernel.data_start_addr,
          kmap->kernel.data_end_addr, kmap->kernel.data_len);
    KDBG2("MemoryMap section=RODATA start=0x%x end=0x%x len=%d", kmap->kernel.rodata_start_addr,
          kmap->kernel.rodata_end_addr, kmap->kernel.rodata_len);
    KDBG2("MemoryMap section=BSS    start=0x%x end=0x%x len=%d", kmap->kernel.bss_start_addr,
          kmap->kernel.bss_end_addr, kmap->kernel.bss_len);

    KDBG1("SystemMemory total=%d KB available_size=%d", kmap->system.total_memory,
          kmap->available.size);
    KDBG1("AvailableRAM start=0x%x end=0x%x", kmap->available.start_addr, kmap->available.end_addr);
}

void init_memory(MultibootInfo* mbinfo) {
    memset(&g_kmap, 0, sizeof(KERNEL_MEMORY_MAP));
    if (get_kernel_memory_map(&g_kmap, mbinfo) != 0) {
        KDBG1("ERROR: get_kernel_memory_map() failed - no usable RAM span found. Halting.");
        HALT("CRITICAL: No usable memory map available!\n");
    }

    display_kernel_memory_map(&g_kmap);

    // Initialize PMM at end of Kernel (Respect BSS/Stack)
    // Use bss_end_addr to ensure we are past the stack
    uint32_t heap_start_addr = g_kmap.kernel.bss_end_addr;

    // Safety Fallback: Use k_end_addr if bss calc seems wrong
    if (g_kmap.kernel.k_end_addr > heap_start_addr) {
        heap_start_addr = g_kmap.kernel.k_end_addr;
    }

    if (heap_start_addr < 0x200000) {
        heap_start_addr = 0x200000;
    }

    // Add 4MB padding to be safe.
    heap_start_addr += 4 * 1024 * 1024;

    if (mbinfo->flags & (1 << 3)) {  // mods flag present
        struct multiboot_module* modules = (struct multiboot_module*)mbinfo->mods_addr;
        if (mbinfo->mods_count > 0) {
            for (int mod_idx = 0; mod_idx < (int)mbinfo->mods_count; mod_idx++) {
                uint32_t mod_end = modules[mod_idx].mod_end;
                if (mod_end > heap_start_addr && mod_end <= g_kmap.available.end_addr) {
                    heap_start_addr = mod_end;
                }
            }
        }
    }

    // Align to Page Boundary
    if ((heap_start_addr & 0xFFF) != 0) {
        heap_start_addr = (heap_start_addr & 0xFFFFF000) + 0x1000;
    }

    // initialize PMM
    pmm_init(heap_start_addr, g_kmap.available.end_addr);
    // Calculate how big the bitmap
    uint32_t bitmap_size = (g_kmap.available.end_addr / PMM_BLOCK_SIZE) / 8;
    // Align bitmap size to page boundary so the free region starts on a clean page
    if (bitmap_size & 0xFFF) {
        bitmap_size = (bitmap_size & 0xFFFFF000) + 0x1000;
    }
    // Mark FREE Region, but SKIP the bitmap!
    pmm_init_region(heap_start_addr + bitmap_size, g_kmap.available.end_addr);

    // Reserve the dedicated kernel-stack zone BEFORE the heap is carved out,
    // so the heap allocator and user allocations skip this region entirely.
    // Kernel thread stacks live here (with guard pages), never in the heap.
    if (kstack_init() != 0) {
        HALT("CRITICAL: Failed to initialize kernel stack zone!\n");
    }

    // HEAP ALLOCATION
    uint32_t actual_heap_start = heap_start_addr + bitmap_size;
    if ((actual_heap_start & 0xFFF) != 0) {
        actual_heap_start = (actual_heap_start & 0xFFFFF000) + 0x1000;
    }

    // Reserve the bulk of low memory for identity-mapped PMM allocations
    // (page tables, user stacks, process directories, ELF loading pages, sys_brk).
    // These MUST stay below 256MB because the kernel accesses them via
    // identity-mapped physical addresses after paging is activated.
    // The kernel heap only needs a modest amount for kmalloc (PCBs, TCBs, buffers, etc.).
    uint32_t heap_size_bytes = 64 * 1024 * 1024;  // 64 MB for kernel objects, GUI, font data
    uint32_t blocks_needed = heap_size_bytes / PMM_BLOCK_SIZE;

    KDBG1("Kernel Heap: Start=0x%x Size=%d MB (%d blocks)", actual_heap_start,
          heap_size_bytes / (1024 * 1024), blocks_needed);

    // Allocate
    void* heap_start = pmm_alloc_blocks(blocks_needed);

    if (heap_start == NULL) {
        HALT("CRITICAL: Failed to allocate calculated heap!\n");
    }

    // Force Heap Alignment (Crucial for Paging)
    uint32_t heap_val = (uint32_t)heap_start;
    if ((heap_val & 0xFFF) != 0) {
        heap_val = (heap_val + 0xFFF) & ~0xFFF;
        blocks_needed--;
    }
    heap_start = (void*)heap_val;

    size_t heap_size = blocks_needed * PMM_BLOCK_SIZE;
    void* heap_end = (void*)((uint32_t)heap_start + heap_size);

    KDBG1("Kernel Heap: 0x%x - 0x%x (%d MB)", heap_start, heap_end, heap_size / 1024 / 1024);

    kheap_init(heap_start, heap_end);
}

void InitializePIT(uint32_t frequency) {
    // The PIT's input clock is approximately 1.193182 MHz
    uint32_t divisor = 1193180 / frequency;

    // Send the command byte.
    // 0x36 = 00 11 01 10
    // 00 (Channel 0)
    // 11 (Access mode: lobyte/hibyte)
    // 011 (Mode 3: Square Wave Generator)
    // 0 (Binary mode)
    outb(PIT_COMMAND_PORT, 0x36);

    // Divisor has to be sent byte-wise, so split it into Low and High bytes.
    uint8_t low = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    // Send the frequency divisor
    outb(PIT_CHANNEL0_PORT, low);
    outb(PIT_CHANNEL0_PORT, high);

    KDBG1("PIT Initialized at %d Hz", (int32_t)frequency);
}

void init_pci(FileSystem* boot_partition, DriverManager* driverManager) {
    KDBG1("Initializing Drivers (PCI Scan)...");
    // ---------------------------------------------------------
    // Dynamic Graphics Loading (with PCI)
    // ---------------------------------------------------------

    // Scan for known BGA/SVGA Vendors
    // 0x1234:0x1111 = Bochs / QEMU
    // 0x80EE:0xBEEF = VirtualBox Graphics Adapter
    // 0x15AD:0x0405 = VMware SVGA II (often used by VBox)

    // Check if BGA Hardware Exists via PCI
    PeripheralComponentInterconnectController* pciCheck =
        new PeripheralComponentInterconnectController();
    if (!pciCheck) {
        HALT("CRITICAL: Failed to allocate PCI controller!\n");
    }
    PeripheralComponentInterconnectDeviceDescriptor* dev = nullptr;

    dev = pciCheck->FindHardwareDevice(0x1234, 0x1111);
    if (dev == nullptr || dev->vendor_id == 0) dev = pciCheck->FindHardwareDevice(0x80EE, 0xBEEF);
    if (dev == nullptr || dev->vendor_id == 0) dev = pciCheck->FindHardwareDevice(0x15AD, 0x0405);

    // Only Proceed if Device Found
    if (dev != nullptr && dev->vendor_id != 0) {
        const char* BGAfilename = PATH_BGA_DRIVER;

        KDBG1("BGA Hardware Detected (ID: %x:%x). Loading Driver... [%s]", dev->vendor_id,
              dev->device_id, BGAfilename);
        File* bgaFile = boot_partition->Open(BGAfilename);

        if (bgaFile) {
            void* entryPoint =
                ModuleLoader::LoadMatchingDriver(bgaFile, dev->vendor_id, dev->device_id);

            if (entryPoint) {
                GetDriverInstancePtr createDriver = (GetDriverInstancePtr)entryPoint;
                void* raw = createDriver();  // Create the object

                if (raw) {
                    Driver* drv = (Driver*)raw;
                    driverManager->AddDriver(drv);

                    // SAFE CAST
                    GraphicsDriver* newScreen = drv->AsGraphicsDriver();

                    if (newScreen) {
                        GraphicsDriver* oldDR = g_GraphicsDriver;

                        // Update Local Reference
                        g_GraphicsDriver = newScreen;

                        // UPDATE GLOBAL VARIABLE
                        g_GraphicsDriver = newScreen;

                        // Copy Old Screen to New Screen
                        int32_t x, y;
                        g_GraphicsDriver->GetScreenCenter(oldDR->GetWidth(), oldDR->GetHeight(), x,
                                                          y);

                        if (oldDR->GetBackBuffer()) {
                            g_GraphicsDriver->DrawBitmap(x, y, oldDR->GetBackBuffer(),
                                                         oldDR->GetWidth(), oldDR->GetHeight());
                        }
                        drv->Activate();
                        g_GraphicsDriver->Flush();
                        KDBG1("BGA Module Loaded Successfully.");
                    } else {
                        KDBG1("Error: Driver loaded, but is not a GraphicsDriver!");
                    }
                }
            }

            bgaFile->Close();
            delete bgaFile;
        } else {
            KDBG1("Hardware found, but %s missing!", BGAfilename);
        }
    } else {
        KDBG1("No BGA Hardware found. Skipping driver load.");
    }

    // Clean up dev for next search
    if (dev) delete dev;
    dev = nullptr;

    // ---------------------------------------------------------
    // 2. Dynamic Audio Loading
    // ---------------------------------------------------------

    // Check for AC97 (0x8086:0x2415)
    dev = pciCheck->FindHardwareDevice(0x8086, 0x2415);

    if (dev != nullptr && dev->vendor_id != 0) {
        const char* driverName = PATH_AC97_DRIVER;
        KDBG1("Audio Hardware Detected. Loading... [%s]", driverName);

        File* drvFile = boot_partition->Open(driverName);
        if (drvFile) {
            void* entryPoint =
                ModuleLoader::LoadMatchingDriver(drvFile, dev->vendor_id, dev->device_id);
            if (entryPoint) {
                GetDriverInstancePtr createDriver = (GetDriverInstancePtr)entryPoint;
                void* raw = createDriver();
                if (raw) {
                    Driver* drv = (Driver*)raw;

                    // CRITICAL: Activate() registers the IRQ.
                    // InterruptManager MUST exist before this line runs.
                    drv->Activate();

                    driverManager->AddDriver(drv);
                    AudioDriver* audio = drv->AsAudioDriver();

                    if (audio) {
                        KDBG1("Initializing Audio Mixer...");
                        // Create the Mixer and link it to the driver
                        g_AudioMixer = new AudioMixer(audio);
                        if (!g_AudioMixer) {
                            HALT("CRITICAL: Failed to allocate AudioMixer!\n");
                        }

                        // Set Master Volume
                        audio->SetVolume(90);
                    }
                }
            }
            drvFile->Close();
            delete drvFile;
        }
    }
    delete pciCheck;
};

void pDesktop(void* arg) {
    DesktopArgs* args = (DesktopArgs*)arg;

#ifdef DEBUG_ENABLED
    KDBG1("GUI task started");
    if (!args) {
        HALT("Error: args is null");
    }
    if (!args->screen) {
        HALT("Error: args->vga is null");
    }
    if (!args->desktop) {
        HALT("Error: args->desktop is null");
    }

#endif

    GraphicsDriver* screen = args->screen;
    Desktop* desktop = args->desktop;

    Font* VBE_font = FontManager::activeInstance->getNewFont();
    VBE_font->setSize(MEDIUM);
/* 
    
    // Font demo window
    Window* fontWin = new Window(desktop, 50, 50, 320, 260);
    if (fontWin) {
        fontWin->setWindowTitle("Font Demo");

        struct FontDemoEntry { const char* text; int y; FontSize size; FontType type; };
        FontDemoEntry entries[] = {
            {"Regular TINY",       30, TINY,   REGULAR},
            {"Regular SMALL",      52, SMALL,  REGULAR},
            {"Regular MEDIUM",     76, MEDIUM, REGULAR},
            {"Regular LARGE",     104, LARGE,  REGULAR},
            {"Bold SMALL",        134, SMALL,  BOLD},
            {"Italic SMALL",      156, SMALL,  ITALIC},
            {"Bold Italic MEDIUM",180, MEDIUM, BOLD_ITALIC},
            {"Regular XLARGE",    212, XLARGE, REGULAR},
        };
        for (int i = 0; i < (int)(sizeof(entries)/sizeof(entries[0])); i++) {
            Label* lbl = new Label(fontWin, 10, entries[i].y, 300, 30, entries[i].text);
            if (lbl) {
                lbl->setSize(entries[i].size);
                lbl->setType(entries[i].type);
                fontWin->AddChild(lbl);
            }
        }
    }

    desktop->AddChild(fontWin); */

    while (true) {
        // Only swap buffers if something actually changed
        uint32_t start = timerTicks;

        // If a fullscreen app is running, skip desktop drawing
        if (g_stop_gui_rendering) {
            screen->Flush();
            Scheduler::activeInstance->Sleep(16);
            continue;
        }

        // Periodic clock update check for taskbar
        static uint32_t lastClockTick = 0;
        if (timerTicks - lastClockTick >= 1000) {
            lastClockTick = timerTicks;
            desktop->MarkDirty();
        }

        if (desktop->isDirty || desktop->MouseMoved()) {
            desktop->Draw(screen);
            uint32_t end = timerTicks;
            uint32_t diff = (uint32_t)(end - start);
            char buf[32];
            itoa(diff, buf, 16, sizeof(buf));
            screen->FillRectangle(5, 5, 50, 35, 0x0);
            screen->DrawString(10, 10, buf, VBE_font, 0xFFFFFFFF);
            screen->DrawString(25, 10, "ms", VBE_font, 0xFFFFFFFF);
            screen->Flush();
        } else {
            Scheduler::activeInstance->Sleep(16);
        }
    }
}

// Boot worker thread: slow/filesystem stages, then starts the desktop thread.
// Returning runs ThreadExit -> KillProcess for this worker process.
void BootMain(void* arg) {
    BootMainArgs* bootArgs = (BootMainArgs*)arg;
    if (!bootArgs || !bootArgs->mbinfo) {
        HALT("CRITICAL: BootMain started with null args!\n");
    }
    MultibootInfo* mbinfo = bootArgs->mbinfo;

    // ---- Slow stage 1: ATA probe -----------------------------------------
    AdvancedTechnologyAttachment* ata = nullptr;
    AdvancedTechnologyAttachment* SATAList[] = {
        new AdvancedTechnologyAttachment(true, 0x1F0),   // Primary Master
        new AdvancedTechnologyAttachment(false, 0x1F0),  // Primary Slave
        new AdvancedTechnologyAttachment(true, 0x170),   // Secondary Master
        new AdvancedTechnologyAttachment(false, 0x170),  // Secondary Slave
        0};
    for (int i = 0; i < 4; i++) {
        if (!SATAList[i]) {
            HALT("CRITICAL: Failed to allocate ATA object!\n");
        }
    }

    for (int i = 0; SATAList[i] != 0; i++) {
        KDBG3("Checking Drive %d...", i);
        uint32_t ata_size = SATAList[i]->Identify();

        if (ata_size == 0) continue;  // No Drive detected

        // Skip CD-ROMs (ATAPI)
        if (SATAList[i]->isAtapi) {
            KDBG1("Drive %d is ATAPI (CD-ROM), skipping", i);
            continue;
        }

        ata = SATAList[i];

        // Use the FIRST drive found.
        KDBG1("Using ATA drive %d (Master/Slave)", i);
        break;
    }

    if (ata == nullptr) {
        HALT(
            "Error: No ATA drive detected!\nPlease connect an ATA drive and restart the system.\n");
    }
    // ---- Slow stage 2: MBR + partition mount ------------------------------
    MSDOSPartitionTable* MSDOS = new MSDOSPartitionTable(ata);
    if (!MSDOS) {
        HALT("CRITICAL: Failed to allocate MSDOSPartitionTable!\n");
    }
    MSDOS->ReadPartitions();
    // Get Boot Partition
    g_bootPartition = MSDOS->partitions[0];
    if (!g_bootPartition) {
        KDBG1("No valid partition found — initializing disk (this runs once on first boot)...");
        MSDOS->Initialize();
        MSDOS->ReadPartitions();
        g_bootPartition = MSDOS->partitions[0];
        if (!g_bootPartition) {
            HALT("Error: Failed to initialize disk.\n");
        }
    }
    g_bootPartition->ListRoot();
    KDBG1("Boot partition mounted. Root listed.");
    KernelSymbolTable::Load(g_bootPartition, PATH_KERNEL_MAP);

    // Boot image
    Bitmap* bootImg = new Bitmap(PATH_BOOT_BMP);
    if (!bootImg) {
        HALT("CRITICAL: Failed to allocate Bitmap for boot image!\n");
    }
    if (bootImg->IsValid()) {
        int32_t x, y;
        g_GraphicsDriver->GetScreenCenter(bootImg->GetWidth(), bootImg->GetHeight(), x, y);
        g_GraphicsDriver->DrawBitmap(x, (int32_t)((g_GraphicsDriver->GetHeight() * 1) / 3),
                                     bootImg->GetBuffer(), bootImg->GetWidth(),
                                     bootImg->GetHeight());
        g_GraphicsDriver->Flush();
    }

    delete bootImg;

    // Optional frameset (Hashx86/gfx/bootanim/frameNN.bmp)
    LoadBootAnimFrames();

    // Start the animation before the slow font loading below.
    ProcessControlBlock* splashProc = g_scheduler->CreateProcess(true, BootSplashAnimator, nullptr);
    if (!splashProc) {
        HALT("CRITICAL: Failed to create boot splash process!\n");
    }

    // ---- Slow stage 3: font files -----------------------------------------
    g_fManager = new FontManager();
    if (!g_fManager) {
        HALT("CRITICAL: Failed to allocate FontManager!\n");
    }

    auto loadFontStyle = [&](const char* path, FontType style, bool critical) {
        File* f = g_bootPartition->Open(path);
        if (!f || f->size == 0) {
            if (critical) {
                KDBG1("Font error: %s not found. Run 'make hdd'.", path);
                while (1) asm volatile("hlt");
            }
            if (f) { f->Close(); delete f; }
            return;
        }
        g_fManager->LoadFile(f, style, path);
        f->Close();
        delete f;
        KDBG1("Loaded font style %d from %s", (int)style, path);
    };

    // Load only REGULAR fonts at boot. Bold/Italic variants are loaded on demand.
    loadFontStyle(PATH_SEGOEUI_FONT, REGULAR, true);
    loadFontStyle(PATH_CASCADIA_FONT, REGULAR, false);

    // Load icon font (FontAwesome) — icon codepoints start in Unicode PUA range
    {
        File* f = g_bootPartition->Open(PATH_ICON_FONT);
        if (f && f->size > 0) {
            g_fManager->LoadFile(f, REGULAR, PATH_ICON_FONT, 0xF000, 128);
            f->Close();
            delete f;
            KDBG1("Loaded icon font from %s", PATH_ICON_FONT);
        } else {
            if (f) { f->Close(); delete f; }
            KDBG1("Icon font not found: %s", PATH_ICON_FONT);
        }
    }

    // Draw the title and re-sync the animation background atomically.
    BootTitleResync();

    // ---- Slow stage 4: desktop + syscall interfaces ------------------------
    Desktop* desktop = new Desktop(GUI_SCREEN_WIDTH, GUI_SCREEN_HEIGHT);
    if (!desktop) {
        HALT("CRITICAL: Failed to allocate Desktop!\n");
    }

    // Created after the desktop (as before) so HguiHandler picks it up.
    g_sysCalls = new SyscallHandler(0x80, g_interrupts);
    if (!g_sysCalls) {
        HALT("CRITICAL: Failed to allocate SyscallHandler!\n");
    }
    HguiHandler* guiCalls = new HguiHandler(0x81, g_interrupts);  // Needs desktop initialized
    if (!guiCalls) {
        HALT("CRITICAL: Failed to allocate HguiHandler!\n");
    }
    // ---- Slow stage 5: PCI devices ------------------------------------------
    {
        // Keep the timer IRQ out of command/ACK polling (an IRQ handler must
        // never steal an ACK byte mid-init).
        InterruptGuard guard;
        init_pci(g_bootPartition, g_driverManager);
    }
    // init_pci may swap the video mode: repaint + re-capture the background.
    BootSplashRepaint();

    // ---- Slow stage 6: input drivers -----------------------------------------
    MouseDriver* mouse = new MouseDriver(g_interrupts, desktop);
    if (!mouse) {
        HALT("CRITICAL: Failed to allocate MouseDriver!\n");
    }
    g_driverManager->AddDriver(mouse);
    KeyboardDriver* keyboard = new KeyboardDriver(g_interrupts, desktop);
    if (!keyboard) {
        HALT("CRITICAL: Failed to allocate KeyboardDriver!\n");
    }
    g_driverManager->AddDriver(keyboard);

    if (mbinfo->flags & (1 << 3)) {  // mods flag present
        if (mbinfo->mods_count > 0) {
            KDBG1("Found %d Modules", mbinfo->mods_count);
            struct multiboot_module* modules = (struct multiboot_module*)mbinfo->mods_addr;
            // fManager.LoadFile(modules[0].mod_start, modules[0].mod_end); // load font file
            (void)modules;
        } else {
            KDBG1("No modules found");
        }
    } else {
        KDBG1("No multiboot modules info available");
    }

    // ---- Slow stage 7: ELF loader + boot sound --------------------------------
    g_elfLoader = new ELFLoader(g_paging, g_scheduler);
    if (!g_elfLoader) {
        HALT("CRITICAL: Failed to allocate ELFLoader!\n");
    }

    if (g_AudioMixer) {
        Wav* sound = new Wav(PATH_BOOT_WAV);
        if (!sound) {
            HALT("CRITICAL: Failed to allocate Wav object for boot sound!\n");
        }
        sound->Play();
    }

    // ---- Handoff: stop the splash, then start the desktop ----------------------
    g_bootSplashDone = true;
    // Let the animator exit before pDesktop takes over the framebuffer.
    if (Scheduler::activeInstance) Scheduler::activeInstance->Sleep(150);
    {
        uint64_t target = timerTicks + 150;
        while (timerTicks < target) {
            asm volatile("sti; hlt");
        }
    }

    KDBG1("Welcome to #x86!");
    {
        InterruptGuard guard;
        g_driverManager->ActivateAll();
    }
    KDBG1("System Drivers Activated.");

    // Start the desktop thread now that boot is complete.
    DesktopArgs* desktopArgs = new DesktopArgs{g_GraphicsDriver, desktop, g_bootPartition};
    if (!desktopArgs) {
        HALT("CRITICAL: Failed to allocate DesktopArgs!\n");
    }
    ProcessControlBlock* process1 = g_scheduler->CreateProcess(true, pDesktop, desktopArgs);
    if (!process1) {
        HALT("CRITICAL: Failed to create desktop process!\n");
    }

    delete bootArgs;
}


extern "C" void kernelMain(void* multiboot_structure, uint32_t magicnumber) {
    initSerial();
    if (magicnumber != 0x2BADB002) {
        KDBG1("Invalid magic number : [%x]", magicnumber);
        // Halt the CPU — return is undefined in a freestanding kernel
        while (1) {
            asm volatile("hlt");
        }
    }

    MultibootInfo* mbinfo = (MultibootInfo*)multiboot_structure;
    KDBG1("Initializing Hardware");

    gdt_init();

    // Initialize PMM and Kheap
    init_memory(mbinfo);
    InitializePIT(1000);

    KDBG1("Initializing paging...");

    g_paging = new Paging();
    if (!g_paging) {
        HALT("CRITICAL: Failed to allocate Paging object!\n");
    }
    g_paging->Activate();
    kstack_zone_activate(g_paging->KernelPageDirectory);

    // NOTE: slow stages run in the BootMain worker thread; only
    // IRQ-independent, filesystem-free setup stays on this path.

    if (!(mbinfo->flags & (1 << 12))) {
        HALT("CRITICAL: Multiboot framebuffer info not available - cannot initialize graphics!\n");
    }
    g_GraphicsDriver =
        new VESA_BIOS_Extensions(mbinfo->framebuffer_width, mbinfo->framebuffer_height, 32,
                                 (uint32_t*)mbinfo->framebuffer_addr);
    if (!g_GraphicsDriver) {
        HALT("CRITICAL: Failed to allocate VESA_BIOS_Extensions!\n");
    }

    // Black base until BootMain draws the boot image.
    {
        InterruptGuard guard;
        g_GraphicsDriver->Flush();  // present the cleared backbuffer as splash base
    }

    // ---- Early multithreading bring-up ------------------------------------
    // Scheduler + timer IRQ go live before the slow stages (BootMain worker).
    g_scheduler = new Scheduler(g_paging);
    if (!g_scheduler) {
        HALT("CRITICAL: Failed to allocate Scheduler!\n");
    }
    g_interrupts = new InterruptManager(g_scheduler, g_paging);
    if (!g_interrupts) {
        HALT("CRITICAL: Failed to allocate InterruptManager!\n");
    }

    g_driverManager = new DriverManager();
    if (!g_driverManager) {
        HALT("CRITICAL: Failed to allocate DriverManager!\n");
    }

    BootMainArgs* bootArgs = new BootMainArgs{mbinfo};
    if (!bootArgs) {
        HALT("CRITICAL: Failed to allocate BootMainArgs!\n");
    }

    // BootMain runs the slow stages and starts the animator itself.
    ProcessControlBlock* bootProc = g_scheduler->CreateProcess(true, BootMain, bootArgs);
    if (!bootProc) {
        HALT("CRITICAL: Failed to create boot worker process!\n");
    }

    KDBG1("Multithreading enabled. Starting boot worker process.");
    g_interrupts->Activate();
    KDBG1("Interrupts Enabled.");

    while (1) {
        asm volatile("hlt");
    }
}
