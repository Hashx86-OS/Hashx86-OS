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

#include "Catalog.h"

#include "AppStoreConfig.h"
#include "HttpClient.h"
#include "StoreUtil.h"

#include <Hx86/debug.h>
#include <Hx86/memory.h>
#include <mbedtls/sha256.h>

#define JSMN_STATIC
#include <jsmn.h>

namespace {

constexpr uint32_t kReleaseBodyCapacity = 768u * 1024u;
constexpr uint32_t kCatalogBodyCapacity = 128u * 1024u;
constexpr int kReleaseTokenCapacity = 32768;
constexpr int kCatalogTokenCapacity = 6144;

char g_releaseBody[kReleaseBodyCapacity];
char g_catalogBody[kCatalogBodyCapacity];
uint32_t g_releaseLength = 0;
uint32_t g_catalogLength = 0;
jsmntok_t g_releaseTokens[kReleaseTokenCapacity];
jsmntok_t g_catalogTokens[kCatalogTokenCapacity];

struct ReleaseAsset {
    char name[APPSTORE_FILE_FIELD_LEN];
    char apiUrl[APPSTORE_URL_FIELD_LEN];
    char sha256[APPSTORE_SHA256_FIELD_LEN];
    uint32_t size;
    bool hasDigest;
};

ReleaseAsset g_releaseAssets[APPSTORE_MAX_ASSETS];
uint32_t g_releaseAssetCount = 0;

struct BufferSink {
    char* data;
    uint32_t capacity;
    uint32_t length;
    bool overflow;
};

bool BufferSinkWrite(void* context, const uint8_t* data, uint32_t length) {
    BufferSink* sink = (BufferSink*)context;
    if (sink == nullptr || data == nullptr) return false;
    if (length >= sink->capacity || sink->length > sink->capacity - 1 - length) {
        sink->overflow = true;
        return false;
    }
    for (uint32_t i = 0; i < length; i++) sink->data[sink->length + i] = (char)data[i];
    sink->length += length;
    sink->data[sink->length] = '\0';
    return true;
}

void DigestToHex(const uint8_t digest[32], char output[APPSTORE_SHA256_FIELD_LEN]) {
    static const char kHex[] = "0123456789abcdef";
    for (uint32_t i = 0; i < 32; i++) {
        output[i * 2] = kHex[digest[i] >> 4];
        output[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    output[64] = '\0';
}

bool IsHexString(const char* text, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        char c = text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

bool DigestMatches(const uint8_t* data, uint32_t length, const char* expectedHex) {
    if (data == nullptr || expectedHex == nullptr) return false;
    uint8_t digest[32];
    if (mbedtls_sha256_ret((const unsigned char*)data, length, digest, 0) != 0) return false;
    char actualHex[APPSTORE_SHA256_FIELD_LEN];
    DigestToHex(digest, actualHex);
    return StoreHexEqual(actualHex, expectedHex);
}

bool DownloadToBuffer(const char* url, const char* accept, char* buffer, uint32_t capacity,
                      uint32_t expectedSize, const char* expectedSha256, uint32_t* outputLength,
                      char* error, uint32_t errorCapacity) {
    if (url == nullptr || buffer == nullptr || capacity == 0) return false;

    buffer[0] = '\0';
    BufferSink sink = {buffer, capacity, 0, false};
    StoreHttpResponse response = {};
    if (!StoreHttpGet(url, accept, BufferSinkWrite, &sink, &response, error, errorCapacity)) {
        if (sink.overflow) {
            StoreStrCopy(error, errorCapacity, "response is too large");
        }
        return false;
    }

    if (expectedSize != 0 && sink.length != expectedSize) {
        StoreFormat(error, errorCapacity, "size mismatch: got %u, expected %u", sink.length,
                    expectedSize);
        return false;
    }
    if (expectedSha256 != nullptr && expectedSha256[0] != '\0' &&
        !DigestMatches((const uint8_t*)buffer, sink.length, expectedSha256)) {
        StoreStrCopy(error, errorCapacity, "SHA-256 check failed");
        return false;
    }
    if (outputLength != nullptr) *outputLength = sink.length;
    return true;
}

bool TokenEquals(const char* text, const jsmntok_t* token, const char* literal) {
    if (text == nullptr || token == nullptr || literal == nullptr) return false;
    uint32_t length = (uint32_t)(token->end - token->start);
    if (length != StoreStrLen(literal)) return false;
    for (uint32_t i = 0; i < length; i++) {
        if (text[token->start + (int)i] != literal[i]) return false;
    }
    return true;
}

bool TokenIsString(const jsmntok_t* token) {
    return token != nullptr && token->type == JSMN_STRING;
}

bool TokenCopy(const char* text, const jsmntok_t* token, char* output, uint32_t capacity) {
    if (text == nullptr || token == nullptr || output == nullptr || capacity == 0) return false;

    const char* cursor = text + token->start;
    const char* end = text + token->end;
    uint32_t used = 0;

    while (cursor < end) {
        char c = *cursor++;
        if (c != '\\') {
            if (used + 1 >= capacity) return false;
            output[used++] = c;
            continue;
        }

        if (cursor >= end) return false;
        char escape = *cursor++;
        char decoded = 0;
        switch (escape) {
            case '"':
            case '\\':
            case '/':
                decoded = escape;
                break;
            case 'b':
                decoded = '\b';
                break;
            case 'f':
                decoded = '\f';
                break;
            case 'n':
                decoded = '\n';
                break;
            case 'r':
                decoded = '\r';
                break;
            case 't':
                decoded = '\t';
                break;
            case 'u': {
                if (end - cursor < 4) return false;
                uint32_t codepoint = 0;
                for (int i = 0; i < 4; i++) {
                    char h = cursor[i];
                    uint32_t digit = 0;
                    if (h >= '0' && h <= '9') {
                        digit = (uint32_t)(h - '0');
                    } else if (h >= 'a' && h <= 'f') {
                        digit = (uint32_t)(h - 'a' + 10);
                    } else if (h >= 'A' && h <= 'F') {
                        digit = (uint32_t)(h - 'A' + 10);
                    } else {
                        return false;
                    }
                    codepoint = (codepoint << 4) | digit;
                }
                cursor += 4;

                if (codepoint < 0x80) {
                    if (used + 1 >= capacity) return false;
                    output[used++] = (char)codepoint;
                } else if (codepoint < 0x800) {
                    if (used + 2 >= capacity) return false;
                    output[used++] = (char)(0xC0 | (codepoint >> 6));
                    output[used++] = (char)(0x80 | (codepoint & 0x3F));
                } else {
                    if (used + 3 >= capacity) return false;
                    output[used++] = (char)(0xE0 | (codepoint >> 12));
                    output[used++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                    output[used++] = (char)(0x80 | (codepoint & 0x3F));
                }
                continue;
            }
            default:
                return false;
        }

        if (used + 1 >= capacity) return false;
        output[used++] = decoded;
    }

    output[used] = '\0';
    return true;
}

bool TokenU32(const char* text, const jsmntok_t* token, uint32_t* value) {
    if (token == nullptr || token->type != JSMN_PRIMITIVE || value == nullptr) return false;
    char buffer[24];
    if (!TokenCopy(text, token, buffer, sizeof(buffer))) return false;
    return StoreParseU32(buffer, value);
}

int SkipToken(const jsmntok_t* tokens, int index, int count) {
    if (index < 0 || index >= count) return count;
    int next = index + 1;
    if (tokens[index].type == JSMN_OBJECT) {
        int children = tokens[index].size * 2;
        for (int i = 0; i < children && next < count; i++) {
            next = SkipToken(tokens, next, count);
        }
    } else if (tokens[index].type == JSMN_ARRAY) {
        for (int i = 0; i < tokens[index].size && next < count; i++) {
            next = SkipToken(tokens, next, count);
        }
    }
    return next;
}

int ObjectFind(const char* text, const jsmntok_t* tokens, int count, int objectIndex,
               const char* key) {
    if (objectIndex < 0 || objectIndex >= count || tokens[objectIndex].type != JSMN_OBJECT) {
        return -1;
    }
    int next = objectIndex + 1;
    for (int i = 0; i < tokens[objectIndex].size; i++) {
        if (next + 1 >= count) return -1;
        if (TokenEquals(text, &tokens[next], key)) return next + 1;
        next = SkipToken(tokens, next + 1, count);
    }
    return -1;
}

int ArrayElement(const jsmntok_t* tokens, int count, int arrayIndex, int element) {
    if (arrayIndex < 0 || arrayIndex >= count || tokens[arrayIndex].type != JSMN_ARRAY) {
        return -1;
    }
    if (element < 0 || element >= tokens[arrayIndex].size) return -1;
    int next = arrayIndex + 1;
    for (int i = 0; i < element; i++) next = SkipToken(tokens, next, count);
    return next < count ? next : -1;
}

bool ParseSha256Value(const char* text, const jsmntok_t* token, char* output, uint32_t capacity) {
    if (!TokenIsString(token) || !TokenCopy(text, token, output, capacity)) return false;
    return StoreStrLen(output) == 64 && IsHexString(output, 64);
}

bool ParseGitHubDigest(const char* text, const jsmntok_t* token, char* output, uint32_t capacity) {
    if (!TokenIsString(token)) return false;
    uint32_t length = (uint32_t)(token->end - token->start);
    if (length != 71) return false;  // "sha256:" plus 64 hex characters.
    if (text[token->start + 0] != 's' || text[token->start + 1] != 'h' ||
        text[token->start + 2] != 'a' || text[token->start + 3] != '2' ||
        text[token->start + 4] != '5' || text[token->start + 5] != '6' ||
        text[token->start + 6] != ':') {
        return false;
    }
    if (capacity < 65) return false;
    for (uint32_t i = 0; i < 64; i++) output[i] = text[token->start + 7 + (int)i];
    output[64] = '\0';
    return IsHexString(output, 64);
}

bool IsSafeField(const char* text) {
    if (text == nullptr || text[0] == '\0') return false;
    if (StoreStrEqual(text, ".") || StoreStrEqual(text, "..")) return false;
    for (uint32_t i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                       c == '-' || c == '_' || c == '.';
        if (!allowed) return false;
    }
    return true;
}

bool IsRepoNameChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
           c == '_' || c == '.';
}

// Accept "owner/repo" and the common GitHub URL spellings, then normalize to
// the owner/repo form used by api.github.com. Reject paths, queries and any
// other characters so a catalog cannot smuggle an arbitrary API URL.
bool NormalizeGitHubRepo(const char* input, char* output, uint32_t capacity) {
    if (input == nullptr || output == nullptr || capacity == 0) return false;

    const char* path = input;
    if (StoreStrStartsWithCase(path, "https://github.com/")) {
        path += 19;
    } else if (StoreStrStartsWithCase(path, "http://github.com/")) {
        path += 18;
    } else if (StoreStrStartsWithCase(path, "github.com/")) {
        path += 11;
    }

    uint32_t length = StoreStrLen(path);
    while (length > 0 && path[length - 1] == '/') length--;
    if (length >= 4) {
        const char* tail = path + length - 4;
        if (tail[0] == '.' && (tail[1] == 'g' || tail[1] == 'G') &&
            (tail[2] == 'i' || tail[2] == 'I') && (tail[3] == 't' || tail[3] == 'T')) {
            length -= 4;
        }
    }
    if (length == 0 || length + 1 > capacity) return false;

    int slash = -1;
    for (uint32_t i = 0; i < length; i++) {
        char c = path[i];
        if (c == '/') {
            if (slash >= 0) return false;
            slash = (int)i;
        } else if (!IsRepoNameChar(c)) {
            return false;
        }
    }
    if (slash <= 0 || slash == (int)length - 1) return false;

    for (uint32_t i = 0; i < length; i++) output[i] = path[i];
    output[length] = '\0';
    return true;
}

bool CopyOptionalField(const char* body, const jsmntok_t* tokens, int tokenCount, int object,
                       const char* key, char* output, uint32_t capacity, const char* fallback) {
    int value = ObjectFind(body, tokens, tokenCount, object, key);
    if (value < 0) {
        return StoreStrCopy(output, capacity, fallback);
    }
    if (!TokenIsString(&tokens[value])) return false;
    return TokenCopy(body, &tokens[value], output, capacity);
}

bool ParseReleaseDocument(const char* body, uint32_t length, jsmntok_t* tokens, int tokenCapacity,
                          char* tagOut, uint32_t tagCapacity, ReleaseAsset* assets,
                          uint32_t maxAssets, uint32_t* assetCountOut, char* error,
                          uint32_t errorCapacity) {
    if (body == nullptr || tokens == nullptr || tagOut == nullptr || tagCapacity == 0 ||
        assets == nullptr || assetCountOut == nullptr) {
        StoreStrCopy(error, errorCapacity, "release parser is not configured");
        return false;
    }
    *assetCountOut = 0;

    jsmn_parser parser;
    jsmn_init(&parser);
    int tokenCount = jsmn_parse(&parser, body, length, tokens, tokenCapacity);
    if (tokenCount < 0) {
        StoreFormat(error, errorCapacity, "release JSON parse failed: %d", tokenCount);
        return false;
    }
    if (tokenCount == 0 || tokens[0].type != JSMN_OBJECT) {
        StoreStrCopy(error, errorCapacity, "release response is not an object");
        return false;
    }

    int tag = ObjectFind(body, tokens, tokenCount, 0, "tag_name");
    if (tag < 0 || !TokenIsString(&tokens[tag]) ||
        !TokenCopy(body, &tokens[tag], tagOut, tagCapacity)) {
        StoreStrCopy(error, errorCapacity, "release tag is missing");
        return false;
    }

    int assetsToken = ObjectFind(body, tokens, tokenCount, 0, "assets");
    if (assetsToken < 0 || tokens[assetsToken].type != JSMN_ARRAY) {
        StoreStrCopy(error, errorCapacity, "release has no assets");
        return false;
    }

    uint32_t assetCount = (uint32_t)tokens[assetsToken].size;
    uint32_t stored = 0;
    for (uint32_t i = 0; i < assetCount; i++) {
        if (stored >= maxAssets) {
            StoreStrCopy(error, errorCapacity, "release has too many assets");
            return false;
        }

        int asset = ArrayElement(tokens, tokenCount, assetsToken, (int)i);
        if (asset < 0 || tokens[asset].type != JSMN_OBJECT) continue;

        int name = ObjectFind(body, tokens, tokenCount, asset, "name");
        int url = ObjectFind(body, tokens, tokenCount, asset, "url");
        int size = ObjectFind(body, tokens, tokenCount, asset, "size");
        int digest = ObjectFind(body, tokens, tokenCount, asset, "digest");

        if (name < 0 || url < 0 || size < 0 || !TokenIsString(&tokens[name]) ||
            !TokenIsString(&tokens[url])) {
            continue;
        }

        ReleaseAsset& entry = assets[stored];
        memset(&entry, 0, sizeof(entry));
        if (!TokenCopy(body, &tokens[name], entry.name, sizeof(entry.name)) ||
            !TokenCopy(body, &tokens[url], entry.apiUrl, sizeof(entry.apiUrl)) ||
            !TokenU32(body, &tokens[size], &entry.size) || entry.size == 0) {
            continue;
        }
        if (!StoreStrStartsWith(entry.apiUrl, "https://api.github.com/")) continue;
        if (digest >= 0) {
            entry.hasDigest =
                ParseGitHubDigest(body, &tokens[digest], entry.sha256, sizeof(entry.sha256));
        }
        stored++;
    }

    *assetCountOut = stored;
    return true;
}

bool ParseRelease(Catalog* catalog, char* error, uint32_t errorCapacity) {
    return ParseReleaseDocument(g_releaseBody, g_releaseLength, g_releaseTokens,
                                kReleaseTokenCapacity, catalog->releaseTag,
                                sizeof(catalog->releaseTag), g_releaseAssets, APPSTORE_MAX_ASSETS,
                                &g_releaseAssetCount, error, errorCapacity);
}

const ReleaseAsset* FindReleaseAsset(const char* name) {
    for (uint32_t i = 0; i < g_releaseAssetCount; i++) {
        if (StoreStrEqual(g_releaseAssets[i].name, name)) return &g_releaseAssets[i];
    }
    return nullptr;
}

bool ParseCatalog(Catalog* catalog, char* error, uint32_t errorCapacity) {
    jsmn_parser parser;
    jsmn_init(&parser);
    int tokenCount =
        jsmn_parse(&parser, g_catalogBody, g_catalogLength, g_catalogTokens, kCatalogTokenCapacity);
    if (tokenCount < 0) {
        StoreFormat(error, errorCapacity, "catalog JSON parse failed: %d", tokenCount);
        return false;
    }
    if (tokenCount == 0 || g_catalogTokens[0].type != JSMN_OBJECT) {
        StoreStrCopy(error, errorCapacity, "catalog response is not an object");
        return false;
    }

    int schema = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, 0, "schema");
    uint32_t schemaVersion = 0;
    if (schema < 0 || !TokenU32(g_catalogBody, &g_catalogTokens[schema], &schemaVersion)) {
        StoreStrCopy(error, errorCapacity, "catalog schema is missing");
        return false;
    }
    if (schemaVersion != 1) {
        StoreFormat(error, errorCapacity, "unsupported catalog schema %u", schemaVersion);
        return false;
    }

    int catalogVersion =
        ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, 0, "catalogVersion");
    if (catalogVersion >= 0) {
        if (!TokenCopy(g_catalogBody, &g_catalogTokens[catalogVersion], catalog->catalogVersion,
                       sizeof(catalog->catalogVersion))) {
            StoreStrCopy(error, errorCapacity, "catalogVersion is too long");
            return false;
        }
    }

    int osVersion = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, 0, "osVersion");
    if (osVersion >= 0) {
        if (!TokenCopy(g_catalogBody, &g_catalogTokens[osVersion], catalog->osVersion,
                       sizeof(catalog->osVersion))) {
            StoreStrCopy(error, errorCapacity, "osVersion is too long");
            return false;
        }
    }

    int apps = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, 0, "apps");
    if (apps < 0 || g_catalogTokens[apps].type != JSMN_ARRAY) {
        StoreStrCopy(error, errorCapacity, "catalog has no apps array");
        return false;
    }
    uint32_t appCount = (uint32_t)g_catalogTokens[apps].size;
    if (appCount > APPSTORE_MAX_APPS) {
        StoreStrCopy(error, errorCapacity, "catalog has too many apps");
        return false;
    }

