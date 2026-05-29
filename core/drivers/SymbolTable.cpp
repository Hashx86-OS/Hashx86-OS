/**
 * @file        SymbolTable.cpp
 * @brief       Kernel Symbol Table Implementation
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "KSYMT"
#include <core/drivers/SymbolTable.h>

KernelSymbol SymbolTable::symbols[1024];
int SymbolTable::count = 0;

// name must outlive the table — callers MUST pass string literals only.
void SymbolTable::Register(const char* name, uint32_t addr) {
    if (count >= 1024) {
        KDBG1("Error: Kernel Symbol Table Full!\n");
        return;
    }
    symbols[count].name = name;
    symbols[count].address = addr;
    count++;
}

uint32_t SymbolTable::Lookup(const char* name) {
    if (!name) return 0;
    for (int i = 0; i < count; i++) {
        if (!symbols[i].name) continue;
        if (strcmp(symbols[i].name, name) == 0) {
            return symbols[i].address;
        }
    }
    return 0;  // Not Found
}
