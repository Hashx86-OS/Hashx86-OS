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

#ifndef GUI_TEST_FRAMEWORK_H
#define GUI_TEST_FRAMEWORK_H

#include <cstdint>
#include <cstdio>

/**
 * namespace guitest - Minimal TAP test framework shared by all host suites.
 *
 * Tests self-register through the TEST() macro; main() runs them, prints a TAP
 * plan and reports the total number of assertions. No allocation and no STL, so
 * the same header can be reused by an in-OS runner later.
 */
namespace guitest {

constexpr int MAX_TESTS = 8192;

typedef void (*TestFn)();

struct TestCase {
    const char* name;
    TestFn fn;
};

inline TestCase g_tests[MAX_TESTS];
inline int g_testCount = 0;
inline int g_checks = 0;
inline int g_failed = 0;

inline void Register(const char* name, TestFn fn) {
    if (g_testCount < MAX_TESTS) {
        g_tests[g_testCount].name = name;
        g_tests[g_testCount].fn = fn;
        g_testCount++;
    }
}

struct Registrar {
    Registrar(const char* name, TestFn fn) {
        Register(name, fn);
    }
};

}  // namespace guitest

/**
 * TEST() - Define and register a named test case.
 */
#define TEST(name)                                             \
    static void name();                                        \
    static ::guitest::Registrar name##_registrar(#name, name); \
    static void name()

/** CHECK() - Assert a boolean condition. */
#define CHECK(cond)                                                       \
    do {                                                                  \
        ::guitest::g_checks++;                                            \
        if (!(cond)) {                                                    \
            ::guitest::g_failed++;                                        \
            std::printf("# FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)

/** CHECK_EQ() - Assert that two values compare equal. */
#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        ::guitest::g_checks++;                                                   \
        auto _a = (a);                                                           \
        auto _b = (b);                                                           \
        if (!(_a == _b)) {                                                       \
            ::guitest::g_failed++;                                               \
            std::printf("# FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); \
        }                                                                        \
    } while (0)

/** CHECK_NE() - Assert that two values compare unequal. */
#define CHECK_NE(a, b)                                                           \
    do {                                                                         \
        ::guitest::g_checks++;                                                   \
        auto _a = (a);                                                           \
        auto _b = (b);                                                           \
        if (!(_a != _b)) {                                                       \
            ::guitest::g_failed++;                                               \
            std::printf("# FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
        }                                                                        \
    } while (0)

#endif  // GUI_TEST_FRAMEWORK_H
