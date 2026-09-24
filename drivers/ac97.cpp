/*
 * MIT License
 *
 * Copyright (c) 2025 Malaka Gunawardana
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <core/driver.h>
#include <core/drivers/AudioDriver.h>
#include <core/drivers/driver_info.h>
#include <core/interrupts.h>
#include <core/memory.h>
#include <core/pci.h>
#include <core/pmm.h>
#include <debug.h>
#include <string.h>

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
#define AC97_AUDIO_BUF_SIZE 0x10000  // 64KB audio buffer.
#define AC97_BDL_BUF_SIZE 0x1000     // 4KB for the BDL (fits 32 entries).
#define AC97_HALF_SIZE (AC97_AUDIO_BUF_SIZE / 2)
#define AC97_BDL_ENTRIES 32

/**
 * struct AC97_BDL_Entry - One buffer descriptor in the AC97 buffer list.
 * @addr: Physical address of the audio buffer page.
 * @length: Buffer length in 16-bit words.
 * @flags: Descriptor flags; bit 15 enables interrupt-on-complete.
 */
struct AC97_BDL_Entry {
    uint32_t addr;
    uint16_t length;  // Words.
    uint16_t flags;
} __attribute__((packed));

DEFINE_DRIVER_INFO("Intel AC97 Audio Driver", "2.2.0-MovingLVI", {AC97_VENDOR_ID, AC97_DEVICE_ID});

class DynamicAC97Driver;

/* ================= IRQ ================= */
/**
 * class AC97IRQ - IRQ handler that forwards playback interrupts to the driver.
 * @driver: The owning DynamicAC97Driver instance.
 */
class AC97IRQ : public InterruptHandler {
    DynamicAC97Driver* driver;

public:
    AC97IRQ(uint8_t irq, DynamicAC97Driver* drv)
        : InterruptHandler(irq, InterruptManager::activeInstance), driver(drv) {}
    uint32_t HandleInterrupt(uint32_t esp) override;
};

/* ================= DRIVER ================= */
/* Number of 4KB pages for the audio data buffer (64KB total). */
#define AC97_BUF_PAGES (AC97_AUDIO_BUF_SIZE / 4096)

/**
 * class DynamicAC97Driver - Intel AC97 audio driver using a moving-LVI DMA ring.
 * @namBar: Base address of the NAM mixer registers.
 * @nabmBar: Base address of the NABM bus-master registers.
 * @irqHandler: IRQ handler receiving playback interrupts.
 * @physPages: Individually mapped physical pages for the audio buffer.
 * @physBdlAddr: Physical address of the buffer descriptor list.
 * @sw_lvi: BDL index currently being prepared for writing (software pointer).
 * @activeHalf: Ping-pong half (0 or 1) that will be written next.
 * @buffersOccupied: Number of queued half-buffers not yet finished by hardware.
 */
class DynamicAC97Driver final : public Driver, public AudioDriver {
    friend class AC97IRQ;

private:
    uint16_t namBar;
    uint16_t nabmBar;
    AC97IRQ* irqHandler;

    // Array of individually-allocated physical pages for the audio buffer.
    // Each page is allocated separately via pmm_alloc_block_low to avoid
    // relying on large contiguous PMM allocations, which can collide with
    // single-page allocations like the Scheduler trampoline on some VMs.
    uint32_t physPages[AC97_BUF_PAGES];  // AC97_AUDIO_BUF_SIZE bytes (64KB = 16 pages).
    uint32_t physBdlAddr;                // Buffer descriptor list (AC97_BDL_BUF_SIZE bytes).

    // --- State ---
    // sw_lvi: The BDL index we are preparing to write to (software pointer).
    volatile uint8_t sw_lvi;

    // activeHalf: Which ping-pong half (0 or 1) will be written next.
    volatile uint8_t activeHalf;

    // buffersOccupied: How many half-buffers are queued but not yet finished
    // by hardware. If 0 we can write; if 2 we are full (waiting for HW).
    volatile uint8_t buffersOccupied;

