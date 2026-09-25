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

#define KDBG_COMPONENT "PMM"
#include <core/paging.h>
#include <core/pmm.h>

PMM_INFO g_pmm_info;

// Set a bit in the memory-map array, bounds-checked.
static inline void pmm_mmap_set(int bit) {
    if (bit >= 0 && bit < (int)g_pmm_info.max_blocks)
        g_pmm_info.memory_map_array[bit / 32] |= (1u << (bit % 32));
}

// Clear a bit in the memory-map array, bounds-checked.
static inline void pmm_mmap_unset(int bit) {
    if (bit >= 0 && bit < (int)g_pmm_info.max_blocks)
        g_pmm_info.memory_map_array[bit / 32] &= ~(1u << (bit % 32));
}

// Test the given bit in the memory-map array, bounds-checked.
static inline int pmm_mmap_test(int bit) {
    if (bit >= 0 && bit < (int)g_pmm_info.max_blocks)
        return (g_pmm_info.memory_map_array[bit / 32] & (1u << (bit % 32))) != 0;
    return 0;
}

uint32_t pmm_get_max_blocks() {
    KDBG3("get_max_blocks=%u", g_pmm_info.max_blocks);
    return g_pmm_info.max_blocks;
}

uint32_t pmm_get_used_blocks() {
    KDBG3("get_used_blocks=%u", g_pmm_info.used_blocks);
    return g_pmm_info.used_blocks;
}

// Find the first free frame in the bitmap and return its index.
int pmm_mmap_first_free() {
    uint32_t entries = (g_pmm_info.max_blocks + 31) / 32;
    for (uint32_t i = 0; i < entries; i++) {
        if (g_pmm_info.memory_map_array[i] != 0xffffffff) {
            for (uint32_t j = 0; j < 32; j++) {
                int bit = i * 32 + j;
                if (bit >= (int)g_pmm_info.max_blocks) break;
                if (!(g_pmm_info.memory_map_array[i] & (1 << j))) {
                    KDBG3("first_free bit=%d", bit);
                    return bit;
                }
            }
        }
    }
    KDBG2("single-frame search result=none");
    return -1;
}

// Find the first free frame below a limit (for low-memory allocations).
int pmm_mmap_first_free_low(uint32_t limit_frame) {
    uint32_t entries = (g_pmm_info.max_blocks + 31) / 32;
    for (uint32_t i = 0; i < entries; i++) {
        // Stop early if the chunk's first bit already exceeds the limit.
        if (i * 32 >= limit_frame) {
            KDBG2("low-memory search result=none limit_frame=%u", limit_frame);
            return -1;
        }

        if (g_pmm_info.memory_map_array[i] != 0xffffffff) {
            for (uint32_t j = 0; j < 32; j++) {
                int bit = i * 32 + j;
                if (bit >= (int)limit_frame) {
                    KDBG2("low-memory search result=none limit_frame=%u", limit_frame);
                    return -1;
                }
                if (bit >= (int)g_pmm_info.max_blocks) {
                    KDBG2("low-memory search result=none max_blocks=%u", g_pmm_info.max_blocks);
                    return -1;
                }

                if (!(g_pmm_info.memory_map_array[i] & (1u << j))) {
                    KDBG3("first_free_low limit_frame=%u bit=%d", limit_frame, bit);
                    return bit;
                }
            }
        }
    }
    KDBG2("low-memory search result=none limit_frame=%u", limit_frame);
    return -1;
}

// Find the first run of `size` contiguous free frames and return its index.
int pmm_mmap_first_free_by_size(uint32_t size) {
    if (size == 0) {
        KDBG2("contiguous search invalid request size=0");
        return -1;
    }

    uint32_t free = 0;
    int start_index = -1;

    uint32_t entries = (g_pmm_info.max_blocks + 31) / 32;
    for (uint32_t i = 0; i < entries; i++) {
        if (g_pmm_info.memory_map_array[i] != 0xffffffff) {
            for (uint32_t j = 0; j < 32; j++) {
                int bit = i * 32 + j;

                if (bit >= (int)g_pmm_info.max_blocks) {
                    KDBG2("contiguous search aborted reason=out_of_range size=%u", size);
                    return -1;
                }

                if (!pmm_mmap_test(bit)) {
                    if (free == 0) start_index = bit;
                    free++;
                    if (free == size) {
                        KDBG3("first_free_by_size size=%u start=%d", size, start_index);
                        return start_index;
                    }
                } else {
                    free = 0;
                    start_index = -1;
                }
            }
        } else {
            // Fully-used word: reset the run so it cannot span a used chunk.
            free = 0;
            start_index = -1;
        }
    }
    KDBG2("contiguous search result=none size=%u", size);
    return -1;
}

// Find the next run of free frames of the given size.
int pmm_next_free_frame(int size) {
    int next = pmm_mmap_first_free_by_size(size);
    KDBG3("next_free_frame size=%d next=%d", size, next);
    return next;
}

