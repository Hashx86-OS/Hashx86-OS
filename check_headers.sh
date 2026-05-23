#!/bin/bash

# Define the list of Strings that MUST exist
REQUIRED_TAGS=("@file" "@brief" "@date" "@version")

echo "--- Checking File Headers ---"
ERROR=0

# Find all .cpp and .c
while IFS= read -r -d '' f; do
    # Skip "build" or "tools" folders
    if [[ $f == *"./build"* ]]; then continue; fi
    if [[ $f == *"./tools"* ]]; then continue; fi

    # Read the first 15 lines of the file
    header_content=$(head -n 15 "$f")

    # Check for EACH tag in the list
    for tag in "${REQUIRED_TAGS[@]}"; do
        if ! echo "$header_content" | grep -q "$tag"; then
            echo "MISSING TAG [$tag] in: $f"
            ERROR=1
        fi
    done
done < <(find . -type f \( -name "*.cpp" -o -name "*.c" -o -name "*.h" -o -name "*.hpp" \) -print0)

if [ $ERROR -eq 1 ]; then
    echo "FAILED: Some files are missing header information."
    exit 1
else
    echo "All files have valid headers."
    exit 0
fi
