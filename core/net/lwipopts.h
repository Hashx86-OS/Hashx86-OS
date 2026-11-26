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

/*
 * lwIP configuration for Hashx86.
 *
 * NO_SYS=0: lwIP's tcpip thread owns the core, the NIC poller posts received
 * frames with tcpip_input(), and user threads use the socket/netconn API. The
 * sys_arch layer lives in core/net/sys_arch.cpp.
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// ---- Core model ----------------------------------------------------------

#define NO_SYS 0
#define SYS_LIGHTWEIGHT_PROT 1
#define LWIP_TIMERS 1
#define LWIP_TCPIP_CORE_LOCKING 1
#define LWIP_TCPIP_CORE_LOCKING_INPUT 0

// lwIP's socket API uses errno; the kernel has no libc errno.h.
#define LWIP_PROVIDE_ERRNO 1

// ---- Memory --------------------------------------------------------------

#define MEM_LIBC_MALLOC 0
#define MEMP_MEM_MALLOC 0
#define MEM_ALIGNMENT 4
#define MEM_SIZE (96 * 1024)

#define PBUF_POOL_SIZE 32
#define PBUF_POOL_BUFSIZE 1536

// ---- Protocols -----------------------------------------------------------

#define LWIP_IPV4 1
#define LWIP_IPV6 0
#define LWIP_ARP 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_HOOK_FILENAME "isn_hook.h"
#define LWIP_DHCP 1
#define LWIP_DNS 1
#define LWIP_AUTOIP 0
#define LWIP_IGMP 0

// Slirp (and most real DHCP servers) answer immediately; skip the extra ARP
// probe for the offered address to keep bring-up fast.
#define DHCP_DOES_ARP_CHECK 0

// Small UDP/DHCP/DNS packets do not need fragmentation during bring-up.
#define IP_FRAG 0
#define IP_REASSEMBLY 0

// ---- Network interfaces --------------------------------------------------

#define LWIP_SINGLE_NETIF 0
#define LWIP_NETIF_LOOPBACK 1
#define LWIP_NETIF_LOOPBACK_MULTITHREADING 1
#define LWIP_NETIF_HOSTNAME 0
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 0
#define LWIP_NETIF_REMOVE_CALLBACK 0
#define LWIP_NETIF_API 0

// ---- APIs ----------------------------------------------------------------

#define LWIP_NETCONN 1
#define LWIP_SOCKET 1
#define LWIP_SO_RCVTIMEO 1
#define LWIP_SO_SNDTIMEO 1
#define LWIP_SO_SNDRCVTIMEO_NONSTANDARD 1
#define LWIP_COMPAT_SOCKETS 0
#define LWIP_POSIX_SOCKETS_IO_NAMES 0

// ---- Pools ---------------------------------------------------------------

#define MEMP_NUM_UDP_PCB 8
#define MEMP_NUM_RAW_PCB 4
#define MEMP_NUM_SYS_TIMEOUT 16
#define MEMP_NUM_NETCONN 8
#define MEMP_NUM_NETBUF 8
#define MEMP_NUM_TCPIP_MSG_API 16
#define MEMP_NUM_TCPIP_MSG_INPKT 32

// TCP client tuning. The HTTP download test stays well inside these windows.
#define MEMP_NUM_TCP_PCB 8
#define MEMP_NUM_TCP_PCB_LISTEN 4
#define MEMP_NUM_TCP_SEG 24
#define TCP_MSS 1460
#define TCP_WND (8 * 1024)
#define TCP_SND_BUF (8 * 1024)
#define TCP_SND_QUEUELEN 16

// Mailbox sizing. A size of 0 tells sys_mbox_new() to use its default.
#define TCPIP_MBOX_SIZE 32
#define DEFAULT_UDP_RECVMBOX_SIZE 8
#define DEFAULT_ACCEPTMBOX_SIZE 8

// ---- Diagnostics ---------------------------------------------------------

#define LWIP_DEBUG 0
#define LWIP_STATS 0

#endif  // LWIPOPTS_H
