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

#ifndef CORE_NET_NETIF_ETH_H
#define CORE_NET_NETIF_ETH_H

class NetworkDevice;
struct netif;

/**
 * NetifEthernetBringUp() - Register a NetworkDevice as lwIP's default netif.
 * @device: Initialized Ethernet device. The netif keeps a borrowed pointer.
 *
 * Installs the static IPv4 configuration 10.0.2.15/24 with gateway 10.0.2.2
 * and wires the device's receive callback into lwIP's Ethernet input path.
 *
 * Return: True when the netif was added and brought up.
 */
bool NetifEthernetBringUp(NetworkDevice* device);

/**
 * NetifEthernetInterface() - Return the registered Ethernet netif, if any.
 */
struct netif* NetifEthernetInterface();

/**
 * NetifEthernetUseStaticConfig() - Stop DHCP and install the fallback address.
 *
 * Used when no DHCP server answers so the interface still has the QEMU slirp
 * static configuration (10.0.2.15/24, gateway 10.0.2.2).
 */
void NetifEthernetUseStaticConfig();

#endif  // CORE_NET_NETIF_ETH_H
