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

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <core/drivers/AudioMixer.h>
#include <core/drivers/GraphicsDriver.h>
#include <core/gdt.h>
#include <core/globals.h>
#include <core/paging.h>
#include <core/ports.h>
#include <core/scheduler.h>
#include <core/timing.h>
#include <debug.h>
#include <gui/bmp.h>
#include <gui/config/config.h>
#include <string.h>
#include <types.h>

/**
 * class InterruptManager - Forward declaration.
 *
 * Lets InterruptHandler reference InterruptManager without including its full
 * definition at this point.
 */
class InterruptManager;

/**
 * class InterruptHandler - Base class for hardware interrupt handlers.
 *
 * Provides a common interface for handling interrupts and manages the
 * association with an InterruptManager.
 */
class InterruptHandler {
protected:
    uint8_t InterruptNumber;             // Interrupt vector associated with this handler.
    InterruptManager* interruptManager;  // InterruptManager that owns this handler.

    /**
     * InterruptHandler() - Bind a handler to an interrupt vector and manager.
     * @InterruptNumber: The interrupt number to handle.
     * @interruptManager: Pointer to the InterruptManager managing this handler.
     */
    InterruptHandler(uint8_t InterruptNumber, InterruptManager* interruptManager);

    /**
     * ~InterruptHandler() - Release the handler from its managing InterruptManager.
     */
    ~InterruptHandler();

public:
    /**
     * HandleInterrupt() - Handle an interrupt.
     * @esp: Stack pointer at the time of the interrupt.
     *
     * Subclasses should override this method to provide specific handling logic.
     *
     * Return: The new stack pointer after the interrupt is handled.
     */
    virtual uint32_t HandleInterrupt(uint32_t esp);
};

/**
 * class InterruptManager - Manage the handling of hardware interrupts.
 *
 * Sets up and manages the Interrupt Descriptor Table (IDT), the Programmable
 * Interrupt Controllers (PIC), and the associated interrupt handlers.
 */
class InterruptManager {
    friend class InterruptHandler;  // Allows InterruptHandler access to private/protected members.

public:
    static InterruptManager*
        activeInstance;  // Pointer to the currently active InterruptManager instance.

protected:
    InterruptHandler* handlers[256];  // Array of handlers, one per interrupt vector.
    Scheduler* scheduler;
    Paging* pager;
    Bitmap* panicImg;

    /**
     * struct GateDescriptor - An entry in the Interrupt Descriptor Table (IDT).
     * @handlerAddressLowBits: Lower 16 bits of the interrupt handler's address.
     * @gdt_codeSegmentSelector: Code segment selector in the Global Descriptor Table.
     * @reserved: Reserved field, must be zero.
     * @access: Access flags for the interrupt gate.
     * @handlerAddressHighBits: Upper 16 bits of the interrupt handler's address.
     */
    struct GateDescriptor {
        uint16_t handlerAddressLowBits;    // Lower 16 bits of the handler's address.
        uint16_t gdt_codeSegmentSelector;  // Code segment selector in the GDT.
        uint8_t reserved;                  // Reserved field, must be zero.
        uint8_t access;                    // Access flags for the interrupt gate.
        uint16_t handlerAddressHighBits;   // Upper 16 bits of the handler's address.
    } __attribute__((packed));             // Packed to ensure correct memory layout.

    static GateDescriptor interruptDescriptorTable[256];  // The Interrupt Descriptor Table (IDT).

    /**
     * struct InterruptDescriptorTablePointer - Pointer structure for the lidt instruction.
     * @size: Size of the IDT in bytes.
     * @base: Base address of the IDT.
     */
    struct InterruptDescriptorTablePointer {
        uint16_t size;          // Size of the IDT in bytes.
        uint32_t base;          // Base address of the IDT.
    } __attribute__((packed));  // Packed to ensure correct memory layout.

    /**
     * SetInterruptDescriptorTableEntry() - Configure one interrupt gate in the IDT.
     * @InterruptNumber: The interrupt number to configure.
     * @codeSegmentSelectorOffset: Code segment selector in the GDT.
     * @handler: Pointer to the interrupt handler function.
     * @DescriptorPrivilegeLevel: Privilege level (0-3) for accessing this interrupt.
     * @DescriptorType: Type of descriptor (e.g. interrupt gate).
     */
    static void SetInterruptDescriptorTableEntry(uint8_t InterruptNumber,
                                                 uint16_t codeSegmentSelectorOffset,
                                                 void (*handler)(),
                                                 uint8_t DescriptorPrivilegeLevel,
                                                 uint8_t DescriptorType);

