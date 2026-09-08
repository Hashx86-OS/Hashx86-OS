#define KDBG_COMPONENT "K.STACK"
#include <core/Iguard.h>
#include <core/kstack.h>
#include <kernel.h>

// --------------------------------------------------------------------------
// Kernel-stack zone allocator
//
// Fixed-size, page-aligned stack slots carved out of a dedicated reserved
// region in the identity-mapped low-memory space.  Every slot is preceded by
// an unmapped guard page (see kstack_zone_activate) so that a stack overflow
// faults immediately instead of silently corrupting the kernel heap or other
// kernel objects.  Kernel stacks NEVER come from the TLSF heap.
//
// IMPORTANT: This allocator must NOT depend on the kernel heap.  It is
// initialized before kheap_init() (so the heap carving skips the zone), so
// it uses a static bitmap rather than heap-backed data structures.
// --------------------------------------------------------------------------

// Minimum size the kernel-stack zone must retain after clamping so that it
// can hold a useful number of slots (16 MiB).
static const uint32_t KSTACK_ZONE_MIN_SIZE = 16 * 1024 * 1024;

static bool g_zoneReady = false;
static uint32_t g_zoneBase = 0;
static uint32_t g_zoneEnd = 0;
static uint32_t g_slotCount = 0;
static uint32_t g_inUse = 0;
static uint32_t g_slotBitmap[(KSTACK_MAX_SLOTS + 31) / 32];

static inline bool kstack_slot_in_use(uint32_t idx) {
    return (g_slotBitmap[idx / 32] >> (idx % 32)) & 1u;
}

static inline void kstack_slot_set(uint32_t idx) {
    g_slotBitmap[idx / 32] |= (1u << (idx % 32));
}

static inline void kstack_slot_clear(uint32_t idx) {
    g_slotBitmap[idx / 32] &= ~(1u << (idx % 32));
}

int kstack_init() {
    if (g_zoneReady) return 0;

    uint32_t base = KSTACK_ZONE_BASE;
    uint32_t size = KSTACK_ZONE_SIZE;

    // The zone must stay inside the identity-mapped low-memory range and
    // inside usable RAM.  If the fixed base does not fit, clamp to whatever
    // is available (shrinking to a whole number of slots).
    uint32_t avail_end = g_kmap.available.end_addr;
    uint32_t cap = (avail_end < 0x10000000) ? avail_end : 0x10000000;

    if ((uint64_t)base + size > cap) {
        if (cap <= base + KSTACK_ZONE_MIN_SIZE) {
            KDBG1(
                "kstack_init FAILED: not enough low memory for stack zone "
                "(avail_end=0x%x)",
                avail_end);
            return -1;
        }
        size = cap - base;
        size -= size % KSTACK_SLOT_SIZE;
        if (size < KSTACK_ZONE_MIN_SIZE) {
            KDBG1("kstack_init FAILED: zone too small after clamping (0x%x)", size);
            return -1;
        }
    }

    // Reserve the region from the PMM so the heap and user allocations skip it.
    pmm_deinit_region(base, size);

    g_zoneBase = base;
    g_zoneEnd = base + size;
    g_slotCount = size / KSTACK_SLOT_SIZE;
    g_inUse = 0;

    memset(g_slotBitmap, 0, sizeof(g_slotBitmap));
    g_zoneReady = true;

    KDBG1("Kernel stack zone 0x%x-0x%x slots=%u slot_size=0x%x", g_zoneBase, g_zoneEnd, g_slotCount,
          KSTACK_SLOT_SIZE);
    return 0;
}

