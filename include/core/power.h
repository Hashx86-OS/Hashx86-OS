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

#pragma once

/**
 * Force-level power control.
 *
 * There is no ACPI interpreter and no orderly shutdown path yet, so both
 * entries here are deliberately blunt: they stop or reset the CPU immediately
 * and never return. Anything that needs to survive a restart has to be on disk
 * before calling them.
 */

/**
 * PowerRestart() - Reset the machine.
 *
 * Pulses the 8042 keyboard controller's CPU-reset line, which every emulator
 * and real PC since the AT honours, then falls back to a deliberate triple
 * fault if the controller never resets the CPU. Does not return.
 */
[[noreturn]] void PowerRestart();

/**
 * PowerShutdown() - Stop the machine.
 *
 * Requests S5 through the default VirtualBox/QEMU/Bochs ACPI control ports,
 * then tries QEMU's optional debug-exit device. If no device responds, halts
 * with interrupts disabled. General physical-PC shutdown still requires ACPI
 * table discovery; these fixed VM ports do not provide that. Does not return.
 */
[[noreturn]] void PowerShutdown();
