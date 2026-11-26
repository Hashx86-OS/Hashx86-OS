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

#include <core/drivers/AudioMixer.h>
#include <debug.h>

namespace {
// About 1.5 seconds of 44.1 kHz stereo 16-bit PCM.
constexpr uint32_t kStreamRingBytes = 256u * 1024u;
}  // namespace

/**
 * AudioMixer::AudioMixer() - Bind the mixer to an audio driver.
 * @drv: Backing AudioDriver, or NULL.
 *
 * Clears the stream and deferred-free tables and allocates the mix scratch
 * buffer from the driver's buffer size.
 */
AudioMixer::AudioMixer(AudioDriver* drv) : driver(drv), mixBuffer(nullptr), bufferSize(0) {
    memset(streams, 0, sizeof(streams));
    memset(pendingFreeBuffers, 0, sizeof(pendingFreeBuffers));
    memset(pendingFreeLengths, 0, sizeof(pendingFreeLengths));

    // Ensure ownsData is cleared for older binaries.
    for (int i = 0; i < 8; i++) streams[i].ownsData = false;

    if (!driver) return;
    bufferSize = driver->GetBufferSize();
    mixBuffer = (uint8_t*)kmalloc(bufferSize);

    if (mixBuffer) {
        memset(mixBuffer, 0, bufferSize);
    }
}

void AudioMixer::SetOutputSampleRate(uint32_t rate) {
    if (driver) driver->SetSampleRate(rate);
}

/**
 * AudioMixer::CreateStream() - Allocate a user-fed 16-bit stereo ring.
 *
 * The ring is owned by the mixer and exposed to the process through the audio
 * syscalls. Only one format is accepted because the shared mix buffer is
 * signed 16-bit stereo.
 */
