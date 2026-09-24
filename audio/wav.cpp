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

#define KDBG_COMPONENT "WAV"
#include <audio/wav.h>
#include <core/globals.h>

// External reference to the mixer created in kernel.cpp.
extern AudioMixer* g_AudioMixer;

/**
 * Wav::Wav() - Construct a WAV player from an open file.
 * @file: Open file handle containing the WAV data.
 */
Wav::Wav(File* file) {
    this->valid = false;
    this->buffer = 0;
    this->length = 0;
    this->sampleRate = 0;
    Load(file);
}

/**
 * Wav::Wav() - Construct a WAV player by opening a file from the boot partition.
 * @path: Path of the WAV file on the boot partition.
 */
Wav::Wav(const char* path) {
    this->valid = false;
    this->buffer = 0;
    this->length = 0;
    this->sampleRate = 0;

    // Get the active filesystem.
    if (!g_bootPartition) {
        KDBG1("Error: File system not ready.");
        return;
    }

    FileSystem* fs = g_bootPartition;
    File* file = fs->Open(path);

    if (file == 0) {
        KDBG1("Error: File not found %s", path);
        return;
    }

    if (file->size == 0) {
        KDBG1("Error: File is empty %s", path);
        file->Close();
        delete file;
        return;
    }

    Load(file);
    file->Close();
    delete file;
}

/**
 * Wav::~Wav() - Release the allocated sample buffer.
 */
Wav::~Wav() {
    if (buffer) {
        kfree(buffer);
        buffer = 0;
    }
}

/**
 * Wav::Load() - Parse a WAV file into the audio buffer.
 * @file: Open file handle positioned at the start of the RIFF data.
 *
 * Reads the RIFF/WAVE header, walks the chunk list for valid "fmt " and
 * "data" chunks, then allocates a kernel buffer for the PCM samples.
 */
void Wav::Load(File* file) {
    if (!file) return;

    // Clear any prior state before reload.
    if (buffer) {
        kfree(buffer);
        buffer = nullptr;
    }
    length = 0;
    valid = false;
    sampleRate = 0;
    channels = 0;
    bitsPerSample = 0;

    WavHeader header;
    if (file->Read((uint8_t*)&header, sizeof(WavHeader)) != sizeof(WavHeader)) {
        KDBG1("Error: Header read failed.");
        return;
    }

    // Validate the RIFF and WAVE signatures.
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        KDBG1("Error: Invalid RIFF/WAVE signature.");
        return;
    }

    WavFmt fmt;
    bool fmtFound = false;
    bool dataFound = false;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;

    // Walk the chunk list.
    while (file->position < file->size) {
        ChunkHeader chunk;
        if (file->Read((uint8_t*)&chunk, sizeof(ChunkHeader)) != sizeof(ChunkHeader)) break;

        uint32_t chunkStart = file->position;

        // "fmt " chunk
        if (strncmp(chunk.id, "fmt ", 4) == 0) {
            if (chunk.size < sizeof(WavFmt)) {
                KDBG1("Error: FMT chunk too small.");
                return;
            }
            // Only accept an exact-size read of the fmt chunk.
            if (file->Read((uint8_t*)&fmt, sizeof(WavFmt)) != sizeof(WavFmt)) {
                KDBG1("Error: FMT chunk read failed.");
                return;
            }
            fmtFound = true;

            // Skip any extra bytes when the fmt chunk is larger than WavFmt.
            if (chunk.size > sizeof(WavFmt)) {
                file->Seek(chunkStart + chunk.size);
            }
        }
        // "data" chunk
        else if (strncmp(chunk.id, "data", 4) == 0) {
            // Compute the bytes remaining from the current position.
            uint32_t available = file->size - file->position;
            dataSize = (chunk.size < available) ? chunk.size : available;
            if (dataSize > 0) {
                dataOffset = file->position;
                dataFound = true;
            } else {
                KDBG2("Warning: data chunk size is zero or beyond file bounds, skipping.");
            }
            break;
        }
        // Unknown chunk: skip it.
        else {
            file->Seek(chunkStart + chunk.size);
        }

        // Skip the padding byte when the chunk size is odd.
        if (chunk.size % 2 != 0) {
            file->Seek(file->position + 1);
        }
    }

    if (!fmtFound || !dataFound) {
        KDBG1("Error: Missing FMT or DATA chunk.");
        return;
    }

    // Both required chunks are present; the exact-size reads validated them.
    // Enforce 16-bit PCM for now, per the mixer's current limits.
    if (fmt.audioFormat != 1) {
        KDBG1("Error: Not PCM format (Format=%d).", fmt.audioFormat);
        return;
    }

    // Store the sample properties.
    this->sampleRate = fmt.sampleRate;
    this->channels = fmt.numChannels;
    this->bitsPerSample = fmt.bitsPerSample;
    this->length = dataSize;

    // Cap the allocation size for sanity.
    if (this->length == 0 || this->length > 64 * 1024 * 1024) {
        KDBG1("Error: Invalid data size %d (max %d)", this->length, 64 * 1024 * 1024);
        return;
    }

    // Allocate the sample buffer.
    this->buffer = (uint8_t*)kmalloc(this->length);
    if (!this->buffer) {
        KDBG1("Error: Out of memory (Size=%d)", this->length);
        return;
    }

    // Read the sample data.
    file->Seek(dataOffset);
    uint32_t read = file->Read(this->buffer, this->length);

    if (read != this->length) {
        KDBG2("Warning: Read mismatch (%d vs %d); truncating to %d bytes", (int32_t)read,
              (int32_t)this->length, (int32_t)read);
        this->length = read;
        // If nothing was read, fail validation.
        if (read == 0) {
            KDBG1("Error: Data chunk read returned 0 bytes.");
            kfree(this->buffer);
            this->buffer = nullptr;
            return;
        }
    }

    this->valid = true;
    KDBG2("Loaded: %d Hz, %d-bit, %s (%d bytes)", sampleRate, bitsPerSample,
          (channels == 2) ? "Stereo" : "Mono", length);
}

/**
 * Wav::Play() - Send the loaded audio to the global mixer.
 * @loop: When true, the mixer repeats the sample until stopped.
 */
void Wav::Play(bool loop) {
    if (!valid || !buffer || !g_AudioMixer) return;

    // Configure the hardware sample rate.
    g_AudioMixer->SetOutputSampleRate(this->sampleRate);

    // Play the buffer through the mixer.
    g_AudioMixer->PlayBuffer(this->buffer, this->length, loop);
}
