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

#ifndef CORE_NET_NET_SOCKET_H
#define CORE_NET_NET_SOCKET_H

#include <stdint.h>

struct ProcessControlBlock;

/*
 * Linux i386 socketcall sub-operation numbers. User wrappers pass these in
 * ebx and a pointer to an argument array in ecx to sys_socketcall (102).
 */
constexpr uint32_t HNET_CALL_SOCKET = 1;
constexpr uint32_t HNET_CALL_BIND = 2;
constexpr uint32_t HNET_CALL_CONNECT = 3;
constexpr uint32_t HNET_CALL_LISTEN = 4;
constexpr uint32_t HNET_CALL_ACCEPT = 5;
constexpr uint32_t HNET_CALL_SEND = 9;
constexpr uint32_t HNET_CALL_RECV = 10;
constexpr uint32_t HNET_CALL_SENDTO = 11;
constexpr uint32_t HNET_CALL_RECVFROM = 12;
constexpr uint32_t HNET_CALL_SHUTDOWN = 13;
// Hashx86 extension: resolve a hostname through lwIP's DNS client.
constexpr uint32_t HNET_CALL_RESOLVE = 30;

/**
 * NetSocketCall() - Dispatch one sys_socketcall request.
 * @call: Linux socketcall sub-operation.
 * @userArgs: User pointer to an array of 32-bit arguments.
 *
 * Return: The operation result, or a negative value on error.
 */
int32_t NetSocketCall(uint32_t call, uint32_t userArgs);

/**
 * NetSocketFdValid() - Test whether @fd names a socket in @process.
 */
bool NetSocketFdValid(ProcessControlBlock* process, uint32_t fd);

/**
 * NetCloseSocketFd() - Close a user-visible socket descriptor.
 *
 * Return: 0 on success, -1 when @fd is not a valid socket.
 */
int32_t NetCloseSocketFd(ProcessControlBlock* process, uint32_t fd);

/**
 * NetCloseProcessSockets() - Enqueue all of a dying process's sockets for close.
 *
 * Called from KillProcess with interrupts disabled; it must not call lwIP or
 * block. The RX thread drains the queue via NetReapClosedSockets().
 */
void NetCloseProcessSockets(ProcessControlBlock* process);

/**
 * NetReapClosedSockets() - Close queued sockets; called from the RX thread.
 */
void NetReapClosedSockets();

#endif  // CORE_NET_NET_SOCKET_H
