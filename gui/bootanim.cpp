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

#define KDBG_COMPONENT "GUI:BOOTANIM"
#include <core/filesystem/Paths.h>
#include <gui/bootanim.h>
#include <kernel.h>

// Boot animation thread: dot row with a travelling pulse.

volatile bool g_bootSplashDone = false;    // Set by BootMain to stop the animator.
volatile bool g_bootSplashExited = false;  // Set by the animator before exiting.

constexpr int BOOTDOTS_N = 5;         // Dots in the row (minimal, cinematic).
constexpr int BOOTDOTS_SPACING = 32;  // Pixels between dot centers.
constexpr int BOOTDOTS_W = 160;       // Box: covers row span plus halo radius and margin.
constexpr int BOOTDOTS_H = 40;
uint32_t g_bootDotsBg[BOOTDOTS_W * BOOTDOTS_H];
bool g_bootDotsBgValid = false;  // Captured on the first frame (the screen settles by then).

// Box geometry shared by the animator and the re-sync path.
static void BootDotsBox(uint32_t W, uint32_t H, int32_t& bx, int32_t& by) {
    int32_t cx = (int32_t)W / 2;
    int32_t cy = (int32_t)(H * 2) / 3 + 120;
    // Keep the dots box clear of the screen bottom edge.
    if (cy + BOOTDOTS_H / 2 > (int32_t)H - 20) {
        cy = (int32_t)H - 20 - BOOTDOTS_H / 2;
    }
    if (cy < BOOTDOTS_H / 2) cy = BOOTDOTS_H / 2;
    bx = cx - BOOTDOTS_W / 2;
    by = cy - BOOTDOTS_H / 2;
}

static void RestoreDotsBg(GraphicsDriver* drv, int32_t bx, int32_t by) {
    uint32_t W = drv->GetWidth();
    uint32_t H = drv->GetHeight();
    uint32_t* fb = drv->GetBackBuffer();
    if (!fb) return;
    for (int r = 0; r < BOOTDOTS_H; r++) {
        int py = by + r;
        if (py < 0 || py >= (int)H) continue;
        for (int c = 0; c < BOOTDOTS_W; c++) {
            int px = bx + c;
            if (px < 0 || px >= (int)W) continue;
            fb[py * W + px] = g_bootDotsBg[r * BOOTDOTS_W + c];
        }
    }
}

static void CaptureDotsBg(GraphicsDriver* drv, int32_t bx, int32_t by) {
    uint32_t W = drv->GetWidth();
    uint32_t H = drv->GetHeight();
    uint32_t* fb = drv->GetBackBuffer();
    if (!fb) return;
    for (int r = 0; r < BOOTDOTS_H; r++) {
        int py = by + r;
        if (py < 0 || py >= (int)H) continue;
        for (int c = 0; c < BOOTDOTS_W; c++) {
            int px = bx + c;
            if (px < 0 || px >= (int)W) continue;
            g_bootDotsBg[r * BOOTDOTS_W + c] = fb[py * W + px];
        }
    }
    g_bootDotsBgValid = true;
}

constexpr int BOOTANIM_FRAME_MS = 80;

static void DrawBootAnimFrame(GraphicsDriver* drv, uint32_t frameIdx);

