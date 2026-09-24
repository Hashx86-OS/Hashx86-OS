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
#include <Hx86/Hsyscalls/syscalls.h>
#include <Hx86/Hx86.h>
#include <Hx86/debug.h>
#include <Hx86/utils/string.h>

HX86_DECLARE_APP(HX86_APP_GUI);

#define MAX_ENTRIES 64

static char currentPath[256] = "/";
static ListViewItemData dirEntries[MAX_ENTRIES];
static int dirEntryCount = 0;

// Forward declaration used by the global g_app pointer.
class ExplorerApp;
static ExplorerApp* g_app = nullptr;

/**
 * class ExplorerApp - File manager GUI for the Explorer program.
 *
 * Owns the window widgets (path label, status label, file list, and up,
 * refresh and open toolbar buttons) and connects them to directory
 * navigation and process-launch logic.
 */
class ExplorerApp {
private:
    Window* mainWindow;
    Label* pathLabel;
    Label* statusLabel;
    HListView* fileList;
    IconButton* btnUp;
    IconButton* btnRefresh;
    IconButton* btnOpen;

public:
    ExplorerApp();
    void refreshDirectory();
    void navigateUp();
    void openSelected();
    void onListClick();
    void handleEvent(uint32_t widgetID, uint32_t eventType);

private:
    int loadDirectory(const char* path);
    bool isExecutable(const char* name);
};

ExplorerApp::ExplorerApp() {
    // Window layout constants.
    const int32_t winW = 1000;
    const int32_t winH = 700;
    const int32_t pad = 10;                     // Edge padding.
    const int32_t titleBarH = 28;               // Window title bar height.
    const int32_t contentW = winW - (pad * 2);  // Usable content width.
    const int32_t toolbarY = titleBarH + pad;   // Toolbar row Y.
    const int32_t toolbarH = 26;
    const int32_t listY = toolbarY + toolbarH + pad;  // ListView Y.
    const int32_t statusH = 18;
    const int32_t statusY = winH - statusH - pad;  // Status bar at bottom.
    const int32_t listH = statusY - listY - pad;   // ListView fills remaining space.

    mainWindow = new Window(desktop, 120, 300, winW, winH);
    mainWindow->setWindowTitle("Explorer");

    // Toolbar buttons on the left side.
    btnUp = new IconButton(mainWindow, pad, toolbarY, 64, toolbarH, "fa-arrow-left", "");
    btnUp->setIconFontSize(16);
    btnRefresh = new IconButton(mainWindow, pad + 72, toolbarY, 64, toolbarH, "fa-refresh", "");
    btnRefresh->setIconFontSize(16);
    btnOpen = new IconButton(mainWindow, pad + 144, toolbarY, 64, toolbarH, "fa-folder-open", "");
    btnOpen->setIconFontSize(16);

    // Path label spans the remaining toolbar space beside the buttons.
    const int32_t btnEnd = pad + 144 + 64 + 6;
    pathLabel = new Label(mainWindow, btnEnd, toolbarY + 2, contentW - (btnEnd - pad), 20, "/");
    pathLabel->setSize(SMALL);

    // File list view fills the main content area.
    fileList = new HListView(mainWindow, pad, listY, contentW, listH);
    fileList->SetHeader("Name");

    // Status bar at the bottom.
    statusLabel = new Label(mainWindow, pad, statusY, contentW, statusH, "Ready");
    statusLabel->setSize(TINY);

    // Add all children to the window.
    mainWindow->AddChild(pathLabel);
    mainWindow->AddChild(btnUp);
    mainWindow->AddChild(btnRefresh);
    mainWindow->AddChild(btnOpen);
    mainWindow->AddChild(fileList);
    mainWindow->AddChild(statusLabel);

    // Wire up widget callbacks to the application methods.
    btnUp->OnClick(this, [](void* inst) { static_cast<ExplorerApp*>(inst)->navigateUp(); });
    btnRefresh->OnClick(this,
                        [](void* inst) { static_cast<ExplorerApp*>(inst)->refreshDirectory(); });
    btnOpen->OnClick(this, [](void* inst) { static_cast<ExplorerApp*>(inst)->openSelected(); });
    fileList->OnClick(this, [](void* inst) { static_cast<ExplorerApp*>(inst)->onListClick(); });

    mainWindow->show();

    // Load the initial directory.
    refreshDirectory();
}

/**
 * loadDirectory() - Populate dirEntries from a filesystem directory.
 * @path: Absolute directory path to scan.
 *
 * Reads directory entries through syscall_getdents(), statting each entry for
 * its size and type. Executables are recognized via isExecutable().
 *
 * Return: The number of entries loaded, or -1 if the path could not be opened.
 */
