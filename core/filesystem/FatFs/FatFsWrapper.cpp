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

#define KDBG_COMPONENT "FATFS"

#include <core/filesystem/FatFs/ff.h>

#include <core/filesystem/FatFs/diskio.h>
#include <core/filesystem/FatFsWrapper.h>
#include <core/filesystem/File.h>
#include <debug.h>
#include <string.h>

/**
 * FatFsWrapper::SlotManager::alloc() - Claim a free open-file slot.
 * @file: The File object the slot backs, or NULL for a directory handle.
 * @isDir: True when the slot holds a DIR stream rather than a FIL stream.
 *
 * Return: A pointer to the claimed slot, or NULL if all MAX_OPEN_FILES slots
 *         are in use.
 */
FatFsSlot* FatFsWrapper::SlotManager::alloc(File* file, bool isDir) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!slots[i].used) {
            slots[i].file = file;
            slots[i].used = true;
            slots[i].isDir = isDir;
            slots[i].dirReadCount = 0;
            return &slots[i];
        }
    }
    return nullptr;
}

/**
 * FatFsWrapper::SlotManager::free() - Release a slot back to the pool.
 * @slot: Slot previously returned by alloc(), may be NULL.
 */
void FatFsWrapper::SlotManager::free(FatFsSlot* slot) {
    if (slot) {
        slot->file = nullptr;
        slot->used = false;
    }
}

/**
 * FatFsWrapper::SlotManager::find() - Locate the slot owning a File object.
 * @file: File owned by one of the slots.
 *
 * Return: The matching slot, or NULL if the File has no slot.
 */
FatFsSlot* FatFsWrapper::SlotManager::find(File* file) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (slots[i].used && slots[i].file == file) return &slots[i];
    }
    return nullptr;
}

/**
 * FatFsWrapper::FatFsWrapper() - Bind a partition and mount it as a volume.
 * @hd: ATA device hosting the partition.
 * @partitionOffset: First sector of the partition on the device.
 * @pdrv: FatFs physical drive number this wrapper drives.
 * @partitionSizeSectors: Partition size in 512-byte sectors.
 */
FatFsWrapper::FatFsWrapper(AdvancedTechnologyAttachment* hd, uint32_t partitionOffset, BYTE pdrv,
                           uint32_t partitionSizeSectors)
    : hd(hd), partitionOffset(partitionOffset), pdrv(pdrv), slotMgr(new SlotManager()) {
    if (!slotMgr) {
        HALT("CRITICAL: Failed to allocate FatFsSlotManager!\n");
    }

    fatfs_init(pdrv, hd, partitionOffset, partitionSizeSectors);
    // Mount as "0:" for pdrv=0, "1:" for pdrv=1, and so on.
    char mountPath[4] = "0:";
    mountPath[0] = '0' + pdrv;
    FRESULT res = f_mount(&fatfs, mountPath, 1);
    if (res != FR_OK) {
        KDBG1("FatFs mount(pdrv=%u) failed: %d", pdrv, res);
    } else {
        KDBG2("FatFs mounted (pdrv=%u, partOffset=%u).", pdrv, partitionOffset);
    }
}

FatFsWrapper::~FatFsWrapper() {
    // Close any open handles while the filesystem is still mounted.
    if (slotMgr) {
        for (int i = 0; i < MAX_OPEN_FILES; i++) {
            FatFsSlot& s = slotMgr->slots[i];
            if (s.used) {
                if (s.isDir) {
                    f_closedir(&s.u.dir);
                } else {
                    f_close(&s.u.fil);
                }
                s.used = false;
            }
        }
    }
    // Now safe to unmount the volume.
    char mountPath[4] = "0:";
    mountPath[0] = '0' + pdrv;
    f_mount(nullptr, mountPath, 0);
    delete slotMgr;
    slotMgr = nullptr;
}

