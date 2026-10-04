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

HX86_DECLARE_APP(HX86_APP_GUI);

// Upper bound mirrored from the kernel's list view capacity.
#define MAX_MODES 16

// Window layout constants.
#define WIN_W 460
#define WIN_H 380
#define PAD 12
#define TITLE_BAR_H 28

// Persisted settings file. This duplicates the kernel's PATH_SETTINGS_FILE
// ("Hashx86/settings.json"); apps are separate binaries built without the
// kernel's include tree, so the string is repeated here rather than shared.
// The kernel tolerates a missing file, so an older build that has not been
// repackaged still boots.
#define SETTINGS_PATH "Hashx86/settings.json"

// Must match SETTINGS_SCHEMA_VERSION in include/core/settings.h. The kernel
// refuses to apply a file stamped newer than the version it understands, so
// bumping one without the other would silently disable persistence.
#define SETTINGS_SCHEMA_VERSION 1

// Forward declaration used by the global g_app pointer.
class SettingsApp;
static SettingsApp* g_app = nullptr;

/**
 * class SettingsApp - Display settings window.
 *
 * Presents the resolutions the graphics driver offers, highlights the one
 * currently active, and applies the user's choice through the display syscall.
 * Applying re-programs the video hardware and re-lays out the desktop, so the
 * window is deliberately sized to fit the smallest supported resolution and
 * survives the switch.
 */
class SettingsApp {
private:
    Window* mainWindow;
    Label* currentLabel;
    Label* statusLabel;
    HListView* modeList;
    Button* btnApply;
    Button* btnSave;

    HDisplayMode modes[MAX_MODES];
    int modeCount;
    HDisplayMode current;

public:
    SettingsApp();
    void refreshCurrentMode();
    void selectCurrentMode();
    void onApply();
    void onSave();
    void onListClick();
};

// Forward declaration of the integer-to-string helper used to render a mode.
static void formatResolution(char* buf, const HDisplayMode* mode);

/**
 * formatResolution() - Render a resolution as "W x H".
 * @buf: Destination buffer, at least 24 bytes.
 * @mode: Geometry to format.
 */
void formatResolution(char* buf, const HDisplayMode* mode) {
    char wbuf[12];
    char hbuf[12];

    itoa(wbuf, 10, mode->width);
    itoa(hbuf, 10, mode->height);

    strcpy(buf, wbuf);
    strcat(buf, " x ");
    strcat(buf, hbuf);
}

SettingsApp::SettingsApp() {
    const int32_t contentW = WIN_W - (PAD * 2);
    const int32_t currentY = TITLE_BAR_H + PAD;
    const int32_t listY = currentY + 26 + PAD;
    const int32_t statusH = 18;
    const int32_t statusY = WIN_H - statusH - PAD;
    const int32_t listH = statusY - listY - PAD - 30;
    const int32_t btnY = statusY - statusH - PAD;
    const int32_t btnW = 100;
    const int32_t saveW = 130;

    mainWindow = new Window(desktop, 240, 180, WIN_W, WIN_H);
    mainWindow->setWindowTitle("Settings");

    // Active resolution readout.
    currentLabel = new Label(mainWindow, PAD, currentY, contentW, 22, "Current: ");
    currentLabel->setSize(SMALL);

    // Available resolutions.
    modeList = new HListView(mainWindow, PAD, listY, contentW, listH);
    modeList->SetHeader("Resolution");

    // Apply changes the live mode only; Save persists the current mode so it is
    // restored at the next boot. Keeping them separate means experimenting with
    // a resolution does not silently rewrite the saved default.
    btnSave = new Button(mainWindow, contentW - saveW, btnY, saveW, 26, "Save default");
    btnApply = new Button(mainWindow, contentW - btnW - saveW - PAD, btnY, btnW, 26, "Apply");

    statusLabel = new Label(mainWindow, PAD, statusY, contentW, statusH, "Select a resolution");
    statusLabel->setSize(TINY);

    mainWindow->AddChild(currentLabel);
    mainWindow->AddChild(modeList);
    mainWindow->AddChild(btnSave);
    mainWindow->AddChild(btnApply);
    mainWindow->AddChild(statusLabel);

    btnApply->OnClick(this, [](void* inst) { static_cast<SettingsApp*>(inst)->onApply(); });
    btnSave->OnClick(this, [](void* inst) { static_cast<SettingsApp*>(inst)->onSave(); });
    modeList->OnClick(this, [](void* inst) { static_cast<SettingsApp*>(inst)->onListClick(); });

    mainWindow->show();

    // ---- Populate the resolution list ------------------------------------
    modeCount = display_get_mode_count();
    if (modeCount > MAX_MODES) modeCount = MAX_MODES;

    static ListViewItemData items[MAX_MODES];
    for (int i = 0; i < modeCount; i++) {
        if (!display_get_mode(i, &modes[i])) {
            modeCount = i;
            break;
        }
        formatResolution(items[i].name, &modes[i]);
        items[i].size = modes[i].width;
        items[i].type = 0;
    }

    modeList->SetItems(items, modeCount);

    refreshCurrentMode();
    selectCurrentMode();
}

/**
 * refreshCurrentMode() - Refresh the "current resolution" readout.
 */
void SettingsApp::refreshCurrentMode() {
    char buf[48];
    if (!display_get_current_mode(&current)) {
        currentLabel->setText("Current: unknown");
        return;
    }

    char res[24];
    formatResolution(res, &current);
    strcpy(buf, "Current: ");
    strcat(buf, res);
    currentLabel->setText(buf);
}

/**
 * selectCurrentMode() - Highlight the row matching the active resolution.
 */
