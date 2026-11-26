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

#define KDBG_COMPONENT "CORE:POWER"

#include <core/ports.h>
#include <core/power.h>
#include <debug.h>

namespace {

/**
 * haltForever() - Stop the CPU with interrupts masked.
 *
 * The only defined exit for a shutdown that no device acted on.
 */
[[noreturn]] void haltForever() {
    for (;;) {
        asm volatile("cli; hlt");
    }
}

/**
 * tripleFaultReset() - Reset the CPU by loading a null IDT and faulting.
 *
 * Used when the keyboard controller does not honour the reset pulse. The CPU
 * cannot service the resulting fault with an empty IDT, so it shuts down and
 * the firmware restarts it.
 */
[[noreturn]] void tripleFaultReset() {
    // A genuinely empty IDT: zero limit and zero base. 'lidt' has to read six
    // bytes from memory, so the descriptor cannot be the constant 0 - that
    // would load the real-mode IVT at address 0 and leave a usable (if
    // accidental) IDT in place, so the int3 below would be dispatched instead of
    // faulting into a triple fault.
    struct __attribute__((packed)) {
        uint16_t limit;
        uint32_t base;
    } nullIdt = {0, 0};

    asm volatile(
        "lidt (%0)\n\t"
        "int3\n\t" ::"r"(&nullIdt)
        : "memory");
    haltForever();
}

}  // namespace

[[noreturn]] void PowerRestart() {
    KDBG1("PowerRestart: pulsing the keyboard controller reset line\n");

    Port8Bit keyboardCommand(0x64);

    // A stray IRQ must not steal the reset sequence halfway through.
    asm volatile("cli");

    // Wait for the input buffer to drain so the pulse is not swallowed.
    // Bounded so an unresponsive controller cannot hang the machine here.
    for (volatile int i = 0; i < 1000000; i++) {
        if ((keyboardCommand.Read() & 0x02) == 0) break;
    }

    // 0xFE is "pulse CPU reset line" on the 8042.
    keyboardCommand.Write(0xFE);

    KDBG1("PowerRestart: controller did not reset, forcing a triple fault\n");
    tripleFaultReset();
}

[[noreturn]] void PowerShutdown() {
    KDBG1("PowerShutdown: requesting emulator power off\n");

    // Stop scheduling while requesting power-off.
    asm volatile("cli" ::: "memory");

    // Default PC firmware PM1 control ports. These are 16-bit ACPI sleep
    // registers, NOT debug-exit ports: SLP_EN is bit 13. A byte write of
    // 0x10 never sets it, leaving the VM running when we halt below.
    // VirtualBox uses sleep type 5; QEMU/Bochs use sleep type 0 for S5.
    // This is a VM fallback, not general ACPI table discovery for physical PCs.
    Port16Bit virtualBoxPower(0x4004);
    Port16Bit qemuPower(0x604);
    Port16Bit bochsPower(0xB004);
    virtualBoxPower.Write((5u << 10) | (1u << 13));
    qemuPower.Write(1u << 13);
    bochsPower.Write(1u << 13);

    // Retain isa-debug-exit for QEMU configurations without ACPI. Prefer
    // normal power-off above, which exits QEMU successfully instead of 33.
    Port8Bit debugExitQemu(0xF4);
    debugExitQemu.Write(0x10);

    KDBG1("PowerShutdown: no power-off device responded, halting the CPU\n");
    haltForever();
}
