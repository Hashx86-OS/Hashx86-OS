/**
 * @file        bootanim.h
 * @brief       Boot splash animation interface (part of #x86 GUI Framework)
 *
 * @date        05/09/2026
 * @version     1.0.0
 */

#ifndef BOOTANIM_H
#define BOOTANIM_H

#include <stdint.h>

struct MultibootInfo;
class Bitmap;
class GraphicsDriver;

struct BootMainArgs {
    MultibootInfo* mbinfo;
};

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

void BootSplashAnimator(void* arg);
void BootTitleResync();
void LoadBootAnimFrames();
void FreeBootAnimFrames();
void BootSplashRepaint();

#endif  // BOOTANIM_H
