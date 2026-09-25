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

#ifndef KSTACK_H
#define KSTACK_H

#include <core/paging.h>
#include <core/pmm.h>
#include <debug.h>
#include <types.h>

// --------------------------------------------------------------------------
// Dedicated kernel-stack zone
//
// The zone is a reserved, contiguous, page-aligned range in the
// identity-mapped low-memory region (< 256MB). Each slot is a fixed-size
// stack (KERNEL_STACK_SIZE) preceded by an unmapped guard page, so a
// downward stack overflow raises a page fault instead of silently corrupting
// heap metadata or adjacent objects.
// --------------------------------------------------------------------------

// Stack size for every kernel thread (kept identical to the historical size).
#define KERNEL_STACK_SIZE (64 * 1024)
#define KERNEL_STACK_PAGES (KERNEL_STACK_SIZE / PAGE_SIZE)

// One unmapped guard page below each stack, catching overflow.
#define KSTACK_GUARD_PAGES 1

// Total footprint of one slot: guard page(s) + data pages.
#define KSTACK_SLOT_PAGES (KERNEL_STACK_PAGES + KSTACK_GUARD_PAGES)
#define KSTACK_SLOT_SIZE (KSTACK_SLOT_PAGES * PAGE_SIZE)

// The zone sits at the top of the identity-mapped range (224MB - 256MB),
// far away from the kernel heap which is carved from low memory.
#define KSTACK_ZONE_BASE 0x0E000000
#define KSTACK_ZONE_SIZE (32 * 1024 * 1024)
#define KSTACK_MAX_SLOTS (KSTACK_ZONE_SIZE / KSTACK_SLOT_SIZE)

/**
 * kstack_init() - Reserve the kernel-stack zone and initialize the free list.
 *
 * Must be called after pmm_init() and before any kernel heap allocation, so
 * the heap allocator skips this region.
 *
 * Return: 0 on success, -1 on failure.
 */
int kstack_init();

/**
 * kstack_alloc() - Allocate one kernel stack slot.
 *
 * Returns the base of the data pages (i.e. the address to use as the thread's
 * kernel stack).
 *
 * Return: The stack base address, or NULL if the zone is exhausted.
 */
void* kstack_alloc();

/**
 * kstack_free() - Return a kernel stack slot to the zone's free list.
 * @stack: Exactly the value previously returned by kstack_alloc().
 */
void kstack_free(void* stack);

/**
 * kstack_zone_activate() - Unmap every slot's guard page after paging starts.
 * @kernel_page_directory: The active kernel page directory.
 *
 * Walks the shared kernel page tables and clears the present bit on each
 * slot's guard page so any access faults cleanly. Safe to call once after
 * Paging::Activate().
 */
void kstack_zone_activate(uint32_t* kernel_page_directory);

/**
 * kstack_is_guard_page() - Test whether an address is a stack guard page.
 * @addr: Address to test.
 *
 * Used by the page-fault handler to report kernel stack overflows.
 *
 * Return: True if the address lies within the unmapped guard page of any
 *         kernel stack slot in the zone.
 */
bool kstack_is_guard_page(uint32_t addr);

/* Diagnostic accessors exposed for the kernel crash reporter. */
uint32_t kstack_get_zone_base();
uint32_t kstack_get_zone_end();
uint32_t kstack_get_slot_size();
uint32_t kstack_get_in_use_count();

#endif  // KSTACK_H
