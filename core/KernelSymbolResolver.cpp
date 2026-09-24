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

#define KDBG_COMPONENT "K.SYMBOL"
#include <core/KernelSymbolResolver.h>
#include <core/filesystem/File.h>

static char* fileBuffer = nullptr;
static SymbolEntry* symbolIndex = nullptr;
static uint32_t symbolCount = 0;

/**
 * KernelSymbolTable::Load() - Load and parse a kernel symbol map file.
 * @fs: Filesystem to read the map from.
 * @path: Path of the "addr <spaces> name" listing, one symbol per line.
 *
 * Frees any previously loaded map, reads the whole file into a kmalloc'd
 * buffer, and builds a sorted-by-line symbol index. The buffer is kept
 * resident so Lookup() can return pointers into it.
 *
 * Context: Call once during boot, before any crash can be reported.
 */
void KernelSymbolTable::Load(FileSystem* fs, const char* path) {
    if (!fs) return;
    // Free previous data before loading new.
    if (fileBuffer) {
        kfree(fileBuffer);
        fileBuffer = nullptr;
    }
    if (symbolIndex) {
        kfree(symbolIndex);
        symbolIndex = nullptr;
    }
    symbolCount = 0;
    KDBG1("Loading map file: %s", path);
    File* file = fs->Open(path);
    if (!file) {
        KDBG1("Failed to open %s", path);
        return;
    }

    if (file->size == 0) {
        KDBG1("Map file is empty!");
        file->Close();
        delete file;
        return;
    }

    // Load the whole file into memory.
    fileBuffer = (char*)kmalloc(file->size + 1);
    if (!fileBuffer) {
        KDBG1("Failed to allocate file buffer for map file!");
        file->Close();
        delete file;
        return;
    }
    int bytesRead = file->Read((uint8_t*)fileBuffer, file->size);
    if (bytesRead != file->size) {
        KDBG1("Failed to read map file: expected %d bytes, got %d", file->size, bytesRead);
        kfree(fileBuffer);
        fileBuffer = nullptr;
        file->Close();
        delete file;
        return;
    }
    fileBuffer[file->size] = 0;  // Null-terminate the buffer.
    file->Close();

    // Allocate the symbol index array.
    uint32_t maxEntries = file->size / 20;
    symbolIndex = (SymbolEntry*)kmalloc(maxEntries * sizeof(SymbolEntry));
    if (!symbolIndex) {
        KDBG1("Failed to allocate symbol index array!");
        kfree(fileBuffer);
        fileBuffer = nullptr;
        delete file;
        return;
    }
    symbolCount = 0;
    delete file;

    // Parse the file line by line.
    char* cursor = fileBuffer;
    while (*cursor) {
        // Skip empty lines or leading whitespace.
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r') {
            cursor++;
            if (*cursor == 0) break;
        }
        if (*cursor == 0) break;

        // Pattern: "0x00100000    functionName"
        if (cursor[0] == '0' && cursor[1] == 'x') {
            uint32_t addr = HexStrToInt(cursor);

            // Skip past the address just read.
            while (*cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r' &&
                   *cursor != 0)
                cursor++;

            // Skip the whitespace between address and name.
            while (*cursor == ' ' || *cursor == '\t') cursor++;

            // EOL check: a name must follow on this line.
            if (*cursor != '\n' && *cursor != '\r' && *cursor != 0) {
                // Store the symbol.
                if (symbolCount >= maxEntries) {
                    KDBG1("Warning: Too many symbols, truncating");
                    break;
                }
                symbolIndex[symbolCount].addr = addr;
                symbolIndex[symbolCount].name = cursor;
                symbolCount++;

                // Fast-forward to the end of line to terminate the string.
                while (*cursor != '\n' && *cursor != '\r' && *cursor != 0) cursor++;

                // Replace the newline with NULL to terminate the name string.
                if (*cursor != 0) {
                    *cursor = 0;
                    cursor++;  // Move to the next char for the next iteration.
                }
                continue;
            }
        }

        // Skip lines that did not start with 0x.
        while (*cursor != '\n' && *cursor != '\r' && *cursor != 0) cursor++;
    }

    KDBG1("Parsed %d functions.", (int32_t)symbolCount);
}

/**
 * KernelSymbolTable::Lookup() - Resolve an address to its nearest symbol.
 * @eip: Address to resolve.
 * @offset: Optional output for the byte offset from the symbol's start.
 *
 * Finds the closest loaded symbol whose address is at or below @eip. A
 * result whose distance exceeds 1 MiB is treated as a mismatch.
 *
 * Return: Pointer to the symbol name, or NULL if nothing matches.
 */
const char* KernelSymbolTable::Lookup(uint32_t eip, uint32_t* offset) {
    if (symbolCount == 0) return nullptr;

    uint32_t bestAddr = 0;
    const char* bestName = nullptr;

    // Find the closest symbol at or below EIP.
    for (uint32_t i = 0; i < symbolCount; i++) {
        uint32_t addr = symbolIndex[i].addr;

        if (addr <= eip) {
            if (addr >= bestAddr) {
                bestAddr = addr;
                bestName = symbolIndex[i].name;
            }
        }
    }

    if (bestName) {
        uint32_t localOffset = eip - bestAddr;
        // Sanity check: a huge offset (>1 MiB) likely means a mismatch.
        if (localOffset > 0x100000) return nullptr;
        if (offset) *offset = localOffset;
        return bestName;
    }
    return nullptr;
}

/**
 * KernelSymbolTable::PrintStackTrace() - Print the current stack trace.
 * @maxFrames: Maximum number of frames to walk before giving up.
 *
 * Walks the EBP-linked frames starting from the current stack pointer,
 * resolving each return address through Lookup(). The walk stops early on
 * invalid pointers, addresses outside kernel memory, or when EBP stops
 * strictly increasing (cycle detection).
 *
 * Context: Called from the crash reporter; safe with IRQs either way.
 */
void KernelSymbolTable::PrintStackTrace(unsigned int maxFrames) {
    StackFrame* stack;

    // Read the current EBP register.
    asm volatile("mov %%ebp, %0" : "=r"(stack));

    KDBG1("[ Stack Trace ]");

    // Cycle detection: older x86 frames live at higher addresses, so track
    // the lowest valid EBP; if EBP ever fails to strictly increase we have a
    // cycle.
    uint32_t prev_ebp = 0;

    for (unsigned int i = 0; i < maxFrames; ++i) {
        // Stop if the stack pointer is null or invalid.
        if (!stack) break;

        // Stop if EBP is outside kernel-mapped memory (0 - 256 MiB).
        // Following user-mode EBP pointers after switching to
        // KernelPageDirectory would page fault and loop forever, since
        // activeInstance is 0 in that window.
        if ((uint32_t)stack < 0x1000 || (uint32_t)stack >= 0x10000000) break;

        // EBP must strictly increase as we walk up the stack.
        if ((uint32_t)stack <= prev_ebp) break;
        prev_ebp = (uint32_t)stack;

        if (i > 2) {
            // Resolve and print this frame's symbol, if any.
            uint32_t offset = 0;
            const char* name = Lookup(stack->eip, &offset);
            if (name)
                KDBG1(" 0x%x <%s+%d>", stack->eip, name, (int32_t)offset);
            else
                KDBG1(" 0x%x", stack->eip);
        }

        // Move to the previous frame (walk up the stack).
        stack = stack->ebp;
    }

    KDBG1("[ End of Stack Trace ]\n");
}
