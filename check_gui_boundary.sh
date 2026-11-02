#!/usr/bin/env bash
#
# Enforce the GUI portability boundary: the widget/event/render layer must not
# reach into the kernel directly. Only an explicit allowlist of kernel-only GUI
# glue files may include <core/...>.

set -u

cd "$(dirname "${BASH_SOURCE[0]}")" || exit 3

# Files that legitimately bind the portable GUI layer to kernel subsystems.
ALLOWLIST=(
    gui/Hgui.cpp
    gui/bmp.cpp
    gui/bootanim.cpp
    gui/desktop.cpp
    gui/taskbar.cpp
    gui/messagebox.cpp
    gui/infodialog.cpp
    gui/terminalview.cpp
    gui/guiplat_kernel.cpp
    gui/widgetHostKernel.cpp
    gui/fonts/fontKernel.cpp
)

is_allowed() {
    local f=$1 p
    for p in "${ALLOWLIST[@]}"; do
        [[ $f == "$p" ]] && return 0
    done
    return 1
}

echo "--- Checking GUI boundary ---"
ERROR=0
COUNT=0

while IFS= read -r -d '' f; do
    COUNT=$((COUNT + 1))
    if is_allowed "$f"; then
        continue
    fi
    if grep -q '#include <core/' "$f"; then
        echo "GUI BOUNDARY VIOLATION: $f includes core/"
        grep -n '#include <core/' "$f"
        ERROR=1
    fi
done < <(find gui -type f -name '*.cpp' -print0)

if [ $ERROR -eq 1 ]; then
    echo "FAILED: portable GUI sources must not include kernel headers."
    exit 1
fi

echo "GUI boundary clean ($COUNT files checked)."
exit 0
