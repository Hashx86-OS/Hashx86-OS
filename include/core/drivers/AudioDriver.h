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

#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include <core/driver.h>
#include <types.h>

typedef void (*AudioCallback)(void* context);

/**
 * class AudioDriver - Abstract base for an output audio device.
 * @sampleRate: Initial sample rate in Hz.
 * @channels: Number of audio channels.
 * @bitsPerSample: Bits per sample.
 * @isPlaying: Whether playback is active.
 * @masterVolume: Master volume level (0-100).
 * @refillCallback: Callback invoked when the hardware needs more data.
 * @callbackContext: Opaque context passed to the refill callback.
 */
class AudioDriver {
protected:
    uint32_t sampleRate;
    uint8_t channels;
    uint8_t bitsPerSample;
    bool isPlaying;
    uint8_t masterVolume;
    AudioCallback refillCallback;
    void* callbackContext;

public:
    /**
     * AudioDriver() - Initialize an audio driver with default settings.
     */
    AudioDriver() {
        this->sampleRate = 44100;
        this->channels = 2;
        this->bitsPerSample = 16;
        this->isPlaying = false;
        this->masterVolume = 100;
        this->refillCallback = nullptr;
        this->callbackContext = nullptr;
    }

    virtual ~AudioDriver() {}

    /**
     * SetRefillCallback() - Register a callback for refill requests.
     * @cb: Callback to invoke, or null to clear.
     * @ctx: Opaque context passed back to the callback.
     */
    void SetRefillCallback(AudioCallback cb, void* ctx) {
        this->refillCallback = cb;
        this->callbackContext = ctx;
    }

    /**
     * SetFormat() - Configure the output sample format.
     * @sampleRate: Sample rate in Hz.
     * @channels: Number of channels.
     * @bits: Bits per sample.
     */
    virtual void SetFormat(uint32_t sampleRate, uint8_t channels, uint8_t bits) = 0;

    /**
     * SetSampleRate() - Change only the sample rate.
     * @newRate: New sample rate in Hz.
     *
     * No-op if the rate is already active.
     */
    virtual void SetSampleRate(uint32_t newRate) {
        if (newRate == this->sampleRate) return;
        SetFormat(newRate, this->channels, this->bitsPerSample);
    }

    virtual uint32_t GetBufferSize() = 0;
    virtual uint32_t WriteData(uint8_t* buffer, uint32_t size) = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;

    /**
     * IsReadyForData() - Check whether the device can accept more data.
     *
     * Return: true when the device is ready.
     */
    virtual bool IsReadyForData() {
        return true;
    }

    /**
     * SetVolume() - Set the master volume.
     * @vol: Volume level, clamped to 0-100.
     */
    virtual void SetVolume(uint8_t vol) {
        if (vol > 100) vol = 100;
        this->masterVolume = vol;
        ApplyHardwareVolume();
    }

    uint32_t GetSampleRate() {
        return sampleRate;
    }
    uint8_t GetChannels() {
        return channels;
    }
    bool IsPlaying() {
        return isPlaying;
    }

protected:
    /**
     * NotifyRefillNeeded() - Invoke the registered refill callback.
     */
    void NotifyRefillNeeded() {
        if (refillCallback) {
            refillCallback(callbackContext);
        }
    }
    virtual void ApplyHardwareVolume() = 0;
};

#endif
