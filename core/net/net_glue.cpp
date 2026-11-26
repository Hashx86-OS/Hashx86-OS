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

// Kernel-side network service for NO_SYS=0 lwIP.
//
// tcpip_init() creates lwIP's core thread. This file also runs one RX polling
// thread to feed frames to tcpip_input() and a separate socket cleanup worker. All lwIP core calls
// happen either in the tcpip thread or under LWIP_TCPIP_CORE_LOCKING.

#define KDBG_COMPONENT "NET"
#include <core/drivers/e1000.h>
#include <core/drivers/rtl8139.h>
#include <core/globals.h>
#include <core/net/net.h>
#include <core/net/net_device.h>
#include <core/net/net_socket.h>
#include <core/random.h>
#include <core/scheduler.h>
#include <debug.h>

#include "netif_eth.h"

extern "C" {
#include <lwip/dhcp.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/timeouts.h>
}

namespace {

bool g_networkInitialized = false;
NetworkDevice* g_networkDevice = nullptr;
int g_rxThreadArg = 0;

/**
 * TcpipInitDone() - Bring up the netif from the tcpip thread.
 *
 * tcpip_init_done is called inside tcpip_thread after it acquires the core
 * lock, before the message loop starts.
 */
void TcpipInitDone(void* arg) {
    NetworkDevice* device = (NetworkDevice*)arg;
    if (device == nullptr) {
        printf("[NET] no supported NIC found\n");
        return;
    }

    if (!NetifEthernetBringUp(device)) {
        printf("[NET] failed to bring up Ethernet netif\n");
        return;
    }

#if defined(HASHX86_NETWORK_STATIC) && HASHX86_NETWORK_STATIC
    // Static VM addressing must be explicitly selected at build time.
    NetifEthernetUseStaticConfig();
#endif
}

/**
 * NetworkRxThreadEntry() - Poll the NIC and feed frames to lwIP.
 */
void NetworkRxThreadEntry(void* arg) {
    (void)arg;

    printf("[NET] RX thread started\n");
    while (true) {
        if (g_networkDevice != nullptr) g_networkDevice->Poll();

        if (g_scheduler) {
            g_scheduler->Sleep(2);
            // Yield immediately: otherwise the loop spins until the next PIT
            // tick even though this thread is already marked blocked.
            asm volatile("int $0x2E" ::: "memory");
        }
    }
}

void NetworkCleanupThreadEntry(void*) {
    for (;;) {
        NetReapClosedSockets();
        if (g_scheduler) {
            g_scheduler->ReapNetworkTerminations();
            g_scheduler->Sleep(10);
            asm volatile("int $0x2E" ::: "memory");
        }
    }
}

}  // namespace

void NetworkInit() {
    if (g_networkInitialized) return;
    if (!RandomInitialize()) {
        printf("[NET] disabled: hardware entropy is unavailable\n");
        return;
    }

    g_networkDevice = Rtl8139Probe();
    if (g_networkDevice == nullptr) {
        g_networkDevice = E1000Probe();
    }

    // tcpip_init() calls lwip_init(), creates the tcpip mailbox/core mutex and
    // starts the stack thread. TcpipInitDone runs on that thread.
    tcpip_init(TcpipInitDone, g_networkDevice);

    g_networkInitialized = true;
    printf("[NET] lwIP %s initialized (NO_SYS=0)\n", LWIP_VERSION_STRING);

    if (!g_scheduler || !g_scheduler->CreateProcess(true, NetworkCleanupThreadEntry, nullptr)) {
        printf("[NET] failed to create socket cleanup worker\n");
        g_networkInitialized = false;
        return;
    }
    if (g_networkDevice != nullptr) {
        if (!g_scheduler ||
            !g_scheduler->CreateProcess(true, NetworkRxThreadEntry, &g_rxThreadArg)) {
            printf("[NET] failed to create RX polling thread\n");
        }
    }
}

bool NetworkIsReady() {
    return g_networkInitialized;
}
