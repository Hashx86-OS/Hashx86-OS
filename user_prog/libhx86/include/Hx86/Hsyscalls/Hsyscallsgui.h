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

#ifndef HSYSCALLSGUI_H
#define HSYSCALLSGUI_H

#include <Hx86/stdint.h>

/** REQ_Element - Kernel-side GUI element types addressable via int 0x81. */
typedef enum {
    WIDGET = 0x0,
    WINDOW = 0x1,
    BUTTON = 0x2,
    EVENT = 0x3,
    DESKTOP = 0x4,
    LABEL = 0x5,
    LISTVIEW = 0x6,
    TERMINAL_VIEW = 0x7,
    FONT = 0x8,
    ICON_BUTTON = 0x9,
} REQ_Element;

/** REQ_MODE - Operations that can be requested for a given element. */
typedef enum {
    CREATE = 0x0,
    ADD_CHILD = 0x1,
    REMOVE_CHILD = 0x2,
    DELETE = 0x3,
    GET = 0x4,
    SET_TEXT = 0x5,
    SET_FONT_SIZE = 0x6,
    SET_ITEMS = 0x7,
    CLEAR_ITEMS = 0x8,
    GET_SELECTED = 0x9,
    SET_SCROLL_META = 0xA,
    GET_SCROLL_ACTION = 0xB,
    MEASURE_TEXT = 0xC,
    SET_FONT_TYPE = 0xD,
    SET_COLOR = 0xE,
    SET_BACKGROUND = 0xF,
    SET_ALIGNMENT = 0x10,
    SET_WIDTH = 0x11,
    SET_HEIGHT = 0x12,
    SET_ITEM_HEIGHT = 0x13,
    SET_ENABLED = 0x14,
    SET_ICON = 0x15,
    SET_ICON_FONT_SIZE = 0x16,
} REQ_MODE;

uint32_t HguiAPI(REQ_Element element, REQ_MODE mode, void* data);

#endif  // HSYSCALLSGUI_H
