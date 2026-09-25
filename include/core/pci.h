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

#ifndef PCI_H
#define PCI_H

#include <core/driver.h>
#include <core/interrupts.h>
#include <core/ports.h>
#include <debug.h>
#include <types.h>

// Temporary device-name lookup table used for identification only.
struct PCIDevice {
    uint16_t vendorID;
    uint16_t deviceID;
    const char* vendorName;
    const char* deviceName;
};

static const PCIDevice pciDevices[] = {
    {0x10DE, 0x1C82, "NVIDIA Corporation", "GeForce GTX 1050"},
    {0x1022, 0x1481, "AMD", "Ryzen Controller"},
    {0x10EC, 0x8168, "Realtek Semiconductor", "RTL8111/8168/8411 Ethernet Controller"},
    {0x106B, 0x003F, "Apple Inc.", "KeyLargo/Intrepid USB"},

    {0x8086, 0x1234, "Intel Corporation", "Sample Device A"},
    {0x8086, 0x29C0, "Intel Corporation", "PCI Express Root Port"},
    {0x8086, 0x3B64, "Intel Corporation", "Lynx Point USB xHCI Host Controller"},
    {0x8086, 0x9D03, "Intel Corporation", "HD Audio Controller"},
    {0x8086, 0x1237, "Intel Corporation", "440FX - 82441FX PMC [Natoma]"},
    {0x8086, 0x7000, "Intel Corporation", "82371SB PIIX3 ISA [Natoma/Triton II]"},
    {0x8086, 0x7010, "Intel Corporation", "82371SB PIIX3 IDE [Natoma/Triton II]"},
    {0x8086, 0x7111, "Intel Corporation", "82371AB/EB/MB PIIX4 IDE"},
    {0x8086, 0x2415, "Intel Corporation", "82801AA AC'97 Audio Controller"},
    {0x8086, 0x7113, "Intel Corporation", "82371AB/EB/MB PIIX4 ACPI"},
    {0x8086, 0x265C, "Intel Corporation",
     "82801FB/FBM/FR/FW/FRW (ICH6 Family) USB2 EHCI Controller"},
    {0x8086, 0x2829, "Intel Corporation",
     "82801HM/HEM (ICH8M/ICH8M-E) SATA Controller [AHCI mode]"},
    {0x8086, 0x100E, "Intel Corporation", "82540EM Gigabit Ethernet Controller"},

    {0x1274, 0x5000, "Ensoniq", "ES1370 AudioPCI"},
    {0x1274, 0x1371, "Ensoniq", "ES1371 AudioPCI-97"},

    {0x15AD, 0x0405, "VMware", "SVGA II Adapter"},
    {0x80EE, 0xCAFE, "InnoTek Systemberatung GmbH", "VirtualBox Guest Service"},

    {0x1AF4, 0x1000, "Red Hat, Inc.", "Virtio Network Device"},
    {0x1D0F, 0xEC20, "Amazon.com, Inc.", "Elastic Network Adapter"},
    {0x1B36, 0x000D, "QEMU", "QEMU PCIe Host Bridge"},
    {0x1234, 0x1111, "Bochs", "Bochs VGA Device"},
    {0x1A03, 0x1150, "ASPEED Technology", "Graphics Family"},
};

/**
 * class PeripheralComponentInterconnectDeviceDescriptor - A discovered PCI device.
 *
 * Records the bus/device/function location of a device together with its ID
 * registers, class information and interrupt line.
 */
class PeripheralComponentInterconnectDeviceDescriptor {
public:
    uint32_t portBase;
    uint32_t interrupt;

    uint16_t bus;
    uint16_t device;
    uint16_t function;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t interface_id;

    uint8_t revision;

    PeripheralComponentInterconnectDeviceDescriptor();
    ~PeripheralComponentInterconnectDeviceDescriptor();
};

/**
 * enum BaseAddressRegisterType - Whether a PCI BAR maps memory or I/O space.
 * @MemoryMapping: Memory-mapped BAR.
 * @InputOutput: I/O-space BAR.
 */
enum BaseAddressRegisterType { MemoryMapping = 0, InputOutput = 1 };

/**
 * class BaseAddressRegister - One decoded base address register of a device.
 * @prefetchable: True when the region is prefetchable.
 * @address: Base address of the mapped region.
 * @size: Size of the mapped region.
 * @type: Whether the register maps memory or I/O space.
 */
class BaseAddressRegister {
public:
    bool prefetchable;
    uint8_t* address;
    uint32_t size;
    BaseAddressRegisterType type;
};

/**
 * class PeripheralComponentInterconnectController - Access the PCI configuration space.
 *
 * Talks to PCI devices through the standard 0xCF8/0xCFC configuration ports
 * and exposes configuration reads, writes and device discovery.
 */
class PeripheralComponentInterconnectController {
private:
    Port32Bit dataPort;
    Port32Bit commandPort;

public:
    PeripheralComponentInterconnectController();
    ~PeripheralComponentInterconnectController();

    /**
     * Read() - Read a 32-bit configuration register of a device.
     * @bus: Bus number of the device.
     * @device: Device number on the bus.
     * @function: Function number within the device.
     * @registeroffset: Register offset (word or dword aligned).
     *
     * Return: The register value shifted so the requested offset is in the
     *         low byte.
     */
    uint32_t Read(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset);

    /**
     * Write() - Write a 32-bit configuration register of a device.
     * @bus: Bus number of the device.
     * @device: Device number on the bus.
     * @function: Function number within the device.
     * @registeroffset: Register offset (word or dword aligned).
     * @value: Value to write.
     */
    void Write(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset,
               uint32_t value);

    /**
     * DeviceHasFunctions() - Test whether a device is a multifunction device.
     * @bus: Bus number of the device.
     * @device: Device number on the bus.
     *
     * Return: True when a device exists and its multifunction bit is set.
     */
    bool DeviceHasFunctions(uint16_t bus, uint16_t device);

    /**
     * GetDeviceDescriptor() - Read the configuration header of a device.
     * @bus: Bus number of the device.
     * @device: Device number on the bus.
     * @function: Function number within the device.
     *
     * Return: A new descriptor populated with the device's registers.
     */
    PeripheralComponentInterconnectDeviceDescriptor* GetDeviceDescriptor(uint16_t bus,
                                                                         uint16_t device,
                                                                         uint16_t function);

    /**
     * GetBaseAddressRegister() - Decode one base address register of a device.
     * @bus: Bus number of the device.
     * @device: Device number on the bus.
     * @function: Function number within the device.
     * @bar: Index of the base address register (0-based).
     *
     * Return: The decoded base address register.
     */
    BaseAddressRegister GetBaseAddressRegister(uint16_t bus, uint16_t device, uint16_t function,
                                               uint16_t bar);

    /**
     * FindHardwareDevice() - Scan the bus for a specific vendor/device ID.
     * @vendorID: Vendor ID to find.
     * @deviceID: Device ID to find.
     *
     * Scans bus 0 first, then probes higher buses.
     *
     * Return: A descriptor of the matched device, or an empty descriptor
     *         (vendor_id == 0) when nothing matches; caller owns the object.
     */
    PeripheralComponentInterconnectDeviceDescriptor* FindHardwareDevice(uint16_t vendorID,
                                                                        uint16_t deviceID);
};

#endif
