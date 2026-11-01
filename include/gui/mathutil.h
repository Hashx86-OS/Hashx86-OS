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

#ifndef GUI_MATHUTIL_H
#define GUI_MATHUTIL_H

#include <types.h>

/**
 * GuiMulDivU32() - Compute (num / den) without a 64-bit division helper.
 * @num: 64-bit numerator, already reduced to at most 2^64-1.
 * @den: Positive 32-bit divisor.
 *
 * The kernel is freestanding and does not link libgcc, so a plain 64-bit `/`
 * emits an undefined reference to __udivdi3. GUI geometry needs wide
 * intermediates (scrollbar positions, slider fractions) but always divides by
 * a 32-bit track size. This restoring long division uses only 32-bit compares
 * and 64-bit shifts, and returns the truncated quotient.
 *
 * The caller must guarantee the quotient fits in 32 bits.
 *
 * Return: The quotient, or 0 when @den is zero.
 */
static inline uint32_t GuiMulDivU32(uint64_t num, uint32_t den) {
    if (den == 0) return 0;

    uint64_t quotient = 0;
    uint64_t remainder = 0;

    for (int i = 63; i >= 0; --i) {
        // Shift the next numerator bit into the running remainder. A 64-bit
        // remainder is required: den may be as large as 2^32-1, so the
        // pre-shift value can be just below that and the shift crosses 32 bits.
        remainder = (remainder << 1) | (uint32_t)((num >> i) & 1u);
        if (remainder >= den) {
            remainder -= den;
            quotient |= (uint64_t)1 << i;
        }
    }

    return (uint32_t)quotient;
}

/**
 * GuiMulDivSigned() - Compute (a * b) / d for signed values.
 * @a: First factor.
 * @b: Second factor.
 * @d: Positive divisor.
 *
 * Multiplies the magnitudes in 64-bit space and divides with
 * GuiMulDivU32(). The quotient is truncated toward zero, matching C++
 * integer-division semantics. Both factors are 32-bit, so their magnitudes
 * multiply to at most 2^62.
 *
 * Return: The truncated quotient, or 0 when @d is non-positive.
 */
static inline int32_t GuiMulDivSigned(int32_t a, int32_t b, int32_t d) {
    if (d <= 0) return 0;

    const bool negative = (a < 0) != (b < 0);
    uint64_t ua = (a < 0) ? (uint64_t)(-(int64_t)a) : (uint64_t)a;
    uint64_t ub = (b < 0) ? (uint64_t)(-(int64_t)b) : (uint64_t)b;

    uint32_t magnitude = GuiMulDivU32(ua * ub, (uint32_t)d);
    return negative ? -(int32_t)magnitude : (int32_t)magnitude;
}

#endif  // GUI_MATHUTIL_H
