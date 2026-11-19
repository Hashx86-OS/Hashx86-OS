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

// User-space socket syscall layer.
//
// Users see descriptors from the process fd namespace; the kernel maps them to
// lwIP's global socket descriptors in ProcessControlBlock::socketTable. Payload
// and sockaddr buffers are copied through CopyFromUser/CopyToUser so a bad user
// pointer cannot fault the kernel.
//
// lwIP calls from here run in the caller's thread. With
// LWIP_TCPIP_CORE_LOCKING=1 they take the core mutex, and blocking waits use
// sys_arch, which yields through the scheduler even though syscall handlers run
// with interrupts disabled.

#include <core/Iguard.h>
#include <core/globals.h>
#include <core/net/net.h>
#include <core/net/net_socket.h>
#include <core/process_types.h>
#include <core/scheduler.h>
#include <debug.h>
#include <string.h>
#include <types.h>

extern "C" {
#include <lwip/api.h>
#include <lwip/err.h>
#include <lwip/ip_addr.h>
#include <lwip/sockets.h>
}

// Provided by core/syscalls.cpp.
bool CopyFromUser(ProcessControlBlock* process, void* dst, const void* srcUser, size_t size);
bool CopyToUser(ProcessControlBlock* process, void* dstUser, const void* src, size_t size);
bool CopyUserString(ProcessControlBlock* process, const char* srcUser, char* dst, size_t dstSize);

