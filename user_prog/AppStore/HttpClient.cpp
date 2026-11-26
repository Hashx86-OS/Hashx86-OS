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

#include "HttpClient.h"

#include "StoreUtil.h"

#include <Hx86/Hnet/Hnet.h>
#include <Hx86/Hsyscalls/syscalls.h>
#include <Hx86/Hx86.h>
#include <Hx86/memory.h>
#include <hashx86_mbedtls.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

namespace {

constexpr uint32_t kMaxUrl = 3072;
constexpr uint32_t kMaxHost = 128;
constexpr uint32_t kMaxPath = 2816;
// The kernel's socket syscalls bounce data through a 4 KiB kernel buffer and
// reject larger single requests. mbedTLS may ask for a full 16 KiB record, so
// every Hnet send/receive must be split to this size.
constexpr uint32_t kHnetChunk = 4096;
constexpr uint32_t kReadBuffer = 4096;
constexpr uint32_t kMaxRedirects = 6;
// Keep the App Store on a small GitHub-focused trust store. The full Mozilla
// bundle makes mbedTLS verify the chain against ~118 roots; on TCG-emulated
// QEMU that takes longer than GitHub's 10-second TLS handshake timeout.
constexpr const char* kCaPath = "/Hashx86/appstore-cacert.pem";

struct ParsedUrl {
    char host[kMaxHost];
    char path[kMaxPath];
    uint16_t port;
};

static mbedtls_entropy_context g_entropy;
static mbedtls_ctr_drbg_context g_ctrDrbg;
static mbedtls_x509_crt g_ca;
static mbedtls_ssl_config g_sslConfig;
static mbedtls_ssl_context g_ssl;
static int32_t g_socketFd = -1;
static bool g_tlsReady = false;

static bool g_sslInitialized = false;
static bool g_sslActive = false;

// TLS 1.2 session tickets let later connections to the same host skip the
// expensive ECDHE exchange and certificate verification. GitHub assets come
// from a handful of hosts, so four slots cover the catalog and app downloads.
constexpr uint32_t kMaxTlsSessions = 4;
struct TlsSessionEntry {
    char host[kMaxHost];
    mbedtls_ssl_session session;
    bool used;
    mbedtls_x509_time notAfter;
};
static TlsSessionEntry g_tlsSessions[kMaxTlsSessions];

TlsSessionEntry* FindTlsSession(const char* host) {
    for (uint32_t i = 0; i < kMaxTlsSessions; i++) {
        if (g_tlsSessions[i].used && StoreStrEqual(g_tlsSessions[i].host, host)) {
            if (mbedtls_x509_time_is_past(&g_tlsSessions[i].notAfter)) {
                mbedtls_ssl_session_free(&g_tlsSessions[i].session);
                g_tlsSessions[i].used = false;
                return nullptr;
            }
            return &g_tlsSessions[i];
        }
    }
    return nullptr;
}

TlsSessionEntry* AllocateTlsSession(const char* host) {
    TlsSessionEntry* existing = FindTlsSession(host);
    if (existing != nullptr) return existing;

    TlsSessionEntry* entry = nullptr;
    for (uint32_t i = 0; i < kMaxTlsSessions; i++) {
        if (!g_tlsSessions[i].used) {
            entry = &g_tlsSessions[i];
            break;
        }
    }
    if (entry == nullptr) entry = &g_tlsSessions[0];

    if (entry->used) {
        mbedtls_ssl_session_free(&entry->session);
    } else {
        mbedtls_ssl_session_init(&entry->session);
    }
    StoreStrCopy(entry->host, sizeof(entry->host), host);
    entry->used = true;
    return entry;
}

void ForgetTlsSession(const char* host) {
    TlsSessionEntry* entry = FindTlsSession(host);
    if (entry == nullptr) return;
    mbedtls_ssl_session_free(&entry->session);
    entry->used = false;
    entry->host[0] = '\0';
}

void RememberTlsSession(const char* host) {
    mbedtls_ssl_session fresh;
    mbedtls_ssl_session_init(&fresh);
    if (mbedtls_ssl_get_session(&g_ssl, &fresh) != 0) {
        mbedtls_ssl_session_free(&fresh);
        return;
    }

    const mbedtls_x509_crt* peer = mbedtls_ssl_get_peer_cert(&g_ssl);
    if (peer == nullptr) {
        mbedtls_ssl_session_free(&fresh);
        return;  // Resumed sessions retain the original cached expiry.
    }
    TlsSessionEntry* entry = AllocateTlsSession(host);
    entry->notAfter = peer->valid_to;
    mbedtls_ssl_session_free(&entry->session);
    entry->session = fresh;
}

void SetError(char* error, uint32_t capacity, const char* message) {
    StoreStrCopy(error, capacity, message);
}

void SetErrorCode(char* error, uint32_t capacity, const char* prefix, int32_t code) {
    StoreFormat(error, capacity, "%s %d", prefix, code);
}

bool ContainsCase(const char* text, const char* needle) {
    if (text == nullptr || needle == nullptr) return false;
    uint32_t needleLen = StoreStrLen(needle);
    if (needleLen == 0) return true;
    for (; *text != '\0'; text++) {
        uint32_t i = 0;
        while (i < needleLen && text[i] != '\0') {
            char a = text[i];
            char b = needle[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            i++;
        }
        if (i == needleLen) return true;
    }
    return false;
}

bool ParseIpv4(const char* text, uint32_t* address) {
    uint32_t octets[4] = {0, 0, 0, 0};
    int index = 0;
    uint32_t value = 0;
    bool haveDigit = false;

    for (const char* p = text;; p++) {
        if (*p >= '0' && *p <= '9') {
            value = value * 10u + (uint32_t)(*p - '0');
            if (value > 255) return false;
            haveDigit = true;
        } else if (*p == '.' || *p == '\0') {
            if (!haveDigit || index > 3) return false;
            octets[index++] = value;
            value = 0;
            haveDigit = false;
            if (*p == '\0') break;
        } else {
            return false;
        }
    }

    if (index != 4) return false;
    *address =
        Hnet::Ip4((uint8_t)octets[0], (uint8_t)octets[1], (uint8_t)octets[2], (uint8_t)octets[3]);
    return true;
}

bool ParseUrl(const char* url, ParsedUrl* parsed) {
    if (url == nullptr || parsed == nullptr) return false;
    if (!StoreStrStartsWithCase(url, "https://")) return false;

    const char* cursor = url + 8;
    const char* hostStart = cursor;
    while (*cursor != '\0' && *cursor != '/' && *cursor != ':' && *cursor != '?' &&
           *cursor != '#') {
        cursor++;
    }
    uint32_t hostLen = (uint32_t)(cursor - hostStart);
    if (hostLen == 0 || hostLen >= sizeof(parsed->host)) return false;
    for (uint32_t i = 0; i < hostLen; i++) parsed->host[i] = hostStart[i];
    parsed->host[hostLen] = '\0';

    parsed->port = 443;
    if (*cursor == ':') {
        cursor++;
        uint32_t port = 0;
        bool haveDigit = false;
        while (*cursor >= '0' && *cursor <= '9') {
            port = port * 10u + (uint32_t)(*cursor - '0');
            if (port > 65535) return false;
            haveDigit = true;
            cursor++;
        }
        if (!haveDigit || port == 0) return false;
        parsed->port = (uint16_t)port;
    }

    if (*cursor != '\0' && *cursor != '/' && *cursor != '?' && *cursor != '#') return false;
    const char* end = cursor;
    while (*end != '\0' && *end != '#') end++;
    uint32_t length = (uint32_t)(end - cursor);
    bool prefix = *cursor != '/';
    if (length + (prefix ? 1u : 0u) >= sizeof(parsed->path)) return false;
    uint32_t offset = prefix ? 1 : 0;
    if (prefix) parsed->path[0] = '/';
    for (uint32_t i = 0; i < length; i++) parsed->path[offset + i] = cursor[i];
    parsed->path[offset + length] = '\0';
    return true;
}

bool ResolveHost(const char* host, uint32_t* address, char* error, uint32_t errorCapacity) {
    if (!ParseIpv4(host, address)) {
        if (Hnet::Resolve(host, address) != 0) {
            StoreFormat(error, errorCapacity, "DNS lookup failed for %s", host);
            return false;
        }
    }
    const uint8_t* octets = (const uint8_t*)address;
    printf("[HTTP] resolve %s -> %u.%u.%u.%u\n", host, octets[0], octets[1], octets[2], octets[3]);
    return true;
}

int TlsSend(void* context, const unsigned char* data, size_t length) {
    (void)context;
    int32_t fd = g_socketFd;
    if (fd < 0) return MBEDTLS_ERR_NET_SEND_FAILED;

    uint32_t sent = 0;
    while (sent < length) {
        uint32_t remaining = (uint32_t)(length - sent);
        uint32_t chunk = remaining < kHnetChunk ? remaining : kHnetChunk;
        int result = Hnet::Send(fd, data + sent, chunk, 0);
        if (result <= 0) return MBEDTLS_ERR_NET_SEND_FAILED;
        sent += (uint32_t)result;
    }
    return (int)length;
}

int TlsRecv(void* context, unsigned char* data, size_t length) {
    (void)context;
    if (length == 0) return 0;
    int32_t fd = g_socketFd;
    if (fd < 0) return MBEDTLS_ERR_NET_RECV_FAILED;

    uint32_t want = length > kHnetChunk ? kHnetChunk : (uint32_t)length;
    uint32_t start = syscall_get_time_ms();
    for (;;) {
        int result = Hnet::Recv(fd, data, want, Hnet::MSG_DONTWAIT);
        if (result >= 0) return result;
        // The kernel preserves EWOULDBLOCK as -2 for nonblocking receives.
        if (result != -2) return MBEDTLS_ERR_NET_RECV_FAILED;
        if ((uint32_t)(syscall_get_time_ms() - start) >= 15000u) {
            Hnet::Close(fd);
            g_socketFd = -1;
            return MBEDTLS_ERR_SSL_TIMEOUT;
        }
        syscall_sleep(1);
    }
}

void TlsInitCleanup() {
    mbedtls_ssl_config_free(&g_sslConfig);
    mbedtls_x509_crt_free(&g_ca);
    mbedtls_ctr_drbg_free(&g_ctrDrbg);
    mbedtls_entropy_free(&g_entropy);
}

bool TlsEnsureReady(char* error, uint32_t errorCapacity) {
    if (g_tlsReady) return true;
    // Free any partial initialization from the previous Refresh attempt.
    mbedtls_ssl_config_free(&g_sslConfig);
    mbedtls_x509_crt_free(&g_ca);
    mbedtls_ctr_drbg_free(&g_ctrDrbg);
    mbedtls_entropy_free(&g_entropy);
    mbedtls_ssl_config_init(&g_sslConfig);
    mbedtls_x509_crt_init(&g_ca);
    Hashx86MbedtlsPlatformInit();

    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_ctrDrbg);
    static const unsigned char kPersonalization[] = "Hashx86AppStore";
    int ret = mbedtls_ctr_drbg_seed(&g_ctrDrbg, mbedtls_entropy_func, &g_entropy, kPersonalization,
                                    sizeof(kPersonalization) - 1);
    if (ret != 0) {
        SetErrorCode(error, errorCapacity, "TLS RNG setup failed:", ret);
        TlsInitCleanup();
        return false;
    }

    uint8_t* caData = nullptr;
    uint32_t caSize = 0;
    if (!StoreReadFile(kCaPath, &caData, &caSize)) {
        StoreFormat(error, errorCapacity, "CA bundle missing: %s", kCaPath);
        TlsInitCleanup();
        return false;
    }

    mbedtls_x509_crt_init(&g_ca);
    ret = mbedtls_x509_crt_parse(&g_ca, caData, caSize + 1);
    kfree(caData);
    if (ret < 0) {
        SetErrorCode(error, errorCapacity, "CA bundle parse failed:", ret);
        TlsInitCleanup();
        return false;
    }

    mbedtls_ssl_config_init(&g_sslConfig);
    ret = mbedtls_ssl_config_defaults(&g_sslConfig, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        SetErrorCode(error, errorCapacity, "TLS config failed:", ret);
        TlsInitCleanup();
        return false;
    }

    mbedtls_ssl_conf_authmode(&g_sslConfig, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&g_sslConfig, &g_ca, nullptr);
    mbedtls_ssl_conf_rng(&g_sslConfig, mbedtls_ctr_drbg_random, &g_ctrDrbg);
    static const char* kAlpnProtocols[] = {"http/1.1", nullptr};
    mbedtls_ssl_conf_alpn_protocols(&g_sslConfig, kAlpnProtocols);

    g_tlsReady = true;
    return true;
}

void TlsClose() {
    if (g_sslActive) {
        mbedtls_ssl_close_notify(&g_ssl);
        g_sslActive = false;
    }
    if (g_socketFd >= 0) {
        Hnet::Close(g_socketFd);
        g_socketFd = -1;
    }
    // Keep the SSL context allocated and reset it for the next connection.
    // Repeated mbedtls_ssl_setup/free cycles are both slow and fragile on the
    // small freestanding heap.
}

bool TlsOpen(const ParsedUrl& url, char* error, uint32_t errorCapacity) {
    if (!TlsEnsureReady(error, errorCapacity)) return false;

    uint32_t address = 0;
    if (!ResolveHost(url.host, &address, error, errorCapacity)) return false;

    int32_t fd = Hnet::Socket(Hnet::SOCK_STREAM, 0);
    if (fd < 0) {
        SetError(error, errorCapacity, "Socket creation failed");
        return false;
    }

    Hnet::SockAddrIn addressIn = {};
    addressIn.sin_len = (uint8_t)sizeof(Hnet::SockAddrIn);
    addressIn.sin_family = Hnet::AF_INET;
    addressIn.sin_port = Hnet::Htons(url.port);
    addressIn.sin_addr = address;

    if (Hnet::Connect(fd, addressIn) != 0) {
        Hnet::Close(fd);
        SetError(error, errorCapacity, "TCP connect failed");
        return false;
    }

    g_socketFd = fd;
    int ret = 0;
    if (!g_sslInitialized) {
        mbedtls_ssl_init(&g_ssl);
        ret = mbedtls_ssl_setup(&g_ssl, &g_sslConfig);
        if (ret != 0) {
            SetErrorCode(error, errorCapacity, "TLS setup failed:", ret);
            mbedtls_ssl_free(&g_ssl);
            TlsClose();
            return false;
        }
        g_sslInitialized = true;
    } else {
        ret = mbedtls_ssl_session_reset(&g_ssl);
        if (ret != 0) {
            SetErrorCode(error, errorCapacity, "TLS session reset failed:", ret);
            TlsClose();
            return false;
        }
    }

    ret = mbedtls_ssl_set_hostname(&g_ssl, url.host);
    if (ret != 0) {
        SetErrorCode(error, errorCapacity, "TLS hostname setup failed:", ret);
        TlsClose();
        return false;
    }

    mbedtls_ssl_set_bio(&g_ssl, &g_socketFd, TlsSend, TlsRecv, nullptr);
    g_sslActive = true;

    TlsSessionEntry* cachedSession = FindTlsSession(url.host);
    bool resumeAttempt = false;
    if (cachedSession != nullptr) {
        int resume = mbedtls_ssl_set_session(&g_ssl, &cachedSession->session);
        if (resume == 0) {
            resumeAttempt = true;
        } else {
            printf("[HTTP] TLS session for %s was not reusable: %d\n", url.host, resume);
            ForgetTlsSession(url.host);
        }
    }

    for (;;) {
        ret = mbedtls_ssl_handshake(&g_ssl);
        if (ret == 0) break;
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            syscall_sleep(1);
            continue;
        }
        printf("[HTTP] TLS handshake %s:%u failed: %d\n", url.host, url.port, ret);
        // A stale ticket must not poison every later connection to this host.
        // Drop it so the retry performs a full handshake.
        if (resumeAttempt) ForgetTlsSession(url.host);
        SetErrorCode(error, errorCapacity, "TLS handshake failed:", ret);
        TlsClose();
        return false;
    }

