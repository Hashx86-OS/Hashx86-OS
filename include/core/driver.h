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

#ifndef DRIVER_H
#define DRIVER_H

#include <core/drivers/ModuleLoader.h>
#include <core/pci.h>
#include <debug.h>
#include <types.h>

class AudioDriver;

/**
 * class Driver - Base class for hardware drivers.
 *
 * Provides a generic interface for drivers, including methods for
 * activation, deactivation and resetting, and holds the name of the driver.
 */
class Driver {
    friend class DriverManager;

public:
    const char* driverName;  // Human-readable driver name.

    inline Driver() {
        driverName = "Unknown";
    }

    inline virtual ~Driver() {}

    inline virtual void Activate() {}

    inline virtual int Reset() {
        return 0;
    }

    inline virtual void Deactivate() {}

    inline void SetName(const char* name) {
        driverName = name;
    }

    virtual AudioDriver* AsAudioDriver() {
        return 0;
    }
    virtual GraphicsDriver* AsGraphicsDriver() {
        return 0;
    }

protected:
    bool is_Active = false;  // Runtime activation state.
};

class DriverManager {
private:
    Driver* drivers[255];  // Array holding up to 255 registered drivers.
    int numDrivers;        // Number of drivers currently registered.

public:
    /**
     * DriverManager() - Construct a driver manager with no registered drivers.
     */
    DriverManager();

    /**
     * AddDriver() - Register a driver with the manager.
     * @driver: Pointer to the driver to be added.
     */
    void AddDriver(Driver* driver);

    /**
     * ActivateAll() - Activate all registered drivers.
     */
    void ActivateAll();
};

// --- DYNAMIC LINKING EXTENSION ---

// GetDriverInstancePtr() - Factory entry point for a dynamically linked driver.
typedef Driver* (*GetDriverInstancePtr)();

#define DYNAMIC_DRIVER(ClassName)               \
    extern "C" Driver* CreateDriverInstance() { \
        return new ClassName();                 \
    }

#endif
