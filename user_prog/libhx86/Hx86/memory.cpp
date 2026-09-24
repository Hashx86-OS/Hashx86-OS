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

#include <Hx86/Hsyscalls/syscalls.h>
#include <Hx86/memory.h>

void* memcpy(void* destination, const void* source, size_t size) {
    uint32_t* dst32 = static_cast<uint32_t*>(destination);
    const uint32_t* src32 = static_cast<const uint32_t*>(source);

    size_t word_count = size / 4;
    for (size_t i = 0; i < word_count; i++) dst32[i] = src32[i];

    uint8_t* dst8 = reinterpret_cast<uint8_t*>(dst32) + word_count * 4;
    const uint8_t* src8 = reinterpret_cast<const uint8_t*>(src32) + word_count * 4;
    for (size_t i = 0; i < (size % 4); i++) dst8[i] = src8[i];

    return destination;
}

void* memset(void* ptr, int value, size_t size) {
    uint8_t* byte_ptr = static_cast<uint8_t*>(ptr);
    for (size_t i = 0; i < size; i++) byte_ptr[i] = static_cast<uint8_t>(value);

    return ptr;
}

int memcmp(const void* ptr1, const void* ptr2, size_t size) {
    const uint8_t* byte_ptr1 = static_cast<const uint8_t*>(ptr1);
    const uint8_t* byte_ptr2 = static_cast<const uint8_t*>(ptr2);

    for (size_t i = 0; i < size; i++)
        if (byte_ptr1[i] != byte_ptr2[i]) return byte_ptr1[i] - byte_ptr2[i];

    return 0;
}

// Start and end addresses of the heap region.
void *g_heap_start_addr = NULL, *g_heap_end_addr = NULL;
unsigned long g_total_size = 0;
unsigned long g_total_used_size = 0;
// Head of the allocated-block list.
heap_BLOCK* g_head = NULL;

// Global flag indicating whether the heap has been initialized.
static bool heap_initialized = false;

/** heap_init() - Initialize the heap and set the total memory size.
 * @start_addr: Start address of the heap region.
 * @end_addr: End address of the heap region.
 *
 * Return: 0 on success, -1 if the region is invalid.
 */
int heap_init(void* start_addr, void* end_addr) {
    if (start_addr > end_addr) {
        printf("failed to init heap\n");
        return -1;
    }
    g_heap_start_addr = start_addr;
    g_heap_end_addr = end_addr;
    g_total_size = (unsigned long)end_addr - (unsigned long)start_addr;
    g_total_used_size = 0;
    heap_initialized = true;
    return 0;
}

/** kbrk() - Grow the heap and return the address of the new bytes.
 * @size: Number of bytes to allocate.
 *
 * Requests more memory from the kernel in chunks (kernel limit of 256KB per
 * call) when the current heap cannot satisfy the request.
 *
 * Return: Address of the newly reserved memory, or NULL on failure.
 */
enum { MAX_BRK_PER_CALL = 64 * 4096 };  // 256KB per sys_brk call.

void* kbrk(int size) {
    void* addr = NULL;
    if (size <= 0) return NULL;

    // Check whether enough memory is available.
    if ((long)(g_total_size - g_total_used_size) <= size) {
        // Request more memory from the kernel in chunks (kernel limit: 256KB per call).
        int needed = size - (g_total_size - g_total_used_size);
        int remaining = (needed + 4095) & ~4095;               // Page-align the request.
        if (remaining < 1024 * 1024) remaining = 1024 * 1024;  // Request at least 1MB.

        while (remaining > 0) {
            int chunk = remaining > MAX_BRK_PER_CALL ? MAX_BRK_PER_CALL : remaining;
            int32_t res = syscall_brk(chunk);
            if (res == -1) return NULL;
            g_total_size += chunk;
            g_heap_end_addr = (void*)((unsigned long)g_heap_end_addr + chunk);
            remaining -= chunk;
        }
    }

    // Add the start address of the total previously used memory.
    addr = (void*)((unsigned long)g_heap_start_addr + g_total_used_size);
    g_total_used_size += size;
    return addr;
}

/** heap_print_blocks() - Print the list of allocated heap blocks. */
void heap_print_blocks() {
    heap_BLOCK* temp = g_head;
    printf("Block Size: %d\n", sizeof(heap_BLOCK));
    while (temp != NULL) {
        printf("size:%d, free:%d, data: 0x%x, curr: 0x%x, next: 0x%x\n", temp->metadata.size,
               temp->metadata.is_free, temp->data, temp, temp->next);
        temp = temp->next;
    }
}