    uint32_t verify = mbedtls_ssl_get_verify_result(&g_ssl);
    if (verify != 0) {
        if (resumeAttempt) ForgetTlsSession(url.host);
        SetErrorCode(error, errorCapacity, "Certificate verify failed:", (int32_t)verify);
        TlsClose();
        return false;
    }

    RememberTlsSession(url.host);
    return true;
}

bool TlsWriteAll(const uint8_t* data, uint32_t length, char* error, uint32_t errorCapacity) {
    uint32_t written = 0;
    while (written < length) {
        int ret = mbedtls_ssl_write(&g_ssl, data + written, length - written);
        if (ret > 0) {
            written += (uint32_t)ret;
            continue;
        }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            syscall_sleep(1);
            continue;
        }
        SetErrorCode(error, errorCapacity, "TLS write failed:", ret);
        return false;
    }
    return true;
}

struct Reader {
    uint8_t buffer[kReadBuffer];
    uint32_t position;
    uint32_t length;
    bool eof;
    bool cleanEof;
    int32_t error;
};

bool ReaderFill(Reader* reader) {
    if (reader->position < reader->length) return true;
    if (reader->eof || reader->error != 0) return false;

    for (;;) {
        int ret = mbedtls_ssl_read(&g_ssl, reader->buffer, sizeof(reader->buffer));
        if (ret > 0) {
            reader->position = 0;
            reader->length = (uint32_t)ret;
            return true;
        }
        if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
            ret == MBEDTLS_ERR_SSL_CONN_EOF) {
            reader->eof = true;
            reader->cleanEof = ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
            return false;
        }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            syscall_sleep(1);
            continue;
        }
        reader->error = ret;
        return false;
    }
}

