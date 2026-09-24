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

#ifndef FILE_SYSTEM_BASE_H
#define FILE_SYSTEM_BASE_H

#include <types.h>

class File;

/**
 * struct KernelDirentHeader - Header of a directory entry from ReadStream.
 * @d_ino: Inode number.
 * @d_reclen: Total record size including header, name and padding.
 * @d_type: Entry type (0=file, 1=dir).
 * @d_name: NUL-terminated filename, aligned to 4.
 */
struct KernelDirentHeader {
    uint32_t d_ino;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
} __attribute__((packed));

/**
 * class FileSystem - Abstract interface for a mounted filesystem.
 */
class FileSystem {
public:
    virtual ~FileSystem() {}

    virtual File* Open(const char* path) = 0;
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
