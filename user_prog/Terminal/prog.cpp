/**
 * @file        prog.cpp
 * @brief       Terminal CLI [BIN]
 *
 * @date        06/03/2026
 * @version     1.1.0
 */

#include "include/prog.h"
#include <Hx86/Hx86.h>

HX86_DECLARE_APP(HX86_APP_GUI);

namespace {

void TerminalBuffer::Init() {
    lineCount = 1;
    for (int i = 0; i < MAX_LINES; i++) {
        lengths[i] = 0;
        lines[i][0] = '\0';
    }
}

void TerminalBuffer::Clear() {
    Init();
}

int TerminalBuffer::CurrentIndex() const {
    return lineCount - 1;
}

void TerminalBuffer::NewLine() {
    if (lineCount < MAX_LINES) {
        lineCount++;
        int idx = lineCount - 1;
        lengths[idx] = 0;
        lines[idx][0] = '\0';
        return;
    }

    for (int i = 1; i < MAX_LINES; i++) {
        lengths[i - 1] = lengths[i];
        for (int j = 0; j <= MAX_COLS; j++) {
            lines[i - 1][j] = lines[i][j];
            if (lines[i][j] == '\0') break;
        }
    }

    lengths[MAX_LINES - 1] = 0;
    lines[MAX_LINES - 1][0] = '\0';
}

void TerminalBuffer::AppendChar(char c, int wrapCols) {
    int idx = CurrentIndex();
    if (lengths[idx] >= min_i(MAX_COLS, wrapCols)) {
        NewLine();
        idx = CurrentIndex();
    }

    if (lengths[idx] < MAX_COLS) {
        lines[idx][lengths[idx]++] = c;
        lines[idx][lengths[idx]] = '\0';
    }
}

void TerminalBuffer::AppendText(const char* text, int wrapCols) {
    for (int i = 0; text[i] != '\0'; i++) {
        AppendChar(text[i], wrapCols);
    }
}

void TerminalBuffer::Backspace(int protectColumns) {
    int idx = CurrentIndex();
    if (lengths[idx] > protectColumns) {
        lengths[idx]--;
        lines[idx][lengths[idx]] = '\0';
    }
}

TerminalApp::TerminalApp()
    : mainWindow(nullptr),
      terminalView(nullptr),
      statusLabel(nullptr),
      terminal(nullptr),
      scrollOffset(0),
      capsLock(false),
      inputStartColumn(0),
      viewDirty(true),
      foregroundPid(-1),
      backgroundPid(-1) {
    renderText[0] = '\0';
    statusText[0] = '\0';
    currentPath[0] = '/';
    currentPath[1] = '\0';
}

TerminalApp::~TerminalApp() {
    if (mainWindow) delete mainWindow;
    if (terminal) delete terminal;
}

bool TerminalApp::Init() {
    terminal = new TerminalBuffer;
    if (!terminal) return false;
    terminal->Init();

    mainWindow = new Window(desktop, 120, 110, WIN_W, WIN_H);
    if (!mainWindow) return false;
    mainWindow->setWindowTitle("Terminal");

    terminalView = new TerminalView(mainWindow, TERM_X, TERM_Y, TERM_W, TERM_H, "");
    statusLabel = new Label(mainWindow, STATUS_X, STATUS_Y, STATUS_W, STATUS_H, "");

    if (!terminalView || !statusLabel) return false;

    terminalView->setSize(TINY);
    statusLabel->setSize(TINY);

    terminalView->OnKeyPress(this, [](void* instance, uint8_t scancode, bool shiftPressed) {
        ((TerminalApp*)instance)->HandleScancode(scancode, shiftPressed);
    });
    terminalView->OnClick(this, [](void* instance) {
        int32_t action = ((TerminalApp*)instance)->terminalView->consumeScrollAction();
        ((TerminalApp*)instance)->HandleScrollAction(action);
    });

    mainWindow->AddChild(terminalView);
    mainWindow->AddChild(statusLabel);

    mainWindow->show();
    syscall_set_cli_host_view(terminalView->ID);

    PushBanner();
    PushPrompt();
    UpdateView();
    return true;
}

void TerminalApp::Run() {
    while (1) {
        PollForegroundProcess();
        if (viewDirty) {
            UpdateView();
        }
        syscall_sleep(16);
    }
}

void TerminalApp::PollForegroundProcess() {
    if (foregroundPid > 0) {
        if (syscall_is_process_alive((uint32_t)foregroundPid) > 0) {
            return;
        }

        foregroundPid = -1;
        PrintLine("[process exited]");
        PushPrompt();
        scrollOffset = 0;
        MarkDirty();
        return;
    }

    if (backgroundPid > 0) {
        if (syscall_is_process_alive((uint32_t)backgroundPid) > 0) {
            return;
        }

        backgroundPid = -1;
        PrintLine("[process exited]");
        PushPrompt();
        scrollOffset = 0;
        MarkDirty();
    }
}

int TerminalApp::VisibleCols() const {
    return max_i(20, (TERM_W - 20) / CHAR_W);
}

int TerminalApp::VisibleRows() const {
    return max_i(8, (TERM_H - 6) / CHAR_H);
}

int TerminalApp::MaxScroll() const {
    return max_i(0, terminal->lineCount - VisibleRows());
}

void TerminalApp::MarkDirty() {
    viewDirty = true;
}

void TerminalApp::PushBanner() {
    terminal->AppendText("Hashx86 Terminal", VisibleCols());
    terminal->NewLine();
    terminal->AppendText("Commands: help, clear, pwd, cd, ls, run", VisibleCols());
    terminal->NewLine();
}

void TerminalApp::PushPrompt() {
    if (terminal->lengths[terminal->CurrentIndex()] != 0) {
        terminal->NewLine();
    }

    terminal->AppendText(currentPath, VisibleCols());
    terminal->AppendText(" $ ", VisibleCols());
    inputStartColumn = terminal->lengths[terminal->CurrentIndex()];
}

void TerminalApp::PrintLine(const char* text) {
    if (text && text[0] != '\0') {
        terminal->AppendText(text, VisibleCols());
    }
    terminal->NewLine();
}

void TerminalApp::ClearScreen() {
    terminal->Clear();
    scrollOffset = 0;
    PushPrompt();
    MarkDirty();
}

void TerminalApp::ScrollUp() {
    scrollOffset = min_i(MaxScroll(), scrollOffset + 2);
    MarkDirty();
}

void TerminalApp::ScrollDown() {
    scrollOffset = max_i(0, scrollOffset - 2);
    MarkDirty();
}

void TerminalApp::AppendStatus() {
    statusText[0] = '\0';

    strcat(statusText, "Lines: ");
    char num[16];
    itoa(num, 10, terminal->lineCount);
    strcat(statusText, num);

    strcat(statusText, "  Scroll: ");
    itoa(num, 10, scrollOffset);
    strcat(statusText, num);

    strcat(statusText, "  F11/F12 or scrollbar");
}

void TerminalApp::AppendCharSafe(char c, int& idx, int limit) {
    if (idx < limit - 1) {
        renderText[idx++] = c;
        renderText[idx] = '\0';
    }
}

void TerminalApp::BuildViewText() {
    renderText[0] = '\0';
    int idx = 0;

    int rows = VisibleRows();
    int cols = VisibleCols();
    int firstLine = max_i(0, terminal->lineCount - rows - scrollOffset);

    for (int r = 0; r < rows; r++) {
        int lineIdx = firstLine + r;
        if (lineIdx < terminal->lineCount) {
            int len = min_i(cols, terminal->lengths[lineIdx]);
            for (int c = 0; c < len; c++) {
                AppendCharSafe(terminal->lines[lineIdx][c], idx, MAX_RENDER_TEXT);
            }
        }

        if (r != rows - 1) {
            AppendCharSafe('\n', idx, MAX_RENDER_TEXT);
        }
    }
}

void TerminalApp::UpdateView() {
    if (scrollOffset > MaxScroll()) {
        scrollOffset = MaxScroll();
    }

    BuildViewText();
    AppendStatus();

    terminalView->setText(renderText);
    terminalView->setScrollMeta(terminal->lineCount, VisibleRows(), scrollOffset);
    statusLabel->setText(statusText);

    viewDirty = false;
}

void TerminalApp::TrimInPlace(char* text) const {
    if (!text) return;

    int start = 0;
    while (text[start] == ' ' || text[start] == '\t') {
        start++;
    }

    if (start > 0) {
        int i = 0;
        while (text[start + i] != '\0') {
            text[i] = text[start + i];
            i++;
        }
        text[i] = '\0';
    }

    int len = strlen(text);
    while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[len - 1] = '\0';
        len--;
    }
}

