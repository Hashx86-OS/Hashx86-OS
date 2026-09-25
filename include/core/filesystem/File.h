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

#ifndef FILE_H
#define FILE_H

#include <debug.h>
#include <types.h>

class FileSystem;
struct ProcessControlBlock;

/**
 * class File - An open handle on a filesystem object.
 */
class File {
public:
    File();
    ~File();

    // Standard metadata.
    char name[128];
    uint32_t size;
    uint32_t id;     // Usually the first cluster number.
    uint32_t flags;  // 1=Dir, 2=ReadOnly, etc.

    // Read cursor at the current position in the file.
    uint32_t position;

    // The filesystem backing this file.
    FileSystem* filesystem;

    /**
     * Read() - Read bytes from the current position into a buffer.
     * @buffer: Destination buffer.
     * @length: Maximum number of bytes to read.
     *
     * Return: Number of bytes actually read, and advances the position.
     */
    int Read(uint8_t* buffer, uint32_t length);

    /**
     * Seek() - Move the file cursor.
     * @pos: New position.
     */
    void Seek(uint32_t pos);

    int Write(uint8_t* buffer, uint32_t length);

    void Close();
};

/**
 * AllocateFd() - Assign the next free file descriptor to a file.
 * @pcb: Owning process.
 * @file: File to assign.
 *
 * Return: The allocated descriptor, or -1 on exhaustion.
 */
int32_t AllocateFd(ProcessControlBlock* pcb, File* file);

/**
 * GetFileByFd() - Look up a file by its descriptor.
 * @pcb: Owning process.
 * @fd: File descriptor.
 *
 * Return: The file, or null when the descriptor is invalid.
 */
File* GetFileByFd(ProcessControlBlock* pcb, uint32_t fd);

/**
 * ReleaseFd() - Free a file descriptor.
 * @pcb: Owning process.
 * @fd: File descriptor to release.
 */
void ReleaseFd(ProcessControlBlock* pcb, uint32_t fd);

#endif
