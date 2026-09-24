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

#define KDBG_COMPONENT "KSYMT"
#include <core/drivers/SymbolTable.h>

KernelSymbol SymbolTable::symbols[1024];
int SymbolTable::count = 0;

/**
 * SymbolTable::Register() - Store a kernel export in the symbol table.
 * @name: Export name; must be a string literal that outlives the table.
 * @addr: Kernel address of the export.
 */
void SymbolTable::Register(const char* name, uint32_t addr) {
    if (count >= 1024) {
        KDBG1("Error: Kernel Symbol Table Full!\n");
        return;
    }
    symbols[count].name = name;
    symbols[count].address = addr;
    count++;
}

/**
 * SymbolTable::Lookup() - Resolve an export name to its kernel address.
 * @name: Export name to search for.
 *
 * Return: The registered address, or 0 when unknown.
 */
uint32_t SymbolTable::Lookup(const char* name) {
    if (!name) return 0;
    for (int i = 0; i < count; i++) {
        if (!symbols[i].name) continue;
        if (strcmp(symbols[i].name, name) == 0) {
            return symbols[i].address;
        }
    }
    return 0;  // Not found.
}