uint32_t ReaderReadSome(Reader* reader, uint8_t* output, uint32_t capacity) {
    if (capacity == 0) return 0;
    if (!ReaderFill(reader)) return 0;
    uint32_t available = reader->length - reader->position;
    uint32_t count = available < capacity ? available : capacity;
    for (uint32_t i = 0; i < count; i++) output[i] = reader->buffer[reader->position + i];
    reader->position += count;
    return count;
}

bool ReaderReadLine(Reader* reader, char* output, uint32_t capacity) {
    if (capacity == 0) return false;
    uint32_t count = 0;
    for (;;) {
        if (reader->position >= reader->length && !ReaderFill(reader)) {
            output[count] = '\0';
            return false;
        }
        char c = (char)reader->buffer[reader->position++];
        if (c == '\n') break;
        if (count + 1 >= capacity) return false;
        output[count++] = c;
    }
    if (count > 0 && output[count - 1] == '\r') count--;
    output[count] = '\0';
    return true;
}

bool ParseStatusLine(const char* line, int32_t* status) {
    if (!StoreStrStartsWith(line, "HTTP/")) return false;
    const char* space = line;
    while (*space != '\0' && *space != ' ') space++;
    if (*space != ' ') return false;
    space++;
    if (space[0] < '0' || space[0] > '9' || space[1] < '0' || space[1] > '9' || space[2] < '0' ||
        space[2] > '9') {
        return false;
    }
    *status = (space[0] - '0') * 100 + (space[1] - '0') * 10 + (space[2] - '0');
    return true;
}

