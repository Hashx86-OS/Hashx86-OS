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

#ifndef ATA_H
#define ATA_H

#include <core/interrupts.h>
#include <core/ports.h>
#include <debug.h>
#include <types.h>

/**
 * class AdvancedTechnologyAttachment - ATA (IDE) hard disk controller.
 * @ata_size: Total drive size in sectors.
 * @master: Whether this controller is the master drive.
 * @dataPort: 16-bit data register.
 * @errorPort: Error/feature register.
 * @sectorCountPort: Sector count register.
 * @lbaLowPort: LBA low byte register.
 * @lbaMidPort: LBA mid byte register.
 * @lbaHiPort: LBA high byte register.
 * @devicePort: Drive/head select register.
 * @commandPort: Command register.
 * @controlPort: Control register.
 */
class AdvancedTechnologyAttachment {
private:
    uint32_t ata_size;

protected:
    bool master;
    Port16Bit dataPort;
    Port8Bit errorPort;
    Port8Bit sectorCountPort;
    Port8Bit lbaLowPort;
    Port8Bit lbaMidPort;
    Port8Bit lbaHiPort;
    Port8Bit devicePort;
    Port8Bit commandPort;
    Port8Bit controlPort;

public:
    AdvancedTechnologyAttachment(bool master, uint16_t portBase);
    ~AdvancedTechnologyAttachment();

    // ATA IDENTIFY results (word 0 = general configuration).
    uint16_t identify_general_config = 0;
    bool isAtapi = false;
    bool isRemovable = false;

    /**
     * Identify() - Query the attached drive.
     *
     * Return: True on success, false on failure.
     */
    uint32_t Identify();

    /**
     * Read28() - Read a 28-bit LBA sector.
     * @sectorNum: Sector number.
     * @data: Destination buffer, default 512 bytes.
     * @count: Number of bytes to read.
     */
    void Read28(uint32_t sectorNum, uint8_t* data, int count = 512);

    /**
     * Write28() - Write a 28-bit LBA sector.
     * @sectorNum: Sector number.
     * @data: Source buffer.
     * @count: Number of bytes to write.
     *
     * Return: True on success, false on failure.
     */
    bool Write28(uint32_t sectorNum, uint8_t* data, uint32_t count);

    /**
     * Flush() - Wait for the drive cache to flush.
     *
     * Return: True on success, false on failure.
     */
    bool Flush();

    uint32_t GetSizeInSectors() {
        return ata_size;
    }
};

#endif  // ATA_H