    /**
     * Delay() - Busy-wait approximately the given number of milliseconds.
     * @ms: Delay duration in milliseconds.
     */
    void Delay(int ms) {
        for (volatile int i = 0; i < ms * 10000; i++);
    }

    /**
     * FindHardware() - Locate the AC97 controller and allocate its IRQ handler.
     *
     * Return: True when the controller was found and registered.
     */
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

    /**
     * OnInterrupt() - Acknowledge playback interrupts and free queue slots.
     */
    void OnInterrupt() {
        uint16_t sr = inw(nabmBar + AC97_PO_SR);

        if ((sr & AC97_SR_BCIS) || (sr & AC97_SR_LVBCI)) {
            // Acknowledge the interrupt.
            outw(nabmBar + AC97_PO_SR, sr & (AC97_SR_BCIS | AC97_SR_LVBCI));

            // A buffer finished, freeing space in the logical queue.
            if (buffersOccupied > 0) {
                buffersOccupied--;
            }
        }
    }

public:
    /**
     * DynamicAC97Driver() - Construct a driver with no hardware mapped yet.
     */
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

    /**
     * ~DynamicAC97Driver() - Release the IRQ handler and DMA allocations.
     */
    ~DynamicAC97Driver() {
        if (irqHandler) delete irqHandler;
        for (int i = 0; i < AC97_BUF_PAGES; i++) {
            if (physPages[i]) pmm_free_block((void*)physPages[i]);
        }
        if (physBdlAddr) pmm_free_block((void*)physBdlAddr);
    }

    /**
     * Activate() - Find the hardware, allocate DMA buffers and start the AC97.
     */
    void Activate() override {
        if (!FindHardware()) return;

        // Allocate DMA audio pages individually (below 256MB for identity
        // mapping). Using individual pages avoids PMM contiguous-allocation
        // issues on VirtualBox where large pmm_alloc_blocks requests can
        // overlap earlier single-page allocations.
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
        // Allocate 4KB for the BDL (fits 32 entries); one page suffices.
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

        // 1. Reset.
        outw(namBar + AC97_REG_RESET, 0);
        Delay(50);
        outw(namBar + AC97_REG_MASTER_VOL, 0);
        outw(namBar + AC97_REG_PCM_VOL, 0);

        // 2. Enable VRA.
        if (inw(namBar + AC97_REG_EXT_AUDIO) & 1) {
            outw(namBar + AC97_REG_EXT_CTRL, 1);
            Delay(10);
            outw(namBar + AC97_REG_PCM_RATE, 44100);
        }

        // 3. Reset the bus master.
        outb(nabmBar + AC97_PO_CR, AC97_CR_RESET);
        Delay(10);
        outb(nabmBar + AC97_PO_CR, 0);

        // 4. Set up the BDL pointer (physical address).
        outl(nabmBar + AC97_PO_BDBAR, physBdlAddr);

        // Clear the DMA buffers (each page individually).
        for (int i = 0; i < AC97_BUF_PAGES; i++) {
            memset((void*)physPages[i], 0, 4096);
        }
        memset((void*)physBdlAddr, 0, sizeof(AC97_BDL_Entry) * AC97_BDL_ENTRIES);

        // 5. Initialize state.
        sw_lvi = 0;
        activeHalf = 0;
        buffersOccupied = 0;

        // Reset the hardware LVI to 0 to start.
        outb(nabmBar + AC97_PO_LVI, 0);

        is_Active = true;
        printf("[AC97] Ready (Moving LVI Mode)\n");
    }

