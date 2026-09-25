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

#define KDBG_COMPONENT "PCI"
#include <core/pci.h>

PeripheralComponentInterconnectDeviceDescriptor::PeripheralComponentInterconnectDeviceDescriptor() {
    this->portBase = 0;
    this->interrupt = 0;
    this->bus = 0;
    this->device = 0;
    this->function = 0;
    this->vendor_id = 0;
    this->device_id = 0;
    this->class_id = 0;
    this->subclass_id = 0;
    this->interface_id = 0;
    this->revision = 0;
}

PeripheralComponentInterconnectDeviceDescriptor::
    ~PeripheralComponentInterconnectDeviceDescriptor() {}

PeripheralComponentInterconnectController::PeripheralComponentInterconnectController()
    : dataPort(0xCFC), commandPort(0xCF8) {}

PeripheralComponentInterconnectController::~PeripheralComponentInterconnectController() {}

uint32_t PeripheralComponentInterconnectController::Read(uint16_t bus, uint16_t device,
                                                         uint16_t function,
                                                         uint32_t registeroffset) {
    uint32_t id = 0x80000000 | ((bus & 0xFF) << 16) | ((device & 0x1F) << 11) |
                  ((function & 0x07) << 8) | (registeroffset & 0xFC);
    commandPort.Write(id);
    uint32_t result = dataPort.Read();
    return result >> (8 * (registeroffset % 4));
}

void PeripheralComponentInterconnectController::Write(uint16_t bus, uint16_t device,
                                                      uint16_t function, uint32_t registeroffset,
                                                      uint32_t value) {
    uint32_t id = 0x80000000 | ((bus & 0xFF) << 16) | ((device & 0x1F) << 11) |
                  ((function & 0x07) << 8) | (registeroffset & 0xFC);
    commandPort.Write(id);
    dataPort.Write(value);
}

bool PeripheralComponentInterconnectController::DeviceHasFunctions(uint16_t bus, uint16_t device) {
    // Check first that a device exists at this bus/device location.
    uint16_t vendor = Read(bus, device, 0, 0x00);
    if (vendor == 0x0000 || vendor == 0xFFFF) {
        return false;  // No device present.
    }
    // Check the multifunction bit.
    return Read(bus, device, 0, 0x0E) & (1 << 7);
}

PeripheralComponentInterconnectDeviceDescriptor*
PeripheralComponentInterconnectController::GetDeviceDescriptor(uint16_t bus, uint16_t device,
                                                               uint16_t function) {
    PeripheralComponentInterconnectDeviceDescriptor* result =
        new PeripheralComponentInterconnectDeviceDescriptor();
    if (!result) {
        HALT("CRITICAL: Failed to allocate PCI device descriptor!\n");
    }
    result->bus = bus;
    result->device = device;
    result->function = function;

    result->vendor_id = Read(bus, device, function, 0x00);
    result->device_id = Read(bus, device, function, 0x02);
    result->class_id = Read(bus, device, function, 0x0b);
    result->subclass_id = Read(bus, device, function, 0x0a);
    result->interface_id = Read(bus, device, function, 0x09);
    result->revision = Read(bus, device, function, 0x08);
    result->interrupt = Read(bus, device, function, 0x3c);
    return result;
}

BaseAddressRegister PeripheralComponentInterconnectController::GetBaseAddressRegister(
    uint16_t bus, uint16_t device, uint16_t function, uint16_t bar) {
    BaseAddressRegister result;
    result.address = 0;  // Init.

    uint32_t headertype = Read(bus, device, function, 0x0E) & 0x7F;
    int maxBARs = 6 - (4 * headertype);
    if (bar >= maxBARs) return result;

    uint32_t bar_value = Read(bus, device, function, 0x10 + 4 * bar);
    result.type = (bar_value & 0x1) ? InputOutput : MemoryMapping;

    if (result.type == MemoryMapping) {
        // Mask the low 4 flag bits to get a 16-byte aligned address.
        result.address = (uint8_t*)(bar_value & 0xFFFFFFF0);
        result.prefetchable = ((bar_value >> 3) & 0x1);
    } else {
        // Input/output space.
        result.address = (uint8_t*)(bar_value & ~0x3);
        result.prefetchable = false;
    }
    return result;
}