void TerminalApp::SplitCommand(const char* input, char* outCmd, char* outArgs) const {
    if (!outCmd || !outArgs) return;

    outCmd[0] = '\0';
    outArgs[0] = '\0';
    if (!input) return;

    int i = 0;
    while (input[i] == ' ' || input[i] == '\t') {
        i++;
    }

    int c = 0;
    while (input[i] != '\0' && input[i] != ' ' && input[i] != '\t' && c < MAX_SHELL_TOKEN - 1) {
        outCmd[c++] = input[i++];
    }
    outCmd[c] = '\0';

    while (input[i] == ' ' || input[i] == '\t') {
        i++;
    }

    int a = 0;
    while (input[i] != '\0' && a < MAX_COLS) {
        outArgs[a++] = input[i++];
    }
    outArgs[a] = '\0';
    TrimInPlace(outArgs);
}

bool TerminalApp::ResolvePath(const char* input, char* outPath) const {
    if (!input || !outPath) return false;

    char raw[MAX_PATH_LEN];
    int rawLen = 0;
    raw[0] = '\0';

    auto appendRawChar = [&](char ch) -> bool {
        if (rawLen >= MAX_PATH_LEN - 1) return false;
        raw[rawLen++] = ch;
        raw[rawLen] = '\0';
        return true;
    };

    auto appendRawText = [&](const char* text) -> bool {
        for (int i = 0; text[i] != '\0'; i++) {
            if (!appendRawChar(text[i])) return false;
        }
        return true;
    };

    if (input[0] == '\0') {
        if (!appendRawText(currentPath)) return false;
    } else if (input[0] == '/') {
        if (!appendRawText(input)) return false;
    } else {
        if (!appendRawText(currentPath)) return false;
        if (rawLen > 0 && raw[rawLen - 1] != '/') {
            if (!appendRawChar('/')) return false;
        }
        if (!appendRawText(input)) return false;
    }

    char normalized[MAX_PATH_LEN];
    int nlen = 0;
    normalized[nlen++] = '/';
    normalized[nlen] = '\0';

    int i = (raw[0] == '/') ? 1 : 0;
    while (true) {
        while (raw[i] == '/') {
            i++;
        }

        if (raw[i] == '\0') {
            break;
        }

        char segment[MAX_SHELL_TOKEN];
        int slen = 0;
        while (raw[i] != '\0' && raw[i] != '/') {
            if (slen < MAX_SHELL_TOKEN - 1) {
                segment[slen++] = raw[i];
            }
            i++;
        }
        segment[slen] = '\0';

        if (string_eq(segment, ".")) {
            continue;
        }

        if (string_eq(segment, "..")) {
            if (nlen > 1) {
                if (normalized[nlen - 1] == '/') nlen--;
                while (nlen > 0 && normalized[nlen - 1] != '/') {
                    nlen--;
                }
                if (nlen == 0) {
                    normalized[nlen++] = '/';
                }
                normalized[nlen] = '\0';
            }
            continue;
        }

        if (nlen > 1 && normalized[nlen - 1] != '/') {
            if (nlen >= MAX_PATH_LEN - 1) return false;
            normalized[nlen++] = '/';
        }

        for (int k = 0; segment[k] != '\0'; k++) {
            if (nlen >= MAX_PATH_LEN - 1) return false;
            normalized[nlen++] = segment[k];
        }
        normalized[nlen] = '\0';
    }

    if (nlen > 1 && normalized[nlen - 1] == '/') {
        normalized[--nlen] = '\0';
    }

    strcpy(outPath, normalized);
    return true;
}