namespace {

constexpr uint32_t kSocketArgCount = 6;
// Bounce buffer limit for datagrams. The QEMU DNS test stays well below this;
// larger payloads would need a dedicated per-call heap policy.
constexpr uint32_t kMaxDatagram = 2048;
// TCP send/recv chunks. The HTTP client reads in small chunks, so a modest
// per-call buffer keeps the kernel stack and heap use bounded.
constexpr uint32_t kMaxStreamChunk = 4096;

constexpr int kDeferredCloseCapacity = 256;

int g_deferredCloseFds[kDeferredCloseCapacity];
volatile int g_deferredCloseHead = 0;
volatile int g_deferredCloseTail = 0;
volatile int g_deferredCloseCount = 0;
volatile bool g_deferredCloseOverflow = false;
bool g_overflowLogged = false;

/**
 * AllocateSocketFd() - Reserve a user-visible descriptor for an lwIP socket.
 *
 * Return: The user fd, or -1 when the process has no space.
 */
int32_t AllocateSocketFd(ProcessControlBlock* process, int lwipFd) {
    if (process == nullptr || lwipFd < 0) return -1;

    InterruptGuard guard;
    for (uint32_t fd = FD_MIN; fd < FD_MAX; fd++) {
        if (process->socketTable[fd] < 0 && process->fdTable[fd] == nullptr) {
            process->socketTable[fd] = lwipFd;
            return (int32_t)fd;
        }
    }
    return -1;
}

/**
 * GetSocketFd() - Resolve a user descriptor to its lwIP socket descriptor.
 *
 * Return: The lwIP fd, or -1 when @userFd is not an open socket.
 */
int GetSocketFd(ProcessControlBlock* process, uint32_t userFd) {
    if (process == nullptr || userFd < FD_MIN || userFd >= FD_MAX) return -1;
    InterruptGuard guard;
    return process->socketTable[userFd];
}

/**
 * ReadSockaddr() - Copy and validate an IPv4 sockaddr from user space.
 */
bool ReadSockaddr(ProcessControlBlock* process, uint32_t userAddress, uint32_t userLength,
                  struct sockaddr_in* out) {
    if (out == nullptr || userAddress == 0) return false;
    if (userLength < sizeof(struct sockaddr_in)) return false;
    if (!CopyFromUser(process, out, (const void*)userAddress, sizeof(*out))) return false;
    return out->sin_family == AF_INET;
}

/**
 * WriteSockaddr() - Copy a received sockaddr and its length back to user space.
 */
bool WriteSockaddr(ProcessControlBlock* process, uint32_t userAddress, uint32_t userLengthPtr,
                   const struct sockaddr_in* address, socklen_t actualLength) {
    if (userAddress == 0 && userLengthPtr == 0) return true;

    socklen_t userLength = 0;
    if (userLengthPtr != 0) {
        if (!CopyFromUser(process, &userLength, (const void*)userLengthPtr, sizeof(userLength))) {
            return false;
        }
    }

    if (userAddress != 0 && userLength != 0) {
        uint32_t copyLength = sizeof(*address);
        if (actualLength < copyLength) copyLength = actualLength;
        if (userLength < copyLength) copyLength = userLength;
        if (copyLength != 0 && !CopyToUser(process, (void*)userAddress, address, copyLength)) {
            return false;
        }
    }

    if (userLengthPtr != 0) {
        if (!CopyToUser(process, (void*)userLengthPtr, &actualLength, sizeof(actualLength))) {
            return false;
        }
    }
    return true;
}

/**
 * SocketCreate() - Create an IPv4 socket and assign a user descriptor.
 */
int32_t SocketCreate(ProcessControlBlock* process, const uint32_t* args) {
    int lwipFd = lwip_socket((int)args[0], (int)args[1], (int)args[2]);
    if (lwipFd < 0) return -1;

    int32_t userFd = AllocateSocketFd(process, lwipFd);
    if (userFd < 0) {
        lwip_close(lwipFd);
        return -1;
    }
    return userFd;
}

/**
 * SocketBind() - Bind a user socket to a local address.
 */
int32_t SocketBind(ProcessControlBlock* process, const uint32_t* args) {
    int lwipFd = GetSocketFd(process, args[0]);
    if (lwipFd < 0) return -1;

    struct sockaddr_in address;
    if (!ReadSockaddr(process, args[1], args[2], &address)) return -1;

    return lwip_bind(lwipFd, (const struct sockaddr*)&address, sizeof(address)) == 0 ? 0 : -1;
}

/**
 * SocketConnect() - Start a connection-oriented socket (TCP client).
 *
 * The blocking connect waits on a netconn semaphore; sys_arch yields to the
 * tcpip thread while the handshake completes.
 */
int32_t SocketConnect(ProcessControlBlock* process, const uint32_t* args) {
    int lwipFd = GetSocketFd(process, args[0]);
    if (lwipFd < 0) return -1;

    struct sockaddr_in address;
    if (!ReadSockaddr(process, args[1], args[2], &address)) return -1;

    return lwip_connect(lwipFd, (const struct sockaddr*)&address, sizeof(address)) == 0 ? 0 : -1;
}

/**
 * SocketSend() - Send bytes on a connected stream socket.
 */
int32_t SocketSend(ProcessControlBlock* process, const uint32_t* args) {
    int lwipFd = GetSocketFd(process, args[0]);
    if (lwipFd < 0) return -1;

    uint32_t length = args[2];
    if (length == 0 || length > kMaxStreamChunk) return -1;

    uint8_t* buffer = (uint8_t*)kmalloc(length);
    if (buffer == nullptr) return -1;
    if (!CopyFromUser(process, buffer, (const void*)args[1], length)) {
        kfree(buffer);
        return -1;
    }

    int32_t result = (int32_t)lwip_send(lwipFd, buffer, length, (int)args[3]);
    kfree(buffer);
    return result;
}

/**
 * SocketRecv() - Receive bytes from a connected stream socket.
 *
 * A zero return means the peer closed the connection. Without MSG_DONTWAIT
 * this blocks on the netconn receive mailbox through sys_arch.
 */
int32_t SocketRecv(ProcessControlBlock* process, const uint32_t* args) {
    int lwipFd = GetSocketFd(process, args[0]);
    if (lwipFd < 0) return -1;

    uint32_t length = args[2];
    if (length == 0 || length > kMaxStreamChunk) return -1;

    uint8_t* buffer = (uint8_t*)kmalloc(length);
    if (buffer == nullptr) return -1;

    int32_t result = (int32_t)lwip_recv(lwipFd, buffer, length, (int)args[3]);
    if (result > 0) {
        if (!CopyToUser(process, (void*)args[1], buffer, (size_t)result)) {
            kfree(buffer);
            return -1;
        }
    }

    kfree(buffer);
    return result;
}

/**
 * SocketShutdown() - Shut down one or both directions of a stream socket.
 */
int32_t SocketShutdown(ProcessControlBlock* process, const uint32_t* args) {
    int lwipFd = GetSocketFd(process, args[0]);
    if (lwipFd < 0) return -1;
    return lwip_shutdown(lwipFd, (int)args[1]) == 0 ? 0 : -1;
}

/**
 * SocketResolve() - Resolve a hostname to an IPv4 address.
 *
 * Uses lwIP's netconn resolver, which sends the DNS query from the tcpip
 * thread and blocks this syscall on a semaphore. sys_arch yields while the
 * answer is pending, exactly like a blocking socket operation.
 */
int32_t SocketResolve(ProcessControlBlock* process, const uint32_t* args) {
    char hostname[128];
    if (!CopyUserString(process, (const char*)args[0], hostname, sizeof(hostname))) return -1;
    if (hostname[0] == '\0') return -1;

    ip_addr_t address;
    if (netconn_gethostbyname(hostname, &address) != ERR_OK) return -1;

    uint32_t ipv4 = ip4_addr_get_u32(ip_2_ip4(&address));
    if (!CopyToUser(process, (void*)args[1], &ipv4, sizeof(ipv4))) return -1;
    return 0;
}

/**
 * SocketSendTo() - Send a datagram from a user socket.
 */
int32_t SocketSendTo(ProcessControlBlock* process, const uint32_t* args) {
    int lwipFd = GetSocketFd(process, args[0]);
    if (lwipFd < 0) return -1;

    uint32_t length = args[2];
    if (length == 0 || length > kMaxDatagram) return -1;

    uint8_t* buffer = (uint8_t*)kmalloc(length);
    if (buffer == nullptr) return -1;
    if (!CopyFromUser(process, buffer, (const void*)args[1], length)) {
        kfree(buffer);
        return -1;
    }

    struct sockaddr_in destination;
    const struct sockaddr* destinationPtr = nullptr;
    socklen_t destinationLength = 0;
    if (args[4] != 0) {
        if (!ReadSockaddr(process, args[4], args[5], &destination)) {
            kfree(buffer);
            return -1;
        }
        destinationPtr = (const struct sockaddr*)&destination;
        destinationLength = sizeof(destination);
    }

    int32_t result = (int32_t)lwip_sendto(lwipFd, buffer, length, (int)args[3], destinationPtr,
                                          destinationLength);
    kfree(buffer);
    return result;
}

/**
 * SocketRecvFrom() - Receive a datagram into a user buffer.
 *
 * Without MSG_DONTWAIT this blocks on lwIP's recv mailbox. The sys_arch wait
 * yields through the scheduler so the tcpip thread can deliver the packet.
 */
int32_t SocketRecvFrom(ProcessControlBlock* process, const uint32_t* args) {
    int lwipFd = GetSocketFd(process, args[0]);
    if (lwipFd < 0) return -1;

    uint32_t length = args[2];
    if (length == 0 || length > kMaxDatagram) return -1;

    uint8_t* buffer = (uint8_t*)kmalloc(length);
    if (buffer == nullptr) return -1;

    struct sockaddr_in from;
    memset(&from, 0, sizeof(from));
    socklen_t fromLength = sizeof(from);

    int32_t result = (int32_t)lwip_recvfrom(lwipFd, buffer, length, (int)args[3],
                                            args[4] != 0 ? (struct sockaddr*)&from : nullptr,
                                            args[4] != 0 ? &fromLength : nullptr);
    if (result > 0) {
        if (!CopyToUser(process, (void*)args[1], buffer, (size_t)result)) {
            kfree(buffer);
            return -1;
        }
        if (!WriteSockaddr(process, args[4], args[5], &from, fromLength)) {
            kfree(buffer);
            return -1;
        }
    }

    kfree(buffer);
    return result;
}

/** Enqueue one lwIP descriptor for deferred close. Caller must hold IRQs off. */
void QueueDeferredClose(int lwipFd) {
    if (g_deferredCloseCount >= kDeferredCloseCapacity) {
        g_deferredCloseOverflow = true;
        return;
    }
    g_deferredCloseFds[g_deferredCloseTail] = lwipFd;
    g_deferredCloseTail++;
    if (g_deferredCloseTail >= kDeferredCloseCapacity) g_deferredCloseTail = 0;
    g_deferredCloseCount++;
}

}  // namespace

