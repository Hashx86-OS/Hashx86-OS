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

#include <Hx86/Hx86.h>

HX86_DECLARE_APP(HX86_APP_CLI);

/**
 * _start() - Read a name from stdin and greet the user.
 * @arg: Program arguments passed by the loader.
 *
 * Prompts for a name, echoing printable characters into a 64-byte buffer, then
 * prints a greeting and waits for Enter before exiting.
 */
extern "C" void _start(void* arg) {
    init_sys(arg);

    printf("Enter your name: ");

    char name[64];
    int nameLen = 0;
    name[0] = '\0';

    while (1) {
        char ch = 0;
        int32_t n = syscall_read(0, &ch, 1);
        if (n <= 0) {
            syscall_sleep(40);
            continue;
        }

        if (ch == '\n' || ch == '\r') {
            break;
        }

        if (ch < 32 || ch > 126) {
            continue;
        }

        if (nameLen < (int)sizeof(name) - 1) {
            name[nameLen++] = ch;
            name[nameLen] = '\0';

            printf("%c", ch);
        }
    }

    printf("\n");
    if (nameLen == 0) {
        printf("Hello friend!\n");
    } else {
        printf("Hello %s!\n", name);
    }

    printf("Press Enter to exit...\n");
    while (1) {
        char ch = 0;
        int32_t n = syscall_read(0, &ch, 1);
        if (n > 0 && (ch == '\n' || ch == '\r')) break;
        syscall_sleep(40);
    }

    syscall_exit_group(0);
}