bool TerminalApp::IsDirectoryPath(const char* path) const {
    if (!path || path[0] == '\0') return false;

    struct stat st;
    if (syscall_stat(path, &st) != 0) return false;
    return (st.st_mode & 0x4000) != 0;
}

bool TerminalApp::IsRegularPath(const char* path) const {
    if (!path || path[0] == '\0') return false;

    struct stat st;
    if (syscall_stat(path, &st) != 0) return false;
    return (st.st_mode & 0x8000) != 0;
}

bool TerminalApp::ResolveExecutablePath(const char* input, char* outPath) const {
    if (!input || !outPath) return false;

    char cmd[MAX_PATH_LEN];
    strcpy(cmd, input);

    int start = 0;
    while (cmd[start] == ' ' || cmd[start] == '\t') start++;
    if (start > 0) {
        int i = 0;
        while (cmd[start + i] != '\0') {
            cmd[i] = cmd[start + i];
            i++;
        }
        cmd[i] = '\0';
    }

    int len = strlen(cmd);
    while (len > 0 && (cmd[len - 1] == ' ' || cmd[len - 1] == '\t')) {
        cmd[len - 1] = '\0';
        len--;
    }

    if (cmd[0] == '\0') return false;

    bool hasSlash = false;
    bool hasDot = false;
    for (int i = 0; cmd[i] != '\0'; i++) {
        if (cmd[i] == '/') hasSlash = true;
        if (cmd[i] == '.') hasDot = true;
    }

    char candidate[MAX_PATH_LEN];

    auto checkCandidate = [&](const char* path) -> bool {
        if (!ResolvePath(path, candidate)) return false;
        if (!IsRegularPath(candidate)) return false;
        strcpy(outPath, candidate);
        return true;
    };

    if (hasSlash) {
        if (checkCandidate(cmd)) return true;

        if (!hasDot) {
            char withExt[MAX_PATH_LEN];
            withExt[0] = '\0';
            strcpy(withExt, cmd);
            strcat(withExt, ".BIN");
            if (checkCandidate(withExt)) return true;
        }
        return false;
    }

    if (checkCandidate(cmd)) return true;

    if (!hasDot) {
        char withExt[MAX_PATH_LEN];
        withExt[0] = '\0';
        strcpy(withExt, cmd);
        strcat(withExt, ".BIN");
        if (checkCandidate(withExt)) return true;
    }

    char sysPath[MAX_PATH_LEN];
    sysPath[0] = '\0';
    strcpy(sysPath, "/SYS32/");
    strcat(sysPath, cmd);

    if (ResolvePath(sysPath, candidate) && IsRegularPath(candidate)) {
        strcpy(outPath, candidate);
        return true;
    }

    if (!hasDot) {
        char sysPathExt[MAX_PATH_LEN];
        sysPathExt[0] = '\0';
        strcpy(sysPathExt, "/SYS32/");
        strcat(sysPathExt, cmd);
        strcat(sysPathExt, ".BIN");

        if (ResolvePath(sysPathExt, candidate) && IsRegularPath(candidate)) {
            strcpy(outPath, candidate);
            return true;
        }
    }

    return false;
}

