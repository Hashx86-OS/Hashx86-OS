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

#include <Hx86/Hgui/Hgui.h>
#include <Hx86/Hx86.h>
#include <Hx86/debug.h>
#include <Hx86/utils/string.h>

namespace {

constexpr int CLI_WIN_X = 120;
constexpr int CLI_WIN_Y = 110;
constexpr int CLI_WIN_W = 920;
constexpr int CLI_WIN_H = 640;

constexpr int CLI_VIEW_X = 10;
constexpr int CLI_VIEW_Y = 34;
constexpr int CLI_VIEW_W = CLI_WIN_W - 20;
constexpr int CLI_VIEW_H = CLI_WIN_H - 44;

constexpr int CLI_TEXT_CAPACITY = 64 * 1024;

constexpr uint8_t SC_BACKSPACE = 0x0E;
constexpr uint8_t SC_TAB = 0x0F;
constexpr uint8_t SC_ENTER = 0x1C;
constexpr uint8_t SC_CAPSLOCK = 0x3A;

static const char kCliNormalMap[128] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7',  '8', '9', '0',  '-',  '=', 0,   0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o',  'p', '[', ']',  '\n', 0,   'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z',  'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,    ' ', 0,   0,    0,
};

static const char kCliShiftMap[128] = {
    0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_',  '+', 0,   0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,   'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z',  'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,
};

Window* g_cliWindow = nullptr;
TerminalView* g_cliView = nullptr;
char* g_cliTextBuffer = nullptr;
int g_cliTextLength = 0;
bool g_cliCapsLock = false;
uint32_t g_cliAttachedViewId = 0;

bool EnsureCliTextBuffer() {
    if (g_cliTextBuffer) return true;

    g_cliTextBuffer = (char*)kmalloc(CLI_TEXT_CAPACITY);
    if (!g_cliTextBuffer) return false;

    g_cliTextBuffer[0] = '\0';
    g_cliTextLength = 0;
    return true;
}

bool PushCliBufferToAttachedView() {
    if (g_cliAttachedViewId == 0 || !g_cliTextBuffer) return false;

    WidgetData data = {g_cliAttachedViewId, 0, 0, 0, 0, g_cliTextBuffer, nullptr, nullptr};
    return HguiAPI(TERMINAL_VIEW, SET_TEXT, (void*)&data);
}

char TranslateCliScancode(uint8_t scancode, bool shiftPressed) {
    if (scancode >= 128) return 0;

    if (scancode == SC_ENTER) return '\n';
    if (scancode == SC_TAB) return '\t';
    if (scancode == SC_BACKSPACE) return '\b';
    if (scancode == SC_CAPSLOCK) {
        g_cliCapsLock = !g_cliCapsLock;
        return 0;
    }

    char normal = kCliNormalMap[scancode];
    if (normal == 0) return 0;

    bool isAlpha = (normal >= 'a' && normal <= 'z') || (normal >= 'A' && normal <= 'Z');
    if (isAlpha) {
        char out = normal;
        bool uppercase = (shiftPressed && !g_cliCapsLock) || (!shiftPressed && g_cliCapsLock);
        if (uppercase && out >= 'a' && out <= 'z') {
            out = (char)(out - 32);
        }
        return out;
    }

    return shiftPressed ? kCliShiftMap[scancode] : normal;
}

}  // namespace

/** init_sys() - Initialize the process heap and auto-start a terminal host.
 * @arg: Pointer to the ProgramArguments passed by the loader.
 *
 * Called once from _start(): initializes the user-space heap through brk and,
 * for binaries tagged as CLI in their ELF metadata, allocates the CLI text
 * buffer and (re)attaches or creates the terminal window.
 */
