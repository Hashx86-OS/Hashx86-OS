/**
 * @file        AudioMixer.cpp
 * @brief       Audio Mixer for #x86
 *
 * @date        29/01/2026
 * @version     1.0.0-beta
 */

#include <core/drivers/AudioMixer.h>

AudioMixer::AudioMixer(AudioDriver* drv) : driver(drv), mixBuffer(nullptr), bufferSize(0),
    pendingFreeBuffer(nullptr), pendingFreeLength(0) {
    memset(streams, 0, sizeof(streams));

    // Ensure ownsData is cleared for safety on older binaries
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

void AudioMixer::PlayBuffer(uint8_t* data, uint32_t length, bool loop) {
    if (!data || length == 0 || (length % 2 != 0) || !driver) return;

    uint32_t flags = lock();

    for (int i = 0; i < 8; i++) {
        if (!streams[i].active) {
            // Make an owned copy of the data to avoid lifetime issues
            uint8_t* copy = (uint8_t*)kmalloc(length);
            if (!copy) { unlock(flags); return; }  // Out of memory; fail gracefully
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
        // Fill ALL available hardware buffers before starting.
        // Only enter the prefill loop if mixBuffer was allocated;
        // otherwise ProcessAudio is a no-op and would spin forever.
        if (mixBuffer) {
            while (driver->IsReadyForData()) {
                ProcessAudio();
            }
        }
        driver->Start();
    }
}

void AudioMixer::Update() {
    if (!driver || !mixBuffer) return;

    // Perform deferred free from IRQ context (ProcessAudio) under lock
    uint32_t flags = lock();
    if (pendingFreeBuffer) {
        kfree(pendingFreeBuffer);
        pendingFreeBuffer = nullptr;
        pendingFreeLength = 0;
    }
    unlock(flags);

    // Keep filling as long as hardware has space
    while (driver->IsReadyForData()) {
        ProcessAudio();
    }
}

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

            // Hard Clipping prevention
            if (mixed > 32767) mixed = 32767;
            if (mixed < -32768) mixed = -32768;

            out[i] = (int16_t)mixed;
            st.position += sizeof(int16_t);
        }

        // If the stream was deactivated and we own the buffer, defer the free
        // to task context (Update) since we are in IRQ context here
        if (!st.active && st.ownsData && st.data) {
            // Chain deferred frees: if a previous buffer is still pending, free it now
            // (only one buffer should deactivate between Update calls)
            if (pendingFreeBuffer) {
                kfree(pendingFreeBuffer);
            }
            pendingFreeBuffer = st.data;
            pendingFreeLength = st.length;
            st.data = nullptr;
            st.length = 0;
            st.position = 0;
            st.ownsData = false;
        }
    }

    unlock(flags);

    driver->WriteData(mixBuffer, bufferSize);
}
