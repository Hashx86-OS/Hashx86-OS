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

// lwIP netif adapter for NetworkDevice.
//
// RX: NetworkDevice::Poll() invokes NetifReceive() on the RX polling thread;
// each frame is copied into a pbuf and handed to netif->input, which is
// tcpip_input() and queues it for the tcpip thread.
// TX: lwIP calls NetifLinkOutput() on the tcpip thread; it flattens the pbuf
// chain and asks the device to queue it.

#include "netif_eth.h"

#include <core/net/net_device.h>
#include <debug.h>
#include <string.h>

extern "C" {
#include <lwip/def.h>
#include <lwip/dhcp.h>
#include <lwip/dns.h>
#include <lwip/err.h>
#include <lwip/etharp.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/tcpip.h>
#include <netif/ethernet.h>
}

namespace {

constexpr uint16_t kMaxFrameLength = 1600;

struct netif g_netif;
bool g_netifReady = false;
uint32_t g_lastLoggedAddress = 0;
uint32_t g_lastLoggedDns = 0;

// Only the tcpip thread calls linkoutput, so one scratch frame is enough.
// Keeping it static avoids a 1.5 KiB stack allocation per transmit.
uint8_t g_txFrame[kMaxFrameLength];

err_t NetifLinkOutput(struct netif* netif, struct pbuf* p);

/**
 * NetifStatusCallback() - Log address changes; runs on the tcpip thread.
 */
void NetifStatusCallback(struct netif* netif) {
    if (netif == nullptr || !netif_is_up(netif) || !netif_is_link_up(netif)) {
        g_lastLoggedAddress = 0;
        return;
    }

    const ip4_addr_t* address = netif_ip4_addr(netif);
    uint32_t value = ip4_addr_get_u32(address);
    if (value == 0) {
        g_lastLoggedAddress = 0;
        return;
    }
    if (value == g_lastLoggedAddress) return;
    g_lastLoggedAddress = value;

    const ip4_addr_t* gateway = netif_ip4_gw(netif);
    printf("[NET] netif en%u: %u.%u.%u.%u gw %u.%u.%u.%u\n", (uint32_t)netif->num,
           (uint32_t)ip4_addr1_16(address), (uint32_t)ip4_addr2_16(address),
           (uint32_t)ip4_addr3_16(address), (uint32_t)ip4_addr4_16(address),
           (uint32_t)ip4_addr1_16(gateway), (uint32_t)ip4_addr2_16(gateway),
           (uint32_t)ip4_addr3_16(gateway), (uint32_t)ip4_addr4_16(gateway));

    // Some routers only advertise DNS over DHCPv6 when a dual-stack (IPv4 +
    // IPv6) APN is active and omit DHCPv4 option 6. Do not leave the resolver
    // pointing at the QEMU/VBox fallback: use the gateway, which normally
    // proxies DNS, and remember 10.0.2.3 only for user-mode networking.
    uint32_t gatewayValue = ip4_addr_get_u32(gateway);
    ip4_addr_t vmGateway;
    IP4_ADDR(&vmGateway, 10, 0, 2, 2);
    bool userModeNetwork = (gatewayValue == ip4_addr_get_u32(&vmGateway));

    const ip_addr_t* currentDns = dns_getserver(0);
    if (ip_addr_isany(currentDns)) {
        ip_addr_t fallback;
        bool haveFallback = false;
        if (userModeNetwork) {
            IP4_ADDR(&fallback, 10, 0, 2, 3);
            haveFallback = true;
        } else if (gatewayValue != 0) {
            ip_addr_set_ip4_u32(&fallback, gatewayValue);
            haveFallback = true;
        }
        if (haveFallback) {
            dns_setserver(0, &fallback);
            printf("[NET] dns: no DHCPv4 option 6, using %u.%u.%u.%u\n",
                   (uint32_t)ip4_addr1_16(ip_2_ip4(&fallback)),
                   (uint32_t)ip4_addr2_16(ip_2_ip4(&fallback)),
                   (uint32_t)ip4_addr3_16(ip_2_ip4(&fallback)),
                   (uint32_t)ip4_addr4_16(ip_2_ip4(&fallback)));
        }
    } else if (ip_addr_isany(dns_getserver(1)) && gatewayValue != 0 && !userModeNetwork) {
        // Keep the router as a second resolver so a stale or unreachable
        // primary DNS handed out by the carrier still has a fallback.
        ip_addr_t backup;
        ip_addr_set_ip4_u32(&backup, gatewayValue);
        if (!ip_addr_cmp(&backup, dns_getserver(0))) {
            dns_setserver(1, &backup);
        }
    }

    const ip_addr_t* dns0 = dns_getserver(0);
    if (dns0 != nullptr && !ip_addr_isany(dns0)) {
        uint32_t dnsValue = ip4_addr_get_u32(ip_2_ip4(dns0));
        if (dnsValue != g_lastLoggedDns) {
            g_lastLoggedDns = dnsValue;
            printf("[NET] dns: %u.%u.%u.%u\n", (uint32_t)ip4_addr1_16(ip_2_ip4(dns0)),
                   (uint32_t)ip4_addr2_16(ip_2_ip4(dns0)), (uint32_t)ip4_addr3_16(ip_2_ip4(dns0)),
                   (uint32_t)ip4_addr4_16(ip_2_ip4(dns0)));
        }
    }
}

/**
 * NetifInitCallback() - Fill in the lwIP netif fields for the device.
 */
err_t NetifInitCallback(struct netif* netif) {
    NetworkDevice* device = (NetworkDevice*)netif->state;
    if (device == nullptr) return ERR_ARG;

    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->mtu = 1500;
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, device->GetMac(), 6);

    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    netif->output = etharp_output;
    netif->linkoutput = NetifLinkOutput;
    netif->status_callback = NetifStatusCallback;
    return ERR_OK;
}

