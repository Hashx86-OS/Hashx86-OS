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

#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#include <core/drivers/AudioDriver.h>
#include <core/memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

class AudioDriver;

/**
 * struct AudioStream - A single loop/one-shot audio source mixed by AudioMixer.
 * @data: PCM sample data.
 * @length: Length of the data in bytes.
 * @position: Current read offset into the data.
 * @active: Whether the stream is playing.
 * @looping: Whether the stream restarts at the end.
 * @ownsData: Whether the mixer owns (and will free) the buffer.
 */
struct AudioStream {
    uint8_t* data;
    uint32_t length;
    uint32_t position;
    bool active;
    bool looping;
    bool ownsData;
};

/**
 * class AudioMixer - Software mixer that merges up to 8 streams into one driver.
 * @driver: Underlying output driver.
 * @streams: Active audio streams.
 * @mixBuffer: Scratch buffer for the mixed output.
 * @bufferSize: Size of the mix buffer.
 * @pendingFreeBuffers: Buffers deferred for free in task context.
 * @pendingFreeLengths: Sizes of the deferred free buffers.
 */
class AudioMixer {
private:
    AudioDriver* driver;
    AudioStream streams[8];

    uint8_t* mixBuffer;
    uint32_t bufferSize;

    // Per-slot IRQ-safe deferred free: ProcessAudio moves st.data into
    // pendingFreeBuffers[slot], Update performs the actual kfree in task context.
    uint8_t* pendingFreeBuffers[8];
    uint32_t pendingFreeLengths[8];

    // Disable interrupts and return the previous EFLAGS (bit 9 = IF).
    static inline uint32_t lock() {
        uint32_t eflags;
        asm volatile("pushf; pop %0; cli" : "=r"(eflags));
        return eflags;
    }
    // Restore interrupts if they were enabled.
    static inline void unlock(uint32_t eflags) {
        if (eflags & 0x200) asm volatile("sti");
    }

public:
    /**
     * AudioMixer() - Construct a mixer over an output driver.
     * @drv: Audio driver that receives the mixed output.
     */
    explicit AudioMixer(AudioDriver* drv);

    /**
     * PlayBuffer() - Start playing a PCM buffer through the mixer.
     * @data: Sample data; ownership transfers to the mixer when not looping.
     * @length: Buffer size in bytes.
     * @loop: Whether to loop the buffer.
     */
    void PlayBuffer(uint8_t* data, uint32_t length, bool loop);

    /**
     * SetOutputSampleRate() - Change the mixer and driver sample rate.
     * @rate: New sample rate in Hz.
     */
    void SetOutputSampleRate(uint32_t rate);

    /**
     * Update() - Mix one output chunk and push it to the driver.
     *
     * Must be called from task context with interrupts enabled.
     */
    void Update();

private:
    /**
     * ProcessAudio() - Mix active streams into the output buffer.
     */
    void ProcessAudio();
};

#endif  // AUDIO_MIXER_H
