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

#ifndef PROCESS_TYPES_H
#define PROCESS_TYPES_H

#include <core/memory.h>
#include <types.h>
#include <utils/linkedList.h>

/**
 * enum AppBinaryType - Classification of a loaded application.
 * @APP_BINARY_UNKNOWN: Unclassified binary.
 * @APP_BINARY_GUI: GUI application.
 * @APP_BINARY_CLI: Command-line application.
 */
enum AppBinaryType {
    APP_BINARY_UNKNOWN = 0,
    APP_BINARY_GUI = 1,
    APP_BINARY_CLI = 2,
};

/**
 * enum ThreadState - Lifecycle state of a thread.
 * @THREAD_STATE_NEW: Created but not yet scheduled.
 * @THREAD_STATE_READY: Runnable and waiting for the CPU.
 * @THREAD_STATE_RUNNING: Currently executing.
 * @THREAD_STATE_BLOCKED: Sleeping or blocked.
 * @THREAD_STATE_TERMINATED: Exited, awaiting reclamation.
 */
enum ThreadState {
    THREAD_STATE_NEW,
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_TERMINATED
};

/**
 * struct CPUState - Saved register set of a thread (fixed for pusha/iret).
 *
 * The layout matches the register order produced by pusha and consumed by
 * iret on context switch.
 */
struct CPUState {
    // General purpose registers.
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;

    // Segment registers.
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;

    // Interrupt information.
    uint32_t error;

    // Return state.
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
} __attribute__((packed));

class Process;            // Forward declaration.
class File;               // Forward declaration.
struct ProgramArguments;  // Forward declaration; freed via KillProcess when set on a PCB.

struct ProcessControlBlock;  // Forward declaration.

constexpr uint32_t FD_MIN = 3;
constexpr uint32_t FD_MAX = 128;  // Upper bound for the process file-descriptor table.

/**
 * struct HeapSegment - Address range of a process heap.
 * @startAddress: First address of the heap.
 * @endAddress: Current heap break.
 * @maxAddress: Maximum allowed heap break.
 */
struct HeapSegment {
    uint32_t startAddress;
    uint32_t endAddress;
    uint32_t maxAddress;
};

/**
 * struct ThreadControlBlock - Scheduler state of a single thread.
 * @tid: Thread identifier.
 * @pid: Identifier of the owning process.
 * @state: Current thread state.
 * @stack: Base of the thread's kernel stack.
 * @context: Saved CPU state used on context switch.
 * @parent: Process control block that owns this thread.
 * @wakeTime: Timer tick at which a blocked thread should wake.
 * @stackSlotIdx: Slot index for user-stack virtual address reuse.
 */
struct ThreadControlBlock {
    uint32_t tid;
    uint32_t pid;
    ThreadState state;

    uint8_t* stack;
    CPUState* context;
    ProcessControlBlock* parent;
    uint32_t wakeTime;
    uint32_t stackSlotIdx;  // Slot index for user-stack virtual address reuse.
};

/**
 * struct ProcessControlBlock - Scheduler state of a process.
 * @pid: Process identifier.
 * @page_directory: Page directory for the process address space.
 * @threads: List of threads owned by the process.
 * @parent: Parent process control block.
 * @isKernelProcess: Whether this process runs in kernel mode.
 * @appType: Application binary type (see AppBinaryType).
 * @heap: Heap segment of the process.
 * @fdTable: Open file descriptor table.
 * @programArgs: Program arguments; freed in KillProcess.
 * @cwd: Current working directory of the process.
 * @stdinQueue: PTY-style stdin ring buffer, routed per process.
 * @stdinHead: Head index of the stdin queue.
 * @stdinTail: Tail index of the stdin queue.
 * @stdinAttached: Whether a stdin source is attached.
 * @isCliHost: Whether this process hosts a CLI.
 * @cliHostViewId: View identifier of the CLI host.
 * @cliHostPid: Process identifier of the CLI host.
 * @cliAttachedViewId: View identifier attached to the CLI host.
 * @deferredStackSlots: Deferred user-stack slot indices.
 * @deferredSlotCount: Number of pending deferred stack slots.
 */
struct ProcessControlBlock {
    static const uint16_t STDIN_QUEUE_SIZE = 256;

    uint32_t pid;

    uint32_t* page_directory;

    LinkedList<ThreadControlBlock*> threads;
    ProcessControlBlock* parent;
    bool isKernelProcess;
    uint16_t appType;
    HeapSegment heap;
    File* fdTable[FD_MAX];
    ProgramArguments* programArgs;  // Owned by the PCB, freed in KillProcess.
    char cwd[256];                  // Current working directory of the process.

    // PTY-style stdin queue: input is routed per process instead of globally.
    char stdinQueue[STDIN_QUEUE_SIZE];
    uint16_t stdinHead;
    uint16_t stdinTail;
    bool stdinAttached;

    // CLI host attachment metadata.
    bool isCliHost;
    uint32_t cliHostViewId;
    uint32_t cliHostPid;
    uint32_t cliAttachedViewId;

    // Deferred user-stack slot indices collected during TerminateThread,
    // recycled in KillProcess after the page-table sweep.
    uint32_t deferredStackSlots[256];
    int deferredSlotCount;
};

#endif  // PROCESS_TYPES_H
