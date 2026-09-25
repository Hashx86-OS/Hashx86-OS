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

#include <core/interrupts.h>
#include <core/syscalls_x86.h>
#include <debug.h>
#include <gui/Hgui.h>
#include <gui/gui.h>
#include <types.h>

/**
 * struct timespec - POSIX-style time spec used by nanosleep.
 * @tv_sec: Seconds.
 * @tv_nsec: Nanoseconds.
 */
struct timespec {
    int32_t tv_sec;
    int32_t tv_nsec;
};

/**
 * struct stat - POSIX file status structure.
 * @st_dev: ID of the device containing the file.
 * @st_ino: Inode number.
 * @st_mode: File type and mode.
 * @st_nlink: Number of hard links.
 * @st_uid: User ID of the owner.
 * @st_gid: Group ID of the owner.
 * @st_rdev: Device ID (if a special file).
 * @st_size: Total size, in bytes.
 * @st_blksize: Block size for filesystem I/O.
 * @st_blocks: Number of 512B blocks allocated.
 */
struct stat {
    uint32_t st_dev;      // ID of device containing the file.
    uint32_t st_ino;      // Inode number.
    uint32_t st_mode;     // File type and mode.
    uint32_t st_nlink;    // Number of hard links.
    uint32_t st_uid;      // User ID of the owner.
    uint32_t st_gid;      // Group ID of the owner.
    uint32_t st_rdev;     // Device ID (if a special file).
    uint32_t st_size;     // Total size, in bytes.
    uint32_t st_blksize;  // Block size for filesystem I/O.
    uint32_t st_blocks;   // Number of 512B blocks allocated.
};

/**
 * struct linux_dirent - Directory entry returned by getdents.
 * @d_ino: Inode number.
 * @d_off: Offset to the next linux_dirent.
 * @d_reclen: Length of this linux_dirent.
 * @d_name: Filename (null-terminated).
 */
struct linux_dirent {
    uint32_t d_ino;     // Inode number.
    uint32_t d_off;     // Offset to the next linux_dirent.
    uint16_t d_reclen;  // Length of this linux_dirent.
    char d_name[];      // Filename (null-terminated).
};

/**
 * enum HSYSCALL - Hashx86-specific user-mode service calls.
 * @Hsys_regEventH: Register an event handler.
 * @Hsys_getFramebuffer: Get the framebuffer address and layout.
 * @Hsys_getInput: Get pending input events.
 * @Hsys_initCli: Initialize a CLI host.
 * @Hsys_stdinPush: Push input into a process stdin queue.
 * @Hsys_getAppMode: Get the app mode of a process.
 * @Hsys_setCliHostView: Attach a view to a CLI host.
 * @Hsys_getCliAttachedView: Get the view attached to a CLI host.
 * @Hsys_isProcessAlive: Test whether a process is still running.
 * @Hsys_getProcessAppMode: Get the app mode of another process.
 */
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

/**
 * class SyscallHandler - Interrupt handler that dispatches system calls.
 *
 * Registered as an interrupt handler on the system-call vector; decodes the
 * syscall number and routes it to a SyscallHandlers method.
 */
class SyscallHandler : public InterruptHandler {
public:
    SyscallHandler(uint8_t InterruptNumber, InterruptManager* interruptManager);
    ~SyscallHandler();

    /**
     * HandleInterrupt() - Dispatch a system call from the interrupt entry.
     * @esp: Stack pointer at the time of the system call.
     *
     * Return: The stack pointer to restore after handling.
     */
    virtual uint32_t HandleInterrupt(uint32_t esp);
};

/**
 * class SyscallHandlers - Static implementations of the kernel syscall table.
 *
 * Each handler follows the Linux ABI: return a non-negative value on success
 * or a negative error code on failure.
 */
class SyscallHandlers {
public:
    static int32_t Handle_sys_restart_syscall();
    static int32_t Handle_sys_exit(uint32_t status);
    static int32_t Handle_sys_exit_group(uint32_t status);
    static int32_t Handle_sys_read(uint32_t fd, char* buf, uint32_t count);
    static int32_t Handle_sys_write(uint32_t fd, const char* buf, uint32_t count);
    static int32_t Handle_sys_open(const char* path, int32_t flags);
    static int32_t Handle_sys_close(uint32_t fd);
    static int32_t Handle_sys_lseek(uint32_t fd, int32_t offset, int32_t whence);
    static int32_t Handle_sys_execve(const char* path, char* const argv[], char* const envp[]);
    static int32_t Handle_sys_brk(uint32_t brk);
    static int32_t Handle_sys_stat(const char* path, struct stat* statbuf);
    static int32_t Handle_sys_clone(CPUState* parent_context, uint32_t clone_flags,
                                    void* child_stack, void* parent_tid, void* tls,
                                    void* child_tid);
    static int32_t Handle_sys_getdents(uint32_t fd, struct linux_dirent* dirp, uint32_t count);
    static int32_t Handle_sys_nanosleep(struct timespec* req, struct timespec* rem);
    static int32_t Handle_sys_getcwd(char* buf, uint32_t size);

    static int32_t Handle_sys_debug(char* str);
    static int32_t Handle_sys_peek_memory(uint32_t address, uint32_t size, int32_t* return_data);

    static int32_t Handle_sys_Hcall(uint32_t hcall_id, uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                    uint32_t arg4);
};

#endif  // SYSCALLS_H