bool HeaderNameEquals(const char* line, const char* name) {
    while (*name != '\0') {
        char a = *line;
        char b = *name;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return false;
        line++;
        name++;
    }
    return *line == ':';
}

bool ReadHeaders(Reader* reader, int32_t* status, bool* chunked, bool* hasLength,
                 uint32_t* contentLength, char* location, uint32_t locationCapacity, char* error,
                 uint32_t errorCapacity) {
    char line[4096];
    if (!ReaderReadLine(reader, line, sizeof(line))) {
        SetError(error, errorCapacity, "HTTP status line missing");
        return false;
    }
    if (!ParseStatusLine(line, status)) {
        SetError(error, errorCapacity, "Invalid HTTP status line");
        return false;
    }

    *chunked = false;
    *hasLength = false;
    *contentLength = 0;
    location[0] = '\0';

    for (;;) {
        if (!ReaderReadLine(reader, line, sizeof(line))) {
            SetError(error, errorCapacity, "HTTP headers truncated");
            return false;
        }
        if (line[0] == '\0') break;

        const char* value = line;
        while (*value != '\0' && *value != ':') value++;
        if (*value != ':') continue;
        const char* text = value + 1;
        while (*text == ' ' || *text == '\t') text++;

        if (HeaderNameEquals(line, "content-length")) {
            uint32_t length = 0;
            const char* p = text;
            if (*p < '0' || *p > '9') return false;
            while (*p >= '0' && *p <= '9') {
                uint32_t digit = *p++ - '0';
                if (length > (0xFFFFFFFFu - digit) / 10u) return false;
                length = length * 10u + digit;
            }
            while (*p == ' ' || *p == '\t') p++;
            if (*p != '\0' || (*hasLength && *contentLength != length)) {
                SetError(error, errorCapacity, "Invalid Content-Length");
                return false;
            }
            *contentLength = length;
            *hasLength = true;
        } else if (HeaderNameEquals(line, "transfer-encoding")) {
            if (ContainsCase(text, "chunked")) *chunked = true;
        } else if (HeaderNameEquals(line, "location")) {
            if (!StoreStrCopy(location, locationCapacity, text)) {
                SetError(error, errorCapacity, "Redirect URL too long");
                return false;
            }
        }
    }
    return true;
}