bool is_block_free(heap_BLOCK* block) {
    if (!block) return false;
    return (block->metadata.is_free == true);
}

/** worst_fit() - Find the first free block large enough for the request.
 * @size: Required block size.
 *
 * Return: Pointer to a usable free block, or NULL if none fits.
 */
heap_BLOCK* worst_fit(int size) {
    heap_BLOCK* temp = g_head;
    while (temp != NULL) {
        if (is_block_free(temp)) {
            if ((int)temp->metadata.size >= size) return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

// Allocate a new heap block.
heap_BLOCK* allocate_new_block(int size) {
    heap_BLOCK* temp = g_head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    heap_BLOCK* new_block = (heap_BLOCK*)kbrk(sizeof(heap_BLOCK));
    if (!new_block) return NULL;

    new_block->metadata.is_free = false;
    new_block->metadata.size = size;
    new_block->data = kbrk(size);
    if (!new_block->data) return NULL;

    new_block->next = NULL;
    temp->next = new_block;
    return new_block;
}

/** kmalloc() - Allocate a block of the requested size.
 * @size: Number of bytes to allocate.
 *
 * Allocates a new head block when the list is empty; otherwise looks for a
 * free block using the worst-fit strategy and falls back to appending a new
 * block when none fits.
 *
 * Return: Pointer to the allocated data, or NULL on failure.
 */
void* kmalloc(int size) {
    if (size <= 0) return NULL;
    if (g_head == NULL) {
        g_head = (heap_BLOCK*)kbrk(sizeof(heap_BLOCK));
        if (!g_head) return NULL;

        g_head->metadata.is_free = false;
        g_head->metadata.size = size;
        g_head->next = NULL;
        g_head->data = kbrk(size);
        if (!g_head->data) return NULL;

        return g_head->data;
    } else {
        heap_BLOCK* worst = worst_fit(size);
        if (worst == NULL) {
            heap_BLOCK* new_block = allocate_new_block(size);
            if (!new_block) return NULL;

            return new_block->data;
        } else {
            worst->metadata.is_free = false;
            return worst->data;
        }
    }
    return NULL;
}

void* aligned_kmalloc(size_t size, size_t alignment) {
    uintptr_t raw_addr = (uintptr_t)kmalloc(size + alignment);
    if (!raw_addr) return nullptr;

    uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
    return (void*)aligned_addr;
}

/** kcalloc() - Allocate memory for n items of size and zero it out.
 * @n: Number of items.
 * @size: Size of each item.
 *
 * Return: Pointer to the zeroed allocation, or NULL on failure.
 */
void* kcalloc(int n, int size) {
    if (n < 0 || size < 0) return NULL;
    void* mem = kmalloc(n * size);
    if (mem) memset(mem, 0, n * size);
    return mem;
}

/** krealloc() - Allocate a new block, copying the old data and freeing it.
 * @ptr: Pointer to the previous allocation, or NULL.
 * @size: New block size.
 *
 * Return: Pointer to the resized allocation, or NULL on failure.
 */
void* krealloc(void* ptr, int size) {
    if (!ptr) return kmalloc(size);
    if (size <= 0) {
        kfree(ptr);
        return NULL;
    }

    heap_BLOCK* temp = g_head;
    while (temp != NULL) {
        if (temp->data == ptr) {
            void* new_ptr = kmalloc(size);
            if (!new_ptr) return NULL;

            memcpy(new_ptr, ptr, temp->metadata.size < size ? temp->metadata.size : size);
            temp->metadata.is_free = true;
            return new_ptr;
        }
        temp = temp->next;
    }
    return NULL;
}

/** kfree() - Mark the block backing the given address as free.
 * @addr: Address previously returned by a heap allocation.
 */
void kfree(void* addr) {
    if (!addr) return;

    heap_BLOCK* temp = g_head;
    while (temp != NULL) {
        if (temp->data == addr) {
            temp->metadata.is_free = true;
            return;
        }
        temp = temp->next;
    }
}

void* operator new(size_t size) {
    return kmalloc(size);
}

void* operator new[](size_t size) {
    return kmalloc(size);
}

// Aligned new operator.
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

// C++14-compliant sized delete operators.
void operator delete(void* ptr, size_t size) noexcept {
    kfree(ptr);
}

void operator delete[](void* ptr, size_t size) noexcept {
    kfree(ptr);
}

// Aligned delete operator.
void operator delete(void* ptr, std::align_val_t) noexcept {
    kfree(ptr);
}

void operator delete[](void* ptr, std::align_val_t) noexcept {
    kfree(ptr);
}
