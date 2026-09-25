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

#ifndef PROGRAM_H
#define PROGRAM_H

#include <Hx86/stdint.h>
#include <Hx86/utils/string.h>

/**
 * typedef constructor - Pointer to a function taking no arguments.
 *
 * Used to reference global constructors during initialization.
 */
typedef void (*constructor)();

/**
 * start_ctors, end_ctors - Bounds of the global constructor section.
 *
 * These symbols are defined by the linker and mark the range of global
 * constructors to call during initialization.
 */
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;

/**
 * callConstructors() - Call every global constructor in [start_ctors, end_ctors).
 *
 * Called during initialization so that all static and global objects are
 * properly constructed.
 */
extern "C" void callConstructors() {
    for (constructor* i = &start_ctors; i != &end_ctors; i++) {
        (*i)();  // Call each constructor in the range.
    }
}

#endif  // PROGRAM_H
