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

#ifndef CORE_NET_NET_DEVICE_H
#define CORE_NET_NET_DEVICE_H

#include <stdint.h>

/**
 * typedef NetworkReceiveHandler - Callback for a received Ethernet frame.
 * @context: Opaque value supplied by SetReceiveHandler().
 * @frame: Frame bytes, starting at the destination MAC address.
 * @length: Frame length in bytes (FCS already removed).
 *
 * Invoked from the network thread, never from interrupt context, so the
 * handler may allocate pbufs and call into lwIP.
 */
typedef void (*NetworkReceiveHandler)(void* context, const uint8_t* frame, uint16_t length);

/**
 * class NetworkDevice - Interface implemented by Ethernet NIC drivers.
 *
 * The lwIP glue layer depends only on this interface; a driver owns its DMA
 * rings, interrupt handler and hardware registers. All methods are called
 * from kernel (non-interrupt) context: the boot thread during bring-up, then
 * the single network thread.
 */
class NetworkDevice {
public:
    virtual ~NetworkDevice() {}

    /**
     * SendFrame() - Transmit one Ethernet frame.
     * @frame: Frame bytes, starting at the destination MAC address.
     * @length: Frame length without the FCS.
     *
     * Return: True when the frame was queued for transmission.
     */
    virtual bool SendFrame(const uint8_t* frame, uint16_t length) = 0;

    /**
     * GetMac() - Return the device's 6-byte hardware address.
     */
    virtual const uint8_t* GetMac() const = 0;

    /**
     * SetReceiveHandler() - Install the frame-receive callback.
     * @handler: Function to call for each received frame (may be null).
     * @context: Opaque value passed back to @handler.
     */
    virtual void SetReceiveHandler(NetworkReceiveHandler handler, void* context) = 0;

    /**
     * Poll() - Drain pending receive descriptors and process TX completions.
     *
     * Drivers that use interrupts only record the event in their IRQ handler
     * and do the actual work here, keeping lwIP off interrupt context.
     */
    virtual void Poll() = 0;
};

#endif  // CORE_NET_NET_DEVICE_H
