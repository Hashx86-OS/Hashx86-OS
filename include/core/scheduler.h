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

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <core/Iguard.h>
#include <core/gdt.h>
#include <core/globals.h>
#include <core/memory.h>
#include <core/paging.h>
#include <core/process_types.h>
#include <core/tss.h>

/**
 * class Scheduler - Manage processes and threads on the kernel.
 *
 * Keeps the process list and the ready/blocked/terminated queue sets, hands
 * out PID/TID values, and performs the interrupt-driven context switch.
 */
class Scheduler {
private:
    LinkedList<ProcessControlBlock*> globalProcessList;
    // State queues.
    LinkedList<ThreadControlBlock*> readyQueue;       // Runnable threads.
    LinkedList<ThreadControlBlock*> blockedQueue;     // Sleeping threads.
    LinkedList<ThreadControlBlock*> terminatedQueue;  // Dead threads.
    LinkedList<ThreadControlBlock*>
        pendingReclaims;  // Threads whose stack must be freed after a context switch.

    uint32_t _pidCounter;
    uint32_t _tidCounter;
    Paging* _pager;
    uint32_t _trampolinePhys;  // Physical page holding the user-mode exit trampoline code.

public:
    static Scheduler* activeInstance;
    ThreadControlBlock* currentThread;
    ThreadControlBlock* idleThread;

    /**
     * Scheduler() - Initialize the scheduler and create the idle thread.
     * @pager: Paging instance used for user-stack allocation.
     *
     * Installs a user-mode exit trampoline and spawns the idle thread.
     */
    Scheduler(Paging* pager);

    // Creation and management.
    /**
     * CreateProcess() - Create a new process that runs a kernel entry function.
     * @isKernel: Whether the process is a kernel process.
     * @entrypoint: Entry function of the process.
     * @arg: Argument passed to the entry function.
     *
     * Return: The new process control block, or NULL on failure.
     */
    ProcessControlBlock* CreateProcess(bool isKernel, void (*entrypoint)(void*), void* arg);

    /**
     * CreateThread() - Create a new thread inside a process.
     * @parent: Process that owns the new thread.
     * @entrypoint: Entry function of the thread.
     * @arg: Argument passed to the entry function.
     *
     * Return: The new thread control block, or NULL on failure.
     */
    ThreadControlBlock* CreateThread(ProcessControlBlock* parent, void (*entrypoint)(void*),
                                     void* arg);

    /**
     * CloneCurrentThread() - Duplicate the calling thread within the same process.
     * @parentContext: Saved CPU state of the calling thread.
     * @clone_flags: Clone flags from the caller.
     * @child_stack: User stack address for the child, or NULL.
     * @parent_tid: User address for the parent's TID, or NULL.
     * @tls: TLS descriptor for the child thread, or NULL.
     * @child_tid: User address for the child's TID, or NULL.
     *
     * Return: The new thread control block, or NULL on failure.
     */
    ThreadControlBlock* CloneCurrentThread(CPUState* parentContext, uint32_t clone_flags,
                                           void* child_stack, void* parent_tid, void* tls,
                                           void* child_tid);

    /**
     * CloneCurrentProcess() - Fork the calling process.
     * @parentContext: Saved CPU state of the calling thread.
     * @clone_flags: Clone flags from the caller.
     * @child_stack: User stack address for the child, or NULL.
     * @parent_tid: User address for the parent's TID, or NULL.
     * @tls: TLS descriptor for the child thread, or NULL.
     * @child_tid: User address for the child's TID, or NULL.
     *
     * Return: The new thread control block, or NULL on failure.
     */
    ThreadControlBlock* CloneCurrentProcess(CPUState* parentContext, uint32_t clone_flags,
                                            void* child_stack, void* parent_tid, void* tls,
                                            void* child_tid);

    /**
     * KillProcess() - Terminate a process and all of its threads.
     * @pid: Process identifier to kill.
     *
     * Return: True on success, false when the process is not found.
     */
    bool KillProcess(uint32_t pid);

    /**
     * TerminateThread() - Mark a thread as terminated and queue it for reclamation.
     * @thread: The thread to terminate.
     */
    void TerminateThread(ThreadControlBlock* thread);

    /**
     * ExitCurrentThread() - Exit the currently running thread.
     *
     * Return: True when the whole process was terminated by the exit.
     */
    bool ExitCurrentThread();

    /**
     * Sleep() - Block the current thread for a number of milliseconds.
     * @milliseconds: How long to sleep.
     */
    void Sleep(uint32_t milliseconds);

    /**
     * WakeThread() - Move a blocked thread back to the ready queue.
     * @thread: The thread to wake.
     */
    void WakeThread(ThreadControlBlock* thread);

    // Core scheduling, called by the interrupt handler.
    /**
     * Schedule() - Pick the next runnable thread and switch to it.
     * @context: Saved CPU state of the outgoing thread.
     *
     * Return: The CPU state to restore for the incoming thread.
     */
    CPUState* Schedule(CPUState* context);

    // Reclaims kernel stacks and TCBs for threads terminated while running
    // (deferred to avoid freeing the current stack).
    void DrainPendingReclaims();

    // Helpers.
    ThreadControlBlock* GetCurrentThread() {
        return currentThread;
    }
    ProcessControlBlock* GetCurrentProcess() {
        return currentThread ? currentThread->parent : nullptr;
    }
    ProcessControlBlock* FindProcess(uint32_t pid);
    Paging* GetPager() {
        return _pager;
    }
};

#endif  // SCHEDULER_H
