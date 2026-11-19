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

#include "GitHub.h"

#include "HttpClient.h"
#include "StoreUtil.h"

#include <Hx86/debug.h>

#define JSMN_STATIC
#include <jsmn.h>

namespace {

constexpr const char* kReleaseApi =
    "https://api.github.com/repos/Hashx86-OS/Quake-Hx86/releases/latest";
constexpr uint32_t kMaxApiBody = 65536;
constexpr uint32_t kMaxTokens = 4096;

// Pinned fallback for the current Quake release. The GitHub API is preferred
// so newer releases are picked up automatically; this entry keeps the store
// usable when the API is rate-limited or unreachable. Replace it when the
// remote catalog lands.
const GitHubRelease kCatalogFallback = {
    "v0.1.0",
    "https://github.com/Hashx86-OS/Quake-Hx86/releases/download/v0.1.0/Quake.bin",
    "https://api.github.com/repos/Hashx86-OS/Quake-Hx86/releases/assets/768968019",
    "4e0751da9ce6b29e1f7d2e22448400caca996b0e4c3113eb33b2ff23256b788b",
    3564760,
    true,
};

char g_apiBody[kMaxApiBody];
uint32_t g_apiLength = 0;
jsmntok_t g_tokens[kMaxTokens];

bool ApiSink(void* context, const uint8_t* data, uint32_t length) {
    (void)context;
    if (length > kMaxApiBody - 1 - g_apiLength) return false;
    for (uint32_t i = 0; i < length; i++) g_apiBody[g_apiLength + i] = (char)data[i];
    g_apiLength += length;
    g_apiBody[g_apiLength] = '\0';
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

bool TokenCopy(const char* text, const jsmntok_t* token, char* output, uint32_t capacity) {
    if (text == nullptr || token == nullptr || output == nullptr || capacity == 0) return false;
    uint32_t length = (uint32_t)(token->end - token->start);
    if (length + 1 > capacity) return false;
    for (uint32_t i = 0; i < length; i++) output[i] = text[token->start + (int)i];
    output[length] = '\0';
    return true;
}

bool TokenU32(const char* text, const jsmntok_t* token, uint32_t* value) {
    if (token == nullptr || token->type != JSMN_PRIMITIVE) return false;
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

bool Contains(const char* text, const char* needle) {
    uint32_t needleLength = StoreStrLen(needle);
    if (needleLength == 0) return true;
    for (; *text != '\0'; text++) {
        uint32_t i = 0;
        while (i < needleLength && text[i] != '\0' && text[i] == needle[i]) i++;
        if (i == needleLength) return true;
    }
    return false;
}

}  // namespace

static bool ResolveFromApi(GitHubRelease* release, char* error, uint32_t errorCapacity) {
    if (release == nullptr) return false;
    release->tag[0] = '\0';
    release->downloadUrl[0] = '\0';
    release->apiUrl[0] = '\0';
    release->sha256[0] = '\0';
    release->size = 0;
    release->fromCatalogFallback = false;

    g_apiLength = 0;
    g_apiBody[0] = '\0';

    StoreHttpResponse response = {};
    if (!StoreHttpGet(kReleaseApi, "application/vnd.github+json", ApiSink, nullptr, &response,
                      error, errorCapacity)) {
        return false;
    }

    jsmn_parser parser;
    jsmn_init(&parser);
    int tokenCount = jsmn_parse(&parser, g_apiBody, g_apiLength, g_tokens, kMaxTokens);
    if (tokenCount < 0) {
        StoreFormat(error, errorCapacity, "GitHub JSON parse failed: %d", tokenCount);
        return false;
    }
    if (tokenCount == 0 || g_tokens[0].type != JSMN_OBJECT) {
        StoreStrCopy(error, errorCapacity, "GitHub API returned unexpected JSON");
        return false;
    }

    int assets = ObjectFind(g_apiBody, g_tokens, tokenCount, 0, "assets");
    if (assets < 0 || g_tokens[assets].type != JSMN_ARRAY) {
        StoreStrCopy(error, errorCapacity, "GitHub release has no assets");
        return false;
    }

    int selected = -1;
    uint32_t assetCount = (uint32_t)g_tokens[assets].size;
    for (uint32_t i = 0; i < assetCount; i++) {
        int asset = ArrayElement(g_tokens, tokenCount, assets, (int)i);
        if (asset < 0 || g_tokens[asset].type != JSMN_OBJECT) continue;
        int name = ObjectFind(g_apiBody, g_tokens, tokenCount, asset, "name");
        if (name >= 0 && TokenEquals(g_apiBody, &g_tokens[name], "Quake.bin")) {
            selected = asset;
            break;
        }
    }
    if (selected < 0) {
        StoreStrCopy(error, errorCapacity, "Quake.bin is missing from the latest release");
        return false;
    }

    int tag = ObjectFind(g_apiBody, g_tokens, tokenCount, 0, "tag_name");
    int url = ObjectFind(g_apiBody, g_tokens, tokenCount, selected, "browser_download_url");
    int apiUrl = ObjectFind(g_apiBody, g_tokens, tokenCount, selected, "url");
    int size = ObjectFind(g_apiBody, g_tokens, tokenCount, selected, "size");
    int digest = ObjectFind(g_apiBody, g_tokens, tokenCount, selected, "digest");

    if (tag < 0 || url < 0 || apiUrl < 0 || size < 0 || digest < 0) {
        StoreStrCopy(error, errorCapacity, "Release metadata is incomplete");
        return false;
    }

    if (!TokenCopy(g_apiBody, &g_tokens[tag], release->tag, sizeof(release->tag))) {
        StoreStrCopy(error, errorCapacity, "Release tag is too long");
        return false;
    }
    if (!TokenCopy(g_apiBody, &g_tokens[url], release->downloadUrl, sizeof(release->downloadUrl))) {
        StoreStrCopy(error, errorCapacity, "Release URL is too long");
        return false;
    }
    if (!TokenCopy(g_apiBody, &g_tokens[apiUrl], release->apiUrl, sizeof(release->apiUrl))) {
        StoreStrCopy(error, errorCapacity, "Release API URL is too long");
        return false;
    }
    if (!TokenU32(g_apiBody, &g_tokens[size], &release->size) || release->size == 0 ||
        release->size > 64u * 1024u * 1024u) {
        StoreStrCopy(error, errorCapacity, "Release asset size is invalid");
        return false;
    }

    if (g_tokens[digest].type != JSMN_STRING ||
        !StoreStrStartsWith(g_apiBody + g_tokens[digest].start, "sha256:")) {
        StoreStrCopy(error, errorCapacity, "Release asset has no SHA-256 digest");
        return false;
    }

    const char* digestText = g_apiBody + g_tokens[digest].start + 7;
    uint32_t digestLength = (uint32_t)(g_tokens[digest].end - g_tokens[digest].start - 7);
    if (digestLength != 64) {
        StoreStrCopy(error, errorCapacity, "Release digest is malformed");
        return false;
    }
    for (uint32_t i = 0; i < digestLength; i++) release->sha256[i] = digestText[i];
    release->sha256[64] = '\0';

    if (!StoreStrStartsWith(release->downloadUrl, "https://github.com/") ||
        !Contains(release->downloadUrl, "/releases/download/")) {
        StoreStrCopy(error, errorCapacity, "Release download URL is not trusted");
        return false;
    }
    if (!StoreStrStartsWith(release->apiUrl, "https://api.github.com/")) {
        StoreStrCopy(error, errorCapacity, "Release API URL is not trusted");
        return false;
    }

    return true;
}

bool GitHubResolveLatest(GitHubRelease* release, char* error, uint32_t errorCapacity) {
    if (release == nullptr) return false;

    if (ResolveFromApi(release, error, errorCapacity)) return true;

    printf("[AppStore] GitHub API unavailable (%s); using built-in catalog\n", error);
    *release = kCatalogFallback;
    return true;
}
