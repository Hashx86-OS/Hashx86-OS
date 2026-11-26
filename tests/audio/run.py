#!/usr/bin/env python3
"""Run native audio tests: python3 tests/audio/run.py.

Compile the production mixer with allocator/driver dependency stubs and a
temporary header replacing only privileged IRQ instructions. These tests
exercise PCM output; they do not emulate DMA timing or a VM audio backend.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="hashx86-audio-") as tmp:
    tmp = Path(tmp)
    def write(name, text):
        dest = tmp / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(text)

    header = (root / "include/core/drivers/AudioMixer.h").read_text()
    header, count = re.subn(
        r"    static inline uint32_t lock\(\) \{.*?\n    \}",
        "    static inline uint32_t lock() { return 0; }", header, flags=re.S)
    assert count == 1
    header, count = re.subn(
        r"    static inline void unlock\(uint32_t eflags\) \{.*?\n    \}",
        "    static inline void unlock(uint32_t) {}", header, flags=re.S)
    assert count == 1
    write("core/drivers/AudioMixer.h", header)
    write("core/driver.h", "#pragma once\n")
    write("core/memory.h", "#pragma once\n#include <cstddef>\n"
          "void* kmalloc(size_t);\nvoid kfree(void*);\n")
    write("debug.h", "#include <cstdio>\n")
    executable = tmp / "mixer-test"
    subprocess.run([
        "g++", "-std=c++17", "-g", "-O1", "-fsanitize=undefined",
        "-I", str(tmp), "-idirafter", str(root / "include"),
        str(root / "core/drivers/AudioMixer.cpp"),
        str(root / "tests/audio/mixer_test.cpp"), "-o", str(executable)
    ], check=True)
    subprocess.run([str(executable)], check=True)

    # Execute the actual driver's queue-accounting method with register reads
    # supplied by the test. No copied implementation of its algorithm.
    driver = (root / "drivers/ac97.cpp").read_text()
    method = re.search(r"    void UpdateConsumed\(\) \{.*?\n    \}", driver, re.S)[0]
    constants = "\n".join(line for line in driver.splitlines()
                          if line.startswith("#define AC97_"))
    write("queue.cpp", """
#include <cstdint>
#include <cassert>
#include <cstdio>
""" + constants + """
struct Queue {
    bool isPlaying = true;
    uint32_t queuedEntries = 0;
    uint8_t lastCiv = 0, writeEntry = 0;
    uint16_t nabmBar = 0, sr = 0, picb = 2048;
    uint8_t civ = 0;
    uint8_t inb(uint16_t) { return civ; }
    uint16_t inw(uint16_t reg) { return reg == AC97_PO_SR ? sr : picb; }
""" + method + """
};
int main() {
    static_assert(AC97_SR_LVBCI == 4);
    Queue q;
    q.isPlaying = false; q.queuedEntries = 32;
    q.UpdateConsumed(); assert(q.queuedEntries == 32);
    q.isPlaying = true; q.civ = 8;
    q.UpdateConsumed(); assert(q.queuedEntries == 24);
    q.UpdateConsumed(); assert(q.queuedEntries == 24);
    q.civ = 31; q.picb = 0; q.sr = AC97_SR_DCH | AC97_SR_CELV;
    q.UpdateConsumed(); assert(q.queuedEntries == 0 && q.lastCiv == 0);
    q.UpdateConsumed(); assert(q.queuedEntries == 0);
    // Resume at entry zero, then consume one entry without advancing CIV.
    q.queuedEntries = 1; q.writeEntry = 1; q.civ = 0;
    q.sr = 0; q.picb = 2048;
    q.UpdateConsumed(); assert(q.queuedEntries == 1);
    q.sr = AC97_SR_DCH | AC97_SR_CELV; q.picb = 0;
    q.UpdateConsumed(); assert(q.queuedEntries == 0 && q.lastCiv == 1);
    // An entire ring drained between polls must not look like zero progress.
    q.queuedEntries = 32; q.writeEntry = 1; q.lastCiv = 1; q.civ = 0;
    q.UpdateConsumed(); assert(q.queuedEntries == 0);
    // Ordinary cursor wrap while the DMA engine is still running.
    q.queuedEntries = 8; q.lastCiv = 30; q.civ = 2; q.sr = 0; q.picb = 100;
    q.UpdateConsumed(); assert(q.queuedEntries == 4 && q.lastCiv == 2);
    puts("AC97 queue regression tests passed");
}
""")
    subprocess.run(["g++", "-std=c++17", "-fsanitize=undefined",
                    str(tmp / "queue.cpp"), "-o", str(tmp / "queue-test")], check=True)
    subprocess.run([str(tmp / "queue-test")], check=True)
