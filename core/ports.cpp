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

#include <core/ports.h>

Port::Port(uint16_t portNumber) {
    this->portNumber = portNumber;
}

Port::~Port() {}

Port8Bit::Port8Bit(uint16_t portNumber) : Port(portNumber) {}

Port8Bit::~Port8Bit() {}

void Port8Bit::Write(uint8_t data) {
    asm volatile("outb %0, %1" : : "a"(data), "Nd"(portNumber) : "memory");
}

uint8_t Port8Bit::Read() {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(portNumber) : "memory");
    return result;
}

Port8BitSlow::Port8BitSlow(uint16_t portNumber) : Port8Bit(portNumber) {}

Port8BitSlow::~Port8BitSlow() {}

void Port8BitSlow::Write(uint8_t data) {
    asm volatile("outb %0, %1\njmp 1f\n1: jmp 1f\n1:" : : "a"(data), "Nd"(portNumber) : "memory");
}

Port16Bit::Port16Bit(uint16_t portNumber) : Port(portNumber) {}

Port16Bit::~Port16Bit() {}

void Port16Bit::Write(uint16_t data) {
    asm volatile("outw %0, %1" : : "a"(data), "Nd"(portNumber) : "memory");
}

uint16_t Port16Bit::Read() {
    uint16_t result;
    asm volatile("inw %1, %0" : "=a"(result) : "Nd"(portNumber) : "memory");
    return result;
}

Port32Bit::Port32Bit(uint16_t portNumber) : Port(portNumber) {}

Port32Bit::~Port32Bit() {}

void Port32Bit::Write(uint32_t data) {
    asm volatile("outl %0, %1" : : "a"(data), "Nd"(portNumber) : "memory");
}

uint32_t Port32Bit::Read() {
    uint32_t result;
    asm volatile("inl %1, %0" : "=a"(result) : "Nd"(portNumber) : "memory");
    return result;
}
