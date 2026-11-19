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

// Realtek RTL8139 Ethernet driver.
//
// The RTL8139 exposes an I/O-port register window (BAR0). This driver uses the
// classic ring mode:
//   - RX: a contiguous 32 KiB ring plus padding for packets that cross the end
//     of the ring. The chip is programmed for "no wrap" so a packet that would
//     cross the boundary is written linearly into the padding; the software
//     cursor uses the ring size as a power-of-two mask.
//   - TX: four descriptors, each with a 2 KiB buffer.
//
// The interrupt handler only acknowledges status bits and records that work is
// pending; Poll() drains the ring from the network thread. lwIP is never
// entered from interrupt context.

#include <core/drivers/rtl8139.h>
#include <core/interrupts.h>
#include <core/net/net_device.h>
#include <core/pci.h>
#include <core/pmm.h>
#include <core/ports.h>
#include <debug.h>
#include <string.h>
#include <types.h>

namespace {

constexpr uint16_t kRtl8139VendorId = 0x10EC;
constexpr uint16_t kRtl8139DeviceId = 0x8139;

// Register offsets from the I/O BAR.
constexpr uint16_t kRegIdr0 = 0x00;      // MAC address, 6 bytes.
constexpr uint16_t kRegTsd0 = 0x10;      // Transmit status, 4 descriptors.
constexpr uint16_t kRegTsad0 = 0x20;     // Transmit buffer addresses.
constexpr uint16_t kRegRbstart = 0x30;   // Receive buffer start address.
constexpr uint16_t kRegCr = 0x37;        // Command register (8-bit).
constexpr uint16_t kRegCapr = 0x38;      // Current address of packet read.
constexpr uint16_t kRegImr = 0x3C;       // Interrupt mask.
constexpr uint16_t kRegIsr = 0x3E;       // Interrupt status (write 1 to clear).
constexpr uint16_t kRegTcr = 0x40;       // Transmit configuration.
constexpr uint16_t kRegRcr = 0x44;       // Receive configuration.
constexpr uint16_t kRegRxMissed = 0x4C;  // Missed-packet counter (write clears).

// Command register bits.
constexpr uint8_t kCrBufferEmpty = 0x01;
constexpr uint8_t kCrTxEnable = 0x04;
constexpr uint8_t kCrRxEnable = 0x08;
constexpr uint8_t kCrReset = 0x10;

// Interrupt status/mask bits.
constexpr uint16_t kIsrRxOk = 0x0001;
constexpr uint16_t kIsrRxErr = 0x0002;
constexpr uint16_t kIsrTxOk = 0x0004;
constexpr uint16_t kIsrTxErr = 0x0008;
constexpr uint16_t kIsrRxOverflow = 0x0010;
constexpr uint16_t kIsrRxFifoOver = 0x0040;

// One ROK plus the TX/error events we care about. Individual acknowledge bits
// are written back to ISR, so keeping them enabled is harmless.
constexpr uint16_t kInterruptMask =
    kIsrRxOk | kIsrRxErr | kIsrTxOk | kIsrTxErr | kIsrRxOverflow | kIsrRxFifoOver;

// Receive configuration:
//   - 32 KiB ring (bits 11-12 = 10)
//   - no-wrap mode (bit 7); packets that cross the end use the padding
//   - maximum RX FIFO threshold and PCI burst (same as Linux 8139too)
//   - accept broadcast and our physical address
constexpr uint32_t kRcrConfig =
    (1u << 12) | (1u << 7) | (7u << 13) | (7u << 8) | (1u << 3) | (1u << 1);

// Transmit configuration: 9.6 us inter-frame gap, unlimited? 6 = 1024-byte DMA
// burst, 8 retries (matches the values used by Linux 8139too).
constexpr uint32_t kTcrConfig = (3u << 24) | (6u << 8) | (8u << 4);

// TSD bits. Bit 13 (OWN) must stay clear when the host kicks a descriptor;
// bit 19 is the early-TX threshold field (256-byte threshold as used by Linux).
constexpr uint32_t kTsdTxOk = 0x00008000;
constexpr uint32_t kTsdEarlyThreshold = 0x00080000;
constexpr uint32_t kTsdSizeMask = 0x00001FFF;

constexpr uint16_t kRxStatusOk = 0x0001;

constexpr uint32_t kRxRingSize = 32 * 1024;
constexpr uint32_t kRxPadding = 16 + 2048;
constexpr uint32_t kRxAllocationSize = kRxRingSize + kRxPadding;
constexpr uint32_t kRxPages = (kRxAllocationSize + 4095) / 4096;

// Reject obviously corrupt ring entries before touching an out-of-range
// offset. 1536 covers a maximum-size Ethernet frame plus the FCS.
constexpr uint16_t kMaxRxSize = 1536;

constexpr uint32_t kTxDescriptors = 4;
constexpr uint32_t kTxBufferSize = 2048;
constexpr uint16_t kMinFrameSize = 60;
constexpr uint16_t kMaxFrameSize = 1600;

constexpr uint8_t kIrqBase = 0x20;

const char* const kHexDigits = "0123456789ABCDEF";

/**
 * MacToString() - Format a MAC address as six colon-separated hex pairs.
 * @mac: Six-byte hardware address.
 * @out: Destination buffer of at least 18 bytes.
 */
void MacToString(const uint8_t* mac, char* out) {
    for (uint32_t i = 0; i < 6; i++) {
        if (i != 0) *out++ = ':';
        *out++ = kHexDigits[(mac[i] >> 4) & 0x0F];
        *out++ = kHexDigits[mac[i] & 0x0F];
    }
    *out = '\0';
}

class Rtl8139Device;

/**
 * class Rtl8139Irq - Interrupt endpoint for the RTL8139.
 *
 * Runs with interrupts disabled and PIC EOI already sent. It must not call
 * lwIP or allocate memory, so it only acknowledges the chip and records state.
 */
class Rtl8139Irq final : public InterruptHandler {
public:
    Rtl8139Irq(uint8_t vector, Rtl8139Device* device);
    uint32_t HandleInterrupt(uint32_t esp) override;

private:
    Rtl8139Device* device;
};

class Rtl8139Device final : public NetworkDevice {
public:
    ~Rtl8139Device() override;

