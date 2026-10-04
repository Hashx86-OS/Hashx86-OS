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

#define KDBG_COMPONENT "CORE:SETTINGS"

#include <core/filesystem/File.h>
#include <core/filesystem/FileSystem.h>
#include <core/globals.h>
#include <core/settings.h>
#include <debug.h>

#include <gui/config/config.h>
#include <string.h>

// JSMN_PARENT_LINKS records each token's enclosing token, which is what makes
// key lookup unambiguous. Without it, an object's `size` field counts its keys
// rather than its child tokens, so a "walk children while index < obj + size"
// loop silently stops before reaching later keys. Costs one int per token.
#define JSMN_PARENT_LINKS
#include <third_party/jsmn/jsmn.h>

/**
 * Settings JSON layout
 * --------------------
 *   {
 *     "schema_version": 1,
 *     "display": { "width": 1600, "height": 900 }
 *   }
 *
 * Written by the Settings app, read here at boot before the desktop is
 * created. Parsing is tolerant by design: unknown keys are skipped so new
 * settings can be added without a kernel change, and a bad value for one key
 * leaves just that key at its default.
 */

// Parsed in place, so the buffer is static rather than on the stack: this runs
// during boot on the BootMain worker thread, and 4 KiB is more than we want to
// assume that thread's stack can absorb.
static char s_settingsBuf[SETTINGS_MAX_BYTES + 1];
static jsmntok_t s_settingsTokens[SETTINGS_MAX_TOKENS];

/**
 * readSettingsFile() - Slurp a settings file into s_settingsBuf.
 * @path: File to read.
 * @outLen: Receives the byte count, excluding any stripped BOM.
 *
 * Return: true on success; false if the file is missing, empty, larger than
 *         SETTINGS_MAX_BYTES, or unreadable.
 */
static bool readSettingsFile(const char* path, uint32_t* outLen) {
    if (!g_bootPartition) return false;

    File* f = g_bootPartition->Open(path);
    if (!f) return false;

    // Reject an oversized file rather than truncating it: a half-parsed JSON
    // document is not a useful thing to reason about, and a legitimate
    // settings file is orders of magnitude below the limit.
    if (f->size == 0 || f->size > SETTINGS_MAX_BYTES) {
        f->Close();
        delete f;
        KDBG1("settings.json: implausible size %u, ignoring", (unsigned)f->size);
        return false;
    }

    int got = f->Read((uint8_t*)s_settingsBuf, f->size);
    f->Close();
    delete f;

    if (got <= 0) {
        KDBG1("settings.json: read returned %d, ignoring", got);
        return false;
    }

    // Skip a UTF-8 BOM. Editors on Windows add one and jsmn treats the bytes
    // as content, which would make the first key unparseable.
    uint32_t start = 0;
    if (got >= 3 && (uint8_t)s_settingsBuf[0] == 0xEF && (uint8_t)s_settingsBuf[1] == 0xBB &&
        (uint8_t)s_settingsBuf[2] == 0xBF) {
        start = 3;
    }

    uint32_t len = (uint32_t)got - start;
    for (uint32_t i = 0; i < len; i++) s_settingsBuf[i] = s_settingsBuf[start + i];
    s_settingsBuf[len] = '\0';

    *outLen = len;
    return true;
}

/**
 * parseSettingsBuffer() - Tokenize a settings document already in memory.
 * @text: NUL-terminated JSON text.
 * @len: Length in bytes, excluding the NUL.
 * @outCount: Receives the number of tokens produced.
 *
 * Split out from the file path so the parsing rules can be exercised without
 * going through the filesystem.
 *
 * Return: true if the text parsed into a JSON object, with tokens left in
 *         s_settingsTokens. False on any parse failure.
 */
static bool parseSettingsBuffer(const char* text, uint32_t len, int* outCount) {
    jsmn_parser parser;
    jsmn_init(&parser);

    int count = jsmn_parse(&parser, (char*)text, len, s_settingsTokens, SETTINGS_MAX_TOKENS);
    if (count < 1) {
        // Negative codes are jsmn errors. NOMEM in particular means the file
        // exceeded the token budget, which for a settings file means it is not
        // a settings file.
        KDBG1("settings.json: parse error %d, ignoring", count);
        return false;
    }

    if (s_settingsTokens[0].type != JSMN_OBJECT) {
        KDBG1("settings.json: root is not an object, ignoring");
        return false;
    }

    *outCount = count;
    return true;
}

