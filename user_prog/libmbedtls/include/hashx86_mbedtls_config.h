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

#ifndef HASHX86_MBEDTLS_CONFIG_H
#define HASHX86_MBEDTLS_CONFIG_H

/*
 * Hashx86 userspace mbedTLS configuration.
 *
 * Start from upstream's default feature set, then remove everything that
 * assumes a hosted C library, files, sockets or OS threading. The platform
 * hooks (calloc/free, time, gmtime, entropy) live in mbedtls_port.cpp.
 */

#include "mbedtls/config.h"

// No hosted I/O, sockets, wall-clock structs or threading.
#undef MBEDTLS_FS_IO
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_THREADING_C
#undef MBEDTLS_THREADING_PTHREAD
#undef MBEDTLS_THREADING_ALT

// The PSA storage backends assume hosted filesystem APIs.
#undef MBEDTLS_PSA_CRYPTO_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_ITS_FILE_C

// No debug prints or library self-tests.
#undef MBEDTLS_DEBUG_C
#undef MBEDTLS_SELF_TEST

// Platform abstraction: we provide allocation, time and gmtime myself.
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_PLATFORM_TIME_ALT
#define MBEDTLS_PLATFORM_TIME_TYPE_MACRO long
#define MBEDTLS_PLATFORM_GMTIME_R_ALT
#define MBEDTLS_PLATFORM_STD_SNPRINTF snprintf

// Entropy comes from the Hashx86 hardware poll instead of /dev/urandom.
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT

// mbedTLS 2.28 needs the Everest backend for X25519 ECDH; without it the
// client advertises Curve25519 but cannot complete the key exchange. Keep the
// Weierstrass curves (P-256/P-384/P-521), which public servers support.
#undef MBEDTLS_ECP_DP_CURVE25519_ENABLED

#endif  // HASHX86_MBEDTLS_CONFIG_H
