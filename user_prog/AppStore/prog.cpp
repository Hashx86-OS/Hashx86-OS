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

#include "GitHub.h"
#include "HttpClient.h"
#include "StoreUtil.h"

#include <Hx86/Hgui/Hgui.h>
#include <Hx86/Hsyscalls/syscalls.h>
#include <Hx86/Hx86.h>
#include <Hx86/debug.h>
#include <Hx86/memory.h>

#include <mbedtls/sha256.h>

HX86_DECLARE_APP(HX86_APP_GUI);

namespace {

constexpr const char* kAppsDir = "/Apps";
constexpr const char* kQuakeDir = "/Apps/Quake";
constexpr const char* kQuakeBinary = "/Apps/Quake/Quake.bin";
constexpr const char* kQuakeTemp = "/Apps/Quake/Quake.bin.part";

Window* g_window = nullptr;
Label* g_engineLabel = nullptr;
Label* g_progressLabel = nullptr;
Label* g_statusLabel = nullptr;
Button* g_installButton = nullptr;
Button* g_launchButton = nullptr;
Button* g_refreshButton = nullptr;

volatile bool g_busy = false;
int32_t g_quakePid = -1;

char g_engineText[160];
char g_progressText[160];
char g_statusText[256];

struct DownloadContext {
    int32_t fd;
    mbedtls_sha256_context sha;
    uint32_t received;
    uint32_t expected;
    uint32_t nextProgress;
};

void SetStatus(const char* text) {
    StoreStrCopy(g_statusText, sizeof(g_statusText), text);
    if (g_statusLabel != nullptr) g_statusLabel->setText(g_statusText);
}

void SetProgress(const char* text) {
    StoreStrCopy(g_progressText, sizeof(g_progressText), text);
    if (g_progressLabel != nullptr) g_progressLabel->setText(g_progressText);
}

void DigestToHex(const uint8_t digest[32], char output[65]) {
    static const char kHex[] = "0123456789abcdef";
    for (uint32_t i = 0; i < 32; i++) {
        output[i * 2] = kHex[digest[i] >> 4];
        output[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    output[64] = '\0';
}

bool DownloadSink(void* context, const uint8_t* data, uint32_t length) {
    DownloadContext* download = static_cast<DownloadContext*>(context);
    if (!StoreWriteAll(download->fd, data, length)) return false;
    if (mbedtls_sha256_update_ret(&download->sha, data, length) != 0) return false;

    download->received += length;
    if (download->received >= download->nextProgress) {
        uint32_t percent = 0;
        if (download->expected != 0) {
            percent = (download->received * 100u) / download->expected;
        }
        char text[160];
        StoreFormat(text, sizeof(text), "Downloading: %u / %u KB (%u%%)",
                    download->received / 1024u, download->expected / 1024u, percent);
        SetProgress(text);
        download->nextProgress += 64u * 1024u;
    }
    return true;
}

bool RefreshState() {
    uint32_t engineSize = 0;
    bool installed = StoreFileExists(kQuakeBinary, &engineSize);

    if (installed) {
        StoreFormat(g_engineText, sizeof(g_engineText), "Engine: installed (%u bytes)", engineSize);
    } else {
        StoreStrCopy(g_engineText, sizeof(g_engineText), "Engine: not installed");
    }

    if (g_engineLabel != nullptr) g_engineLabel->setText(g_engineText);

    bool canLaunch = installed && !g_busy;
    if (g_installButton != nullptr) {
        g_installButton->setText(installed ? "Update" : "Install");
        g_installButton->setEnabled(!g_busy);
    }
    if (g_launchButton != nullptr) g_launchButton->setEnabled(canLaunch);
    if (g_refreshButton != nullptr) g_refreshButton->setEnabled(!g_busy);
    return installed;
}

void FinishWorker(const char* message) {
    g_busy = false;
    RefreshState();
    SetStatus(message);
}

void WorkerFail(const char* message) {
    char text[256];
    StoreFormat(text, sizeof(text), "Install failed: %s", message);
    SetProgress("");
    FinishWorker(text);
}

void InstallWorker(void* argument) {
    (void)argument;

    char error[192];
    GitHubRelease release = {};
    SetStatus("Contacting GitHub...");
    if (!GitHubResolveLatest(&release, error, sizeof(error))) {
        printf("[AppStore] release lookup failed: %s\n", error);
        WorkerFail(error);
        return;
    }
    printf("[AppStore] release %s size=%u\n", release.tag, release.size);

    char message[192];
    StoreFormat(message, sizeof(message), "Preparing to install %s...", release.tag);
    SetStatus(message);

    if (!StoreEnsureDir(kAppsDir) || !StoreEnsureDir(kQuakeDir)) {
        WorkerFail("could not create /Apps/Quake");
        return;
    }

    int32_t fd = syscall_open(kQuakeTemp, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        WorkerFail("could not create the temporary file");
        return;
    }

    DownloadContext download = {};
    download.fd = fd;
    download.expected = release.size;
    download.nextProgress = 64u * 1024u;
    mbedtls_sha256_init(&download.sha);
    if (mbedtls_sha256_starts_ret(&download.sha, 0) != 0) {
        syscall_close((uint32_t)fd);
        syscall_unlink(kQuakeTemp);
        mbedtls_sha256_free(&download.sha);
        WorkerFail("SHA-256 initialization failed");
        return;
    }

    SetProgress("Downloading: 0 KB");
    printf("[AppStore] downloading %s\n", release.apiUrl);
    StoreHttpResponse response = {};
    bool downloaded = StoreHttpGet(release.apiUrl, "application/octet-stream", DownloadSink,
                                   &download, &response, error, sizeof(error));

    uint8_t digest[32] = {};
    int finishResult = mbedtls_sha256_finish_ret(&download.sha, digest);
    syscall_close((uint32_t)fd);
    mbedtls_sha256_free(&download.sha);

    if (!downloaded) {
        printf("[AppStore] asset download failed: %s\n", error);
        syscall_unlink(kQuakeTemp);
        WorkerFail(error);
        return;
    }
    if (finishResult != 0) {
        syscall_unlink(kQuakeTemp);
        WorkerFail("SHA-256 finalization failed");
        return;
    }
    if (download.received != release.size) {
        syscall_unlink(kQuakeTemp);
        WorkerFail("downloaded size does not match the release metadata");
        return;
    }

    char digestHex[65];
    DigestToHex(digest, digestHex);
    if (!StoreHexEqual(digestHex, release.sha256)) {
        syscall_unlink(kQuakeTemp);
        WorkerFail("SHA-256 check failed");
        return;
    }

    syscall_unlink(kQuakeBinary);
    if (syscall_rename(kQuakeTemp, kQuakeBinary) != 0) {
        syscall_unlink(kQuakeTemp);
        WorkerFail("could not move the verified binary into place");
        return;
    }

    RefreshState();
    StoreFormat(message, sizeof(message), "Installed %s.", release.tag);
    SetProgress("");
    FinishWorker(message);
}

void StartInstall() {
    if (g_busy) return;
    g_busy = true;
    SetStatus("Starting installer...");
    SetProgress("");
    RefreshState();

    // Run synchronously in the GUI event thread. The libhx86 heap and TLS
    // context are not thread-safe, and running the transfer on a second
    // thread races the GUI thread's allocations.
    InstallWorker(nullptr);
}

void LaunchQuake() {
    if (g_busy) return;
    if (!StoreFileExists(kQuakeBinary, nullptr)) {
        SetStatus("Install Quake before launching it.");
        return;
    }
    if (g_quakePid > 0 && syscall_is_process_alive(g_quakePid)) {
        SetStatus("Quake is already running.");
        return;
    }

    int32_t pid = syscall_execve(kQuakeBinary, nullptr, nullptr);
    if (pid >= 0) {
        g_quakePid = pid;
        SetStatus("Launching Quake...");
    } else {
        SetStatus("Quake failed to launch.");
    }
}

void RefreshClicked() {
    RefreshState();
    SetStatus("Ready");
}

}  // namespace

class AppStoreApp {
public:
    AppStoreApp();
};

AppStoreApp::AppStoreApp() {
    g_window = new Window(desktop, 100, 100, 720, 500);
    g_window->setWindowTitle("App Store");

    Label* title = new Label(g_window, 20, 20, 680, 32, "App Store");
    title->setSize(LARGE);

    Label* subtitle =
        new Label(g_window, 20, 58, 680, 22, "Install applications from the Hashx86 catalog.");
    subtitle->setSize(SMALL);

    Label* quakeTitle = new Label(g_window, 20, 94, 680, 28, "Quake");
    quakeTitle->setSize(MEDIUM);

    Label* description =
        new Label(g_window, 20, 124, 680, 22,
                  "WinQuake port for Hashx86. Software rendering, keyboard and mouse input.");
    description->setSize(TINY);

    Label* license = new Label(g_window, 20, 148, 680, 22, "License: GPL-2.0-or-later");
    license->setSize(TINY);

    Label* source =
        new Label(g_window, 20, 170, 680, 22, "Source: github.com/Hashx86-OS/Quake-Hx86");
    source->setSize(TINY);

    g_engineLabel = new Label(g_window, 20, 210, 680, 22, "Engine: checking...");
    g_engineLabel->setSize(SMALL);

    g_progressLabel = new Label(g_window, 20, 260, 680, 22, "");
    g_progressLabel->setSize(SMALL);

    g_installButton = new Button(g_window, 20, 310, 120, 32, "Install");
    g_launchButton = new Button(g_window, 152, 310, 100, 32, "Launch");
    g_refreshButton = new Button(g_window, 264, 310, 100, 32, "Refresh");

    g_statusLabel = new Label(g_window, 20, 360, 680, 60, "Ready. Install Quake to begin.");
    g_statusLabel->setSize(TINY);

    g_window->AddChild(title);
    g_window->AddChild(subtitle);
    g_window->AddChild(quakeTitle);
    g_window->AddChild(description);
    g_window->AddChild(license);
    g_window->AddChild(source);
    g_window->AddChild(g_engineLabel);
    g_window->AddChild(g_progressLabel);
    g_window->AddChild(g_installButton);
    g_window->AddChild(g_launchButton);
    g_window->AddChild(g_refreshButton);
    g_window->AddChild(g_statusLabel);

    g_installButton->OnClick([]() { StartInstall(); });
    g_launchButton->OnClick([]() { LaunchQuake(); });
    g_refreshButton->OnClick([]() { RefreshClicked(); });

    g_window->show();
    RefreshState();
}

extern "C" void _start(void* arg) {
    init_sys(arg);
    init_graphics();
    printf("[AppStore] Starting...\n");
    new AppStoreApp();
}
