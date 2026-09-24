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

#include <core/globals.h>

char Buffer[32];
uint64_t timerTicks = 0;
bool g_scheduler_preserves_fpu = false;
bool g_stop_gui_rendering = false;
int g_gui_owner_pid = -1;

Paging* g_paging = nullptr;
FontManager* g_fManager = nullptr;
InterruptManager* g_interrupts = nullptr;
Scheduler* g_scheduler = nullptr;
SyscallHandler* g_sysCalls = nullptr;
DriverManager* g_driverManager = nullptr;
AudioMixer* g_AudioMixer = nullptr;
GraphicsDriver* g_GraphicsDriver = nullptr;
AudioDriver* g_AudioDriver = nullptr;
FileSystem* g_bootPartition = nullptr;
ELFLoader* g_elfLoader = nullptr;
