/**
 * @file        memory.cpp
 * @brief       Memory Manager Implementation
 *
 * @date        29/01/2026
 * @version     1.0.0-beta
 */

#define KDBG_COMPONENT "K.HEAP"
#include <core/memory.h>

// start & end addresses pointing to memory
void *g_kheap_start_addr = NULL, *g_kheap_end_addr = NULL;
unsigned long g_total_size = 0;
unsigned long g_total_used_size = 0;
// list head
KHEAP_BLOCK* g_head = NULL;

// Global flag to indicate if kheap is initialized
static bool kheap_initialized = false;

/**
 * initialize heap and set total memory size
 */
int kheap_init(void* start_addr, void* end_addr) {
    if (start_addr > end_addr) {
        KDBG1("Init failed start=0x%x end=0x%x", start_addr, end_addr);
        return -1;
    }

    g_kheap_start_addr = start_addr;
    g_kheap_end_addr = end_addr;
    g_total_size = (unsigned long)end_addr - (unsigned long)start_addr;
    g_total_used_size = 0;
    kheap_initialized = true;
    return 0;
}

/**
 * increase the heap memory by size & get its address
 */
void* kbrk(size_t size) {
    if (size == 0) return NULL;
    // Round size up to max_align_t (16 bytes) to satisfy default operator new alignment
    size_t aligned = (size + 15) & ~(size_t)15;
    // check memory is available or not
    size_t available = g_total_size - g_total_used_size;
    if (available < aligned) {
        KDBG1("HeapExhausted req=%u avail=%u", (uint32_t)aligned, (uint32_t)available);
        return NULL;
    }
    // add start addr with total previously used memory
    void* addr = (void*)((unsigned long)g_kheap_start_addr + g_total_used_size);
    g_total_used_size += aligned;
    return addr;
}

/**
 * print list of allocated blocks
 */
void kheap_print_blocks() {
    KHEAP_BLOCK* temp = g_head;
    KDBG3("PrintBlocks size=%d", sizeof(KHEAP_BLOCK));
    while (temp != NULL) {
        KDBG3("Block size=%d free=%d data=0x%x curr=0x%x next=0x%x", temp->metadata.size,
              temp->metadata.is_free, temp->data, temp, temp->next);
        temp = temp->next;
    }
}

bool is_block_free(KHEAP_BLOCK* block) {
    if (!block) return false;
    return (block->metadata.is_free == true);
}

/**
 * this just check freed memory is greater than the required one
 */