int32_t AudioMixer::CreateStream(uint32_t sampleRate, uint32_t channels, uint32_t bits,
                                 int32_t ownerPid) {
    if (!driver || !mixBuffer) return -1;
    if (channels != 2 || bits != 16) return -1;
    if (sampleRate < 8000 || sampleRate > 48000) return -1;

    uint8_t* ring = (uint8_t*)kmalloc(kStreamRingBytes);
    if (!ring) return -1;
    memset(ring, 0, kStreamRingBytes);

    uint32_t flags = lock();
    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (!streams[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        unlock(flags);
        kfree(ring);
        return -1;
    }

    AudioStream& st = streams[slot];
    st.data = ring;
    st.length = kStreamRingBytes;
    st.position = 0;
    st.active = true;
    st.looping = false;
    st.ownsData = false;
    st.streaming = true;
    st.primed = false;
    st.capacity = kStreamRingBytes;
    st.writePos = 0;
    st.readPos = 0;
    st.ownerPid = ownerPid;
    SetOutputSampleRate(sampleRate);
    unlock(flags);

    // Update starts playback after samples arrive through WriteStream.
    // Opening an empty stream must not prefill the DMA ring with silence.
    printf("[AUDIO] stream %d: %u Hz, owner %d\n", slot, sampleRate, ownerPid);
    return slot;
}

/**
 * AudioMixer::WriteStream() - Copy whole frames into a streaming ring.
 *
 * Return: Bytes accepted (possibly short), or -1 for a bad handle or owner.
 */
int32_t AudioMixer::WriteStream(int32_t streamId, const uint8_t* data, uint32_t length,
                                int32_t ownerPid) {
    if (streamId < 0 || streamId >= 8 || data == nullptr || length == 0) return -1;

    length &= ~3u;  // stereo 16-bit frames only
    if (length == 0) return 0;

    uint32_t flags = lock();
    AudioStream& st = streams[streamId];
    if (!st.active || !st.streaming || st.ownerPid != ownerPid) {
        unlock(flags);
        return -1;
    }

    uint32_t used = st.writePos - st.readPos;
    if (used > st.capacity) used = st.capacity;
    uint32_t available = st.capacity - used;
    if (length > available) length = available;

    uint32_t offset = st.writePos % st.capacity;
    uint32_t first = length;
    if (first > st.capacity - offset) first = st.capacity - offset;
    memcpy(st.data + offset, data, first);
    if (length > first) memcpy(st.data, data + first, length - first);
    st.writePos += length;

    unlock(flags);
    return (int32_t)length;
}

/**
 * AudioMixer::GetStreamPosition() - Return the mixer's read position.
 *
 * Return: Monotonic byte position, or -1 for a bad handle or owner.
 */
int32_t AudioMixer::GetStreamPosition(int32_t streamId, int32_t ownerPid) {
    if (streamId < 0 || streamId >= 8) return -1;

    uint32_t flags = lock();
    AudioStream& st = streams[streamId];
    int32_t position = -1;
    if (st.active && st.streaming && st.ownerPid == ownerPid) {
        position = (int32_t)st.readPos;
    }
    unlock(flags);
    return position;
}

/**
 * AudioMixer::CloseStream() - Release a streaming ring.
 *
 * Return: 0 on success, or -1 for a bad handle or owner.
 */
int32_t AudioMixer::CloseStream(int32_t streamId, int32_t ownerPid) {
    if (streamId < 0 || streamId >= 8) return -1;

    uint8_t* ring = nullptr;
    uint32_t flags = lock();
    AudioStream& st = streams[streamId];
    if (!st.active || !st.streaming || st.ownerPid != ownerPid) {
        unlock(flags);
        return -1;
    }

    ring = st.data;
    st.data = nullptr;
    st.length = 0;
    st.position = 0;
    st.active = false;
    st.looping = false;
    st.ownsData = false;
    st.streaming = false;
    st.capacity = 0;
    st.writePos = 0;
    st.readPos = 0;
    st.ownerPid = -1;
    unlock(flags);

    if (ring) kfree(ring);
    printf("[AUDIO] stream %d closed, owner %d\n", streamId, ownerPid);
    return 0;
}

/**
 * AudioMixer::CloseStreamsForPid() - Release streams left by a dying process.
 */
void AudioMixer::CloseStreamsForPid(int32_t ownerPid) {
    for (int32_t i = 0; i < 8; i++) {
        CloseStream(i, ownerPid);
    }
}

/**
 * AudioMixer::PlayBuffer() - Queue PCM data for playback on one channel.
 *
 * Copies the samples into an owned per-channel buffer (the caller's pointer
 * is never retained) and assigns them to the first inactive slot. If the
 * hardware was idle it is prefilled from all active streams and started.
 *
 * Context: Serialized by the internal IRQ lock.
 */
void AudioMixer::PlayBuffer(uint8_t* data, uint32_t length, bool loop) {
    if (!data || length == 0 || (length % 4 != 0) || !driver) return;

    uint32_t flags = lock();

    for (int i = 0; i < 8; i++) {
        if (!streams[i].active) {
            // Make an owned copy to avoid lifetime issues.
            uint8_t* copy = (uint8_t*)kmalloc(length);
            if (!copy) {
                unlock(flags);
                return;
            }  // Out of memory; fail gracefully.
            memcpy(copy, data, length);
            streams[i].data = copy;
            streams[i].ownsData = true;
            streams[i].length = length;
            streams[i].position = 0;
            streams[i].looping = loop;
            streams[i].active = true;
            streams[i].streaming = false;
            streams[i].capacity = 0;
            streams[i].writePos = 0;
            streams[i].readPos = 0;
            streams[i].ownerPid = -1;
            break;
        }
    }

    unlock(flags);
    Update();
}

/**
 * AudioMixer::Update() - Feed new mixed data to the hardware.
 *
 * Runs the deferred kfree() queue (built in IRQ context) and keeps writing
 * mixed buffers while the hardware accepts data.
 *
 * Context: Timer IRQ or task context; serialized by the internal IRQ lock.
 */
void AudioMixer::Update() {
    if (!driver || !mixBuffer) return;

    // Release buffers retired by the previous mixing pass.
    uint32_t flags = lock();
    for (int i = 0; i < 8; i++) {
        if (pendingFreeBuffers[i]) {
            kfree(pendingFreeBuffers[i]);
            pendingFreeBuffers[i] = nullptr;
            pendingFreeLengths[i] = 0;
        }
    }
    // Serialize readiness, mixing and startup against timer-driven updates.
    bool wroteData = false;
    while (driver->IsReadyForData()) {
        if (!ProcessAudio()) break;
        wroteData = true;
    }
    if (wroteData && !driver->IsPlaying()) driver->Start();
    unlock(flags);
}

/**
 * AudioMixer::ProcessAudio() - Mix all active streams into one hardware frame.
 *
 * Zeroes the scratch buffer, sums every active stream (clipping at 16-bit
 * bounds), and writes the result to the hardware. Streams that reach their
 * end: looping ones restart, others are deactivated and their owned buffers
 * are queued for a deferred free because this runs in IRQ context.
 *
 * Context: IRQ context; serialized by the internal IRQ lock.
 */
bool AudioMixer::ProcessAudio() {
    if (!driver || !mixBuffer) return false;

    uint32_t flags = lock();

    // Mix only the duration backed by real samples. Empty streams must not
    // commit silence ahead of data that the producer will write next tick.
    uint32_t bytesToMix = 0;
    AudioStream savedStreams[8];
    memcpy(savedStreams, streams, sizeof(streams));
    for (int s = 0; s < 8; s++) {
        AudioStream& st = streams[s];
        if (!st.active) continue;
        uint32_t available = st.streaming ? (uint32_t)(st.writePos - st.readPos)
                                          : (st.looping ? bufferSize : st.length - st.position);
        // Collect two pages before the first submission, giving the producer
        // about 46 ms of scheduling slack at 44.1 kHz. Later short writes
        // can use short descriptors without inserting silence.
        if (st.streaming && !st.primed) {
            if (available < bufferSize * 2u) continue;
            st.primed = true;
        }
        if (available > bytesToMix) bytesToMix = available;
    }
    if (bytesToMix > bufferSize) bytesToMix = bufferSize;
    bytesToMix &= ~3u;
    if (bytesToMix == 0) {
        unlock(flags);
        return false;
    }
    memset(mixBuffer, 0, bytesToMix);

    int16_t* out = (int16_t*)mixBuffer;
    uint32_t samples = bytesToMix / sizeof(int16_t);

    for (int s = 0; s < 8; s++) {
        AudioStream& st = streams[s];
        if (!st.active) continue;

        if (st.streaming) {
            if (!st.primed) continue;
            // Stereo 16-bit ring: advance only when a complete frame exists,
            // so an underrun leaves the read position parked until data
            // arrives instead of skipping samples.
            uint32_t frames = samples / 2;
            for (uint32_t f = 0; f < frames; f++) {
                int16_t left = 0;
                int16_t right = 0;
                if ((uint32_t)(st.writePos - st.readPos) >= 4) {
                    uint32_t sampleIndex = (st.readPos % st.capacity) / sizeof(int16_t);
                    left = ((int16_t*)st.data)[sampleIndex];
                    right = ((int16_t*)st.data)[sampleIndex + 1];
                    st.readPos += 4;
                }

                int32_t mixedLeft = out[f * 2] + left;
                int32_t mixedRight = out[f * 2 + 1] + right;
                if (mixedLeft > 32767) mixedLeft = 32767;
                if (mixedLeft < -32768) mixedLeft = -32768;
                if (mixedRight > 32767) mixedRight = 32767;
                if (mixedRight < -32768) mixedRight = -32768;
                out[f * 2] = (int16_t)mixedLeft;
                out[f * 2 + 1] = (int16_t)mixedRight;
            }
            continue;
        }

        for (uint32_t i = 0; i < samples; i++) {
            if (st.position >= st.length) {
                if (st.looping) {
                    st.position = 0;
                } else {
                    st.active = false;
                    break;
                }
            }

            uint32_t sampleIndex = st.position / sizeof(int16_t);
            int16_t sample = ((int16_t*)st.data)[sampleIndex];
            int32_t mixed = out[i] + sample;

            // Clamp the sum to the int16 range.
            if (mixed > 32767) mixed = 32767;
            if (mixed < -32768) mixed = -32768;

            out[i] = (int16_t)mixed;
            st.position += sizeof(int16_t);
        }

        if (!st.looping && st.position >= st.length) st.active = false;
    }

    // A rejected write must not lose source samples or retire their buffers.
    if (driver->WriteData(mixBuffer, bytesToMix) != bytesToMix) {
        memcpy(streams, savedStreams, sizeof(streams));
        unlock(flags);
        return false;
    }

    for (int s = 0; s < 8; s++) {
        AudioStream& st = streams[s];
        // Retire only after the DMA write succeeds; Update frees it on
        // the next pass.
        if (!st.active && st.ownsData && st.data) {
            pendingFreeBuffers[s] = st.data;
            pendingFreeLengths[s] = st.length;
            st.data = nullptr;
            st.length = 0;
            st.position = 0;
            st.ownsData = false;
        }
    }

    unlock(flags);
    return true;
}
