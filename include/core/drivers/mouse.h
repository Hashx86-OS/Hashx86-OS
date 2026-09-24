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

#ifndef MOUSE_H
#define MOUSE_H

#include <core/driver.h>
#include <core/interrupts.h>
#include <core/ports.h>
#include <types.h>

/**
 * class MouseEventHandler - Base class for handling mouse events.
 *
 * Provides a set of virtual methods that can be overridden to handle various
 * mouse events such as movement, button presses and scrolling.
 */
class MouseEventHandler {
public:
    MouseEventHandler();

    /**
     * OnMouseMove() - Called when the mouse is moved.
     * @dx: Change in X coordinate.
     * @dy: Change in Y coordinate.
     */
    virtual void OnMouseMove(int dx, int dy);

    /**
     * OnMouseDown() - Called when a mouse button is pressed.
     * @button: The pressed button.
     */
    virtual void OnMouseDown(uint8_t button);

    /**
     * OnMouseUp() - Called when a mouse button is released.
     * @button: The released button.
     */
    virtual void OnMouseUp(uint8_t button);
};

/**
 * class MouseDriver - Driver for handling mouse hardware and events.
 *
 * Interfaces with the mouse hardware through ports, processes interrupts and
 * notifies the event handler of mouse events.
 */
class MouseDriver : public InterruptHandler, public Driver {
    Port8Bit dataPort;                // Port for reading mouse data.
    Port8Bit commandPort;             // Port for sending commands to the mouse.
    MouseEventHandler* eventHandler;  // Handler for mouse events.
    uint8_t buffer[3];                // Buffer for storing mouse packet data.
    uint8_t offset;                   // Current offset in the buffer.
    uint8_t buttons;                  // Current state of the mouse buttons.
    int8_t x = 40, y = 12;            // Cursor position.
    int32_t accumDX;                  // Accumulated mouse X delta for polling.
    int32_t accumDY;                  // Accumulated mouse Y delta for polling.

public:
    static MouseDriver* activeInstance;

    // Get and reset accumulated mouse delta since last poll.
    void GetMouseDelta(int32_t& dx, int32_t& dy) {
        dx = accumDX;
        dy = accumDY;
        accumDX = 0;
        accumDY = 0;
    }
    // Get the current button state (bit 0=left, 1=right, 2=middle).
    uint8_t GetButtons() {
        return buttons;
    }
    /**
     * MouseDriver() - Construct a mouse driver.
     * @manager: Pointer to the interrupt manager.
     * @handler: Pointer to the mouse event handler.
     */
    MouseDriver(InterruptManager* manager, MouseEventHandler* handler);

    /**
     * ~MouseDriver() - Destroy a mouse driver.
     */
    ~MouseDriver();

    /**
     * Activate() - Activate the mouse driver and initialize the hardware.
     */
    void Activate();

    /**
     * HandleInterrupt() - Handle mouse interrupts and process mouse packets.
     * @esp: Current stack pointer.
     *
     * Return: Updated stack pointer after handling the interrupt.
     */
    virtual uint32_t HandleInterrupt(uint32_t esp);
};

#endif  // MOUSE_H
