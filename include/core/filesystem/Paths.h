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

#pragma once

// Kernel binary search path on the root partition.
#define PATH_KERNEL_MAP "kernel.map"

// System applications (OS tools).
#define PATH_HASHX86_APPS "Hashx86/apps/"
#define PATH_CALCULATOR "Hashx86/apps/test.bin"
#define PATH_MEMVIEW "Hashx86/apps/MeMView.bin"
#define PATH_EXPLORER "Hashx86/apps/Explorer.bin"
#define PATH_TERMINAL "Hashx86/apps/Terminal.bin"
#define PATH_CLIHELLO "Hashx86/apps/CLIHello.bin"

// User applications.
#define PATH_APPS "Hashx86/apps/"

// Game3D (user app with assets).
#define PATH_GAME3D "Apps/Game3D/Game3D.bin"

// System drivers.
#define PATH_DRIVERS "Hashx86/drivers/"
#define PATH_BGA_DRIVER "Hashx86/drivers/bga.sys"
#define PATH_AC97_DRIVER "Hashx86/drivers/ac97.sys"

// Graphics resources.
#define PATH_GFX "Hashx86/gfx/"
#define PATH_BOOT_BMP "Hashx86/gfx/boot.bmp"
#define PATH_DESKTOP_BMP "Hashx86/gfx/desktop.bmp"
#define PATH_ICON_BMP "Hashx86/gfx/icon.bmp"
#define PATH_CURSOR_BMP "Hashx86/gfx/cursor.bmp"
#define PATH_PANIC_BMP "Hashx86/gfx/panic.bmp"
// Boot animation frameset (optional): frame00.bmp, frame01.bmp, ...
#define PATH_BOOTANIM_PREFIX "Hashx86/gfx/bootanim/frame"

// Fonts.
#define PATH_FONTS "Hashx86/fonts/"
#define PATH_SEGOEUI_FONT "Hashx86/fonts/segoeui.ttf"
#define PATH_SEGOEUI_BOLD_FONT "Hashx86/fonts/segoeuib.ttf"
#define PATH_SEGOEUI_ITALIC_FONT "Hashx86/fonts/segoeuii.ttf"
#define PATH_SEGOEUI_BOLD_ITALIC_FONT "Hashx86/fonts/segoeuiz.ttf"
#define PATH_CASCADIA_FONT "Hashx86/fonts/CascadiaMono-Regular.ttf"
#define PATH_CASCADIA_BOLD_FONT "Hashx86/fonts/CascadiaMono-Bold.ttf"
#define PATH_CASCADIA_ITALIC_FONT "Hashx86/fonts/CascadiaMono-Italic.ttf"
#define PATH_CASCADIA_BOLD_ITALIC_FONT "Hashx86/fonts/CascadiaMono-BoldItalic.ttf"
#define PATH_ICON_FONT "Hashx86/fonts/fa-solid-900.ttf"

// Audio.
#define PATH_AUDIO "Hashx86/audio/"
#define PATH_BOOT_WAV "Hashx86/audio/boot.wav"
