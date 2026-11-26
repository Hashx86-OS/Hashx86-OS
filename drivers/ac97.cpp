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
/* NABM layout per the AC'97 spec (and QEMU/VirtualBox): PCM-In (record)
 * lives at 0x00-0x0B, PCM-Out (playback) at 0x10-0x1B. Driving 0x00 makes
 * the record engine consume the buffers: CIV advances, but nothing is
 * played, and VirtualBox reports the stream as an input stream. */
#define AC97_PO_BDBAR 0x10
#define AC97_PO_CIV 0x14
#define AC97_PO_LVI 0x15
#define AC97_PO_SR 0x16
#define AC97_PO_PICB 0x18
#define AC97_PO_CR 0x1B

#define AC97_CR_RUN 0x01
#define AC97_CR_RESET 0x02
#define AC97_CR_IOCE 0x10

#define AC97_SR_DCH 0x01
#define AC97_SR_BCIS 0x08
#define AC97_SR_LVBCI 0x04
#define AC97_SR_CELV 0x02

/* ================= Memory ================= */
/* Audio buffer and BDL sizes (in bytes) */
// Keep a 32-page ring, but refill one page at a time (about 23 ms at
// 44.1 kHz stereo). Large mixer chunks turn short producer delays into
// long runs of queued silence.
#define AC97_AUDIO_BUF_SIZE 0x20000
#define AC97_BDL_BUF_SIZE 0x1000
#define AC97_CHUNK_SIZE 4096
#define AC97_BDL_ENTRIES 32
#define AC97_BDL_MASK (AC97_BDL_ENTRIES - 1)

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

DEFINE_DRIVER_INFO("Intel AC97 Audio Driver", "2.3.0-PORegs", {AC97_VENDOR_ID, AC97_DEVICE_ID});

class DynamicAC97Driver;

/* ================= DRIVER ================= */
/* Number of 4KB pages for the audio data buffer (128KB total). */
#define AC97_BUF_PAGES (AC97_AUDIO_BUF_SIZE / 4096)

/**
 * class DynamicAC97Driver - Intel AC97 audio driver using a descriptor ring.
 * @namBar: Base address of the NAM mixer registers.
 * @nabmBar: Base address of the NABM bus-master registers.
 * @physPages: Individually mapped physical pages for the audio buffer.
 * @physBdlAddr: Physical address of the buffer descriptor list.
 * @writeEntry: BDL index where the next chunk will be written.
 */
class DynamicAC97Driver final : public Driver, public AudioDriver {
private:
    uint16_t namBar;
    uint16_t nabmBar;

    // Array of individually-allocated physical pages for the audio buffer.
    // Each page is allocated separately via pmm_alloc_block_low to avoid
    // relying on large contiguous PMM allocations, which can collide with
    // single-page allocations like the Scheduler trampoline on some VMs.
    uint32_t physPages[AC97_BUF_PAGES];  // AC97_AUDIO_BUF_SIZE bytes (128KB = 32 pages).
    uint32_t physBdlAddr;                // Buffer descriptor list (AC97_BDL_BUF_SIZE bytes).

    // --- State ---
    // writeEntry: The BDL index we are preparing to write to (software pointer).
    // queuedEntries: Entries written but not yet seen consumed by CIV. This
    //   must be tracked separately: with the engine stopped, CIV does not move
    //   and a pure (writeEntry - CIV) difference wraps to zero when the ring
    //   fills, which would make the mixer's prefill loop spin forever.
    // lastCiv: Next unaccounted entry (CIV while running, CIV+1 at drain).
    volatile uint8_t writeEntry;
    uint32_t queuedEntries;
    uint8_t lastCiv;

    /**
     * Delay() - Busy-wait approximately the given number of milliseconds.
     * @ms: Delay duration in milliseconds.
     */
    void Delay(int ms) {
        for (volatile int i = 0; i < ms * 10000; i++);
    }

    /**
     * FindHardware() - Locate and map the AC97 controller.
     *
     * Return: True when the controller was found and mapped.
     */
    bool FindHardware() {
        PeripheralComponentInterconnectController pci;
        auto* dev = pci.FindHardwareDevice(AC97_VENDOR_ID, AC97_DEVICE_ID);
        if (!dev || dev->vendor_id == 0) return false;

        constexpr uint32_t kPciCommandInterruptDisable = 1u << 10;
        uint32_t cmd = pci.Read(dev->bus, dev->device, dev->function, 0x04);
        pci.Write(dev->bus, dev->device, dev->function, 0x04,
                  (cmd & 0xFFFF0000) | ((cmd & 0xFFFF) | 0x07 | kPciCommandInterruptDisable));

        namBar =
            (uint16_t)((uint32_t)pci.GetBaseAddressRegister(dev->bus, dev->device, dev->function, 0)
                           .address &
                       0xFFFC);
        nabmBar =
            (uint16_t)((uint32_t)pci.GetBaseAddressRegister(dev->bus, dev->device, dev->function, 1)
                           .address &
                       0xFFFC);

        // Poll completions because the PCI interrupt-line register may not
        // match the firmware's routed IRQ. The mixer polls every 10 ms.
        printf("[AC97] Found device, polling for completions\n");
        return true;
    }

    /**
     * AcknowledgeStatus() - Clear completion bits the model may have raised.
     *
     * Queue accounting uses CIV and does not require completion interrupts.
     */
    void AcknowledgeStatus() {
        if (nabmBar == 0 || physBdlAddr == 0) return;
        uint16_t sr = inw(nabmBar + AC97_PO_SR);
        if (sr & (AC97_SR_BCIS | AC97_SR_LVBCI)) {
            outw(nabmBar + AC97_PO_SR, sr & (AC97_SR_BCIS | AC97_SR_LVBCI));
        }
    }