void TerminalApp::CommandPwd() {
    PrintLine(currentPath);
}

void TerminalApp::CommandCd(const char* args) {
    if (!args || args[0] == '\0') {
        CommandPwd();
        return;
    }

    char resolved[MAX_PATH_LEN];
    if (!ResolvePath(args, resolved)) {
        PrintLine("cd: invalid path");
        return;
    }

    if (!IsDirectoryPath(resolved)) {
        PrintLine("cd: directory not found");
        return;
    }

    strcpy(currentPath, resolved);
}

void TerminalApp::CommandLs(const char* args) {
    char target[MAX_PATH_LEN];
    if (args && args[0] != '\0') {
        if (!ResolvePath(args, target)) {
            PrintLine("ls: invalid path");
            return;
        }
    } else {
        strcpy(target, currentPath);
    }

    if (!IsDirectoryPath(target)) {
        PrintLine("ls: not a directory");
        return;
    }

    int32_t fd = syscall_open(target, 0);
    if (fd < 0) {
        PrintLine("ls: failed to open directory");
        return;
    }

    char buffer[1024];
    int listed = 0;

    while (1) {
        int32_t bytes = syscall_getdents(fd, (struct linux_dirent*)buffer, sizeof(buffer));
        if (bytes <= 0) break;

        uint32_t offset = 0;
        while (offset < (uint32_t)bytes) {
            struct linux_dirent* ent = (struct linux_dirent*)(buffer + offset);
            if (ent->d_reclen == 0) break;

            if (ent->d_name[0] != '\0') {
                char line[MAX_PATH_LEN + 8];
                line[0] = '\0';
                strcat(line, ent->d_name);

                char fullPath[MAX_PATH_LEN];
                fullPath[0] = '\0';
                strcpy(fullPath, target);
                if (fullPath[strlen(fullPath) - 1] != '/') strcat(fullPath, "/");
                strcat(fullPath, ent->d_name);

                if (IsDirectoryPath(fullPath)) {
                    strcat(line, "/");
                }

                PrintLine(line);
                listed++;
            }

            offset += ent->d_reclen;
        }
    }

    syscall_close(fd);

    char summary[48];
    char num[16];
    summary[0] = '\0';
    itoa(num, 10, listed);
    strcat(summary, num);
    strcat(summary, " item(s)");
    PrintLine(summary);
}

