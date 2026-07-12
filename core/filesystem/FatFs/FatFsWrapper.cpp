#define KDBG_COMPONENT "FATFS"
#include <core/filesystem/FatFsWrapper.h>
#include "diskio.h"
#include <core/filesystem/File.h>
#include <debug.h>
#include <string.h>

// SlotManager implementation — instance-scoped slot tracking.
// (struct and slot data declared in FatFsWrapper.h)
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

void FatFsWrapper::SlotManager::free(FatFsSlot* slot) {
    if (slot) {
        slot->file = nullptr;
        slot->used = false;
    }
}

FatFsSlot* FatFsWrapper::SlotManager::find(File* file) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (slots[i].used && slots[i].file == file) return &slots[i];
    }
    return nullptr;
}

FatFsWrapper::FatFsWrapper(AdvancedTechnologyAttachment* hd, uint32_t partitionOffset, BYTE pdrv,
                           uint32_t partitionSizeSectors)
    : hd(hd), partitionOffset(partitionOffset), pdrv(pdrv), slotMgr(new SlotManager()) {

    if (!slotMgr) {
        HALT("CRITICAL: Failed to allocate FatFsSlotManager!\n");
    }

    fatfs_init(pdrv, hd, partitionOffset, partitionSizeSectors);
    /* Mount as "0:" for pdrv=0, "1:" for pdrv=1, etc. */
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

/* Copy path to a local buffer with FatFs-compatible format */
bool FatFsWrapper::PathToFatFs(const char* path, char* out, uint32_t outLen) {
    if (!path || !out || outLen < 3) return false;
    /* Prepend drive prefix (e.g. "0:", "1:") so the path resolves on the correct volume */
    out[0] = '0' + pdrv;
    out[1] = ':';
    uint32_t j = 2;
    uint32_t i = 0;
    /* Strip leading '/' for FatFs relative paths */
    if (path[0] == '/') i = 1;
    while (path[i] != 0 && j < outLen - 1) {
        out[j++] = path[i++];
    }
    /* Check whether the entire source was consumed */
    if (path[i] != 0) return false;
    out[j] = 0;
    return true;
}

File* FatFsWrapper::Open(const char* path) {
    if (!path) return nullptr;

    char fatPath[256];
    if (!PathToFatFs(path, fatPath, sizeof(fatPath))) return nullptr;

    /* Root directory — detect after drive prefix (e.g. "0:" with nothing after) */
    if (fatPath[0] != 0 && fatPath[1] == ':' && fatPath[2] == 0) {
        DIR d;
        FRESULT res = f_opendir(&d, fatPath);
        if (res != FR_OK) return nullptr;
        FatFsSlot* slot = slotMgr->alloc(nullptr, true);
        if (!slot) { f_closedir(&d); return nullptr; }
        slot->u.dir = d;
        slot->dirReadCount = 0;

        File* root = new File();
        if (!root) { f_closedir(&slot->u.dir); slotMgr->free(slot); return nullptr; }
        root->name[0] = '/';
        root->name[1] = 0;
        root->size = 0;
        root->id = 0;
        root->position = 0;
        root->filesystem = this;
        root->flags = 1;
        slot->file = root;
        return root;
    }

    FILINFO info;
    FRESULT res = f_stat(fatPath, &info);
    if (res != FR_OK) return nullptr;

    BYTE mode = FA_READ | FA_OPEN_EXISTING;
    if (info.fattrib & AM_DIR) {
        /* Open directory via f_opendir */
        DIR d;
        FRESULT res2 = f_opendir(&d, fatPath);
        if (res2 != FR_OK) return nullptr;
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
        uint32_t i = 0;
        while (path[i] && i < 127) {
            dir->name[i] = path[i];
            i++;
        }
        dir->name[i] = 0;
        dir->size = 0;
        dir->id = 0;
        dir->position = 0;
        dir->filesystem = this;
        dir->flags = 1;
        slot->file = dir;
        return dir;
    }

    FIL fil;
    res = f_open(&fil, fatPath, mode);
    if (res != FR_OK) return nullptr;

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

    return file;
}

uint32_t FatFsWrapper::ReadStream(File* file, uint8_t* buffer, uint32_t length) {
    if (!file || !buffer || length == 0) return 0;

    FatFsSlot* slot = slotMgr->find(file);
    if (!slot) return 0;

    if (slot->isDir) {
        /* Directory: return entries in KernelDirent format */
        uint32_t total = 0;

        /* Rewind directory cursor to the beginning before fast-forward, because
         * subsequent ReadStream calls resume from where f_readdir left off after the
         * previous batch — without rewind, the skip loop would advance past entries
         * that were never returned to the caller. */
        f_readdir(&slot->u.dir, NULL);

        /* Fast-forward to correct entry position using accumulated d_reclen byte offset.
         * file->position holds the byte offset from the start of the directory stream,
         * matching what the caller advanced via d_reclen. */
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

            /* Calculate name length and record size */
            uint32_t namelen = 0;
            while (info.fname[namelen]) namelen++;
            uint32_t reclen = sizeof(KernelDirentHeader) + namelen + 1;  /* +1 for null */
            /* Align to 4 bytes */
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

    /* Regular file: read via f_read */
    FRESULT res = f_lseek(&slot->u.fil, file->position);
    if (res != FR_OK) return 0;

    UINT br = 0;
    res = f_read(&slot->u.fil, buffer, length, &br);
    if (res != FR_OK) return 0;
    return (uint32_t)br;
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
