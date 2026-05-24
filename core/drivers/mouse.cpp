/**
 * @file        mouse.cpp
 * @brief       Generic Mouse Driver for #x86
 *
 * @date        19/01/2025
 * @version     1.0.2-beta
 */

#include <core/drivers/mouse.h>

MouseDriver* MouseDriver::activeInstance = nullptr;

/**
 * MouseEventHandler constructor
 */
MouseEventHandler::MouseEventHandler() {}

/**
 * Virtual method to handle MouseMove events
 */
void MouseEventHandler::OnMouseMove(int dx, int dy) {};

/**
 * Virtual method to handle MouseDown events
 */
void MouseEventHandler::OnMouseDown(uint8_t button) {}

/**
 * Virtual method to handle MouseUp events
 */
void MouseEventHandler::OnMouseUp(uint8_t button) {}

/**
 * @brief Constructs the MouseDriver object.
 *
 * @param manager Pointer to the interrupt manager.
 * @param handler Pointer to the mouse event handler.
 */
MouseDriver::MouseDriver(InterruptManager* manager, MouseEventHandler* handler)
    : InterruptHandler(0x2C, manager),  // IRQ12 for mouse
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
 * @brief Destructor for the MouseDriver.
 */
MouseDriver::~MouseDriver() {}

/**
 * @brief Activates the mouse driver and initializes the mouse hardware.
 */
void MouseDriver::Activate() {
    // Enable the mouse
    commandPort.Write(0xAB);  // Enable the auxiliary device (mouse)

    // Set mouse configuration
    commandPort.Write(0x20);               // Request current configuration byte
    uint8_t status = dataPort.Read() | 2;  // Enable IRQ12 (mouse interrupts)
    commandPort.Write(0x60);               // Set configuration byte
    dataPort.Write(status);

    // Enable mouse device
    commandPort.Write(0xD4);  // Signal the mouse device
    dataPort.Write(0xF4);     // Enable packet streaming
    dataPort.Read();          // Acknowledge response
    this->is_Active = true;
}

/**
 * @brief Handles mouse interrupts and processes mouse events.
 *
 * @param esp Current stack pointer.
 * @return Updated stack pointer after handling the interrupt.
 */
uint32_t MouseDriver::HandleInterrupt(uint32_t esp) {
    uint8_t status = commandPort.Read();
    // Only proceed if both AUX (bit 5 = 0x20) and output-buffer-full (bit 0 = 0x01) are set
    if ((status & 0x21) != 0x21) return esp;

    uint8_t data = dataPort.Read();
    if (eventHandler == 0) return esp;

    // PS/2 mouse 3-byte packet sync: byte 0 must have bit 3 set (always 1)
    if (offset == 0) {
        if (!(data & 0x08)) {
            // Lost sync — discard and stay at offset 0 waiting for a valid start byte
            return esp;
        }
    }

    buffer[offset] = data;
    offset = (offset + 1) % 3;

    if (offset == 0) {
        if (buffer[1] != 0 || buffer[2] != 0) {
            int32_t dx = (int8_t)buffer[1];
            int32_t dy = -((int8_t)buffer[2]);
            accumDX += dx;
            accumDY += dy;
            eventHandler->OnMouseMove(dx, dy);
        }

        for (uint8_t i = 0; i < 3; i++) {
            if ((buffer[0] & (0x1 << i)) != (buttons & (0x1 << i))) {
                if (buttons & (0x1 << i))
                    eventHandler->OnMouseUp(i + 1);
                else
                    eventHandler->OnMouseDown(i + 1);
            }
        }
        buttons = buffer[0];
    }

    return esp;
}
