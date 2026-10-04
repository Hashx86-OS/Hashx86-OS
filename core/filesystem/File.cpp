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

#define KDBG_COMPONENT "FILE"
#include <core/filesystem/File.h>
#include <core/filesystem/FileSystem.h>
#include <core/process_types.h>

File::File() {
    this->size = 0;
    this->id = 0;
    this->position = 0;
    this->filesystem = 0;
    this->flags = 0;
    this->openFlags = O_RDONLY;
    for (int i = 0; i < 128; i++) this->name[i] = 0;
}

File::~File() {
    if (this->filesystem) {
        this->Close();
    }
}

/**
 * File::Read() - Read up to length bytes at the current position.
 * @buffer: Destination buffer.
 * @length: Maximum number of bytes to read.
 *
 * Regular files clamp the read to the remaining file size; the position
 * advances by the number of bytes actually read.
 *
 * Return: The number of bytes read, or 0 on failure.
 */
int File::Read(uint8_t* buffer, uint32_t length) {
    if (this->filesystem == 0 || buffer == 0 || length == 0) return 0;
    if (!this->IsReadable()) return 0;

    bool isDirectory = (this->flags & 1) != 0;

    if (!isDirectory) {
        // Clamp the read to the file's remaining size.
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
    if (!this->IsWritable() && this->position > this->size) this->position = this->size;
}

/**
 * File::Write() - Write bytes at the current position, extending the file.
 * @buffer: Source buffer.
 * @length: Number of bytes to write.
 *
 * Directories and read-only handles are refused. The position and the cached
 * size are advanced so a subsequent Seek()/Read() sees the new contents; a
 * write past the old end leaves the gap as a hole rather than shifting data.
 *
 * Return: Number of bytes written, or -1 on failure.
 */
int File::Write(uint8_t* buffer, uint32_t length) {
    if (this->filesystem == 0 || buffer == 0) return -1;
    if (length == 0) return 0;
    if (!this->IsWritable()) {
        KDBG1("Write refused on read-only handle '%s'", this->name);
        return -1;
    }
    if (this->flags & 1) {
        KDBG1("Write refused on directory '%s'", this->name);
        return -1;
    }

    // O_APPEND ignores the cached position and writes at the current end of
    // file, so advance the cursor from there rather than from the stale value.
    uint32_t start = (this->openFlags & O_APPEND) ? this->size : this->position;

    uint32_t written = this->filesystem->WriteStream(this, buffer, length);
    if (written == 0) return -1;

    this->position = start + written;
    if (this->position > this->size) this->size = this->position;

    return (int)written;
}

void File::Close() {
    if (this->filesystem) {
        this->filesystem->CloseFile(this);
        this->filesystem = 0;
    }
}

/**
 * AllocateFd() - Claim the lowest free descriptor slot for a file.
 * @pcb: The owning process.
 * @file: The file to assign.
 *
 * Return: The allocated descriptor, or -1 if none is free.
 */
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

/**
 * GetFileByFd() - Resolve a descriptor to its file.
 * @pcb: The owning process.
 * @fd: The descriptor to look up.
 *
 * Return: The file, or NULL for an out-of-range or empty slot.
 */
File* GetFileByFd(ProcessControlBlock* pcb, uint32_t fd) {
    if (!pcb || fd < FD_MIN || fd >= FD_MAX) return nullptr;
    return pcb->fdTable[fd];
}

/**
 * ReleaseFd() - Free a descriptor slot.
 * @pcb: The owning process.
 * @fd: The descriptor to release.
 */
void ReleaseFd(ProcessControlBlock* pcb, uint32_t fd) {
    if (!pcb || fd < FD_MIN || fd >= FD_MAX) return;
    pcb->fdTable[fd] = nullptr;
}
