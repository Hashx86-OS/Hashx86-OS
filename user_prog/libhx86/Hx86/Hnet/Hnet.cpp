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

#include <Hx86/Hnet/Hnet.h>
#include <Hx86/Hsyscalls/syscalls.h>

namespace {

// Linux i386 socketcall sub-operations, matching the kernel dispatcher.
constexpr uint32_t kCallSocket = 1;
constexpr uint32_t kCallBind = 2;
constexpr uint32_t kCallConnect = 3;
constexpr uint32_t kCallSend = 9;
constexpr uint32_t kCallRecv = 10;
constexpr uint32_t kCallSendTo = 11;
constexpr uint32_t kCallRecvFrom = 12;
constexpr uint32_t kCallShutdown = 13;
constexpr uint32_t kCallResolve = 30;

int32_t SocketCall(uint32_t operation, uint32_t args[6]) {
    int32_t result = -1;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "a"(sys_socketcall), "b"(operation), "c"(args)
                 : "memory");
    return result;
}

}  // namespace

namespace Hnet {

int Socket(int type, int protocol) {
    uint32_t args[6] = {(uint32_t)AF_INET, (uint32_t)type, (uint32_t)protocol, 0, 0, 0};
    return SocketCall(kCallSocket, args);
}

int Bind(int fd, const SockAddrIn& address) {
    uint32_t args[6] = {(uint32_t)fd, (uint32_t)&address, (uint32_t)sizeof(SockAddrIn), 0, 0, 0};
    return SocketCall(kCallBind, args);
}

int Connect(int fd, const SockAddrIn& address) {
    uint32_t args[6] = {(uint32_t)fd, (uint32_t)&address, (uint32_t)sizeof(SockAddrIn), 0, 0, 0};
    return SocketCall(kCallConnect, args);
}

int Send(int fd, const void* data, uint32_t length, uint32_t flags) {
    uint32_t args[6] = {(uint32_t)fd, (uint32_t)data, length, flags, 0, 0};
    return SocketCall(kCallSend, args);
}

int Recv(int fd, void* data, uint32_t length, uint32_t flags) {
    uint32_t args[6] = {(uint32_t)fd, (uint32_t)data, length, flags, 0, 0};
    return SocketCall(kCallRecv, args);
}

int Shutdown(int fd, int how) {
    uint32_t args[6] = {(uint32_t)fd, (uint32_t)how, 0, 0, 0, 0};
    return SocketCall(kCallShutdown, args);
}

int Resolve(const char* hostname, uint32_t* address) {
    uint32_t args[6] = {(uint32_t)hostname, (uint32_t)address, 0, 0, 0, 0};
    return SocketCall(kCallResolve, args);
}

int SendTo(int fd, const void* data, uint32_t length, uint32_t flags, const SockAddrIn* address) {
    uint32_t args[6] = {
        (uint32_t)fd, (uint32_t)data,    length,
        flags,        (uint32_t)address, address != nullptr ? (uint32_t)sizeof(SockAddrIn) : 0};
    return SocketCall(kCallSendTo, args);
}

int RecvFrom(int fd, void* data, uint32_t length, uint32_t flags, SockAddrIn* address,
             uint32_t* addressLength) {
    uint32_t args[6] = {(uint32_t)fd, (uint32_t)data,    length,
                        flags,        (uint32_t)address, (uint32_t)addressLength};
    return SocketCall(kCallRecvFrom, args);
}

int Close(int fd) {
    return syscall_close((uint32_t)fd);
}

}  // namespace Hnet
