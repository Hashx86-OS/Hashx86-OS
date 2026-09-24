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
#include <core/drivers/GraphicsDriver.h>
#include <core/drivers/driver_info.h>
#include <core/globals.h>
#include <core/paging.h>
#include <core/pci.h>
#include <gui/config/config.h>

// --- BGA Constants ---
#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA 0x01CF
#define VBE_DISPI_INDEX_ID 0
#define VBE_DISPI_INDEX_XRES 1
#define VBE_DISPI_INDEX_YRES 2
#define VBE_DISPI_INDEX_BPP 3
#define VBE_DISPI_INDEX_ENABLE 4
#define VBE_DISPI_INDEX_BANK 5
#define VBE_DISPI_INDEX_VIRT_WIDTH 6

#define VBE_DISPI_INDEX_X_OFFSET 8
#define VBE_DISPI_INDEX_Y_OFFSET 9

#define VBE_DISPI_DISABLED 0x00
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_LFB_ENABLED 0x40

DEFINE_DRIVER_INFO("BGA Driver for Hashx86", "0.1.0", {0x1234, 0x1111},  // QEMU / Bochs
                   {0x80EE, 0xBEEF},                                     // VirtualBox
                   {0x15AD, 0x0405}                                      // VMware / VBox SVGA
);

/**
 * class DynamicBGADriver - Bochs/VirtualBox/VMware SVGA graphics driver.
 * @physFramebufferAddr: Physical address of the linear framebuffer.
 */
class DynamicBGADriver : public Driver, public GraphicsDriver {
private:
    uint32_t physFramebufferAddr;

    /**
     * WriteRegister() - Write one VBE-DISPI register pair.
     * @index: Register index.
     * @value: Value to write.
     */
    void WriteRegister(uint16_t index, uint16_t value) {
        outw(VBE_DISPI_IOPORT_INDEX, index);
        outw(VBE_DISPI_IOPORT_DATA, value);
    }

    /**
     * ReadRegister() - Read one VBE-DISPI register.
     * @index: Register index.
     *
     * Return: The register value.
     */
    uint16_t ReadRegister(uint16_t index) {
        outw(VBE_DISPI_IOPORT_INDEX, index);
        return inw(VBE_DISPI_IOPORT_DATA);
    }

    /**
     * FindFramebufferPCI() - Locate the LFB physical address for a BGA adapter.
     *
     * Scans the PCI bus for a known BGA/SVGA device, enables bus mastering
     * and reads the first memory-mapped BAR.
     *
     * Return: The framebuffer physical address, or 0 on failure.
     */
    uint32_t FindFramebufferPCI() {
        PeripheralComponentInterconnectController pci;
        PeripheralComponentInterconnectDeviceDescriptor* dev;

        dev = pci.FindHardwareDevice(0x1234, 0x1111);
        if (!dev || dev->vendor_id == 0) {
            dev = pci.FindHardwareDevice(0x80EE, 0xBEEF);
        }
        if (!dev || dev->vendor_id == 0) {
            dev = pci.FindHardwareDevice(0x15AD, 0x0405);
        }
        if (!dev || dev->vendor_id == 0) {
            return 0;
        }

        // Enable bus mastering.
        uint32_t pci_cmd = pci.Read(dev->bus, dev->device, dev->function, 0x04);
        pci.Write(dev->bus, dev->device, dev->function, 0x04, pci_cmd | 0x07);

        // Scan the BARs.
        for (int i = 0; i < 6; i++) {
            BaseAddressRegister bar =
                pci.GetBaseAddressRegister(dev->bus, dev->device, dev->function, i);

            if (bar.type == MemoryMapping && bar.address != 0) {
                // PCI spec: the lower 4 bits are flags (prefetchable, type, etc.).
                uint32_t addr = (uint32_t)((uint32_t)bar.address & (uint32_t)0xFFFFFFF0);
                return addr;
            }
        }
        return 0;
    }

public:
    /**
     * DynamicBGADriver() - Construct the driver without attaching hardware.
     */
    DynamicBGADriver() : GraphicsDriver(GUI_SCREEN_WIDTH, GUI_SCREEN_HEIGHT, GUI_SCREEN_BPP, 0) {
        this->driverName = "BGA Driver for Hashx86";
        this->physFramebufferAddr = 0;
    }