void* kstack_alloc() {
    InterruptGuard guard;
    if (!g_zoneReady) return NULL;
    if (g_inUse >= g_slotCount) {
        KDBG1("kstack_alloc FAILED: zone exhausted (in_use=%u/%u)", g_inUse, g_slotCount);
        return NULL;
    }

    // Scan for the first free slot (linear is fine; slot count is small).
    uint32_t words = (g_slotCount + 31) / 32;
    for (uint32_t w = 0; w < words; w++) {
        if (g_slotBitmap[w] == 0xFFFFFFFF) continue;
        for (uint32_t b = 0; b < 32; b++) {
            uint32_t idx = w * 32 + b;
            if (idx >= g_slotCount) break;
            if (kstack_slot_in_use(idx)) continue;
            kstack_slot_set(idx);
            g_inUse++;
            uint32_t stack = g_zoneBase + idx * KSTACK_SLOT_SIZE + KSTACK_GUARD_PAGES * PAGE_SIZE;
            KDBG3("kstack_alloc idx=%u stack=0x%x in_use=%u", idx, stack, g_inUse);
            return (void*)stack;
        }
    }

    KDBG1("kstack_alloc FAILED: no free slot found (in_use=%u/%u)", g_inUse, g_slotCount);
    return NULL;
}

void kstack_free(void* stack) {
    if (!stack) return;
    InterruptGuard guard;
    if (!g_zoneReady) return;

    uint32_t addr = (uint32_t)stack;
    uint32_t guardOffset = KSTACK_GUARD_PAGES * PAGE_SIZE;

    if (addr < g_zoneBase + guardOffset || addr >= g_zoneEnd) {
        KDBG1("kstack_free: invalid stack 0x%x (zone 0x%x-0x%x)", addr, g_zoneBase, g_zoneEnd);
        return;
    }

    uint32_t off = addr - g_zoneBase - guardOffset;
    if (off % KSTACK_SLOT_SIZE != 0) {
        KDBG1("kstack_free: unaligned stack 0x%x", addr);
        return;
    }

    uint32_t idx = off / KSTACK_SLOT_SIZE;
    if (idx >= g_slotCount) {
        KDBG1("kstack_free: out of range idx=%u addr=0x%x", idx, addr);
        return;
    }

    // Guard against double-free: freeing an already-free slot would let two
    // threads share the same stack later.
    if (!kstack_slot_in_use(idx)) {
        KDBG1("kstack_free: double free idx=%u addr=0x%x", idx, addr);
        return;
    }

    kstack_slot_clear(idx);
    g_inUse--;
    KDBG3("kstack_free idx=%u addr=0x%x in_use=%u", idx, addr, g_inUse);
}

void kstack_zone_activate(uint32_t* kernel_page_directory) {
    if (!kernel_page_directory || !g_zoneReady) return;

    for (uint32_t s = 0; s < g_slotCount; s++) {
        uint32_t guard = g_zoneBase + s * KSTACK_SLOT_SIZE;
        uint32_t pd_idx = guard >> 22;
        uint32_t pt_idx = (guard >> 12) & 0x3FF;
        if (!(kernel_page_directory[pd_idx] & PAGE_PRESENT)) continue;
        uint32_t* pt = (uint32_t*)(kernel_page_directory[pd_idx] & 0xFFFFF000);
        pt[pt_idx] = 0;
        asm volatile("invlpg (%0)" ::"r"(guard) : "memory");
    }

    KDBG1("Kernel stack guard pages armed (slots=%u)", g_slotCount);
}

bool kstack_is_guard_page(uint32_t addr) {
    if (!g_zoneReady) return false;
    if (addr < g_zoneBase || addr >= g_zoneEnd) return false;
    uint32_t off = addr - g_zoneBase;
    uint32_t in_slot = off % KSTACK_SLOT_SIZE;
    return in_slot < KSTACK_GUARD_PAGES * PAGE_SIZE;
}

uint32_t kstack_get_zone_base() {
    return g_zoneBase;
}

uint32_t kstack_get_zone_end() {
    return g_zoneEnd;
}

uint32_t kstack_get_slot_size() {
    return KSTACK_SLOT_SIZE;
}

uint32_t kstack_get_in_use_count() {
    return g_inUse;
}