// Helper used to look up specific hardware.
PeripheralComponentInterconnectDeviceDescriptor*
PeripheralComponentInterconnectController::FindHardwareDevice(uint16_t vendorID,
                                                              uint16_t deviceID) {
    // Scan bus 0 first (most devices sit on bus 0 in QEMU/VirtualBox), probing
    // higher buses only when the device is not found there.
    for (int bus = 0; bus < 256; bus++) {
        // Check this bus exists by probing device 0, function 0.
        if (bus > 0) {
            uint16_t probe = Read(bus, 0, 0, 0x00);
            if (probe == 0x0000 || probe == 0xFFFF) {
                continue;  // No devices on this bus, skip it.
            }
        }
        for (int device = 0; device < 32; device++) {
            int numFunctions = DeviceHasFunctions(bus, device) ? 8 : 1;
            for (int function = 0; function < numFunctions; function++) {
                PeripheralComponentInterconnectDeviceDescriptor* dev =
                    GetDeviceDescriptor(bus, device, function);

                if (dev->vendor_id == 0x0000 || dev->vendor_id == 0xFFFF) {
                    delete dev;
                    continue;
                }

                if (dev->vendor_id == vendorID && dev->device_id == deviceID) {
                    KDBG2("Found Hardware: Vendor=0x%x Device=0x%x at Bus=%d Device=%d Func=%d",
                          dev->vendor_id, dev->device_id, dev->bus, dev->device, dev->function);
                    return dev;
                }

                delete dev;
            }
        }
    }
    // Return an empty descriptor when nothing matches.
    PeripheralComponentInterconnectDeviceDescriptor* empty =
        new PeripheralComponentInterconnectDeviceDescriptor();
    if (!empty) {
        HALT("CRITICAL: Failed to allocate PCI device descriptor!\n");
    }
    empty->vendor_id = 0;
    return empty;
}

extern "C" {
// C-friendly helper to locate BAR0.
uint32_t pci_find_bar0(uint16_t vendor, uint16_t device) {
    PeripheralComponentInterconnectController pci;
    PeripheralComponentInterconnectDeviceDescriptor* dev = pci.FindHardwareDevice(vendor, device);
    if (!dev || dev->vendor_id == 0) {
        KDBG1("Failed to find device Vendor=0x%x Device=0x%x for BAR0", vendor, device);
        if (dev) delete dev;
        return 0;
    }

    // Return the BAR0 address directly.
    BaseAddressRegister bar = pci.GetBaseAddressRegister(dev->bus, dev->device, dev->function, 0);
    KDBG2("Found BAR0 for Vendor=0x%x Device=0x%x at 0x%x", vendor, device, (uint32_t)bar.address);
    uint32_t addr = (uint32_t)bar.address;
    delete dev;
    return addr;
}

// C-friendly helper to enable bus mastering.
void pci_enable_bus_master(uint16_t vendor, uint16_t device) {
    PeripheralComponentInterconnectController pci;
    PeripheralComponentInterconnectDeviceDescriptor* dev = pci.FindHardwareDevice(vendor, device);
    if (dev && dev->vendor_id != 0) {
        uint32_t cmd = pci.Read(dev->bus, dev->device, dev->function, 0x04);
        if (!(cmd & 0x0004)) {
            // Set only the bus-master bit (bit 2), preserving I/O and memory decode bits.
            uint32_t new_command = (cmd & 0xFFFF) | 0x0004;
            pci.Write(dev->bus, dev->device, dev->function, 0x04, new_command);
            KDBG2("Enabled Bus Master for Vendor=0x%x Device=0x%x", vendor, device);
        }
    } else {
        KDBG1("Failed to find device Vendor=0x%x Device=0x%x to enable Bus Master", vendor, device);
    }
    if (dev) delete dev;
}
}
