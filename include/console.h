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

#ifndef CONSOLE_H
#define CONSOLE_H

#include <core/ports.h>
#include <debug.h>
#include <stdarg.h>

// Base address for video memory in text mode.
#define VIDEO_MEMORY_ADDRESS 0xb8000

// Screen dimensions for text mode (80x25 characters).
#define SCREEN_WIDTH 80   // Number of columns on the screen.
#define SCREEN_HEIGHT 25  // Number of rows on the screen.

/**
 * enum TextColor - Foreground and background color codes for text mode.
 * @BLACK: Color index 0x0.
 * @BLUE: Color index 0x1.
 * @GREEN: Color index 0x2.
 * @CYAN: Color index 0x3.
 * @RED: Color index 0x4.
 * @MAGENTA: Color index 0x5.
 * @BROWN: Color index 0x6.
 * @LIGHT_GRAY: Color index 0x7.
 * @DARK_GRAY: Color index 0x8.
 * @LIGHT_BLUE: Color index 0x9.
 * @LIGHT_GREEN: Color index 0xA.
 * @LIGHT_CYAN: Color index 0xB.
 * @LIGHT_RED: Color index 0xC.
 * @LIGHT_MAGENTA: Color index 0xD.
 * @YELLOW: Color index 0xE.
 * @WHITE: Color index 0xF.
 */
typedef enum {
    BLACK = 0x0,
    BLUE = 0x1,
    GREEN = 0x2,
    CYAN = 0x3,
    RED = 0x4,
    MAGENTA = 0x5,
    BROWN = 0x6,
    LIGHT_GRAY = 0x7,
    DARK_GRAY = 0x8,
    LIGHT_BLUE = 0x9,
    LIGHT_GREEN = 0xA,
    LIGHT_CYAN = 0xB,
    LIGHT_RED = 0xC,
    LIGHT_MAGENTA = 0xD,
    YELLOW = 0xE,
    WHITE = 0xF
} TextColor;

/**
 * printf() - Print formatted text on the screen in the given color.
 * @color: Text color for the output.
 * @format: printf-style format string.
 * @...: Arguments referenced by the format string.
 */
void printf(TextColor color, const char* format, ...);

/**
 * MSGPrintf() - Print a tagged message with a custom tag color.
 * @cTag: Color used for the printed tag.
 * @tag: Tag identifying the module or context.
 * @format: printf-style format string.
 * @...: Arguments referenced by the format string.
 */
void MSGPrintf(TextColor cTag, const char* tag, const char* format, ...);

/**
 * clearScreen() - Clear the screen and reset the cursor to the origin.
 */
void clearScreen();

/**
 * scrollScreen() - Scroll all text rows up by one and clear the last row.
 */
void scrollScreen();

/**
 * updateCursor() - Program the VGA hardware cursor position.
 * @row: New cursor row.
 * @col: New cursor column.
 */
void updateCursor(int row, int col);
void scrollScreen();

/**
 * combineColors() - Merge foreground and background colors into one byte.
 * @foreground: Foreground color code in the low nibble.
 * @background: Background color code in the high nibble.
 *
 * Return: The combined color byte.
 */
TextColor combineColors(TextColor foreground, TextColor background);

#endif  // CONSOLE_H