bool ReadFinalHeaders(Reader* reader, int32_t* status, bool* chunked, bool* hasLength,
                      uint32_t* contentLength, char* location, uint32_t capacity, char* error,
                      uint32_t errorCapacity) {
    for (uint32_t count = 0; count < 16; count++) {
        if (!ReadHeaders(reader, status, chunked, hasLength, contentLength, location, capacity,
                         error, errorCapacity))
            return false;
        if (*status == 101) {
            SetError(error, errorCapacity, "HTTP protocol switch is unsupported");
            return false;
        }
        if (*status < 100 || *status >= 200) return true;
    }
    SetError(error, errorCapacity, "Too many informational responses");
    return false;
}

// Remove dot segments from the path without changing its query or fragment.
void NormalizeRedirectPath(char* path) {
    uint32_t read = 0, written = 0;
    auto end = [](char c) { return c == '\0' || c == '?' || c == '#'; };
    while (!end(path[read])) {
        if (StoreStrStartsWith(path + read, "/./")) {
            read += 2;
        } else if (StoreStrStartsWith(path + read, "/.") && end(path[read + 2])) {
            read += 2;
            path[written++] = '/';
        } else if (StoreStrStartsWith(path + read, "/../") ||
                   (StoreStrStartsWith(path + read, "/..") && end(path[read + 3]))) {
            read += 3;
            while (written && path[written - 1] != '/') written--;
            if (written) written--;
            if (end(path[read])) path[written++] = '/';
        } else {
            if (path[read] == '/') path[written++] = path[read++];
            while (!end(path[read]) && path[read] != '/') path[written++] = path[read++];
        }
    }
    do {
        path[written++] = path[read];
    } while (path[read++] != '\0');
}

