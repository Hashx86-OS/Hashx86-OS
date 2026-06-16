#include <Hx86/Hx86.h>

HX86_DECLARE_APP(HX86_APP_CLI);

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

            char echoed[2];
            echoed[0] = ch;
            echoed[1] = '\0';
            printf("%s", echoed);
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
