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

#ifndef WIDGET_GALLERY_H
#define WIDGET_GALLERY_H

class Desktop;
class Window;

/**
 * OpenWidgetGallery() - Open a window containing one example of every widget.
 * @desktop: Desktop to attach the window to.
 *
 * Kernel-side demo: it builds the window and its widget tree directly, so it
 * works without a userspace client. Adding an entry to the start menu calls
 * this. Return: The created window, or nullptr on failure.
 */
Window* OpenWidgetGallery(Desktop* desktop);

#endif  // WIDGET_GALLERY_H