bool BuildRedirectUrl(const ParsedUrl& current, const char* location, char* output,
                      uint32_t capacity) {
    char joined[kMaxUrl];
    if (StoreStrStartsWithCase(location, "https://")) {
        if (!StoreStrCopy(joined, sizeof(joined), location)) return false;
    } else if (StoreStrStartsWith(location, "//")) {
        if (6u + StoreStrLen(location) >= sizeof(joined)) return false;
        StoreFormat(joined, sizeof(joined), "https:%s", location);
    } else {
        // A colon in the first path component denotes an unsupported scheme.
        for (const char* p = location; *p && *p != '/' && *p != '?' && *p != '#'; p++) {
            if (*p == ':') return false;
        }
        char authority[160];
        if (current.port == 443)
            StoreStrCopy(authority, sizeof(authority), current.host);
        else
            StoreFormat(authority, sizeof(authority), "%s:%u", current.host, current.port);
        char path[kMaxPath];
        if (location[0] == '/') {
            if (!StoreStrCopy(path, sizeof(path), location)) return false;
        } else {
            uint32_t prefix = 0;
            while (current.path[prefix] &&
                   (location[0] == '#' || location[0] == '\0' || current.path[prefix] != '?'))
                prefix++;
            if (location[0] != '?' && location[0] != '#' && location[0] != '\0') {
                while (prefix && current.path[prefix - 1] != '/') prefix--;
            }
            if (prefix + StoreStrLen(location) >= sizeof(path)) return false;
            for (uint32_t i = 0; i < prefix; i++) path[i] = current.path[i];
            StoreStrCopy(path + prefix, sizeof(path) - prefix, location);
        }
        NormalizeRedirectPath(path);
        if (8u + StoreStrLen(authority) + StoreStrLen(path) >= sizeof(joined)) return false;
        StoreFormat(joined, sizeof(joined), "https://%s%s", authority, path);
    }
    ParsedUrl verified;
    if (!ParseUrl(joined, &verified)) return false;
    return StoreStrCopy(output, capacity, joined);
}

