#!/usr/bin/env bash
#
# Validate that every first-party C/C++ source and header file begins with the
# exact text of the project LICENSE wrapped in a /* ... */ block comment, that
# every first-party NASM source (.asm) begins with the same text wrapped in ;
# line comments, and that every first-party GNU ld linker script (.ld) begins
# with the same /* ... */ block.
#
# Third-party / borrowed code and generated tool output are skipped (see
# EXCLUDE_PATHS below); those files must keep their own upstream headers.
#
# Exit status:
#   0 = all first-party headers are correct
#   1 = at least one first-party file is missing the license header
#       (the offending file names are printed on stdout)

set -u

cd "$(dirname "${BASH_SOURCE[0]}")" || exit 3

LICENSE_FILE="LICENSE"
LICENSE_TEXT="$(cat "$LICENSE_FILE")" || exit 3

# Paths that must NEVER be license-checked:
#   - build/                : generated kernel/user artifacts
#   - tools/                : host-side installer tooling
#   - include/stb/          : stb_truetype (Sean Barrett, public domain)
#   - core/tlsf/, include/core/tlsf/ : TLSF allocator (Matthew Conte)
#   - stdlib/math/          : fdlibm math routines (Sun Microsystems)
#   - stdlib/string/        : PDCLib string routines (public domain)
#   - include/ctype.h, include/string.h, include/stdlib/fdlibm.h :
#                            standalone PDCLib / fdlibm headers
#
# Upstream ChaN FatFs sources are excluded at file level so the project's own
# ATA glue (core/filesystem/FatFs/diskio.cpp and
# core/filesystem/FatFs/FatFsWrapper.cpp) IS still license-checked:
#   - core/filesystem/FatFs/ff.c, ffunicode.c          (FatFs core)
#   - include/core/filesystem/FatFs/ff.h, ffconf.h, diskio.h (FatFs API)
EXCLUDE_PATHS=(
    ./build
    ./tools
    ./include/stb
    ./core/tlsf
    ./include/core/tlsf
    ./stdlib/math
    ./stdlib/string
    ./core/filesystem/FatFs/ff.c
    ./core/filesystem/FatFs/ffunicode.c
    ./include/core/filesystem/FatFs/ff.h
    ./include/core/filesystem/FatFs/ffconf.h
    ./include/core/filesystem/FatFs/diskio.h
    ./include/ctype.h
    ./include/string.h
    ./include/stdlib/fdlibm.h
)

is_excluded() {
    local f=$1
    local p
    for p in "${EXCLUDE_PATHS[@]}"; do
        if [[ $f == "$p" || $f == "$p"/* ]]; then
            return 0
        fi
    done
    return 1
}

# Strip block-comment decoration (`/*`, `*/`, leading `*`) so we compare the
# real text of the header against the real text of the LICENSE file.
normalize() {
    sed \
        -e 's@^[[:space:]]*/\*@@' \
        -e 's@\*/[[:space:]]*$@@' \
        -e 's@^[[:space:]]*\*[[:space:]]*@@' \
        -e 's@^[[:space:]]*//[[:space:]]*@@' \
        -e 's@^[[:space:]]*;[[:space:]]*@@' \
        -e 's@[[:space:]]*$@@' \
        -e '/^$/d'
}

EXPECTED="$(printf '%s\n' "$LICENSE_TEXT" | normalize)"
# The header block is the license (N lines) plus the opening `/*` and closing `*/`.
EXPECTED_LINES=$(($(wc -l < "$LICENSE_FILE") + 2))

echo "--- Checking File Headers ---"
ERROR=0
COUNT=0

while IFS= read -r -d '' f; do
    if is_excluded "$f"; then
        continue
    fi
    COUNT=$((COUNT + 1))

    header=$(head -n "$EXPECTED_LINES" "$f")
    actual=$(printf '%s\n' "$header" | normalize)

    if [[ $actual != "$EXPECTED" ]]; then
        echo "MISSING LICENSE HEADER: $f"
        ERROR=1
    fi
done < <(find . -type f \( -name "*.cpp" -o -name "*.c" -o -name "*.h" -o -name "*.hpp" -o -name "*.asm" -o -name "*.ld" \) -print0)

if [ $ERROR -eq 1 ]; then
    echo "FAILED: some first-party files are missing the license header."
    exit 1
else
    echo "All $COUNT first-party files have the correct license header."
    exit 0
fi
