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

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <Hx86/debug.h>
#include <Hx86/types.h>
#include <stddef.h>

// Memory manipulation functions.
void* memcpy(void* destination, const void* source, size_t size);
void* memset(void* ptr, int value, size_t size);
int memcmp(const void* ptr1, const void* ptr2, size_t size);

/** struct _heap_block - Singly linked list node backing a heap allocation. */
typedef struct _heap_block {
    struct {
        uint32_t size;    // Size of the allocated data region.
        uint8_t is_free;  // 1 if the block is free, 0 otherwise.
    } metadata;
    struct _heap_block* next;  // Pointer to the next block in the list.
    void* data;                // Pointer to the allocated data region.
} __attribute__((packed)) heap_BLOCK;

/** heap_init() - Initialize the heap and set the total memory size.
 * @start_addr: Start address of the heap region.
 * @end_addr: End address of the heap region.
 *
 * Return: 0 on success, -1 if the region is invalid.
 */
int heap_init(void* start_addr, void* end_addr);

/** kbrk() - Grow the heap and return the address of the new bytes.
 * @size: Number of bytes to allocate.
 *
 * Return: Address of the newly reserved memory, or NULL on failure.
 */
void* kbrk(int size);

/** heap_print_blocks() - Print the list of allocated heap blocks. */
void heap_print_blocks();

/** kmalloc() - Allocate a block of the requested size.
 * @size: Number of bytes to allocate.
 *
 * Allocates from the first free block that fits, appending a new block when
 * the list is empty or no block fits. Internal/external fragmentation
 * handling is still pending.
 *
 * Return: Pointer to the allocated data, or NULL on failure.
 */
void* kmalloc(int size);

/** kcalloc() - Allocate memory for n items of size and zero it out.
 * @n: Number of items.
 * @size: Size of each item.
 *
 * Return: Pointer to the zeroed allocation, or NULL on failure.
 */
void* kcalloc(int n, int size);

/** krealloc() - Allocate a new block, copying the old data and freeing it.
 * @ptr: Pointer to the previous allocation, or NULL.
 * @size: New block size.
 *
 * Return: Pointer to the resized allocation, or NULL on failure.
 */
void* krealloc(void* ptr, int size);

/** kfree() - Mark the block backing the given address as free.
 * @addr: Address previously returned by a heap allocation.
 */
void kfree(void* addr);

/** is_block_free() - Check whether a block is marked free.
 * @block: Heap block to inspect.
 *
 * Return: true if the block is free, false otherwise.
 */
bool is_block_free(heap_BLOCK* block);

/** worst_fit() - Find the first free block large enough for the request.
 * @size: Required block size.
 *
 * Return: Pointer to a usable free block, or NULL if none fits.
 */
heap_BLOCK* worst_fit(int size);

// Allocate a new heap block and append it to the list.
heap_BLOCK* allocate_new_block(int size);

void* operator new(size_t size);

void* operator new[](size_t size);

void* operator new(size_t size, std::align_val_t);

void* operator new[](size_t size, std::align_val_t);

void operator delete(void* ptr) noexcept;

void operator delete[](void* ptr) noexcept;

// C++14-compliant sized delete operators.
void operator delete(void* ptr, size_t size) noexcept;

void operator delete[](void* ptr, size_t size) noexcept;

void operator delete(void* ptr, std::align_val_t) noexcept;

void operator delete[](void* ptr, std::align_val_t) noexcept;

#endif  // MEMORY_MANAGER_H
