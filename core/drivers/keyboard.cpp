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

#define KDBG_COMPONENT "KEYBOARD"
#include <core/Iguard.h>
#include <core/drivers/keyboard.h>
#include <core/memory.h>

KeyboardDriver* KeyboardDriver::activeInstance = nullptr;

// Modifier key states.
bool leftShiftPressed = false;
bool rightShiftPressed = false;
bool leftCtrlPressed = false;
bool rightCtrlPressed = false;
bool leftAltPressed = false;
bool rightAltPressed = false;
bool capsLockActive = false;

/**
 * KeyboardEventHandler::KeyboardEventHandler() - Default constructor.
 */
KeyboardEventHandler::KeyboardEventHandler() {}

/**
 * KeyboardEventHandler::OnKeyDown() - Called for a pressed character key.
 * @key: NUL-terminated single-character string.
 */
void KeyboardEventHandler::OnKeyDown(const char* key) {}

/**
 * KeyboardEventHandler::OnKeyUp() - Called for a released character key.
 * @key: NUL-terminated single-character string.
 */
void KeyboardEventHandler::OnKeyUp(const char* key) {}

/**
 * KeyboardEventHandler::OnSpecialKeyDown() - Called for a special key press.
 * @key: Raw scancode of the special key.
 */
void KeyboardEventHandler::OnSpecialKeyDown(uint8_t key) {}

/**
 * KeyboardEventHandler::OnSpecialKeyUp() - Called for a special key release.
 * @key: Raw scancode of the special key.
 */
void KeyboardEventHandler::OnSpecialKeyUp(uint8_t key) {}

/**
 * KeyboardDriver::KeyboardDriver() - Construct the keyboard driver.
 * @manager: Interrupt manager owning IRQ 1.
 * @handler: Event handler receiving key events.
 *
 * Creates the InterruptHandler (IRQ 0x21) with the data and command ports,
 * clears key and input-queue state, and publishes this instance as the
 * active keyboard driver.
 */
KeyboardDriver::KeyboardDriver(InterruptManager* manager, KeyboardEventHandler* handler)
    : InterruptHandler(0x21, manager), dataPort(0x60), commandPort(0x64) {
    this->eventHandler = handler;
    this->driverName = "Generic Keyboard Driver  ";
    memset(this->keyStates, 0, sizeof(this->keyStates));
    memset(this->inputQueue, 0, sizeof(this->inputQueue));
    this->inputHead = 0;
    this->inputTail = 0;
    activeInstance = this;
}

/**
 * KeyboardDriver::~KeyboardDriver() - Default destructor.
 */
KeyboardDriver::~KeyboardDriver() {}

/**
 * KeyboardDriver::QueueInputChar() - Buffer a character for later polling.
 * @c: Character to enqueue.
 *
 * Drops the oldest buffered character when the queue is full so the newest
 * input is preserved.
 */
void KeyboardDriver::QueueInputChar(char c) {
    uint16_t nextHead = (uint16_t)((inputHead + 1) % INPUT_QUEUE_SIZE);

    // Full queue: drop the oldest character to keep the latest input.
    if (nextHead == inputTail) {
        inputTail = (uint16_t)((inputTail + 1) % INPUT_QUEUE_SIZE);
    }

    inputQueue[inputHead] = c;
    inputHead = nextHead;
}

/**
 * KeyboardDriver::PopInputChar() - Dequeue a buffered character.
 * @out: Receives the character, written only on success.
 *
 * Context: Serialized by InterruptGuard.
 *
 * Return: True when a character was dequeued.
 */
bool KeyboardDriver::PopInputChar(char* out) {
    if (!out) return false;

    InterruptGuard guard;
    if (inputHead == inputTail) return false;

    *out = inputQueue[inputTail];
    inputTail = (uint16_t)((inputTail + 1) % INPUT_QUEUE_SIZE);
    return true;
}

/**
 * KeyboardDriver::ClearInputQueue() - Discard all buffered characters.
 *
 * Context: Serialized by InterruptGuard.
 */
void KeyboardDriver::ClearInputQueue() {
    InterruptGuard guard;
    inputHead = 0;
    inputTail = 0;
}