    /**
     * Deactivate() - Stop playback and free the DMA allocations.
     */
    void Deactivate() override {
        Stop();
        // Free the DMA buffers.
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

    /**
     * GetBufferSize() - Report the size of one ping-pong half-buffer.
     *
     * Return: Size in bytes.
     */
    uint32_t GetBufferSize() override {
        return AC97_HALF_SIZE;
    }

    /**
     * SetFormat() - Set the playback sample rate.
     * @rate: Sample rate in Hertz.
     */
    void SetFormat(uint32_t rate, uint8_t, uint8_t) override {
        outw(namBar + AC97_REG_PCM_RATE, (uint16_t)rate);
        sampleRate = rate;
    }

    /**
     * ApplyHardwareVolume() - Write the master and PCM volume registers.
     */
    void ApplyHardwareVolume() override {
        uint8_t att = 63 - ((masterVolume * 63) / 100);
        uint16_t vol = (att << 8) | att;
        outw(namBar + AC97_REG_MASTER_VOL, vol);
        outw(namBar + AC97_REG_PCM_VOL, vol);
    }

    /**
     * Start() - Run the DMA engine and enable playback interrupts.
     */
    void Start() override {
        // Run and enable interrupts.
        outb(nabmBar + AC97_PO_CR, AC97_CR_RUN | AC97_CR_IOCE);
        isPlaying = true;
    }

    /**
     * Stop() - Stop the DMA engine.
     */
    void Stop() override {
        outb(nabmBar + AC97_PO_CR, 0);
        isPlaying = false;
    }

    /**
     * IsReadyForData() - Check whether another half-buffer can be queued.
     *
     * Return: True when fewer than two frames are pending.
     */
    bool IsReadyForData() override {
        // We can buffer up to two frames ahead.
        return buffersOccupied < 2;
    }

    /**
     * WriteData() - Queue a half-buffer of PCM data for playback.
     * @buffer: Pointer to the PCM samples to play.
     * @size: Number of bytes to queue.
     *
     * Copies the data into the inactive ping-pong half, updates the buffer
     * descriptor list and advances the software LVI pointer.
     *
     * Return: The number of bytes queued, or 0 when nothing could be written.
     */
    uint32_t WriteData(uint8_t* buffer, uint32_t size) override {
        if (size == 0) return 0;
        if (physBdlAddr == 0 || physPages[0] == 0) return 0;  // DMA not active.
        if (size > AC97_HALF_SIZE) size = AC97_HALF_SIZE;
        InterruptGuard guard;
        if (buffersOccupied >= 2) {
            return 0;
        }

        // Calculate how many 4KB pages this write spans (1-8 pages).
        uint32_t pagesUsed = (size + 4095) / 4096;
        if (pagesUsed > AC97_BUF_PAGES / 2) pagesUsed = AC97_BUF_PAGES / 2;

        // Pick the ping-pong half to write into (0 or 1).
        uint32_t pageBase = activeHalf * (AC97_BUF_PAGES / 2);

        // Copy the data into the individual pages and write the BDL entries.
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
            // Set IOC on the last entry only.
            bdl[bdlIdx].flags = (remaining == 0) ? 0x8000 : 0;
        }
        asm volatile("wbinvd" ::: "memory");

        // Only update the BDL/LVI state when pages were actually written.
        if (pagesUsed > 0) {
            uint8_t lastIdx = (sw_lvi + pagesUsed - 1) % AC97_BDL_ENTRIES;
            outb(nabmBar + AC97_PO_LVI, lastIdx);
            sw_lvi = (sw_lvi + pagesUsed) % AC97_BDL_ENTRIES;
            activeHalf = 1 - activeHalf;
            buffersOccupied++;
        }
        return size;
    }

    /**
     * AsAudioDriver() - Expose this driver through the audio interface.
     *
     * Return: This instance as an AudioDriver.
     */
    AudioDriver* AsAudioDriver() override {
        return this;
    }
};

/**
 * AC97IRQ::HandleInterrupt() - Forward the interrupt to the sound driver.
 * @esp: Saved stack pointer from the interrupt entry.
 *
 * Return: The (possibly adjusted) stack pointer.
 */
uint32_t AC97IRQ::HandleInterrupt(uint32_t esp) {
    if (driver) driver->OnInterrupt();
    return esp;
}

/**
 * CreateDriverInstance() - Driver entry point returning a new AC97 driver.
 *
 * Return: A newly constructed DynamicAC97Driver instance.
 */
extern "C" Driver* CreateDriverInstance() {
    DynamicAC97Driver* drv = new DynamicAC97Driver();
    if (!drv) {
        HALT("CRITICAL: [AC97] Failed to allocate DynamicAC97Driver!\n");
    }
    return drv;
}