void BootSplashAnimator(void* arg) {
    (void)arg;
    KDBG1("Boot dots animator started (early multithreading live)");

    uint32_t frame = 0;
    while (!g_bootSplashDone) {
        GraphicsDriver* drv = g_GraphicsDriver;
        if (drv) {
            if (g_bootAnimSet.count > 0) {
                DrawBootAnimFrame(drv, frame);
            } else {
                InterruptGuard guard;  // Atomic frame blit (dots mode).
                uint32_t W = drv->GetWidth();
                uint32_t H = drv->GetHeight();
                uint32_t* fb = drv->GetBackBuffer();
                int32_t bx, by;
                BootDotsBox(W, H, bx, by);
                int32_t cx = bx + BOOTDOTS_W / 2;
                int32_t cy = by + BOOTDOTS_H / 2;

                if (fb && !g_bootDotsBgValid) CaptureDotsBg(drv, bx, by);

                // Erase the previous frame by restoring the saved background.
                if (fb && g_bootDotsBgValid) RestoreDotsBg(drv, bx, by);

                // Glow sweep: the light pulse travels left to right, wrapping around.
                int head = (int)(frame % BOOTDOTS_N);
                for (int i = 0; i < BOOTDOTS_N; i++) {
                    int age = (head - i + BOOTDOTS_N) % BOOTDOTS_N;
                    uint32_t core;
                    uint32_t tint;
                    if (age == 0) {
                        core = 0xFFFFFFFF;  // Ice-white hot.
                        tint = 0xDFF6FF;
                    } else if (age == 1) {
                        core = 0xFFD9EDEF;
                        tint = 0xB9DCE8;
                    } else if (age == 2) {
                        core = 0xFFA9BCC4;
                        tint = 0x7E99A6;
                    } else if (age == 3) {
                        core = 0xFF6E7F88;
                        tint = 0x4E5E66;
                    } else {
                        core = 0xFF454F55;  // Fading ember.
                        tint = 0x2E383D;
                    }
                    int dx = (i - (BOOTDOTS_N - 1) / 2) * BOOTDOTS_SPACING;
                    // Radial glow: an opaque core with alpha halo rings (PutPixel blends).
                    constexpr int R = 11;
                    for (int yy = -R; yy <= R; yy++) {
                        for (int xx = -R; xx <= R; xx++) {
                            int d2 = xx * xx + yy * yy;
                            uint32_t col;
                            if (d2 <= 16) {
                                col = core;  // r<=4 solid core.
                            } else if (d2 <= 36) {
                                col = (0x90u << 24) | tint;  // Strong glow.
                            } else if (d2 <= 81) {
                                col = (0x40u << 24) | tint;  // Soft glow.
                            } else if (d2 <= 121) {
                                col = (0x18u << 24) | tint;  // Faint halo.
                            } else {
                                continue;
                            }
                            drv->PutPixel(cx + dx + xx, cy + yy, col);
                        }
                    }
                }

                drv->Flush();
            }  // Dots mode.
        }
        frame++;
        // Yield to the boot worker until the next tick.
        if (Scheduler::activeInstance) {
            Scheduler::activeInstance->Sleep(g_bootAnimSet.count > 0 ? BOOTANIM_FRAME_MS : 150);
        }
        asm volatile("sti; hlt");
    }

    FreeBootAnimFrames();       // The animator is the last frameset user.
    g_bootSplashExited = true;  // Handshake: the boot worker may take the framebuffer.
    KDBG1("Boot dots animator exiting");
}

// Optional image-frame animation: Hashx86/gfx/bootanim/frameNN.bmp (24/32-bit
// BMP, uniform size, contiguous from 00). Falls back to dots when absent.

constexpr int BOOTANIM_MAX_FRAMES = 64;
constexpr int BOOTANIM_MAX_DIM = 256;

BootAnimSet g_bootAnimSet;