    catalog->appCount = 0;
    for (uint32_t i = 0; i < appCount; i++) {
        int appToken = ArrayElement(g_catalogTokens, tokenCount, apps, (int)i);
        if (appToken < 0 || g_catalogTokens[appToken].type != JSMN_OBJECT) {
            StoreFormat(error, errorCapacity, "app %u is not an object", i + 1);
            return false;
        }

        CatalogApp app;
        memset(&app, 0, sizeof(app));
        app.compatible = true;

        int id = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, appToken, "id");
        int name = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, appToken, "name");
        int version = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, appToken, "version");
        if (id < 0 || name < 0 || version < 0 ||
            !TokenCopy(g_catalogBody, &g_catalogTokens[id], app.id, sizeof(app.id)) ||
            !TokenCopy(g_catalogBody, &g_catalogTokens[name], app.name, sizeof(app.name)) ||
            !TokenCopy(g_catalogBody, &g_catalogTokens[version], app.version,
                       sizeof(app.version))) {
            StoreFormat(error, errorCapacity, "app %u is missing id, name or version", i + 1);
            return false;
        }
        if (!IsSafeField(app.id)) {
            StoreFormat(error, errorCapacity, "app %u has an unsafe id", i + 1);
            return false;
        }

        char repoRaw[APPSTORE_SOURCE_FIELD_LEN] = "";
        if (!CopyOptionalField(g_catalogBody, g_catalogTokens, tokenCount, appToken, "description",
                               app.description, sizeof(app.description), "") ||
            !CopyOptionalField(g_catalogBody, g_catalogTokens, tokenCount, appToken, "icon",
                               app.icon, sizeof(app.icon), "fa-file") ||
            !CopyOptionalField(g_catalogBody, g_catalogTokens, tokenCount, appToken, "category",
                               app.category, sizeof(app.category), "Other") ||
            !CopyOptionalField(g_catalogBody, g_catalogTokens, tokenCount, appToken, "license",
                               app.license, sizeof(app.license), "Unknown") ||
            !CopyOptionalField(g_catalogBody, g_catalogTokens, tokenCount, appToken, "source",
                               app.source, sizeof(app.source), "") ||
            !CopyOptionalField(g_catalogBody, g_catalogTokens, tokenCount, appToken, "minOsVersion",
                               app.minOsVersion, sizeof(app.minOsVersion), "") ||
            !CopyOptionalField(g_catalogBody, g_catalogTokens, tokenCount, appToken, "repo",
                               repoRaw, sizeof(repoRaw), "") ||
            !CopyOptionalField(g_catalogBody, g_catalogTokens, tokenCount, appToken, "iconAsset",
                               app.iconAsset, sizeof(app.iconAsset), "") ||
            !CopyOptionalField(g_catalogBody, g_catalogTokens, tokenCount, appToken, "bannerAsset",
                               app.bannerAsset, sizeof(app.bannerAsset), "")) {
            StoreFormat(error, errorCapacity, "app %u has a field that is too long", i + 1);
            return false;
        }
        if (repoRaw[0] != '\0' && !NormalizeGitHubRepo(repoRaw, app.repo, sizeof(app.repo))) {
            StoreFormat(error, errorCapacity, "app %u has an invalid repo URL", i + 1);
            return false;
        }

        if (app.minOsVersion[0] != '\0' &&
            StoreVersionCompare(APPSTORE_OS_VERSION, app.minOsVersion) < 0) {
            app.compatible = false;
        }

        int asset = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, appToken, "asset");
        int url = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, appToken, "url");
        if (asset >= 0 && !TokenCopy(g_catalogBody, &g_catalogTokens[asset], app.assetName,
                                     sizeof(app.assetName))) {
            StoreFormat(error, errorCapacity, "app %u asset name is too long", i + 1);
            return false;
        }

        if (app.repo[0] != '\0') {
            if (url >= 0) {
                StoreFormat(error, errorCapacity, "app %u specifies both repo and url", i + 1);
                return false;
            }
            if (asset < 0) {
                StoreFormat(error, errorCapacity, "app %u needs an asset name with repo", i + 1);
                return false;
            }
            // The latest release of the app's own repository is resolved on demand.
            app.resolved = false;
        } else if (asset >= 0) {
            if (url >= 0) {
                StoreFormat(error, errorCapacity, "app %u specifies both asset and url", i + 1);
                return false;
            }
            const ReleaseAsset* releaseAsset = FindReleaseAsset(app.assetName);
            if (releaseAsset == nullptr) {
                StoreFormat(error, errorCapacity, "app %u asset '%s' is not in the release", i + 1,
                            app.assetName);
                return false;
            }
            if (!releaseAsset->hasDigest) {
                StoreFormat(error, errorCapacity, "app %u asset has no SHA-256 digest", i + 1);
                return false;
            }
            StoreStrCopy(app.downloadUrl, sizeof(app.downloadUrl), releaseAsset->apiUrl);
            StoreStrCopy(app.sha256, sizeof(app.sha256), releaseAsset->sha256);
            app.size = releaseAsset->size;
            app.resolved = true;
        } else if (url >= 0) {
            if (!TokenCopy(g_catalogBody, &g_catalogTokens[url], app.downloadUrl,
                           sizeof(app.downloadUrl)) ||
                !StoreStrStartsWith(app.downloadUrl, "https://")) {
                StoreFormat(error, errorCapacity, "app %u has an invalid download URL", i + 1);
                return false;
            }
            int size = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, appToken, "size");
            int sha = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, appToken, "sha256");
            if (size < 0 || !TokenU32(g_catalogBody, &g_catalogTokens[size], &app.size) ||
                app.size == 0 || sha < 0 ||
                !ParseSha256Value(g_catalogBody, &g_catalogTokens[sha], app.sha256,
                                  sizeof(app.sha256))) {
                StoreFormat(error, errorCapacity, "app %u needs size and sha256 with url", i + 1);
                return false;
            }
            app.resolved = true;
        } else {
            StoreFormat(error, errorCapacity, "app %u needs repo, asset or url", i + 1);
            return false;
        }

        if (app.resolved && (app.size == 0 || app.size > APPSTORE_MAX_APP_BYTES)) {
            StoreFormat(error, errorCapacity, "app %u size is out of range", i + 1);
            return false;
        }

        int binary = ObjectFind(g_catalogBody, g_catalogTokens, tokenCount, appToken, "binary");
        if (binary >= 0) {
            if (!TokenCopy(g_catalogBody, &g_catalogTokens[binary], app.binary,
                           sizeof(app.binary))) {
                StoreFormat(error, errorCapacity, "app %u binary name is too long", i + 1);
                return false;
            }
        } else if (app.assetName[0] != '\0') {
            StoreStrCopy(app.binary, sizeof(app.binary), app.assetName);
        } else {
            const char* slash = nullptr;
            for (const char* p = app.downloadUrl; *p != '\0'; p++) {
                if (*p == '/') slash = p;
            }
            StoreStrCopy(app.binary, sizeof(app.binary), slash ? slash + 1 : app.downloadUrl);
            for (uint32_t c = 0; app.binary[c] != '\0'; c++) {
                if (app.binary[c] == '?') {
                    app.binary[c] = '\0';
                    break;
                }
            }
        }
        if (!IsSafeField(app.binary)) {
            StoreFormat(error, errorCapacity, "app %u has an unsafe binary name", i + 1);
            return false;
        }

        if (app.iconAsset[0] != '\0') {
            const ReleaseAsset* icon = FindReleaseAsset(app.iconAsset);
            if (icon == nullptr) {
                StoreFormat(error, errorCapacity, "app %u icon asset '%s' is not in the release",
                            i + 1, app.iconAsset);
                return false;
            }
            if (!icon->hasDigest) {
                StoreFormat(error, errorCapacity, "app %u icon asset has no SHA-256 digest", i + 1);
                return false;
            }
            StoreStrCopy(app.iconRef.url, sizeof(app.iconRef.url), icon->apiUrl);
            StoreStrCopy(app.iconRef.sha256, sizeof(app.iconRef.sha256), icon->sha256);
            app.iconRef.size = icon->size;
        }
        if (app.bannerAsset[0] != '\0') {
            const ReleaseAsset* banner = FindReleaseAsset(app.bannerAsset);
            if (banner == nullptr) {
                StoreFormat(error, errorCapacity, "app %u banner asset '%s' is not in the release",
                            i + 1, app.bannerAsset);
                return false;
            }
            if (!banner->hasDigest) {
                StoreFormat(error, errorCapacity, "app %u banner asset has no SHA-256 digest",
                            i + 1);
                return false;
            }
            StoreStrCopy(app.bannerRef.url, sizeof(app.bannerRef.url), banner->apiUrl);
            StoreStrCopy(app.bannerRef.sha256, sizeof(app.bannerRef.sha256), banner->sha256);
            app.bannerRef.size = banner->size;
        }

        catalog->apps[catalog->appCount++] = app;
    }

    if (catalog->appCount == 0) {
        StoreStrCopy(error, errorCapacity, "catalog contains no apps");
        return false;
    }
    return true;
}

}  // namespace