/**
 * FatFsWrapper::PathToFatFs() - Rewrite a kernel path into a FatFs path.
 * @path: Kernel path, e.g. "/Hashx86/apps/test.bin".
 * @out: Destination buffer, at least 3 bytes long.
 * @outLen: Size of @out.
 *
 * Prepends the "N:" drive prefix for this wrapper's volume and strips the
 * leading '/' so the path resolves relative to the FAT volume root.
 *
 * Return: True if the whole path fit into @out.
 */
bool FatFsWrapper::PathToFatFs(const char* path, char* out, uint32_t outLen) {
    if (!path || !out || outLen < 3) return false;
    out[0] = '0' + pdrv;
    out[1] = ':';
    uint32_t j = 2;
    uint32_t i = 0;
    // FatFs paths are relative to the volume root; drop the leading '/'.
    if (path[0] == '/') i = 1;
    while (path[i] != 0 && j < outLen - 1) {
        out[j++] = path[i++];
    }
    // Fail if the entire source was not consumed (buffer too small).
    if (path[i] != 0) return false;
    out[j] = 0;
    return true;
}

/**
 * FatFsWrapper::Open() - Open a file or directory and return a File handle.
 * @path: Kernel path, e.g. "/Hashx86/apps/test.bin". "/" opens the volume
 *        root directory.
 *
 * Return: A File handle bound to an open FatFs stream, or NULL on failure.
 */
File* FatFsWrapper::Open(const char* path) {
    return this->OpenWithFlags(path, O_RDONLY);
}

/**
 * FatFsWrapper::OpenWithFlags() - Open a file honouring the requested access mode.
 * @path: Path to open.
 * @flags: O_* flags as passed to sys_open().
 *
 * The O_* flags are mapped onto FatFs' FA_* open modes. O_CREAT without O_EXCL
 * uses FA_OPEN_ALWAYS so an existing file is preserved, matching POSIX, while
 * O_CREAT|O_EXCL uses FA_CREATE_NEW and fails on an existing name. Directories
 * are always returned read-only.
 *
 * Return: A File handle bound to an open FatFs stream, or NULL on failure.
 */
