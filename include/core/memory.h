#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <string.h>
#include <core/Iguard.h>
#include <core/globals.h>
#include <debug.h>
#include <stddef.h>
#include <types.h>

namespace std {
enum class align_val_t : size_t;
}

int  kheap_init(void* start_addr, void* end_addr);
void kheap_print_blocks();

void* kmalloc(size_t size);
void* kbrk(size_t size);

void* aligned_kmalloc(size_t size, size_t alignment);
void  aligned_kfree(void* ptr);
void* kcalloc(int n, int size);
void* krealloc(void* ptr, size_t size);
void  kfree(void* addr);

// C++ New/Delete
void* operator new(size_t size);
void* operator new[](size_t size);
void* operator new(size_t size, std::align_val_t);
void* operator new[](size_t size, std::align_val_t);
void  operator delete(void* ptr) noexcept;
void  operator delete[](void* ptr) noexcept;
void  operator delete(void* ptr, size_t size) noexcept;
void  operator delete[](void* ptr, size_t size) noexcept;
void  operator delete(void* ptr, std::align_val_t) noexcept;
void  operator delete[](void* ptr, std::align_val_t) noexcept;

#endif  // MEMORY_MANAGER_H