    bool SendFrame(const uint8_t* frame, uint16_t length) override;
    const uint8_t* GetMac() const override;
    void SetReceiveHandler(NetworkReceiveHandler handler, void* context) override;
    void Poll() override;

    /** Called from the IRQ handler; must remain allocation- and lwIP-free. */
    void HandleInterrupt();

private:
    friend NetworkDevice* ::Rtl8139Probe();

    /** Probe PCI, reset the chip and bring the rings up. */
    bool Initialize();

    /** Program the ring addresses and chip configuration after a reset. */
    void ProgramReceivePath();

    /** Recover from an RX overflow or a corrupt ring entry. */
    void RecoverReceiver();

    /** Free descriptor buffers that the chip has finished transmitting. */
    void ReclaimTransmitDescriptors();

    /** Disable the device and release DMA memory and the IRQ handler. */
    void ReleaseResources();

    uint16_t ioBase = 0;
    uint8_t mac[6] = {};
    bool initialized = false;

    uint8_t* rxBuffer = nullptr;
    uint32_t rxBufferPhys = 0;
    uint32_t rxCursor = 0;

    uint8_t* txBuffers[kTxDescriptors] = {};
    uint32_t txBufferPhys[kTxDescriptors] = {};
    bool txBusy[kTxDescriptors] = {};
    uint8_t nextTx = 0;

    NetworkReceiveHandler receiveHandler = nullptr;
    void* receiveContext = nullptr;