int32_t NetSocketCall(uint32_t call, uint32_t userArgs) {
    Scheduler* scheduler = Scheduler::activeInstance;
    if (scheduler == nullptr || !NetworkIsReady()) return -1;

    ProcessControlBlock* process = scheduler->GetCurrentProcess();
    if (process == nullptr || userArgs == 0) return -1;

    uint32_t args[kSocketArgCount];
    if (!CopyFromUser(process, args, (const void*)userArgs, sizeof(args))) return -1;

    switch (call) {
        case HNET_CALL_SOCKET:
            return SocketCreate(process, args);
        case HNET_CALL_BIND:
            return SocketBind(process, args);
        case HNET_CALL_CONNECT:
            return SocketConnect(process, args);
        case HNET_CALL_SEND:
            return SocketSend(process, args);
        case HNET_CALL_RECV:
            return SocketRecv(process, args);
        case HNET_CALL_SENDTO:
            return SocketSendTo(process, args);
        case HNET_CALL_RECVFROM:
            return SocketRecvFrom(process, args);
        case HNET_CALL_SHUTDOWN:
            return SocketShutdown(process, args);
        case HNET_CALL_RESOLVE:
            return SocketResolve(process, args);
        default:
            // listen/accept/setsockopt arrive with a future server milestone.
            return -1;
    }
}

