#!/usr/bin/env bash
#
# Rewrite first-party C/C++ source and header files so that every file begins
# with the exact text of the LICENSE file wrapped in a /* ... */ block comment.
#
# This script is idempotent and safe to re-run: it strips any existing leading
# block-comment banner (e.g. the legacy "@file/@brief/@date/@version" Doxygen
# headers) together with blank padding, then prepends the license header.
# Second-party and third-party code is never touched (see EXCLUDE_PATHS).
#
# Usage:
#   tools/license_headers.sh           # apply headers to all first-party files
#   tools/license_headers.sh --dry-run # report what would change, change nothing

set -eu

DRY_RUN=0
if [ "${1:-}" = "--dry-run" ]; then
    DRY_RUN=1
fi

# Resolve the repository root (this script lives in <root>/tools/).
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

LICENSE_FILE="LICENSE"

# Paths that must NEVER be modified (same list as in check_headers.sh):
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
# core/filesystem/FatFs/FatFsWrapper.cpp) IS still licensed:
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

# Render the license as a /* ... */ block comment. Every LICENSE line becomes
# " * <line>" and every blank LICENSE line becomes a bare " *" line.
build_header() {
    printf '/*\n'
    while IFS= read -r line; do
        if [ -z "$line" ]; then
            printf ' *\n'
        else
            printf ' * %s\n' "$line"
        fi
    done < "$LICENSE_FILE"
    printf ' */\n'
}

HEADER="$(build_header)"

# Strip the leading comment banner and blank padding from a file, printing the
# remaining body. Stops at the first code / preprocessor line, so genuine
# comments that are not a top-of-file banner (e.g. "// Widget Base Class")
# are preserved.
strip_leading_banner() {
    awk '
        BEGIN { skip = 1; inblock = 0 }
        {
            if (skip && inblock == 0 && $0 ~ /^[[:space:]]*\/\*/) { inblock = 1; next }
            if (inblock) {
                if ($0 ~ /\*\//) inblock = 0
                next
            }
            if (skip && $0 ~ /^[[:space:]]*$/) next
            skip = 0
            print
        }
    ' "$1"
}

echo "--- Applying License Headers ---"
CHANGED=0
COUNT=0

while IFS= read -r -d '' f; do
    if is_excluded "$f"; then
        continue
    fi
    COUNT=$((COUNT + 1))

    new_content="${HEADER}"$'\n\n'"$(strip_leading_banner "$f")"
    current="$(cat "$f")"

    if [[ $current != "$new_content" ]]; then
        echo "HEADER -> $f"
        if [ $DRY_RUN -eq 0 ]; then
            printf '%s\n' "$new_content" > "$f"
        fi
        CHANGED=$((CHANGED + 1))
    fi
done < <(find . -type f \( -name "*.cpp" -o -name "*.c" -o -name "*.h" -o -name "*.hpp" \) -print0)

if [ $DRY_RUN -eq 1 ]; then
    echo "Scanned $COUNT first-party files; $CHANGED would change (dry run)."
else
    echo "Scanned $COUNT first-party files; $CHANGED updated."
fi