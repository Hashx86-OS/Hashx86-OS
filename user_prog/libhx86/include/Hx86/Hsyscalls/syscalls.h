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

#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <Hx86/Hsyscalls/syscalls_x86.h>
#include <Hx86/stdint.h>

/** struct timespec - Time interval in seconds and nanoseconds. */
struct timespec {
    int32_t tv_sec;   // Seconds.
    int32_t tv_nsec;  // Nanoseconds.
};

/** struct stat - File metadata returned by syscall_stat(). */
struct stat {
    uint32_t st_dev;      // ID of the device containing the file.
    uint32_t st_ino;      // Inode number.
    uint32_t st_mode;     // File type and mode.
    uint32_t st_nlink;    // Number of hard links.
    uint32_t st_uid;      // User ID of the owner.
    uint32_t st_gid;      // Group ID of the owner.
    uint32_t st_rdev;     // Device ID, for special files.
    uint32_t st_size;     // Total size, in bytes.
    uint32_t st_blksize;  // Block size for filesystem I/O.
    uint32_t st_blocks;   // Number of 512-byte blocks allocated.
};

/** struct linux_dirent - Directory entry returned by syscall_getdents(). */
struct linux_dirent {
    uint32_t d_ino;     // Inode number.
    uint32_t d_off;     // Offset to the next linux_dirent entry.
    uint16_t d_reclen;  // Length of this linux_dirent entry.
    char d_name[];      // Filename, NUL-terminated.
};

void syscall_exit(uint32_t status);
void syscall_exit_group(uint32_t status);
int32_t syscall_read(uint32_t fd, char* buf, uint32_t count);
int32_t syscall_write(uint32_t fd, const char* buf, uint32_t count);
int32_t syscall_open(const char* path, int32_t flags);
int32_t syscall_close(uint32_t fd);
int32_t syscall_execve(const char* path, char* const argv[], char* const envp[]);
int32_t syscall_brk(int32_t increment);
int32_t syscall_stat(const char* path, struct stat* statbuf);
int32_t syscall_clone(uint32_t clone_flags, void* child_stack, void* parent_tid, void* tls,
                      void* child_tid);
int32_t syscall_getdents(uint32_t fd, struct linux_dirent* dirp, uint32_t count);
void syscall_nanosleep(struct timespec* req, struct timespec* rem);
int32_t syscall_getcwd(char* buf, uint32_t bufSize);
void syscall_debug(const char* str);
uint32_t syscall_peek_memory(uint32_t address, uint32_t size);

/** HSYSCALL - Hashx86-specific sub-commands dispatched via sys_Hcall. */
typedef enum {
    Hsys_regEventH = 1,
    Hsys_getFramebuffer = 2,
    Hsys_getInput = 3,
    Hsys_initCli = 4,
    Hsys_stdinPush = 5,
    Hsys_getAppMode = 6,
    Hsys_setCliHostView = 7,
    Hsys_getCliAttachedView = 8,
    Hsys_isProcessAlive = 9,
    Hsys_getProcessAppMode = 10,
} HSYSCALL;

/** struct InputState - Input snapshot returned by syscall_get_input().
 * @keyStates: Pressed state of all 128 scancodes.
 * @mouseDX: Mouse X-axis movement since the last frame.
 * @mouseDY: Mouse Y-axis movement since the last frame.
 * @mouseButtons: Number of pressed mouse buttons.
 */
struct InputState {
    uint8_t keyStates[128];
    int32_t mouseDX;
    int32_t mouseDY;
    uint8_t mouseButtons;
} __attribute__((packed));

/** struct FramebufferInfo - Physical framebuffer returned to the GUI process. */
struct FramebufferInfo {
    uint32_t buffer;  // Physical framebuffer address.
    uint32_t width;   // Framebuffer width, in pixels.
    uint32_t height;  // Framebuffer height, in pixels.
};

uint32_t syscall_Hgui(uint32_t element, uint32_t mode, void* data);
uint32_t syscall_register_event_handler(void (*entrypoint)(void*), void* arg);
int32_t syscall_init_cli();
int32_t syscall_stdin_push(uint32_t pid, char c);
int32_t syscall_get_app_mode();
int32_t syscall_set_cli_host_view(uint32_t viewId);
int32_t syscall_get_cli_attached_view();
int32_t syscall_is_process_alive(uint32_t pid);
int32_t syscall_get_process_app_mode(uint32_t pid);
void syscall_get_input(InputState* state);
FramebufferInfo syscall_get_framebuffer();
#endif  // SYSCALLS_H
