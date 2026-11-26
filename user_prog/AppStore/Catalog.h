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

#ifndef APPSTORE_CATALOG_H
#define APPSTORE_CATALOG_H

#include <stdint.h>

#define APPSTORE_MAX_APPS 32
#define APPSTORE_MAX_ASSETS 256
#define APPSTORE_MAX_APP_BYTES (64u * 1024u * 1024u)

#define APPSTORE_CATALOG_FIELD_LEN 32
#define APPSTORE_NAME_FIELD_LEN 64
#define APPSTORE_DESCRIPTION_FIELD_LEN 256
#define APPSTORE_ICON_FIELD_LEN 40
#define APPSTORE_LICENSE_FIELD_LEN 64
#define APPSTORE_SOURCE_FIELD_LEN 256
#define APPSTORE_REPO_FIELD_LEN 128
#define APPSTORE_FILE_FIELD_LEN 96
#define APPSTORE_URL_FIELD_LEN 512
#define APPSTORE_SHA256_FIELD_LEN 65

/** struct CatalogAssetRef - A resolved release asset for catalog metadata. */
struct CatalogAssetRef {
    char url[APPSTORE_URL_FIELD_LEN];
    char sha256[APPSTORE_SHA256_FIELD_LEN];
    uint32_t size;
};

/**
 * struct CatalogApp - One installable entry from the remote catalog.
 *
 * The catalog entry is normalized before it reaches the UI. For a repo-backed
 * entry, @repo is the GitHub "owner/repo" whose latest release supplies the
 * binary, and @assetName names the asset inside that release. CatalogResolveApp()
 * fills @downloadUrl, @sha256 and @size from that release on demand. Direct-URL
 * and legacy same-release entries are resolved while the catalog is parsed.
 * @binary is always a safe file name inside /Apps/<id>/.
 */
struct CatalogApp {
    char id[APPSTORE_CATALOG_FIELD_LEN];
    char name[APPSTORE_NAME_FIELD_LEN];
    char version[APPSTORE_CATALOG_FIELD_LEN];
    char description[APPSTORE_DESCRIPTION_FIELD_LEN];
    char icon[APPSTORE_ICON_FIELD_LEN];
    char category[APPSTORE_CATALOG_FIELD_LEN];
    char license[APPSTORE_LICENSE_FIELD_LEN];
    char source[APPSTORE_SOURCE_FIELD_LEN];
    char minOsVersion[APPSTORE_CATALOG_FIELD_LEN];
    char iconAsset[APPSTORE_FILE_FIELD_LEN];
    char bannerAsset[APPSTORE_FILE_FIELD_LEN];
    CatalogAssetRef iconRef;
    CatalogAssetRef bannerRef;
    char repo[APPSTORE_REPO_FIELD_LEN];
    char assetName[APPSTORE_FILE_FIELD_LEN];
    char binary[APPSTORE_FILE_FIELD_LEN];
    char downloadUrl[APPSTORE_URL_FIELD_LEN];
    char sha256[APPSTORE_SHA256_FIELD_LEN];
    uint32_t size;
    bool resolved;
    bool compatible;
};

/** struct Catalog - The latest release catalog and its normalized app list. */
struct Catalog {
    char releaseTag[APPSTORE_NAME_FIELD_LEN];
    char catalogVersion[APPSTORE_CATALOG_FIELD_LEN];
    char osVersion[APPSTORE_CATALOG_FIELD_LEN];
    uint32_t appCount;
    CatalogApp apps[APPSTORE_MAX_APPS];
};

/**
 * CatalogFetch() - Fetch and parse the latest catalog release.
 *
 * Resolves https://api.github.com/repos/<APPSTORE_CATALOG_REPO>/releases/latest,
 * finds APPSTORE_CATALOG_ASSET among the release assets, downloads it with its
 * GitHub-provided SHA-256 digest, then parses the app entries. Entries with a
 * "repo" leave their binary unresolved until CatalogResolveApp() is called.
 * Legacy entries with "asset" and no repo resolve against the catalog release;
 * entries with a direct "url" must carry their own size and sha256.
 *
 * The catalog itself is rejected if it is missing, oversized or fails hash
 * verification. A malformed app entry fails the whole fetch so a partial
 * catalog is never presented as complete.
 *
 * Return: true on success, false with @error filled on failure.
 */
bool CatalogFetch(Catalog* catalog, char* error, uint32_t errorCapacity);

/**
 * CatalogResolveApp() - Resolve a repo-backed app's binary metadata.
 * @app: Catalog entry. For a repo entry, @repo and @assetName must be set.
 * @error: Destination for a human-readable failure message.
 * @errorCapacity: Size of @error in bytes.
 *
 * Fetches the app repository's latest GitHub release and copies the selected
 * asset's API URL, size and SHA-256 digest into @app. Direct-URL entries and
 * entries already resolved by CatalogFetch() return success immediately.
 *
 * Return: true on success, false with @error filled on failure.
 */
bool CatalogResolveApp(CatalogApp* app, char* error, uint32_t errorCapacity);

#endif  // APPSTORE_CATALOG_H
