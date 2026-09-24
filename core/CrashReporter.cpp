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

#include <core/CrashReporter.h>
#include <core/filesystem/Paths.h>

#include <gui/desktop.h>
#include <gui/infodialog.h>
#include <stdlib.h>
#include <string.h>

namespace {
InfoDialog* g_crashDialog = nullptr;

/**
 * GetExceptionName() - Map an exception vector to a human-readable name.
 * @exceptionNumber: CPU exception vector that fired.
 *
 * Return: A static, never-freed string naming the exception.
 */
const char* GetExceptionName(uint8_t exceptionNumber) {
    switch (exceptionNumber) {
        case 0x00:
            return "Division By Zero";
        case 0x06:
            return "Invalid Opcode";
        case 0x0D:
            return "General Protection Fault";
        case 0x0E:
            return "Page Fault";
        case 0x10:
            return "x87 FPU Error";
        case 0x13:
            return "SIMD Floating Point Exception";
        default:
            return "CPU Exception";
    }
}
}  // namespace

/**
 * ShowUserCrashDialog() - Show a crash diagnostic dialog for a thread.
 * @pid: Process identifier of the crashing thread.
 * @tid: Thread identifier of the crashing thread.
 * @exceptionNumber: CPU exception vector that fired.
 * @errorCode: CPU error code pushed with the exception.
 * @eip: Instruction pointer where the exception occurred.
 * @cr2: CR2 value captured for a page fault, or 0 otherwise.
 * @tickSnapshot: Timer tick captured at crash time (reserved for future use).
 *
 * Reuses a single kernel-owned InfoDialog instance (PID 0, so app cleanup
 * never tears it down) and refreshes its content with the failing thread's
 * state.
 *
 * Context: Called from the exception handler path.
 */
void CrashReporter::ShowUserCrashDialog(uint32_t pid, uint32_t tid, uint8_t exceptionNumber,
                                        uint32_t errorCode, uint32_t eip, uint32_t cr2,
                                        uint32_t tickSnapshot) {
    (void)tickSnapshot;  // Reserved for future use, kept for ABI compatibility.
    Desktop* desktop = Desktop::activeInstance;
    if (!desktop) return;

    if (!g_crashDialog) {
        // The dialog is removed via a syscall when the user presses OK.
        g_crashDialog = new InfoDialog(desktop);
        if (!g_crashDialog) {
            HALT("CRITICAL: Failed to allocate crash info dialog!\n");
        }

        g_crashDialog->SetTitleText("Crash Diagnostics");
        g_crashDialog->SetIconBitmap(PATH_ICON_BMP);
        desktop->AddChild(g_crashDialog);
    }

    // Assign a fresh widget ID so FindWidgetByID works, but keep PID 0
    // (kernel-owned) so RemoveAppByPID does not tear down the dialog.
    g_crashDialog->SetID(desktop->getNewID());

    char line1[96];
    char line2[128];
    char line3[96];
    char details[384];
    char num[16];

    auto append_safe = [](char* dest, size_t destSize, const char* src) {
        if (!dest || !src || destSize == 0) return;
        size_t cur = strlen(dest);
        size_t i = 0;
        while (cur + i + 1 < destSize && src[i]) {
            dest[cur + i] = src[i];
            i++;
        }
        dest[cur + i] = '\0';
    };

    line1[0] = '\0';
    append_safe(line1, sizeof(line1), "PID: ");
    itoa((int)pid, num, 10, sizeof(num));
    append_safe(line1, sizeof(line1), num);
    append_safe(line1, sizeof(line1), "  TID: ");
    itoa((int)tid, num, 10, sizeof(num));
    append_safe(line1, sizeof(line1), num);

    line2[0] = '\0';
    append_safe(line2, sizeof(line2), "Exception: 0x");
    itoa((int)exceptionNumber, num, 16, sizeof(num));
    append_safe(line2, sizeof(line2), num);
    append_safe(line2, sizeof(line2), " (");
    append_safe(line2, sizeof(line2), GetExceptionName(exceptionNumber));
    append_safe(line2, sizeof(line2), ")  ERR: 0x");
    itoa((int)errorCode, num, 16, sizeof(num));
    append_safe(line2, sizeof(line2), num);

    line3[0] = '\0';
    append_safe(line3, sizeof(line3), "EIP: 0x");
    itoa((int)eip, num, 16, sizeof(num));
    append_safe(line3, sizeof(line3), num);
    append_safe(line3, sizeof(line3), "  CR2: 0x");
    itoa((int)cr2, num, 16, sizeof(num));
    append_safe(line3, sizeof(line3), num);

    details[0] = '\0';
    append_safe(details, sizeof(details), line1);
    append_safe(details, sizeof(details), "\n");
    append_safe(details, sizeof(details), line2);
    append_safe(details, sizeof(details), "\n");
    append_safe(details, sizeof(details), line3);

    g_crashDialog->SetContent("The application crashed and was closed.",
                              "Review the details below for debugging.", details);
    g_crashDialog->ShowDialog();
    desktop->GetFocus(g_crashDialog);
}
