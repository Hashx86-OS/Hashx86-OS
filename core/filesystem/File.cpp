/**
 * @file        File.cpp
 * @brief       File System Abstraction
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "FILE"
#include <core/filesystem/FileSystem.h>
#include <core/filesystem/File.h>
#include <core/process_types.h>

File::File() {
    this->size = 0;
    this->id = 0;
    this->position = 0;
    this->filesystem = 0;
    this->flags = 0;
    for (int i = 0; i < 128; i++) this->name[i] = 0;
}

File::~File() {
    if (this->filesystem) {
        this->Close();
    }
}

int File::Read(uint8_t* buffer, uint32_t length) {
    if (this->filesystem == 0 || buffer == 0 || length == 0) return 0;

    bool isDirectory = (this->flags & 1) != 0;

    if (!isDirectory) {
        // Safety for regular files
        if (this->position >= this->size) return 0;

        if (this->position + length > this->size) {
            length = this->size - this->position;
        }
    }

    uint32_t bytesRead = this->filesystem->ReadStream(this, buffer, length);
    this->position += bytesRead;

    return (int)bytesRead;
}

void File::Seek(uint32_t pos) {
    this->position = pos;
    if (this->position > this->size) this->position = this->size;
}

int File::Write(uint8_t* buffer, uint32_t length) {
    (void)buffer;
    (void)length;
    KDBG1("Write called on base File object — no backend registered; failing");
    return -1;
}

void File::Close() {
    if (this->filesystem) {
        this->filesystem->CloseFile(this);
        this->filesystem = 0;
    }
}

int32_t AllocateFd(ProcessControlBlock* pcb, File* file) {
    if (!pcb || !file) return -1;
    for (uint32_t fd = FD_MIN; fd < FD_MAX; fd++) {
        if (!pcb->fdTable[fd]) {
            pcb->fdTable[fd] = file;
            return (int32_t)fd;
        }
    }
    return -1;
}

File* GetFileByFd(ProcessControlBlock* pcb, uint32_t fd) {
    if (!pcb || fd < FD_MIN || fd >= FD_MAX) return nullptr;
    return pcb->fdTable[fd];
}

void ReleaseFd(ProcessControlBlock* pcb, uint32_t fd) {
    if (!pcb || fd < FD_MIN || fd >= FD_MAX) return;
    pcb->fdTable[fd] = nullptr;
}
