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

#include <core/ports.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <types.h>

/**
 * initSerial() - Configure the serial port for 115200 baud logging.
 */
void initSerial();
/**
 * SerialPrint() - Transmit a NUL-terminated string.
 * @str: String to transmit.
 */
void SerialPrint(const char* str);
/**
 * writeSerial() - Transmit one character.
 * @c: Character to transmit.
 */
void writeSerial(char c);

#ifndef KDBG_ENABLE
#define KDBG_ENABLE 0
#endif

#ifndef KDBG_LEVEL
#define KDBG_LEVEL 0
#endif

#ifndef KDBG_COMPONENT
#define KDBG_COMPONENT "GEN"
#endif

#define KDBG__EMIT(level, component, format, ...)              \
    do {                                                       \
        printf("[%s] " format "\n", component, ##__VA_ARGS__); \
    } while (0)

#define KDBGN__EMIT(level, component, format, ...)           \
    do {                                                     \
        printf("[%s] " format "", component, ##__VA_ARGS__); \
    } while (0)

#if KDBG_ENABLE && (KDBG_LEVEL >= 1)
#define KDBG_L1(component, format, ...) KDBG__EMIT(1, component, format, ##__VA_ARGS__)
#define KDBG1(format, ...) KDBG__EMIT(1, KDBG_COMPONENT, format, ##__VA_ARGS__)
#else
#define KDBG_L1(component, format, ...) ((void)0)
#define KDBG1(format, ...) ((void)0)
#endif

#if KDBG_ENABLE && (KDBG_LEVEL >= 1)
#define KDBG_L1N(component, format, ...) KDBGN__EMIT(1, component, format, ##__VA_ARGS__)
#define KDBG1N(format, ...) KDBGN__EMIT(1, KDBG_COMPONENT, format, ##__VA_ARGS__)
#else
#define KDBG_L1N(component, format, ...) ((void)0)
#define KDBG1N(format, ...) ((void)0)
#endif

#if KDBG_ENABLE && (KDBG_LEVEL >= 2)
#define KDBG_L2(component, format, ...) KDBG__EMIT(2, component, format, ##__VA_ARGS__)
#define KDBG2(format, ...) KDBG__EMIT(2, KDBG_COMPONENT, format, ##__VA_ARGS__)
#else
#define KDBG_L2(component, format, ...) ((void)0)
#define KDBG2(format, ...) ((void)0)
#endif

#if KDBG_ENABLE && (KDBG_LEVEL >= 3)
#define KDBG_L3(component, format, ...) KDBG__EMIT(3, component, format, ##__VA_ARGS__)
#define KDBG3(format, ...) KDBG__EMIT(3, KDBG_COMPONENT, format, ##__VA_ARGS__)
#else
#define KDBG_L3(component, format, ...) ((void)0)
#define KDBG3(format, ...) ((void)0)
#endif

/**
 * printf() - Format and print a message to the serial port.
 * @format: printf-style format string.
 * @...: Arguments referenced by the format string.
 */
void printf(const char* format, ...);

/**
 * DebugPrintf() - Print a tagged log line terminated by a newline.
 * @tag: Tag identifying the component.
 * @format: printf-style format string.
 * @...: Arguments referenced by the format string.
 */
void DebugPrintf(const char* tag, const char* format, ...);

/**
 * Printf() - Print a tagged message without a trailing newline.
 * @tag: Tag identifying the component.
 * @format: printf-style format string.
 * @...: Arguments referenced by the format string.
 */
void Printf(const char* tag, const char* format, ...);

#define HALT(msg)            \
    printf(msg);             \
    do {                     \
        asm volatile("hlt"); \
    } while (1)

#endif  // DEBUG_H