/**
 * parseSettingsFile() - Read and tokenize a settings file.
 * @path: File to parse.
 * @outCount: Receives the number of tokens produced.
 *
 * Return: true if the file parsed into a JSON object, with tokens left in
 *         s_settingsTokens. False on any read or parse failure.
 */
static bool parseSettingsFile(const char* path, int* outCount) {
    uint32_t len = 0;
    if (!readSettingsFile(path, &len)) return false;
    return parseSettingsBuffer(s_settingsBuf, len, outCount);
}

/**
 * tokenTextLen() - Byte length of a token's text.
 * @t: Token to measure.
 *
 * Uses end - start. The `size` field is not a text length: primitives report
 * size 0, and a key string reports 1 because jsmn counts the value as a child
 * of the key rather than of the object.
 */
static uint32_t tokenTextLen(const jsmntok_t* t) {
    if (t->end < t->start) return 0;
    return (uint32_t)(t->end - t->start);
}

/**
 * tokenKeyEquals() - Compare a jsmn string token against a C string.
 * @text: Buffer the tokens were parsed from.
 * @t: Token to test.
 * @key: NUL-terminated key to compare against.
 *
 * This jsmn version reports token positions as integer offsets into the source
 * buffer rather than pointers. The comparison walks the token's recorded text
 * length instead of relying on jsmn's NUL-termination of string tokens, so a
 * key containing an embedded NUL cannot be tricked into a prefix match.
 */
static bool tokenKeyEquals(const char* text, const jsmntok_t* t, const char* key) {
    uint32_t len = tokenTextLen(t);
    if (len != strlen(key)) return false;
    for (uint32_t i = 0; i < len; i++) {
        if (text[t->start + i] != key[i]) return false;
    }
    return true;
}

/**
 * tokenDepthTo() - Nesting depth of a token relative to an ancestor.
 * @tokens: Token array.
 * @idx: Token to measure.
 * @ancestor: Expected ancestor token index.
 *
 * Return: Number of parent links from @idx up to @ancestor (1 for a direct
 *         child), or -1 if @ancestor is not in @idx's parent chain.
 */
static int tokenDepthTo(const jsmntok_t* tokens, int idx, int ancestor) {
    int depth = 0;
    while (idx != ancestor) {
        if (idx < 0) return -1;
        idx = tokens[idx].parent;
        depth++;
        if (depth > SETTINGS_MAX_TOKENS) return -1;  // Cycle guard.
    }
    return depth;
}

/**
 * objectLookup() - Find a key's value token inside a JSON object.
 * @text: Buffer the tokens were parsed from.
 * @tokens: Token array.
 * @count: Total token count.
 * @objIdx: Index of the object token.
 * @key: Key to find.
 *
 * Walks the object's direct children, which are its key strings, and returns
 * the token following the matching key. Nested values are stepped over by
 * depth, so an object inside a value cannot be mistaken for a key of the outer
 * object.
 *
 * Return: Index of the value token, or -1 if the key is absent.
 */
static int objectLookup(const char* text, const jsmntok_t* tokens, int count, int objIdx,
                        const char* key) {
    if (objIdx < 0 || objIdx >= count) return -1;
    if (tokens[objIdx].type != JSMN_OBJECT) return -1;

    int i = objIdx + 1;
    while (i < count && tokenDepthTo(tokens, i, objIdx) == 1) {
        if (tokens[i].type != JSMN_STRING) return -1;  // Malformed pair.
        if (tokenKeyEquals(text, &tokens[i], key)) {
            return (i + 1 < count) ? (i + 1) : -1;
        }

        // Step over the value token and anything nested inside it, landing on
        // the next direct child.
        int j = i + 1;
        while (j < count && tokenDepthTo(tokens, j, objIdx) != 1) j++;
        i = j;
    }

    return -1;
}

/**
 * tokenToU16() - Convert a jsmn primitive token to a bounded unsigned value.
 * @text: Buffer the tokens were parsed from.
 * @t: Token to convert.
 * @out: Receives the value.
 *
 * Only plain non-negative decimal integers are accepted. A quoted "1600", a
 * float, a negative, or a value that overflows uint16_t is rejected rather
 * than coerced, so a typo in the file cannot turn into a wild resolution.
 *
 * Return: true if @out was written.
 */
