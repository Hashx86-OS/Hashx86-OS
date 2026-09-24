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

#ifndef WAV_H
#define WAV_H

#include <core/drivers/AudioMixer.h>
#include <core/filesystem/File.h>
#include <core/filesystem/FileSystem.h>
#include <core/filesystem/msdospart.h>
#include <core/memory.h>
#include <debug.h>
#include <stdint.h>
#include <string.h>

// WAV file structures.

/**
 * struct WavHeader - Fixed 12-byte RIFF/WAVE file header.
 * @riff: "RIFF" signature.
 * @overallSize: File size minus the first 8 bytes.
 * @wave: "WAVE" signature.
 */
struct WavHeader {
    char riff[4];          // "RIFF".
    uint32_t overallSize;  // File size minus 8.
    char wave[4];          // "WAVE".
};

/**
 * struct ChunkHeader - 8-byte RIFF chunk header.
 * @id: Four-character chunk identifier.
 * @size: Chunk payload size in bytes.
 */
struct ChunkHeader {
    char id[4];
    uint32_t size;
};

/**
 * struct WavFmt - Contents of the "fmt " chunk.
 * @audioFormat: Audio format; 1 is PCM.
 * @numChannels: Channel count; 1 is mono, 2 is stereo.
 * @sampleRate: Sample rate in Hertz.
 * @byteRate: Average bytes per second.
 * @blockAlign: Bytes per sample frame.
 * @bitsPerSample: Bits per sample.
 */
struct WavFmt {
    uint16_t audioFormat;  // 1 = PCM.
    uint16_t numChannels;  // 1 = Mono, 2 = Stereo.
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

/**
 * class Wav - Loader and player for 16-bit PCM WAV files.
 * @buffer: Allocated sample data.
 * @length: Sample data size in bytes.
 * @sampleRate: Sample rate in Hertz.
 * @channels: Channel count.
 * @bitsPerSample: Bits per sample.
 * @valid: Whether a WAV file was loaded successfully.
 */
class Wav {
public:
    // Audio data.
    uint8_t* buffer;
    uint32_t length;

    // Audio properties.
    uint32_t sampleRate;
    uint8_t channels;
    uint8_t bitsPerSample;
    bool valid;

public:
    // Constructors mirroring the Bitmap pattern.
    /**
     * Wav::Wav() - Construct a WAV player from an open file.
     * @file: Open file handle containing the WAV data.
     */
    Wav(File* file);
    /**
     * Wav::Wav() - Construct a WAV player by opening a path on the boot partition.
     * @path: Path of the WAV file.
     */
    Wav(const char* path);
    /**
     * Wav::~Wav() - Release the allocated sample buffer.
     */
    ~Wav();

    /**
     * Play() - Send the loaded audio to the global mixer.
     * @loop: When true, the mixer repeats the sample until stopped.
     */
    void Play(bool loop = false);

private:
    /**
     * Load() - Parse a WAV file into the audio buffer.
     * @file: Open file handle positioned at the start of the RIFF data.
     */
    void Load(File* file);
};

#endif
