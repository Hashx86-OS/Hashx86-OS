#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <stdint.h>

char* itoa(int32_t num, char* str, size_t capacity, uint32_t base);
char* itoa_safe(int32_t num, char* str, size_t capacity, uint32_t base);

#endif  // STDLIB_H
