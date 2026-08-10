#define KDBG_COMPONENT "K.HEAP"
#include <core/memory.h>
#include <core/tlsf/tlsf.h>

// TLSF control structure lives at the start of the heap region.
static tlsf_t g_tlsf = NULL;

// Heap boundaries (also used by PMM/paging for address-range checks)
void *g_kheap_start_addr = NULL, *g_kheap_end_addr = NULL;

int kheap_init(void* start_addr, void* end_addr) {
    // Do not publish heap boundaries until validation and probe succeed: every
    // failure path below leaves these globals cleared and g_tlsf NULL.
    g_kheap_start_addr = NULL;
    g_kheap_end_addr = NULL;

    if (start_addr > end_addr) {
        KDBG1("Init failed start=0x%x end=0x%x", start_addr, end_addr);
        return -1;
    }

    size_t pool_size = (uint8_t*)end_addr - (uint8_t*)start_addr;

    // Guard before subtracting: the subtraction below underflows for pool
    // sizes smaller than the TLSF control structure plus pool overhead.
    if (pool_size < tlsf_size() + tlsf_pool_overhead()) {
        KDBG1("kheap_init FAILED: pool too small (%u bytes) for TLSF control "
              "(need >= %u)",
              (unsigned int)pool_size,
              (unsigned int)(tlsf_size() + tlsf_pool_overhead()));
        return -1;
    }

    // TLSF needs its control structure at the start of the pool plus enough
    // remaining space for at least one minimum-sized allocatable block.
    // Mirror tlsf_add_pool's alignment logic so the pre-check is exact.
    size_t pool_bytes = (pool_size - tlsf_size() - tlsf_pool_overhead()) & ~(size_t)(tlsf_align_size() - 1);
    if (pool_bytes < tlsf_block_size_min()) {
        KDBG1("kheap_init FAILED: pool too small (%u bytes) for TLSF (need >= %u)",
              (unsigned int)pool_size,
              (unsigned int)(tlsf_size() + tlsf_pool_overhead() + tlsf_block_size_min()));
        return -1;
    }

    // tlsf_create_with_pool carves out the control structure from the
    // beginning and uses the rest as the allocatable pool.
    g_tlsf = tlsf_create_with_pool(start_addr, pool_size);
    if (!g_tlsf) {
        KDBG1("tlsf_create_with_pool failed");
        return -1;
    }

    // Confirm an allocatable pool was actually added rather than relying only
    // on the non-null control pointer: a minimum-size probe allocation must
    // succeed (and be freed again) for the heap to be usable.
    void* probe = tlsf_malloc(g_tlsf, tlsf_block_size_min());
    if (!probe) {
        KDBG1("kheap_init FAILED: no allocatable block in TLSF pool");
        g_tlsf = NULL;
        return -1;
    }
    tlsf_free(g_tlsf, probe);

    // All validation passed — only now publish the heap boundaries.
    g_kheap_start_addr = start_addr;
    g_kheap_end_addr = end_addr;

    KDBG1("Heap 0x%x-0x%x (%u MB), TLSF control=0x%x pool=0x%x",
          start_addr, end_addr,
          (unsigned int)(pool_size / (1024 * 1024)),
          start_addr,
          (uint8_t*)start_addr + tlsf_size());
    return 0;
}

void kheap_print_blocks() {
    InterruptGuard guard;
    if (!g_tlsf) return;
    KDBG3("--- kheap blocks (via tlsf_walk_pool) ---");
    pool_t pool = tlsf_get_pool(g_tlsf);
    tlsf_walk_pool(pool, NULL, NULL);
}

void* kmalloc(size_t size) {
    InterruptGuard guard;
    if (size == 0 || size > 0x7FFFFFFF || !g_tlsf) return NULL;
    void* ptr = tlsf_malloc(g_tlsf, size);
    if (!ptr) KDBG1("OOM size=%u", (unsigned int)size);
    return ptr;
}

// TLSF uses 32-bit internal size fields; reject requests that exceed the
// wrapper limit before calling into it, accounting for alignment headroom.
static bool tlsf_request_valid(size_t size, size_t extra) {
    if (size > 0x7FFFFFFF) return false;
    return extra <= 0x7FFFFFFF && size + extra <= 0x7FFFFFFF;
}

void* kbrk(size_t size) {
    return kmalloc(size);
}

void* aligned_kmalloc(size_t size, size_t alignment) {
    InterruptGuard guard;
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return NULL;
    if (!g_tlsf || !tlsf_request_valid(size, alignment)) return NULL;

    void* ptr = tlsf_memalign(g_tlsf, alignment, size);
    if (!ptr) KDBG1("AlignedOOM size=%u align=%u", (unsigned int)size, (unsigned int)alignment);
    return ptr;
}

void aligned_kfree(void* ptr) {
    kfree(ptr);
}

void* kcalloc(int n, int size) {
    InterruptGuard guard;
    if (n <= 0 || size <= 0 || !g_tlsf) return NULL;
    size_t total = (size_t)n * (size_t)size;
    if (total / (size_t)size != (size_t)n) return NULL;
    if (total > 0x7FFFFFFF) return NULL;

    void* mem = tlsf_malloc(g_tlsf, total);
    if (mem) memset(mem, 0, total);
    return mem;
}

void* krealloc(void* ptr, size_t size) {
    InterruptGuard guard;
    if (!g_tlsf || !tlsf_request_valid(size, 0)) {
        if (ptr) kfree(ptr);
        return NULL;
    }
    return tlsf_realloc(g_tlsf, ptr, size);
}

void kfree(void* addr) {
    InterruptGuard guard;
    if (!addr || !g_tlsf) return;
    tlsf_free(g_tlsf, addr);
}

// --- C++ Operators ---

void* operator new(size_t size) { return kmalloc(size); }
void* operator new[](size_t size) { return kmalloc(size); }
void* operator new(size_t size, std::align_val_t alignment) {
    return aligned_kmalloc(size, static_cast<size_t>(alignment));
}
void* operator new[](size_t size, std::align_val_t alignment) {
    return aligned_kmalloc(size, static_cast<size_t>(alignment));
}
void operator delete(void* ptr) noexcept { kfree(ptr); }
void operator delete[](void* ptr) noexcept { kfree(ptr); }
void operator delete(void* ptr, size_t size) noexcept { (void)size; kfree(ptr); }
void operator delete[](void* ptr, size_t size) noexcept { (void)size; kfree(ptr); }
void operator delete(void* ptr, std::align_val_t) noexcept { kfree(ptr); }
void operator delete[](void* ptr, std::align_val_t) noexcept { kfree(ptr); }

// --- Memory routines (extern "C" so kernel callers can link against them) ---

extern "C" void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

extern "C" void* memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

extern "C" int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        ++p1; ++p2;
    }
    return 0;
}
