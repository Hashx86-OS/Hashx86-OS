#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <stdint.h>

char* itoa(int32_t num, char* str, uint32_t base, size_t capacity);
char* itoa_safe(int32_t num, char* str, uint32_t base, size_t capacity);

#endif  // STDLIB_H
