#!/bin/bash
# newapp.sh — Create a new user-space app for Hashx86 OS
# Usage: bash newapp.sh

set -e

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
USER_PROG_DIR="$ROOT_DIR/user_prog"
TOP_MAKEFILE="$ROOT_DIR/Makefile"
PROG_MAKEFILE="$USER_PROG_DIR/Makefile"
LINKER_LD_SRC="$USER_PROG_DIR/MeMView/linker.ld"
PROG_H_SRC="$USER_PROG_DIR/MeMView/include/prog.h"

die() { echo "Error: $*" >&2; exit 1; }

# --- validate app name ---
while true; do
    read -r -p "App name: " APP_NAME
    APP_NAME="${APP_NAME#"${APP_NAME%%[![:space:]]*}"}"
    APP_NAME="${APP_NAME%"${APP_NAME##*[![:space:]]}"}"
    if [ -z "$APP_NAME" ]; then
        echo "  Name cannot be empty."
        continue
    fi
    if ! echo "$APP_NAME" | grep -qE '^[a-zA-Z_][a-zA-Z0-9_]*$'; then
        echo "  Name must start with a letter or underscore, alphanumeric only."
        continue
    fi
    break
done

NEW_DIR="$USER_PROG_DIR/$APP_NAME"
if [ -d "$NEW_DIR" ]; then
    die "Directory '$NEW_DIR' already exists."
fi
if [ -f "$NEW_DIR" ]; then
    die "File '$NEW_DIR' already exists."
fi

# --- select type ---
while true; do
    read -r -p "App type (1=GUI, 2=CLI): " APP_TYPE
    case "$APP_TYPE" in
        1) APP_TYPE_NAME="GUI"; break ;;
        2) APP_TYPE_NAME="CLI"; break ;;
        *) echo "  Enter 1 for GUI or 2 for CLI." ;;
    esac
done

echo "Creating $APP_TYPE_NAME app '$APP_NAME' ..."

# --- create directory structure ---
mkdir -p "$NEW_DIR/include"
cp "$LINKER_LD_SRC" "$NEW_DIR/linker.ld"
cp "$PROG_H_SRC" "$NEW_DIR/include/prog.h"

# --- generate prog.cpp ---
if [ "$APP_TYPE" = "2" ]; then
    cat > "$NEW_DIR/prog.cpp" << CLIEOF
#include <Hx86/Hx86.h>

HX86_DECLARE_APP(HX86_APP_CLI);

extern "C" void _start(void* arg) {
    init_sys(arg);

    printf("Hello from $APP_NAME!\n");
    printf("Press Enter to exit...\n");

    while (1) {
        char ch = 0;
        int32_t n = syscall_read(0, &ch, 1);
        if (n > 0 && (ch == '\n' || ch == '\r')) break;
        syscall_sleep(40);
    }

    syscall_exit_group(0);
}
CLIEOF
else
    cat > "$NEW_DIR/prog.cpp" << GUIEOF
#include <Hx86/Hgui/Hgui.h>
#include <Hx86/Hx86.h>
#include <Hx86/utils/string.h>

HX86_DECLARE_APP(HX86_APP_GUI);

class ${APP_NAME}App {
private:
    Window* mainWindow;
    Label* helloLabel;
    Button* clickButton;
    Label* statusLabel;
    int clickCount;

public:
    ${APP_NAME}App() : clickCount(0) {
        mainWindow = new Window(desktop, 200, 200, 400, 250);
        mainWindow->setWindowTitle("$APP_NAME");

        helloLabel = new Label(mainWindow, 20, 20, 360, 30, "Hello from $APP_NAME!");
        helloLabel->setSize(LARGE);
        mainWindow->AddChild(helloLabel);

        clickButton = new Button(mainWindow, 20, 70, 120, 30, "Click me");
        clickButton->OnClick(this, [](void* inst) {
            static_cast<${APP_NAME}App*>(inst)->onButtonClick();
        });
        mainWindow->AddChild(clickButton);

        statusLabel = new Label(mainWindow, 20, 120, 360, 25, "Click count: 0");
        mainWindow->AddChild(statusLabel);

        mainWindow->show();
    }

