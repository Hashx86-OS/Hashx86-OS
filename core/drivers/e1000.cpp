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

// Intel 82540EM (e1000) Ethernet driver.
//
// VirtualBox exposes its default Intel PRO/1000 MT Desktop NIC as 8086:100E.
// The device has a 128 KiB memory-mapped register window in BAR0. This driver
// uses the legacy 16-byte RX/TX descriptor format, keeps interrupts masked and
// polls the rings from the network thread, matching the RTL8139 driver.

#include <core/drivers/e1000.h>
#include <core/globals.h>
#include <core/net/net_device.h>
#include <core/paging.h>
#include <core/pci.h>
#include <core/pmm.h>
#include <debug.h>
#include <string.h>
#include <types.h>

namespace {

constexpr uint16_t kIntelVendorId = 0x8086;
constexpr uint16_t k82540EmDeviceId = 0x100E;

constexpr uint32_t kMmioRegionSize = 0x20000;

// Register offsets from the memory-mapped BAR.
constexpr uint32_t kRegCtrl = 0x0000;
constexpr uint32_t kRegStatus = 0x0008;
constexpr uint32_t kRegEerd = 0x0014;
constexpr uint32_t kRegIcr = 0x00C0;
constexpr uint32_t kRegIms = 0x00D0;
constexpr uint32_t kRegImc = 0x00D8;
constexpr uint32_t kRegRctl = 0x0100;
constexpr uint32_t kRegTctl = 0x0400;
constexpr uint32_t kRegTipg = 0x0410;
constexpr uint32_t kRegRdbal = 0x2800;
constexpr uint32_t kRegRdbah = 0x2804;
constexpr uint32_t kRegRdlen = 0x2808;
constexpr uint32_t kRegRdh = 0x2810;
constexpr uint32_t kRegRdt = 0x2818;
constexpr uint32_t kRegTdbal = 0x3800;
constexpr uint32_t kRegTdbah = 0x3804;
constexpr uint32_t kRegTdlen = 0x3808;
constexpr uint32_t kRegTdh = 0x3810;
constexpr uint32_t kRegTdt = 0x3818;
constexpr uint32_t kRegMta = 0x5200;
constexpr uint32_t kRegRal0 = 0x5400;
constexpr uint32_t kRegRah0 = 0x5404;

constexpr uint32_t kCtrlSetLinkUp = 1u << 6;
constexpr uint32_t kCtrlReset = 1u << 26;
constexpr uint32_t kStatusLinkUp = 1u << 1;

constexpr uint32_t kEerdStart = 1u << 0;
constexpr uint32_t kEerdDone = 1u << 4;
constexpr uint32_t kEerdAddressShift = 8;
constexpr uint32_t kEerdDataShift = 16;

constexpr uint32_t kRctlEnable = 1u << 1;
constexpr uint32_t kRctlBroadcastAccept = 1u << 15;
constexpr uint32_t kRctlStripCrc = 1u << 26;
constexpr uint32_t kRctlConfig = kRctlEnable | kRctlBroadcastAccept | kRctlStripCrc;

constexpr uint32_t kTctlEnable = 1u << 1;
constexpr uint32_t kTctlPadShortPackets = 1u << 3;
constexpr uint32_t kTctlCollisionThreshold = 0x10u << 4;
constexpr uint32_t kTctlCollisionDistance = 0x40u << 12;
constexpr uint32_t kTctlConfig =
    kTctlEnable | kTctlPadShortPackets | kTctlCollisionThreshold | kTctlCollisionDistance;

constexpr uint32_t kTipgConfig = 10u | (8u << 10) | (6u << 20);

constexpr uint32_t kRahAddressValid = 1u << 31;
constexpr uint32_t kMtaEntries = 128;

constexpr uint8_t kRxDescDone = 0x01;
constexpr uint8_t kRxDescEndOfPacket = 0x02;
constexpr uint8_t kTxDescDone = 0x01;
constexpr uint8_t kTxCmdEndOfPacket = 0x01;
constexpr uint8_t kTxCmdInsertFcs = 0x02;
constexpr uint8_t kTxCmdReportStatus = 0x08;

constexpr uint32_t kRxDescriptorCount = 32;
constexpr uint32_t kTxDescriptorCount = 8;
constexpr uint32_t kBufferSize = 2048;
constexpr uint16_t kMinFrameSize = 60;
constexpr uint16_t kMaxFrameSize = 1600;

struct E1000RxDescriptor {
    uint32_t addressLow;
    uint32_t addressHigh;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct E1000TxDescriptor {
    uint32_t addressLow;
    uint32_t addressHigh;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

static_assert(sizeof(E1000RxDescriptor) == 16, "e1000 RX descriptor must be 16 bytes");
static_assert(sizeof(E1000TxDescriptor) == 16, "e1000 TX descriptor must be 16 bytes");

const char* const kHexDigits = "0123456789ABCDEF";

/**
 * MacToString() - Format a MAC address as six colon-separated hex pairs.
 */
void MacToString(const uint8_t* mac, char* out) {
    for (uint32_t i = 0; i < 6; i++) {
        if (i != 0) *out++ = ':';
        *out++ = kHexDigits[(mac[i] >> 4) & 0x0F];
        *out++ = kHexDigits[mac[i] & 0x0F];
    }
    *out = '\0';
}

class E1000Device final : public NetworkDevice {
public:
    ~E1000Device() override;

    bool SendFrame(const uint8_t* frame, uint16_t length) override;
    const uint8_t* GetMac() const override;
    void SetReceiveHandler(NetworkReceiveHandler handler, void* context) override;
    void Poll() override;

private:
    friend NetworkDevice* ::E1000Probe();

    bool Initialize();
    bool ReadMacAddress();
    bool InitializeRings();
    bool MarkMmioUncached();
    void ReclaimTransmitDescriptors();
    void ReleaseResources();

    uint32_t Read(uint32_t reg) const;
    void Write(uint32_t reg, uint32_t value);
    uint16_t ReadEeprom(uint8_t address);

    volatile uint8_t* mmio = nullptr;
    uint32_t mmioPhysical = 0;
    uint8_t mac[6] = {};
    bool initialized = false;
    bool linkLogged = false;

    void* rxRingMemory = nullptr;
    uint32_t rxRingPhysical = 0;
    volatile E1000RxDescriptor* rxRing = nullptr;
    uint8_t* rxBuffers[kRxDescriptorCount] = {};
    uint32_t rxTail = 0;

    void* txRingMemory = nullptr;
    uint32_t txRingPhysical = 0;
    volatile E1000TxDescriptor* txRing = nullptr;
    uint8_t* txBuffers[kTxDescriptorCount] = {};
    uint32_t txHead = 0;
    uint32_t txTail = 0;

    NetworkReceiveHandler receiveHandler = nullptr;
    void* receiveContext = nullptr;
};

E1000Device::~E1000Device() {
    ReleaseResources();
}

uint32_t E1000Device::Read(uint32_t reg) const {
    return *reinterpret_cast<volatile uint32_t*>(mmio + reg);
}

void E1000Device::Write(uint32_t reg, uint32_t value) {
    *reinterpret_cast<volatile uint32_t*>(mmio + reg) = value;
}

uint16_t E1000Device::ReadEeprom(uint8_t address) {
    Write(kRegEerd, kEerdStart | ((uint32_t)address << kEerdAddressShift));
    for (uint32_t i = 0; i < 1000000; i++) {
        uint32_t value = Read(kRegEerd);
        if ((value & kEerdDone) != 0) {
            return (uint16_t)(value >> kEerdDataShift);
        }
        asm volatile("pause");
    }
    return 0xFFFF;
}

bool E1000Device::MarkMmioUncached() {
    if (g_paging == nullptr || g_paging->KernelPageDirectory == nullptr) return false;

    uint32_t first = mmioPhysical & ~(PAGE_SIZE - 1);
    uint32_t last = (mmioPhysical + kMmioRegionSize - 1) & ~(PAGE_SIZE - 1);
    for (uint32_t address = first;; address += PAGE_SIZE) {
        uint32_t pdIndex = address >> 22;
        uint32_t ptIndex = (address >> 12) & 0x03FF;
        uint32_t directoryEntry = g_paging->KernelPageDirectory[pdIndex];
        if ((directoryEntry & PAGE_PRESENT) == 0) return false;

        uint32_t* table = (uint32_t*)(directoryEntry & 0xFFFFF000);
        table[ptIndex] |= PAGE_NO_CACHE | PAGE_WRITE_THRU;
        asm volatile("invlpg (%0)" ::"r"(address) : "memory");

        if (address >= last) break;
        if (address > 0xFFFFF000u - PAGE_SIZE) return false;
    }
    return true;
}

bool E1000Device::ReadMacAddress() {
    uint16_t words[3];
    for (uint32_t i = 0; i < 3; i++) words[i] = ReadEeprom((uint8_t)i);

    mac[0] = (uint8_t)(words[0] & 0xFF);
    mac[1] = (uint8_t)(words[0] >> 8);
    mac[2] = (uint8_t)(words[1] & 0xFF);
    mac[3] = (uint8_t)(words[1] >> 8);
    mac[4] = (uint8_t)(words[2] & 0xFF);
    mac[5] = (uint8_t)(words[2] >> 8);

    bool valid = false;
    for (uint32_t i = 0; i < 6; i++) {
        if (mac[i] != 0x00 && mac[i] != 0xFF) valid = true;
    }

    if (!valid) {
        // Some firmware leaves the EEPROM inaccessible until later in POST.
        // The reset default is also mirrored in RAL0/RAH0.
        uint32_t ral = Read(kRegRal0);
        uint32_t rah = Read(kRegRah0) & 0xFFFF;
        mac[0] = (uint8_t)(ral & 0xFF);
        mac[1] = (uint8_t)((ral >> 8) & 0xFF);
        mac[2] = (uint8_t)((ral >> 16) & 0xFF);
        mac[3] = (uint8_t)((ral >> 24) & 0xFF);
        mac[4] = (uint8_t)(rah & 0xFF);
        mac[5] = (uint8_t)((rah >> 8) & 0xFF);

        valid = false;
        for (uint32_t i = 0; i < 6; i++) {
            if (mac[i] != 0x00 && mac[i] != 0xFF) valid = true;
        }
    }

    return valid && (mac[0] & 0x01) == 0;
}

bool E1000Device::InitializeRings() {
    rxRingMemory = pmm_alloc_block_low(256 * 1024 * 1024);
    txRingMemory = pmm_alloc_block_low(256 * 1024 * 1024);
    if (rxRingMemory == nullptr || txRingMemory == nullptr) {
        printf("[NET] e1000: descriptor ring allocation failed\n");
        return false;
    }

    memset(rxRingMemory, 0, 4096);
    memset(txRingMemory, 0, 4096);
    rxRingPhysical = (uint32_t)rxRingMemory;
    txRingPhysical = (uint32_t)txRingMemory;
    rxRing = (volatile E1000RxDescriptor*)rxRingMemory;
    txRing = (volatile E1000TxDescriptor*)txRingMemory;

    for (uint32_t i = 0; i < kRxDescriptorCount; i++) {
        rxBuffers[i] = (uint8_t*)pmm_alloc_block_low(256 * 1024 * 1024);
        if (rxBuffers[i] == nullptr) {
            printf("[NET] e1000: RX buffer allocation failed\n");
            return false;
        }
        memset(rxBuffers[i], 0, kBufferSize);
        rxRing[i].addressLow = (uint32_t)rxBuffers[i];
        rxRing[i].addressHigh = 0;
        rxRing[i].length = 0;
        rxRing[i].checksum = 0;
        rxRing[i].status = 0;
        rxRing[i].errors = 0;
        rxRing[i].special = 0;
    }

    for (uint32_t i = 0; i < kTxDescriptorCount; i++) {
        txBuffers[i] = (uint8_t*)pmm_alloc_block_low(256 * 1024 * 1024);
        if (txBuffers[i] == nullptr) {
            printf("[NET] e1000: TX buffer allocation failed\n");
            return false;
        }
        memset(txBuffers[i], 0, kBufferSize);
        txRing[i].addressLow = (uint32_t)txBuffers[i];
        txRing[i].addressHigh = 0;
        txRing[i].length = 0;
        txRing[i].cso = 0;
        txRing[i].cmd = 0;
        txRing[i].status = 0;
        txRing[i].css = 0;
        txRing[i].special = 0;
    }

    Write(kRegRdbal, rxRingPhysical);
    Write(kRegRdbah, 0);
    Write(kRegRdlen, kRxDescriptorCount * sizeof(E1000RxDescriptor));
    Write(kRegRdh, 0);
    Write(kRegRdt, kRxDescriptorCount - 1);

    Write(kRegTdbal, txRingPhysical);
    Write(kRegTdbah, 0);
    Write(kRegTdlen, kTxDescriptorCount * sizeof(E1000TxDescriptor));
    Write(kRegTdh, 0);
    Write(kRegTdt, 0);

    Write(kRegTipg, kTipgConfig);
    Write(kRegRctl, kRctlConfig);
    Write(kRegTctl, kTctlConfig);

    rxTail = 0;
    txHead = 0;
    txTail = 0;
    return true;
}

bool E1000Device::Initialize() {
    PeripheralComponentInterconnectController pci;
    PeripheralComponentInterconnectDeviceDescriptor* dev =
        pci.FindHardwareDevice(kIntelVendorId, k82540EmDeviceId);
    if (dev == nullptr || dev->vendor_id == 0) {
        delete dev;
        return false;
    }

    uint16_t bus = dev->bus;
    uint16_t deviceNumber = dev->device;
    uint16_t function = dev->function;
    uint8_t irqLine = (uint8_t)(dev->interrupt & 0xFF);

    // Memory decoding and bus mastering are both required.
    uint32_t command = pci.Read(bus, deviceNumber, function, 0x04);
    pci.Write(bus, deviceNumber, function, 0x04,
              (command & 0xFFFF0000) | ((command & 0xFFFF) | 0x0007));

    uint32_t rawBar = pci.Read(bus, deviceNumber, function, 0x10);
    uint32_t barType = (rawBar >> 1) & 0x03;
    if ((rawBar & 0x01) != 0 || barType > 0x02) {
        delete dev;
        printf("[NET] e1000: BAR0 is not a memory region\n");
        return false;
    }
    if (barType == 0x02 && pci.Read(bus, deviceNumber, function, 0x14) != 0) {
        delete dev;
        printf("[NET] e1000: 64-bit MMIO above 4 GiB is unsupported\n");
        return false;
    }

    uint32_t bar = rawBar & 0xFFFFFFF0;
    delete dev;

    constexpr uint32_t kLowMemoryLimit = 256u * 1024u * 1024u;
    if (bar == 0 || (bar >= kLowMemoryLimit && bar < 0xC0000000u)) {
        printf("[NET] e1000: BAR0 0x%x is outside the identity-mapped ranges\n", bar);
        return false;
    }
    if (bar > 0xFFFFFFFFu - kMmioRegionSize) {
        printf("[NET] e1000: BAR0 0x%x is too close to the 4 GiB boundary\n", bar);
        return false;
    }

    mmioPhysical = bar;
    mmio = reinterpret_cast<volatile uint8_t*>(bar);
    if (!MarkMmioUncached()) {
        printf("[NET] e1000: MMIO region 0x%x is not mapped\n", bar);
        mmio = nullptr;
        return false;
    }

    Write(kRegImc, 0xFFFFFFFF);
    (void)Read(kRegIcr);
    Write(kRegIms, 0);

    Write(kRegCtrl, Read(kRegCtrl) | kCtrlReset);
    bool resetDone = false;
    for (uint32_t i = 0; i < 1000000; i++) {
        if ((Read(kRegCtrl) & kCtrlReset) == 0) {
            resetDone = true;
            break;
        }
        asm volatile("pause");
    }
    if (!resetDone) {
        printf("[NET] e1000: controller reset timed out\n");
        ReleaseResources();
        return false;
    }

    // The EEPROM auto-read and PHY need a short settle period after reset.
    for (uint32_t i = 0; i < 100000; i++) asm volatile("pause");

    Write(kRegImc, 0xFFFFFFFF);
    (void)Read(kRegIcr);
    Write(kRegIms, 0);

    if (!ReadMacAddress()) {
        printf("[NET] e1000: could not read a valid MAC address\n");
        ReleaseResources();
        return false;
    }

    for (uint32_t i = 0; i < kMtaEntries; i++) {
        Write(kRegMta + i * 4, 0);
    }

    uint32_t ral = (uint32_t)mac[0] | ((uint32_t)mac[1] << 8) | ((uint32_t)mac[2] << 16) |
                   ((uint32_t)mac[3] << 24);
    uint32_t rah = (uint32_t)mac[4] | ((uint32_t)mac[5] << 8) | kRahAddressValid;
    Write(kRegRal0, ral);
    Write(kRegRah0, rah);

    if (!InitializeRings()) {
        ReleaseResources();
        return false;
    }

    Write(kRegCtrl, Read(kRegCtrl) | kCtrlSetLinkUp);

    char macText[18];
    MacToString(mac, macText);
    printf("[NET] e1000: PCI %x:%x.%x mmio=0x%x irq=%u mac=%s\n", (uint32_t)bus,
           (uint32_t)deviceNumber, (uint32_t)function, bar, (uint32_t)irqLine, macText);

    initialized = true;
    return true;
}

void E1000Device::ReclaimTransmitDescriptors() {
    while (txHead != txTail) {
        if ((txRing[txHead].status & kTxDescDone) == 0) break;
        txRing[txHead].status = 0;
        txHead = (txHead + 1) % kTxDescriptorCount;
    }
}

bool E1000Device::SendFrame(const uint8_t* frame, uint16_t length) {
    if (!initialized || frame == nullptr || length == 0 || length > kMaxFrameSize) {
        return false;
    }

    ReclaimTransmitDescriptors();

    uint32_t next = (txTail + 1) % kTxDescriptorCount;
    if (next == txHead) {
        return false;
    }

    uint32_t index = txTail;
    uint16_t transmitLength = length < kMinFrameSize ? kMinFrameSize : length;
    memcpy(txBuffers[index], frame, length);
    if (transmitLength > length) {
        memset(txBuffers[index] + length, 0, transmitLength - length);
    }

    volatile E1000TxDescriptor* desc = &txRing[index];
    desc->length = transmitLength;
    desc->cso = 0;
    desc->cmd = kTxCmdEndOfPacket | kTxCmdInsertFcs | kTxCmdReportStatus;
    desc->status = 0;
    desc->css = 0;
    desc->special = 0;

    asm volatile("" ::: "memory");
    Write(kRegTdt, next);
    txTail = next;
    return true;
}

const uint8_t* E1000Device::GetMac() const {
    return mac;
}

void E1000Device::SetReceiveHandler(NetworkReceiveHandler handler, void* context) {
    receiveHandler = handler;
    receiveContext = context;
}

void E1000Device::Poll() {
    if (!initialized) return;

    // TX completions are reclaimed by SendFrame() on the tcpip thread. Poll()
    // runs on the RX thread, so touching txHead/txTail here would race it.
    if (!linkLogged && (Read(kRegStatus) & kStatusLinkUp) != 0) {
        linkLogged = true;
        printf("[NET] e1000: link up\n");
    }

    for (uint32_t processed = 0; processed < 64; processed++) {
        volatile E1000RxDescriptor* desc = &rxRing[rxTail];
        uint8_t status = desc->status;
        if ((status & kRxDescDone) == 0) break;

        asm volatile("" ::: "memory");
        uint16_t length = desc->length;
        bool usable = (status & kRxDescEndOfPacket) != 0 && desc->errors == 0 && length >= 14 &&
                      length <= kMaxFrameSize;
        if (usable && receiveHandler != nullptr) {
            receiveHandler(receiveContext, rxBuffers[rxTail], length);
        }

        desc->status = 0;
        desc->errors = 0;
        desc->length = 0;
        asm volatile("" ::: "memory");
        Write(kRegRdt, rxTail);
        rxTail = (rxTail + 1) % kRxDescriptorCount;
    }
}

void E1000Device::ReleaseResources() {
    if (mmio != nullptr) {
        Write(kRegImc, 0xFFFFFFFF);
        Write(kRegRctl, 0);
        Write(kRegTctl, 0);
    }

    for (uint32_t i = 0; i < kRxDescriptorCount; i++) {
        if (rxBuffers[i] != nullptr) {
            pmm_free_block(rxBuffers[i]);
            rxBuffers[i] = nullptr;
        }
    }
    for (uint32_t i = 0; i < kTxDescriptorCount; i++) {
        if (txBuffers[i] != nullptr) {
            pmm_free_block(txBuffers[i]);
            txBuffers[i] = nullptr;
        }
    }
    if (rxRingMemory != nullptr) {
        pmm_free_block(rxRingMemory);
        rxRingMemory = nullptr;
    }
    if (txRingMemory != nullptr) {
        pmm_free_block(txRingMemory);
        txRingMemory = nullptr;
    }

    rxRing = nullptr;
    txRing = nullptr;
    rxTail = 0;
    txHead = 0;
    txTail = 0;
    initialized = false;
    mmio = nullptr;
}

}  // namespace

NetworkDevice* E1000Probe() {
    E1000Device* device = new E1000Device();
    if (device == nullptr) return nullptr;

    if (!device->Initialize()) {
        delete device;
        return nullptr;
    }
    return device;
}
