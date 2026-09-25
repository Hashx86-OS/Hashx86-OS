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

#ifndef PMM_H
#define PMM_H

#include <core/memory.h>
#include <debug.h>
#include <types.h>

typedef uint32_t PMM_PHYSICAL_ADDRESS;
#define PMM_BLOCK_SIZE 4096  // 4096 bytes (4 KiB) per block.

/**
 * struct PMM_INFO - Global state of the physical memory manager.
 * @memory_size: Total physical memory size, in bytes.
 * @max_blocks: Total number of 4 KiB blocks.
 * @memory_map_array: Bitmap of block allocation state, one bit per block.
 * @memory_map_end: End of the allocation bitmap.
 * @used_blocks: Number of currently used blocks.
 */
typedef struct {
    uint32_t memory_size;
    uint32_t max_blocks;
    uint32_t* memory_map_array;
    uint32_t memory_map_end;
    uint32_t used_blocks;
} PMM_INFO;

/**
 * pmm_get_max_blocks() - Return the total number of 4 KiB blocks.
 *
 * Return: The block count.
 */
uint32_t pmm_get_max_blocks();

/**
 * pmm_get_used_blocks() - Return the number of used blocks.
 *
 * Return: The used block count.
 */
uint32_t pmm_get_used_blocks();

/**
 * pmm_mmap_first_free() - Find the first free block in the bitmap.
 *
 * Return: Index of the first free block, or -1 if none is free.
 */
int pmm_mmap_first_free();

/**
 * pmm_mmap_first_free_by_size() - Find the first run of contiguous free frames.
 * @size: Number of contiguous free frames to find.
 *
 * Return: Index of the first free frame of the run, or -1 on failure.
 */
int pmm_mmap_first_free_by_size(uint32_t size);

/**
 * pmm_next_free_frame() - Find the next run of free frames.
 * @size: Number of free frames to require.
 *
 * Return: Index of the start of the free run, or -1 on failure.
 */
int pmm_next_free_frame(int size);

/**
 * pmm_init() - Initialize the memory allocation bitmap.
 * @bitmap: Address of the bitmap storage.
 * @total_memory_size: Total physical memory to manage, in bytes.
 *
 * Marks every block as used, then aligns the end of the bitmap to a block
 * boundary.
 */
void pmm_init(PMM_PHYSICAL_ADDRESS bitmap, uint32_t total_memory_size);

/**
 * pmm_init_region() - Mark a region of blocks as free.
 * @base: Base address of the region.
 * @region_size: Size of the region, in bytes.
 */
void pmm_init_region(PMM_PHYSICAL_ADDRESS base, uint32_t region_size);

/**
 * pmm_deinit_region() - Mark a region of blocks as used.
 * @base: Base address of the region.
 * @region_size: Size of the region, in bytes.
 */
void pmm_deinit_region(PMM_PHYSICAL_ADDRESS base, uint32_t region_size);

/**
 * pmm_alloc_block() - Allocate a single 4 KiB block.
 *
 * Return: Address of the allocated block, or NULL on failure.
 */
void* pmm_alloc_block();

/**
 * pmm_alloc_block_low() - Allocate a single block below an address limit.
 * @limit_addr: Exclusive upper bound for the block address.
 *
 * Critical for page tables, which must be identity-mapped in low memory.
 *
 * Return: Address of the allocated block, or NULL on failure.
 */
void* pmm_alloc_block_low(uint32_t limit_addr);

/**
 * pmm_free_block() - Free a single block.
 * @p: Address previously returned by pmm_alloc_block().
 *
 * Rejects null, unaligned, out-of-range and already-free addresses, and
 * refuses to free kernel identity-map page-table frames.
 */
void pmm_free_block(void* p);

/**
 * pmm_alloc_blocks() - Allocate a contiguous run of blocks.
 * @size: Number of blocks to allocate.
 *
 * Return: Address of the first block, or NULL on failure.
 */
void* pmm_alloc_blocks(uint32_t size);

/**
 * pmm_free_blocks() - Free a contiguous run of blocks.
 * @p: Address previously returned by pmm_alloc_blocks().
 * @size: Number of blocks to free.
 */
void pmm_free_blocks(void* p, uint32_t size);

#endif
