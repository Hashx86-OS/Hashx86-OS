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

#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>

/* Forward declarations of the kernel's primary subsystems. */
class AudioMixer;
class GraphicsDriver;
class AudioDriver;
class Paging;
class FontManager;
class InterruptManager;
class Scheduler;
class SyscallHandler;
class DriverManager;
class FileSystem;
class ELFLoader;

/* Kernel utility buffer used by the console/debug synthesizers. */
extern char Buffer[32];

/* Monotonic tick counter driven by the PIT handler. */
extern uint64_t timerTicks;

/* Runtime configuration flags. */
extern bool g_scheduler_preserves_fpu;
extern bool g_stop_gui_rendering;
extern int g_gui_owner_pid;

/* Singleton subsystem instances. */
extern Paging* g_paging;
extern FontManager* g_fManager;
extern InterruptManager* g_interrupts;
extern Scheduler* g_scheduler;
extern SyscallHandler* g_sysCalls;
extern DriverManager* g_driverManager;
extern AudioMixer* g_AudioMixer;
extern GraphicsDriver* g_GraphicsDriver;
extern AudioDriver* g_AudioDriver;
extern FileSystem* g_bootPartition;
extern ELFLoader* g_elfLoader;

#endif  // GLOBALS_H
