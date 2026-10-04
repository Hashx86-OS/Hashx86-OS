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

#pragma once

#include <stdint.h>

/**
 * SETTINGS_SCHEMA_VERSION - Version stamped into settings.json by the Settings
 * app and checked by the loader.
 *
 * Bump this when a change is not backwards compatible. A file carrying a
 * *newer* version than the kernel understands is ignored wholesale rather than
 * partially applied, so downgrading the OS never reads settings it cannot
 * honour. Forward-compatible additions (new keys) do not need a bump: unknown
 * keys are skipped.
 */
#define SETTINGS_SCHEMA_VERSION 1

/**
 * SETTINGS_MAX_BYTES - Upper bound on settings.json.
 *
 * The file is read fully into a fixed buffer and parsed in place, so this also
 * bounds the stack/static cost of loading. A settings file this large would
 * mean something is wrong; such a file is rejected rather than truncated.
 */
#define SETTINGS_MAX_BYTES 4096

/**
 * SETTINGS_MAX_TOKENS - jsmn token budget.
 *
 * Sized for the current schema with generous headroom for added keys. Running
 * out is reported as "too complex" instead of being retried, since a valid
 * settings file should never come close.
 */
#define SETTINGS_MAX_TOKENS 64

/**
 * struct Settings - In-memory view of the persisted settings.
 *
 * Every field is optional in the file. SettingsLoad() only overwrites the
 * fields actually present, so a caller pre-populated with defaults keeps them
 * for anything the file omits.
 */
struct Settings {
    uint16_t displayWidth;   // Preferred desktop width in pixels.
    uint16_t displayHeight;  // Preferred desktop height in pixels.
};

/**
 * SettingsDefaults() - Fill in the built-in defaults.
 * @s: Struct to initialise.
 *
 * These are the values the OS falls back to when there is no settings file, or
 * when the file is unreadable, malformed, or written by a newer schema.
 */
void SettingsDefaults(struct Settings* s);

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
bool SettingsLoad(struct Settings* s, const char* path);

/**
 * SettingsSchemaVersionOf() - Read only the schema_version of a settings file.
 * @path: Path to the settings file.
 * @out: Receives the version when the file parses and carries the key.
 *
 * Return: true if @out was written.
 */
bool SettingsSchemaVersionOf(const char* path, uint32_t* out);
