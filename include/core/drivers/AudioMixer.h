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
 * struct AudioStream - A single loop/one-shot or streaming audio source.
 * @data: PCM sample data.
 * @length: Length of the data in bytes.
 * @position: Current read offset into one-shot data.
 * @active: Whether the stream is playing.
 * @looping: Whether a one-shot stream restarts at the end.
 * @ownsData: Whether the mixer owns (and will free) the buffer.
 * @streaming: Whether this slot reads from a user-fed ring buffer.
 * @capacity: Ring buffer size in bytes for a streaming slot.
 * @writePos: Monotonic bytes written by the user program.
 * @readPos: Monotonic bytes consumed by the mixer.
 * @ownerPid: PID allowed to write/query the streaming slot, or -1.
 */
struct AudioStream {
    uint8_t* data;
    uint32_t length;
    uint32_t position;
    bool active;
    bool looping;
    bool ownsData;
    bool streaming;
    bool closing;  // The handle is closed; queued source frames still drain.
    bool primed;   // Initial streaming prebuffer has been collected.
    uint32_t startupTick;
    bool startupPending;
    uint32_t capacity;
    volatile uint32_t writePos;
    volatile uint32_t readPos;
    int32_t ownerPid;
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
        asm volatile("pushf; pop %0; cli" : "=r"(eflags)::"memory");
        return eflags;
    }
    // Restore interrupts if they were enabled.
    static inline void unlock(uint32_t eflags) {
        if (eflags & 0x200) asm volatile("sti" ::: "memory");
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
     * CreateStream() - Allocate a 16-bit stereo streaming ring.
     * @sampleRate: Requested output rate in Hz.
     * @channels: Channel count; must be 2.
     * @bits: Sample width; must be 16.
     * @ownerPid: Process that owns the stream.
     *
     * Return: Slot index in [0, 8), or -1 when audio is unavailable.
     */
    int32_t CreateStream(uint32_t sampleRate, uint32_t channels, uint32_t bits, int32_t ownerPid);

    /**
     * WriteStream() - Copy PCM frames into a streaming ring.
     *
     * The write is short when the ring is full; callers should retry later.
     *
     * Return: Bytes accepted, or -1 for a bad handle/owner.
     */
    int32_t WriteStream(int32_t streamId, const uint8_t* data, uint32_t length, int32_t ownerPid);

    /**
     * GetStreamPosition() - Bytes consumed from a streaming ring.
     *
     * Return: Frame-aligned byte position modulo 2^31 (always nonnegative), or -1 for a bad
     * handle/owner.
     */
    int32_t GetStreamPosition(int32_t streamId, int32_t ownerPid);

    /**
     * CloseStream() - Close the handle and drain accepted frames before freeing the ring.
     *
     * Return: 0 on success, -1 for a bad handle/owner.
     */
    int32_t CloseStream(int32_t streamId, int32_t ownerPid);

    /**
     * CloseStreamsForPid() - Release every stream owned by a dying process.
     */
    void CloseStreamsForPid(int32_t ownerPid);

    /**
     * Update() - Mix one output chunk and push it to the driver.
     *
     * Called by the timer IRQ or task context; preserves interrupt state.
     */
    void Update();

private:
    /**
     * ProcessAudio() - Queue available samples; false when no progress is possible.
     */
    bool ProcessAudio();
};

#endif  // AUDIO_MIXER_H