void LoadBootAnimFrames() {
    BootAnimSet& s = g_bootAnimSet;
    s.frames = nullptr;
    s.count = 0;
    s.w = 0;
    s.h = 0;
    s.bg = nullptr;
    s.bgValid = false;

    Bitmap* tmp[BOOTANIM_MAX_FRAMES];
    int n = 0;
    for (int i = 0; i < BOOTANIM_MAX_FRAMES; i++) {
        // frame00.bmp through frame63.bmp (hand-padded, no snprintf in the kernel).
        char idx[8];
        itoa(i, idx, 10, sizeof(idx));
        char path[80];
        int p = 0;
        const char* pre = PATH_BOOTANIM_PREFIX;
        while (*pre && p < (int)sizeof(path) - 10) path[p++] = *pre++;
        if (i < 10) path[p++] = '0';
        for (const char* q = idx; *q && p < (int)sizeof(path) - 5; q++) path[p++] = *q;
        path[p++] = '.';
        path[p++] = 'b';
        path[p++] = 'm';
        path[p++] = 'p';
        path[p++] = '\0';

        Bitmap* b = new Bitmap(path);
        if (!b || !b->IsValid()) {
            if (b) delete b;
            break;  // The first missing file ends the set (also: no frames at all).
        }
        if (b->GetWidth() <= 0 || b->GetHeight() <= 0 || b->GetWidth() > BOOTANIM_MAX_DIM ||
            b->GetHeight() > BOOTANIM_MAX_DIM) {
            KDBG1("BootAnim: %s bad size %dx%d, set ends here", path, b->GetWidth(),
                  b->GetHeight());
            delete b;
            break;
        }
        if (n > 0 && (b->GetWidth() != s.w || b->GetHeight() != s.h)) {
            KDBG1("BootAnim: %s size mismatch, set ends here", path);
            delete b;
            break;
        }
        if (n == 0) {
            s.w = b->GetWidth();
            s.h = b->GetHeight();
        }
        tmp[n++] = b;
    }

    if (n == 0) {
        KDBG1("BootAnim: no frames, using procedural dots");
        return;
    }

    s.frames = new Bitmap*[n];
    s.bg = new uint32_t[(size_t)s.w * (size_t)s.h];
    if (!s.frames || !s.bg) {
        KDBG1("BootAnim: out of memory, using procedural dots");
        for (int k = 0; k < n; k++) delete tmp[k];
        if (s.frames) delete[] s.frames;
        if (s.bg) delete[] s.bg;
        s.frames = nullptr;
        s.bg = nullptr;
        return;
    }
    for (int k = 0; k < n; k++) s.frames[k] = tmp[k];
    s.count = n;
    KDBG1("BootAnim: %d frames %dx%d loaded", n, s.w, s.h);
}

void FreeBootAnimFrames() {
    BootAnimSet& s = g_bootAnimSet;
    for (int i = 0; i < s.count; i++) delete s.frames[i];
    delete[] s.frames;
    delete[] s.bg;
    s.frames = nullptr;
    s.count = 0;
    s.w = 0;
    s.h = 0;
    s.bg = nullptr;
    s.bgValid = false;
}

// Frame box anchored where the dots row sits.
static void BootAnimBox(uint32_t W, uint32_t H, int fw, int fh, int32_t& bx, int32_t& by) {
    int32_t cx = (int32_t)W / 2;
    int32_t cy = (int32_t)(H * 2) / 3 + 120;
    int32_t x = cx - fw / 2;
    int32_t y = cy - fh / 2;
    if (y + fh > (int32_t)H - 20) y = (int32_t)H - 20 - fh;
    if (y < 0) y = 0;
    if (x < 0) x = 0;
    bx = x;
    by = y;
}

static void RestoreFrameBg(GraphicsDriver* drv, int32_t bx, int32_t by) {
    BootAnimSet& s = g_bootAnimSet;
    uint32_t W = drv->GetWidth();
    uint32_t H = drv->GetHeight();
    uint32_t* fb = drv->GetBackBuffer();
    if (!fb || !s.bg) return;
    for (int r = 0; r < s.h; r++) {
        int py = by + r;
        if (py < 0 || py >= (int)H) continue;
        for (int c = 0; c < s.w; c++) {
            int px = bx + c;
            if (px < 0 || px >= (int)W) continue;
            fb[py * W + px] = s.bg[r * s.w + c];
        }
    }
}

static void CaptureFrameBg(GraphicsDriver* drv, int32_t bx, int32_t by) {
    BootAnimSet& s = g_bootAnimSet;
    uint32_t W = drv->GetWidth();
    uint32_t H = drv->GetHeight();
    uint32_t* fb = drv->GetBackBuffer();
    if (!fb || !s.bg) return;
    for (int r = 0; r < s.h; r++) {
        int py = by + r;
        if (py < 0 || py >= (int)H) continue;
        for (int c = 0; c < s.w; c++) {
            int px = bx + c;
            if (px < 0 || px >= (int)W) continue;
            s.bg[r * s.w + c] = fb[py * W + px];
        }
    }
    s.bgValid = true;
}