    Rtl8139Irq* irqHandler = nullptr;
    volatile bool rxRecoveryPending = false;
    bool errorLogged = false;
    bool irqObserved = false;
    uint32_t recoveryCount = 0;
    uint32_t rxPackets = 0;
    uint32_t txPackets = 0;
    volatile uint32_t irqCount = 0;
};

Rtl8139Irq::Rtl8139Irq(uint8_t vector, Rtl8139Device* dev)
    : InterruptHandler(vector, InterruptManager::activeInstance), device(dev) {}

uint32_t Rtl8139Irq::HandleInterrupt(uint32_t esp) {
    device->HandleInterrupt();
    return esp;
}

Rtl8139Device::~Rtl8139Device() {
    ReleaseResources();
}

void Rtl8139Device::HandleInterrupt() {
    uint16_t status = inw(ioBase + kRegIsr);
    if (status == 0) return;

    // ISR bits are write-1-to-clear, including the error flags.
    outw(ioBase + kRegIsr, status);
    irqCount++;

    if (status & (kIsrRxOverflow | kIsrRxFifoOver)) {
        rxRecoveryPending = true;
    }
}

bool Rtl8139Device::Initialize() {
    PeripheralComponentInterconnectController pci;
    PeripheralComponentInterconnectDeviceDescriptor* dev =
        pci.FindHardwareDevice(kRtl8139VendorId, kRtl8139DeviceId);
    if (!dev || dev->vendor_id == 0) {
        delete dev;
        return false;
    }

    uint16_t bus = dev->bus;
    uint16_t deviceNumber = dev->device;
    uint16_t function = dev->function;

    // Config space 0x3C contains the interrupt line in its low byte followed
    // by the interrupt pin. 0xFF (and anything outside the 8259 range) means
    // the line is not routed; fall back to polling instead of hijacking an
    // unrelated IRQ.
    uint8_t irqByte = (uint8_t)(dev->interrupt & 0xFF);
    uint8_t irqLine = irqByte < 16 ? irqByte : 0xFF;

    // Enable I/O space and bus mastering.
    uint32_t command = pci.Read(bus, deviceNumber, function, 0x04);
    pci.Write(bus, deviceNumber, function, 0x04,
              (command & 0xFFFF0000) | ((command & 0xFFFF) | 0x0007));

    BaseAddressRegister bar = pci.GetBaseAddressRegister(bus, deviceNumber, function, 0);
    delete dev;

    if (bar.type != InputOutput || bar.address == nullptr) {
        printf("[NET] rtl8139: BAR0 is not an I/O region\n");
        return false;
    }
    ioBase = (uint16_t)((uint32_t)bar.address & 0xFFFC);

    // Soft reset. The reset bit clears itself when the chip is ready.
    outb(ioBase + kRegCr, kCrReset);
    bool resetDone = false;
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(ioBase + kRegCr) & kCrReset) == 0) {
            resetDone = true;
            break;
        }
        asm volatile("pause");
    }
    if (!resetDone) {
        printf("[NET] rtl8139: chip reset timed out\n");
        return false;
    }

    for (uint32_t i = 0; i < 6; i++) {
        mac[i] = inb(ioBase + kRegIdr0 + i);
    }

    // The kernel identity-maps the first 256 MiB; the chip needs 32-bit DMA
    // addresses, so keep every buffer in that range.
    void* rx = pmm_alloc_blocks(kRxPages);
    if (!rx || ((uint32_t)rx + kRxAllocationSize) > (256u * 1024u * 1024u)) {
        printf("[NET] rtl8139: RX ring allocation failed\n");
        if (rx) pmm_free_blocks(rx, kRxPages);
        return false;
    }
    rxBuffer = (uint8_t*)rx;
    rxBufferPhys = (uint32_t)rx;
    memset(rxBuffer, 0, kRxAllocationSize);

    for (uint32_t i = 0; i < kTxDescriptors; i++) {
        void* buffer = pmm_alloc_block_low(256u * 1024u * 1024u);
        if (!buffer) {
            printf("[NET] rtl8139: TX buffer allocation failed\n");
            ReleaseResources();
            return false;
        }
        txBuffers[i] = (uint8_t*)buffer;
        txBufferPhys[i] = (uint32_t)buffer;
        memset(txBuffers[i], 0, kTxBufferSize);
    }

    ProgramReceivePath();

    char macText[18];
    MacToString(mac, macText);
    printf("[NET] rtl8139: PCI %x:%x.%x io=0x%x irq=%u mac=%s\n", (uint32_t)bus,
           (uint32_t)deviceNumber, (uint32_t)function, (uint32_t)ioBase, (uint32_t)irqLine,
           macText);

    // Clear stale status bits before enabling the interrupt mask.
    outw(ioBase + kRegIsr, 0xFFFF);
    outw(ioBase + kRegImr, 0x0000);

    if (irqLine != 0 && irqLine < 16 && InterruptManager::activeInstance != nullptr) {
        irqHandler = new Rtl8139Irq((uint8_t)(kIrqBase + irqLine), this);
    }

    if (irqHandler != nullptr) {
        outw(ioBase + kRegImr, kInterruptMask);
    } else {
        // Polling still works; only the wake-up interrupt is unavailable.
        printf("[NET] rtl8139: no usable IRQ, polling only\n");
    }

    initialized = true;
    return true;
}

