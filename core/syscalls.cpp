/**
 * @file        syscalls.cpp
 * @brief       System Calls Interface for #x86
 *
 * @date        20/01/2026
 * @version     1.0.0-beta
 */

#define KDBG_COMPONENT "SYSCALL"
#include <core/drivers/keyboard.h>
#include <core/drivers/mouse.h>
#include <core/filesystem/File.h>
#include <core/filesystem/msdospart.h>
#include <core/globals.h>
#include <core/paging.h>
#include <core/pmm.h>
#include <core/syscalls.h>

namespace {
constexpr uint32_t USER_LOWER_BOUND = 0x10000000;
constexpr uint32_t USER_UPPER_BOUND = 0xC0000000;

bool IsUserRange(ProcessControlBlock* proc, uint32_t addr, size_t size) {
    if (!proc || !g_paging || size == 0) return false;
    if (addr < USER_LOWER_BOUND) return false;
    uint32_t end = addr + (uint32_t)size - 1;
    if (end < addr || end >= USER_UPPER_BOUND) return false;

    uint32_t start_page = addr & ~(PAGE_SIZE - 1);
    uint32_t end_page = end & ~(PAGE_SIZE - 1);
    for (uint32_t page = start_page; page <= end_page; page += PAGE_SIZE) {
        if (g_paging->GetPhysicalAddress(proc->page_directory, page) == 0) {
            return false;
        }
    }
    return true;
}

bool CopyToUser(ProcessControlBlock* proc, void* dst_user, const void* src, size_t size) {
    if (!dst_user || !src || size == 0) return false;
    uint32_t user_addr = (uint32_t)dst_user;
    if (!IsUserRange(proc, user_addr, size)) return false;

    const uint8_t* in = (const uint8_t*)src;
    size_t remaining = size;
    while (remaining > 0) {
        uint32_t phys = g_paging->GetPhysicalAddress(proc->page_directory, user_addr);
        if (!phys) return false;
        uint32_t offset = user_addr & (PAGE_SIZE - 1);
        uint32_t chunk = PAGE_SIZE - offset;
        if (chunk > remaining) chunk = (uint32_t)remaining;
        memcpy((void*)phys, in, chunk);
        in += chunk;
        user_addr += chunk;
        remaining -= chunk;
    }
    return true;
}

bool CopyFromUser(ProcessControlBlock* proc, void* dst, const void* src_user, size_t size) {
    if (!dst || !src_user || size == 0) return false;
    uint32_t user_addr = (uint32_t)src_user;
    if (!IsUserRange(proc, user_addr, size)) return false;

    uint8_t* out = (uint8_t*)dst;
    size_t remaining = size;
    while (remaining > 0) {
        uint32_t phys = g_paging->GetPhysicalAddress(proc->page_directory, user_addr);
        if (!phys) return false;
        uint32_t offset = user_addr & (PAGE_SIZE - 1);
        uint32_t chunk = PAGE_SIZE - offset;
        if (chunk > remaining) chunk = (uint32_t)remaining;
        memcpy(out, (void*)phys, chunk);
        out += chunk;
        user_addr += chunk;
        remaining -= chunk;
    }
    return true;
}

// Bounded NUL-terminated user string copy: reads up to dst_size-1 bytes,
// stops at first NUL, always NUL-terminates dst. Returns true on success.
bool CopyUserString(ProcessControlBlock* proc, const char* src_user, char* dst, size_t dst_size) {
    if (!dst || dst_size == 0) return false;
    if (!src_user) {
        dst[0] = '\0';
        return true;
    }
    uint32_t user_addr = (uint32_t)src_user;
    if (user_addr < USER_LOWER_BOUND) {
        dst[0] = '\0';
        return false;
    }
    size_t i = 0;
    while (i + 1 < dst_size) {
        uint32_t phys = g_paging->GetPhysicalAddress(proc->page_directory, user_addr);
        if (!phys) {
            dst[0] = '\0';
            return false;
        }
        char c = *(char*)phys;
        dst[i++] = c;
        user_addr++;
        if (c == '\0') return true;
    }
    dst[dst_size - 1] = '\0';
    return true;
}
}  // namespace