    void onButtonClick() {
        clickCount++;
        char buf[64] = "Click count: ";
        itoa(buf + 13, 10, clickCount);
        statusLabel->setText(buf);
    }
};

static ${APP_NAME}App* g_app = nullptr;

extern "C" void _start(void* arg) {
    init_sys(arg);
    init_graphics();
    g_app = new ${APP_NAME}App();
}
GUIEOF
fi

# --- generate Makefile ---
cat > "$NEW_DIR/Makefile" << 'MKEOF'
# user_prog/XXX/Makefile

GPP = g++
LD = ld

INCLUDES = -Iinclude -I../libhx86/include

CPPFLAGS = -m32 -g -ffreestanding -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-common $(INCLUDES)

LDFLAGS = -melf_i386 -L../../build/libhx86 -lhx86

BUILD_DIR = ../../build
APP_NAME = XXX

SRCS = prog.cpp
OBJS = $(addprefix $(BUILD_DIR)/obj/$(APP_NAME)/, $(SRCS:.cpp=.o))
TARGET = $(BUILD_DIR)/user/$(APP_NAME).bin

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p $(dir $(TARGET))
	@echo "[APP] Linking $@"
	@$(LD) -T linker.ld -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/obj/$(APP_NAME)/%.o: %.cpp
	mkdir -p $(dir $@)
	@echo "[APP] Compiling $<"
	@$(GPP) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
MKEOF
sed -i "s/XXX/$APP_NAME/g" "$NEW_DIR/Makefile"

echo "  Created $NEW_DIR/prog.cpp"
echo "  Created $NEW_DIR/Makefile"
echo "  Created $NEW_DIR/linker.ld"
echo "  Created $NEW_DIR/include/prog.h"

# --- update user_prog/Makefile (SUBDIRS) ---
if grep -qw "$APP_NAME" "$PROG_MAKEFILE" 2>/dev/null; then
    echo "  Skipping user_prog/Makefile: '$APP_NAME' already in SUBDIRS"
else
    # Find indentation of existing entries
    INDENT=$(grep -E '^\s+\S+\\$' "$PROG_MAKEFILE" | head -1 | sed 's/[^ \t].*$//')
    [ -z "$INDENT" ] && INDENT=$'\t\t  '

    # Add backslash to the last SUBDIRS entry (the one without \)
    sed -i '/^SUBDIRS/,/^[[:space:]]*$/{
        /^[[:space:]]*$/{
            # Insert new entry before blank line
            i\'"$INDENT$APP_NAME"'
        }
        /\\$/!{
            /^SUBDIRS/!{
                /^[[:space:]]*$/!{
                    # Line without backslash that is not SUBDIRS and not blank -> add backslash
                    s/$/\\/
                }
            }
        }
    }' "$PROG_MAKEFILE"

    echo "  Updated $PROG_MAKEFILE (SUBDIRS)"
fi

# --- update top-level Makefile (hdd target) ---
if grep -Fqe "user/${APP_NAME}.bin" "$TOP_MAKEFILE" 2>/dev/null; then
    echo "  Skipping Makefile: '$APP_NAME' already in hdd target"
else
    HDD_LINE="	-sudo cp \$(BUILD_DIR)/user/${APP_NAME}.bin /mnt/vdi_p1/Hashx86/apps/${APP_NAME}.bin"
    awk -v line="$HDD_LINE" '
    {
        print
        if ($0 ~ /^[ \t]*#[ \t]*5\./) {
            printf "%s\n\n", line
        }
    }
    ' "$TOP_MAKEFILE" > "$TOP_MAKEFILE.tmp" && mv "$TOP_MAKEFILE.tmp" "$TOP_MAKEFILE"
    echo "  Updated $TOP_MAKEFILE (hdd target)"
fi

echo ""
echo "Done! App '$APP_NAME' created ($APP_TYPE_NAME)."
echo ""
echo "Build:  make -C user_prog $APP_NAME"
echo "Clean:  make -C user_prog/$APP_NAME clean"
echo "HDD:    make hdd  (copies binary to disk image)"
