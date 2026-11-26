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

// 64-bit division helpers.
//
// Hashx86 user programs link with ld -nostdlib and have no libgcc archive.
// mbedTLS bignum code still needs 64-bit division on 32-bit x86, so these
// shift-subtract routines provide the same ABI as libgcc.

#include <stdint.h>

extern "C" {

uint64_t __udivmoddi4(uint64_t numerator, uint64_t denominator, uint64_t* remainder) {
    if (denominator == 0) {
        if (remainder != nullptr) *remainder = 0;
        return 0;
    }

    uint64_t quotient = 0;
    uint64_t rest = 0;
    for (int bit = 63; bit >= 0; bit--) {
        rest = (rest << 1) | ((numerator >> bit) & 1u);
        if (rest >= denominator) {
            rest -= denominator;
            quotient |= (uint64_t)1 << bit;
        }
    }

    if (remainder != nullptr) *remainder = rest;
    return quotient;
}

uint64_t __udivdi3(uint64_t numerator, uint64_t denominator) {
    return __udivmoddi4(numerator, denominator, nullptr);
}

uint64_t __umoddi3(uint64_t numerator, uint64_t denominator) {
    uint64_t remainder = 0;
    __udivmoddi4(numerator, denominator, &remainder);
    return remainder;
}

int64_t __divmoddi4(int64_t numerator, int64_t denominator, int64_t* remainder) {
    bool negativeResult = (numerator < 0) != (denominator < 0);
    uint64_t absNumerator = numerator < 0 ? (~(uint64_t)numerator + 1u) : (uint64_t)numerator;
    uint64_t absDenominator =
        denominator < 0 ? (~(uint64_t)denominator + 1u) : (uint64_t)denominator;

    uint64_t absRemainder = 0;
    uint64_t quotient = __udivmoddi4(absNumerator, absDenominator, &absRemainder);

    if (remainder != nullptr) {
        int64_t signedRemainder = (int64_t)absRemainder;
        *remainder = numerator < 0 ? -signedRemainder : signedRemainder;
    }
    return (int64_t)(negativeResult ? (uint64_t)0 - quotient : quotient);
}

int64_t __divdi3(int64_t numerator, int64_t denominator) {
    return __divmoddi4(numerator, denominator, nullptr);
}

int64_t __moddi3(int64_t numerator, int64_t denominator) {
    int64_t remainder = 0;
    __divmoddi4(numerator, denominator, &remainder);
    return remainder;
}

}  // extern "C"
