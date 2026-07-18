#define KDBG_COMPONENT "K.HEAP"
#include <core/memory.h>
#include <core/tlsf/tlsf.h>

// TLSF control structure lives at the start of the heap region.
static tlsf_t g_tlsf = NULL;

// Heap boundaries (also used by PMM/paging for address-range checks)
void *g_kheap_start_addr = NULL, *g_kheap_end_addr = NULL;

int kheap_init(void* start_addr, void* end_addr) {
    if (start_addr > end_addr) {
        KDBG1("Init failed start=0x%x end=0x%x", start_addr, end_addr);
        return -1;
    }
    g_kheap_start_addr = start_addr;
    g_kheap_end_addr = end_addr;

    size_t pool_size = (uint8_t*)end_addr - (uint8_t*)start_addr;

    // TLSF needs its control structure at the start of the pool.
    // tlsf_create_with_pool carves out the control structure from the
    // beginning and uses the rest as the allocatable pool.
    g_tlsf = tlsf_create_with_pool(start_addr, pool_size);
    if (!g_tlsf) {
        KDBG1("tlsf_create_with_pool failed");
        return -1;
    }

    KDBG1("Heap 0x%x-0x%x (%u MB), TLSF control=0x%x pool=0x%x",
          start_addr, end_addr,
          (unsigned int)(pool_size / (1024 * 1024)),
          start_addr,
          (uint8_t*)start_addr + tlsf_size());
    return 0;
}

void kheap_print_blocks() {
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

void* kbrk(size_t size) {
    return kmalloc(size);
}

void* aligned_kmalloc(size_t size, size_t alignment) {
    InterruptGuard guard;
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return NULL;
    if (!g_tlsf) return NULL;

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
    if (!g_tlsf) {
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
