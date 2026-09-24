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

#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <debug.h>
#include <string.h>
#include <types.h>

/**
 * struct KernelSymbol - A name-to-address mapping for an exported symbol.
 * @name: NUL-terminated symbol name.
 * @address: Virtual address of the symbol.
 */
struct KernelSymbol {
    const char* name;
    uint32_t address;
};

#define EXPORT_SYMBOL_ASM(mangled_name)                          \
    do {                                                         \
        uint32_t addr;                                           \
        asm volatile("movl $" mangled_name ", %0" : "=r"(addr)); \
        SymbolTable::Register(mangled_name, addr);               \
    } while (0)

/**
 * class SymbolTable - Fixed-size registry of kernel symbols for drivers.
 */
class SymbolTable {
public:
    /**
     * Register() - Record a kernel function name -> address mapping.
     * @name: Symbol name.
     * @addr: Symbol address.
     */
    static void Register(const char* name, uint32_t addr);

    /**
     * Lookup() - Find a function address by name.
     * @name: Symbol name.
     *
     * Return: The symbol address, or 0 when not found.
     */
    static uint32_t Lookup(const char* name);

// Helper macro for the kernel to export a symbol by name.
#define EXPORT_SYMBOL(func) SymbolTable::Register(#func, (uint32_t) & func)

private:
    static KernelSymbol symbols[1024];
    static int count;
};

#endif