KHEAP_BLOCK* worst_fit(size_t size) {
    KHEAP_BLOCK* temp = g_head;
    while (temp != NULL) {
        if (is_block_free(temp)) {
            if ((size_t)temp->metadata.size >= size) return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

// allocate a new heap block
KHEAP_BLOCK* allocate_new_block(size_t size) {
    if (!g_head) return NULL;
    KHEAP_BLOCK* temp = g_head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    KHEAP_BLOCK* new_block = (KHEAP_BLOCK*)kbrk(sizeof(KHEAP_BLOCK));
    if (!new_block) return NULL;

    new_block->metadata.is_free = false;
    new_block->metadata.size = size;
    new_block->data = kbrk(size);
    if (!new_block->data) {
        // Undo the metadata allocation so the heap break is consistent.
        g_total_used_size -= sizeof(KHEAP_BLOCK);
        return NULL;
    }

    new_block->next = NULL;
    temp->next = new_block;
    return new_block;
}

/**
 * allocate given size if list is null
 * otherwise try some memory allocation algorithm like best fit etc
 * to find best block to allocate
 */
void* kmalloc(size_t size) {
    InterruptGuard guard;
    if (size == 0 || size > 0x7FFFFFFF) {
        KDBG2("AllocInvalid invalid_size=%u", (uint32_t)size);
        return NULL;
    }
    if (g_head == NULL) {
        // Allocate metadata block and data block separately so that failure
        // of the data allocation does not leave a half-initialized g_head.
        KHEAP_BLOCK* temp_head = (KHEAP_BLOCK*)kbrk(sizeof(KHEAP_BLOCK));
        if (!temp_head) {
            KDBG1("AllocFail reason=InitHeapMetadataOM");
            return NULL;
        }

        temp_head->metadata.is_free = false;
        temp_head->metadata.size = size;
        temp_head->next = NULL;
        temp_head->data = kbrk(size);
        if (!temp_head->data) {
            // Data allocation failed — do not expose temp_head.
            // Reset used_size so the kbrk for metadata is effectively reclaimed.
            // (kbrk only tracks g_total_used_size; we undo the metadata allocation.)
            g_total_used_size -= sizeof(KHEAP_BLOCK);
            return NULL;
        }
        g_head = temp_head;
        return g_head->data;
    } else {
        KHEAP_BLOCK* worst = worst_fit(size);
        if (worst == NULL) {
            KHEAP_BLOCK* new_block = allocate_new_block(size);
            if (!new_block) {
                KDBG1("AllocFail size=%d reason=NoBlock/OOM", size);
                return NULL;
            }

            // allocate_new_block already initializes metadata and data
            return new_block->data;
        } else {
            worst->metadata.is_free = false;
            worst->metadata.size = size;
            // if (size != 8) KDBG3("Alloc size=%d ptr=0x%x", size, worst->data);
            return worst->data;
        }
    }
    return NULL;
}

void* aligned_kmalloc(size_t size, size_t alignment) {
    InterruptGuard guard;
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return nullptr;
    static_assert(sizeof(uintptr_t) == sizeof(void*), "uintptr_t must match pointer size");
    if (size > (size_t)-1 - alignment - sizeof(void*)) return nullptr;
    uintptr_t raw_addr = (uintptr_t)kmalloc(size + alignment + sizeof(void*));
    if (!raw_addr) return nullptr;

    uintptr_t aligned_addr = (raw_addr + sizeof(void*) + alignment - 1) & ~(alignment - 1);
    ((void**)aligned_addr)[-1] = (void*)raw_addr;
    KDBG3("AlignedAlloc size=%d align=%d raw=0x%x addr=0x%x", size, alignment, raw_addr,
          aligned_addr);
    return (void*)aligned_addr;
}

void aligned_kfree(void* ptr) {
    if (!ptr) return;
    void* raw = ((void**)ptr)[-1];
    kfree(raw);
}

/**
 * allocate memory n * size & zeroing out
 */
void* kcalloc(int n, int size) {
    InterruptGuard guard;
    if (n <= 0 || size <= 0) return NULL;
    // Compute total bytes in a wider type to detect overflow
    size_t total = (size_t)n * (size_t)size;
    if (total / (size_t)size != (size_t)n) {
        KDBG1("Calloc overflow n=%d size=%d", n, size);
        return NULL;
    }
    if (total > 0x7FFFFFFF) {
        KDBG1("Calloc overflow n=%d size=%d total=%u", n, size, (uint32_t)total);
        return NULL;
    }
    void* mem = kmalloc(total);
    if (mem) memset(mem, 0, total);
    KDBG3("Calloc n=%d size=%d addr=0x%x", n, size, mem);
    return mem;
}

/**
 * allocate a new block of memory
 * copy previous block data & set free the previous block
 */
void* krealloc(void* ptr, size_t size) {
    InterruptGuard guard;
    if (!ptr) return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    KHEAP_BLOCK* temp = g_head;
    while (temp != NULL) {
        if (temp->data == ptr) {
            void* new_ptr = kmalloc(size);
            if (!new_ptr) return NULL;

            // Uses the optimized memcpy automatically
            memcpy(new_ptr, ptr, temp->metadata.size < size ? temp->metadata.size : size);
            temp->metadata.is_free = true;
            // if (size != 8) KDBG3("Realloc ptr=0x%x new_ptr=0x%x size=%d", ptr, new_ptr, size);
            return new_ptr;
        }
        temp = temp->next;
    }
    return NULL;
}

/**
 * set free the block
 */
void kfree(void* addr) {
    InterruptGuard guard;
    if (!addr) {
        KDBG2("FreeInvalid ptr=NULL");
        return;
    }

    KHEAP_BLOCK* temp = g_head;
    while (temp != NULL) {
        if (temp->data == addr) {
            temp->metadata.is_free = true;
            // if (temp->metadata.size != 8) KDBG3("Free ptr=0x%x", addr);
            return;
        }
        temp = temp->next;
    }
    KDBG1("FreeError ptr=0x%x reason=NotFound", addr);
}

// --- C++ OPERATORS ---

void* operator new(size_t size) {
    return kmalloc(size);
}

void* operator new[](size_t size) {
    return kmalloc(size);
}

void* operator new(size_t size, std::align_val_t alignment) {
    return aligned_kmalloc(size, static_cast<size_t>(alignment));
}

void* operator new[](size_t size, std::align_val_t alignment) {
    return aligned_kmalloc(size, static_cast<size_t>(alignment));
}

void operator delete(void* ptr) noexcept {
    kfree(ptr);
}

void operator delete[](void* ptr) noexcept {
    kfree(ptr);
}

void operator delete(void* ptr, size_t size) noexcept {
    kfree(ptr);
}

void operator delete[](void* ptr, size_t size) noexcept {
    kfree(ptr);
}

void operator delete(void* ptr, std::align_val_t) noexcept {
    aligned_kfree(ptr);
}

void operator delete[](void* ptr, std::align_val_t) noexcept {
    aligned_kfree(ptr);
}

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