int ExplorerApp::loadDirectory(const char* path) {
    dirEntryCount = 0;

    int32_t fd = syscall_open(path, 0);
    if (fd < 0) {
        printf("[Explorer] Failed to open: %s\n", path);
        return -1;
    }

    char buffer[1024];
    while (dirEntryCount < MAX_ENTRIES) {
        int32_t bytes = syscall_getdents(fd, (struct linux_dirent*)buffer, sizeof(buffer));
        if (bytes <= 0) break;

        uint32_t offset = 0;
        while (offset < (uint32_t)bytes && dirEntryCount < MAX_ENTRIES) {
            struct linux_dirent* ent = (struct linux_dirent*)(buffer + offset);
            if (ent->d_name[0] != '\0') {
                ListViewItemData& item = dirEntries[dirEntryCount];

                // Copy the entry name, truncating at the 63-byte limit.
                int j = 0;
                while (ent->d_name[j] && j < 63) {
                    item.name[j] = ent->d_name[j];
                    j++;
                }
                item.name[j] = 0;

                // Stat the entry for its size and type.
                char fullpath[256];
                memset(fullpath, 0, sizeof(fullpath));
                strcpy(fullpath, path);
                if (fullpath[strlen(fullpath) - 1] != '/') strcat(fullpath, "/");
                strcat(fullpath, item.name);

                struct stat st;
                item.size = 0;
                item.type = 0;  // Default: regular file.

                if (syscall_stat(fullpath, &st) == 0) {
                    item.size = st.st_size;
                    if (st.st_mode & 0x4000) {
                        item.type = 1;  // Directory.
                    } else if (isExecutable(item.name)) {
                        item.type = 2;  // Executable.
                    }
                }

                dirEntryCount++;
            }
            offset += ent->d_reclen;
        }
    }

    syscall_close(fd);
    return dirEntryCount;
}

/**
 * isExecutable() - Test whether a name has a .BIN extension.
 * @name: File name to test.
 *
 * Return: True when the final four characters are ".BIN" (case-insensitive).
 */
bool ExplorerApp::isExecutable(const char* name) {
    int len = strlen(name);
    if (len < 5) return false;
    // Check the .BIN extension (case-insensitive).
    char c1 = name[len - 3] | 0x20, c2 = name[len - 2] | 0x20, c3 = name[len - 1] | 0x20;
    if ((name[len - 4] == '.') && (c1 == 'b') && (c2 == 'i') && (c3 == 'n')) {
        return true;
    }
    return false;
}

void ExplorerApp::refreshDirectory() {
    statusLabel->setText("Loading...");
    pathLabel->setText(currentPath);

    int count = loadDirectory(currentPath);

    if (count >= 0) {
        fileList->SetItems(dirEntries, dirEntryCount);

        // Build the status message with the entry count.
        char statusMsg[64];
        memset(statusMsg, 0, sizeof(statusMsg));
        strcpy(statusMsg, "");
        char numStr[16];
        itoa(numStr, 10, dirEntryCount);
        strcat(statusMsg, numStr);
        strcat(statusMsg, " items");
        statusLabel->setText(statusMsg);
    } else {
        fileList->Clear();
        statusLabel->setText("Failed to open directory");
    }
}

void ExplorerApp::navigateUp() {
    int len = strlen(currentPath);
    if (len <= 1) return;  // Already at the root.

    // Remove a trailing slash, if any.
    if (currentPath[len - 1] == '/' && len > 1) {
        currentPath[len - 1] = 0;
        len--;
    }

    // Find the last directory separator.
    int lastSlash = 0;
    for (int i = len - 1; i >= 0; i--) {
        if (currentPath[i] == '/') {
            lastSlash = i;
            break;
        }
    }

    if (lastSlash == 0) {
        currentPath[0] = '/';
        currentPath[1] = 0;
    } else {
        currentPath[lastSlash] = 0;
    }

    refreshDirectory();
}

void ExplorerApp::openSelected() {
    int sel = fileList->GetSelectedIndex();
    if (sel < 0 || sel >= dirEntryCount) {
        statusLabel->setText("No item selected");
        return;
    }

    ListViewItemData& item = dirEntries[sel];

    if (item.type == 1) {
        // Descend into the directory.
        if (strlen(currentPath) > 1) strcat(currentPath, "/");
        strcat(currentPath, item.name);
        refreshDirectory();
    } else if (item.type == 2) {
        // Launch the executable.
        char fullpath[256];
        memset(fullpath, 0, sizeof(fullpath));
        strcpy(fullpath, currentPath);
        if (fullpath[strlen(fullpath) - 1] != '/') strcat(fullpath, "/");
        strcat(fullpath, item.name);

        statusLabel->setText("Launching...");
        int32_t pid = syscall_execve(fullpath, nullptr, nullptr);
        if (pid > 0) {
            statusLabel->setText("Launched!");
        } else {
            statusLabel->setText("Launch failed");
        }
    } else {
        // Regular file - show its name as info.
        char info[64];
        memset(info, 0, sizeof(info));
        strcpy(info, item.name);
        strcat(info, " selected");
        statusLabel->setText(info);
    }
}

void ExplorerApp::onListClick() {
    int sel = fileList->GetSelectedIndex();
    if (sel >= 0 && sel < dirEntryCount) {
        char info[64];
        memset(info, 0, sizeof(info));
        strcpy(info, dirEntries[sel].name);
        statusLabel->setText(info);

        printf("[Explorer] Selected: %s\n", dirEntries[sel].name);
    }
}

void ExplorerApp::handleEvent(uint32_t widgetID, uint32_t eventType) {
    if (widgetID == btnUp->ID)
        navigateUp();
    else if (widgetID == btnRefresh->ID)
        refreshDirectory();
    else if (widgetID == btnOpen->ID)
        openSelected();
    else if (widgetID == fileList->ID)
        onListClick();
}

/**
 * _start() - Application entry point for the Explorer file manager.
 * @arg: Program arguments passed by the loader.
 *
 * Initializes the system and graphics, then constructs the ExplorerApp, which
 * builds the window. Returns leaving the GUI event loop to run.
 */
extern "C" void _start(void* arg) {
    init_sys(arg);
    init_graphics();
    printf("[Explorer] Starting...\n");

    g_app = new ExplorerApp();
}