bool NetSocketFdValid(ProcessControlBlock* process, uint32_t fd) {
    return GetSocketFd(process, fd) >= 0;
}

int32_t NetCloseSocketFd(ProcessControlBlock* process, uint32_t fd) {
    if (process == nullptr || fd < FD_MIN || fd >= FD_MAX) return -1;

    int lwipFd;
    {
        InterruptGuard guard;
        lwipFd = process->socketTable[fd];
        if (lwipFd < 0) return -1;
        process->socketTable[fd] = -1;
    }

    return lwip_close(lwipFd) == 0 ? 0 : -1;
}

void NetCloseProcessSockets(ProcessControlBlock* process) {
    if (process == nullptr) return;

    InterruptGuard guard;
    for (uint32_t fd = FD_MIN; fd < FD_MAX; fd++) {
        int lwipFd = process->socketTable[fd];
        if (lwipFd < 0) continue;
        process->socketTable[fd] = -1;
        QueueDeferredClose(lwipFd);
    }
}

void NetReapClosedSockets() {
    for (;;) {
        int lwipFd;
        {
            InterruptGuard guard;
            if (g_deferredCloseCount == 0) break;
            lwipFd = g_deferredCloseFds[g_deferredCloseHead];
            g_deferredCloseHead++;
            if (g_deferredCloseHead >= kDeferredCloseCapacity) g_deferredCloseHead = 0;
            g_deferredCloseCount--;
        }
        printf("[NET] process exit closed socket fd=%d\n", lwipFd);
        lwip_close(lwipFd);
    }

    if (g_deferredCloseOverflow && !g_overflowLogged) {
        g_overflowLogged = true;
        printf("[NET] socket close queue overflow; leaked a socket\n");
    }
}