SyscallHandler::SyscallHandler(uint8_t InterruptNumber, InterruptManager* interruptManager)
    : InterruptHandler(InterruptNumber + 0x20, interruptManager) {}

SyscallHandler::~SyscallHandler() {}

uint32_t SyscallHandler::HandleInterrupt(uint32_t esp) {
    CPUState* cpu = (CPUState*)esp;

    // Linux standard x86:
    // eax = syscall number
    // ebx = arg1
    // ecx = arg2
    // edx = arg3
    // esi = arg4
    // edi = arg5
    // ebp = arg6

    int32_t return_val = 0;

    switch ((uint32_t)cpu->eax) {
        case sys_restart_syscall:
            return_val = SyscallHandlers::Handle_sys_restart_syscall();
            break;

        case sys_exit:
            return_val = SyscallHandlers::Handle_sys_exit(cpu->ebx);
            break;

        case sys_read:
            return_val = SyscallHandlers::Handle_sys_read(cpu->ebx, (char*)cpu->ecx, cpu->edx);
            break;

        case sys_open:
            return_val = SyscallHandlers::Handle_sys_open((const char*)cpu->ebx, cpu->ecx);
            break;
        case sys_close:
            return_val = SyscallHandlers::Handle_sys_close(cpu->ebx);
            break;
        case sys_execve:
            return_val = SyscallHandlers::Handle_sys_execve(
                (const char*)cpu->ebx, (char* const*)cpu->ecx, (char* const*)cpu->edx);
            break;

        case sys_brk:
            return_val = SyscallHandlers::Handle_sys_brk(cpu->ebx);
            break;

        case sys_stat:
            return_val =
                SyscallHandlers::Handle_sys_stat((const char*)cpu->ebx, (struct stat*)cpu->ecx);
            break;

        case sys_clone:
            return_val = SyscallHandlers::Handle_sys_clone(
                cpu, cpu->ebx, (void*)cpu->ecx, (void*)cpu->edx, (void*)cpu->esi, (void*)cpu->edi);
            break;

        case sys_getdents:
            return_val = SyscallHandlers::Handle_sys_getdents(
                cpu->ebx, (struct linux_dirent*)cpu->ecx, cpu->edx);
            break;

        case sys_nanosleep:
            return_val = SyscallHandlers::Handle_sys_nanosleep((struct timespec*)cpu->ebx,
                                                               (struct timespec*)cpu->ecx);
            break;

        case sys_debug:
            return_val = SyscallHandlers::Handle_sys_debug((char*)cpu->ebx);
            break;

        case sys_peek_memory:
            return_val =
                SyscallHandlers::Handle_sys_peek_memory(cpu->ebx, cpu->ecx, (int32_t*)cpu->edx);
            break;

        case sys_Hcall:
            return_val =
                SyscallHandlers::Handle_sys_Hcall(cpu->ebx, cpu->ecx, cpu->edx, cpu->esi, cpu->edi);
            break;

        default:
            KDBG1("Unknown system call: %u\n", cpu->eax);
            return_val = -1;
            break;
    }

    cpu->eax = return_val;
    return esp;
}

int32_t SyscallHandlers::Handle_sys_restart_syscall() {
    KDBG1("sys_restart\n");

    // Use a triple fault to restart the system (Not the best way, but for now this is good :) )
    asm volatile(
        "cli;"
        "lidt (%0);"  // Load an invalid IDT
        "int3;"       // Trigger an interrupt
        ::"r"(0));
    return 0;
}

int32_t SyscallHandlers::Handle_sys_exit(uint32_t status) {
    Scheduler* sched = Scheduler::activeInstance;
    if (!sched) return -1;

    // Save PID before ExitCurrentThread potentially destroys the process
    ProcessControlBlock* process = sched->GetCurrentProcess();
    uint32_t pid = process ? process->pid : 0;

    bool processKilled = sched->ExitCurrentThread();

    // Clean up GUI resources only if the entire process was terminated
    if (processKilled && pid) {
        Desktop::activeInstance->RemoveAppByPID(pid);
        HguiHandler::activeInstance->RemoveAppByPID(pid);

        // Restore Desktop rendering if the owner exits
        if (g_stop_gui_rendering && g_gui_owner_pid == (int)pid) {
            KDBG1("sys_exit: Releasing GUI lock from PID %d", pid);
            g_stop_gui_rendering = false;
            g_gui_owner_pid = -1;
            // Force redraw of desktop
            if (Desktop::activeInstance) Desktop::activeInstance->MarkDirty();
        }

        KDBG1("sys_exit: Process PID %d terminated with status %d", pid, status);
    }
    return 0;  // Technically never returns
}

