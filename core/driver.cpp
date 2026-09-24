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

#define KDBG_COMPONENT "DRIVER.MGR"
#include <core/driver.h>
#include <core/pmm.h>

extern "C" void pci_enable_bus_master(uint16_t vendor, uint16_t device);
extern "C" uint32_t pci_find_bar0(uint16_t vendor, uint16_t device);
extern void vprintf(const char* format, va_list args);

/**
 * drvPrintf() - Print a driver log line prefixed with the component tag.
 * @format: printf-style format string.
 */
void drvPrintf(const char* format, ...) {
#if KDBG_ENABLE && (KDBG_LEVEL >= 1)
    printf("[%s] ", KDBG_COMPONENT);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#else
    (void)format;
#endif
}

/**
 * DriverManager::DriverManager() - Construct the driver manager.
 *
 * Initializes the driver registry to empty and exports the kernel symbols
 * that loadable driver modules link against (printf, memory allocators,
 * new/delete operators, PCI/paging and graphics entry points).
 */
DriverManager::DriverManager() {
    KDBG1("Loading...");
    numDrivers = 0;

    // Export the kernel printf and its mangled alias.
    void (*printf_ptr)(const char*, ...) = drvPrintf;
    SymbolTable::Register("printf", (uint32_t)printf_ptr);
    SymbolTable::Register("_Z6printfPKcz", (uint32_t)printf_ptr);

    // Export the kernel memory allocators.
    EXPORT_SYMBOL(kmalloc);
    EXPORT_SYMBOL(kfree);

    // Export the C++ new/delete operators.
    void* (*new_ptr)(size_t) = operator new;
    void (*delete_ptr)(void*) = operator delete;
    void (*delete_sized_ptr)(void*, size_t) = operator delete;
    // Export the C++ array new/delete operators.
    void* (*new_array_ptr)(size_t) = operator new[];
    void (*delete_array_ptr)(void*) = operator delete[];

    SymbolTable::Register("_Znaj", (uint32_t)new_array_ptr);
    SymbolTable::Register("_ZdaPv", (uint32_t)delete_array_ptr);
    SymbolTable::Register("_Znwj", (uint32_t)new_ptr);
    SymbolTable::Register("_ZdlPv", (uint32_t)delete_ptr);
    SymbolTable::Register("_ZdlPvj", (uint32_t)delete_sized_ptr);

    SymbolTable::Register("__cxa_pure_virtual", (uint32_t)__cxa_pure_virtual);

    // Export PCI enumeration and bus-master helpers.
    SymbolTable::Register("pci_enable_bus_master", (uint32_t)pci_enable_bus_master);
    SymbolTable::Register("pci_find_bar0", (uint32_t)pci_find_bar0);

    SymbolTable::Register("memcpy", (uint32_t)memcpy);
    SymbolTable::Register("memset", (uint32_t)memset);

    // Export PMM functions for driver DMA allocations (mangled C++ names).
    SymbolTable::Register("_Z19pmm_alloc_block_lowj", (uint32_t)(void*)pmm_alloc_block_low);
    SymbolTable::Register("_Z14pmm_free_blockPv", (uint32_t)(void*)pmm_free_block);
    SymbolTable::Register("_Z16pmm_alloc_blocksj", (uint32_t)(void*)pmm_alloc_blocks);
    SymbolTable::Register("_Z15pmm_free_blocksPvj", (uint32_t)(void*)pmm_free_blocks);

    EXPORT_SYMBOL_ASM("_Z7kmallocj");

    // Export the interrupt handler and manager entry points.
    EXPORT_SYMBOL_ASM("_ZN16InterruptHandlerD2Ev");
    EXPORT_SYMBOL_ASM("_ZN16InterruptManager14activeInstanceE");
    EXPORT_SYMBOL_ASM("_ZN16InterruptHandlerC2EhP16InterruptManager");

    // Export the PCI controller methods.
    EXPORT_SYMBOL_ASM("_ZN41PeripheralComponentInterconnectControllerC1Ev");  // Constructor.
    EXPORT_SYMBOL_ASM("_ZN41PeripheralComponentInterconnectControllerD1Ev");  // Destructor.
    EXPORT_SYMBOL_ASM("_ZN41PeripheralComponentInterconnectController18FindHardwareDeviceEtt");
    EXPORT_SYMBOL_ASM("_ZN41PeripheralComponentInterconnectController4ReadEtttj");  // Read.
    EXPORT_SYMBOL_ASM("_ZN41PeripheralComponentInterconnectController5WriteEtttjj");
    EXPORT_SYMBOL_ASM(
        "_ZN41PeripheralComponentInterconnectController22GetBaseAddressRegisterEtttt");

    // Export paging symbols.
    EXPORT_SYMBOL_ASM("g_paging");
    EXPORT_SYMBOL_ASM("_ZN6Paging7MapPageEPjjjj");            // Paging::MapPage.
    EXPORT_SYMBOL_ASM("_ZN6Paging18GetPhysicalAddressEPjj");  // Paging::GetPhysicalAddress.

    // Export GraphicsDriver methods.
    EXPORT_SYMBOL_ASM("_ZN14GraphicsDriverC2EjjjPj");  // Constructor.
    EXPORT_SYMBOL_ASM("_ZN14GraphicsDriverD2Ev");      // Destructor.
    EXPORT_SYMBOL_ASM("_ZN14GraphicsDriver5FlushEv");
    EXPORT_SYMBOL_ASM("_ZN14GraphicsDriver8PutPixelEiij");  // The (int, int, uint32) overload.
}

/**
 * DriverManager::AddDriver() - Register a driver with the manager.
 * @drv: Driver instance to register.
 *
 * Stores the driver in the internal array, up to a hard limit of 255
 * entries. Null pointers and a full table are rejected.
 */
void DriverManager::AddDriver(Driver* drv) {
    if (drv == nullptr) {
        KDBG1("AddDriver failed: null driver");
        return;
    }
    if (numDrivers >= 255) {
        KDBG1("AddDriver failed: driver table full");
        return;
    }
    drivers[numDrivers] = drv;  // Add the driver to the array.
    numDrivers++;               // Increment the driver count.
}

/**
 * DriverManager::ActivateAll() - Activate every registered driver.
 *
 * Iterates the driver array and calls Activate() on each driver that is not
 * already active, logging each one.
 */
void DriverManager::ActivateAll() {
    for (int i = 0; i < numDrivers; i++) {
        if (!drivers[i]->is_Active) {
            KDBG1("Activating Driver: %s", drivers[i]->driverName);
            drivers[i]->Activate();  // Activate the driver.
            KDBG1("Driver %s: [OK]", drivers[i]->driverName);
        }
    }
}