/**
 * WaitForKBACK() - Wait for an ACK from the keyboard controller.
 * @dataPort: Keyboard data port.
 * @commandPort: Keyboard command/status port.
 * @retries: Number of attempts before giving up.
 *
 * Return: True when a 0xFA ACK was received.
 */
static bool WaitForKBACK(Port8Bit& dataPort, Port8Bit& commandPort, int retries) {
    for (int i = 0; i < retries; i++) {
        for (int wait = 0; wait < 10000; wait++) {
            if (commandPort.Read() & 0x1) {
                uint8_t ack = dataPort.Read();
                if (ack == 0xFA) return true;
                if (ack == 0xFE) break;
                return false;
            }
        }
    }
    return false;
}

/**
 * KeyboardDriver::Activate() - Enable IRQ 1 and start keyboard scanning.
 *
 * Clears stale output, enables the interface, configures the controller
 * command byte for IRQ 1, and sends the enable-scanning command.
 */
void KeyboardDriver::Activate() {
    // Clear the keyboard output buffer.
    while (commandPort.Read() & 0x1) dataPort.Read();

    // Enable the keyboard interface (controller command, no ACK expected).
    commandPort.Write(0xAE);

    // Read the controller command byte.
    commandPort.Write(0x20);
    int bufReady = 0;
    for (int wait = 0; wait < 10000; wait++) {
        if (commandPort.Read() & 0x1) {
            bufReady = 1;
            break;
        }
    }
    if (!bufReady) {
        this->is_Active = false;
        return;
    }
    uint8_t status = (dataPort.Read() | 1) & ~0x10;  // Enable IRQ1, disable the key lock.
    // Write back the modified command byte (controller command, no ACK expected).
    commandPort.Write(0x60);
    dataPort.Write(status);

    // Activate keyboard scanning.
    dataPort.Write(0xF4);
    if (!WaitForKBACK(dataPort, commandPort, 3)) {
        this->is_Active = false;
        return;
    }
    this->is_Active = true;
}

/**
 * KeyboardDriver::HandleInterrupt() - Process a keyboard IRQ.
 * @esp: Stack pointer from the interrupt entry.
 *
 * Reads the scancode, tracks key state and modifier flags, and maps normal
 * scancodes to characters through the normal/shift tables, honoring Shift,
 * Caps Lock, and the extended (0xE0) prefix. Character keys go to the input
 * queue; special keys and character keys are reported to the event handler.
 *
 * Context: Runs on the IRQ 1 handler path.
 *
 * Return: The stack pointer, passed through unchanged.
 */