File* FatFsWrapper::OpenWithFlags(const char* path, uint32_t flags) {
    if (!path) return nullptr;

    // O_RDONLY is 0, so an unset access mode means read-only.
    uint32_t acc = flags & O_ACCMODE;
    if (acc != O_RDONLY && acc != O_WRONLY && acc != O_RDWR) acc = O_RDONLY;

    // Truncating requires write access; refuse the nonsensical combination
    // rather than letting FatFs truncate a handle that cannot write.
    if ((flags & O_TRUNC) && acc == O_RDONLY) return nullptr;

    // O_EXCL only has meaning together with O_CREAT; on its own it must not
    // silently turn a plain open into a create.
    if ((flags & O_EXCL) && !(flags & O_CREAT)) return nullptr;

    char fatPath[256];
    if (!PathToFatFs(path, fatPath, sizeof(fatPath))) return nullptr;

    // Root directory: detected when only the drive prefix (e.g. "0:") remains.
    if (fatPath[0] != 0 && fatPath[1] == ':' && fatPath[2] == 0) {
        DIR d;
        FRESULT res = f_opendir(&d, fatPath);
        if (res != FR_OK) return nullptr;
        FatFsSlot* slot = slotMgr->alloc(nullptr, true);
        if (!slot) {
            f_closedir(&d);
            return nullptr;
        }
        slot->u.dir = d;
        slot->dirReadCount = 0;

        File* root = new File();
        if (!root) {
            f_closedir(&slot->u.dir);
            slotMgr->free(slot);
            return nullptr;
        }
        root->name[0] = '/';
        root->name[1] = 0;
        root->size = 0;
        root->id = 0;
        root->position = 0;
        root->filesystem = this;
        root->flags = 1;
        root->openFlags = O_RDONLY;
        slot->file = root;
        return root;
    }

    // A create request legitimately targets a name that does not exist yet, so
    // a failed f_stat is only fatal when the caller did not ask to create.
    bool creating = (flags & O_CREAT) != 0;
    bool isDir = false;

    FILINFO info;
    FRESULT res = f_stat(fatPath, &info);
    if (res != FR_OK) {
        if (!creating) return nullptr;
    } else {
        isDir = (info.fattrib & AM_DIR) != 0;
    }

    // A directory is enumerated through a DIR handle, not a FIL handle, so it
    // needs its own open path regardless of the access mode asked for.
    if (isDir) {
        DIR d;
        res = f_opendir(&d, fatPath);
        if (res != FR_OK) return nullptr;

        FatFsSlot* slot = slotMgr->alloc(nullptr, true);
        if (!slot) {
            f_closedir(&d);
            return nullptr;
        }
        slot->u.dir = d;
        slot->dirReadCount = 0;

        File* dir = new File();
        if (!dir) {
            f_closedir(&slot->u.dir);
            slotMgr->free(slot);
            return nullptr;
        }
        for (uint32_t i = 0; i < sizeof(dir->name) - 1 && path[i]; i++) dir->name[i] = path[i];
        dir->name[sizeof(dir->name) - 1] = 0;
        dir->size = 0;
        dir->id = 0;
        dir->position = 0;
        dir->filesystem = this;
        dir->flags = 1;
        dir->openFlags = O_RDONLY;
        slot->file = dir;
        return dir;
    }

    BYTE mode;
    if (acc == O_WRONLY) {
        mode = FA_WRITE;
    } else if (acc == O_RDWR) {
        mode = FA_READ | FA_WRITE;
    } else {
        mode = FA_READ;
    }

    // FatFs expresses "truncate" as FA_CREATE_ALWAYS, which also creates.
    // O_CREAT|O_TRUNC must map to it rather than to FA_OPEN_ALWAYS, or an
    // existing file would silently keep its old contents.
    if (flags & O_EXCL) {
        mode |= FA_CREATE_NEW;
    } else if (flags & O_TRUNC) {
        mode |= FA_CREATE_ALWAYS;
    } else if (flags & O_CREAT) {
        mode |= FA_OPEN_ALWAYS;
    } else {
        mode |= FA_OPEN_EXISTING;
    }

    FIL fil;
    memset(&fil, 0, sizeof(fil));
    res = f_open(&fil, fatPath, mode);
    if (res != FR_OK) return nullptr;
    fil.clust = fil.obj.sclust;

    File* file = new File();
    if (!file) {
        f_close(&fil);
        return nullptr;
    }

    FatFsSlot* slot = slotMgr->alloc(file, false);
    if (!slot) {
        f_close(&fil);
        delete file;
        return nullptr;
    }
    slot->u.fil = fil;

    uint32_t i = 0;
    while (path[i] && i < 127) {
        file->name[i] = path[i];
        i++;
    }
    file->name[i] = 0;
    file->size = (uint32_t)f_size(&slot->u.fil);
    file->id = 0;
    file->position = 0;
    file->filesystem = this;
    file->flags = 0;
    file->openFlags = flags;

    return file;
}

/**
 * FatFsWrapper::ReadStream() - Read from a file or directory stream.
 * @file: Handle from Open().
 * @buffer: Destination buffer.
 * @length: Number of bytes to read.
 *
 * For regular files this reads via f_read() from the file's current position.
 * For directories it emits an aligned sequence of KernelDirentHeader records,
 * resuming from the byte offset recorded in file->position.
 *
 * Return: Bytes produced; 0 on a bad handle or FatFs error.
 */