void Rtl8139Device::ProgramReceivePath() {
    outl(ioBase + kRegRbstart, rxBufferPhys);
    outb(ioBase + kRegCr, kCrRxEnable | kCrTxEnable);
    outl(ioBase + kRegRcr, kRcrConfig);
    outl(ioBase + kRegTcr, kTcrConfig);

    for (uint32_t i = 0; i < kTxDescriptors; i++) {
        outl(ioBase + kRegTsad0 + i * 4, txBufferPhys[i]);
        txBusy[i] = false;
    }
    nextTx = 0;

    outl(ioBase + kRegRxMissed, 0);

    rxCursor = 0;
    rxRecoveryPending = false;
}

void Rtl8139Device::RecoverReceiver() {
    recoveryCount++;
    if (recoveryCount <= 3) {
        printf("[NET] rtl8139: resetting RX path (recovery %u)\n", recoveryCount);
    }

    outb(ioBase + kRegCr, 0x00);
    outb(ioBase + kRegCr, kCrReset);
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(ioBase + kRegCr) & kCrReset) == 0) break;
        asm volatile("pause");
    }

    memset(rxBuffer, 0, kRxAllocationSize);
    ProgramReceivePath();

    outw(ioBase + kRegIsr, 0xFFFF);
    outw(ioBase + kRegImr, irqHandler != nullptr ? kInterruptMask : 0);
}

void Rtl8139Device::ReclaimTransmitDescriptors() {
    for (uint32_t i = 0; i < kTxDescriptors; i++) {
        if (txBusy[i] && (inl(ioBase + kRegTsd0 + i * 4) & kTsdTxOk) != 0) {
            txBusy[i] = false;
        }
    }
}

bool Rtl8139Device::SendFrame(const uint8_t* frame, uint16_t length) {
    if (!initialized || frame == nullptr || length == 0 || length > kMaxFrameSize) {
        return false;
    }

    ReclaimTransmitDescriptors();

    int32_t index = -1;
    for (uint32_t i = 0; i < kTxDescriptors; i++) {
        uint8_t candidate = (uint8_t)((nextTx + i) % kTxDescriptors);
        if (!txBusy[candidate]) {
            index = candidate;
            break;
        }
    }
    if (index < 0) {
        // All four descriptors are still owned by the chip. lwIP will retry
        // after its next timer tick; dropping here is preferable to blocking
        // the single network thread.
        return false;
    }

    uint16_t transmitLength = length < kMinFrameSize ? kMinFrameSize : length;
    memcpy(txBuffers[index], frame, length);
    if (transmitLength > length) {
        memset(txBuffers[index] + length, 0, transmitLength - length);
    }

    // Make sure the chip sees the buffer contents before the descriptor kick.
    asm volatile("" ::: "memory");
    outl(ioBase + kRegTsad0 + index * 4, txBufferPhys[index]);
    outl(ioBase + kRegTsd0 + index * 4,
         ((uint32_t)transmitLength & kTsdSizeMask) | kTsdEarlyThreshold);
    txBusy[index] = true;
    nextTx = (uint8_t)((index + 1) % kTxDescriptors);
    txPackets++;
    return true;
}

