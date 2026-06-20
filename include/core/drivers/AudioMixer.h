#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#include <core/drivers/AudioDriver.h>
#include <core/memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

class AudioDriver;

struct AudioStream {
    uint8_t* data;
    uint32_t length;
    uint32_t position;
    bool active;
    bool looping;
    bool ownsData;
};

class AudioMixer {
private:
    AudioDriver* driver;
    AudioStream streams[8];

    uint8_t* mixBuffer;
    uint32_t bufferSize;

    // Per-slot IRQ-safe deferred free: ProcessAudio moves st.data into
    // pendingFreeBuffers[slot], Update performs the actual kfree in task context
    uint8_t* pendingFreeBuffers[8];
    uint32_t pendingFreeLengths[8];

    // Disable interrupts, return previous EFLAGS (bit 9 = IF)
    static inline uint32_t lock() {
        uint32_t eflags;
        asm volatile("pushf; pop %0; cli" : "=r"(eflags));
        return eflags;
    }
    // Restore interrupts if they were enabled
    static inline void unlock(uint32_t eflags) {
        if (eflags & 0x200) asm volatile("sti");
    }

public:
    explicit AudioMixer(AudioDriver* drv);

    void PlayBuffer(uint8_t* data, uint32_t length, bool loop);

    void SetOutputSampleRate(uint32_t rate);

    void Update();

private:
    void ProcessAudio();
};

#endif  // AUDIO_MIXER_H