// Initialize the physical memory bitmap over the given range.
void pmm_init(PMM_PHYSICAL_ADDRESS bitmap, uint32_t total_memory_size) {
    g_pmm_info.memory_size = total_memory_size;
    g_pmm_info.memory_map_array = (uint32_t*)bitmap;

    g_pmm_info.max_blocks = total_memory_size / PMM_BLOCK_SIZE;
    g_pmm_info.used_blocks = g_pmm_info.max_blocks;

    // Mark every block as used (all bits set).
    uint32_t map_size = (g_pmm_info.max_blocks + 7) / 8;
    memset(g_pmm_info.memory_map_array, 0xff, map_size);

    // Record the end of the bitmap.
    g_pmm_info.memory_map_end = (uint32_t)g_pmm_info.memory_map_array + map_size;

    // Align the end marker to the next 4096-byte boundary.
    if (g_pmm_info.memory_map_end % PMM_BLOCK_SIZE != 0) {
        g_pmm_info.memory_map_end += PMM_BLOCK_SIZE - (g_pmm_info.memory_map_end % PMM_BLOCK_SIZE);
    }

    KDBG1("startup bitmap=0x%x total=%uKB blocks=%u", bitmap, total_memory_size / 1024,
          g_pmm_info.max_blocks);
    KDBG2("map_array=0x%x map_end=0x%x used=%u", (uint32_t)g_pmm_info.memory_map_array,
          g_pmm_info.memory_map_end, g_pmm_info.used_blocks);
}

void pmm_init_region(PMM_PHYSICAL_ADDRESS base, uint32_t region_size) {
    if (region_size == 0) {
        KDBG3("init_region skipped size=0");
        return;
    }

    int align = base / PMM_BLOCK_SIZE;
    int blocks = region_size / PMM_BLOCK_SIZE;

    while (blocks > 0) {
        if (pmm_mmap_test(align)) {
            pmm_mmap_unset(align);
            g_pmm_info.used_blocks--;
        }
        align++;
        blocks--;
    }

    KDBG2("region free-mark base=0x%x size=%uKB used=%u", base, region_size / 1024,
          g_pmm_info.used_blocks);
}

void pmm_deinit_region(PMM_PHYSICAL_ADDRESS base, uint32_t region_size) {
    if (region_size == 0) {
        KDBG3("deinit_region skipped size=0");
        return;
    }

    int align = base / PMM_BLOCK_SIZE;
    int blocks = region_size / PMM_BLOCK_SIZE;

    while (blocks > 0) {
        if (!pmm_mmap_test(align)) {
            pmm_mmap_set(align);
            g_pmm_info.used_blocks++;
        }
        align++;
        blocks--;
    }

    KDBG2("region reserve-mark base=0x%x size=%uKB used=%u", base, region_size / 1024,
          g_pmm_info.used_blocks);
}

void* pmm_alloc_block() {
    if (g_pmm_info.used_blocks >= g_pmm_info.max_blocks) {
        KDBG2("single-frame allocation failed reason=no_free_blocks");
        return NULL;
    }

    int frame = pmm_mmap_first_free();
    if (frame == -1) {
        KDBG2("single-frame allocation failed reason=frame_not_found");
        return NULL;
    }

    pmm_mmap_set(frame);

    // Use absolute addressing.
    PMM_PHYSICAL_ADDRESS addr = (frame * PMM_BLOCK_SIZE);

    g_pmm_info.used_blocks++;

    KDBG3("alloc_block frame=%d addr=0x%x used=%u", frame, addr, g_pmm_info.used_blocks);

    return (void*)addr;
}

void* pmm_alloc_block_low(uint32_t limit_addr) {
    if (g_pmm_info.used_blocks >= g_pmm_info.max_blocks) {
        KDBG2("low-memory allocation failed reason=no_free_blocks limit=0x%x", limit_addr);
        return NULL;
    }

    int limit_frame = limit_addr / PMM_BLOCK_SIZE;
    int frame = pmm_mmap_first_free_low(limit_frame);

    if (frame == -1) {
        KDBG2("low-memory allocation failed reason=frame_not_found limit=0x%x", limit_addr);
        return NULL;
    }

    pmm_mmap_set(frame);

    // Use absolute addressing.
    PMM_PHYSICAL_ADDRESS addr = (frame * PMM_BLOCK_SIZE);
    g_pmm_info.used_blocks++;

    KDBG3("alloc_block_low limit=0x%x frame=%d addr=0x%x used=%u", limit_addr, frame, addr,
          g_pmm_info.used_blocks);

    return (void*)addr;
}

