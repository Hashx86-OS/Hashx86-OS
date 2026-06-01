/**
 * @file        scheduler.cpp
 * @brief       Standard Round-Robin Scheduler with State Queues for #x86
 *
 * @date        11/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "SCHEDULER"
#include <core/elf.h>
#include <core/scheduler.h>

extern TaskStateSegment g_tss;

// Virtual address for user-mode stacks (just below the 3GB hardware boundary)
#define USER_STACK_VIRT_TOP 0xC0000000
#define USER_STACK_VIRT_BOTTOM 0x10000000

// Virtual address for the user-mode thread exit trampoline (1GB mark, in user space)
#define USER_EXIT_TRAMPOLINE_VIRT 0x40000000

#define KERNEL_STACK_SIZE (64 * 1024)

// Number of pages per User-Mode stack (16KB)
#define USER_STACK_PAGES 4

Scheduler* Scheduler::activeInstance = nullptr;
void FlushSerial();

void IdleTask(void* arg) {
    while (1) {
        // Clear the log buffer to the screen
        FlushSerial();

        asm volatile("sti");
        asm volatile("hlt");
    }
}

// This function acts as the "return address" for all kernel threads.
void ThreadExit() {
    if (Scheduler::activeInstance) {
        Scheduler::activeInstance->ExitCurrentThread();
    }

    while (1) {
        asm volatile("hlt");
    }
}

Scheduler::Scheduler(Paging* pager) {
    _pager = pager;
    _pidCounter = 0;
    _tidCounter = 0;
    currentThread = nullptr;
    activeInstance = this;

    // Allocate and write a user-mode exit trampoline
    // Must be in identity-mapped range (<256MB)
    _trampolinePhys = (uint32_t)pmm_alloc_block_low(256 * 1024 * 1024);
    if (!_trampolinePhys) {
        HALT("CRITICAL: Failed to allocate trampoline page!");
    }
    memset((void*)_trampolinePhys, 0, 4096);

    // Position-Independent Code
    /*
    0xB8 0x01 0x00 0x00 0x00   →   mov eax, 1
    0xCD 0x80                  →   int 0x80
    0xEB 0xFE                  →   jmp $ (relative jump to itself)
    */
    uint8_t* code = (uint8_t*)_trampolinePhys;

    // mov eax, 1  (sys_exit)
    code[0] = 0xB8;
    code[1] = 0x01;
    code[2] = 0x00;
    code[3] = 0x00;
    code[4] = 0x00;
    // int 0x80
    code[5] = 0xCD;
    code[6] = 0x80;
    // jmp $  (safe infinite loop — hlt is privileged and would #GP in Ring 3)
    code[7] = 0xEB;
    code[8] = 0xFE;

    idleThread = CreateThread(nullptr, IdleTask, nullptr);
    if (!idleThread) {
        HALT("CRITICAL: Failed to create idle thread!\n");
    }
    KDBG1("Scheduler initialized. Trampoline=0x%x", _trampolinePhys);
}

ProcessControlBlock* Scheduler::CreateProcess(bool isKernel, void (*entrypoint)(void*), void* arg) {
    InterruptGuard guard;
    ProcessControlBlock* pcb = new ProcessControlBlock();
    if (!pcb) {
        HALT("CRITICAL: Failed to allocate ProcessControlBlock!\n");
    }
    pcb->pid = _pidCounter++;
    pcb->isKernelProcess = isKernel;
    for (uint32_t fd = FD_MIN; fd < FD_MAX; fd++) {
        pcb->fdTable[fd] = nullptr;
    }

    // MEMORY SPACE SETUP
    if (isKernel) {
        pcb->page_directory = _pager->KernelPageDirectory;
    } else {
        pcb->page_directory = _pager->CreateProcessDirectory();
    }

    // Map the user-mode exit trampoline into this process's address space
    if (!isKernel) {
        if (!_pager->MapPage(pcb->page_directory, USER_EXIT_TRAMPOLINE_VIRT, _trampolinePhys,
                             PAGE_PRESENT | PAGE_USER)) {
            KDBG1("CreateProcess PID=%d FAILED: MapPage for exit trampoline", pcb->pid);
            if (pcb->page_directory != _pager->KernelPageDirectory) {
                pmm_free_block(pcb->page_directory);
            }
            delete pcb;
            return nullptr;
        }
    }

    // Register PCB before CreateThread so the child is discoverable if scheduled
    globalProcessList.PushBack(pcb);

    // Create the main thread (Stack setup)
    ThreadControlBlock* mainThread = CreateThread(pcb, entrypoint, arg);
    if (!mainThread) {
        KDBG1("CreateProcess PID=%d Kernel=%d FAILED: CreateThread returned null", pcb->pid,
              isKernel);
        if (!isKernel && pcb->page_directory != _pager->KernelPageDirectory) {
            for (uint32_t pd = 64; pd < 768; pd++) {
                if (!(pcb->page_directory[pd] & PAGE_PRESENT)) continue;
                uint32_t* pt = (uint32_t*)(pcb->page_directory[pd] & 0xFFFFF000);
                for (uint32_t i = 0; i < 1024; i++) {
                    if (!(pt[i] & PAGE_PRESENT)) continue;
                    uint32_t phys = pt[i] & 0xFFFFF000;
                    if (phys && phys != _trampolinePhys) {
                        pmm_free_block((void*)phys);
                    }
                    pt[i] = 0;
                }
                pmm_free_block((void*)(pcb->page_directory[pd] & 0xFFFFF000));
                pcb->page_directory[pd] = 0;
            }
            pmm_free_block(pcb->page_directory);
        }
        globalProcessList.Remove([pcb](ProcessControlBlock* p) { return p == pcb; });
        delete pcb;
        return nullptr;
    }

    KDBG1("CreateProcess PID=%d Kernel=%d", pcb->pid, isKernel);
    return pcb;
}

