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

#include <Hx86/Hgui/display.h>
#include <Hx86/Hgui/widget.h>
#include <Hx86/Hsyscalls/Hsyscallsgui.h>

/**
 * unpackMode() - Split the packed geometry word returned by the kernel.
 * @packed: Value with the height in the high 16 bits and width in the low 16.
 * @out: Receives the decoded geometry.
 *
 * Return: False when the kernel signalled an error instead of a geometry.
 */
static bool unpackMode(int32_t packed, HDisplayMode* out) {
    if (packed < 0 || !out) return false;
    out->width = (uint16_t)(packed & 0xFFFF);
    out->height = (uint16_t)(((uint32_t)packed >> 16) & 0xFFFF);
    return true;
}

int display_get_mode_count() {
    WidgetData data = {0, 0, 0, 0, 0, nullptr};
    return (int)HguiAPI(DISPLAY, GET_MODE_COUNT, (void*)&data);
}

bool display_get_mode(int index, HDisplayMode* out) {
    if (index < 0 || !out) return false;

    WidgetData data = {(uint32_t)index, 0, 0, 0, 0, nullptr};
    return unpackMode((int32_t)HguiAPI(DISPLAY, GET_MODE, (void*)&data), out);
}

bool display_get_current_mode(HDisplayMode* out) {
    WidgetData data = {0, 0, 0, 0, 0, nullptr};
    return unpackMode((int32_t)HguiAPI(DISPLAY, GET_CURRENT_MODE, (void*)&data), out);
}

bool display_set_mode(uint16_t width, uint16_t height) {
    WidgetData data = {(uint32_t)width, (int32_t)height, 0, 0, 0, nullptr};
    return (int32_t)HguiAPI(DISPLAY, SET_MODE, (void*)&data) == 1;
}