void TerminalApp::CommandRun(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLine("run: missing program name");
        return;
    }

    char resolved[MAX_PATH_LEN];
    if (!ResolveExecutablePath(args, resolved)) {
        PrintLine("run: executable not found");
        return;
    }

    int32_t pid = syscall_execve(resolved, nullptr, nullptr);
    if (pid <= 0) {
        PrintLine("run: failed to launch executable");
        return;
    }

    int32_t appMode = syscall_get_process_app_mode((uint32_t)pid);
    if (appMode == HX86_APP_CLI) {
        foregroundPid = pid;

        char line[MAX_PATH_LEN + 48];
        char num[16];
        line[0] = '\0';
        strcat(line, "Attached PID ");
        itoa(num, 10, pid);
        strcat(line, num);
        strcat(line, ": ");
        strcat(line, resolved);
        PrintLine(line);
        return;
    }

    backgroundPid = pid;

    char line[MAX_PATH_LEN + 48];
    char num[16];
    line[0] = '\0';
    strcat(line, "Launched PID ");
    itoa(num, 10, pid);
    strcat(line, num);
    strcat(line, ": ");
    strcat(line, resolved);
    PrintLine(line);
}

char TerminalApp::TranslateScancode(uint8_t scancode, bool shiftPressed) const {
    if (scancode >= 128) return 0;

    char base = 0;
    if (is_alpha(kNormalMap[scancode])) {
        base = kNormalMap[scancode];
        bool uppercase = (shiftPressed && !capsLock) || (!shiftPressed && capsLock);
        if (uppercase) {
            base = to_upper(base);
        }
    } else {
        base = shiftPressed ? kShiftMap[scancode] : kNormalMap[scancode];
    }

    return base;
}

