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

#ifndef GUI_ABI_H
#define GUI_ABI_H

#include <stdint.h>

/**
 * enum REQ_Element - Widget type codes used by the GUI syscall interface.
 *
 * Each code selects which widget class the kernel instantiates or targets for
 * a syscall request. Shared verbatim between the kernel handler and the
 * userspace GUI library; the values are a frozen ABI.
 */
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
    DISPLAY = 0xA,
} REQ_Element;

/**
 * enum REQ_MODE - Operation codes for the GUI syscall interface.
 *
 * Each code selects the operation applied to the widget named by REQ_Element.
 * Shared verbatim between the kernel handler and the userspace GUI library;
 * the values are a frozen ABI.
 */
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
    GET_MODE_COUNT = 0x17,
    GET_MODE = 0x18,
    GET_CURRENT_MODE = 0x19,
    SET_MODE = 0x1A,
    SET_SELECTED = 0x1B,
    ADD_COLUMN = 0x1C,
} REQ_MODE;

/** GUI_LISTVIEW_MAX_COLUMNS - Maximum columns a list view accepts. */
#define GUI_LISTVIEW_MAX_COLUMNS 4

/** GUI_LISTVIEW_CELL - Maximum length of one list view cell string. */
#define GUI_LISTVIEW_CELL 64

/**
 * struct ListViewItemData - One list view row in the syscall payload.
 * @cells: Text for each column, indexed by column order.
 * @type: Leading icon type (0 file, 1 directory, 2 executable).
 */
struct ListViewItemData {
    char cells[GUI_LISTVIEW_MAX_COLUMNS][GUI_LISTVIEW_CELL];
    uint8_t type;
};

/**
 * struct WidgetData - Register window for the GUI syscall.
 * @param0: Target widget ID.
 * @param1: Optional signed parameter (geometry, font size, ...).
 * @param2: Optional signed parameter (alignment, ...).
 * @param3: Optional unsigned parameter.
 * @param4: Optional unsigned parameter.
 * @param5: Optional string input.
 * @param6: Optional string output buffer.
 * @param7: Optional string output buffer.
 */
struct WidgetData {
    uint32_t param0;
    int32_t param1;
    int32_t param2;
    uint32_t param3;
    uint32_t param4;
    const char* param5;
    char* param6;
    char* param7;
};

#endif  // GUI_ABI_H
