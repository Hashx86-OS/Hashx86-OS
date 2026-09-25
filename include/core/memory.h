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

#include <core/Iguard.h>
#include <core/globals.h>
#include <debug.h>
#include <stddef.h>
#include <string.h>
#include <types.h>

namespace std {
enum class align_val_t : size_t;
}

/**
 * kheap_init() - Initialize the kernel heap over a fixed address range.
 * @start_addr: First address of the heap region.
 * @end_addr: Address one past the end of the heap region.
 *
 * Bootstraps the TLSF allocator over the given range.
 *
 * Return: 0 on success, -1 when the region is too small or invalid.
 */
int kheap_init(void* start_addr, void* end_addr);

/**
 * kheap_print_blocks() - Print the kernel heap block layout to the debug log.
 */
void kheap_print_blocks();

/**
 * kmalloc() - Allocate bytes from the kernel heap.
 * @size: Number of bytes to allocate.
 *
 * Return: Pointer to the allocated memory, or NULL on failure.
 */
void* kmalloc(size_t size);

/**
 * kbrk() - Grow the heap break to satisfy a request.
 * @size: Number of additional bytes to make available.
 *
 * Currently implemented as an alias for kmalloc().
 *
 * Return: Pointer to the allocated memory, or NULL on failure.
 */
void* kbrk(size_t size);

/**
 * aligned_kmalloc() - Allocate memory aligned to a power of two.
 * @size: Number of bytes to allocate.
 * @alignment: Required alignment, which must be a power of two.
 *
 * Return: Pointer to the aligned allocation, or NULL on failure.
 */
void* aligned_kmalloc(size_t size, size_t alignment);

/**
 * aligned_kfree() - Free an aligned allocation from the kernel heap.
 * @ptr: Pointer previously returned by aligned_kmalloc().
 */
void aligned_kfree(void* ptr);

/**
 * kcalloc() - Allocate zero-initialized memory from the kernel heap.
 * @n: Number of elements to allocate.
 * @size: Size in bytes of each element.
 *
 * Return: Pointer to the zeroed allocation, or NULL on failure.
 */
void* kcalloc(int n, int size);

/**
 * krealloc() - Resize an allocation from the kernel heap.
 * @ptr: Pointer to the previous allocation, or NULL.
 * @size: New size in bytes.
 *
 * Return: Pointer to the resized allocation, or NULL on failure.
 */
void* krealloc(void* ptr, size_t size);

/**
 * kfree() - Free an allocation back to the kernel heap.
 * @addr: Pointer previously returned by a kernel heap allocator.
 */
void kfree(void* addr);

// C++ new and delete operators.
void* operator new(size_t size);
void* operator new[](size_t size);
void* operator new(size_t size, std::align_val_t);
void* operator new[](size_t size, std::align_val_t);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete(void* ptr, size_t size) noexcept;
void operator delete[](void* ptr, size_t size) noexcept;
void operator delete(void* ptr, std::align_val_t) noexcept;
void operator delete[](void* ptr, std::align_val_t) noexcept;

#endif  // MEMORY_MANAGER_H
