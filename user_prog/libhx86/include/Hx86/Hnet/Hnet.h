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

#ifndef HX86_HNET_H
#define HX86_HNET_H

#include <stdint.h>

/**
 * namespace Hnet - Minimal BSD-style IPv4 socket API for user programs.
 *
 * The kernel exposes the lwIP socket layer through sys_socketcall (102).
 * Descriptors returned here share the process fd namespace with files and can
 * be closed with Hnet::Close() or the regular close() syscall.
 */
namespace Hnet {

constexpr uint8_t AF_INET = 2;
constexpr uint8_t SOCK_STREAM = 1;
constexpr uint8_t SOCK_DGRAM = 2;
constexpr uint32_t MSG_DONTWAIT = 0x08;

constexpr int SHUT_RD = 0;
constexpr int SHUT_WR = 1;
constexpr int SHUT_RDWR = 2;

/**
 * struct SockAddrIn - IPv4 socket address (lwIP/BSD wire layout).
 * @sin_len: Structure length, normally sizeof(SockAddrIn).
 * @sin_family: AF_INET.
 * @sin_port: Port in network byte order.
 * @sin_addr: IPv4 address in network byte order.
 * @sin_zero: Padding, must be zero.
 */
struct SockAddrIn {
    uint8_t sin_len;
    uint8_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t sin_zero[8];
} __attribute__((packed));

/** Htons() - Convert a 16-bit value to network byte order. */
inline uint16_t Htons(uint16_t value) {
    return (uint16_t)((value >> 8) | (value << 8));
}

/** Htonl() - Convert a 32-bit value to network byte order. */
inline uint32_t Htonl(uint32_t value) {
    return ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) |
           ((value & 0x00FF0000u) >> 8) | ((value & 0xFF000000u) >> 24);
}

/** Ip4() - Build an IPv4 address in network byte order from four octets. */
inline uint32_t Ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return Htonl(((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d);
}

/**
 * Socket() - Create an IPv4 socket.
 * @type: SOCK_DGRAM (SOCK_STREAM is added with the TCP milestone).
 * @protocol: Usually 0.
 *
 * Return: A non-negative descriptor, or -1 on failure.
 */
int Socket(int type, int protocol = 0);

/**
 * Bind() - Bind a socket to a local IPv4 address.
 */
int Bind(int fd, const SockAddrIn& address);

/**
 * Connect() - Connect a stream socket to a remote IPv4 address.
 *
 * Blocks until the TCP handshake completes or fails.
 *
 * Return: 0 on success, -1 on failure.
 */
int Connect(int fd, const SockAddrIn& address);

/**
 * Send() - Send bytes on a connected stream socket.
 *
 * Return: Bytes sent, or -1 on failure.
 */
int Send(int fd, const void* data, uint32_t length, uint32_t flags);

/**
 * Recv() - Receive bytes from a connected stream socket.
 *
 * Return: Bytes received, 0 when the peer closed, or -1 on failure.
 */
int Recv(int fd, void* data, uint32_t length, uint32_t flags);

/**
 * Shutdown() - Close one or both directions of a stream socket.
 */
int Shutdown(int fd, int how);

/**
 * Resolve() - Resolve a hostname through the kernel's DNS client.
 * @hostname: NUL-terminated name (at most 127 bytes).
 * @address: Receives the IPv4 address in network byte order.
 *
 * Blocks until DNS answers or fails.
 *
 * Return: 0 on success, -1 on failure.
 */
int Resolve(const char* hostname, uint32_t* address);

/**
 * SendTo() - Send a UDP datagram.
 * @address: Optional destination. May be null for a connected socket.
 *
 * Return: Bytes sent, or -1 on failure.
 */
int SendTo(int fd, const void* data, uint32_t length, uint32_t flags, const SockAddrIn* address);

/**
 * RecvFrom() - Receive a UDP datagram.
 * @address: Optional source address output.
 * @addressLength: Optional in/out size for @address.
 *
 * Without MSG_DONTWAIT the call blocks until a datagram arrives.
 *
 * Return: Bytes received, or -1 on failure.
 */
int RecvFrom(int fd, void* data, uint32_t length, uint32_t flags, SockAddrIn* address,
             uint32_t* addressLength);

/**
 * Close() - Close a socket descriptor.
 *
 * Return: 0 on success, -1 on failure.
 */
int Close(int fd);

}  // namespace Hnet

#endif  // HX86_HNET_H
