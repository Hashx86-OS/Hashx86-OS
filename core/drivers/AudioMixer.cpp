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
 * AudioMixer::PlayBuffer() - Queue PCM data for playback on one channel.
 *
 * Copies the samples into an owned per-channel buffer (the caller's pointer
 * is never retained) and assigns them to the first inactive slot. If the
 * hardware was idle it is prefilled from all active streams and started.
 *
 * Context: Serialized by the internal IRQ lock.
 */
void AudioMixer::PlayBuffer(uint8_t* data, uint32_t length, bool loop) {
    if (!data || length == 0 || (length % 2 != 0) || !driver) return;

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
            break;
        }
    }

    bool wasPlaying = driver->IsPlaying();
    unlock(flags);

    if (!wasPlaying) {
        // Fill ALL available hardware buffers before starting. Only enter the
        // prefill loop if mixBuffer was allocated; otherwise ProcessAudio is a
        // no-op and would spin forever.
        if (mixBuffer) {
            while (driver->IsReadyForData()) {
                ProcessAudio();
            }
        }
        driver->Start();
    }
}

/**
 * AudioMixer::Update() - Feed new mixed data to the hardware.
 *
 * Runs the deferred kfree() queue (built in IRQ context) and keeps writing
 * mixed buffers while the hardware accepts data.
 *
 * Context: Task context; serialized by the internal IRQ lock.
 */
void AudioMixer::Update() {
    if (!driver || !mixBuffer) return;

    // Perform deferred frees queued from IRQ context by ProcessAudio().
    uint32_t flags = lock();
    for (int i = 0; i < 8; i++) {
        if (pendingFreeBuffers[i]) {
            kfree(pendingFreeBuffers[i]);
            pendingFreeBuffers[i] = nullptr;
            pendingFreeLengths[i] = 0;
        }
    }
    unlock(flags);

    // Keep filling as long as the hardware has space.
    while (driver->IsReadyForData()) {
        ProcessAudio();
    }
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
void AudioMixer::ProcessAudio() {
    if (!driver || !mixBuffer) return;

    uint32_t flags = lock();

    memset(mixBuffer, 0, bufferSize);

    int16_t* out = (int16_t*)mixBuffer;
    uint32_t samples = bufferSize / sizeof(int16_t);
    bool activeStreams = false;

    for (int s = 0; s < 8; s++) {
        AudioStream& st = streams[s];
        if (!st.active) continue;

        activeStreams = true;
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

        // A just-finished owned buffer cannot be freed here (IRQ context), so
        // defer it to task context (Update).
        if (!st.active && st.ownsData && st.data) {
            pendingFreeBuffers[s] = st.data;
            pendingFreeLengths[s] = st.length;
            st.data = nullptr;
            st.length = 0;
            st.position = 0;
            st.ownsData = false;
        }
    }

    driver->WriteData(mixBuffer, bufferSize);

    unlock(flags);
}
