/**
 * @file        ac97.cpp
 * @brief       AC97 Audio Driver Implementation
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#include <core/driver.h>
#include <core/drivers/AudioDriver.h>
#include <core/drivers/driver_info.h>
#include <core/interrupts.h>
#include <core/memory.h>
#include <core/pci.h>
#include <core/pmm.h>
#include <debug.h>
#include <utils/string.h>

/* ================= IDs ================= */
#define AC97_VENDOR_ID 0x8086
#define AC97_DEVICE_ID 0x2415

/* ================= Mixer ================= */
#define AC97_REG_RESET 0x00
#define AC97_REG_MASTER_VOL 0x02
#define AC97_REG_PCM_VOL 0x18
#define AC97_REG_EXT_AUDIO 0x28
#define AC97_REG_EXT_CTRL 0x2A
#define AC97_REG_PCM_RATE 0x2C

/* ================= Bus Master ================= */
#define AC97_PO_BDBAR 0x10
#define AC97_PO_CIV 0x14
#define AC97_PO_LVI 0x15
#define AC97_PO_SR 0x16
#define AC97_PO_CR 0x1B

#define AC97_CR_RUN 0x01
#define AC97_CR_RESET 0x02
#define AC97_CR_IOCE 0x10

#define AC97_SR_DCH 0x01
#define AC97_SR_BCIS 0x08
#define AC97_SR_LVBCI 0x20

/* ================= Memory ================= */
/* Audio buffer and BDL sizes (in bytes) */
#define AC97_AUDIO_BUF_SIZE 0x10000  // 64KB audio buffer
#define AC97_BDL_BUF_SIZE 0x1000     // 4KB for BDL (fits 32 entries)
#define AC97_HALF_SIZE (AC97_AUDIO_BUF_SIZE / 2)
#define AC97_BDL_ENTRIES 32

struct AC97_BDL_Entry {
    uint32_t addr;
    uint16_t length;  // words
    uint16_t flags;
} __attribute__((packed));

DEFINE_DRIVER_INFO("Intel AC97 Audio Driver", "2.2.0-MovingLVI", {AC97_VENDOR_ID, AC97_DEVICE_ID});

class DynamicAC97Driver;

/* ================= IRQ ================= */
class AC97IRQ : public InterruptHandler {
    DynamicAC97Driver* driver;

public:
    AC97IRQ(uint8_t irq, DynamicAC97Driver* drv)
        : InterruptHandler(irq, InterruptManager::activeInstance), driver(drv) {}
    uint32_t HandleInterrupt(uint32_t esp) override;
};

/* ================= DRIVER ================= */
/* Number of 4KB pages for the audio data buffer (64KB total) */
#define AC97_BUF_PAGES (AC97_AUDIO_BUF_SIZE / 4096)

class DynamicAC97Driver final : public Driver, public AudioDriver {
    friend class AC97IRQ;

private:
    uint16_t namBar;
    uint16_t nabmBar;
    AC97IRQ* irqHandler;

    // Array of individually-allocated physical pages for the audio buffer.
    // Each page is allocated separately via pmm_alloc_block_low to avoid
    // relying on large contiguous PMM allocations (which can collide with
    // single-page allocations like the Scheduler trampoline on some VMs).
    uint32_t physPages[AC97_BUF_PAGES];  // AC97_AUDIO_BUF_SIZE bytes (64KB = 16 pages)
    uint32_t physBdlAddr;                // Buffer descriptor list (AC97_BDL_BUF_SIZE bytes)

    // --- State ---
    // sw_lvi: The BDL index we are currently preparing to write to (Software Pointer)
    volatile uint8_t sw_lvi;

    // activeHalf: Which ping-pong half (0 or 1) will be written next
    volatile uint8_t activeHalf;

    // buffersOccupied: How many half-buffers are queued but not yet finished by HW.
    // If 0, we can write. If 2, we are full (waiting for HW).
    volatile uint8_t buffersOccupied;

    void Delay(int ms) {
        for (volatile int i = 0; i < ms * 10000; i++);
    }