int32_t SyscallHandlers::Handle_sys_read(uint32_t fd, char* buf, uint32_t count) {
    if (fd <= 2 || !buf || count == 0) return -1;
    ProcessControlBlock* process = Scheduler::activeInstance->GetCurrentProcess();
    File* file = GetFileByFd(process, fd);
    if (!file) return -1;

    // Validate Buffer is User Space
    uint32_t start = (uint32_t)buf;
    if (start < USER_LOWER_BOUND || count == 0 || start > 0xFFFFFFFFu - count + 1) {
        KDBG1("sys_read: SECURITY VIOLATION: Buffer invalid buf=0x%x count=%u", buf, count);
        return -1;
    }
    uint32_t end = start + count - 1;
    if (end >= USER_UPPER_BOUND || !IsUserRange(process, start, count)) {
        KDBG1("sys_read: SECURITY VIOLATION: Buffer crosses kernel buf=0x%x count=%u", buf, count);
        return -1;
    }

    // Read into a kernel buffer, then copy to user space
    uint8_t* kernelBuf = (uint8_t*)kmalloc(count);
    if (!kernelBuf) return -1;
    int bytesRead = file->Read(kernelBuf, count);
    if (bytesRead > 0) {
        if (!CopyToUser(process, buf, kernelBuf, (size_t)bytesRead)) {
            kfree(kernelBuf);
            return -1;
        }
    }
    kfree(kernelBuf);
    return bytesRead;
}

int32_t SyscallHandlers::Handle_sys_open(const char* path, int32_t flags) {
    (void)flags;
    if (!path) return -1;

    ProcessControlBlock* process = Scheduler::activeInstance->GetCurrentProcess();
    char kpath[256];
    if (!CopyUserString(process, path, kpath, sizeof(kpath))) return -1;

    extern MSDOSPartitionTable* g_PartitionTable;
    if (MSDOSPartitionTable::activeInstance && MSDOSPartitionTable::activeInstance->partitions[0]) {
        FAT32* fs = MSDOSPartitionTable::activeInstance->partitions[0];

        File* f = fs->Open(kpath);
        if (!f) return -1;

        int32_t fd = AllocateFd(process, f);
        if (fd < 0) {
            f->Close();
            delete f;
            return -1;
        }
        return fd;
    }
    return -1;
}

int32_t SyscallHandlers::Handle_sys_close(uint32_t fd) {
    if (fd <= 2) return 0;  // stdin/stdout/stderr placeholders

    ProcessControlBlock* process = Scheduler::activeInstance->GetCurrentProcess();
    File* file = GetFileByFd(process, fd);
    if (!file) return -1;

    file->Close();
    delete file;
    ReleaseFd(process, fd);
    return 0;
}