void TerminalApp::HandleScancode(uint8_t sc, bool shiftPressed) {
    PollForegroundProcess();

    if (foregroundPid > 0) {
        if (sc == SC_F11) {
            ScrollUp();
        } else if (sc == SC_F12) {
            ScrollDown();
        } else {
            char forwarded = 0;
            if (sc == SC_ENTER) {
                forwarded = '\n';
            } else if (sc == SC_BACKSPACE) {
                forwarded = '\b';
            } else if (sc == SC_TAB) {
                forwarded = '\t';
            } else if (sc == SC_CAPSLOCK) {
                capsLock = !capsLock;
            } else {
                char c = TranslateScancode(sc, shiftPressed);
                if (c >= 32 && c <= 126) {
                    forwarded = c;
                }
            }

            if (forwarded != 0) {
                if (syscall_stdin_push((uint32_t)foregroundPid, forwarded) <= 0) {
                    PollForegroundProcess();
                }
            }
        }

        if (viewDirty) {
            UpdateView();
        }
        return;
    }

    if (sc == SC_ESC) {
        syscall_exit_group(0);
    } else if (sc == SC_CAPSLOCK) {
        capsLock = !capsLock;
    } else if (sc == SC_BACKSPACE) {
        terminal->Backspace(inputStartColumn);
        scrollOffset = 0;
    } else if (sc == SC_ENTER) {
        HandleEnter();
    } else if (sc == SC_F11) {
        ScrollUp();
    } else if (sc == SC_F12) {
        ScrollDown();
    } else {
        char c = TranslateScancode(sc, shiftPressed);
        if (c >= 32 && c <= 126) {
            terminal->AppendChar(c, VisibleCols());
            scrollOffset = 0;
        }
    }

    if (scrollOffset > MaxScroll()) {
        scrollOffset = MaxScroll();
    }

    MarkDirty();
}

void TerminalApp::HandleScrollAction(int32_t scrollAction) {
    if (scrollAction == 0) return;

    if (scrollAction <= -1000000) {
        int absOffset = -(scrollAction + 1000000);
        if (absOffset < 0) absOffset = 0;
        if (absOffset > MaxScroll()) absOffset = MaxScroll();
        scrollOffset = absOffset;
        MarkDirty();
    } else if (scrollAction > 0) {
        if (scrollAction >= 5) {
            scrollOffset = min_i(MaxScroll(), scrollOffset + VisibleRows());
        } else {
            ScrollUp();
        }
    } else if (scrollAction < 0) {
        if (scrollAction <= -5) {
            scrollOffset = max_i(0, scrollOffset - VisibleRows());
        } else {
            ScrollDown();
        }
        MarkDirty();
    }

    if (viewDirty) {
        UpdateView();
    }
}

void TerminalApp::ExecuteCommand(const char* cmd) {
    char input[MAX_COLS + 1];
    input[0] = '\0';
    if (cmd) {
        strcpy(input, cmd);
    }
    TrimInPlace(input);

    if (input[0] == '\0') {
        PushPrompt();
        scrollOffset = 0;
        return;
    }

    char command[MAX_SHELL_TOKEN];
    char args[MAX_COLS + 1];
    SplitCommand(input, command, args);

    if (string_eq(command, "clear")) {
        terminal->Clear();
        PushPrompt();
        scrollOffset = 0;
        return;
    }

    if (string_eq(command, "help")) {
        PrintLine("Built-ins:");
        PrintLine("  help         Show this help");
        PrintLine("  clear        Clear terminal output");
        PrintLine("  pwd          Print current directory");
        PrintLine("  cd <path>    Change directory");
        PrintLine("  ls [path]    List directory entries");
        PrintLine("  run <prog>   Run an executable");
        PrintLine("Tip: you can run binaries directly, e.g. CLIHELLO.BIN");
    } else if (string_eq(command, "pwd")) {
        CommandPwd();
    } else if (string_eq(command, "cd")) {
        CommandCd(args);
    } else if (string_eq(command, "ls")) {
        CommandLs(args);
    } else if (string_eq(command, "run")) {
        CommandRun(args);
    } else {
        CommandRun(command);
    }

    if (foregroundPid <= 0) {
        PushPrompt();
        scrollOffset = 0;
    }
}

void TerminalApp::HandleEnter() {
    int idx = terminal->CurrentIndex();
    char cmd[MAX_COLS + 1];
    int out = 0;

    for (int i = inputStartColumn; i < terminal->lengths[idx] && out < MAX_COLS; i++) {
        cmd[out++] = terminal->lines[idx][i];
    }
    cmd[out] = '\0';

    terminal->NewLine();
    ExecuteCommand(cmd);
}

}  // namespace

extern "C" void _start(void* arg) {
    init_sys(arg);
    init_graphics();

    TerminalApp* app = new TerminalApp();
    if (!app) {
        syscall_exit(1);
    }

    if (!app->Init()) {
        syscall_exit(1);
    }

    app->Run();
}
