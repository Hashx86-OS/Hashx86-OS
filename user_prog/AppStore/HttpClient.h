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

#ifndef APPSTORE_HTTPCLIENT_H
#define APPSTORE_HTTPCLIENT_H

#include <stdint.h>

/** StoreBodySink - Receives decoded HTTP body bytes as they arrive. */
typedef bool (*StoreBodySink)(void* context, const uint8_t* data, uint32_t length);

/** struct StoreHttpResponse - Final response metadata after redirects. */
struct StoreHttpResponse {
    int32_t status;
    uint32_t contentLength;
    bool hasContentLength;
    bool chunked;
};

/**
 * StoreHttpGet() - Perform an HTTPS GET, following redirects.
 *
 * Only https:// URLs are accepted. The response body is passed to @sink in
 * order; redirect responses are consumed internally and never reach the sink.
 *
 * Return: true when a final 2xx response body was read, false on error.
 */
bool StoreHttpGet(const char* url, const char* accept, StoreBodySink sink, void* context,
                  StoreHttpResponse* response, char* error, uint32_t errorCapacity);

#endif  // APPSTORE_HTTPCLIENT_H