    /**
     * UpdateConsumed() - Account for entries the controller has finished.
     *
     * Use CIV while running and the halted status to reclaim the final entry.
     */
    void UpdateConsumed() {
        if (!isPlaying || queuedEntries == 0) return;
        uint8_t civ = (uint8_t)(inb(nabmBar + AC97_PO_CIV) & AC97_BDL_MASK);
        uint16_t sr = inw(nabmBar + AC97_PO_SR);
        if ((sr & (AC97_SR_DCH | AC97_SR_CELV)) == (AC97_SR_DCH | AC97_SR_CELV) &&
            inw(nabmBar + AC97_PO_PICB) == 0) {
            // At the end of the list CIV stays on the *completed* final
            // entry. Reclaim it too, including a full ring consumed between
            // polls. LVI publication resumes the engine at the next entry.
            queuedEntries = 0;
            lastCiv = writeEntry;
            return;
        }
        uint32_t delta = (uint8_t)(civ - lastCiv) & AC97_BDL_MASK;
        if (delta > queuedEntries) delta = queuedEntries;
        queuedEntries -= delta;
        lastCiv = civ;
    }

public:
    /**
     * DynamicAC97Driver() - Construct a driver with no hardware mapped yet.
     */
    DynamicAC97Driver() {
        driverName = "Intel AC97";
        namBar = nabmBar = 0;
        for (int i = 0; i < AC97_BUF_PAGES; i++) physPages[i] = 0;
        physBdlAddr = 0;
        writeEntry = 0;
        queuedEntries = 0;
        lastCiv = 0;
    }

    /**
     * ~DynamicAC97Driver() - Release the DMA allocations.
     */
    ~DynamicAC97Driver() {
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
        writeEntry = 0;
        queuedEntries = 0;
        lastCiv = 0;

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
     * GetBufferSize() - Report the size of one mixer chunk.
     *
     * Return: Size in bytes.
     */
    bool HasQueuedData() override {
        InterruptGuard guard;
        UpdateConsumed();
        return queuedEntries != 0;
    }

    uint32_t GetBufferSize() override {
        return AC97_CHUNK_SIZE;
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
     * Start() - Run the DMA engine.
     */
    void Start() override {
        // PCI INTx remains disabled; completions are polled.
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
     * IsReadyForData() - Check whether another mixer chunk can be queued.
     *
     * Queue space is computed from the DMA cursor and halted status.
     *
     * Return: True when at least one page fits in the ring.
     */
    bool IsReadyForData() override {
        if (nabmBar == 0 || physBdlAddr == 0) return false;
        InterruptGuard guard;
        UpdateConsumed();
        AcknowledgeStatus();
        return queuedEntries < AC97_BDL_ENTRIES;
    }

    /**
     * WriteData() - Queue a chunk of PCM data for playback.
     * @buffer: Pointer to the PCM samples to play.
     * @size: Number of bytes to queue.
     *
     * Copies the data into free BDL entries and advances LVI past them. The
     * ring is only written behind the controller's CIV cursor.
     *
     * Return: The number of bytes queued, or 0 when nothing could be written.
     */
    uint32_t WriteData(uint8_t* buffer, uint32_t size) override {
        size &= ~3u;  // Whole stereo 16-bit frames only.
        if (size == 0) return 0;
        if (physBdlAddr == 0 || physPages[0] == 0) return 0;  // DMA not active.
        if (size > AC97_CHUNK_SIZE) size = AC97_CHUNK_SIZE;
        InterruptGuard guard;

        uint32_t pagesUsed = (size + 4095) / 4096;
        if (pagesUsed == 0 || pagesUsed > AC97_BDL_ENTRIES) return 0;
        UpdateConsumed();
        if (queuedEntries + pagesUsed > AC97_BDL_ENTRIES) return 0;

        // Copy the data into the individual pages and write the BDL entries,
        // walking forward from the software write cursor.
        AC97_BDL_Entry* bdl = (AC97_BDL_Entry*)physBdlAddr;
        uint32_t remaining = size;
        uint32_t srcOff = 0;

        for (uint32_t p = 0; p < pagesUsed && remaining > 0; p++) {
            uint8_t idx = (uint8_t)((writeEntry + p) & AC97_BDL_MASK);
            uint32_t dstPhys = physPages[idx];
            uint32_t chunk = (remaining < 4096) ? remaining : 4096;
            memcpy((void*)dstPhys, buffer + srcOff, chunk);
            srcOff += chunk;
            remaining -= chunk;

            bdl[idx].addr = dstPhys;
            bdl[idx].length = (uint16_t)(chunk / 2);
            // Poll the DMA cursor instead of requesting an interrupt.
            bdl[idx].flags = 0;
        }
        // x86 PCI DMA is cache coherent. Publish sample/descriptor stores
        // before LVI without flushing every CPU cache on each refill.
        asm volatile("" ::: "memory");

        uint8_t lastIdx = (uint8_t)((writeEntry + pagesUsed - 1) & AC97_BDL_MASK);
        outb(nabmBar + AC97_PO_LVI, lastIdx);
        writeEntry = (uint8_t)((writeEntry + pagesUsed) & AC97_BDL_MASK);
        queuedEntries += pagesUsed;
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
