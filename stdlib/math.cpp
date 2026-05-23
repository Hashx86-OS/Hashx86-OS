/**
 * @file        math.cpp
 * @brief       Math Library Implementation
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#include <stdlib/math.h>

double sin(double x) {
    double res;
    asm volatile("fsin" : "=t"(res) : "0"(x));
    return res;
}

double cos(double x) {
    double res;
    asm volatile("fcos" : "=t"(res) : "0"(x));
    return res;
}

double tan(double x) {
    double res;
    // fptan pushes 1.0, then tan(x).
    // We execute fstp %st(0) to pop that extra 1.0 off the stack.
    asm volatile("fptan; fstp %%st(0)" : "=t"(res) : "0"(x));
    return res;
}

double sqrt(double x) {
    double res;
    asm volatile("fsqrt" : "=t"(res) : "0"(x));
    return res;
}

double abs(double x) {
    return (x < 0) ? -x : x;
}

double pow(double base, int exp) {
    if (exp == 0) return 1.0;
    if (exp < 0) {
        if (base == 0.0) return 0.0;
        base = 1.0 / base;

        // Avoid overflow when negating INT_MIN in freestanding builds.
        if (exp == (-2147483647 - 1)) {
            // -(INT_MIN) overflows int. Set exp to INT_MAX and
            // remember we need one extra multiplication after the loop.
            exp = 2147483647;
            // After the loop we must multiply by base once more.
            // Use a simple flag to track this since base is now the reciprocal.
            double res = 1.0;
            for (int i = 0; i < exp; i++) res *= base;
            res *= base;  // Extra factor for the missing +1 in exponent
            return res;
        } else {
            exp = -exp;
        }
    }
    double res = 1.0;
    for (int i = 0; i < exp; i++) res *= base;
    return res;
}

int floor(double x) {
    int intPart = (int)x;

    // If x is negative and has a fractional part, floor is one less than intPart.
    if (x < 0 && x != intPart) {
        return intPart - 1;
    }

    return intPart;
}

int ceil(double x) {
    int intPart = (int)x;

    // If x is greater than its integer part (e.g. 2.3 > 2), round up.
    if (x > intPart) {
        return intPart + 1;
    }

    // Otherwise (e.g. 3.0 or -2.5), the integer part is the ceiling.
    return intPart;
}