uint32_t KeyboardDriver::HandleInterrupt(uint32_t esp) {
    uint8_t key = dataPort.Read();

    static bool isExtendedScancode = false;
    static uint8_t keyStatesNormal[128] = {0};
    static uint8_t keyStatesExt[128] = {0};

    if (key == 0xE0) {
        isExtendedScancode = true;
        return esp;
    }

    // Normal (unshifted) scancode map.
    static const char normalKeyMap[128] = {
        0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9',  '0', '-', '=',  0,  // 0x00 - 0x0E
        0,   'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',  '[', ']', '\n', 0,  // 0x0F - 0x1D
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\',     // 0x1E - 0x2C
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*', 0,   ' ',      // 0x2D - 0x39
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,        // 0x3A - 0x48
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0         // 0x49 - 0x58
    };

    // Shifted scancode map.
    static const char shiftKeyMap[128] = {
        0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',  0,  // 0x00 - 0x0E
        0,   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,  // 0x0F - 0x1D
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|',      // 0x1E - 0x2C
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ',      // 0x2D - 0x39
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,        // 0x3A - 0x48
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0         // 0x49 - 0x58
    };

    if (isExtendedScancode) {
        isExtendedScancode = false;

        if (key < 0x80) {
            keyStatesExt[key] = 1;
            keyStates[key] = keyStatesNormal[key] || keyStatesExt[key];
        } else {
            uint8_t releaseScancode = key & 0x7F;
            if (releaseScancode < 128) {
                keyStatesExt[releaseScancode] = 0;
                keyStates[releaseScancode] =
                    keyStatesNormal[releaseScancode] || keyStatesExt[releaseScancode];
            }
        }

        // Always update the modifier state.
        switch (key) {
            case 0x1D:
                rightCtrlPressed = true;
                break;
            case 0x38:
                rightAltPressed = true;
                break;
            case 0x9D:
                rightCtrlPressed = false;
                break;
            case 0xB8:
                rightAltPressed = false;
                break;
        }
        // Invoke the callbacks only when a handler is set.
        if (this->eventHandler) {
            switch (key) {
                case 0x1D:
                case 0x38:
                case 0x48:
                case 0x50:
                case 0x4B:
                case 0x4D:
                case 0x53:
                    eventHandler->OnSpecialKeyDown(key);
                    break;
                case 0x9D:
                case 0xB8:
                case 0xC8:
                case 0xD0:
                case 0xCB:
                case 0xCD:
                case 0xD3:
                    eventHandler->OnSpecialKeyUp(key);
                    break;
            }
        }
        return esp;
    }

    // Normal (non-extended) scancode.
    if (key < 0x80) {
        keyStatesNormal[key] = 1;
        keyStates[key] = keyStatesNormal[key] || keyStatesExt[key];
        // Always update the modifier state.
        switch (key) {
            case 0x2A:
                leftShiftPressed = true;
                break;
            case 0x36:
                rightShiftPressed = true;
                break;
            case 0x1D:
                leftCtrlPressed = true;
                break;
            case 0x38:
                leftAltPressed = true;
                break;
            case 0x3A:
                capsLockActive = !capsLockActive;
                break;
        }
        // Invoke the callbacks only when a handler is set.
        if (this->eventHandler) {
            switch (key) {
                case 0x1C:  // Enter.
                    eventHandler->OnSpecialKeyDown(key);
                    QueueInputChar('\n');
                    break;
                case 0x0F:  // Tab.
                    eventHandler->OnSpecialKeyDown(key);
                    QueueInputChar('\t');
                    break;
                case 0x0E:  // Backspace.
                    eventHandler->OnSpecialKeyDown(key);
                    QueueInputChar('\b');
                    break;
                case 0x2A:
                case 0x36:
                case 0x1D:
                case 0x38:
                case 0x3A:
                case 0x01:
                case 0x3B:
                case 0x3C:
                case 0x3D:
                case 0x3E:
                case 0x3F:
                case 0x40:
                case 0x41:
                case 0x42:
                case 0x43:
                case 0x44:
                case 0x57:
                case 0x58:
                    eventHandler->OnSpecialKeyDown(key);
                    break;
                default:
                    if (key < 128) {
                        char character = normalKeyMap[key];
                        bool shiftPressed = leftShiftPressed || rightShiftPressed;
                        if (character >= 'a' && character <= 'z') {
                            if (shiftPressed ^ capsLockActive) character = shiftKeyMap[key];
                        } else {
                            if (shiftPressed) character = shiftKeyMap[key];
                        }
                        if (character != 0) {
                            QueueInputChar(character);
                            char keyStr[2] = {character, '\0'};
                            eventHandler->OnKeyDown(keyStr);
                        }
                    }
                    break;
            }
        }
    } else {
        uint8_t releaseScancode = key & 0x7F;
        if (releaseScancode < 128) {
            keyStatesNormal[releaseScancode] = 0;
            keyStates[releaseScancode] =
                keyStatesNormal[releaseScancode] || keyStatesExt[releaseScancode];
        }
        // Always update the modifier state.
        switch (key) {
            case 0xAA:
                leftShiftPressed = false;
                break;
            case 0xB6:
                rightShiftPressed = false;
                break;
            case 0x9D:
                leftCtrlPressed = false;
                break;
            case 0xB8:
                leftAltPressed = false;
                break;
        }
        // Invoke the callbacks only when a handler is set.
        if (this->eventHandler) {
            switch (key) {
                case 0x9C:
                case 0xAA:
                case 0xB6:
                case 0x9D:
                case 0xB8:
                case 0x8F:
                case 0x8E:
                case 0x81:
                case 0xBB:
                case 0xBC:
                case 0xBD:
                case 0xBE:
                case 0xBF:
                case 0xC0:
                case 0xC1:
                case 0xC2:
                case 0xC3:
                case 0xC4:
                case 0xD7:
                case 0xD8:
                    eventHandler->OnSpecialKeyUp(key);
                    break;
            }
        }
    }

    return esp;
}
