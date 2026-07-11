#ifndef FATFS_WRAPPER_H
#define FATFS_WRAPPER_H

#include <core/filesystem/FileSystem.h>
#include <core/filesystem/FatFs/ff.h>
#include <core/drivers/ata.h>

/* Max simultaneously open files through the wrapper */
#define MAX_OPEN_FILES 32

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

/* Init function for FatFs disk I/O layer. Must be called before mounting. */
void fatfs_init(BYTE pdrv, AdvancedTechnologyAttachment* ata, uint32_t partitionStartLBA,
                uint32_t partitionSizeSectors);

class FatFsWrapper : public FileSystem {
public:
    FatFsWrapper(AdvancedTechnologyAttachment* hd, uint32_t partitionOffset, BYTE pdrv,
                 uint32_t partitionSizeSectors);
    ~FatFsWrapper();

    File* Open(char* path) override;
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

    bool PathToFatFs(char* path, char* out, uint32_t outLen);
};

#endif
