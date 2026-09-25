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

#include <core/drivers/mouse.h>

MouseDriver* MouseDriver::activeInstance = nullptr;

/**
 * MouseEventHandler::MouseEventHandler() - Default constructor.
 */
MouseEventHandler::MouseEventHandler() {}

/**
 * MouseEventHandler::OnMouseMove() - Called when the mouse moves.
 * @dx: Horizontal delta.
 * @dy: Vertical delta.
 */
void MouseEventHandler::OnMouseMove(int dx, int dy) {};

/**
 * MouseEventHandler::OnMouseDown() - Called when a button is pressed.
 * @button: Button number (1-based).
 */
void MouseEventHandler::OnMouseDown(uint8_t button) {}

/**
 * MouseEventHandler::OnMouseUp() - Called when a button is released.
 * @button: Button number (1-based).
 */
void MouseEventHandler::OnMouseUp(uint8_t button) {}

/**
 * MouseDriver::MouseDriver() - Construct the mouse driver.
 * @manager: Interrupt manager owning IRQ 12.
 * @handler: Event handler receiving mouse events.
 *
 * Creates the InterruptHandler (IRQ 0x2C) with the PS/2 data and command
 * ports and publishes this instance as the active mouse driver.
 */
MouseDriver::MouseDriver(InterruptManager* manager, MouseEventHandler* handler)
    : InterruptHandler(0x2C, manager),  // IRQ12 for the mouse.
      dataPort(0x60),
      commandPort(0x64) {
    this->eventHandler = handler;
    this->driverName = "Generic Mouse Driver     ";
    this->offset = 0;
    this->buttons = 0;
    this->accumDX = 0;
    this->accumDY = 0;
    activeInstance = this;
}

/**
 * MouseDriver::~MouseDriver() - Default destructor.
 */
MouseDriver::~MouseDriver() {}

/**
 * WaitForInputBufferClear() - Wait until the PS/2 input buffer is empty.
 * @commandPort: PS/2 command/status port.
 *
 * Return: True when the controller may accept a command.
 */
static bool WaitForInputBufferClear(Port8Bit& commandPort) {
    for (int wait = 0; wait < 10000; wait++) {
        if (!(commandPort.Read() & 0x2)) return true;
    }
    return false;
}

/**
 * WaitForOutputBufferFull() - Wait until the PS/2 output buffer has data.
 * @commandPort: PS/2 command/status port.
 *
 * Return: True when data can be read.
 */
static bool WaitForOutputBufferFull(Port8Bit& commandPort) {
    for (int wait = 0; wait < 10000; wait++) {
        if (commandPort.Read() & 0x1) return true;
    }
    return false;
}

/**
 * MouseDriver::Activate() - Enable the mouse and start packet streaming.
 *
 * Enables the auxiliary device, programs the controller command byte for
 * IRQ12, sends the enable-streaming command, and consumes the ACK.
 */
void MouseDriver::Activate() {
    // Enable the mouse.
    if (!WaitForInputBufferClear(commandPort)) {
        this->is_Active = false;
        return;
    }
    commandPort.Write(0xA8);  // Enable the auxiliary device (mouse).

    // Set the controller configuration.
    if (!WaitForInputBufferClear(commandPort)) {
        this->is_Active = false;
        return;
    }
    commandPort.Write(0x20);  // Request the current configuration byte.
    if (!WaitForOutputBufferFull(commandPort)) {
        this->is_Active = false;
        return;
    }
    uint8_t status = dataPort.Read() | 2;  // Enable IRQ12 (mouse interrupts).
    if (!WaitForInputBufferClear(commandPort)) {
        this->is_Active = false;
        return;
    }
    commandPort.Write(0x60);  // Set the configuration byte.
    if (!WaitForInputBufferClear(commandPort)) {
        this->is_Active = false;
        return;
    }
    dataPort.Write(status);

    // Enable the mouse device.
    if (!WaitForInputBufferClear(commandPort)) {
        this->is_Active = false;
        return;
    }
    commandPort.Write(0xD4);  // Signal the mouse device.
    if (!WaitForInputBufferClear(commandPort)) {
        this->is_Active = false;
        return;
    }
    dataPort.Write(0xF4);  // Enable packet streaming.
    if (!WaitForOutputBufferFull(commandPort)) {
        this->is_Active = false;
        return;
    }
    dataPort.Read();  // Ack response.
    this->is_Active = true;
}

/**
 * MouseDriver::HandleInterrupt() - Process a PS/2 mouse IRQ.
 * @esp: Stack pointer from the interrupt entry.
 *
 * Synchronizes the 3-byte packet stream, accumulates per-IRQ deltas, and
 * reports movement and button transitions to the event handler.
 *
 * Context: Runs on the IRQ 12 handler path.
 *
 * Return: The stack pointer, passed through unchanged.
 */
uint32_t MouseDriver::HandleInterrupt(uint32_t esp) {
    uint8_t status = commandPort.Read();
    // Proceed only when both AUX data (bit 5 = 0x20) and output-buffer-full
    // (bit 0 = 0x01) are set.
    if ((status & 0x21) != 0x21) return esp;

    uint8_t data = dataPort.Read();
    if (eventHandler == 0) return esp;

    // PS/2 3-byte packet sync: byte 0 must have bit 3 set (always 1).
    if (offset == 0) {
        if (!(data & 0x08)) {
            // Lost sync: discard and stay at offset 0 awaiting a valid start byte.
            return esp;
        }
    }

    buffer[offset] = data;
    offset = (offset + 1) % 3;

    if (offset == 0) {
        if (buffer[1] != 0 || buffer[2] != 0) {
            int32_t dx = (buffer[0] & 0x40) ? 0 : (int8_t)buffer[1];
            int32_t dy = (buffer[0] & 0x80) ? 0 : -((int8_t)buffer[2]);
            accumDX += dx;
            accumDY += dy;
            eventHandler->OnMouseMove(dx, dy);
        }

        uint8_t btn_state = buffer[0] & 0x7;
        for (uint8_t i = 0; i < 3; i++) {
            if ((btn_state & (0x1 << i)) != (buttons & (0x1 << i))) {
                if (buttons & (0x1 << i))
                    eventHandler->OnMouseUp(i + 1);
                else
                    eventHandler->OnMouseDown(i + 1);
            }
        }
        buttons = btn_state;
    }

    return esp;
}
