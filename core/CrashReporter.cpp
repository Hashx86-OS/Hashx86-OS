/**
 * @file        CrashReporter.cpp
 * @brief       Kernel-side crash reporting orchestration
 *
 * @date        11/03/2026
 * @version     1.0.0
 */

#include <core/CrashReporter.h>

#include <gui/desktop.h>
#include <gui/infodialog.h>
#include <utils/string.h>

namespace {
InfoDialog* g_crashDialog = nullptr;

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

void CrashReporter::ShowUserCrashDialog(uint32_t pid, uint32_t tid, uint8_t exceptionNumber,
                                        uint32_t errorCode, uint32_t eip, uint32_t cr2,
                                        uint32_t tickSnapshot) {
    (void)tickSnapshot;  // Reserved for future use / kept for ABI compatibility
    Desktop* desktop = Desktop::activeInstance;
    if (!desktop) return;

    if (!g_crashDialog) {
        g_crashDialog = new InfoDialog(
            desktop);  // Need to use a syscall to remove the widget properly when press okay
        if (!g_crashDialog) {
            HALT("CRITICAL: Failed to allocate crash info dialog!\n");
        }

        g_crashDialog->SetPID(0);
        g_crashDialog->SetID(desktop->getNewID());
        g_crashDialog->SetTitleText("Crash Diagnostics");
        g_crashDialog->SetIconBitmap("BITMAPS/ICON.BMP");
        desktop->AddChild(g_crashDialog);
    }

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
    itoa(num, 10, (int)pid);
    append_safe(line1, sizeof(line1), num);
    append_safe(line1, sizeof(line1), "  TID: ");
    itoa(num, 10, (int)tid);
    append_safe(line1, sizeof(line1), num);

    line2[0] = '\0';
    append_safe(line2, sizeof(line2), "Exception: 0x");
    itoa(num, 16, (int)exceptionNumber);
    append_safe(line2, sizeof(line2), num);
    append_safe(line2, sizeof(line2), " (");
    append_safe(line2, sizeof(line2), GetExceptionName(exceptionNumber));
    append_safe(line2, sizeof(line2), ")  ERR: 0x");
    itoa(num, 16, (int)errorCode);
    append_safe(line2, sizeof(line2), num);

    line3[0] = '\0';
    append_safe(line3, sizeof(line3), "EIP: 0x");
    itoa(num, 16, (int)eip);
    append_safe(line3, sizeof(line3), num);
    append_safe(line3, sizeof(line3), "  CR2: 0x");
    itoa(num, 16, (int)cr2);
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
