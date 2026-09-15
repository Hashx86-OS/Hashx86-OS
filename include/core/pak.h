#ifndef PAK_H
#define PAK_H

#include <stdint.h>

#define PAK_MAGIC "PACK"
#define PAK_NAME_LEN 56

struct PakHeader {
    char magic[4];
    uint32_t dirOffset;
    uint32_t dirSize;
} __attribute__((packed));

struct PakDirEntry {
    char name[PAK_NAME_LEN];
    uint32_t offset;
    uint32_t size;
} __attribute__((packed));

#endif
