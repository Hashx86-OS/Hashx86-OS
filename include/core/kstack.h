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
// identity-mapped low-memory region (< 256MB).  Each slot is a fixed-size
// stack (KERNEL_STACK_SIZE) preceded by an UNMAPPED guard page, so a
// downward stack overflow raises a page fault instead of silently
// corrupting heap metadata or adjacent objects.
// --------------------------------------------------------------------------

// Stack size for every kernel thread (kept identical to the historical value).
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
 * Reserve the kernel-stack zone from the PMM and initialize the free list.
 * Must be called after pmm_init() and BEFORE any kernel heap allocation,
 * so the heap allocator skips this region.
 *
 * @return 0 on success, -1 on failure.
 */
int kstack_init();

/**
 * Allocate a kernel stack slot.  Returns the base of the data pages
 * (i.e. the address to use as the thread's kernel stack), or NULL if the
 * zone is exhausted.
 */
void* kstack_alloc();

/**
 * Return a previously allocated kernel stack slot to the free list.
 * The pointer must be exactly the value returned by kstack_alloc().
 */
void kstack_free(void* stack);

/**
 * Once paging is active, walk the shared kernel page tables and clear the
 * present bit on every slot's guard page so any access faults cleanly.
 * Safe to call once after Paging::Activate().
 */
void kstack_zone_activate(uint32_t* kernel_page_directory);

/**
 * Returns true if addr lies within the (unmapped) guard page of any kernel
 * stack slot in the zone.  Used by the page-fault handler to report
 * kernel stack overflows.
 */
bool kstack_is_guard_page(uint32_t addr);

// Exposed for diagnostics / fault reporting.
uint32_t kstack_get_zone_base();
uint32_t kstack_get_zone_end();
uint32_t kstack_get_slot_size();
uint32_t kstack_get_in_use_count();

#endif  // KSTACK_H
