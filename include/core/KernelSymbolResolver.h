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

#ifndef KERNELSYMBOLRESOLVER_H
#define KERNELSYMBOLRESOLVER_H

#include <core/filesystem/FileSystem.h>
#include <core/memory.h>
#include <debug.h>
#include <string.h>
#include <types.h>

/**
 * struct StackFrame - A frame on the x86 call stack during stack unwinding.
 * @ebp: Pointer to the previous stack frame.
 * @eip: The instruction pointer (return address).
 */
struct StackFrame {
    struct StackFrame* ebp;  // Pointer to the previous stack frame.
    uint32_t eip;            // The instruction pointer (return address).
};

/**
 * struct SymbolEntry - A single kernel address-to-name mapping.
 * @addr: Address of the symbol.
 * @name: Name of the symbol.
 */
struct SymbolEntry {
    uint32_t addr;
    char* name;
};

/**
 * class KernelSymbolTable - Resolve kernel addresses to symbol names.
 *
 * Loads the kernel symbol map from disk and provides address lookup and
 * stack-trace printing for the crash reporter.
 */
class KernelSymbolTable {
public:
    /**
     * Load() - Load and parse a kernel symbol map file.
     * @fs: Filesystem to read the map from.
     * @path: Path of the "addr <spaces> name" listing, one symbol per line.
     *
     * Frees any previously loaded map, reads the whole file into a buffer, and
     * builds a sorted-by-line symbol index. The buffer is kept resident so
     * Lookup() can return pointers into it.
     */
    static void Load(FileSystem* fs, const char* path);

    /**
     * Lookup() - Resolve an address to its nearest symbol.
     * @address: Address to resolve.
     * @offset: Optional output for the byte offset from the symbol's start.
     *
     * Finds the closest loaded symbol whose address is at or below @address. A
     * result whose distance exceeds 1 MiB is treated as a mismatch.
     *
     * Return: Pointer to the symbol name, or NULL if nothing matches.
     */
    static const char* Lookup(uint32_t address, uint32_t* offset);

    /**
     * PrintStackTrace() - Print the current stack trace.
     * @maxFrames: Maximum number of frames to walk before giving up.
     *
     * Walks the EBP-linked frames starting from the current stack pointer,
     * resolving each return address through Lookup(). The walk stops early on
     * invalid pointers or when EBP stops strictly increasing (cycle detection).
     *
     * Context: Called from the crash reporter; safe with IRQs either way.
     */
    static void PrintStackTrace(unsigned int maxFrames);
};

#endif  // KERNELSYMBOLRESOLVER_H
