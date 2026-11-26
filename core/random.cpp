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

#include <core/Iguard.h>
#include <core/random.h>
#include <string.h>

extern uint64_t timerTicks;
namespace {
uint32_t stream[16];
uint32_t isnKey[8];
uint32_t isnCounter;
bool ready;

uint32_t Rotate(uint32_t x, unsigned n) {
    return (x << n) | (x >> (32 - n));
}
void Quarter(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    a += b;
    d = Rotate(d ^ a, 16);
    c += d;
    b = Rotate(b ^ c, 12);
    a += b;
    d = Rotate(d ^ a, 8);
    c += d;
    b = Rotate(b ^ c, 7);
}
// ChaCha20 block function: RFC 8439 section 2.3. ISNs use an independent key.
void Block(const uint32_t input[16], uint32_t output[16]) {
    memcpy(output, input, 64);
    for (int i = 0; i < 10; i++) {
        Quarter(output[0], output[4], output[8], output[12]);
        Quarter(output[1], output[5], output[9], output[13]);
        Quarter(output[2], output[6], output[10], output[14]);
        Quarter(output[3], output[7], output[11], output[15]);
        Quarter(output[0], output[5], output[10], output[15]);
        Quarter(output[1], output[6], output[11], output[12]);
        Quarter(output[2], output[7], output[8], output[13]);
        Quarter(output[3], output[4], output[9], output[14]);
    }
    for (int i = 0; i < 16; i++) output[i] += input[i];
}
bool HardwareWord(uint32_t* out) {
    uint32_t a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    if (!(c & (1u << 30))) return false;
    for (int attempt = 0; attempt < 64; attempt++) {
        unsigned char ok;
        asm volatile("rdrand %0; setc %1" : "=r"(*out), "=qm"(ok)::"cc");
        if (ok) return true;
        asm volatile("pause");
    }
    return false;
}
}  // namespace

bool RandomInitialize() {
    InterruptGuard guard;
    if (ready) return true;
    uint32_t seed[19];
    for (int i = 0; i < 19; i++) {
        if (!HardwareWord(&seed[i])) {
            memset(seed, 0, sizeof(seed));
            return false;
        }
    }
    stream[0] = 0x61707865;
    stream[1] = 0x3320646e;
    stream[2] = 0x79622d32;
    stream[3] = 0x6b206574;
    memcpy(stream + 4, seed, 32);
    stream[12] = 0;
    memcpy(stream + 13, seed + 8, 12);
    memcpy(isnKey, seed + 11, 32);
    memset(seed, 0, sizeof(seed));
    ready = true;
    return true;
}
bool RandomBytes(void* output, size_t length) {
    InterruptGuard guard;
    if (!RandomInitialize()) return false;
    uint8_t* bytes = (uint8_t*)output;
    while (length) {
        uint32_t block[16];
        Block(stream, block);
        if (++stream[12] == 0 && ++stream[13] == 0 && ++stream[14] == 0) ++stream[15];
        size_t n = length < 64 ? length : 64;
        memcpy(bytes, block, n);
        memset(block, 0, sizeof(block));
        bytes += n;
        length -= n;
    }
    return true;
}
uint32_t RandomTcpIsn(uint32_t local, uint16_t lp, uint32_t remote, uint16_t rp) {
    InterruptGuard guard;
    if (!RandomInitialize()) {
        for (;;) asm volatile("cli; hlt");
    }
    uint32_t input[16], output[16];
    memcpy(input, stream, 16);
    memcpy(input + 4, isnKey, 32);
    input[12] = 0;
    input[13] = local;
    input[14] = remote;
    input[15] = ((uint32_t)lp << 16) | rp;
    Block(input, output);
    uint32_t result = output[0] + (uint32_t)timerTicks * 250u + ++isnCounter;
    memset(input, 0, sizeof(input));
    memset(output, 0, sizeof(output));
    return result;
}