uint32_t FatFsWrapper::ReadStream(File* file, uint8_t* buffer, uint32_t length) {
    if (!file || !buffer || length == 0) return 0;

    FatFsSlot* slot = slotMgr->find(file);
    if (!slot) return 0;

    if (slot->isDir) {
        uint32_t total = 0;

        // Rewind the cursor before fast-forwarding: f_readdir() resumes from
        // where the previous batch left off, and without a rewind the skip
        // loop below would advance past entries never returned to the caller.
        f_readdir(&slot->u.dir, NULL);

        // Fast-forward to the caller's position using the accumulated d_reclen
        // byte offset stored in file->position.
        uint32_t skipped = 0;
        while (skipped < file->position) {
            FILINFO info;
            if (f_readdir(&slot->u.dir, &info) != FR_OK || info.fname[0] == 0) {
                break;
            }
            uint32_t namelen = 0;
            while (info.fname[namelen]) namelen++;
            uint32_t reclen = sizeof(KernelDirentHeader) + namelen + 1;
            reclen = (reclen + 3) & ~3;
            skipped += reclen;
            slot->dirReadCount++;
        }

        while (total + sizeof(KernelDirentHeader) <= length) {
            FILINFO info;
            if (f_readdir(&slot->u.dir, &info) != FR_OK || info.fname[0] == 0) {
                break;
            }

            // Each record carries the name plus one NUL terminator, aligned to 4.
            uint32_t namelen = 0;
            while (info.fname[namelen]) namelen++;
            uint32_t reclen = sizeof(KernelDirentHeader) + namelen + 1;
            reclen = (reclen + 3) & ~3;

            if (total + reclen > length) break;

            KernelDirentHeader* hdr = (KernelDirentHeader*)(buffer + total);
            hdr->d_ino = 0;
            hdr->d_reclen = reclen;
            hdr->d_type = (info.fattrib & AM_DIR) ? 1 : 0;
            uint8_t* dst = (uint8_t*)hdr + sizeof(KernelDirentHeader);
            for (uint32_t i = 0; i < namelen; i++) {
                dst[i] = (uint8_t)info.fname[i];
            }
            dst[namelen] = 0;

            total += reclen;
            slot->dirReadCount++;
        }

        return total;
    }

    // Regular file: seek to the current position, then read.
    FRESULT res = f_lseek(&slot->u.fil, file->position);
    if (res != FR_OK) return 0;

    UINT br = 0;
    res = f_read(&slot->u.fil, buffer, length, &br);
    if (res != FR_OK) return 0;
    return (uint32_t)br;
}

/**
 * FatFsWrapper::WriteStream() - Write bytes at a file handle's position.
 * @file: Handle from OpenWithFlags().
 * @buffer: Source buffer.
 * @length: Number of bytes to write.
 *
 * Mirrors ReadStream() by seeking to the tracked position first, so the
 * kernel-side cursor is authoritative. O_APPEND instead seeks to the current
 * end of file on every call. Directory handles are rejected.
 *
 * Return: Bytes written; 0 on a bad handle or FatFs error.
 */
uint32_t FatFsWrapper::WriteStream(File* file, uint8_t* buffer, uint32_t length) {
    if (!file || !buffer || length == 0) return 0;

    FatFsSlot* slot = slotMgr->find(file);
    if (!slot) return 0;
    if (slot->isDir) return 0;

    FSIZE_t target = (file->openFlags & O_APPEND) ? f_size(&slot->u.fil) : (FSIZE_t)file->position;

    // f_lseek() computes fp->clust as the cluster the seek target lands in, and
    // f_write() walks the chain forward from whatever fp->clust holds - it
    // never re-derives the cluster from fp->fptr. So the cursor must survive
    // the seek: resetting it to the start of the chain after f_lseek() would
    // make a write at a nonzero offset land in cluster 0 and overwrite the
    // front of the file. Seed it only for the case f_lseek() does not cover:
    // a target of 0 skips its normal-seek branch entirely, leaving fp->clust
    // as f_open() set it (0, i.e. no cluster chain to follow).
    if (target == 0) {
        slot->u.fil.clust = slot->u.fil.obj.sclust;
    }

    FRESULT res = f_lseek(&slot->u.fil, target);
    if (res != FR_OK) return 0;

    UINT bw = 0;
    res = f_write(&slot->u.fil, buffer, length, &bw);
    if (res != FR_OK) return 0;
    return (uint32_t)bw;
}