bool ReadChunkedBody(Reader* reader, StoreBodySink sink, void* context, char* error,
                     uint32_t errorCapacity) {
    char line[128];
    uint8_t buffer[kReadBuffer];

    for (;;) {
        if (!ReaderReadLine(reader, line, sizeof(line))) {
            SetError(error, errorCapacity, "Chunk size missing");
            return false;
        }

        uint32_t chunkSize = 0;
        const char* p = line;
        bool haveDigit = false;
        while (*p != '\0' && *p != ';') {
            uint32_t digit;
            if (*p >= '0' && *p <= '9') {
                digit = (uint32_t)(*p - '0');
            } else if (*p >= 'a' && *p <= 'f') {
                digit = (uint32_t)(*p - 'a' + 10);
            } else if (*p >= 'A' && *p <= 'F') {
                digit = (uint32_t)(*p - 'A' + 10);
            } else {
                break;
            }
            if (chunkSize > 0x0FFFFFFFu / 16u) {
                SetError(error, errorCapacity, "Chunk size overflow");
                return false;
            }
            chunkSize = chunkSize * 16u + digit;
            haveDigit = true;
            p++;
        }
        if (!haveDigit) {
            SetError(error, errorCapacity, "Invalid chunk size");
            return false;
        }

        if (chunkSize == 0) {
            // Trailer headers end with an empty line.
            for (;;) {
                if (!ReaderReadLine(reader, line, sizeof(line))) {
                    SetError(error, errorCapacity, "Truncated chunk trailers");
                    return false;
                }
                if (line[0] == '\0') return true;
            }
        }

        uint32_t remaining = chunkSize;
        while (remaining != 0) {
            uint32_t want = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            uint32_t got = ReaderReadSome(reader, buffer, want);
            if (got == 0) {
                SetError(error, errorCapacity, "Truncated chunked body");
                return false;
            }
            if (sink != nullptr && !sink(context, buffer, got)) {
                SetError(error, errorCapacity, "Body sink rejected data");
                return false;
            }
            remaining -= got;
        }

        if (!ReaderReadLine(reader, line, sizeof(line)) || line[0] != '\0') {
            SetError(error, errorCapacity, "Malformed chunk terminator");
            return false;
        }
    }
}

bool ReadFixedBody(Reader* reader, uint32_t length, StoreBodySink sink, void* context, char* error,
                   uint32_t errorCapacity) {
    uint8_t buffer[kReadBuffer];
    uint32_t remaining = length;
    while (remaining != 0) {
        uint32_t want = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        uint32_t got = ReaderReadSome(reader, buffer, want);
        if (got == 0) {
            SetError(error, errorCapacity, "Truncated HTTP body");
            return false;
        }
        if (sink != nullptr && !sink(context, buffer, got)) {
            SetError(error, errorCapacity, "Body sink rejected data");
            return false;
        }
        remaining -= got;
    }
    return true;
}

bool ReadUntilEof(Reader* reader, StoreBodySink sink, void* context, char* error,
                  uint32_t errorCapacity) {
    uint8_t buffer[kReadBuffer];
    for (;;) {
        uint32_t got = ReaderReadSome(reader, buffer, sizeof(buffer));
        if (got == 0) {
            if (reader->error != 0) {
                SetErrorCode(error, errorCapacity, "TLS read failed:", reader->error);
                return false;
            }
            if (!reader->cleanEof) {
                SetError(error, errorCapacity, "TLS stream ended without close_notify");
                return false;
            }
            return true;
        }
        if (sink != nullptr && !sink(context, buffer, got)) {
            SetError(error, errorCapacity, "Body sink rejected data");
            return false;
        }
    }
}

bool ReadResponseBody(int32_t status, Reader* reader, bool chunked, bool hasLength, uint32_t length,
                      StoreBodySink sink, void* context, char* error, uint32_t errorCapacity) {
    if (status == 204) return true;
    if (chunked) return ReadChunkedBody(reader, sink, context, error, errorCapacity);
    if (hasLength) return ReadFixedBody(reader, length, sink, context, error, errorCapacity);
    return ReadUntilEof(reader, sink, context, error, errorCapacity);
}

bool SendRequest(const ParsedUrl& url, const char* accept, char* error, uint32_t errorCapacity) {
    char authority[160];
    if (url.port == 443) {
        StoreStrCopy(authority, sizeof(authority), url.host);
    } else {
        StoreFormat(authority, sizeof(authority), "%s:%u", url.host, url.port);
    }

    char request[4096];
    StoreFormat(request, sizeof(request),
                "GET %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: Hashx86-AppStore/0.1\r\n"
                "Accept: %s\r\n"
                "Accept-Encoding: identity\r\n"
                "Connection: close\r\n"
                "\r\n",
                url.path, authority, accept != nullptr ? accept : "*/*");

    uint32_t length = StoreStrLen(request);
    if (length == 0 || length == sizeof(request) - 1) {
        SetError(error, errorCapacity, "HTTP request too long");
        return false;
    }
    return TlsWriteAll((const uint8_t*)request, length, error, errorCapacity);
}