ThreadControlBlock* Scheduler::CreateThread(ProcessControlBlock* parent, void (*entrypoint)(void*),
                                            void* arg) {
    InterruptGuard guard;
    ThreadControlBlock* tcb = new ThreadControlBlock();
    if (!tcb) return nullptr;

    tcb->tid = _tidCounter++;
    tcb->parent = parent;
    tcb->pid = parent ? parent->pid : 0;

    // Allocate 64KB kernel stack
    tcb->stack = (uint8_t*)kmalloc(KERNEL_STACK_SIZE);
    if (!tcb->stack) {
        KDBG1("CreateThread: failed to allocate kernel stack for TID=%d", tcb->tid);
        delete tcb;
        return nullptr;
    }

    // Calculate the TOP of the stack
    uint32_t* stackTop = (uint32_t*)(tcb->stack + KERNEL_STACK_SIZE);

    // Map the context struct to the top of the kernel stack
    tcb->context = (CPUState*)((uint8_t*)stackTop - sizeof(CPUState));
    memset(tcb->context, 0, sizeof(CPUState));

    // Determine if this is a Kernel or User thread
    bool isKernel = (parent == nullptr || parent->isKernelProcess);

    // COMMON SETUP
    tcb->context->eax = 0;
    tcb->context->ebx = 0;
    tcb->context->eip = (uint32_t)entrypoint;
    tcb->context->eflags = 0x202;  // Interrupts Enabled

    if (isKernel) {
        // KERNEL THREAD (Ring 0)
        tcb->context->cs = 0x08;
        tcb->context->ds = 0x10;
        tcb->context->es = 0x10;
        tcb->context->fs = 0x10;
        tcb->context->gs = 0x10;

        // Kernel thread ABI shim:
        // After interrupt restore + iret (same CPL), ESP points to CPUState::esp field,
        // so CPUState::esp behaves as return address and CPUState::ss as first argument.
        tcb->context->esp = (uint32_t)ThreadExit;  // fake return address
        tcb->context->ss = (uint32_t)arg;          // first function argument
    } else {
        // USER THREAD (Ring 3)
        tcb->context->cs = 0x1B;  // User Code (0x18 | 3)
        tcb->context->ds = 0x23;  // User Data (0x20 | 3)
        tcb->context->es = 0x23;
        tcb->context->fs = 0x23;
        tcb->context->gs = 0x23;

        // Allocate USER-MODE stack (USER_STACK_PAGES pages)
        // Must be in identity-mapped range (<256MB) because kernel writes arg/retaddr to it
        uint32_t user_stack_size = USER_STACK_PAGES * PAGE_SIZE;
        uint64_t stack_offset64 = (uint64_t)tcb->tid * (uint64_t)user_stack_size;
        if (stack_offset64 > (uint64_t)USER_STACK_VIRT_TOP - user_stack_size) {
            kfree(tcb->stack);
            delete tcb;
            return nullptr;
        }
        uint32_t user_stack_base = USER_STACK_VIRT_TOP - (uint32_t)stack_offset64 - user_stack_size;
        if (user_stack_base < USER_STACK_VIRT_BOTTOM) {
            kfree(tcb->stack);
            delete tcb;
            return nullptr;
        }
        uint32_t top_page_phys = 0;

        for (uint32_t p = 0; p < USER_STACK_PAGES; p++) {
            uint32_t phys = (uint32_t)pmm_alloc_block_low(256 * 1024 * 1024);
            if (!phys) {
                KDBG1("CreateThread: Failed to allocate user stack page %d! Low Memory Exhausted?",
                      p);
                // Free previously allocated pages
                for (uint32_t q = 0; q < p; q++) {
                    uint32_t va = user_stack_base + q * PAGE_SIZE;
                    uint32_t pf = _pager->GetPhysicalAddress(parent->page_directory, va);
                    if (pf != 0xFFFFFFFF) {
                        // Unmap before freeing to leave clean page tables
                        uint32_t pd_idx = va >> 22;
                        uint32_t pt_idx = (va >> 12) & 0x3FF;
                        uint32_t* pt = (uint32_t*)(parent->page_directory[pd_idx] & 0xFFFFF000);
                        pt[pt_idx] = 0;
                        asm volatile("invlpg %0" : : "m"(*(uint8_t*)va) : "memory");
                        pmm_free_block((void*)pf);
                    }
                }
                kfree(tcb->stack);
                delete tcb;
                return nullptr;
            }
            memset((void*)phys, 0, PAGE_SIZE);
            uint32_t vaddr = user_stack_base + p * PAGE_SIZE;
            if (!_pager->MapPage(parent->page_directory, vaddr, phys,
                                 PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
                KDBG1("CreateThread: Failed to map user stack page %d!", p);
                pmm_free_block((void*)phys);
                for (uint32_t q = 0; q < p; q++) {
                    uint32_t va = user_stack_base + q * PAGE_SIZE;
                    uint32_t pf = _pager->GetPhysicalAddress(parent->page_directory, va);
                    if (pf != 0xFFFFFFFF) {
                        // Unmap before freeing to leave clean page tables
                        uint32_t pd_idx = va >> 22;
                        uint32_t pt_idx = (va >> 12) & 0x3FF;
                        uint32_t* pt = (uint32_t*)(parent->page_directory[pd_idx] & 0xFFFFF000);
                        pt[pt_idx] = 0;
                        asm volatile("invlpg %0" : : "m"(*(uint8_t*)va) : "memory");
                        pmm_free_block((void*)pf);
                    }
                }
                kfree(tcb->stack);
                delete tcb;
                return nullptr;
            }
            if (p == USER_STACK_PAGES - 1) top_page_phys = phys;
        }

        // Write arg and return address to the TOP of the stack (highest page, last 8 bytes)
        uint32_t* user_stack_top_phys = (uint32_t*)(top_page_phys + PAGE_SIZE);
        {
            uint32_t pd_idx = top_page_phys >> 22;
            uint32_t pt_idx = (top_page_phys >> 12) & 0x3FF;
            uint32_t* pt = (uint32_t*)(parent->page_directory[pd_idx] & 0xFFFFF000);
            KDBG1(
                "CreateThread: top_page_phys=0x%x pd[%u]=0x%x kernel_pd[%u]=0x%x "
                "pte[%u]=0x%x pt_virt=0x%x",
                top_page_phys, pd_idx, parent->page_directory[pd_idx], pd_idx,
                _pager->KernelPageDirectory[pd_idx], pt_idx, pt[pt_idx], top_page_phys);
        }
        user_stack_top_phys[-1] = (uint32_t)arg;              // Argument
        user_stack_top_phys[-2] = USER_EXIT_TRAMPOLINE_VIRT;  // Return to exit trampoline

        // IRET will pop SS:ESP for Ring 0 -> Ring 3 transition
        tcb->context->esp = user_stack_base + user_stack_size - 8;
        tcb->context->ss = 0x23;
    }

    if (parent != nullptr) {
        parent->threads.PushBack(tcb);
        tcb->state = THREAD_STATE_READY;
        readyQueue.PushBack(tcb);
    }

    if (arg == nullptr) {
        KDBG1("WARNING: Thread TID %d created with NULL arg!", tcb->tid);
    }

    KDBG1("CreateThread TID=%d PID=%d EIP=0x%x", tcb->tid, tcb->pid, entrypoint);
    return tcb;
}

ThreadControlBlock* Scheduler::CloneCurrentThread(CPUState* parentContext, uint32_t clone_flags,
                                                  void* child_stack, void* parent_tid, void* tls,
                                                  void* child_tid) {
    InterruptGuard guard;

    if (!currentThread || !currentThread->parent || !parentContext) {
        return nullptr;
    }

    ProcessControlBlock* parent = currentThread->parent;
    ThreadControlBlock* tcb = new ThreadControlBlock();
    if (!tcb) return nullptr;

    tcb->tid = _tidCounter++;
    tcb->parent = parent;
    tcb->pid = parent->pid;
    tcb->wakeTime = 0;

    // Each thread still needs its own kernel stack for IRQ/syscall context switches.
    tcb->stack = (uint8_t*)kmalloc(KERNEL_STACK_SIZE);
    if (!tcb->stack) {
        delete tcb;
        return nullptr;
    }

    uint32_t* stackTop = (uint32_t*)(tcb->stack + KERNEL_STACK_SIZE);
    tcb->context = (CPUState*)((uint8_t*)stackTop - sizeof(CPUState));
    memcpy(tcb->context, parentContext, sizeof(CPUState));

    // Linux clone contract: child returns 0 from clone.
    tcb->context->eax = 0;

    // When child_stack is null, allocate a distinct user stack for the child
    // so both threads do not share one user stack.
    if (child_stack) {
        tcb->context->esp = (uint32_t)child_stack;
    } else {
        // Allocate a small user stack for the child thread
        uint32_t user_stack_size = 4096;
        uint32_t user_stack_phys = (uint32_t)pmm_alloc_block_low(256 * 1024 * 1024);
        if (!user_stack_phys) {
            kfree(tcb->stack);
            delete tcb;
            return nullptr;
        }
        memset((void*)user_stack_phys, 0, 4096);
        // Map it at a fixed high user address (just below 3GB) unique per thread
        uint64_t stack_virt64 = (uint64_t)0xBFFF0000 - ((uint64_t)tcb->tid * 4096ULL);
        if (stack_virt64 < 0x10000000ULL || stack_virt64 > 0xFFFFFFFFULL) {
            pmm_free_block((void*)user_stack_phys);
            kfree(tcb->stack);
            delete tcb;
            return nullptr;
        }
        uint32_t user_stack_virt = (uint32_t)stack_virt64;
        if (!_pager->MapPage(parent->page_directory, user_stack_virt, user_stack_phys,
                             PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
            pmm_free_block((void*)user_stack_phys);
            kfree(tcb->stack);
            delete tcb;
            return nullptr;
        }
        // Set esp to top of the allocated page
        tcb->context->esp = user_stack_virt + 4096 - 8;
        // Write a return address to the exit trampoline (bottom of stack)
        uint32_t* stack_top_phys = (uint32_t*)(user_stack_phys + 4096);
        stack_top_phys[-1] = 0;  // Argument
        stack_top_phys[-2] = USER_EXIT_TRAMPOLINE_VIRT;
    }

    // Best-effort handling for common TID reporting flags.
    constexpr uint32_t CLONE_PARENT_SETTID = 0x00100000;
    constexpr uint32_t CLONE_CHILD_SETTID = 0x01000000;
    constexpr uint32_t USER_LOWER_BOUND = 0x10000000;
    constexpr uint32_t KERNEL_BASE = 0xC0000000;

    // Validate that the full 4-byte range is mapped, in user space, and does not cross a page
    // boundary
    auto safeWriteTid = [&](void* addr, uint32_t tid_val, uint32_t* page_dir) -> bool {
        uint32_t uaddr = (uint32_t)addr;
        if (uaddr < USER_LOWER_BOUND) return false;
        uint32_t end = uaddr + sizeof(uint32_t) - 1;
        if (end < uaddr || end >= KERNEL_BASE) return false;
        // Check that write stays within a single page
        if ((uaddr & (PAGE_SIZE - 1)) > PAGE_SIZE - sizeof(uint32_t)) return false;
        // Verify both start and end pages are mapped
        for (uint32_t page = uaddr & ~(PAGE_SIZE - 1); page <= end; page += PAGE_SIZE) {
            if (_pager->GetPhysicalAddress(page_dir, page) == 0xFFFFFFFF) return false;
        }
        uint32_t phys = _pager->GetPhysicalAddress(page_dir, uaddr);
        if (phys == 0xFFFFFFFF) return false;
        *(uint32_t*)phys = tid_val;
        return true;
    };

    if ((clone_flags & CLONE_PARENT_SETTID) && parent_tid) {
        safeWriteTid(parent_tid, tcb->tid, parent->page_directory);
    }
    if ((clone_flags & CLONE_CHILD_SETTID) && child_tid) {
        safeWriteTid(child_tid, tcb->tid, parent->page_directory);
    }

    // TLS install is architecture-ABI specific; keep ignored for now.
    (void)tls;

    parent->threads.PushBack(tcb);
    tcb->state = THREAD_STATE_READY;
    readyQueue.PushBack(tcb);

    KDBG1("CloneCurrentThread parent TID=%d -> child TID=%d PID=%d EIP=0x%x ESP=0x%x",
          currentThread->tid, tcb->tid, tcb->pid, tcb->context->eip, tcb->context->esp);

    return tcb;
}

ThreadControlBlock* Scheduler::CloneCurrentProcess(CPUState* parentContext, uint32_t clone_flags,
                                                   void* child_stack, void* parent_tid, void* tls,
                                                   void* child_tid) {
    InterruptGuard guard;

    if (!currentThread || !currentThread->parent || !parentContext) {
        return nullptr;
    }

    ProcessControlBlock* parent = currentThread->parent;
    if (parent->isKernelProcess) {
        KDBG1("CloneCurrentProcess: kernel process clone is unsupported");
        return nullptr;
    }

    ProcessControlBlock* childProc = new ProcessControlBlock();
    if (!childProc) return nullptr;

    childProc->pid = _pidCounter++;
    childProc->isKernelProcess = false;
    childProc->parent = parent;
    childProc->page_directory = _pager->CreateProcessDirectory();
    childProc->heap = parent->heap;
    for (uint32_t fd = FD_MIN; fd < FD_MAX; fd++) {
        childProc->fdTable[fd] = nullptr;
    }

    if (!childProc->page_directory) {
        delete childProc;
        return nullptr;
    }

    auto freeChildAddressSpace = [&]() {
        for (uint32_t pd = 64; pd < 768; pd++) {
            if (!(childProc->page_directory[pd] & PAGE_PRESENT)) continue;

            uint32_t* pt = (uint32_t*)(childProc->page_directory[pd] & 0xFFFFF000);
            for (uint32_t i = 0; i < 1024; i++) {
                if (!(pt[i] & PAGE_PRESENT)) continue;
                uint32_t phys = pt[i] & 0xFFFFF000;
                if (phys && phys != _trampolinePhys) {
                    pmm_free_block((void*)phys);
                }
                pt[i] = 0;
            }

            pmm_free_block((void*)(childProc->page_directory[pd] & 0xFFFFF000));
            childProc->page_directory[pd] = 0;
        }

        pmm_free_block(childProc->page_directory);
        childProc->page_directory = nullptr;
    };

    // Duplicate user-space mappings [256MB, 3GB) page-by-page.
    for (uint32_t pd = 64; pd < 768; pd++) {
        if (!(parent->page_directory[pd] & PAGE_PRESENT)) continue;

        uint32_t* parentTable = (uint32_t*)(parent->page_directory[pd] & 0xFFFFF000);
        for (uint32_t pt = 0; pt < 1024; pt++) {
            if (!(parentTable[pt] & PAGE_PRESENT)) continue;

            uint32_t srcPhys = parentTable[pt] & 0xFFFFF000;
            uint32_t dstPhys = (uint32_t)pmm_alloc_block_low(256 * 1024 * 1024);
            if (!dstPhys) {
                KDBG1("CloneCurrentProcess: out of low memory while copying pages");
                freeChildAddressSpace();
                delete childProc;
                return nullptr;
            }

            memcpy((void*)dstPhys, (void*)srcPhys, PAGE_SIZE);

            uint32_t virt = (pd << 22) | (pt << 12);
            uint32_t flags = parentTable[pt] & 0xFFF;
            if (!_pager->MapPage(childProc->page_directory, virt, dstPhys, flags)) {
                pmm_free_block((void*)dstPhys);
                freeChildAddressSpace();
                delete childProc;
                return nullptr;
            }
        }
    }

    ThreadControlBlock* tcb = new ThreadControlBlock();
    if (!tcb) {
        freeChildAddressSpace();
        delete childProc;
        return nullptr;
    }

    tcb->tid = _tidCounter++;
    tcb->parent = childProc;
    tcb->pid = childProc->pid;
    tcb->wakeTime = 0;
    tcb->stack = (uint8_t*)kmalloc(KERNEL_STACK_SIZE);
    if (!tcb->stack) {
        delete tcb;
        freeChildAddressSpace();
        delete childProc;
        return nullptr;
    }

    uint32_t* stackTop = (uint32_t*)(tcb->stack + KERNEL_STACK_SIZE);
    tcb->context = (CPUState*)((uint8_t*)stackTop - sizeof(CPUState));
    memcpy(tcb->context, parentContext, sizeof(CPUState));
    tcb->context->eax = 0;

    // When child_stack is null in a CLONE_VM context, allocate a distinct user stack
    if (child_stack) {
        tcb->context->esp = (uint32_t)child_stack;
    } else {
        uint32_t user_stack_size = 4096;
        uint32_t user_stack_phys = (uint32_t)pmm_alloc_block_low(256 * 1024 * 1024);
        if (!user_stack_phys) {
            kfree(tcb->stack);
            delete tcb;
            if (childProc->page_directory) freeChildAddressSpace();
            delete childProc;
            return nullptr;
        }
        memset((void*)user_stack_phys, 0, 4096);
        uint64_t stack_virt64 = (uint64_t)0xBFFF0000 - ((uint64_t)tcb->tid * 4096ULL);
        if (stack_virt64 < 0x10000000ULL || stack_virt64 > 0xFFFFFFFFULL) {
            pmm_free_block((void*)user_stack_phys);
            kfree(tcb->stack);
            delete tcb;
            if (childProc->page_directory) freeChildAddressSpace();
            delete childProc;
            return nullptr;
        }
        uint32_t user_stack_virt = (uint32_t)stack_virt64;
        if (!_pager->MapPage(childProc->page_directory, user_stack_virt, user_stack_phys,
                             PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
            pmm_free_block((void*)user_stack_phys);
            kfree(tcb->stack);
            delete tcb;
            if (childProc->page_directory) freeChildAddressSpace();
            delete childProc;
            return nullptr;
        }
        uint32_t* stack_top_phys = (uint32_t*)(user_stack_phys + 4096);
        stack_top_phys[-1] = 0;
        stack_top_phys[-2] = USER_EXIT_TRAMPOLINE_VIRT;
        tcb->context->esp = user_stack_virt + 4096 - 8;
    }

    constexpr uint32_t CLONE_PARENT_SETTID = 0x00100000;
    constexpr uint32_t CLONE_CHILD_SETTID = 0x01000000;
    constexpr uint32_t USER_LOWER_BOUND = 0x10000000;
    constexpr uint32_t KERNEL_BASE = 0xC0000000;

    auto safeWriteTid = [&](void* addr, uint32_t tid_val, uint32_t* page_dir) -> bool {
        uint32_t uaddr = (uint32_t)addr;
        if (uaddr < USER_LOWER_BOUND) return false;
        uint32_t end = uaddr + sizeof(uint32_t) - 1;
        if (end < uaddr || end >= KERNEL_BASE) return false;
        if ((uaddr & (PAGE_SIZE - 1)) > PAGE_SIZE - sizeof(uint32_t)) return false;
        for (uint32_t page = uaddr & ~(PAGE_SIZE - 1); page <= end; page += PAGE_SIZE) {
            if (_pager->GetPhysicalAddress(page_dir, page) == 0xFFFFFFFF) return false;
        }
        uint32_t phys = _pager->GetPhysicalAddress(page_dir, uaddr);
        if (phys == 0xFFFFFFFF) return false;
        *(uint32_t*)phys = tid_val;
        return true;
    };

    if ((clone_flags & CLONE_PARENT_SETTID) && parent_tid) {
        safeWriteTid(parent_tid, tcb->tid, parent->page_directory);
    }

    if ((clone_flags & CLONE_CHILD_SETTID) && child_tid) {
        safeWriteTid(child_tid, tcb->tid, childProc->page_directory);
    }

    // TLS setup is deferred until TLS/GDT model support is wired.
    (void)tls;

    childProc->threads.PushBack(tcb);
    tcb->state = THREAD_STATE_READY;
    readyQueue.PushBack(tcb);
    globalProcessList.PushBack(childProc);

    KDBG1("CloneCurrentProcess parent PID=%d/TID=%d -> child PID=%d/TID=%d EIP=0x%x ESP=0x%x",
          parent->pid, currentThread->tid, childProc->pid, tcb->tid, tcb->context->eip,
          tcb->context->esp);

    return tcb;
}

bool Scheduler::KillProcess(uint32_t pid) {
    InterruptGuard guard;
    ProcessControlBlock* target = nullptr;
    int pCount = globalProcessList.GetSize();
    for (int i = 0; i < pCount; i++) {
        ProcessControlBlock* temp = globalProcessList.PopFront();
        if (temp->pid == pid) target = temp;
        globalProcessList.PushBack(temp);
        if (target) break;
    }
    if (!target) return false;

    // RESOURCE CLEANUP START
    uint32_t currentCR3;
    asm volatile("mov %%cr3, %0" : "=r"(currentCR3));
    if ((uint32_t)target->page_directory == currentCR3) {
        // Switch to Kernel Page Directory to safely free resources
        _pager->SwitchDirectory(_pager->KernelPageDirectory);
    }

    // Terminate all threads (removes from scheduler queues, frees kernel stacks)
    int tCount = target->threads.GetSize();
    for (int i = 0; i < tCount; i++) {
        ThreadControlBlock* t = target->threads.PopFront();
        TerminateThread(t);
    }

    // Free Page Tables and Page Directory (if not Kernel)
    // This also reclaims all user-space page frames (stacks, heap, etc.)
    // via the PTE sweep below — no need to free them separately.
    if (!target->isKernelProcess) {
        // Free User Page Tables (Indices 64 to 768)
        // Kernel tables (0-63) and High Mem (768-1023) are shared, CANNOT FREE
        for (int i = 64; i < 768; i++) {
            if (!(target->page_directory[i] & PAGE_PRESENT)) continue;

            // First, free every individual page frame pointed to by this table's PTEs
            uint32_t* pt = (uint32_t*)(target->page_directory[i] & 0xFFFFF000);
            for (uint32_t j = 0; j < 1024; j++) {
                if (!(pt[j] & PAGE_PRESENT)) continue;
                uint32_t phys = pt[j] & 0xFFFFF000;
                // Don't free the shared exit trampoline page
                if (phys && phys != _trampolinePhys) {
                    pmm_free_block((void*)phys);
                }
                pt[j] = 0;
            }
            // Then free the page table itself
            pmm_free_block((void*)(target->page_directory[i] & 0xFFFFF000));
            target->page_directory[i] = 0;
        }

        // Free privately-copied kernel-range page tables (indices 0-63 and 768-1023).
        // These were created (e.g., by Hsys_getFramebuffer) as per-process copies of
        // shared kernel PDEs.  Their PTEs still point to shared kernel frames, so we
        // free only the page table frame, not the individual frames.
        for (int i = 0; i < 64; i++) {
            if (!(target->page_directory[i] & PAGE_PRESENT)) continue;
            if (target->page_directory[i] == _pager->KernelPageDirectory[i]) continue;
            pmm_free_block((void*)(target->page_directory[i] & 0xFFFFF000));
            target->page_directory[i] = 0;
        }
        for (int i = 768; i < 1024; i++) {
            if (!(target->page_directory[i] & PAGE_PRESENT)) continue;
            if (target->page_directory[i] == _pager->KernelPageDirectory[i]) continue;
            pmm_free_block((void*)(target->page_directory[i] & 0xFFFFF000));
            target->page_directory[i] = 0;
        }

        // Free the Directory itself
        pmm_free_block(target->page_directory);
    }

    // RESOURCE CLEANUP END

    // Free program arguments (kmalloc'd strings + struct)
    FreeProgramArguments(target->programArgs);
    target->programArgs = nullptr;

    // Remove from Global List
    globalProcessList.Remove([target](ProcessControlBlock* p) { return p == target; });

    delete target;

    KDBG1("KillProcess PID=%d success", pid);
    return true;
}

void Scheduler::TerminateThread(ThreadControlBlock* thread) {
    InterruptGuard guard;
    if (!thread) return;
    if (thread->state == THREAD_STATE_TERMINATED) return;

    KDBG1("TerminateThread TID=%d", thread->tid);

    thread->state = THREAD_STATE_TERMINATED;
    readyQueue.Remove([thread](ThreadControlBlock* t) { return t == thread; });
    blockedQueue.Remove([thread](ThreadControlBlock* t) { return t == thread; });

    // Remove from parent's thread list to prevent KillProcess from
    // iterating over a dangling pointer later.
    if (thread->parent) {
        thread->parent->threads.Remove([thread](ThreadControlBlock* t) { return t == thread; });
    }

    if (thread == currentThread) {
        // Defer cleanup: this thread is still running on its own kernel stack.
        // Freeing it now would corrupt the stack we are executing on.
        // Schedule() will drain pendingReclaims after switching away.
        currentThread = nullptr;
        pendingReclaims.PushBack(thread);
    } else {
        // Safe to clean up immediately — thread is not running.
        if (thread->stack) {
            kfree((void*)thread->stack);
            thread->stack = nullptr;
        }
        delete thread;
    }
}

bool Scheduler::ExitCurrentThread() {
    if (!currentThread) return false;

    ProcessControlBlock* parent = currentThread->parent;

    if (!parent) {
        // Kernel thread without parent process
        TerminateThread(currentThread);
        return false;
    }

    // Count non-terminated threads in the parent process
    int activeThreadCount = 0;
    int totalThreads = parent->threads.GetSize();
    for (int i = 0; i < totalThreads; i++) {
        ThreadControlBlock* thread = parent->threads.PopFront();
        if (thread->state != THREAD_STATE_TERMINATED) {
            activeThreadCount++;
        }
        parent->threads.PushBack(thread);
    }

    // Last thread standing -> kill the entire process
    if (activeThreadCount <= 1) {
        KDBG1("Thread TID %d is last in process PID %d - terminating process", currentThread->tid,
              parent->pid);
        KillProcess(parent->pid);
        return true;
    } else {
        KDBG1("Thread TID %d exiting, %d threads remain in process PID %d", currentThread->tid,
              activeThreadCount - 1, parent->pid);
        TerminateThread(currentThread);
        return false;
    }
}

void Scheduler::Sleep(uint32_t milliseconds) {
    InterruptGuard guard;
    if (!currentThread) return;
    currentThread->wakeTime = timerTicks + milliseconds;
    currentThread->state = THREAD_STATE_BLOCKED;
    // KDBG3("Sleep TID=%d ms=%d", currentThread->tid, milliseconds);
}

void Scheduler::WakeThread(ThreadControlBlock* thread) {
    InterruptGuard guard;
    if (!thread) return;
    if (thread->state != THREAD_STATE_BLOCKED) return;
    thread->state = THREAD_STATE_READY;
    thread->wakeTime = 0;
    blockedQueue.Remove([thread](ThreadControlBlock* t) { return t == thread; });
    readyQueue.PushBack(thread);
    // KDBG3("WakeThread TID=%d", thread->tid);
}

CPUState* Scheduler::Schedule(CPUState* context) {
    if (currentThread) {
        currentThread->context = context;
        if ((currentThread->state == THREAD_STATE_RUNNING) && currentThread != idleThread) {
            currentThread->state = THREAD_STATE_READY;
            readyQueue.PushBack(currentThread);
        } else if (currentThread->state == THREAD_STATE_BLOCKED) {
            blockedQueue.PushBack(currentThread);
        } else if (currentThread->state == THREAD_STATE_TERMINATED) {
            terminatedQueue.PushBack(currentThread);
        }
    }

    if (blockedQueue.GetSize() > 0) {
        int count = blockedQueue.GetSize();
        for (int i = 0; i < count; i++) {
            ThreadControlBlock* t = blockedQueue.PopFront();
            if (t->state == THREAD_STATE_BLOCKED && t->wakeTime <= timerTicks) {
                t->state = THREAD_STATE_READY;
                t->wakeTime = 0;
                readyQueue.PushBack(t);
            } else {
                blockedQueue.PushBack(t);
            }
        }
    }

    if (readyQueue.GetSize() == 0) {
        // No real work to do, Run the Idle Thread.
        currentThread = idleThread;
        currentThread->state = THREAD_STATE_RUNNING;
        g_tss.esp0 = (uint32_t)(idleThread->stack + KERNEL_STACK_SIZE);
        _pager->SwitchDirectory((_pager->KernelPageDirectory));
        DrainPendingReclaims();
        return currentThread->context;

    } else {
        // Normal Round Robin
        currentThread = readyQueue.PopFront();
    }
    currentThread->state = THREAD_STATE_RUNNING;

    // KDBG3("Switching to TID=%d, PID=%d, EIP=0x%x, ESP=0x%x", currentThread->tid,
    //        currentThread->pid, currentThread->context->eip, currentThread->context->esp);

    g_tss.esp0 = (uint32_t)(currentThread->stack + KERNEL_STACK_SIZE);

    if (currentThread->parent) {
        _pager->SwitchDirectory((currentThread->parent->page_directory));
    } else {
        _pager->SwitchDirectory((_pager->KernelPageDirectory));
    }

    // Now running on the new thread's stack — safe to reclaim deferred threads.
    DrainPendingReclaims();

    return currentThread->context;
}

void Scheduler::DrainPendingReclaims() {
    while (pendingReclaims.GetSize() > 0) {
        ThreadControlBlock* thread = pendingReclaims.PopFront();
        if (thread->stack) {
            kfree((void*)thread->stack);
            thread->stack = nullptr;
        }
        delete thread;
    }
}
