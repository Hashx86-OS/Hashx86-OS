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

#include <core/Iguard.h>
#include <debug.h>

// --- Ring buffer configuration ---
#define SERIAL_BUFFER_SIZE 2048
static char serialBuffer[SERIAL_BUFFER_SIZE];
static volatile uint32_t readHead = 0;
static volatile uint32_t writeHead = 0;

void vprintf(const char* format, va_list args);

/**
 * initSerial() - Configure the serial port for 115200 baud logging.
 *
 * Programs the UART line control, FIFO and modem control registers on the
 * first COM port (0x3F8).
 */
void initSerial() {
    outb(0x3F8 + 1, 0x00);  // Disable interrupts.
    outb(0x3F8 + 3, 0x80);  // Enable DLAB (set baud rate divisor).

    // Set the baud rate to max speed (115200 baud).
    outb(0x3F8 + 0, 0x01);  // Set divisor to 1 (low byte).
    outb(0x3F8 + 1, 0x00);  // Set divisor to 1 (high byte).

    outb(0x3F8 + 3, 0x03);  // 8 bits, no parity, one stop bit.
    outb(0x3F8 + 2, 0xC7);  // Enable FIFO, clear it, 14-byte threshold.
    outb(0x3F8 + 4, 0x0B);  // Enable IRQs, RTS/DSR set.
}

/**
 * IsSerialReady() - Check whether the serial port can transmit.
 *
 * Return: True when the transmit-holding register is empty.
 */
bool IsSerialReady() {
    return (inb(0x3F8 + 5) & 0x20) != 0;
}

/**
 * FlushSerial() - Drain the ring buffer to the serial port.
 *
 * Writes buffered characters while the hardware is ready and data remains.
 */
void FlushSerial() {
    InterruptGuard guard;
    // Keep flushing while the hardware is ready and data remains.
    while (readHead != writeHead && IsSerialReady()) {
        char c = serialBuffer[readHead];
        outb(0x3F8, c);
        readHead = (readHead + 1) % SERIAL_BUFFER_SIZE;
    }
}

/**
 * SerialPush() - Queue one character and attempt an immediate flush.
 * @c: Character to transmit.
 */
void SerialPush(char c) {
    InterruptGuard guard;
    // Push to the buffer first.
    uint32_t nextHead = (writeHead + 1) % SERIAL_BUFFER_SIZE;
    if (nextHead != readHead) {
        serialBuffer[writeHead] = c;
        writeHead = nextHead;
    }

    // Attempt to flush immediately if the hardware is ready.
    // This keeps logs flowing even when the scheduler is slow.
    FlushSerial();
}

/**
 * writeSerial() - Transmit one character.
 * @c: Character to transmit.
 */
void writeSerial(char c) {
    SerialPush(c);
}

/**
 * SerialPrint() - Transmit a NUL-terminated string.
 * @str: String to transmit; a null pointer is printed as "(null)".
 */
void SerialPrint(const char* str) {
    if (str == nullptr) {
        const char* nullStr = "(null)";
        for (int i = 0; nullStr[i] != '\0'; i++) writeSerial(nullStr[i]);
        return;
    }
    for (size_t i = 0; str[i] != '\0'; i++) {
        writeSerial(str[i]);
    }
}

/**
 * printf() - Format and print a message to the serial port.
 * @format: printf-style format string.
 * @...: Arguments referenced by the format string.
 */
void printf(const char* format, ...) {
    InterruptGuard guard;  // Protects the formatting and buffer push.
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/**
 * vprintf() - Format and print a message using an explicit arg list.
 * @format: printf-style format string.
 * @args: Pre-initialized argument list for the format string.
 */
void vprintf(const char* format, va_list args) {
    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            if (format[i + 1] == '\0') {
                writeSerial('%');
                break;
            }
            i++;
            switch (format[i]) {
                case 'd': {
                    int num = va_arg(args, int);
                    char buffer[12];
                    int index = 11;
                    buffer[index] = '\0';

                    if (num == 0) {
                        buffer[--index] = '0';
                    } else if (num == -2147483648) {
                        const char* minStr = "-2147483648";
                        for (int j = 0; minStr[j] != '\0'; j++) writeSerial(minStr[j]);
                        break;
                    } else {
                        bool isNegative = (num < 0);
                        if (isNegative) num = -num;
                        while (num > 0) {
                            buffer[--index] = (num % 10) + '0';
                            num /= 10;
                        }
                        if (isNegative) buffer[--index] = '-';
                    }
                    for (int j = index; buffer[j] != '\0'; j++) writeSerial(buffer[j]);
                    break;
                }
                case 'u': {
                    uint32_t num = va_arg(args, uint32_t);
                    char buffer[11];
                    int index = 10;
                    buffer[index] = '\0';
                    if (num == 0) {
                        buffer[--index] = '0';
                    } else {
                        while (num > 0) {
                            buffer[--index] = (num % 10) + '0';
                            num /= 10;
                        }
                    }
                    for (int j = index; buffer[j] != '\0'; j++) writeSerial(buffer[j]);
                    break;
                }
                case 'x': {
                    uint32_t num = va_arg(args, uint32_t);
                    char buffer[9];
                    int index = 8;
                    buffer[index] = '\0';
                    const char* hexDigits = "0123456789ABCDEF";
                    if (num == 0) {
                        buffer[--index] = '0';
                    } else {
                        while (num > 0) {
                            buffer[--index] = hexDigits[num % 16];
                            num /= 16;
                        }
                    }
                    for (int j = index; buffer[j] != '\0'; j++) writeSerial(buffer[j]);
                    break;
                }
                case 's': {
                    const char* str = va_arg(args, const char*);
                    if (str == nullptr) str = "(null)";
                    for (int j = 0; str[j] != '\0'; j++) writeSerial(str[j]);
                    break;
                }
                default:
                    break;
            }
        } else {
            writeSerial(format[i]);
        }
    }
}

/**
 * DebugPrintf() - Print a tagged log line terminated by a newline.
 * @tag: Tag identifying the component.
 * @format: printf-style format string.
 * @...: Arguments referenced by the format string.
 */
void DebugPrintf(const char* tag, const char* format, ...) {
    InterruptGuard guard;
    // This runs extremely fast (microseconds) because it only writes to RAM.
    printf("%s:", tag);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

/**
 * Printf() - Print a tagged message without a trailing newline.
 * @tag: Tag identifying the component.
 * @format: printf-style format string.
 * @...: Arguments referenced by the format string.
 */
void Printf(const char* tag, const char* format, ...) {
    InterruptGuard guard;
    printf("%s:", tag);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
