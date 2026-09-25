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

#ifndef BOOTANIM_H
#define BOOTANIM_H

#include <stdint.h>

struct MultibootInfo;
class Bitmap;
class GraphicsDriver;

/**
 * struct BootMainArgs - Parameters handed to the boot splasher.
 * @mbinfo: Multiboot info describing the booted system.
 */
struct BootMainArgs {
    MultibootInfo* mbinfo;
};

/**
 * struct BootAnimSet - The shared boot-splash frame set.
 * @frames: Array of animation frames as loaded bitmaps.
 * @count: Number of frames in @frames.
 * @w: Frame width in pixels.
 * @h: Frame height in pixels.
 * @bg: Owner-held copy of the screen background behind the splash.
 * @bgValid: Whether @bg holds valid captured pixels.
 */
struct BootAnimSet {
    Bitmap** frames;
    int count;
    int w;
    int h;
    uint32_t* bg;
    bool bgValid;
};

extern BootAnimSet g_bootAnimSet;
extern volatile bool g_bootSplashDone;
extern volatile bool g_bootSplashExited;

/**
 * BootSplashAnimator() - Splasher task entry that animates frames until done.
 * @arg: Pointer to a BootMainArgs.
 */
void BootSplashAnimator(void* arg);

/**
 * BootTitleResync() - Re-sync the splash title to the current log sink.
 */
void BootTitleResync();

/**
 * LoadBootAnimFrames() - Load and decode the boot animation frames.
 */
void LoadBootAnimFrames();

/**
 * FreeBootAnimFrames() - Release the animation frames and background buffer.
 */
void FreeBootAnimFrames();

/**
 * BootSplashRepaint() - Redraw the current frame and captured background.
 */
void BootSplashRepaint();

#endif  // BOOTANIM_H
