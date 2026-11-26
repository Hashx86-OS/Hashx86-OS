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

#ifndef APPSTORE_IMAGE_DECODER_H
#define APPSTORE_IMAGE_DECODER_H

#include <stdint.h>

/** struct ImagePixels - Decoded image pixels in 0xAARRGGBB form. */
struct ImagePixels {
    uint32_t* pixels;
    int32_t width;
    int32_t height;
};

/**
 * DecodeJpeg() - Decode a JPEG buffer into 0xAARRGGBB pixels.
 * @data: JPEG bytes.
 * @length: Number of bytes.
 * @image: Output image; caller owns @image.pixels and frees it with kfree().
 *
 * The decoder is stb_image compiled for JPEG only. It runs in user space so the
 * kernel never needs a JPEG codec.
 *
 * Return: true on success, false for malformed/oversized images or OOM.
 */
bool DecodeJpeg(const uint8_t* data, uint32_t length, ImagePixels* image);

#endif  // APPSTORE_IMAGE_DECODER_H
