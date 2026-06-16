#!/bin/bash

FIX_MODE=0
if [ "$1" == "--fix" ]; then
    FIX_MODE=1
    echo "--- Checking and FIXING End-of-File Newlines ---"
else
    echo "--- Checking End-of-File Newlines ---"
fi

ERROR=0

while IFS= read -r -d '' f; do
    # Skip build/tool directories at any depth
    if [[ $f == */build/* || $f == */build ]]; then continue; fi
    if [[ $f == */tools/* || $f == */tools ]]; then continue; fi
    if [[ -s "$f" && -n "$(tail -c 1 "$f")" ]]; then
        if [ $FIX_MODE -eq 1 ]; then
            # Append a newline
            echo "" >> "$f"
            echo "Fixed: $f"
        else
            echo "Missing Newline: $f"
            ERROR=1
        fi
    fi
done < <(find . -type f \( -name "*.cpp" -o -name "*.c" -o -name "*.h" -o -name "*.asm" -o -name "*.s" -o -name "*.S" \) -print0)

if [ $FIX_MODE -eq 0 ]; then
    if [ $ERROR -eq 1 ]; then
        echo "FAILED: Some files are missing the final newline."
        exit 1
    else
        echo "All files look good."
        exit 0
    fi
fi