bool CatalogFetch(Catalog* catalog, char* error, uint32_t errorCapacity) {
    if (catalog == nullptr) return false;
    if (error != nullptr && errorCapacity != 0) error[0] = '\0';
    memset(catalog, 0, sizeof(*catalog));

    char releaseUrl[256];
    if (APPSTORE_CATALOG_REPO[0] == '\0') {
        StoreStrCopy(error, errorCapacity, "catalog repository is not configured");
        return false;
    }
    StoreFormat(releaseUrl, sizeof(releaseUrl), "https://api.github.com/repos/%s/releases/latest",
                APPSTORE_CATALOG_REPO);

    printf("[AppStore] fetching catalog release %s\n", APPSTORE_CATALOG_REPO);
    if (!DownloadToBuffer(releaseUrl, "application/vnd.github+json", g_releaseBody,
                          sizeof(g_releaseBody), 0, nullptr, &g_releaseLength, error,
                          errorCapacity)) {
        return false;
    }

    if (!ParseRelease(catalog, error, errorCapacity)) return false;

    const ReleaseAsset* catalogAsset = FindReleaseAsset(APPSTORE_CATALOG_ASSET);
    if (catalogAsset == nullptr) {
        StoreFormat(error, errorCapacity, "release has no %s asset", APPSTORE_CATALOG_ASSET);
        return false;
    }
    if (!catalogAsset->hasDigest) {
        StoreStrCopy(error, errorCapacity, "catalog asset has no SHA-256 digest");
        return false;
    }

    if (!DownloadToBuffer(catalogAsset->apiUrl, "application/octet-stream", g_catalogBody,
                          sizeof(g_catalogBody), catalogAsset->size, catalogAsset->sha256,
                          &g_catalogLength, error, errorCapacity)) {
        return false;
    }
    if (!ParseCatalog(catalog, error, errorCapacity)) return false;

    printf("[AppStore] catalog %s: %u apps\n", catalog->catalogVersion, catalog->appCount);
    return true;
}

