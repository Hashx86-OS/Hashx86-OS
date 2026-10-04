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

#ifndef HDISPLAY_H
#define HDISPLAY_H

#include <Hx86/stdint.h>

/** struct HDisplayMode - One selectable framebuffer resolution. */
struct HDisplayMode {
    uint16_t width;
    uint16_t height;
};

/**
 * display_get_mode_count() - Return how many resolutions the kernel offers.
 *
 * Return: The number of entries GetDisplayMode() accepts, or 0 on failure.
 */
int display_get_mode_count();

/**
 * display_get_mode() - Fetch one offered resolution.
 * @index: Position in the list, 0 .. display_get_mode_count() - 1.
 *
 * Return: True on success, false when @index is out of range.
 */
bool display_get_mode(int index, HDisplayMode* out);

/**
 * display_get_current_mode() - Report the active framebuffer resolution.
 * @out: Receives the current geometry.
 *
 * Return: True on success, false when no display is available.
 */
bool display_get_current_mode(HDisplayMode* out);

/**
 * display_set_mode() - Switch the display to a new resolution.
 * @width: Requested width in pixels.
 * @height: Requested height in pixels.
 *
 * The kernel re-programs the graphics hardware and re-lays out the desktop.
 * Returns false when the adapter or driver rejects the request, for example
 * while a fullscreen application owns the screen.
 *
 * Return: True when the new resolution is active.
 */
bool display_set_mode(uint16_t width, uint16_t height);

#endif  // HDISPLAY_H
