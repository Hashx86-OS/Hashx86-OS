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

#ifndef DRIVER_INFO_H
#define DRIVER_INFO_H

#include <stdint.h>

#define DRIVER_INFO_MAGIC 0x44525649

/**
 * struct HardwareID - A PCI vendor/device ID pair.
 * @vendor_id: PCI vendor ID.
 * @device_id: PCI device ID.
 */
struct HardwareID {
    uint16_t vendor_id;
    uint16_t device_id;
};

/**
 * struct DriverManifest - Static metadata describing a loadable driver.
 * @magic: Manifest magic number.
 * @name: Driver name.
 * @version: Driver version string.
 * @devices: Supported device ID pairs.
 */
struct DriverManifest {
    uint32_t magic;
    char name[32];
    char version[16];

    // Up to 4 supported device ID pairs.
    HardwareID devices[4];
};

#define DEFINE_DRIVER_INFO(name_str, version_str, ...)                                            \
    extern "C" __attribute__((section(".driver_info"), used)) DriverManifest _driver_metadata = { \
        DRIVER_INFO_MAGIC, name_str, version_str, {__VA_ARGS__}};

#endif