bool CatalogResolveApp(CatalogApp* app, char* error, uint32_t errorCapacity) {
    if (error != nullptr && errorCapacity != 0) error[0] = '\0';
    if (app == nullptr) {
        StoreStrCopy(error, errorCapacity, "no application selected");
        return false;
    }
    if (app->resolved) return true;
    if (app->repo[0] == '\0') {
        StoreStrCopy(error, errorCapacity, "application has no repository");
        return false;
    }
    if (app->assetName[0] == '\0') {
        StoreStrCopy(error, errorCapacity, "application has no release asset");
        return false;
    }

    char releaseUrl[320];
    StoreFormat(releaseUrl, sizeof(releaseUrl), "https://api.github.com/repos/%s/releases/latest",
                app->repo);
    printf("[AppStore] resolving %s from %s\n", app->id, app->repo);
    if (!DownloadToBuffer(releaseUrl, "application/vnd.github+json", g_releaseBody,
                          sizeof(g_releaseBody), 0, nullptr, &g_releaseLength, error,
                          errorCapacity)) {
        return false;
    }

    char tag[APPSTORE_NAME_FIELD_LEN] = "";
    if (!ParseReleaseDocument(g_releaseBody, g_releaseLength, g_releaseTokens,
                              kReleaseTokenCapacity, tag, sizeof(tag), g_releaseAssets,
                              APPSTORE_MAX_ASSETS, &g_releaseAssetCount, error, errorCapacity)) {
        return false;
    }

    const ReleaseAsset* asset = FindReleaseAsset(app->assetName);
    if (asset == nullptr) {
        StoreFormat(error, errorCapacity, "asset '%s' is not in %s", app->assetName, app->repo);
        return false;
    }
    if (!asset->hasDigest) {
        StoreFormat(error, errorCapacity, "asset '%s' has no SHA-256 digest", app->assetName);
        return false;
    }
    if (asset->size == 0 || asset->size > APPSTORE_MAX_APP_BYTES) {
        StoreFormat(error, errorCapacity, "asset '%s' size is out of range", app->assetName);
        return false;
    }

    StoreStrCopy(app->downloadUrl, sizeof(app->downloadUrl), asset->apiUrl);
    StoreStrCopy(app->sha256, sizeof(app->sha256), asset->sha256);
    app->size = asset->size;
    app->resolved = true;
    return true;
}
