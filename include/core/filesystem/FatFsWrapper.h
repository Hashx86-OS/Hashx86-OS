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

#ifndef FATFS_WRAPPER_H
#define FATFS_WRAPPER_H

#include <core/drivers/ata.h>
#include <core/filesystem/FatFs/ff.h>
#include <core/filesystem/FileSystem.h>

// Maximum concurrently open files through the wrapper.
#define MAX_OPEN_FILES 32

/**
 * struct FatFsSlot - An open file or directory handle.
 * @file: Associated File object.
 * @used: Whether this slot is in use.
 * @isDir: Whether the slot holds a directory handle.
 * @dirReadCount: Remaining directory reads before exhaustion.
 * @u: FatFs file or directory handle.
 */
struct FatFsSlot {
    File* file;
    bool used;
    bool isDir;
    uint32_t dirReadCount;
    union {
        FIL fil;
        DIR dir;
    } u;
};

/**
 * fatfs_init() - Initialize the FatFs disk I/O layer.
 * @pdrv: Physical drive number.
 * @ata: ATA controller backing the drive.
 * @partitionStartLBA: First logical block of the partition.
 * @partitionSizeSectors: Partition size in sectors.
 *
 * Must be called before mounting.
 */
void fatfs_init(BYTE pdrv, AdvancedTechnologyAttachment* ata, uint32_t partitionStartLBA,
                uint32_t partitionSizeSectors);

/**
 * class FatFsWrapper - FatFs-backed FileSystem implementation.
 */
class FatFsWrapper : public FileSystem {
public:
    FatFsWrapper(AdvancedTechnologyAttachment* hd, uint32_t partitionOffset, BYTE pdrv,
                 uint32_t partitionSizeSectors);
    ~FatFsWrapper();

    File* Open(const char* path) override;
    uint32_t ReadStream(File* file, uint8_t* buffer, uint32_t length) override;
    void CloseFile(File* file) override;
    void ListRoot() override;
    void ListDir(char* path) override;
    void CreateFile(char* path) override;
    void MakeDirectory(char* path) override;
    void DeleteFile(char* path) override;
    void DeleteDirectory(char* path) override;
    void ReadFile(char* path, uint8_t* buffer, uint32_t length) override;
    void WriteFile(char* path, uint8_t* buffer, uint32_t length) override;
    uint32_t GetFileSize(char* path) override;
    void Format() override;

private:
    FatFsWrapper(const FatFsWrapper&) = delete;
    FatFsWrapper& operator=(const FatFsWrapper&) = delete;

    /**
     * struct SlotManager - Fixed-size pool of open file/directory slots.
     * @slots: The slot array.
     */
    struct SlotManager {
        FatFsSlot slots[MAX_OPEN_FILES];

        FatFsSlot* alloc(File* file, bool isDir);
        void free(FatFsSlot* slot);
        FatFsSlot* find(File* file);
    };

    FATFS fatfs;
    AdvancedTechnologyAttachment* hd;
    uint32_t partitionOffset;
    BYTE pdrv;
    SlotManager* slotMgr;

    bool PathToFatFs(const char* path, char* out, uint32_t outLen);
};

#endif
