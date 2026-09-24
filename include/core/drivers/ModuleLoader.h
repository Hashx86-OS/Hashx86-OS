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

#ifndef MODULE_LOADER_H
#define MODULE_LOADER_H

#include <core/drivers/SymbolTable.h>
#include <core/drivers/driver_info.h>
#include <core/elf.h>
#include <core/filesystem/File.h>
#include <core/memory.h>
#include <debug.h>
#include <types.h>

extern "C" void __cxa_pure_virtual();

/**
 * class ModuleLoader - Statically load relocatable ELF driver modules.
 */
class ModuleLoader {
public:
    /**
     * LoadMatchingDriver() - Load a driver module matching a PCI device.
     * @file: Open ELF driver file.
     * @target_vid: PCI vendor ID to match.
     * @target_did: PCI device ID to match.
     *
     * Return: Address of the CreateDriverInstance() entry point, or null.
     */
    static void* LoadMatchingDriver(File* file, uint16_t target_vid, uint16_t target_did);
    /**
     * Probe() - Read the driver manifest from a module file.
     * @file: Open ELF driver file.
     * @info: Receives the parsed manifest.
     *
     * Return: true when the module contains a valid driver manifest.
     */
    static bool Probe(File* file, DriverManifest* info);

private:
    static void* LoadDriver(File* file);

    // Internal ELF helpers (specific to x86 relocation).
    static int ApplyRelocation(uint32_t type, uint32_t* target, uint32_t value, uint32_t addend);
};

#endif