/**
 * NetifLinkOutput() - Transmit one pbuf through the Ethernet device.
 */
err_t NetifLinkOutput(struct netif* netif, struct pbuf* p) {
    if (netif == nullptr || p == nullptr) return ERR_ARG;

    uint16_t total = p->tot_len;
    if (total < SIZEOF_ETH_HDR || total > kMaxFrameLength) {
        return ERR_BUF;
    }

    if (pbuf_copy_partial(p, g_txFrame, total, 0) != total) {
        return ERR_BUF;
    }

    NetworkDevice* device = (NetworkDevice*)netif->state;
    if (device == nullptr || !device->SendFrame(g_txFrame, total)) {
        return ERR_IF;
    }
    return ERR_OK;
}

/**
 * NetifReceive() - Device receive callback; runs on the RX polling thread.
 */
void NetifReceive(void* context, const uint8_t* frame, uint16_t length) {
    struct netif* netif = (struct netif*)context;
    if (netif == nullptr || frame == nullptr || length < SIZEOF_ETH_HDR ||
        length > kMaxFrameLength) {
        return;
    }

    struct pbuf* p = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
    if (p == nullptr) return;

    if (pbuf_take(p, frame, length) != ERR_OK) {
        pbuf_free(p);
        return;
    }

    // tcpip_input() queues the pbuf for the tcpip thread. On error it has not
    // taken ownership, so release it here.
    if (netif->input(p, netif) != ERR_OK) {
        pbuf_free(p);
    }
}

}  // namespace

bool NetifEthernetBringUp(NetworkDevice* device) {
    if (g_netifReady) return true;
    if (device == nullptr) return false;

    // Start with no IPv4 address; DHCP supplies it after the link is up.
    // This function runs on the tcpip thread (from tcpip_init's completion
    // callback), so lwIP core access is serialized by the core lock.
    if (netif_add(&g_netif, nullptr, nullptr, nullptr, device, NetifInitCallback, tcpip_input) ==
        nullptr) {
        printf("[NET] failed to add Ethernet netif\n");
        return false;
    }

    netif_set_default(&g_netif);
    device->SetReceiveHandler(NetifReceive, &g_netif);
    netif_set_up(&g_netif);

    g_netifReady = true;
    printf("[NET] netif en%u: DHCP\n", (uint32_t)g_netif.num);

    // No resolver is installed here: DHCP option 6 sets it when the lease
    // arrives, and NetifStatusCallback() falls back to the gateway if the
    // server omits it (dual-stack routers often advertise DNS over DHCPv6).

    err_t err = dhcp_start(&g_netif);
    if (err != ERR_OK) {
        printf("[NET] dhcp: start failed (%d)\n", (int)err);
    }
    return true;
}

struct netif* NetifEthernetInterface() {
    return g_netifReady ? &g_netif : nullptr;
}

void NetifEthernetUseStaticConfig() {
    if (!g_netifReady) return;

    dhcp_stop(&g_netif);

    ip4_addr_t address;
    ip4_addr_t netmask;
    ip4_addr_t gateway;
    IP4_ADDR(&address, 10, 0, 2, 15);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 10, 0, 2, 2);
    netif_set_addr(&g_netif, &address, &netmask, &gateway);

    // User-mode networking (QEMU slirp/VirtualBox NAT) resolves on 10.0.2.3.
    ip_addr_t fallbackDns;
    IP4_ADDR(&fallbackDns, 10, 0, 2, 3);
    dns_setserver(0, &fallbackDns);

    printf("[NET] netif static 10.0.2.15/24 gw 10.0.2.2\n");
}
