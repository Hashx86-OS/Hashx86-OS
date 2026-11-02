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

#ifndef GUI_INPUT_EVENT_H
#define GUI_INPUT_EVENT_H

#include <types.h>

/** enum INPUT_EVENT_TYPE - Kinds of input event the GUI routes to widgets. */
typedef enum {
    IE_NONE = 0,
    IE_MOUSE_DOWN,
    IE_MOUSE_UP,
    IE_MOUSE_MOVE,
    IE_MOUSE_WHEEL,
    IE_MOUSE_DOUBLE_CLICK,
    IE_KEY_DOWN,
    IE_KEY_UP,
    IE_KEY_REPEAT,
    IE_SPECIAL_DOWN,
    IE_SPECIAL_UP,
    IE_FOCUS_TAB,
} INPUT_EVENT_TYPE;

/** Modifier bits carried by key events. */
#define MOD_CTRL 0x1
#define MOD_SHIFT 0x2
#define MOD_ALT 0x4

/**
 * struct InputEvent - A single decoded input event.
 * @type: INPUT_EVENT_TYPE.
 * @modifiers: MOD_* bitmask.
 * @x: Pointer X in desktop coordinates (unused for wheel events).
 * @y: Pointer Y, or the wheel delta when @type is IE_MOUSE_WHEEL.
 * @button: 1=left, 2=middle, 3=right.
 * @key: NUL-terminated character key, or empty for non-character events.
 * @special: Special keycode, or 0.
 * @repeatCount: Repeat index for IE_KEY_REPEAT.
 */
struct InputEvent {
    uint8_t type;
    uint8_t modifiers;
    int32_t x;
    int32_t y;
    uint8_t button;
    char key[4];
    uint8_t special;
    uint32_t repeatCount;
};

#endif  // GUI_INPUT_EVENT_H
