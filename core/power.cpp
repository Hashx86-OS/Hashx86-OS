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
    asm volatile(
        "lidt (%0)\n\t"
        "int3\n\t" ::"r"(0));
    haltForever();
}

}  // namespace

void PowerRestart() {
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

void PowerShutdown() {
    KDBG1("PowerShutdown: requesting emulator power off\n");

    // Emulators expose a debug-exit port that terminates the VM on any write.
    // This project's run targets attach QEMU's isa-debug-exit, which defaults
    // to 0xF4; 0x604 and 0xB004 are the other common choices. On real hardware
    // all three are unmapped, the writes are discarded, and we fall through to
    // halting.
    //
    // The value doubles as QEMU's exit status as (value << 1) | 1, so 0x10
    // exits 33 and a shutdown is distinguishable from a crash exit.
    Port8Bit debugExitQemu(0xF4);
    Port8Bit debugExitQemuAlt(0x604);
    Port8Bit debugExitBochs(0xB004);

    debugExitQemu.Write(0x10);
    debugExitQemuAlt.Write(0x10);
    debugExitBochs.Write(0x10);

    KDBG1("PowerShutdown: no power-off device responded, halting the CPU\n");
    haltForever();
}