void FatFsWrapper::CloseFile(File* file) {
    if (!file) return;
    FatFsSlot* slot = slotMgr->find(file);
    if (slot) {
        if (slot->isDir) {
            f_closedir(&slot->u.dir);
        } else {
            f_close(&slot->u.fil);
        }
        slotMgr->free(slot);
    }
}

void FatFsWrapper::ListRoot() {
    ListDir((char*)"/");
}

void FatFsWrapper::ListDir(char* path) {
    char fatPath[256];
    if (!PathToFatFs(path, fatPath, sizeof(fatPath))) return;

    DIR dir;
    FRESULT res = f_opendir(&dir, fatPath);
    if (res != FR_OK) {
        KDBG1("ListDir: opendir failed: %d", res);
        return;
    }

    KDBG2("Listing: %s", path);
    FILINFO info;
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0) {
        KDBG2(" %s%s", info.fname, (info.fattrib & AM_DIR) ? "/" : "");
    }
    f_closedir(&dir);
}

void FatFsWrapper::CreateFile(char* path) {
    char fatPath[256];
    if (!PathToFatFs(path, fatPath, sizeof(fatPath))) return;

    FIL fil;
    FRESULT res = f_open(&fil, fatPath, FA_CREATE_NEW | FA_WRITE);
    if (res != FR_OK) {
        KDBG1("CreateFile: %d", res);
        return;
    }
    f_close(&fil);
    KDBG2("Created: %s", path);
}

void FatFsWrapper::MakeDirectory(char* path) {
    char fatPath[256];
    if (!PathToFatFs(path, fatPath, sizeof(fatPath))) return;

    FRESULT res = f_mkdir(fatPath);
    if (res != FR_OK) {
        KDBG1("MkDir: %d", res);
        return;
    }
    KDBG2("Dir created: %s", path);
}

void FatFsWrapper::DeleteFile(char* path) {
    char fatPath[256];
    if (!PathToFatFs(path, fatPath, sizeof(fatPath))) return;

    FRESULT res = f_unlink(fatPath);
    if (res != FR_OK) {
        KDBG1("DeleteFile: %d", res);
        return;
    }
    KDBG2("Deleted: %s", path);
}

void FatFsWrapper::DeleteDirectory(char* path) {
    DeleteFile(path);
}

void FatFsWrapper::ReadFile(char* path, uint8_t* buffer, uint32_t length) {
    char fatPath[256];
    if (!PathToFatFs(path, fatPath, sizeof(fatPath))) return;

    FIL fil;
    if (f_open(&fil, fatPath, FA_READ) != FR_OK) return;
    UINT br = 0;
    f_read(&fil, buffer, length, &br);
    f_close(&fil);
}

void FatFsWrapper::WriteFile(char* path, uint8_t* buffer, uint32_t length) {
    char fatPath[256];
    if (!PathToFatFs(path, fatPath, sizeof(fatPath))) return;

    FIL fil;
    FRESULT res = f_open(&fil, fatPath, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) return;
    UINT bw = 0;
    f_write(&fil, buffer, length, &bw);
    f_close(&fil);
}

uint32_t FatFsWrapper::GetFileSize(char* path) {
    char fatPath[256];
    if (!PathToFatFs(path, fatPath, sizeof(fatPath))) return 0;

    FILINFO info;
    FRESULT res = f_stat(fatPath, &info);
    return (res == FR_OK) ? (uint32_t)info.fsize : 0;
}

void FatFsWrapper::Format() {
    KDBG2("FatFs Format (pdrv=%u)...", pdrv);
    MKFS_PARM opt;
    memset(&opt, 0, sizeof(opt));
    opt.fmt = FM_FAT32 | FM_SFD;
    opt.au_size = 4096;

    char mountPath[4] = "0:";
    mountPath[0] = '0' + pdrv;
    uint8_t work[4096];
    FRESULT res = f_mkfs(mountPath, &opt, work, sizeof(work));
    if (res != FR_OK) {
        KDBG1("Format failed: %d", res);
        return;
    }
    KDBG2("Format done.");
}
