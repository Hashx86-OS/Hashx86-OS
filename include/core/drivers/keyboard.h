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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <core/driver.h>
#include <core/interrupts.h>
#include <core/ports.h>
#include <types.h>

/**
 * class KeyboardEventHandler - Base class for handling keyboard events.
 *
 * Provides virtual methods to handle key press and release events,
 * including special keys.
 */
class KeyboardEventHandler {
public:
    /**
     * KeyboardEventHandler() - Default constructor.
     */
    KeyboardEventHandler();

    /**
     * OnKeyDown() - Handle a key press event.
     * @key: Character for the pressed key.
     */
    virtual void OnKeyDown(const char* key);

    /**
     * OnSpecialKeyDown() - Handle a special key press event.
     * @key: Scancode of the pressed special key.
     */
    virtual void OnSpecialKeyDown(uint8_t key);

    /**
     * OnKeyUp() - Handle a key release event.
     * @key: Character for the released key.
     */
    virtual void OnKeyUp(const char* key);

    /**
     * OnSpecialKeyUp() - Handle a special key release event.
     * @key: Scancode of the released special key.
     */
    virtual void OnSpecialKeyUp(uint8_t key);
};

/**
 * class KeyboardDriver - Driver for managing keyboard input.
 *
 * Handles keyboard interrupts and integrates with the interrupt manager
 * to process key events.
 */
class KeyboardDriver : public InterruptHandler, public Driver {
    Port8Bit dataPort;                   // Port for reading keyboard data.
    Port8Bit commandPort;                // Port for sending commands to the keyboard.
    KeyboardEventHandler* eventHandler;  // Handler for keyboard events.
    uint8_t keyStates[128];              // Scancode-indexed key state (1=pressed, 0=released).

    static const uint16_t INPUT_QUEUE_SIZE = 256;
    char inputQueue[INPUT_QUEUE_SIZE];
    uint16_t inputHead;
    uint16_t inputTail;

    void QueueInputChar(char c);

public:
    static KeyboardDriver* activeInstance;

    // Return 1 if the key with the given scancode is currently pressed.
    uint8_t GetKeyState(uint8_t scancode) {
        return (scancode < 128) ? keyStates[scancode] : 0;
    }
    // Return a pointer to the full key state array (128 bytes).
    uint8_t* GetKeyStates() {
        return keyStates;
    }

    // Pop one character from the keyboard input queue.
    bool PopInputChar(char* out);

    // Clear all pending characters from the keyboard input queue.
    void ClearInputQueue();
    /**
     * KeyboardDriver() - Construct a keyboard driver.
     * @manager: Pointer to the interrupt manager.
     * @handler: Pointer to the keyboard event handler.
     */
    KeyboardDriver(InterruptManager* manager, KeyboardEventHandler* handler);

    /**
     * ~KeyboardDriver() - Destroy a keyboard driver.
     */
    ~KeyboardDriver();

    /**
     * Activate() - Activate the keyboard driver and initialize the hardware.
     */
    void Activate();

    /**
     * HandleInterrupt() - Handle keyboard interrupts and process key events.
     * @esp: Current stack pointer.
     *
     * Return: Updated stack pointer after handling the interrupt.
     */
    virtual uint32_t HandleInterrupt(uint32_t esp);
};

#endif  // KEYBOARD_H
