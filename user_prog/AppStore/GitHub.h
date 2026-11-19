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

#ifndef APPSTORE_GITHUB_H
#define APPSTORE_GITHUB_H

#include <stdint.h>

/** struct GitHubRelease - The Quake.bin asset of the latest Quake release. */
struct GitHubRelease {
    char tag[64];
    char downloadUrl[512];
    char apiUrl[512];
    char sha256[65];
    uint32_t size;
    bool fromCatalogFallback;
};

/**
 * GitHubResolveLatest() - Resolve the latest Quake-Hx86 release asset.
 *
 * Queries the GitHub Releases API, finds the Quake.bin asset and returns its
 * download URL, size and SHA-256 digest. The digest is required; older
 * releases without one are rejected rather than installed unverified.
 *
 * Return: true on success, false with @error filled on failure.
 */
bool GitHubResolveLatest(GitHubRelease* release, char* error, uint32_t errorCapacity);

#endif  // APPSTORE_GITHUB_H
