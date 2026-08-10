// Order matters: console.h includes debug.h (HALT, printf).
// We undef HALT and provide our own VGA-text version.
#include <console.h>
#include <core/drivers/ata.h>
#include <core/filesystem/FatFs/ff.h>
#include <core/filesystem/FatFs/diskio.h>
#include <core/filesystem/FatFsWrapper.h>
#include <core/multiboot.h>
#include <core/pak.h>
#include <string.h>
#include <types.h>

#undef HALT
#define HALT(msg)                             \
    do {                                      \
        printf(RED, "\nFATAL: " msg "\n");    \
        printf(RED, "System halted.\n");      \
        asm volatile("cli; hlt");             \
        for (;;) asm volatile("hlt");         \
    } while (0)

static FRESULT formatPartition(BYTE pdrv, AdvancedTechnologyAttachment* ata,
                               uint32_t start, uint32_t size) {
    fatfs_init(pdrv, ata, start, size);
    char path[4] = "0:";
    path[0] = '0' + pdrv;
    MKFS_PARM opt;
    memset(&opt, 0, sizeof(opt));
    opt.fmt = FM_FAT32 | FM_SFD;
    opt.au_size = 0;
    uint8_t work[4096];
    return f_mkfs(path, &opt, work, sizeof(work));
}

static void installGRUB(AdvancedTechnologyAttachment* ata,
                        const void* bootImg, uint32_t bootImgSize,
                        const void* coreImg, uint32_t coreImgSize,
                        uint32_t p1_start, uint32_t p1_size,
                        uint32_t p2_start, uint32_t p2_size) {
    (void)p2_start;
    (void)p2_size;

    // core.img is written into the MBR gap (sectors 1..p1_start-1) between
    // the boot sector and partition 1.  Validate it fits before writing any
    // sector, otherwise the tail would overwrite the start of partition 1.
    uint32_t coreSectors = (coreImgSize + 511) / 512;
    if (coreSectors > p1_start - 1) {
        printf(RED,
               "core.img needs %u sectors but MBR gap (LBA 1..%u) only holds %u\n",
               coreSectors, p1_start - 1, p1_start - 1);
        HALT("core.img too large for MBR gap");
    }

    uint8_t mbr[512];
    memcpy(mbr, bootImg, 512);

    memset(&mbr[446], 0, 64);
    mbr[446 + 0] = 0x80;
    mbr[446 + 4] = 0x0C;
    *(uint32_t*)&mbr[446 + 8] = p1_start;
    *(uint32_t*)&mbr[446 + 12] = p1_size;

    mbr[462 + 0] = 0x00;
    mbr[462 + 4] = 0x0C;
    *(uint32_t*)&mbr[462 + 8] = p2_start;
    *(uint32_t*)&mbr[462 + 12] = p2_size;

    ata->Write28(0, mbr, 512);
    printf(LIGHT_GRAY, "MBR written\n");

    const uint8_t* src = (const uint8_t*)coreImg;
    for (uint32_t i = 0; i < coreSectors; i++) {
        uint8_t sector[512];
        memset(sector, 0, 512);
        uint32_t remaining = coreImgSize - i * 512;
        uint32_t chunk = remaining > 512 ? 512 : remaining;
        memcpy(sector, src + i * 512, chunk);
        ata->Write28(1 + i, sector, 512);
    }
    ata->Flush();
    printf(LIGHT_GRAY, "core.img written (%u sectors)\n", coreSectors);
}