struct RetrySinkContext {
    StoreBodySink sink;
    void* context;
    bool received;
};

bool RetrySink(void* context, const uint8_t* data, uint32_t length) {
    RetrySinkContext* retry = static_cast<RetrySinkContext*>(context);
    retry->received = true;
    return retry->sink == nullptr || retry->sink(retry->context, data, length);
}

}  // namespace

static bool StoreHttpGetOnce(const char* url, const char* accept, StoreBodySink sink, void* context,
                             StoreHttpResponse* response, char* error, uint32_t errorCapacity) {
    if (error != nullptr && errorCapacity != 0) error[0] = '\0';
    if (response != nullptr) {
        response->status = 0;
        response->contentLength = 0;
        response->hasContentLength = false;
        response->chunked = false;
    }
    if (url == nullptr) {
        SetError(error, errorCapacity, "Missing URL");
        return false;
    }

    char currentUrl[kMaxUrl];
    if (!StoreStrCopy(currentUrl, sizeof(currentUrl), url)) {
        SetError(error, errorCapacity, "URL too long");
        return false;
    }

    for (uint32_t redirect = 0; redirect <= kMaxRedirects; redirect++) {
        ParsedUrl parsed;
        if (!ParseUrl(currentUrl, &parsed)) {
            SetError(error, errorCapacity, "Only https:// URLs are supported");
            return false;
        }

        printf("[HTTP] GET %s\n", currentUrl);
        if (!TlsOpen(parsed, error, errorCapacity)) return false;
        if (!SendRequest(parsed, accept, error, errorCapacity)) {
            TlsClose();
            return false;
        }

        Reader reader = {};
        int32_t status = 0;
        bool chunked = false;
        bool hasLength = false;
        uint32_t contentLength = 0;
        char location[kMaxUrl];
        if (!ReadFinalHeaders(&reader, &status, &chunked, &hasLength, &contentLength, location,
                              sizeof(location), error, errorCapacity)) {
            TlsClose();
            return false;
        }

        printf("[HTTP] status %d\n", status);
        if ((status == 301 || status == 302 || status == 303 || status == 307 || status == 308) &&
            location[0] != '\0') {
            char nextUrl[kMaxUrl];
            if (!BuildRedirectUrl(parsed, location, nextUrl, sizeof(nextUrl))) {
                SetError(error, errorCapacity, "Invalid redirect URL");
                TlsClose();
                return false;
            }
            printf("[HTTP] redirect -> %s\n", nextUrl);
            TlsClose();
            if (!StoreStrCopy(currentUrl, sizeof(currentUrl), nextUrl)) {
                SetError(error, errorCapacity, "Redirect URL too long");
                return false;
            }
            continue;
        }

        if (status < 200 || status >= 300) {
            SetErrorCode(error, errorCapacity, "HTTP status", status);
            TlsClose();
            return false;
        }

        if (response != nullptr) {
            response->status = status;
            response->contentLength = contentLength;
            response->hasContentLength = hasLength;
            response->chunked = chunked;
        }

        bool ok = ReadResponseBody(status, &reader, chunked, hasLength, contentLength, sink,
                                   context, error, errorCapacity);

        TlsClose();
        return ok;
    }

    SetError(error, errorCapacity, "Too many redirects");
    return false;
}

bool StoreHttpGet(const char* url, const char* accept, StoreBodySink sink, void* context,
                  StoreHttpResponse* response, char* error, uint32_t errorCapacity) {
    // Retry connection/handshake failures, but never replay a request once the
    // body sink has seen bytes: that would corrupt the file or SHA-256 state.
    //
    // The App Store can start before DHCP has bound the interface (e1000 link
    // establishment is asynchronous), so a DNS failure often just means the
    // network is not up yet. Keep retrying for ~10 seconds so the first
    // catalog fetch survives boot instead of failing in a couple of seconds.
    constexpr int kAttempts = 20;
    for (int attempt = 0; attempt < kAttempts; attempt++) {
        RetrySinkContext retry = {sink, context, false};
        if (StoreHttpGetOnce(url, accept, RetrySink, &retry, response, error, errorCapacity)) {
            return true;
        }
        if (retry.received) return false;
        if (attempt + 1 < kAttempts) {
            printf("[HTTP] retry %d after: %s\n", attempt + 1, error);
            syscall_sleep(500);
        }
    }
    return false;
}
