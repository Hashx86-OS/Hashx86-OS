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

// CMOS real-time clock reader.
//
// The GUI taskbar shows hours/minutes from the same chip; this file exposes a
// full UTC Unix timestamp for TLS/X.509 certificate validity checks.

#include <core/Iguard.h>
#include <core/ports.h>
#include <core/rtc.h>
#include <types.h>

namespace {

constexpr uint16_t kCmosAddressPort = 0x70;
constexpr uint16_t kCmosDataPort = 0x71;

/**
 * RtcRead() - Read one CMOS register with NMI disabled during selection.
 */
uint8_t RtcRead(uint8_t reg) {
    outb(kCmosAddressPort, (uint8_t)(reg | 0x80));
    uint8_t value = inb(kCmosDataPort);
    outb(kCmosAddressPort, reg);
    return value;
}

/** Convert a BCD byte to binary. */
uint8_t BcdToBinary(uint8_t value) {
    return (uint8_t)(((value >> 4) * 10) + (value & 0x0F));
}

/**
 * DaysFromCivil() - Days since 1970-01-01 for a Gregorian date.
 *
 * Howard Hinnant's civil-from-days algorithm, restricted to 32-bit arithmetic
 * so the freestanding kernel needs no 64-bit division helpers.
 */
int32_t DaysFromCivil(uint32_t year, uint32_t month, uint32_t day) {
    int32_t y = (int32_t)year - (month <= 2 ? 1 : 0);
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    uint32_t yoe = (uint32_t)(y - era * 400);
    int32_t mp = (int32_t)month - (month > 2 ? 3 : -9);
    uint32_t doy = (uint32_t)((153 * mp + 2) / 5) + day - 1;
    uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int32_t)doe - 719468;
}

struct RtcDateTime {
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
};

bool ReadDateTime(RtcDateTime* out) {
    // Wait until the RTC is not mid-update (bounded retry).
    for (int retries = 0; retries < 100000; retries++) {
        if ((RtcRead(0x0A) & 0x80) == 0) break;
        if (retries == 99999) return false;
    }

    uint8_t second = RtcRead(0x00);
    uint8_t minute = RtcRead(0x02);
    uint8_t hour = RtcRead(0x04);
    uint8_t day = RtcRead(0x07);
    uint8_t month = RtcRead(0x08);
    uint8_t year = RtcRead(0x09);
    uint8_t century = RtcRead(0x32);
    uint8_t statusB = RtcRead(0x0B);

    bool binaryMode = (statusB & 0x04) != 0;
    bool is24Hour = (statusB & 0x02) != 0;
    bool isPm = false;

    if (!is24Hour) {
        isPm = (hour & 0x80) != 0;
        hour &= 0x7F;
    }

    if (!binaryMode) {
        second = BcdToBinary(second);
        minute = BcdToBinary(minute);
        hour = BcdToBinary(hour);
        day = BcdToBinary(day);
        month = BcdToBinary(month);
        year = BcdToBinary(year);
        century = BcdToBinary(century);
    }

    if (!is24Hour) {
        if (isPm && hour != 12) hour += 12;
        if (!isPm && hour == 12) hour = 0;
    }

    uint32_t fullYear;
    if (century == 19 || century == 20 || century == 21) {
        fullYear = (uint32_t)century * 100 + year;
    } else {
        fullYear = 2000u + year;
    }

    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59 ||
        fullYear < 1970) {
        return false;
    }

    out->year = fullYear;
    out->month = month;
    out->day = day;
    out->hour = hour;
    out->minute = minute;
    out->second = second;
    return true;
}

}  // namespace

uint32_t RtcUnixTime() {
    InterruptGuard guard;
    RtcDateTime dateTime;

    // A second read guards against the clock rolling over between fields.
    for (int attempt = 0; attempt < 3; attempt++) {
        if (!ReadDateTime(&dateTime)) return 0;
        RtcDateTime check;
        if (!ReadDateTime(&check)) return 0;
        if (check.year == dateTime.year && check.month == dateTime.month &&
            check.day == dateTime.day && check.hour == dateTime.hour &&
            check.minute == dateTime.minute && check.second == dateTime.second) {
            break;
        }
        if (attempt == 2) return 0;
    }

    int32_t days = DaysFromCivil(dateTime.year, dateTime.month, dateTime.day);
    if (days < 0) return 0;

    uint32_t seconds =
        (uint32_t)days * 86400u + dateTime.hour * 3600u + dateTime.minute * 60u + dateTime.second;
    return seconds;
}