// Recursively delete a file or directory tree.
static void wipePath(const char* path) {
    DIR d;
    if (f_opendir(&d, path) != FR_OK) {
        f_unlink(path);
        return;
    }
    FILINFO info;
    while (f_readdir(&d, &info) == FR_OK && info.fname[0]) {
        // Skip only "." and ".." — dot-prefixed names like ".config" are real
        // files and must still be deleted.
        if (strcmp(info.fname, ".") == 0 || strcmp(info.fname, "..") == 0) continue;
        char full[256];
        uint32_t baseLen = 0;
        while (path[baseLen]) baseLen++;
        uint32_t nameLen = 0;
        while (info.fname[nameLen]) nameLen++;
        // Complete path needs base + '/' + name + null terminator.
        if (baseLen + 1 + nameLen + 1 > sizeof(full)) continue;
        uint32_t i = 0;
        while (path[i]) { full[i] = path[i]; i++; }
        full[i] = '/'; i++;
        uint32_t j = 0;
        while (info.fname[j]) { full[i] = info.fname[j]; i++; j++; }
        full[i] = 0;
        wipePath(full);
    }
    f_closedir(&d);
    f_unlink(path);
}

static void pitSleepMs(uint32_t ms) {
    while (ms > 0) {
        uint32_t chunk = ms > 50 ? 50 : ms;
        uint32_t count = chunk * 1193;
        outb(0x43, 0x30);
        outb(0x40, count & 0xFF);
        outb(0x40, (count >> 8) & 0xFF);
        for (;;) {
            outb(0x43, 0x00);
            uint8_t lo = inb(0x40);
            uint8_t hi = inb(0x40);
            if (((uint16_t)lo | ((uint16_t)hi << 8)) == 0) break;
        }
        ms -= chunk;
    }
}

// Send an ATAPI START STOP UNIT (eject) packet to a drive slot.
static bool atapiEject(bool master, uint16_t base) {
    uint16_t cmdPort = base + 7;
    uint16_t devPort = base + 6;

    outb(devPort, master ? 0xA0 : 0xB0);
    inb(cmdPort); inb(cmdPort); inb(cmdPort); inb(cmdPort);

    // No device present: the status register reads 0xFF (bus floats high) or
    // 0x00. Bail immediately instead of spinning through the busy-wait
    // timeouts below (which each allow 1M port reads and are very slow under
    // emulation).
    uint32_t w = 0;
    uint8_t status = inb(cmdPort);
    if (status == 0xFF || status == 0x00) return false;
    while ((status & 0x80) && w++ < 1000000) status = inb(cmdPort);

    outb(base + 1, 0);
    outb(base + 2, 0);
    outb(base + 3, 0);
    outb(base + 4, 0);
    outb(base + 5, 0);
    outb(cmdPort, 0xA0);

    w = 0;
    status = inb(cmdPort);
    while ((status & 0x80) || !(status & 0x08)) {
        // A non-packet device (e.g. a fixed ATA disk) aborts the PACKET
        // command with ERR set; stop waiting the moment that happens instead
        // of exhausting the full timeout.
        if (status & 0x01) return false;
        if (w++ > 1000000) return false;
        status = inb(cmdPort);
    }
    if (status & 0x01) return false;

    uint16_t cdb[6];
    cdb[0] = 0x001B;
    cdb[1] = 0x0000;
    cdb[2] = 0x0002;
    cdb[3] = 0x0000;
    cdb[4] = 0x0000;
    cdb[5] = 0x0000;
    outsw(base, cdb, 6);

    w = 0;
    while ((inb(cmdPort) & 0x80) && w++ < 1000000) {}
    return !(inb(cmdPort) & 0x01);
}

static void ejectCd() {
    printf(LIGHT_GRAY, "Ejecting CD-ROM...\n");
    if (atapiEject(true, 0x1F0) || atapiEject(false, 0x1F0) ||
        atapiEject(true, 0x170) || atapiEject(false, 0x170))
        printf(GREEN, "CD ejected\n");
    else
        printf(YELLOW, "No CD-ROM drive found, continuing\n");
}

// Read a single PS/2 scancode, draining any queued bytes.
static uint8_t readScancode(void) {
    while (!(inb(0x64) & 0x01)) {}
    uint8_t sc = inb(0x60);
    while (inb(0x64) & 0x01) inb(0x60);
    return sc;
}