void SettingsApp::selectCurrentMode() {
    for (int i = 0; i < modeCount; i++) {
        if (modes[i].width == current.width && modes[i].height == current.height) {
            modeList->SetSelectedIndex(i);
            return;
        }
    }
    modeList->SetSelectedIndex(-1);
}

/**
 * onListClick() - Report the highlighted resolution in the status bar.
 */
void SettingsApp::onListClick() {
    int sel = modeList->GetSelectedIndex();
    if (sel < 0 || sel >= modeCount) {
        statusLabel->setText("Select a resolution");
        return;
    }

    char buf[48];
    char res[24];
    formatResolution(res, &modes[sel]);
    strcpy(buf, "Selected: ");
    strcat(buf, res);
    statusLabel->setText(buf);
}

/**
 * onApply() - Switch the display to the selected resolution.
 *
 * The syscall re-programs the video hardware and re-lays out the desktop
 * underneath this window, which is why the window was sized to fit every
 * supported mode in the first place.
 */
void SettingsApp::onApply() {
    int sel = modeList->GetSelectedIndex();
    if (sel < 0 || sel >= modeCount) {
        statusLabel->setText("Select a resolution first");
        return;
    }

    const HDisplayMode* target = &modes[sel];

    if (target->width == current.width && target->height == current.height) {
        statusLabel->setText("Already using this resolution");
        return;
    }

    statusLabel->setText("Applying...");

    if (!display_set_mode(target->width, target->height)) {
        statusLabel->setText("Mode not supported by this display");
        printf("[Settings] Mode %ux%u rejected\n", target->width, target->height);
        return;
    }

    refreshCurrentMode();

    char buf[48];
    char res[24];
    formatResolution(res, &current);
    strcpy(buf, "Applied: ");
    strcat(buf, res);
    statusLabel->setText(buf);
    printf("[Settings] Resolution changed to %ux%u\n", current.width, current.height);

    // Re-highlight the row that now matches the active mode.
    selectCurrentMode();
}

/**
 * serializeSettings() - Render the current settings as JSON.
 * @buf: Destination buffer.
 * @bufSize: Size of @buf in bytes.
 * @mode: Geometry to record.
 *
 * Hand-rolled rather than pulled from the kernel's parser: the app only ever
 * writes this one fixed shape, and keeping the writer here means the kernel
 * side stays read-only. Output is pretty-printed because the file is small
 * enough to be hand-edited, and a future field is a matter of appending a
 * line here.
 *
 * Return: Number of bytes written (excluding the NUL), or 0 if it would not
 *         fit in @buf.
 */
static int serializeSettings(char* buf, int bufSize, const HDisplayMode* mode) {
    char wbuf[12];
    char hbuf[12];
    char vbuf[12];

    itoa(wbuf, 10, mode->width);
    itoa(hbuf, 10, mode->height);
    itoa(vbuf, 10, SETTINGS_SCHEMA_VERSION);

    int n = 0;

// Append a literal, refusing rather than truncating if the buffer is short:
// a half-written JSON document would be rejected wholesale at next boot.
#define APPEND(s)                           \
    do {                                    \
        const char* _s = (s);               \
        while (*_s) {                       \
            if (n + 1 >= bufSize) return 0; \
            buf[n++] = *_s++;               \
        }                                   \
    } while (0)

    APPEND("{\n");
    APPEND("  \"schema_version\": ");
    APPEND(vbuf);
    APPEND(",\n");

    APPEND("  \"display\": {\n");
    APPEND("    \"width\": ");
    APPEND(wbuf);
    APPEND(",\n");
    APPEND("    \"height\": ");
    APPEND(hbuf);
    APPEND("\n");

    APPEND("  }\n");
    APPEND("}\n");

#undef APPEND

    buf[n] = '\0';
    return n;
}

/**
 * onSave() - Persist the current resolution as the next boot's default.
 *
 * Writes settings.json through the regular open/write/close syscalls, so this
 * is a real file write from ring 3. The write is not atomic: there is no
 * rename() to swap a temporary file in, so a power cut mid-write can leave a
 * truncated document. That is survivable by design, because the kernel treats
 * an unparseable settings file as "use the defaults" rather than failing boot.
 */
void SettingsApp::onSave() {
    if (!display_get_current_mode(&current)) {
        statusLabel->setText("Current resolution unknown");
        return;
    }

    char json[256];
    int len = serializeSettings(json, (int)sizeof(json), &current);
    if (len == 0) {
        statusLabel->setText("Failed to build settings");
        return;
    }

    int fd = syscall_open(SETTINGS_PATH, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        statusLabel->setText("Cannot open settings file");
        printf("[Settings] open %s failed (%d)\n", SETTINGS_PATH, fd);
        return;
    }

    int written = syscall_write((uint32_t)fd, json, (uint32_t)len);
    syscall_close((uint32_t)fd);

    if (written != len) {
        statusLabel->setText("Write failed, settings not saved");
        printf("[Settings] wrote %d of %d bytes\n", written, len);
        return;
    }

    char buf[48];
    char res[24];
    formatResolution(res, &current);
    strcpy(buf, "Saved default: ");
    strcat(buf, res);
    statusLabel->setText(buf);
    printf("[Settings] Wrote %d bytes to %s (display %ux%u)\n", len, SETTINGS_PATH,
           (unsigned)current.width, (unsigned)current.height);
}

/**
 * _start() - Application entry point for the Settings program.
 * @arg: Program arguments passed by the loader.
 *
 * Initializes the system and graphics, then constructs the SettingsApp, which
 * builds the window. Returns leaving the GUI event loop to run.
 */
extern "C" void _start(void* arg) {
    init_sys(arg);
    init_graphics();
    printf("[Settings] Starting...\n");

    g_app = new SettingsApp();
}
