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

#include <cstring>

#include "framework.h"

// Suites self-register when included. Add new suites here.
#include "suites/core_suite.cpp"
#include "suites/event_suite.cpp"
#include "suites/new_widgets_suite.cpp"
#include "suites/widget_base_suite.cpp"
#include "suites/widgets_suite.cpp"

int main(int argc, char** argv) {
    bool countOnly = (argc > 1 && std::strcmp(argv[1], "--count") == 0);

    if (countOnly) {
        for (int i = 0; i < guitest::g_testCount; i++) {
            guitest::g_tests[i].fn();
        }
        std::printf("%d\n", guitest::g_checks);
        return 0;
    }

    std::printf("1..%d\n", guitest::g_testCount);

    int failedTests = 0;
    for (int i = 0; i < guitest::g_testCount; i++) {
        int before = guitest::g_failed;
        guitest::g_tests[i].fn();
        bool ok = (guitest::g_failed == before);
        std::printf("%s %d - %s\n", ok ? "ok" : "not ok", i + 1, guitest::g_tests[i].name);
        if (!ok) failedTests++;
    }

    std::printf("# TOTAL_CHECKS=%d\n", guitest::g_checks);
    std::printf("# TOTAL_TESTS=%d FAILED_TESTS=%d FAILED_CHECKS=%d\n", guitest::g_testCount,
                failedTests, guitest::g_failed);

    if (failedTests) return 1;
    if (guitest::g_checks < 10000) {
        std::printf("# ERROR: only %d checks ran; the suite must produce at least 10000.\n",
                    guitest::g_checks);
        return 2;
    }
    return 0;
}
