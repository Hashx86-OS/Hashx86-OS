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

#ifndef PORT_H
#define PORT_H

#include <types.h>  // For type definitions such as uint16_t, uint8_t, etc.

/**
 * inb() - Read a byte from an I/O port.
 * @portNumber: The I/O port number.
 *
 * Return: The byte read from the port.
 */
static inline uint8_t inb(uint16_t portNumber) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(portNumber));
    return result;
}

/**
 * outb() - Write a byte to an I/O port.
 * @portNumber: The I/O port number.
 * @value: The byte to write.
 */
static inline void outb(uint16_t portNumber, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(portNumber));
}

/**
 * inw() - Read a word (16 bits) from an I/O port.
 * @port: The I/O port number.
 *
 * Return: The word read from the port.
 */
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * outw() - Write a word (16 bits) to an I/O port.
 * @port: The I/O port number.
 * @val: The word to write.
 */
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * insw() - Read a run of words from an I/O port into memory.
 * @portNumber: The I/O port number.
 * @buffer: Destination buffer.
 * @count: Number of words (16-bit) to read.
 */
static inline void insw(uint16_t portNumber, void* buffer, uint32_t count) {
    asm volatile("rep insw" : "+D"(buffer), "+c"(count) : "d"(portNumber) : "memory");
}

/**
 * outsw() - Write a run of words from memory to an I/O port.
 * @portNumber: The I/O port number.
 * @buffer: Source buffer.
 * @count: Number of words (16-bit) to write.
 */
static inline void outsw(uint16_t portNumber, void* buffer, uint32_t count) {
    asm volatile("rep outsw" : "+S"(buffer), "+c"(count) : "d"(portNumber) : "memory");
}

/**
 * outl() - Write a dword (32 bits) to an I/O port.
 * @port: The I/O port number.
 * @val: The dword to write.
 */
static inline void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * inl() - Read a dword (32 bits) from an I/O port.
 * @port: The I/O port number.
 *
 * Return: The dword read from the port.
 */
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * class Port - Base class representing a generic I/O port.
 *
 * Provides the foundation for accessing hardware ports at different data
 * widths. Holds the port number shared by all derived classes.
 */
class Port {
protected:
    uint16_t portNumber;  // Port number associated with the I/O operation.

    /**
     * Port() - Initialize the port number.
     * @portNumber: The I/O port number.
     *
     * Protected: the class is abstract and not meant for direct instantiation.
     */
    Port(uint16_t portNumber);

    /**
     * ~Port() - Destroy the port.
     *
     * Protected virtual destructor; ensures proper cleanup for derived classes.
     */
    ~Port();

public:
    uint16_t getPortNumber() {
        return portNumber;
    }
};

/**
 * class Port8Bit - An 8-bit I/O port.
 *
 * Allows for reading and writing one byte (8 bits) of data.
 */
class Port8Bit : public Port {
public:
    /**
     * Port8Bit() - Initialize an 8-bit port.
     * @portNumber: The I/O port number.
     */
    Port8Bit(uint16_t portNumber);

    /**
     * ~Port8Bit() - Destroy the 8-bit port.
     */
    ~Port8Bit();

    /**
     * Write() - Write a byte (8 bits) to the port.
     * @data: The data to write.
     */
    virtual void Write(uint8_t data);

    /**
     * Read() - Read a byte (8 bits) from the port.
     *
     * Return: The data read from the port.
     */
    virtual uint8_t Read();
};

/**
 * class Port8BitSlow - An 8-bit I/O port with a slower write operation.
 *
 * Introduces an artificial delay to account for slower hardware requirements.
 */
class Port8BitSlow : public Port8Bit {
public:
    /**
     * Port8BitSlow() - Initialize a slow 8-bit port.
     * @portNumber: The I/O port number.
     */
    Port8BitSlow(uint16_t portNumber);

    /**
     * ~Port8BitSlow() - Destroy the slow 8-bit port.
     */
    ~Port8BitSlow();

    /**
     * Write() - Write a byte to the port with a delay.
     * @data: The data to write.
     */
    virtual void Write(uint8_t data);
};

/**
 * class Port16Bit - A 16-bit I/O port.
 *
 * Allows for reading and writing two bytes (16 bits) of data.
 */
class Port16Bit : public Port {
public:
    /**
     * Port16Bit() - Initialize a 16-bit port.
     * @portNumber: The I/O port number.
     */
    Port16Bit(uint16_t portNumber);

    /**
     * ~Port16Bit() - Destroy the 16-bit port.
     */
    ~Port16Bit();

    /**
     * Write() - Write a 16-bit word to the port.
     * @data: The data to write.
     */
    virtual void Write(uint16_t data);

    /**
     * Read() - Read a 16-bit word from the port.
     *
     * Return: The word read from the port.
     */
    virtual uint16_t Read();
};

/**
 * class Port32Bit - A 32-bit I/O port.
 *
 * Allows for reading and writing four bytes (32 bits) of data.
 */
class Port32Bit : public Port {
public:
    /**
     * Port32Bit() - Initialize a 32-bit port.
     * @portNumber: The I/O port number.
     */
    Port32Bit(uint16_t portNumber);

    /**
     * ~Port32Bit() - Destroy the 32-bit port.
     */
    ~Port32Bit();

    /**
     * Write() - Write a 32-bit dword to the port.
     * @data: The data to write.
     */
    virtual void Write(uint32_t data);

    /**
     * Read() - Read a 32-bit dword from the port.
     *
     * Return: The dword read from the port.
     */
    virtual uint32_t Read();
};

#endif  // PORT_H
