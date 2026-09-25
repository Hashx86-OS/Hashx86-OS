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

/** @typedef constructor - Pointer to a no-argument, no-return function.
 *
 * Used to reference global constructors during initialization.
 */
typedef void (*constructor)();

// Linker-provided bounds of the global constructors section.
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;

/** callConstructors() - Call every global constructor between start_ctors and end_ctors.
 *
 * Invoked during initialization so that all static/global objects are properly
 * constructed before main() runs.
 */
extern "C" void callConstructors() {
    for (constructor* i = &start_ctors; i != &end_ctors; i++) {
        (*i)();  // Call each constructor in the range.
    }
}

/** class Calculator - Simple GUI calculator application. */
class Calculator {
public:
    Calculator();
    ~Calculator();

    void onPressNum(uint32_t num);
    void onPressFunc(char func);
    void evaluate();
    void clearCalculator();

private:
    Window* mainWindow;
    Label* screen;
    Button* btn_0;
    Button* btn_1;
    Button* btn_2;
    Button* btn_3;
    Button* btn_4;
    Button* btn_5;
    Button* btn_6;
    Button* btn_7;
    Button* btn_8;
    Button* btn_9;
    Button* btn_dot;
    Button* btn_plus;
    Button* btn_minus;
    Button* btn_multiplication;
    Button* btn_division;
    Button* btn_solve;
    Button* btn_clear;

    char input[64] = {0};
    int inputIndex = 0;
    double currentValue = 0;
    char lastOperator = 0;
    bool newInput = true;
    bool hasDecimal = false;
    bool hasResult = false;
};

#endif  // PROGRAM_H