static void DrawBootAnimFrame(GraphicsDriver* drv, uint32_t frameIdx) {
    BootAnimSet& s = g_bootAnimSet;
    if (s.count <= 0 || !s.frames || !s.bg) return;
    InterruptGuard guard;  // Atomic frame blit.
    Bitmap* bmp = s.frames[frameIdx % s.count];
    if (!bmp || !bmp->IsValid()) return;
    uint32_t W = drv->GetWidth();
    uint32_t H = drv->GetHeight();
    uint32_t* fb = drv->GetBackBuffer();
    int32_t bx, by;
    BootAnimBox(W, H, s.w, s.h, bx, by);
    if (fb && !s.bgValid) CaptureFrameBg(drv, bx, by);
    if (fb && s.bgValid) RestoreFrameBg(drv, bx, by);
    // 32-bit frames alpha-blend (glow); 24-bit frames are opaque.
    drv->DrawBitmap(bx, by, bmp->GetBuffer(), s.w, s.h);
    drv->Flush();
}

// Draws the title and re-syncs the animation background atomically, so the
// saved background ends up with the title and without animation pixels.
void BootTitleResync() {
    InterruptGuard guard;
    GraphicsDriver* drv = g_GraphicsDriver;
    if (!drv) return;
    const bool useFrames = (g_bootAnimSet.count > 0);
    int32_t bx, by;
    if (useFrames) {
        BootAnimBox(drv->GetWidth(), drv->GetHeight(), g_bootAnimSet.w, g_bootAnimSet.h, bx, by);
        if (g_bootAnimSet.bgValid) RestoreFrameBg(drv, bx, by);
    } else {
        BootDotsBox(drv->GetWidth(), drv->GetHeight(), bx, by);
        if (g_bootDotsBgValid) RestoreDotsBg(drv, bx, by);
    }
    if (g_fManager) {
        Font* BOOT = g_fManager->getNewFont();
        if (BOOT != nullptr) {
            BOOT->setSize(XLARGE);
            int32_t x, y;
            drv->GetScreenCenter(BOOT->getStringLength("Hash x86"), 0, x, y);
            drv->DrawString(x, (int32_t)((drv->GetHeight() * 1) / 3 + 300), "Hash x86", BOOT,
                            0xFFFFFFFF);
        }
    }
    if (useFrames) {
        BootAnimBox(drv->GetWidth(), drv->GetHeight(), g_bootAnimSet.w, g_bootAnimSet.h, bx, by);
        CaptureFrameBg(drv, bx, by);
    } else {
        BootDotsBox(drv->GetWidth(), drv->GetHeight(), bx, by);
        CaptureDotsBg(drv, bx, by);
    }
    drv->Flush();
}

// Repaints the static splash on the current driver and re-captures the
// animation background. Required after init_pci (BGA swaps video mode).
void BootSplashRepaint() {
    InterruptGuard guard;
    GraphicsDriver* drv = g_GraphicsDriver;
    if (!drv) return;

    uint32_t W = drv->GetWidth();
    uint32_t H = drv->GetHeight();
    uint32_t* fb = drv->GetBackBuffer();
    if (fb && W > 0 && H > 0) {
        uint64_t n = (uint64_t)W * (uint64_t)H;
        for (uint64_t i = 0; i < n; i++) fb[i] = 0xFF000000;
    }

    Bitmap* bootImg = new Bitmap(PATH_BOOT_BMP);
    if (bootImg && bootImg->IsValid()) {
        int32_t x, y;
        drv->GetScreenCenter(bootImg->GetWidth(), bootImg->GetHeight(), x, y);
        drv->DrawBitmap(x, (int32_t)((drv->GetHeight() * 1) / 3), bootImg->GetBuffer(),
                        bootImg->GetWidth(), bootImg->GetHeight());
    }
    if (bootImg) delete bootImg;

    if (g_fManager) {
        Font* title = g_fManager->getNewFont();
        if (title) {
            title->setSize(XLARGE);
            int32_t x, y;
            drv->GetScreenCenter(title->getStringLength("Hash x86"), 0, x, y);
            drv->DrawString(x, (int32_t)((drv->GetHeight() * 1) / 3 + 300), "Hash x86", title,
                            0xFFFFFFFF);
        }
    }
    drv->Flush();

    const bool useFrames = (g_bootAnimSet.count > 0);
    int32_t bx, by;
    if (useFrames) {
        BootAnimBox(W, H, g_bootAnimSet.w, g_bootAnimSet.h, bx, by);
        CaptureFrameBg(drv, bx, by);
    } else {
        BootDotsBox(W, H, bx, by);
        CaptureDotsBg(drv, bx, by);
    }
}