const uint8_t* Rtl8139Device::GetMac() const {
    return mac;
}

void Rtl8139Device::SetReceiveHandler(NetworkReceiveHandler handler, void* context) {
    receiveHandler = handler;
    receiveContext = context;
}

void Rtl8139Device::Poll() {
    if (!initialized) return;

    if (!irqObserved && irqCount != 0) {
        irqObserved = true;
        printf("[NET] rtl8139: interrupt delivery active\n");
    }

    if (rxRecoveryPending) {
        if (!errorLogged) {
            printf("[NET] rtl8139: RX overflow\n");
            errorLogged = true;
        }
        RecoverReceiver();
        return;
    }

    // Bound each call so a continuous stream cannot starve lwIP timers.
    for (uint32_t processed = 0; processed < 64; processed++) {
        if ((inb(ioBase + kRegCr) & kCrBufferEmpty) != 0) break;

        // The chip writes packet headers with 16-bit stores. Force the
        // compiler to re-read the DMA buffer after checking the command
        // register rather than hoisting the loads.
        asm volatile("" ::: "memory");

        uint32_t offset = rxCursor & (kRxRingSize - 1);
        uint16_t status = *(volatile uint16_t*)(rxBuffer + offset);
        uint16_t size = *(volatile uint16_t*)(rxBuffer + offset + 2);

        if ((status & kRxStatusOk) == 0 || size < 8 || size > kMaxRxSize) {
            if (!errorLogged) {
                printf("[NET] rtl8139: malformed RX status=0x%x size=%u\n", (uint32_t)status,
                       (uint32_t)size);
                errorLogged = true;
            }
            RecoverReceiver();
            return;
        }

        uint16_t frameLength = (uint16_t)(size - 4);  // Strip the 4-byte FCS.
        if (frameLength >= 14 && receiveHandler != nullptr) {
            receiveHandler(receiveContext, rxBuffer + offset + 4, frameLength);
            rxPackets++;
        }

        // Header (4) + packet + FCS, padded to a 4-byte boundary.
        rxCursor = (rxCursor + (uint32_t)size + 4u + 3u) & ~3u;
        outw(ioBase + kRegCapr, (uint16_t)(rxCursor - 16u));
    }
}

void Rtl8139Device::ReleaseResources() {
    InterruptGuard guard;

    if (ioBase != 0) {
        outw(ioBase + kRegImr, 0x0000);
    }
    if (irqHandler != nullptr) {
        delete irqHandler;
        irqHandler = nullptr;
    }

    for (uint32_t i = 0; i < kTxDescriptors; i++) {
        if (txBuffers[i] != nullptr) {
            pmm_free_block(txBuffers[i]);
            txBuffers[i] = nullptr;
            txBufferPhys[i] = 0;
        }
        txBusy[i] = false;
    }

    if (rxBuffer != nullptr) {
        pmm_free_blocks(rxBuffer, kRxPages);
        rxBuffer = nullptr;
        rxBufferPhys = 0;
    }

    initialized = false;
}

}  // namespace

NetworkDevice* Rtl8139Probe() {
    Rtl8139Device* device = new Rtl8139Device();
    if (device == nullptr) return nullptr;

    if (!device->Initialize()) {
        delete device;
        return nullptr;
    }
    return device;
}
