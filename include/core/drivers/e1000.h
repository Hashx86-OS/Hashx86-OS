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

#ifndef CORE_DRIVERS_E1000_H
#define CORE_DRIVERS_E1000_H

class NetworkDevice;

/**
 * E1000Probe() - Probe PCI for an Intel 82540EM Ethernet controller.
 *
 * VirtualBox presents its default Intel PRO/1000 MT Desktop NIC as an
 * 8086:100E. The driver enables memory decoding and bus mastering, resets the
 * controller, reads the MAC address from the EEPROM and brings up the legacy
 * RX/TX descriptor rings. Interrupts stay masked; the network thread polls the
 * rings through NetworkDevice::Poll().
 *
 * Return: A ready-to-use device owned by the caller, or null when no supported
 *         controller is present or initialization failed.
 */
NetworkDevice* E1000Probe();

#endif  // CORE_DRIVERS_E1000_H
