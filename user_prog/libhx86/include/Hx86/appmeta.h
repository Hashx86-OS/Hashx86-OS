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

#ifndef HX86_APPMETA_H
#define HX86_APPMETA_H

#include <Hx86/stdint.h>

// Magic value and version that identify the embedded application metadata.
#define HX86_APP_META_MAGIC 0x36385848
#define HX86_APP_META_VERSION 1

// Application types recognized by the loader.
enum Hx86AppType {
    HX86_APP_UNKNOWN = 0,
    HX86_APP_GUI = 1,
    HX86_APP_CLI = 2,
};

/** struct Hx86AppMeta - Application metadata embedded in the .hx86meta section. */
struct Hx86AppMeta {
    uint32_t magic;    // HX86_APP_META_MAGIC marker.
    uint16_t version;  // HX86_APP_META_VERSION.
    uint16_t appType;  // One of the Hx86AppType values.
} __attribute__((packed));

/** HX86_DECLARE_APP(appTypeValue) - Emit an Hx86AppMeta record for this binary. */
#define HX86_DECLARE_APP(appTypeValue)                                                       \
    __attribute__((used, section(".hx86meta"))) static const Hx86AppMeta g_hx86_app_meta = { \
        HX86_APP_META_MAGIC, HX86_APP_META_VERSION, (appTypeValue)}

#endif  // HX86_APPMETA_H
