#ifndef FILE_SYSTEM_BASE_H
#define FILE_SYSTEM_BASE_H

#include <types.h>

class File;

/* Format of directory entries returned by ReadStream for directories */
struct KernelDirentHeader {
    uint32_t d_ino;
    uint16_t d_reclen;  /* total record size including header + name + padding */
    uint8_t  d_type;    /* 0=file, 1=dir */
    char     d_name[];  /* null-terminated filename, aligned to 4 */
} __attribute__((packed));

class FileSystem {
public:
    virtual ~FileSystem() {}

    virtual File* Open(char* path) = 0;
    virtual uint32_t ReadStream(File* file, uint8_t* buffer, uint32_t length) = 0;
    virtual void CloseFile(File* file) = 0;
    virtual void ListRoot() = 0;
    virtual void ListDir(char* path) = 0;
    virtual void CreateFile(char* path) = 0;
    virtual void MakeDirectory(char* path) = 0;
    virtual void DeleteFile(char* path) = 0;
    virtual void DeleteDirectory(char* path) = 0;
    virtual void ReadFile(char* path, uint8_t* buffer, uint32_t length) = 0;
    virtual void WriteFile(char* path, uint8_t* buffer, uint32_t length) = 0;
    virtual uint32_t GetFileSize(char* path) = 0;
    virtual void Format() = 0;
};

#endif