    bool FindHardware() {
        PeripheralComponentInterconnectController pci;
        auto* dev = pci.FindHardwareDevice(AC97_VENDOR_ID, AC97_DEVICE_ID);
        if (!dev || dev->vendor_id == 0) return false;

        uint32_t cmd = pci.Read(dev->bus, dev->device, dev->function, 0x04);
        pci.Write(dev->bus, dev->device, dev->function, 0x04,
                  (cmd & 0xFFFF0000) | ((cmd & 0xFFFF) | 0x07));

        namBar =
            (uint16_t)((uint32_t)pci.GetBaseAddressRegister(dev->bus, dev->device, dev->function, 0)
                           .address &
                       0xFFFC);
        nabmBar =
            (uint16_t)((uint32_t)pci.GetBaseAddressRegister(dev->bus, dev->device, dev->function, 1)
                           .address &
                       0xFFFC);

        irqHandler = new AC97IRQ(dev->interrupt + 0x20, this);
        if (!irqHandler) {
            HALT("CRITICAL: [AC97] Failed to allocate AC97 IRQ handler!\n");
        }
        printf("[AC97] Found device IRQ=%d\n", dev->interrupt);
        return true;
    }

    void OnInterrupt() {
        uint16_t sr = inw(nabmBar + AC97_PO_SR);

        if ((sr & AC97_SR_BCIS) || (sr & AC97_SR_LVBCI)) {
            // ACK interrupt
            outw(nabmBar + AC97_PO_SR, sr & (AC97_SR_BCIS | AC97_SR_LVBCI));

            // A buffer finished!
            // This means we have space in our logical queue.
            if (buffersOccupied > 0) {
                buffersOccupied--;
            }
        }
    }

public:
    DynamicAC97Driver() {
        driverName = "Intel AC97";
        namBar = nabmBar = 0;
        irqHandler = nullptr;
        for (int i = 0; i < AC97_BUF_PAGES; i++) physPages[i] = 0;
        physBdlAddr = 0;
        sw_lvi = 0;
        activeHalf = 0;
        buffersOccupied = 0;
    }

    ~DynamicAC97Driver() {
        if (irqHandler) delete irqHandler;
        for (int i = 0; i < AC97_BUF_PAGES; i++) {
            if (physPages[i]) pmm_free_block((void*)physPages[i]);
        }
        if (physBdlAddr) pmm_free_block((void*)physBdlAddr);
    }

    void Activate() override {
        if (!FindHardware()) return;

        // Allocate DMA audio buffer as individual 4KB pages (<256MB for identity mapping).
        // Using individual pages avoids PMM contiguous-allocation issues on VirtualBox
        // where large pmm_alloc_blocks requests can overlap earlier single-page allocations.
        for (int i = 0; i < AC97_BUF_PAGES; i++) {
            physPages[i] = (uint32_t)pmm_alloc_block_low(256 * 1024 * 1024);
            if (!physPages[i]) {
                printf("[AC97] Error: Failed to allocate DMA audio page %d/%d\n", i + 1,
                       AC97_BUF_PAGES);
                for (int j = 0; j < i; j++) {
                    pmm_free_block((void*)physPages[j]);
                    physPages[j] = 0;
                }
                return;
            }
        }
        // Allocate 4KB for BDL (fits 32 entries) - needs just 1 page
        physBdlAddr = (uint32_t)pmm_alloc_block_low(256 * 1024 * 1024);
        if (!physBdlAddr) {
            printf("[AC97] Error: Failed to allocate DMA BDL\n");
            for (int i = 0; i < AC97_BUF_PAGES; i++) {
                pmm_free_block((void*)physPages[i]);
                physPages[i] = 0;
            }
            return;
        }

        printf("[AC97] DMA buffer (%d pages), BDL @ 0x%x\n", AC97_BUF_PAGES, physBdlAddr);

        // 1. Reset
        outw(namBar + AC97_REG_RESET, 0);
        Delay(50);
        outw(namBar + AC97_REG_MASTER_VOL, 0);
        outw(namBar + AC97_REG_PCM_VOL, 0);

        // 2. Enable VRA
        if (inw(namBar + AC97_REG_EXT_AUDIO) & 1) {
            outw(namBar + AC97_REG_EXT_CTRL, 1);
            Delay(10);
            outw(namBar + AC97_REG_PCM_RATE, 44100);
        }

        // 3. Reset Bus Master
        outb(nabmBar + AC97_PO_CR, AC97_CR_RESET);
        Delay(10);
        outb(nabmBar + AC97_PO_CR, 0);

        // 4. Setup BDL Pointer (physical address)
        outl(nabmBar + AC97_PO_BDBAR, physBdlAddr);

        // Clear DMA buffers (each page individually)
        for (int i = 0; i < AC97_BUF_PAGES; i++) {
            memset((void*)physPages[i], 0, 4096);
        }
        memset((void*)physBdlAddr, 0, sizeof(AC97_BDL_Entry) * AC97_BDL_ENTRIES);

        // 5. Initialize State
        sw_lvi = 0;
        activeHalf = 0;
        buffersOccupied = 0;

        // Reset HW LVI to 0 to start
        outb(nabmBar + AC97_PO_LVI, 0);

        is_Active = true;
        printf("[AC97] Ready (Moving LVI Mode)\n");
    }