int32_t SyscallHandlers::Handle_sys_execve(const char* path, char* const argv[],
                                           char* const envp[]) {
    if (!path || !g_elfLoader) return -1;

    ProcessControlBlock* proc = Scheduler::activeInstance->GetCurrentProcess();
    char kpath[256];
    if (!CopyUserString(proc, path, kpath, sizeof(kpath))) return -1;

    extern MSDOSPartitionTable* g_PartitionTable;
    if (MSDOSPartitionTable::activeInstance && MSDOSPartitionTable::activeInstance->partitions[0]) {
        FAT32* fs = MSDOSPartitionTable::activeInstance->partitions[0];
        File* f = fs->Open(kpath);
        if (f && f->size > 0) {
            // Hashx86 native loading spins up a new process rather than replacing context.
            // Copy up to 5 argv strings from user space into kernel-owned memory for now.
            ProgramArguments* args =
                new ProgramArguments{nullptr, nullptr, nullptr, nullptr, nullptr};
            ProcessControlBlock* proc = Scheduler::activeInstance->GetCurrentProcess();
            bool argvFailed = false;
            if (argv && proc) {
                const int MAX_ARGS = 5;
                const int MAX_ARG_LEN = 512;
                // Cleanup helper for partial argv allocations
                auto cleanupArgs = [](ProgramArguments* a) {
                    if (a->str1) { kfree((void*)a->str1); a->str1 = nullptr; }
                    if (a->str2) { kfree((void*)a->str2); a->str2 = nullptr; }
                    if (a->str3) { kfree((void*)a->str3); a->str3 = nullptr; }
                    if (a->str4) { kfree((void*)a->str4); a->str4 = nullptr; }
                    if (a->str5) { kfree((void*)a->str5); a->str5 = nullptr; }
                    delete a;
                };
                for (int i = 0; i < MAX_ARGS; i++) {
                    // Read pointer from user argv array
                    char* userPtr = nullptr;
                    if (!CopyFromUser(proc, &userPtr, &argv[i], sizeof(void*))) { argvFailed = true; break; }
                    if (!userPtr) break;  // NULL terminator

                    // Measure and copy string safely (cap length)
                    char* buf = (char*)kmalloc(MAX_ARG_LEN);
                    if (!buf) { argvFailed = true; break; }
                    // Read at most MAX_ARG_LEN-1 bytes
                    size_t read = 0;
                    while (read + 1 < (size_t)MAX_ARG_LEN) {
                        char c = 0;
                        if (!CopyFromUser(proc, &c, (const void*)((uint32_t)userPtr + read), 1)) {
                            kfree(buf);
                            buf = nullptr;
                            break;
                        }
                        buf[read++] = c;
                        if (c == '\0') break;
                    }
                    if (!buf) { argvFailed = true; break; }
                    buf[MAX_ARG_LEN - 1] = '\0';
                    switch (i) {
                        case 0:
                            args->str1 = buf;
                            break;
                        case 1:
                            args->str2 = buf;
                            break;
                        case 2:
                            args->str3 = buf;
                            break;
                        case 3:
                            args->str4 = buf;
                            break;
                        case 4:
                            args->str5 = buf;
                            break;
                    }
                }
                if (argvFailed) {
                    cleanupArgs(args);
                    args = nullptr;
                    f->Close();
                    delete f;
                    return -1;  // Abort — do not continue with null args
                }
            }

            ProcessControlBlock* child = g_elfLoader->loadELF(f, args);
            f->Close();
            delete f;
            if (child) {
                // Return child PID
                return child->pid;
            }
        } else if (f) {
            f->Close();
            delete f;
        }
    }
    return -1;
}

