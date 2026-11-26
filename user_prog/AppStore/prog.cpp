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

#include "AppStoreConfig.h"
#include "Catalog.h"
#include "HttpClient.h"
#include "ImageDecoder.h"
#include "StoreUtil.h"

#include <Hx86/Hgui/Hgui.h>
#include <Hx86/Hgui/imageview.h>
#include <Hx86/Hgui/listview.h>
#include <Hx86/Hsyscalls/syscalls.h>
#include <Hx86/Hx86.h>
#include <Hx86/debug.h>
#include <Hx86/memory.h>

#include <mbedtls/sha256.h>

HX86_DECLARE_APP(HX86_APP_GUI);

namespace {

constexpr const char* kAppsDir = "/Apps";
constexpr const char* kArtworkCacheDir = "/Hashx86/cache/appstore";

Window* g_window = nullptr;
HListView* g_appList = nullptr;
HImageView* g_iconView = nullptr;
HImageView* g_bannerView = nullptr;
Label* g_catalogLabel = nullptr;
Label* g_nameLabel = nullptr;
Label* g_versionLabel = nullptr;
Label* g_descriptionLine1 = nullptr;
Label* g_descriptionLine2 = nullptr;
Label* g_metaLabel = nullptr;
Label* g_sourceLabel = nullptr;
Label* g_sizeLabel = nullptr;
Label* g_requirementLabel = nullptr;
Label* g_installedLabel = nullptr;
Label* g_progressLabel = nullptr;
Label* g_statusLabel = nullptr;
Button* g_installButton = nullptr;
Button* g_launchButton = nullptr;
Button* g_refreshButton = nullptr;

Catalog g_catalog;
ListViewItemData g_listItems[APPSTORE_MAX_APPS];

struct RunningProcess {
    char id[APPSTORE_CATALOG_FIELD_LEN];
    int32_t pid;
};

RunningProcess g_runningProcesses[APPSTORE_MAX_APPS];
volatile bool g_busy = false;

char g_artworkId[APPSTORE_CATALOG_FIELD_LEN];
bool g_artworkLoaded = false;
ImagePixels g_artIcon = {};
ImagePixels g_artBanner = {};

char g_progressText[192];
char g_statusText[256];

struct DownloadContext {
    int32_t fd;
    mbedtls_sha256_context sha;
    uint32_t received;
    uint32_t expected;
    uint32_t nextProgress;
    const char* appName;
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

void ApplyArtwork() {
    if (g_iconView != nullptr) {
        g_iconView->SetImage(g_artIcon.pixels, g_artIcon.width, g_artIcon.height);
    }
    if (g_bannerView != nullptr) {
        g_bannerView->SetImage(g_artBanner.pixels, g_artBanner.width, g_artBanner.height);
    }
}

void ClearArtwork() {
    if (g_artIcon.pixels != nullptr) kfree(g_artIcon.pixels);
    if (g_artBanner.pixels != nullptr) kfree(g_artBanner.pixels);
    g_artIcon = {};
    g_artBanner = {};
    g_artworkId[0] = '\0';
    g_artworkLoaded = false;
    ApplyArtwork();
}

struct ImageDownloadSink {
    uint8_t* data;
    uint32_t capacity;
    uint32_t length;
    bool overflow;
};

bool ImageDownloadWrite(void* context, const uint8_t* data, uint32_t length) {
    ImageDownloadSink* sink = (ImageDownloadSink*)context;
    if (sink == nullptr || data == nullptr) return false;
    if (length >= sink->capacity || sink->length > sink->capacity - 1 - length) {
        sink->overflow = true;
        return false;
    }
    for (uint32_t i = 0; i < length; i++) sink->data[sink->length + i] = data[i];
    sink->length += length;
    return true;
}

bool IsSha256Name(const char* text) {
    if (text == nullptr) return false;
    for (uint32_t i = 0; i < 64; i++) {
        char c = text[i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return text[64] == '\0';
}

bool BuildArtworkCachePath(const CatalogAssetRef& ref, char* path, uint32_t capacity) {
    if (!IsSha256Name(ref.sha256)) return false;
    StoreFormat(path, capacity, "%s/%s.jpg", kArtworkCacheDir, ref.sha256);
    return path[0] != '\0';
}

bool LoadCachedArtwork(const CatalogAssetRef& ref, ImagePixels* image) {
    char path[192];
    if (!BuildArtworkCachePath(ref, path, sizeof(path))) return false;

    uint32_t size = 0;
    if (!StoreFileExists(path, &size) || size != ref.size || size > 4u * 1024u * 1024u) {
        return false;
    }

    uint8_t* data = nullptr;
    uint32_t readSize = 0;
    if (!StoreReadFile(path, &data, &readSize) || data == nullptr || readSize != ref.size) {
        if (data != nullptr) kfree(data);
        return false;
    }

    uint8_t digest[32] = {};
    char digestHex[65];
    bool hashOk = mbedtls_sha256_ret(data, readSize, digest, 0) == 0;
    DigestToHex(digest, digestHex);
    if (!hashOk || !StoreHexEqual(digestHex, ref.sha256)) {
        kfree(data);
        syscall_unlink(path);
        return false;
    }

    bool decoded = DecodeJpeg(data, readSize, image);
    kfree(data);
    if (!decoded) {
        syscall_unlink(path);
        return false;
    }

    printf("[AppStore] using cached artwork %s\n", ref.sha256);
    return true;
}

void SaveArtworkCache(const CatalogAssetRef& ref, const uint8_t* data, uint32_t length) {
    char path[192];
    if (!BuildArtworkCachePath(ref, path, sizeof(path))) return;
    if (!StoreEnsureDir("/Hashx86/cache") || !StoreEnsureDir(kArtworkCacheDir)) return;

    char tempPath[200];
    StoreFormat(tempPath, sizeof(tempPath), "%s.tmp", path);
    int32_t fd = syscall_open(tempPath, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) return;

    bool written = StoreWriteAll(fd, data, length);
    syscall_close((uint32_t)fd);
    if (!written) {
        syscall_unlink(tempPath);
        return;
    }

    syscall_unlink(path);
    if (syscall_rename(tempPath, path) != 0) {
        syscall_unlink(tempPath);
        printf("[AppStore] could not cache artwork\n");
    }
}

bool DownloadArtwork(const CatalogAssetRef& ref, ImagePixels* image, char* error,
                     uint32_t errorCapacity) {
    image->pixels = nullptr;
    image->width = 0;
    image->height = 0;
    if (ref.url[0] == '\0' || ref.size == 0 || ref.size > 8u * 1024u * 1024u) return false;

    // Reuse a verified download so revisiting an app does not pay for another
    // pair of TLS handshakes.
    if (LoadCachedArtwork(ref, image)) return true;

    uint8_t* data = (uint8_t*)kmalloc((int)(ref.size + 1));
    if (data == nullptr) {
        StoreStrCopy(error, errorCapacity, "out of memory for artwork");
        return false;
    }

    ImageDownloadSink sink = {data, ref.size + 1, 0, false};
    StoreHttpResponse response = {};
    error[0] = '\0';
    bool downloaded = StoreHttpGet(ref.url, "application/octet-stream", ImageDownloadWrite, &sink,
                                   &response, error, errorCapacity);
    if (!downloaded) {
        if (sink.overflow) StoreStrCopy(error, errorCapacity, "artwork is too large");
        kfree(data);
        return false;
    }
    if (sink.length != ref.size) {
        StoreStrCopy(error, errorCapacity, "artwork size mismatch");
        kfree(data);
        return false;
    }

    uint8_t digest[32];
    if (mbedtls_sha256_ret(data, sink.length, digest, 0) != 0) {
        StoreStrCopy(error, errorCapacity, "artwork SHA-256 failed");
        kfree(data);
        return false;
    }
    char digestHex[65];
    DigestToHex(digest, digestHex);
    if (!StoreHexEqual(digestHex, ref.sha256)) {
        StoreStrCopy(error, errorCapacity, "artwork SHA-256 mismatch");
        kfree(data);
        return false;
    }

    bool decoded = DecodeJpeg(data, sink.length, image);
    if (decoded) {
        SaveArtworkCache(ref, data, sink.length);
    } else {
        StoreStrCopy(error, errorCapacity, "JPEG decode failed");
    }
    kfree(data);
    return decoded;
}

void LoadArtwork(const CatalogApp* app) {
    if (app == nullptr) {
        ClearArtwork();
        return;
    }
    if (g_artworkLoaded && StoreStrEqual(g_artworkId, app->id)) {
        ApplyArtwork();
        return;
    }

    ClearArtwork();
    StoreStrCopy(g_artworkId, sizeof(g_artworkId), app->id);

    char error[160];
    bool iconOk = false;
    bool bannerOk = false;
    if (app->iconRef.url[0] != '\0') {
        iconOk = DownloadArtwork(app->iconRef, &g_artIcon, error, sizeof(error));
        if (!iconOk) printf("[AppStore] icon failed: %s\n", error);
        // Show the icon while the larger banner is still loading.
        ApplyArtwork();
    }
    if (app->bannerRef.url[0] != '\0') {
        bannerOk = DownloadArtwork(app->bannerRef, &g_artBanner, error, sizeof(error));
        if (!bannerOk) printf("[AppStore] banner failed: %s\n", error);
        ApplyArtwork();
    }

    // Keep failures retryable: revisiting the app will try the download again.
    g_artworkLoaded = iconOk || bannerOk;
    ApplyArtwork();
}

bool DownloadSink(void* context, const uint8_t* data, uint32_t length) {
    DownloadContext* download = static_cast<DownloadContext*>(context);
    if (!StoreWriteAll(download->fd, data, length)) return false;
    if (mbedtls_sha256_update_ret(&download->sha, data, length) != 0) return false;

    download->received += length;
    if (download->received >= download->nextProgress) {
        uint32_t percent = 0;
        if (download->expected != 0) {
            percent = download->received / (download->expected / 100u + 1u);
            if (percent > 100u) percent = 100u;
        }
        char text[192];
        StoreFormat(text, sizeof(text), "Downloading %s: %u / %u KB (%u%%)",
                    download->appName ? download->appName : "app", download->received / 1024u,
                    download->expected / 1024u, percent);
        SetProgress(text);
        download->nextProgress += 64u * 1024u;
    }
    return true;
}

bool AppDir(const CatalogApp* app, char* path, uint32_t capacity) {
    StoreFormat(path, capacity, "%s/%s", kAppsDir, app->id);
    return path[0] != '\0';
}

bool AppBinary(const CatalogApp* app, char* path, uint32_t capacity) {
    char dir[96];
    if (!AppDir(app, dir, sizeof(dir))) return false;
    StoreFormat(path, capacity, "%s/%s", dir, app->binary);
    return path[0] != '\0';
}

bool AppTemp(const CatalogApp* app, char* path, uint32_t capacity) {
    char binaryPath[160];
    if (!AppBinary(app, binaryPath, sizeof(binaryPath))) return false;
    StoreFormat(path, capacity, "%s.part", binaryPath);
    return path[0] != '\0';
}

bool IsInstalled(const CatalogApp* app, uint32_t* size) {
    char path[160];
    if (!AppBinary(app, path, sizeof(path))) return false;
    return StoreFileExists(path, size);
}

int32_t RunningPidFor(const CatalogApp* app) {
    for (uint32_t i = 0; i < APPSTORE_MAX_APPS; i++) {
        if (g_runningProcesses[i].pid > 0 && StoreStrEqual(g_runningProcesses[i].id, app->id)) {
            return g_runningProcesses[i].pid;
        }
    }
    return -1;
}

void SetRunningPid(const CatalogApp* app, int32_t pid) {
    for (uint32_t i = 0; i < APPSTORE_MAX_APPS; i++) {
        if (g_runningProcesses[i].pid <= 0 || StoreStrEqual(g_runningProcesses[i].id, app->id)) {
            StoreStrCopy(g_runningProcesses[i].id, sizeof(g_runningProcesses[i].id), app->id);
            g_runningProcesses[i].pid = pid;
            return;
        }
    }
}

int SelectedIndex() {
    return g_appList ? g_appList->GetSelectedIndex() : -1;
}

CatalogApp* SelectedApp() {
    int index = SelectedIndex();
    if (index < 0 || index >= (int)g_catalog.appCount) return nullptr;
    return &g_catalog.apps[index];
}

void FormatSize(uint32_t bytes, char* output, uint32_t capacity) {
    if (bytes >= 1024u * 1024u) {
        uint32_t whole = bytes / (1024u * 1024u);
        uint32_t tenth = (bytes / (1024u * 102u)) % 10u;
        StoreFormat(output, capacity, "%u.%u MB", whole, tenth);
    } else {
        StoreFormat(output, capacity, "%u KB", (bytes + 1023u) / 1024u);
    }
}

void SetTwoLineDescription(const char* text) {
    char line1[96] = "";
    char line2[96] = "";
    if (text == nullptr || text[0] == '\0') {
        StoreStrCopy(line1, sizeof(line1), "No description.");
    } else {
        uint32_t length = StoreStrLen(text);
        uint32_t split = 0;
        for (uint32_t i = 0; i < length && i < 58; i++) {
            if (text[i] == '\n') {
                split = i;
                break;
            }
        }
        if (split == 0) {
            split = length > 58 ? 58 : length;
            if (length > 58) {
                while (split > 0 && text[split] != ' ') split--;
                if (split == 0) split = 58;
            }
        }

        uint32_t out = 0;
        for (uint32_t i = 0; i < split && out + 1 < sizeof(line1); i++) line1[out++] = text[i];
        line1[out] = '\0';
        while (text[split] == ' ' || text[split] == '\n') split++;
        out = 0;
        for (uint32_t i = split; text[i] != '\0' && out + 1 < sizeof(line2); i++) {
            line2[out++] = text[i];
        }
        line2[out] = '\0';
    }

    if (g_descriptionLine1 != nullptr) g_descriptionLine1->setText(line1);
    if (g_descriptionLine2 != nullptr) g_descriptionLine2->setText(line2);
}

void ShowSelectedApp() {
    CatalogApp* app = SelectedApp();
    if (app == nullptr) {
        ClearArtwork();
        if (g_nameLabel != nullptr) g_nameLabel->setText("No application selected");
        if (g_versionLabel != nullptr) g_versionLabel->setText("");
        SetTwoLineDescription("");
        if (g_metaLabel != nullptr) g_metaLabel->setText("");
        if (g_sourceLabel != nullptr) g_sourceLabel->setText("");
        if (g_sizeLabel != nullptr) g_sizeLabel->setText("");
        if (g_requirementLabel != nullptr) g_requirementLabel->setText("");
        if (g_installedLabel != nullptr) g_installedLabel->setText("");
        if (g_installButton != nullptr) g_installButton->setEnabled(false);
        if (g_launchButton != nullptr) g_launchButton->setEnabled(false);
        if (g_refreshButton != nullptr) g_refreshButton->setEnabled(!g_busy);
        return;
    }

    char text[512];
    uint32_t installedSize = 0;
    bool installed = IsInstalled(app, &installedSize);

    if (g_nameLabel != nullptr) g_nameLabel->setText(app->name);
    if (g_versionLabel != nullptr) {
        StoreFormat(text, sizeof(text), "Version %s", app->version);
        g_versionLabel->setText(text);
    }
    SetTwoLineDescription(app->description);

    if (g_metaLabel != nullptr) {
        StoreFormat(text, sizeof(text), "%s  |  License: %s", app->category, app->license);
        g_metaLabel->setText(text);
    }
    if (g_sourceLabel != nullptr) {
        if (app->source[0] != '\0') {
            StoreFormat(text, sizeof(text), "Source: %s", app->source);
            g_sourceLabel->setText(text);
        } else if (app->repo[0] != '\0') {
            StoreFormat(text, sizeof(text), "Source: https://github.com/%s", app->repo);
            g_sourceLabel->setText(text);
        } else {
            g_sourceLabel->setText("Source: not listed");
        }
    }
    if (g_sizeLabel != nullptr) {
        if (app->size != 0) {
            char sizeText[32];
            FormatSize(app->size, sizeText, sizeof(sizeText));
            StoreFormat(text, sizeof(text), "Download: %s", sizeText);
            g_sizeLabel->setText(text);
        } else if (app->repo[0] != '\0') {
            StoreFormat(text, sizeof(text), "Download: latest %s release", app->repo);
            g_sizeLabel->setText(text);
        } else {
            g_sizeLabel->setText("Download: unknown");
        }
    }
    if (g_requirementLabel != nullptr) {
        if (app->minOsVersion[0] == '\0') {
            g_requirementLabel->setText("Requires: any Hashx86 version");
        } else if (app->compatible) {
            StoreFormat(text, sizeof(text), "Requires: Hashx86 %s or newer", app->minOsVersion);
            g_requirementLabel->setText(text);
        } else {
            StoreFormat(text, sizeof(text), "Requires: Hashx86 %s (this OS is %s)",
                        app->minOsVersion, APPSTORE_OS_VERSION);
            g_requirementLabel->setText(text);
        }
    }
    if (g_installedLabel != nullptr) {
        if (installed) {
            char sizeText[32];
            FormatSize(installedSize, sizeText, sizeof(sizeText));
            StoreFormat(text, sizeof(text), "Installed: %s", sizeText);
        } else {
            StoreStrCopy(text, sizeof(text), "Installed: no");
        }
        g_installedLabel->setText(text);
    }

    LoadArtwork(app);

    if (g_installButton != nullptr) {
        g_installButton->setText(installed ? "Update" : "Install");
        g_installButton->setEnabled(!g_busy && app->compatible);
    }
    if (g_launchButton != nullptr) {
        g_launchButton->setEnabled(!g_busy && installed && app->compatible);
    }
    if (g_refreshButton != nullptr) g_refreshButton->setEnabled(!g_busy);
}

void PopulateList() {
    if (g_appList == nullptr) return;

    int previousIndex = SelectedIndex();
    char previousId[APPSTORE_CATALOG_FIELD_LEN] = "";
    if (previousIndex >= 0 && previousIndex < (int)g_catalog.appCount) {
        StoreStrCopy(previousId, sizeof(previousId), g_catalog.apps[previousIndex].id);
    }

    g_appList->Clear();
    for (uint32_t i = 0; i < g_catalog.appCount; i++) {
        CatalogApp& app = g_catalog.apps[i];
        ListViewItemData& item = g_listItems[i];
        memset(&item, 0, sizeof(item));
        StoreStrCopy(item.cells[0], sizeof(item.cells[0]), app.name);
        StoreStrCopy(item.cells[1], sizeof(item.cells[1]), app.version);

        uint32_t installedSize = 0;
        bool installed = IsInstalled(&app, &installedSize);
        const char* status = "Available";
        if (!app.compatible) {
            status = "Requires newer OS";
        } else if (installed) {
            status = "Installed";
        }
        StoreStrCopy(item.cells[2], sizeof(item.cells[2]), status);
        item.type = installed ? 2 : 0;
    }
    g_appList->SetItems(g_listItems, (int)g_catalog.appCount);

    int selected = 0;
    if (previousId[0] != '\0') {
        for (uint32_t i = 0; i < g_catalog.appCount; i++) {
            if (StoreStrEqual(g_catalog.apps[i].id, previousId)) {
                selected = (int)i;
                break;
            }
        }
    }
    if (g_catalog.appCount > 0) g_appList->SetSelectedIndex(selected);
    ShowSelectedApp();
}

void LoadCatalog() {
    if (g_busy) return;
    g_busy = true;
    if (g_installButton != nullptr) g_installButton->setEnabled(false);
    if (g_launchButton != nullptr) g_launchButton->setEnabled(false);
    if (g_refreshButton != nullptr) g_refreshButton->setEnabled(false);
    SetStatus("Fetching latest catalog...");
    SetProgress("");
    if (g_catalogLabel != nullptr) g_catalogLabel->setText("Catalog: loading...");

    char error[224];
    if (!CatalogFetch(&g_catalog, error, sizeof(error))) {
        printf("[AppStore] catalog failed: %s\n", error);
        g_busy = false;
        if (g_appList != nullptr) g_appList->Clear();
        if (g_catalogLabel != nullptr) g_catalogLabel->setText("Catalog: unavailable");
        SetProgress("");
        SetStatus(error);
        ShowSelectedApp();
        return;
    }

    g_busy = false;
    if (g_catalogLabel != nullptr) {
        char text[160];
        StoreFormat(text, sizeof(text), "Catalog: %s  |  %u apps", g_catalog.releaseTag,
                    g_catalog.appCount);
        g_catalogLabel->setText(text);
    }
    ClearArtwork();
    PopulateList();
    SetProgress("");
    SetStatus("Catalog loaded. Select an application.");
}

void WorkerFail(const char* message) {
    char text[256];
    StoreFormat(text, sizeof(text), "Install failed: %s", message);
    g_busy = false;
    SetProgress("");
    PopulateList();
    SetStatus(text);
}

void InstallSelected() {
    if (g_busy) return;
    CatalogApp* app = SelectedApp();
    if (app == nullptr) {
        SetStatus("Select an application first.");
        return;
    }
    if (!app->compatible) {
        SetStatus("This application requires a newer Hashx86 version.");
        return;
    }

    char dir[96];
    char binaryPath[160];
    char tempPath[176];
    if (!AppDir(app, dir, sizeof(dir)) || !AppBinary(app, binaryPath, sizeof(binaryPath)) ||
        !AppTemp(app, tempPath, sizeof(tempPath))) {
        SetStatus("The catalog entry has an invalid install path.");
        return;
    }

    g_busy = true;
    if (g_installButton != nullptr) g_installButton->setEnabled(false);
    if (g_launchButton != nullptr) g_launchButton->setEnabled(false);
    if (g_refreshButton != nullptr) g_refreshButton->setEnabled(false);

    if (!app->resolved) {
        SetStatus("Resolving the application's latest release...");
        char resolveError[224];
        if (!CatalogResolveApp(app, resolveError, sizeof(resolveError))) {
            WorkerFail(resolveError);
            return;
        }
    }

    char message[192];
    StoreFormat(message, sizeof(message), "Preparing %s %s...", app->name, app->version);
    SetStatus(message);
    SetProgress("");

    if (!StoreEnsureDir(kAppsDir) || !StoreEnsureDir(dir)) {
        WorkerFail("could not create the application directory");
        return;
    }

    int32_t fd = syscall_open(tempPath, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        WorkerFail("could not create the temporary file");
        return;
    }

    DownloadContext download = {};
    download.fd = fd;
    download.expected = app->size;
    download.nextProgress = 64u * 1024u;
    download.appName = app->name;
    mbedtls_sha256_init(&download.sha);
    if (mbedtls_sha256_starts_ret(&download.sha, 0) != 0) {
        syscall_close((uint32_t)fd);
        syscall_unlink(tempPath);
        mbedtls_sha256_free(&download.sha);
        WorkerFail("SHA-256 initialization failed");
        return;
    }

    char initialProgress[192];
    StoreFormat(initialProgress, sizeof(initialProgress), "Downloading %s: 0 KB", app->name);
    SetProgress(initialProgress);
    printf("[AppStore] downloading %s %s\n", app->name, app->downloadUrl);

    StoreHttpResponse response = {};
    char error[224];
    error[0] = '\0';
    bool downloaded = StoreHttpGet(app->downloadUrl, "application/octet-stream", DownloadSink,
                                   &download, &response, error, sizeof(error));

    uint8_t digest[32] = {};
    int finishResult = mbedtls_sha256_finish_ret(&download.sha, digest);
    syscall_close((uint32_t)fd);
    mbedtls_sha256_free(&download.sha);

    if (!downloaded) {
        syscall_unlink(tempPath);
        WorkerFail(error);
        return;
    }
    if (finishResult != 0) {
        syscall_unlink(tempPath);
        WorkerFail("SHA-256 finalization failed");
        return;
    }
    if (download.received != app->size) {
        syscall_unlink(tempPath);
        WorkerFail("downloaded size does not match the catalog");
        return;
    }

    char digestHex[65];
    DigestToHex(digest, digestHex);
    if (!StoreHexEqual(digestHex, app->sha256)) {
        syscall_unlink(tempPath);
        WorkerFail("SHA-256 check failed");
        return;
    }

    syscall_unlink(binaryPath);
    if (syscall_rename(tempPath, binaryPath) != 0) {
        syscall_unlink(tempPath);
        WorkerFail("could not move the verified binary into place");
        return;
    }

    g_busy = false;
    PopulateList();
    SetProgress("");
    StoreFormat(message, sizeof(message), "Installed %s %s.", app->name, app->version);
    SetStatus(message);
}

void LaunchSelected() {
    if (g_busy) return;
    CatalogApp* app = SelectedApp();
    if (app == nullptr) {
        SetStatus("Select an application first.");
        return;
    }

    char path[160];
    uint32_t installedSize = 0;
    if (!AppBinary(app, path, sizeof(path)) || !StoreFileExists(path, &installedSize)) {
        SetStatus("Install the application before launching it.");
        return;
    }
    if (!app->compatible) {
        SetStatus("This application requires a newer Hashx86 version.");
        return;
    }

    int32_t runningPid = RunningPidFor(app);
    if (runningPid > 0 && syscall_is_process_alive((uint32_t)runningPid)) {
        SetStatus("This application is already running.");
        return;
    }

    int32_t pid = syscall_execve(path, nullptr, nullptr);
    if (pid >= 0) {
        SetRunningPid(app, pid);
        char text[160];
        StoreFormat(text, sizeof(text), "Launching %s...", app->name);
        SetStatus(text);
    } else {
        char text[192];
        StoreFormat(text, sizeof(text), "%s failed to launch.", app->name);
        SetStatus(text);
    }
}

void RefreshClicked() {
    LoadCatalog();
}

}  // namespace

class AppStoreApp {
public:
    AppStoreApp();
};

AppStoreApp::AppStoreApp() {
    for (uint32_t i = 0; i < APPSTORE_MAX_APPS; i++) {
        g_runningProcesses[i].id[0] = '\0';
        g_runningProcesses[i].pid = -1;
    }

    g_window = new Window(desktop, 80, 60, 900, 700);
    g_window->setWindowTitle("App Store");

    Label* subtitle =
        new Label(g_window, 20, 56, 860, 22, "Applications from the Hashx86 catalog.");
    subtitle->setSize(SMALL);

    g_catalogLabel = new Label(g_window, 20, 80, 860, 20, "Catalog: not loaded");
    g_catalogLabel->setSize(TINY);

    g_appList = new HListView(g_window, 20, 106, 350, 420);
    g_appList->AddColumn("Application", 168);
    g_appList->AddColumn("Version", 70);
    g_appList->AddColumn("Status", 0);
    g_appList->OnClick([]() { ShowSelectedApp(); });

    g_bannerView = new HImageView(g_window, 390, 106, 450, 253);
    g_iconView = new HImageView(g_window, 390, 364, 64, 64);

    g_nameLabel = new Label(g_window, 466, 364, 404, 30, "No application selected");
    g_nameLabel->setSize(MEDIUM);

    g_versionLabel = new Label(g_window, 466, 396, 404, 22, "");
    g_versionLabel->setSize(TINY);

    g_descriptionLine1 = new Label(g_window, 390, 436, 490, 20, "");
    g_descriptionLine1->setSize(TINY);
    g_descriptionLine2 = new Label(g_window, 390, 458, 490, 20, "");
    g_descriptionLine2->setSize(TINY);

    g_metaLabel = new Label(g_window, 390, 492, 490, 20, "");
    g_metaLabel->setSize(TINY);
    g_sourceLabel = new Label(g_window, 390, 516, 490, 20, "");
    g_sourceLabel->setSize(TINY);
    g_sizeLabel = new Label(g_window, 390, 540, 490, 20, "");
    g_sizeLabel->setSize(TINY);
    g_requirementLabel = new Label(g_window, 390, 564, 490, 20, "");
    g_requirementLabel->setSize(TINY);
    g_installedLabel = new Label(g_window, 390, 588, 490, 20, "");
    g_installedLabel->setSize(TINY);

    g_progressLabel = new Label(g_window, 20, 610, 860, 22, "");
    g_progressLabel->setSize(SMALL);
    g_statusLabel = new Label(g_window, 20, 640, 860, 50, "Ready.");
    g_statusLabel->setSize(TINY);

    g_installButton = new Button(g_window, 20, 540, 120, 32, "Install");
    g_launchButton = new Button(g_window, 152, 540, 100, 32, "Launch");
    g_refreshButton = new Button(g_window, 264, 540, 100, 32, "Refresh");

    g_window->AddChild(subtitle);
    g_window->AddChild(g_catalogLabel);
    g_window->AddChild(g_appList);
    g_window->AddChild(g_bannerView);
    g_window->AddChild(g_iconView);
    g_window->AddChild(g_nameLabel);
    g_window->AddChild(g_versionLabel);
    g_window->AddChild(g_descriptionLine1);
    g_window->AddChild(g_descriptionLine2);
    g_window->AddChild(g_metaLabel);
    g_window->AddChild(g_sourceLabel);
    g_window->AddChild(g_sizeLabel);
    g_window->AddChild(g_requirementLabel);
    g_window->AddChild(g_installedLabel);
    g_window->AddChild(g_progressLabel);
    g_window->AddChild(g_statusLabel);
    g_window->AddChild(g_installButton);
    g_window->AddChild(g_launchButton);
    g_window->AddChild(g_refreshButton);

    g_installButton->OnClick([]() { InstallSelected(); });
    g_launchButton->OnClick([]() { LaunchSelected(); });
    g_refreshButton->OnClick([]() { RefreshClicked(); });

    g_window->show();
    ShowSelectedApp();
    LoadCatalog();
}

extern "C" void _start(void* arg) {
    init_sys(arg);
    init_graphics();
    printf("[AppStore] Starting...\n");
    new AppStoreApp();
}