    void Deactivate() override {
        Stop();
        // Free DMA buffers
        for (int i = 0; i < AC97_BUF_PAGES; i++) {
            if (physPages[i]) {
                pmm_free_block((void*)physPages[i]);
                physPages[i] = 0;
            }
        }
        if (physBdlAddr) {
            pmm_free_block((void*)physBdlAddr);
            physBdlAddr = 0;
        }
    }

    uint32_t GetBufferSize() override {
        return AC97_HALF_SIZE;
    }

    void SetFormat(uint32_t rate, uint8_t, uint8_t) override {
        outw(namBar + AC97_REG_PCM_RATE, (uint16_t)rate);
        sampleRate = rate;
    }

    void ApplyHardwareVolume() override {
        uint8_t att = 63 - ((masterVolume * 63) / 100);
        uint16_t vol = (att << 8) | att;
        outw(namBar + AC97_REG_MASTER_VOL, vol);
        outw(namBar + AC97_REG_PCM_VOL, vol);
    }

    void Start() override {
        // Run and Enable Interrupts
        outb(nabmBar + AC97_PO_CR, AC97_CR_RUN | AC97_CR_IOCE);
        isPlaying = true;
    }

    void Stop() override {
        outb(nabmBar + AC97_PO_CR, 0);
        isPlaying = false;
    }

    bool IsReadyForData() override {
        // We can buffer up to 2 frames ahead.
        return buffersOccupied < 2;
    }

    uint32_t WriteData(uint8_t* buffer, uint32_t size) override {
        if (size == 0) return 0;
        if (physBdlAddr == 0 || physPages[0] == 0) return 0;  // DMA not active
        if (size > AC97_HALF_SIZE) size = AC97_HALF_SIZE;
        InterruptGuard guard;
        if (buffersOccupied >= 2) {
            return 0;
        }

        // Calculate how many 4KB pages this write spans (1-8 pages)
        uint32_t pagesUsed = (size + 4095) / 4096;
        if (pagesUsed > AC97_BUF_PAGES / 2) pagesUsed = AC97_BUF_PAGES / 2;

        // Determine which half of the buffer to use (Ping-Pong: 0 or 1)
        uint32_t pageBase = activeHalf * (AC97_BUF_PAGES / 2);

        // Copy data to individual pages and write BDL entries
        AC97_BDL_Entry* bdl = (AC97_BDL_Entry*)physBdlAddr;
        uint32_t remaining = size;
        uint32_t srcOff = 0;

        for (uint32_t p = 0; p < pagesUsed && remaining > 0; p++) {
            uint32_t dstPhys = physPages[pageBase + p];
            uint32_t chunk = (remaining < 4096) ? remaining : 4096;
            memcpy((void*)dstPhys, buffer + srcOff, chunk);
            srcOff += chunk;
            remaining -= chunk;

            uint8_t bdlIdx = (sw_lvi + p) % AC97_BDL_ENTRIES;
            bdl[bdlIdx].addr = dstPhys;
            bdl[bdlIdx].length = (uint16_t)(chunk / 2);
            // IOC on LAST entry only
            bdl[bdlIdx].flags = (remaining == 0) ? 0x8000 : 0;
        }
        asm volatile("wbinvd" ::: "memory");

        // Only update BDL/LVI state when pages were actually written
        if (pagesUsed > 0) {
            uint8_t lastIdx = (sw_lvi + pagesUsed - 1) % AC97_BDL_ENTRIES;
            outb(nabmBar + AC97_PO_LVI, lastIdx);
            sw_lvi = (sw_lvi + pagesUsed) % AC97_BDL_ENTRIES;
            activeHalf = 1 - activeHalf;
            buffersOccupied++;
        }
        return size;
    }

    AudioDriver* AsAudioDriver() override {
        return this;
    }
};

uint32_t AC97IRQ::HandleInterrupt(uint32_t esp) {
    if (driver) driver->OnInterrupt();
    return esp;
}

extern "C" Driver* CreateDriverInstance() {
    DynamicAC97Driver* drv = new DynamicAC97Driver();
    if (!drv) {
        HALT("CRITICAL: [AC97] Failed to allocate DynamicAC97Driver!\n");
    }
    return drv;
}