void init_sys(void* arg) {
    if (!args) {
        args = (ProgramArguments*)arg;

        // Initialize the heap via brk.
        int32_t heap_start = syscall_brk(0);  // Get the current program break.
        syscall_brk(256 * 1024);              // Grow the initial heap by 256KB.
        int32_t heap_end = syscall_brk(0);    // Get the new program break.
        heap_init((void*)heap_start, (void*)heap_end);

        // Auto-initialize terminal host for binaries tagged as CLI in ELF metadata.
        int32_t appMode = syscall_get_app_mode();
        if (appMode == HX86_APP_CLI) {
            int32_t attachedView = syscall_get_cli_attached_view();
            if (attachedView > 0) {
                g_cliAttachedViewId = (uint32_t)attachedView;
                if (syscall_init_cli() > 0) {
                    if (!EnsureCliTextBuffer()) {
                        syscall_debug("init_sys: failed to allocate CLI text buffer\n");
                    }
                } else {
                    syscall_debug("init_sys: syscall_init_cli() failed\n");
                    g_cliAttachedViewId = 0;
                }
            } else {
                g_cliAttachedViewId = 0;
                init_cli();
            }
        }

        printf("[PROG] : Heap :- 0x%x - 0x%x\n", heap_start, heap_end);
    }
}

/** init_cli() - Create or attach a terminal host for a CLI application.
 *
 * Return: true on success, false if the terminal could not be set up.
 */
bool init_cli() {
    int32_t ret = syscall_init_cli();
    if (ret <= 0) return false;

    if (g_cliAttachedViewId != 0) {
        return EnsureCliTextBuffer();
    }

    if (!desktop) {
        init_graphics();
    }
    if (!desktop) return false;

    if (!g_cliWindow) {
        g_cliWindow = new Window(desktop, CLI_WIN_X, CLI_WIN_Y, CLI_WIN_W, CLI_WIN_H);
        if (!g_cliWindow) return false;
        g_cliWindow->setWindowTitle("Terminal");
    }

    if (!g_cliView) {
        g_cliView =
            new TerminalView(g_cliWindow, CLI_VIEW_X, CLI_VIEW_Y, CLI_VIEW_W, CLI_VIEW_H, "");
        if (!g_cliView) return false;
        g_cliView->setSize(TINY);
        g_cliView->OnKeyPress([](uint8_t scancode, bool shiftPressed) {
            char c = TranslateCliScancode(scancode, shiftPressed);
            if (c != 0) {
                syscall_stdin_push(0, c);
            }
        });
        g_cliWindow->AddChild(g_cliView);
        g_cliWindow->show();
    }

    if (!EnsureCliTextBuffer()) return false;

    return true;
}

/** cli_append_output() - Append text to the CLI output buffer.
 * @text: NUL-terminated text to append.
 *
 * Writes into the shared CLI text buffer, mirroring the result into the local
 * terminal view or pushing it to the attached host view.
 *
 * Return: true on success, false if no buffer or view is available.
 */
bool cli_append_output(const char* text) {
    if (!text || !g_cliTextBuffer) return false;

    int len = strlen(text);
    if (len <= 0) return true;

    if (len >= CLI_TEXT_CAPACITY) {
        text += (len - (CLI_TEXT_CAPACITY - 1));
        len = CLI_TEXT_CAPACITY - 1;
        g_cliTextLength = 0;
        g_cliTextBuffer[0] = '\0';
    }

    if (g_cliTextLength + len >= CLI_TEXT_CAPACITY) {
        g_cliTextLength = 0;
        g_cliTextBuffer[0] = '\0';
    }

    memcpy(g_cliTextBuffer + g_cliTextLength, text, (size_t)len);
    g_cliTextLength += len;
    g_cliTextBuffer[g_cliTextLength] = '\0';

    if (g_cliView) {
        g_cliView->setText(g_cliTextBuffer);
        return true;
    }

    if (g_cliAttachedViewId != 0) {
        return PushCliBufferToAttachedView();
    }

    return false;
}

/** syscall_sleep() - Sleep for the given number of milliseconds.
 * @ms: Sleep duration in milliseconds.
 */
void syscall_sleep(uint32_t ms) {
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;
    syscall_nanosleep(&req, nullptr);
}