// Require explicit confirmation before destructive disk writes.
static bool confirmDestructive(const char* what) {
    printf(LIGHT_GRAY, "%s", what);
    printf(LIGHT_GRAY, " Type 'y' to confirm, any other key to abort: ");
    uint8_t sc = readScancode();
    if (sc == 0x15) return true;  // 'y' make code
    return false;
}

static void ensurePath(const char* path) {
    char buf[128];
    buf[0] = '0';
    buf[1] = ':';
    uint32_t j = 2;
    for (uint32_t i = 0; path[i]; i++) {
        if (j >= sizeof(buf) - 1) {
            printf(YELLOW, "WARN: path too long for FatFs buffer (truncated): %s\n", path);
            return;
        }
        if (path[i] == '/') {
            buf[j] = 0;
            f_mkdir(buf);
        }
        buf[j++] = path[i];
    }
    buf[j] = 0;
}

extern "C" void kernelMain(void* multiboot_structure, uint32_t magicnumber) {
    if (magicnumber != 0x2BADB002) {
        for (;;) asm volatile("hlt");
    }

    MultibootInfo* mbinfo = (MultibootInfo*)multiboot_structure;

    clearScreen();
    printf(WHITE, "Hashx86-OS Installer v1.0\n");
    printf(LIGHT_GRAY, "Booting installer kernel...\n");

    void* pakData = nullptr;
    uint32_t pakSize = 0;
    if (mbinfo->flags & (1 << 3) && mbinfo->mods_count > 0) {
        struct multiboot_module* modules = (struct multiboot_module*)mbinfo->mods_addr;
        for (uint32_t i = 0; i < mbinfo->mods_count; i++) {
            const char* cmdline = (const char*)modules[i].cmdline;
            if (!cmdline) {
                if (mbinfo->mods_count == 1) {
                    pakData = (void*)modules[i].mod_start;
                    pakSize = modules[i].mod_end - modules[i].mod_start;
                }
                continue;
            }
            const char* name = cmdline;
            for (const char* p = name; *p; p++)
                if (*p == '/') name = p + 1;
            if (strcmp(name, "installer.pak") == 0) {
                pakData = (void*)modules[i].mod_start;
                pakSize = modules[i].mod_end - modules[i].mod_start;
                break;
            }
        }
    }

    if (!pakData)
        HALT("installer.pak not found. Boot with: module /installer.pak");

    printf(LIGHT_GREEN, "Found installer.pak (%u bytes)\n", pakSize);

    if (pakSize < sizeof(PakHeader))
        HALT("PAK file too small");
    PakHeader* phdr = (PakHeader*)pakData;
    if (phdr->magic[0] != 'P' || phdr->magic[1] != 'A' ||
        phdr->magic[2] != 'C' || phdr->magic[3] != 'K')
        HALT("Invalid PAK magic");

    // Validate the directory region immediately: it must lie entirely within
    // the module payload and hold a whole number of entries.
    if (phdr->dirOffset > pakSize || phdr->dirSize > pakSize - phdr->dirOffset ||
        phdr->dirSize % sizeof(PakDirEntry) != 0)
        HALT("PAK directory out of bounds");

    const PakDirEntry* dir = (const PakDirEntry*)((uint8_t*)pakData + phdr->dirOffset);
    uint32_t numEntries = phdr->dirSize / sizeof(PakDirEntry);
    printf(LIGHT_GRAY, "PAK: %u entries\n", numEntries);

    // Validate every entry name before it reaches strcmp/strlen/%s: the name
    // field must hold a NUL terminator within the fixed-size field, otherwise
    // string operations would read past the directory entry.
    for (uint32_t i = 0; i < numEntries; i++) {
        bool terminated = false;
        for (uint32_t j = 0; j < sizeof(dir[i].name); j++) {
            if (dir[i].name[j] == '\0') {
                terminated = true;
                break;
            }
        }
        if (!terminated)
            HALT("PAK entry name is not NUL-terminated");
    }

    AdvancedTechnologyAttachment ata0(true, 0x1F0);
    AdvancedTechnologyAttachment ata1(false, 0x1F0);
    AdvancedTechnologyAttachment ata2(true, 0x170);
    AdvancedTechnologyAttachment ata3(false, 0x170);

    AdvancedTechnologyAttachment* ata = nullptr;
    uint32_t totalSectors = 0;

    for (int i = 0; i < 4; i++) {
        AdvancedTechnologyAttachment* dev =
            i == 0 ? &ata0 : i == 1 ? &ata1 : i == 2 ? &ata2 : &ata3;
        totalSectors = dev->Identify();
        if (totalSectors > 0) {
            // Skip CD-ROMs (ATAPI) and removable media 
            // valid installation targets.
            if (dev->isAtapi) {
                printf(LIGHT_GRAY, "Drive %d is ATAPI (CD-ROM), skipping\n", i);
                continue;
            }
            if (dev->isRemovable) {
                printf(LIGHT_GRAY, "Drive %d is removable media, skipping\n", i);
                continue;
            }
            ata = dev;
            printf(WHITE, "Using drive %d (%u sectors), media type: fixed disk\n", i,
                   totalSectors);
            break;
        }
    }

    if (!ata) HALT("No ATA drive detected");
    if (totalSectors <= 63) HALT("Disk too small");

    // Explicit confirmation before any partition creation or disk write.
    if (!confirmDestructive("\nWARNING: ALL data on the selected drive will be erased.")) {
        printf(RED, "\nInstallation aborted — no changes were made.\n");
        for (;;) asm volatile("hlt");
    }

    uint32_t available = totalSectors - 63;
    uint32_t p1_size = available / 2;
    uint32_t p2_size = available - p1_size;
    uint32_t p1_start = 63;
    uint32_t p2_start = 63 + p1_size;

    printf(LIGHT_GRAY, "Part 1: LBA %u +%u  Part 2: LBA %u +%u\n",
           p1_start, p1_size, p2_start, p2_size);

    const void* bootImg = nullptr;
    uint32_t bootImgSize = 0;
    const void* coreImg = nullptr;
    uint32_t coreImgSize = 0;
    for (uint32_t i = 0; i < numEntries; i++) {
        if (dir[i].offset > pakSize || dir[i].size > pakSize - dir[i].offset) {
            printf(YELLOW, "WARN: entry %u out of bounds, skipping\n", i);
            continue;
        }
        const void* data = (uint8_t*)pakData + dir[i].offset;
        if (strcmp(dir[i].name, "boot/boot.img") == 0) {
            bootImg = data; bootImgSize = dir[i].size;
        } else if (strcmp(dir[i].name, "boot/core.img") == 0) {
            coreImg = data; coreImgSize = dir[i].size;
        }
    }

    if (!bootImg || bootImgSize < 512) HALT("boot.img missing or too small");
    if (!coreImg || coreImgSize == 0) HALT("core.img missing");

    printf(LIGHT_BLUE, "Installing GRUB...\n");
    installGRUB(ata, bootImg, bootImgSize, coreImg, coreImgSize,
                p1_start, p1_size, p2_start, p2_size);

    // Partition 1 — always format so the installed filesystem is known-good
    {
        printf(WHITE, "Formatting partition 1...\n");
        FRESULT res = formatPartition(0, ata, p1_start, p1_size);
        if (res != FR_OK)
            HALT("Format partition 1 failed");
        printf(LIGHT_GRAY, "Partition 1 formatted\n");
    }

    // Partition 2 — format unconditionally as well
    {
        printf(WHITE, "Formatting partition 2...\n");
        FRESULT res = formatPartition(1, ata, p2_start, p2_size);
        if (res != FR_OK)
            HALT("Format partition 2 failed");
        printf(LIGHT_GRAY, "Partition 2 formatted\n");
    }

    // Mount partition 1 and extract files
    FATFS fatfs;
    fatfs_init(0, ata, p1_start, p1_size);
    {
        FRESULT res = f_mount(&fatfs, "0:", 1);
        if (res != FR_OK) {
            printf(RED, "Mount failed: %d\n", res);
            HALT("Mount partition 1 failed");
        }
    }

    printf(LIGHT_GRAY, "Cleaning partition 1...\n");
    wipePath("0:");

    printf(WHITE, "Extracting files...\n");
    uint32_t extracted = 0;
    uint32_t skipped = 0;
    uint32_t failed = 0;
    for (uint32_t i = 0; i < numEntries; i++) {
        const char* name = dir[i].name;
        if (dir[i].offset > pakSize || dir[i].size > pakSize - dir[i].offset) {
            printf(YELLOW, "WARN: entry %u out of bounds, skipping\n", i);
            failed++;
            continue;
        }
        const uint8_t* data = (const uint8_t*)pakData + dir[i].offset;
        uint32_t size = dir[i].size;

        if (strcmp(name, "boot/boot.img") == 0 ||
            strcmp(name, "boot/core.img") == 0) {
            skipped++;
            continue;
        }

        char fatPath[128];

        // The full path must fit: "0:" prefix + name + null terminator.
        if (strlen(name) + 2 > sizeof(fatPath)) {
            printf(YELLOW, "WARN: entry name too long, skipping: %s\n", name);
            failed++;
            continue;
        }

        ensurePath(name);

        fatPath[0] = '0';
        fatPath[1] = ':';
        uint32_t j = 2;
        // Loop bound retained for safety; the shared name-length check above
        // guarantees the complete name already fits, so no truncation occurs.
        for (uint32_t k = 0; name[k] && j < sizeof(fatPath) - 1; k++)
            fatPath[j++] = name[k];
        fatPath[j] = 0;

        bool ok = false;
        if (size > 0) {
            FIL fil;
            FRESULT res = f_open(&fil, fatPath, FA_CREATE_ALWAYS | FA_WRITE);
            if (res == FR_OK) {
                UINT bw = 0;
                res = f_write(&fil, data, size, &bw);
                FRESULT closeRes = f_close(&fil);
                if (res == FR_OK && closeRes == FR_OK && bw == size) {
                    ok = true;
                } else {
                    printf(YELLOW, "  failed: %s (write=%d close=%d bw=%u/%u)\n", name,
                           res, closeRes, (unsigned int)bw, (unsigned int)size);
                }
            } else {
                printf(YELLOW, "  failed: %s (%d)\n", name, res);
            }
        } else {
            FIL fil;
            FRESULT res = f_open(&fil, fatPath, FA_CREATE_NEW | FA_WRITE);
            if (res == FR_OK) {
                FRESULT closeRes = f_close(&fil);
                if (closeRes == FR_OK) {
                    ok = true;
                } else {
                    printf(YELLOW, "  failed: %s (close=%d)\n", name, closeRes);
                }
            } else {
                printf(YELLOW, "  failed: %s (%d)\n", name, res);
            }
        }
        if (ok)
            extracted++;
        else
            failed++;
    }

    printf(GREEN, "%u files extracted (%u skipped, %u failed)\n", extracted, skipped, failed);
    f_mount(nullptr, "0:", 0);

    // Any failed extraction leaves the installed volume incomplete; do not
    // reboot into a broken installation.
    if (failed > 0) {
        printf(RED, "\nInstallation FAILED: %u file(s) could not be written.\n", failed);
        printf(RED, "Remove installation media and retry.\n");
        for (;;) asm volatile("hlt");
    }

    printf(WHITE, "\n");
    printf(WHITE, "==============================================================\n");
    printf(LIGHT_GREEN,  "  Installation complete. Remove installation media and reset.\n");
    printf(WHITE, "==============================================================\n");

    ejectCd();

    printf(YELLOW, "Rebooting in 5 seconds...\n");
    for (int i = 5; i > 0; i--) {
        printf(LIGHT_GRAY, "%d...", i);
        pitSleepMs(1000);
    }
    printf(WHITE, "\nRebooting now!\n");
    pitSleepMs(500);

    outb(0x64, 0xFE);
    for (;;) asm volatile("hlt");
}
