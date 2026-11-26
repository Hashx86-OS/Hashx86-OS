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

#ifndef APPSTORE_CONFIG_H
#define APPSTORE_CONFIG_H

// The latest release of this repository is used as the catalog source. Its
// assets must contain APPSTORE_CATALOG_ASSET and the JPEG artwork referenced
// by that catalog; application binaries come from each app's own repository.
// Only the catalog repository location is fixed here; release tags, asset IDs,
// URLs, sizes and hashes all come from GitHub metadata or the catalog itself.
#ifndef APPSTORE_CATALOG_REPO
#define APPSTORE_CATALOG_REPO "Hashx86-OS/AppStore-Hx86"
#endif

#ifndef APPSTORE_CATALOG_ASSET
#define APPSTORE_CATALOG_ASSET "catalog.json"
#endif

// The OS version exposed to the catalog's minOsVersion checks. Update this
// when releasing a new OS image.
#ifndef APPSTORE_OS_VERSION
#define APPSTORE_OS_VERSION "0.1.0"
#endif

#endif  // APPSTORE_CONFIG_H