    /**
     * Activate() - Find the framebuffer and set up the video mode.
     */
    void Activate() override {
        // Locate the hardware and its framebuffer.
        this->physFramebufferAddr = FindFramebufferPCI();

        if (this->physFramebufferAddr == 0) {
            printf("[BGA] Error: No compatible Graphics Card found via PCI.\n");
            return;
        }

        printf("[BGA] Hardware Found. LFB @ 0x%x\n", this->physFramebufferAddr);

        // Set the video mode.
        WriteRegister(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
        WriteRegister(VBE_DISPI_INDEX_ID, 0xB0C5);  // VBE 3.0.
        WriteRegister(VBE_DISPI_INDEX_X_OFFSET, 0);
        WriteRegister(VBE_DISPI_INDEX_Y_OFFSET, 0);
        WriteRegister(VBE_DISPI_INDEX_XRES, this->width);
        WriteRegister(VBE_DISPI_INDEX_YRES, this->height);
        WriteRegister(VBE_DISPI_INDEX_BPP, 32);
        WriteRegister(VBE_DISPI_INDEX_VIRT_WIDTH, this->width);
        WriteRegister(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

        uint16_t bpp_result = ReadRegister(VBE_DISPI_INDEX_BPP);
        if (bpp_result != 32) {
            printf("[BGA] Warning: Hardware refused 32-bit mode! Got: %d\n", bpp_result);
        }
        uint64_t fb_size = (uint64_t)this->width * (uint64_t)this->height * 4;
        if (fb_size == 0 || fb_size > 0xFFFFFFFFu) {
            printf("[BGA] Error: Invalid framebuffer size\n");
            return;
        }
        if (!g_paging || !g_paging->KernelPageDirectory) {
            printf("[BGA] Error: Paging not initialized\n");
            return;
        }

        uint32_t start = this->physFramebufferAddr & ~(PAGE_SIZE - 1);

        // Compute end in 64-bit to avoid wrap on 32-bit truncation.
        uint64_t end64 = (uint64_t)this->physFramebufferAddr + fb_size + PAGE_SIZE - 1;
        // Also mask in 64-bit so the alignment is correct even if the sum exceeds 4GB.
        uint64_t alignedEnd = end64 & ~((uint64_t)PAGE_SIZE - 1);
        if (alignedEnd < (uint64_t)start || alignedEnd > 0xFFFFFFFFu) {
            printf("[BGA] Error: Framebuffer region wraps or exceeds 4GB\n");
            return;
        }
        uint32_t end = (uint32_t)alignedEnd;

        for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
            uint32_t existing = g_paging->GetPhysicalAddress(g_paging->KernelPageDirectory, addr);
            if (existing == 0xFFFFFFFF || existing != addr) {
                if (!g_paging->MapPage(g_paging->KernelPageDirectory, addr, addr,
                                       PAGE_PRESENT | PAGE_RW)) {
                    printf("[BGA] Error: Failed to map framebuffer\n");
                    return;
                }
            }
        }

        // Update the GraphicsDriver framebuffer pointer.
        this->videoMemory = (uint32_t*)this->physFramebufferAddr;

        printf("[BGA] Mode Set: %dx%d\n", this->width, this->height);
        this->is_Active = true;
    }

    /**
     * Deactivate() - Disable the display.
     */
    void Deactivate() override {
        WriteRegister(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    }

    /**
     * Reset() - Re-activate the driver.
     *
     * Return: 0 on success.
     */
    int Reset() override {
        Activate();
        return 0;
    }

    /**
     * GetPhysicalAddress() - Expose the framebuffer physical address for mapping.
     *
     * Return: The linear framebuffer address.
     */
    uint32_t GetPhysicalAddress() {
        return physFramebufferAddr;
    }

    /**
     * AsGraphicsDriver() - Expose this driver through the graphics interface.
     *
     * Return: This instance as a GraphicsDriver.
     */
    GraphicsDriver* AsGraphicsDriver() override {
        return this;
    }
};

/**
 * CreateDriverInstance() - Driver entry point returning a new BGA driver.
 *
 * Return: A newly constructed DynamicBGADriver instance.
 */
extern "C" Driver* CreateDriverInstance() {
    DynamicBGADriver* drv = new DynamicBGADriver();
    if (!drv) {
        HALT("CRITICAL: [BGA] Failed to allocate DynamicBGADriver!\n");
    }
    return drv;
}