int32_t SyscallHandlers::Handle_sys_brk(uint32_t brk) {
    InterruptGuard guard;
    ProcessControlBlock* process = Scheduler::activeInstance->GetCurrentProcess();

    uint32_t old_brk = process->heap.endAddress;

    // If brk is 0, just return the current limit.
    if (brk == 0) {
        return (int32_t)old_brk;
    }

    // Only allow increasing the brk boundary for now.
    if (brk > old_brk) {
        if (brk > process->heap.maxAddress) {
            KDBG1("sys_brk: Heap Overflow! Max: 0x%x, Req: 0x%x", process->heap.maxAddress, brk);
            return -1;
        }

        uint32_t page_start = (old_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint32_t page_end = (brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        if (g_paging && page_end > page_start) {
            // Track newly allocated frames for rollback on failure
            struct BrkFrame {
                uint32_t vaddr;
                uint32_t phys;
            };
            BrkFrame brkFrames[64];  // Max 64 pages = 256KB per sys_brk call
            int brkCount = 0;

            for (uint32_t addr = page_start; addr < page_end; addr += PAGE_SIZE) {
                // Check if already mapped
                if (g_paging->GetPhysicalAddress(process->page_directory, addr) == 0) {
                    if (brkCount >= 64) {
                        KDBG1("sys_brk: Per-call page limit (64) reached! Rolling back %d pages",
                              brkCount);
                        for (int r = 0; r < brkCount; r++) {
                            g_paging->MapPage(process->page_directory, brkFrames[r].vaddr, 0, 0);
                            asm volatile("invlpg (%0)" ::"r"(brkFrames[r].vaddr) : "memory");
                            pmm_free_block((void*)brkFrames[r].phys);
                        }
                        return -1;
                    }
                    uint32_t phys_frame = (uint32_t)pmm_alloc_block_low(256 * 1024 * 1024);
                    if (!phys_frame) {
                        KDBG1("sys_brk: Out of physical memory! Rolling back %d pages",
                              brkCount);
                        for (int r = 0; r < brkCount; r++) {
                            g_paging->MapPage(process->page_directory, brkFrames[r].vaddr, 0, 0);
                            asm volatile("invlpg (%0)" ::"r"(brkFrames[r].vaddr) : "memory");
                            pmm_free_block((void*)brkFrames[r].phys);
                        }
                        return -1;
                    }
                    if (!g_paging->MapPage(process->page_directory, addr, phys_frame,
                                           PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
                        pmm_free_block((void*)phys_frame);
                        KDBG1("sys_brk: MapPage failed! Rolling back %d pages",
                              brkCount);
                        for (int r = 0; r < brkCount; r++) {
                            g_paging->MapPage(process->page_directory, brkFrames[r].vaddr, 0, 0);
                            asm volatile("invlpg (%0)" ::"r"(brkFrames[r].vaddr) : "memory");
                            pmm_free_block((void*)brkFrames[r].phys);
                        }
                        return -1;
                    }
                    brkFrames[brkCount].vaddr = addr;
                    brkFrames[brkCount].phys = phys_frame;
                    brkCount++;
                }
            }
        }
        process->heap.endAddress = brk;
    }

    // We do not handle shrinking heap at the moment

    return (int32_t)process->heap.endAddress;
}

int32_t SyscallHandlers::Handle_sys_stat(const char* path, struct stat* statbuf) {
    if (!path || !statbuf) return -1;

    ProcessControlBlock* process = Scheduler::activeInstance->GetCurrentProcess();
    char kpath[256];
    if (!CopyUserString(process, path, kpath, sizeof(kpath))) return -1;

    // Build stat in kernel memory, then copy to user at the end
    struct stat k_stat;
    memset(&k_stat, 0, sizeof(k_stat));

    extern MSDOSPartitionTable* g_PartitionTable;
    if (MSDOSPartitionTable::activeInstance && MSDOSPartitionTable::activeInstance->partitions[0]) {
        FAT32* fs = MSDOSPartitionTable::activeInstance->partitions[0];

        // Handling Root Drive Stat Check
        if (kpath[0] == '/' && kpath[1] == '\0') {
            k_stat.st_mode = 0x4000;  // S_IFDIR
            k_stat.st_size = 0;
            return CopyToUser(process, statbuf, &k_stat, sizeof(k_stat)) ? 0 : -1;
        }

        File* f = fs->Open(kpath);
        if (f) {
            k_stat.st_size = f->size;
            k_stat.st_ino = f->id;
            k_stat.st_blksize = 512;
            k_stat.st_blocks = (f->size + 511) / 512;
            if (f->flags & 1) {           // Directory Flag mapped loosely
                k_stat.st_mode = 0x4000;  // S_IFDIR
            } else {
                k_stat.st_mode = 0x8000;  // S_IFREG
            }
            f->Close();
            delete f;
            return CopyToUser(process, statbuf, &k_stat, sizeof(k_stat)) ? 0 : -1;
        }
    }
    return -1;
}

int32_t SyscallHandlers::Handle_sys_clone(CPUState* parent_context, uint32_t clone_flags,
                                          void* child_stack, void* parent_tid, void* tls,
                                          void* child_tid) {
    Scheduler* sched = Scheduler::activeInstance;
    if (!sched || !parent_context) return -1;

    KDBG1("sys_clone: flags=0x%x child_stack=0x%x", clone_flags, (uint32_t)child_stack);

    constexpr uint32_t CLONE_VM = 0x00000100;

    ThreadControlBlock* child = nullptr;
    if (clone_flags & CLONE_VM) {
        child = sched->CloneCurrentThread(parent_context, clone_flags, child_stack, parent_tid, tls,
                                          child_tid);
        if (!child) {
            KDBG1("sys_clone: failed to clone thread");
            return -1;
        }
    } else {
        child = sched->CloneCurrentProcess(parent_context, clone_flags, child_stack, parent_tid,
                                           tls, child_tid);
        if (!child) {
            KDBG1("sys_clone: failed to clone process");
            return -1;
        }
    }

    // Parent return value: child TID (child sees 0 via cloned context->eax).
    return (int32_t)child->tid;
}

int32_t SyscallHandlers::Handle_sys_getdents(uint32_t fd, struct linux_dirent* dirp,
                                             uint32_t count) {
    if (fd <= 2 || !dirp || count == 0) return -1;
    ProcessControlBlock* process = Scheduler::activeInstance->GetCurrentProcess();
    File* dirFile = GetFileByFd(process, fd);
    if (!dirFile || !dirFile->filesystem) return -1;
    if ((dirFile->flags & 1) == 0) return -1;

    // We are streaming raw directory entries 512 bytes at a time
    // DirectoryEntryFat32 is 32 bytes each. We buffer enough for a few.
    uint8_t buffer[512];
    int bytesRead = dirFile->Read(buffer, 512);

    if (bytesRead <= 0) return 0;  // EOF

    uint32_t offsetWritten = 0;
    DirectoryEntryFat32* entries = (DirectoryEntryFat32*)buffer;

    for (int i = 0; i < (bytesRead / 32); i++) {
        DirectoryEntryFat32* e = &entries[i];

        if (e->name[0] == 0x00) {
            return offsetWritten;  // End of directory structure completely
        }
        if (e->name[0] == 0xE5) continue;                                           // Deleted Entry
        if (e->name[0] == '.' && e->name[1] == ' ' && e->name[2] == ' ') continue;  // Root dots

        // VFAT Long File Names Flag (0x0F)
        if ((e->attributes & 0x0F) == 0x0F) continue;

        // Calculate needed spacing for dynamic string
        // Ex: "GAME3D" + "." + "BIN" + \0
        char parsedName[13];
        int j = 0, nameIdx = 0;
        for (j = 0; j < 8 && e->name[j] != ' '; j++) {
            parsedName[nameIdx++] = e->name[j];
        }
        if (e->ext[0] != ' ') {
            parsedName[nameIdx++] = '.';
            for (j = 0; j < 3 && e->ext[j] != ' '; j++) {
                parsedName[nameIdx++] = e->ext[j];
            }
        }
        parsedName[nameIdx] = '\0';

        uint32_t reclen = sizeof(struct linux_dirent) + nameIdx + 1;
        // Align length to 4-byte boundaries roughly
        reclen = (reclen + 3) & ~3;

        if (offsetWritten + reclen > count) {
            // Buffer full, rollback file position up to here so we catch it next request!
            dirFile->position -= (bytesRead - (i * 32));
            return offsetWritten;
        }

        // Build dirent in kernel memory, then copy to user space
        // Use a temporary byte buffer sized to reclen (flexible d_name trailing member)
        uint8_t* direntBuffer = (uint8_t*)kmalloc(reclen);
        if (!direntBuffer) {
            dirFile->position -= (bytesRead - (i * 32));
            return offsetWritten;
        }
        struct linux_dirent* k_dirent = (struct linux_dirent*)direntBuffer;
        k_dirent->d_ino = ((uint32_t)e->firstClusterHi << 16) | e->firstClusterLow;
        k_dirent->d_off = dirFile->position;
        k_dirent->d_reclen = reclen;

        // Copy the name string into the buffer at the flexible array offset
        for (j = 0; j <= nameIdx; j++) {
            k_dirent->d_name[j] = parsedName[j];
        }

        bool copyOk = CopyToUser(process, (uint8_t*)dirp + offsetWritten, direntBuffer, reclen);
        kfree(direntBuffer);

        if (!copyOk) {
            dirFile->position -= (bytesRead - (i * 32));
            return offsetWritten;
        }

        offsetWritten += reclen;
    }

    return offsetWritten;
}

int32_t SyscallHandlers::Handle_sys_nanosleep(struct timespec* req, struct timespec* rem) {
    ProcessControlBlock* process = Scheduler::activeInstance->GetCurrentProcess();

    // Copy timespec structures from user space
    struct timespec k_req, k_rem;
    memset(&k_req, 0, sizeof(k_req));
    memset(&k_rem, 0, sizeof(k_rem));

    if (req) {
        if (!CopyFromUser(process, &k_req, req, sizeof(k_req))) return -1;
    }

    uint32_t sleep_ms = (k_req.tv_sec * 1000) + (k_req.tv_nsec / 1000000);
    Scheduler::activeInstance->Sleep(sleep_ms);

    if (rem) {
        k_rem.tv_sec = 0;
        k_rem.tv_nsec = 0;
        if (!CopyToUser(process, rem, &k_rem, sizeof(k_rem))) return -1;
    }
    return 0;
}

int32_t SyscallHandlers::Handle_sys_debug(char* str) {
    ProcessControlBlock* process = Scheduler::activeInstance->GetCurrentProcess();
    char kstr[256];
    if (!CopyUserString(process, str, kstr, sizeof(kstr))) return -1;
    // ATOMIC PRINT
    InterruptGuard guard;
    KDBG1N("PID %d, %s", g_scheduler->GetCurrentProcess()->pid, kstr);
    return 0;
}

int32_t SyscallHandlers::Handle_sys_peek_memory(uint32_t address, uint32_t size,
                                                int32_t* return_data) {
#if !KDBG_ENABLE
    (void)address;
    (void)size;
    if (return_data) {
        int32_t zero = 0;
        ProcessControlBlock* p = Scheduler::activeInstance ? Scheduler::activeInstance->GetCurrentProcess() : nullptr;
        if (p && IsUserRange(p, (uint32_t)return_data, sizeof(int32_t)))
            CopyToUser(p, return_data, &zero, sizeof(int32_t));
    }
    return -1;
#endif
    ProcessControlBlock* process = Scheduler::activeInstance->GetCurrentProcess();
    if (!process) {
        if (return_data)
            *return_data = 0;
        return -1;
    }
    // [DEV-ONLY] Peek memory allows reading the identity-mapped kernel range (0-256MB).
    // This is exposed to all processes for debugging/development purposes and MUST be
    // restricted to kernel processes or removed entirely for the final OS release.
    // Only allow reading from identity-mapped kernel range (0 - 256MB)
    uint32_t limit = 256 * 1024 * 1024;
    if (address + size > limit || size == 0 || size > 4) {
        if (return_data) {
            int32_t zero = 0;
            if (IsUserRange(process, (uint32_t)return_data, sizeof(int32_t)))
                CopyToUser(process, return_data, &zero, sizeof(int32_t));
        }
        return -1;
    }

    uint32_t value = 0;
    switch (size) {
        case 1:
            value = *(uint8_t*)address;
            break;
        case 2:
            value = *(uint16_t*)address;
            break;
        case 4:
            value = *(uint32_t*)address;
            break;
    }
    if (return_data) {
        if (!IsUserRange(process, (uint32_t)return_data, sizeof(int32_t)) ||
            !CopyToUser(process, return_data, &value, sizeof(int32_t)))
            return -1;
    }
    return 0;
}

int32_t SyscallHandlers::Handle_sys_Hcall(uint32_t hcall_id, uint32_t arg1, uint32_t arg2,
                                          uint32_t arg3, uint32_t arg4) {
    ProcessControlBlock* current_process = Scheduler::activeInstance->GetCurrentProcess();

    if (hcall_id == Hsys_regEventH) {
        KDBG1("Hsys_regEventH: Creating a new Thread for handler");
        void* threadArgs = (void*)arg1;
        void* entryPoint = (void*)arg2;

        ThreadControlBlock* thread = Scheduler::activeInstance->CreateThread(
            current_process, reinterpret_cast<void (*)(void*)>(entryPoint),
            reinterpret_cast<void*>(threadArgs));

        Desktop::activeInstance->createNewHandler(current_process->pid, thread);

        return (int32_t)thread->tid;
    } else if (hcall_id == Hsys_getFramebuffer) {
        // Return framebuffer info: ptr to buffer, width, height passed in
        uint32_t* pBuffer = (uint32_t*)arg1;
        uint32_t* pWidth = (uint32_t*)arg2;
        uint32_t* pHeight = (uint32_t*)arg3;

        extern GraphicsDriver* g_GraphicsDriver;
        extern Paging* g_paging;

        if (g_GraphicsDriver) {
            uint32_t bufferAddr = (uint32_t)g_GraphicsDriver->GetBackBuffer();
            uint32_t width = g_GraphicsDriver->GetWidth();
            uint32_t height = g_GraphicsDriver->GetHeight();
            if ((pBuffer && !IsUserRange(current_process, (uint32_t)pBuffer, sizeof(uint32_t))) ||
                (pWidth && !IsUserRange(current_process, (uint32_t)pWidth, sizeof(uint32_t))) ||
                (pHeight && !IsUserRange(current_process, (uint32_t)pHeight, sizeof(uint32_t)))) {
                return -1;
            }
            if ((pBuffer && !CopyToUser(current_process, pBuffer, &bufferAddr, sizeof(uint32_t))) ||
                (pWidth && !CopyToUser(current_process, pWidth, &width, sizeof(uint32_t))) ||
                (pHeight && !CopyToUser(current_process, pHeight, &height, sizeof(uint32_t)))) {
                return -1;
            }

            // GRANT ACCESS: Grant user-mode access to the kernel backbuffer.
            // The framebuffer resides in the kernel identity-mapped range (pd_idx < 64)
            // whose PDEs are shared across all processes via CreateProcessDirectory.
            // MapPage cannot be used here (our guard rejects kernel-range addresses);
            // instead we set PAGE_USER directly on the existing shared PTEs.
            uint32_t size = width * height * 4;

            // Align start/end to page boundaries
            uint32_t startPage = bufferAddr & ~(PAGE_SIZE - 1);
            uint32_t endPage = (bufferAddr + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

            for (uint32_t addr = startPage; addr < endPage; addr += PAGE_SIZE) {
                uint32_t pd_idx = addr >> 22;
                uint32_t pt_idx = (addr >> 12) & 0x03FF;
                uint32_t* table = (uint32_t*)(current_process->page_directory[pd_idx] & 0xFFFFF000);
                table[pt_idx] |= PAGE_USER;
                asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
            }

            // The PDE must also allow user access for the CPU to permit ring-3 reads/writes.
            uint32_t startPDIdx = startPage >> 22;
            uint32_t endPDIdx = endPage >> 22;
            for (uint32_t i = startPDIdx; i <= endPDIdx; i++) {
                current_process->page_directory[i] |= PAGE_USER;
            }

            // Flush TLB to ensure new permissions take effect immediately
            asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");

            // STOP KERNEL GUI RENDERING
            g_stop_gui_rendering = true;
            g_gui_owner_pid = Scheduler::activeInstance->GetCurrentProcess()->pid;
            KDBG1("Hsys_getFramebuffer: PID %d took ownership of screen", g_gui_owner_pid);

            return 1;
        } else {
            return -1;
        }
    } else if (hcall_id == Hsys_getInput) {
        struct InputState {
            uint8_t keyStates[128];
            int32_t mouseDX;
            int32_t mouseDY;
            uint8_t mouseButtons;
        } __attribute__((packed));

        InputState* userState = (InputState*)arg1;
        if (userState && IsUserRange(current_process, (uint32_t)userState, sizeof(InputState))) {
            InputState tmp;
            memset(&tmp, 0, sizeof(InputState));
            // Copy keyboard state
            if (KeyboardDriver::activeInstance) {
                uint8_t* keys = KeyboardDriver::activeInstance->GetKeyStates();
                for (int i = 0; i < 128; i++) {
                    tmp.keyStates[i] = keys[i];
                }
            }
            // Copy and reset mouse state
            if (MouseDriver::activeInstance) {
                int32_t dx, dy;
                MouseDriver::activeInstance->GetMouseDelta(dx, dy);
                tmp.mouseDX = dx;
                tmp.mouseDY = dy;
                tmp.mouseButtons = MouseDriver::activeInstance->GetButtons();
            }
            if (!CopyToUser(current_process, userState, &tmp, sizeof(InputState))) {
                return -1;
            }
            return 1;
        } else {
            return -1;
        }
    } else {
        // Default case (optional: handle unknown Hcalls)
        KDBG1("Unknown Hcall ID: %d", hcall_id);
    }
    return -1;
}