static bool tokenToU16(const char* text, const jsmntok_t* t, uint16_t* out) {
    if (t->type != JSMN_PRIMITIVE) return false;

    uint32_t len = tokenTextLen(t);
    if (len == 0 || len > 5) return false;  // 65535 is five digits.

    uint32_t v = 0;
    for (uint32_t i = 0; i < len; i++) {
        char c = text[t->start + (int)i];
        if (c < '0' || c > '9') return false;
        v = (v * 10) + (uint32_t)(c - '0');
        if (v > 0xFFFF) return false;
    }

    if (v == 0) return false;  // 0 is never a usable geometry.
    *out = (uint16_t)v;
    return true;
}

/**
 * readDisplayMode() - Pull display.width/height out of a parsed document.
 * @tokens: Token array.
 * @count: Total token count.
 * @s: Struct to merge into; untouched if the keys are missing or invalid.
 */
static void readDisplayMode(const char* text, const jsmntok_t* tokens, int count,
                            struct Settings* s) {
    int displayIdx = objectLookup(text, tokens, count, 0, "display");
    if (displayIdx < 0) return;
    if (tokens[displayIdx].type != JSMN_OBJECT) {
        KDBG1("settings.json: \"display\" is not an object, ignoring");
        return;
    }

    uint16_t w, h;

    int wIdx = objectLookup(text, tokens, count, displayIdx, "width");
    if (wIdx >= 0) {
        if (tokenToU16(text, &tokens[wIdx], &w)) {
            s->displayWidth = w;
        } else {
            KDBG1("settings.json: bad display.width, keeping %u", (unsigned)s->displayWidth);
        }
    }

    int hIdx = objectLookup(text, tokens, count, displayIdx, "height");
    if (hIdx >= 0) {
        if (tokenToU16(text, &tokens[hIdx], &h)) {
            s->displayHeight = h;
        } else {
            KDBG1("settings.json: bad display.height, keeping %u", (unsigned)s->displayHeight);
        }
    }
}

/**
 * SettingsDefaults() - Fill in the built-in defaults.
 * @s: Struct to initialise.
 *
 * Mirrors GUI_SCREEN_WIDTH/GUI_SCREEN_HEIGHT, the geometry the desktop uses
 * when it has nothing better to go on.
 */
void SettingsDefaults(struct Settings* s) {
    if (!s) return;
    s->displayWidth = GUI_SCREEN_WIDTH;
    s->displayHeight = GUI_SCREEN_HEIGHT;
}

/**
 * SettingsLoad() - Merge settings.json into a defaults struct.
 * @s: Struct pre-populated with defaults; present keys are overwritten.
 * @path: Path to the settings file.
 *
 * The merge is deliberately partial and forgiving: a missing file, malformed
 * JSON, or an out-of-range value for one key leaves that key at its default
 * rather than discarding the whole file. That way a hand-edited file with one
 * bad line still restores the rest.
 *
 * Return: true if the file existed and parsed as a JSON object; false if it
 *         was absent, too large, or not valid JSON (in which case @s is
 *         untouched).
 */
bool SettingsLoad(struct Settings* s, const char* path) {
    if (!s || !path) return false;

    int count = 0;
    if (!parseSettingsFile(path, &count)) return false;

    // A file from a newer schema is skipped wholesale. Partially applying a
    // document whose meaning we may not fully understand is worse than
    // ignoring it, so a downgrade falls back to defaults instead of guessing.
    int verIdx = objectLookup(s_settingsBuf, s_settingsTokens, count, 0, "schema_version");
    if (verIdx >= 0) {
        uint16_t ver;
        if (tokenToU16(s_settingsBuf, &s_settingsTokens[verIdx], &ver) &&
            ver > SETTINGS_SCHEMA_VERSION) {
            KDBG1("settings.json: schema v%u is newer than v%d, ignoring", (unsigned)ver,
                  SETTINGS_SCHEMA_VERSION);
            return false;
        }
    }

    readDisplayMode(s_settingsBuf, s_settingsTokens, count, s);
    return true;
}

/**
 * SettingsSchemaVersionOf() - Read only the schema_version of a settings file.
 * @path: Path to the settings file.
 * @out: Receives the version when the file parses and carries the key.
 *
 * Return: true if @out was written.
 */
bool SettingsSchemaVersionOf(const char* path, uint32_t* out) {
    if (!path || !out) return false;

    int count = 0;
    if (!parseSettingsFile(path, &count)) return false;

    int verIdx = objectLookup(s_settingsBuf, s_settingsTokens, count, 0, "schema_version");
    if (verIdx < 0) return false;

    uint16_t ver;
    if (!tokenToU16(s_settingsBuf, &s_settingsTokens[verIdx], &ver)) return false;

    *out = ver;
    return true;
}
