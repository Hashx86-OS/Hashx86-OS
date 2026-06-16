/**
 * @file        syscalls.cpp
 * @brief       Hx86 System Call Implementation
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#include <Hx86/Hsyscalls/syscalls.h>

void syscall_exit(uint32_t status = 0) {
    asm volatile("int $0x80" : : "a"(sys_exit), "b"(status));
}

void syscall_exit_group(uint32_t status) {
    asm volatile("int $0x80" : : "a"(sys_exit_group), "b"(status));
}

int32_t syscall_read(uint32_t fd, char* buf, uint32_t count) {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_read), "b"(fd), "c"(buf), "d"(count)
                 : "memory");
    return return_data;
}

int32_t syscall_write(uint32_t fd, const char* buf, uint32_t count) {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_write), "b"(fd), "c"(buf), "d"(count)
                 : "memory");
    return return_data;
}

int32_t syscall_open(const char* path, int32_t flags) {
    int32_t return_data = -1;
    asm volatile("int $0x80" : "=a"(return_data) : "a"(sys_open), "b"(path), "c"(flags) : "memory");
    return return_data;
}

int32_t syscall_close(uint32_t fd) {
    int32_t return_data = -1;
    asm volatile("int $0x80" : "=a"(return_data) : "a"(sys_close), "b"(fd) : "memory");
    return return_data;
}

int32_t syscall_execve(const char* path, char* const argv[], char* const envp[]) {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_execve), "b"(path), "c"(argv), "d"(envp)
                 : "memory");
    return return_data;
}

int32_t syscall_brk(int32_t increment) {
    int32_t current_brk;
    int32_t new_brk;

    // Get current brk
    asm volatile("int $0x80" : "=a"(current_brk) : "a"(sys_brk), "b"(0));

    if (increment == 0) {
        return current_brk;
    }

    // Set new brk
    asm volatile("int $0x80" : "=a"(new_brk) : "a"(sys_brk), "b"(current_brk + increment));

    if (new_brk == -1 || new_brk == current_brk) {
        return -1;  // Failed to allocate
    }

    return current_brk;  // Return old program break on success
}

int32_t syscall_stat(const char* path, struct stat* statbuf) {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_stat), "b"(path), "c"(statbuf)
                 : "memory");
    return return_data;
}

int32_t syscall_clone(uint32_t clone_flags, void* child_stack, void* parent_tid, void* tls,
                      void* child_tid) {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_clone), "b"(clone_flags), "c"(child_stack), "d"(parent_tid), "S"(tls),
                   "D"(child_tid)
                 : "memory");

    return return_data;
}

int32_t syscall_getdents(uint32_t fd, struct linux_dirent* dirp, uint32_t count) {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_getdents), "b"(fd), "c"(dirp), "d"(count)
                 : "memory");
    return return_data;
}

void syscall_nanosleep(struct timespec* req, struct timespec* rem) {
    asm volatile("int $0x80" : : "a"(sys_nanosleep), "b"(req), "c"(rem) : "memory");
}

void syscall_debug(const char* str) {
    asm volatile("int $0x80" : : "a"(sys_debug), "b"(str));
}

uint32_t syscall_peek_memory(uint32_t address, uint32_t size) {
    int32_t val = 0;
    int32_t return_data = 0;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_peek_memory), "b"(address), "c"(size), "d"(&val)
                 : "memory");
    return (uint32_t)val;
}

uint32_t syscall_Hgui(uint32_t element, uint32_t mode, void* data) {
    int32_t return_data = -1;
    asm volatile("int $0x81" : "=a"(return_data) : "a"(element), "b"(mode), "c"(data));
    return (uint32_t)return_data;
}

uint32_t syscall_register_event_handler(void (*entrypoint)(void*), void* arg) {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_Hcall), "b"(Hsys_regEventH), "c"(arg), "d"(entrypoint)
                 : "memory");

    return (uint32_t)return_data;
}

int32_t syscall_init_cli() {
    int32_t return_data = -1;
    asm volatile("int $0x80" : "=a"(return_data) : "a"(sys_Hcall), "b"(Hsys_initCli) : "memory");
    return return_data;
}

int32_t syscall_stdin_push(uint32_t pid, char c) {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_Hcall), "b"(Hsys_stdinPush), "c"(pid), "d"((uint32_t)(uint8_t)c)
                 : "memory");
    return return_data;
}

int32_t syscall_get_app_mode() {
    int32_t return_data = -1;
    asm volatile("int $0x80" : "=a"(return_data) : "a"(sys_Hcall), "b"(Hsys_getAppMode) : "memory");
    return return_data;
}

int32_t syscall_set_cli_host_view(uint32_t viewId) {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_Hcall), "b"(Hsys_setCliHostView), "c"(viewId)
                 : "memory");
    return return_data;
}

int32_t syscall_get_cli_attached_view() {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_Hcall), "b"(Hsys_getCliAttachedView)
                 : "memory");
    return return_data;
}

int32_t syscall_is_process_alive(uint32_t pid) {
    int32_t return_data = 0;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_Hcall), "b"(Hsys_isProcessAlive), "c"(pid)
                 : "memory");
    return return_data;
}

int32_t syscall_get_process_app_mode(uint32_t pid) {
    int32_t return_data = 0;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_Hcall), "b"(Hsys_getProcessAppMode), "c"(pid)
                 : "memory");
    return return_data;
}

void syscall_get_input(InputState* state) {
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_Hcall), "b"(Hsys_getInput), "c"(state)
                 : "memory");
}
FramebufferInfo syscall_get_framebuffer() {
    uint32_t buffer = 0, width = 0, height = 0;
    int32_t return_data = -1;
    asm volatile("int $0x80"
                 : "=a"(return_data)
                 : "a"(sys_Hcall), "b"(Hsys_getFramebuffer), "c"(&buffer), "d"(&width), "S"(&height)
                 : "memory");

    FramebufferInfo info;
    info.buffer = buffer;
    info.width = width;
    info.height = height;
    return info;
}