void pmm_free_block(void* p) {
    if (p == nullptr) {
        KDBG2("free_block rejected: null pointer");
        return;
    }
    PMM_PHYSICAL_ADDRESS addr = (PMM_PHYSICAL_ADDRESS)p;

    // Reject unaligned addresses to prevent freeing the wrong frame.
    if ((addr % PMM_BLOCK_SIZE) != 0) {
        KDBG2("free_block rejected: unaligned addr=0x%x", addr);
        return;
    }

    // Guard: refuse to free kernel identity-map page-table frames.
    extern Paging* g_paging;
    if (g_paging && g_paging->KernelPageDirectory) {
        uint32_t frame_addr = addr & 0xFFFFF000;
        // Collect caller return addresses via the EBP chain for diagnostics.
        uint32_t caller1 = 0, caller2 = 0, caller3 = 0;
        uint32_t ebp;
        asm volatile("mov %%ebp, %0" : "=r"(ebp));
        uint32_t mem_bound = g_pmm_info.memory_size;
        if (ebp > 0x1000 && ebp < mem_bound) {
            caller1 = ((uint32_t*)ebp)[1];
            uint32_t next = ((uint32_t*)ebp)[0];
            if (next > 0x1000 && next < mem_bound) {
                caller2 = ((uint32_t*)next)[1];
                uint32_t next2 = ((uint32_t*)next)[0];
                if (next2 > 0x1000 && next2 < mem_bound) {
                    caller3 = ((uint32_t*)next2)[1];
                }
            }
        }
        for (int i = 0; i < 64; i++) {
            if ((g_paging->KernelPageDirectory[i] & 0xFFFFF000) == frame_addr) {
                KDBG1("BLOCKED free of kernel PT frame=0x%x pd_idx=%d callers=0x%x,0x%x,0x%x",
                      frame_addr, i, caller1, caller2, caller3);
                return;
            }
        }
        for (int i = 768; i < 1024; i++) {
            if ((g_paging->KernelPageDirectory[i] & 0xFFFFF000) == frame_addr) {
                KDBG1("BLOCKED free of kernel PT frame=0x%x pd_idx=%d callers=0x%x,0x%x,0x%x",
                      frame_addr, i, caller1, caller2, caller3);
                return;
            }
        }
        if (frame_addr == ((uint32_t)g_paging->KernelPageDirectory & 0xFFFFF000)) {
            KDBG1("BLOCKED free of kernel page directory frame=0x%x callers=0x%x,0x%x,0x%x",
                  frame_addr, caller1, caller2, caller3);
            return;
        }
    }

    // Use absolute addressing.
    int frame = addr / PMM_BLOCK_SIZE;

    if (frame < 0 || (uint32_t)frame >= g_pmm_info.max_blocks) {
        KDBG2("free_block invalid frame=%d addr=0x%x", frame, addr);
        return;
    }
    if (!pmm_mmap_test(frame)) {
        KDBG3("free_block already free frame=%d addr=0x%x", frame, addr);
        return;
    }
    pmm_mmap_unset(frame);
    g_pmm_info.used_blocks--;

    KDBG3("free_block frame=%d addr=0x%x used=%u", frame, addr, g_pmm_info.used_blocks);
}

void* pmm_alloc_blocks(uint32_t size) {
    uint32_t i;
    if (g_pmm_info.used_blocks + size > g_pmm_info.max_blocks) {
        KDBG2("contiguous allocation failed reason=no_free_blocks size=%u", size);
        return NULL;
    }

    int frame = pmm_mmap_first_free_by_size(size);
    if (frame == -1) {
        KDBG2("contiguous allocation failed reason=frame_not_found size=%u", size);
        return NULL;
    }

    for (i = 0; i < size; i++) pmm_mmap_set(frame + i);

    // Use absolute addressing.
    PMM_PHYSICAL_ADDRESS addr = (frame * PMM_BLOCK_SIZE);

    g_pmm_info.used_blocks += size;

    KDBG2("contiguous allocation size=%u addr=0x%x used=%u", size, addr, g_pmm_info.used_blocks);
    KDBG3("alloc_blocks bytes=%u", size * PMM_BLOCK_SIZE);

    return (void*)addr;
}

void pmm_free_blocks(void* p, uint32_t size) {
    if (p == nullptr) {
        KDBG2("free_blocks rejected: null pointer size=%u", size);
        return;
    }
    PMM_PHYSICAL_ADDRESS addr = (PMM_PHYSICAL_ADDRESS)p;

    // Reject unaligned addresses.
    if ((addr % PMM_BLOCK_SIZE) != 0) {
        KDBG2("free_blocks rejected: unaligned addr=0x%x", addr);
        return;
    }

    // Use absolute addressing.
    int frame = addr / PMM_BLOCK_SIZE;

    if (frame < 0 || (uint32_t)frame >= g_pmm_info.max_blocks ||
        frame + size > g_pmm_info.max_blocks) {
        KDBG2("contiguous release invalid frame=%d size=%u addr=0x%x", frame, size, addr);
        return;
    }

    uint32_t freed = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (pmm_mmap_test(frame + i)) {
            pmm_mmap_unset(frame + i);
            freed++;
        }
    }

    g_pmm_info.used_blocks -= freed;

    KDBG2("contiguous release size=%u addr=0x%x freed=%u used=%u", size, addr, freed,
          g_pmm_info.used_blocks);
}
