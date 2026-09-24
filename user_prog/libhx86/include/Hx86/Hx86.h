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

#ifndef Hx86_H
#define Hx86_H

#include <Hx86/Hgui/eventHandler.h>
#include <Hx86/Hsyscalls/syscalls.h>
#include <Hx86/appmeta.h>
#include <Hx86/debug.h>
#include <Hx86/globals.h>
#include <Hx86/memory.h>

/** init_sys() - Initialize the process heap and auto-start a terminal host.
 * @arg: Pointer to the ProgramArguments passed by the loader.
 */
void init_sys(void* arg);

/** init_cli() - Create or attach a terminal host for a CLI application.
 * Return: true on success, false if the terminal could not be set up.
 */
bool init_cli();

/** cli_append_output() - Append text to the CLI output buffer.
 * @text: NUL-terminated text to append.
 * Return: true on success, false if no buffer or view is available.
 */
bool cli_append_output(const char* text);

/** syscall_sleep() - Sleep for the given number of milliseconds.
 * @ms: Sleep duration in milliseconds.
 */
void syscall_sleep(uint32_t ms);

#endif  // Hx86_H
