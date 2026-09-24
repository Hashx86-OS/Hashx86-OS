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

#ifndef DEBUG_H
#define DEBUG_H

#include <Hx86/Hsyscalls/syscalls.h>
#include <Hx86/types.h>
#include <stdarg.h>

/** DEBUG_LOG(format, ...) - Log a debug message with a consistent prefix.
 * @format: printf-style format string.
 * @...: Additional arguments for the format string.
 *
 * Logs a message prefixed with "[DEBUG]".
 */
#define DEBUG_LOG(format, ...) DebugPrintf("[DEBUG]", format, ##__VA_ARGS__)

/** PRINT(tag, format, ...) - Print a tagged message from a module.
 * @tag: String identifying the module or context of the message.
 * @format: printf-style format string.
 * @...: Additional arguments for the format string.
 */
#define PRINT(tag, format, ...) Printf(tag, format, ##__VA_ARGS__)

// printf() writes to stdout for the serial monitor.
void printf(const char* format, ...);

// DebugPrintf() prints a message prefixed with a debug tag.
void DebugPrintf(const char* tag, const char* format, ...);

// Printf() prints a message prefixed with a module tag.
void Printf(const char* tag, const char* format, ...);

#endif  // DEBUG_H