    // Static methods to handle specific interrupts.
    static void IgnoreInterruptRequest();  // Handles ignored interrupts.
    static void HandleInterruptRequest0x00();
    static void HandleInterruptRequest0x01();
    static void HandleInterruptRequest0x02();
    static void HandleInterruptRequest0x03();
    static void HandleInterruptRequest0x04();
    static void HandleInterruptRequest0x05();
    static void HandleInterruptRequest0x06();
    static void HandleInterruptRequest0x07();
    static void HandleInterruptRequest0x08();
    static void HandleInterruptRequest0x09();
    static void HandleInterruptRequest0x0A();
    static void HandleInterruptRequest0x0B();
    static void HandleInterruptRequest0x0C();
    static void HandleInterruptRequest0x0D();
    static void HandleInterruptRequest0x0E();
    static void HandleInterruptRequest0x0F();
    static void HandleInterruptRequest0x31();

    static void HandleInterruptRequest0x80();
    static void HandleInterruptRequest0x81();

    static void HandleException0x00();
    static void HandleException0x01();
    static void HandleException0x02();
    static void HandleException0x03();
    static void HandleException0x04();
    static void HandleException0x05();
    static void HandleException0x06();
    static void HandleException0x07();
    static void HandleException0x08();
    static void HandleException0x09();
    static void HandleException0x0A();
    static void HandleException0x0B();
    static void HandleException0x0C();
    static void HandleException0x0D();
    static void HandleException0x0E();
    static void HandleException0x0F();
    static void HandleException0x10();
    static void HandleException0x11();
    static void HandleException0x12();
    static void HandleException0x13();

    // I/O ports for the Programmable Interrupt Controller (PIC).
    Port8BitSlow picMasterCommand;  // Command port for the master PIC.
    Port8BitSlow picMasterData;     // Data port for the master PIC.
    Port8BitSlow picSlaveCommand;   // Command port for the slave PIC.
    Port8BitSlow picSlaveData;      // Data port for the slave PIC.

public:
    /**
     * InterruptManager() - Set up the IDT and initialize the PIC.
     * @scheduler: Scheduler that owns the interrupt-driven context switches.
     * @pager: Paging instance used for address-space handling.
     *
     * Programs every IDT slot with a default handler and registers the PIC
     * ports, making this instance the active manager.
     */
    InterruptManager(Scheduler* scheduler, Paging* pager);

    /**
     * ~InterruptManager() - Destroy the active interrupt manager.
     */
    ~InterruptManager();

    /**
     * Activate() - Make this instance the active manager and enable interrupts.
     */
    void Activate();

    /**
     * Deactivate() - Disable interrupts and clear the active manager.
     */
    void Deactivate();

    /**
     * handleInterrupt() - Dispatch an interrupt to the active manager.
     * @interruptNumber: The interrupt number that occurred.
     * @esp: Stack pointer at the time of the interrupt.
     *
     * Return: The new stack pointer after the interrupt is handled.
     */
    static uint32_t handleInterrupt(uint8_t interruptNumber, uint32_t esp);

    /**
     * handleException() - Dispatch an exception to the active manager.
     * @interruptNumber: The exception number that occurred.
     * @esp: Stack pointer at the time of the exception.
     *
     * Return: The new stack pointer after the exception is handled.
     */
    static uint32_t handleException(uint8_t interruptNumber, uint32_t esp);

    /**
     * DoHandleInterrupt() - Handle an interrupt on this manager instance.
     * @interruptNumber: The interrupt number that occurred.
     * @esp: Stack pointer at the time of the interrupt.
     *
     * Dispatches the interrupt to the appropriate handler.
     *
     * Return: The new stack pointer after the interrupt is handled.
     */
    uint32_t DoHandleInterrupt(uint8_t interruptNumber, uint32_t esp);

    /**
     * DohandleException() - Handle an exception on this manager instance.
     * @interruptNumber: The exception number that occurred.
     * @esp: Stack pointer at the time of the exception.
     *
     * Dispatches the exception to the appropriate handler.
     *
     * Return: The new stack pointer after the exception is handled.
     */
    uint32_t DohandleException(uint8_t interruptNumber, uint32_t esp);
};

#endif
