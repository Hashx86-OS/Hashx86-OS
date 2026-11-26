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
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

uint64_t timerTicks = 0;

void* kmalloc(size_t n) {
    return malloc(n);
}
void kfree(void* p) {
    free(p);
}

class Output : public AudioDriver {
public:
    std::vector<uint8_t> pcm;
    unsigned slots = 32;
    bool reject = false;
    uint32_t GetBufferSize() override {
        return 4096;
    }
    bool IsReadyForData() override {
        return slots != 0;
    }
    uint32_t WriteData(uint8_t* p, uint32_t n) override {
        if (reject || !slots) return 0;
        assert(n <= 4096 && n % 4 == 0);
        --slots;
        pcm.insert(pcm.end(), p, p + n);
        return n;
    }
    void Start() override {
        assert(!pcm.empty());
        isPlaying = true;
    }
    void Stop() override {
        isPlaying = false;
    }
    void SetFormat(uint32_t r, uint8_t, uint8_t) override {
        sampleRate = r;
    }
    void ApplyHardwareVolume() override {}
};

int main() {
    Output out;
    AudioMixer mixer(&out);
    mixer.Update();
    assert(out.pcm.empty() && !out.IsPlaying());
    int id = mixer.CreateStream(44100, 2, 16, 7);
    assert(id >= 0);
    mixer.Update();
    assert(out.pcm.empty() && !out.IsPlaying());

    std::vector<uint8_t> expected(8192, 0x12);
    assert(mixer.WriteStream(id, expected.data(), 4096, 7) == 4096);
    mixer.Update();
    assert(out.pcm.empty());  // Wait for the initial prebuffer.
    assert(mixer.WriteStream(id, expected.data() + 4096, 4096, 7) == 4096);
    out.reject = true;
    mixer.Update();
    assert(mixer.GetStreamPosition(id, 7) == 0 && !out.IsPlaying());
    out.reject = false;
    mixer.Update();
    assert(out.IsPlaying() && out.pcm == expected);

    // Hundreds of intermittent short submissions cross the software ring
    // boundary. Output must be byte-exact with no inserted silent padding.
    for (int i = 0; i < 400; ++i) {
        std::vector<uint8_t> chunk(1764, uint8_t(i));  // 10 ms at 44.1 kHz.
        assert(mixer.WriteStream(id, chunk.data(), chunk.size(), 7) == 1764);
        expected.insert(expected.end(), chunk.begin(), chunk.end());
        out.slots = 32;
        mixer.Update();
        mixer.Update();  // Empty update cannot append silence.
        assert(out.pcm == expected);
    }
    assert(mixer.GetStreamPosition(id, 7) == int(expected.size()));
    assert(mixer.CloseStream(id, 7) == 0);

    // One-shot tails and rejected writes also preserve every frame.
    Output one;
    AudioMixer wav(&one);
    std::vector<uint8_t> sound(5000, 0x34);
    one.reject = true;
    wav.PlayBuffer(sound.data(), sound.size(), false);
    assert(one.pcm.empty());
    one.reject = false;
    wav.Update();
    wav.Update();
    assert(one.pcm == sound);
    Output shortOut;
    AudioMixer shortMixer(&shortOut);
    int shortId = shortMixer.CreateStream(44100, 2, 16, 7);
    uint8_t shortPcm[4] = {1, 2, 3, 4};
    assert(shortMixer.WriteStream(shortId, shortPcm, sizeof(shortPcm), 7) == 4);
    shortMixer.Update();
    assert(shortOut.pcm.empty());
    timerTicks = 99;
    shortMixer.Update();
    assert(shortOut.pcm.empty());
    timerTicks = 100;
    shortMixer.Update();
    assert(shortOut.pcm == std::vector<uint8_t>(shortPcm, shortPcm + 4));
    assert(shortMixer.CreateStream(48000, 2, 16, 7) == -1);
    assert(shortOut.GetSampleRate() == 44100);
    int compatible = shortMixer.CreateStream(44100, 2, 16, 7);
    assert(compatible >= 0 && shortOut.IsPlaying());
    assert(shortMixer.CloseStream(compatible, 7) == 0);
    shortMixer.streams[shortId].readPos = 0x7FFFFFFCu;
    shortMixer.streams[shortId].writePos = 0x7FFFFFFCu;
    assert(shortMixer.GetStreamPosition(shortId, 7) == 0x7FFFFFFC);
    shortMixer.streams[shortId].readPos += 4;
    shortMixer.streams[shortId].writePos += 4;
    assert(shortMixer.GetStreamPosition(shortId, 7) == 0);
    assert(shortMixer.CloseStream(shortId, 7) == 0);
    Output closeOut;
    AudioMixer closeMixer(&closeOut);
    int closeId = closeMixer.CreateStream(44100, 2, 16, 7);
    assert(closeMixer.WriteStream(closeId, shortPcm, 4, 7) == 4);
    assert(closeMixer.CloseStream(closeId, 7) == 0);
    assert(closeMixer.WriteStream(closeId, shortPcm, 4, 7) == -1);
    closeMixer.Update();
    assert(closeOut.pcm == std::vector<uint8_t>(shortPcm, shortPcm + 4));
    closeMixer.Update();  // Deferred ring release.
    puts("audio mixer regression tests passed");
}
