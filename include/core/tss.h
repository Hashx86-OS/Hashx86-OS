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

#ifndef TSS_H
#define TSS_H

#include <types.h>

/**
 * struct TaskStateSegment - An x86 task state segment.
 * @prev_tss: Link to the previous TSS.
 * @esp0: Ring-0 stack pointer, loaded on a privilege transfer.
 * @ss0: Ring-0 stack segment.
 * @esp1: Ring-1 stack pointer (unused).
 * @ss1: Ring-1 stack segment (unused).
 * @esp2: Ring-2 stack pointer (unused).
 * @ss2: Ring-2 stack segment (unused).
 * @cr3: Page directory base of the task.
 * @eip: Instruction pointer of the saved task.
 * @eflags: Saved EFLAGS register.
 * @eax: Saved EAX register.
 * @ecx: Saved ECX register.
 * @edx: Saved EDX register.
 * @ebx: Saved EBX register.
 * @esp: Saved stack pointer.
 * @ebp: Saved frame pointer.
 * @esi: Saved ESI register.
 * @edi: Saved EDI register.
 * @es: Saved ES segment.
 * @cs: Saved CS segment.
 * @ss: Saved SS segment.
 * @ds: Saved DS segment.
 * @fs: Saved FS segment.
 * @gs: Saved GS segment.
 * @ldt: Local descriptor table selector.
 * @trap: Trap flag.
 * @iomap_base: Offset of the I/O permission bitmap, or end-of-TSS.
 */
struct TaskStateSegment {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

#endif
