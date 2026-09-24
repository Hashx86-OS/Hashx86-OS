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

#define KDBG_COMPONENT "ATA"
#include <core/drivers/ata.h>

AdvancedTechnologyAttachment::AdvancedTechnologyAttachment(bool master, uint16_t portBase)
    : dataPort(portBase),
      errorPort(portBase + 0x1),
      sectorCountPort(portBase + 0x2),
      lbaLowPort(portBase + 0x3),
      lbaMidPort(portBase + 0x4),
      lbaHiPort(portBase + 0x5),
      devicePort(portBase + 0x6),
      commandPort(portBase + 0x7),
      controlPort(portBase + 0x206) {
    this->master = master;
}

AdvancedTechnologyAttachment::~AdvancedTechnologyAttachment() {}

/**
 * AdvancedTechnologyAttachment::Identify() - Probe the drive and read its size.
 *
 * Selects the device, latches the device signature to tell ATA from ATAPI,
 * issues the matching IDENTIFY command, and reads 256 identity words. Word 0
 * supplies the general configuration and words 60/61 the LBA28 sector count.
 *
 * Return: The total number of sectors, or 0 when the drive is absent or the
 *         identify sequence fails.
 */
uint32_t AdvancedTechnologyAttachment::Identify() {
    KDBG1("Identifying %s %s drive...",
          ((this->dataPort.getPortNumber() == 0x1F0) ? "primary" : "secondary"),
          (master ? "master" : "slave"));

    devicePort.Write(master ? 0xA0 : 0xB0);
    controlPort.Write(0);

    devicePort.Write(master ? 0xA0 : 0xB0);
    uint8_t status = commandPort.Read();
    if (status == 0xFF) {
        KDBG1("No Device (Status 0xFF)");
        return 0;
    }

    // Device-select settle delay before latching the signature registers: ATA
    // requires ~400ns after SEL, which four consecutive status reads provide
    // (the same port-access pattern used on the other ATA paths).
    for (int i = 0; i < 4; i++) {
        status = commandPort.Read();
    }

    // Read the device signature latched on device select. ATAPI packet devices
    // report 0x14/0xEB in LBA mid/high - the definitive signature; some ATA
    // drives also report 0x01/0x01 in sector-count/LBA-low, so that weaker
    // pattern is not used. Use IDENTIFY PACKET DEVICE (0xA1) for them and
    // IDENTIFY DEVICE (0xEC) otherwise, so the identification block (and the
    // word-0 decode below) always comes from the command the device actually
    // implements.
    uint8_t sigLbaMid = lbaMidPort.Read();
    uint8_t sigLbaHigh = lbaHiPort.Read();
    bool packetDevice = (sigLbaMid == 0x14 && sigLbaHigh == 0xEB);

    devicePort.Write(master ? 0xA0 : 0xB0);
    sectorCountPort.Write(0);
    lbaLowPort.Write(0);
    lbaMidPort.Write(0);
    lbaHiPort.Write(0);
    commandPort.Write(packetDevice ? 0xA1 : 0xEC);  // Identify command.

    status = commandPort.Read();
    if (status == 0x00) {
        KDBG1("No Device (Status 0x00)");
        return 0;
    }

    uint32_t bsyWait = 0;
    while (((status & 0x80) == 0x80) && ((status & 0x01) != 0x01)) {
        if (bsyWait++ > 1000000) {
            KDBG1("IDENTIFY ERROR: BSY timeout");
            return 0;
        }
        status = commandPort.Read();
    }

    if (status & 0x01) {
        KDBG1("IDENTIFY ERROR");
        return 0;
    }

    uint32_t totalSectors = 0;

    for (int i = 0; i < 256; i++) {
        // Poll for DRQ before each word, with timeout/error handling.
        uint32_t drqWait = 0;
        status = commandPort.Read();
        while ((status & 0x08) != 0x08) {
            if ((status & 0x01) == 0x01) {
                KDBG1("IDENTIFY ERROR: ERR set while waiting for DRQ");
                return 0;
            }
            if ((status & 0x20) == 0x20) {
                KDBG1("IDENTIFY ERROR: DF set while waiting for DRQ");
                return 0;
            }
            if (drqWait++ > 1000000) {
                KDBG1("IDENTIFY ERROR: DRQ timeout at word %d", i);
                return 0;
            }
            status = commandPort.Read();
        }

        uint16_t data = dataPort.Read();

        // Word 0 holds the general configuration: bit 15 = ATAPI, bit 7 =
        // removable media.
        if (i == 0) {
            identify_general_config = data;
            isAtapi = (data & 0x8000) != 0;
            isRemovable = (data & 0x0080) != 0;
        }

        // Words 60 and 61 contain the total sector count for LBA28.
        if (i == 60) {
            totalSectors = data;
        } else if (i == 61) {
            totalSectors |= ((uint32_t)data << 16);
        }
    }

    KDBG1("HDD Identified. Size: %d Sectors (%d MB)", (int32_t)totalSectors,
          (int32_t)(totalSectors * 512) / 1024 / 1024);
    this->ata_size = totalSectors;
    return totalSectors;
}

/**
 * AdvancedTechnologyAttachment::Read28() - Read one sector via PIO LBA28.
 * @sectorNum: 28-bit LBA sector number.
 * @data: Destination buffer, at least @count bytes.
 * @count: Bytes to copy out; 512 reads directly into @data.
 *
 * Issues READ SECTOR (0x20), waits out BSY/DRQ with error checks, then
 * transfers the 512-byte block. Partial reads go through a temporary sector
 * buffer.
 */
void AdvancedTechnologyAttachment::Read28(uint32_t sectorNum, uint8_t* data, int count) {
    if (sectorNum > 0x0FFFFFFF) return;
    if (data == nullptr || count <= 0) return;

    devicePort.Write((master ? 0xE0 : 0xF0) | ((sectorNum & 0x0F000000) >> 24));
    errorPort.Write(0);
    sectorCountPort.Write(1);
    lbaLowPort.Write(sectorNum & 0x000000FF);
    lbaMidPort.Write((sectorNum & 0x0000FF00) >> 8);
    lbaHiPort.Write((sectorNum & 0x00FF0000) >> 16);
    commandPort.Write(0x20);

    uint8_t status = commandPort.Read();
    // ATA 400ns delay via four status reads.
    commandPort.Read();
    commandPort.Read();
    commandPort.Read();

    uint32_t bsyWait = 0;
    while ((status & 0x80) == 0x80) {
        if (bsyWait++ > 1000000) {
            KDBG1("READ ERROR: BSY timeout");
            return;
        }
        status = commandPort.Read();
    }
    if ((status & 0x01) == 0x01) {
        KDBG1("READ ERROR");
        return;
    }
    uint32_t drqWait = 0;
    while ((status & 0x08) != 0x08) {
        if ((status & 0x01) == 0x01) {
            KDBG1("READ ERROR: ERR set while waiting for DRQ");
            return;
        }
        if ((status & 0x20) == 0x20) {
            KDBG1("READ ERROR: DF set while waiting for DRQ");
            return;
        }
        if (drqWait++ > 1000000) {
            KDBG1("READ ERROR: DRQ timeout");
            return;
        }
        status = commandPort.Read();
    }

    // --- Optimized read ---
    if (count == 512) {
        // Fast path: transfer the sector straight to the caller's buffer.
        insw(dataPort.getPortNumber(), data, 256);
    } else {
        // Slow path: copy the requested prefix out of a scratch sector.
        uint8_t sectorBuffer[512];
        insw(dataPort.getPortNumber(), sectorBuffer, 256);

        int safeCount = count;
        if (safeCount < 0) safeCount = 0;
        if (safeCount > 512) safeCount = 512;

        for (int i = 0; i < safeCount; i++) {
            data[i] = sectorBuffer[i];
        }
    }
}

/**
 * ata_wait_drq() - Poll the status register until data is ready or an error.
 * @commandPort: Port to poll.
 * @op: Operation name used in diagnostics.
 *
 * Return: True when DRQ is set and no error/debug flag is pending.
 */
static bool ata_wait_drq(Port8Bit& commandPort, const char* op) {
    uint8_t status = commandPort.Read();
    uint32_t bsyWait = 0;
    while ((status & 0x80) == 0x80) {
        if (bsyWait++ > 1000000) {
            KDBG1("%s ERROR: BSY timeout", op);
            return false;
        }
        status = commandPort.Read();
    }
    uint32_t drqWait = 0;
    while ((status & 0x08) != 0x08) {
        if ((status & 0x01) == 0x01) {
            KDBG1("%s ERROR: ERR set while waiting for DRQ", op);
            return false;
        }
        if ((status & 0x20) == 0x20) {
            KDBG1("%s ERROR: DF set while waiting for DRQ", op);
            return false;
        }
        if (drqWait++ > 1000000) {
            KDBG1("%s ERROR: DRQ timeout", op);
            return false;
        }
        status = commandPort.Read();
    }
    return true;
}

/**
 * ata_wait_ready() - Poll until a PIO transfer is fully absorbed.
 * @commandPort: Port to poll.
 * @op: Operation name used in diagnostics.
 *
 * Return: True when BSY/DRQ have cleared and ERR/DF are not set.
 */
static bool ata_wait_ready(Port8Bit& commandPort, const char* op) {
    uint8_t status = commandPort.Read();
    uint32_t wait = 0;
    // Poll until the PIO data transfer is fully absorbed: both BSY and DRQ
    // must clear. A controller may legitimately keep DRQ asserted as BSY
    // drops, so only a fully idle status (BSY=0, DRQ=0) counts as completion.
    while (((status & 0x80) == 0x80) || ((status & 0x08) == 0x08)) {
        if ((status & 0x01) == 0x01) {
            KDBG1("%s ERROR: ERR set while waiting for completion", op);
            return false;
        }
        if (wait++ > 1000000) {
            KDBG1("%s ERROR: completion timeout", op);
            return false;
        }
        status = commandPort.Read();
    }
    if ((status & 0x01) == 0x01) {
        KDBG1("%s ERROR: ERR set after completion", op);
        return false;
    }
    if ((status & 0x20) == 0x20) {
        KDBG1("%s ERROR: DF set after completion", op);
        return false;
    }
    return true;
}

/**
 * AdvancedTechnologyAttachment::Write28() - Write one sector via PIO LBA28.
 * @sectorNum: 28-bit LBA sector number.
 * @data: Sector image to write.
 * @count: Bytes from @data to write (up to a full sector).
 *
 * Full-sector writes go straight out. Partial writes first READ SECTOR, merge
 * the caller's bytes into the scratch sector, then WRITE SECTOR. Every path
 * finishes with a FLUSH CACHE.
 *
 * Return: True on success.
 */
bool AdvancedTechnologyAttachment::Write28(uint32_t sectorNum, uint8_t* data, uint32_t count) {
    if (sectorNum > 0x0FFFFFFF) return false;
    if (data == nullptr || count <= 0) return false;  // No-op: reject null or zero length.
    if (count > 512) count = 512;

    devicePort.Write((master ? 0xE0 : 0xF0) | ((sectorNum & 0x0F000000) >> 24));
    errorPort.Write(0);
    sectorCountPort.Write(1);
    lbaLowPort.Write(sectorNum & 0x000000FF);
    lbaMidPort.Write((sectorNum & 0x0000FF00) >> 8);
    lbaHiPort.Write((sectorNum & 0x00FF0000) >> 16);

    if (count == 512) {
        // Full-sector write: issue WRITE and send the data directly.
        commandPort.Write(0x30);
        if (!ata_wait_drq(commandPort, "WRITE")) return false;
        outsw(dataPort.getPortNumber(), data, 256);
    } else {
        // Partial write: read the sector first, merge, then write it back.
        commandPort.Write(0x20);  // READ SECTOR.
        if (!ata_wait_drq(commandPort, "READ")) return false;

        uint8_t sectorBuffer[512];
        insw(dataPort.getPortNumber(), sectorBuffer, 256);

        // Overwrite only the first 'count' bytes with the caller's data.
        for (int i = 0; i < (int)count; i++) {
            sectorBuffer[i] = data[i];
        }

        // Wait for the READ SECTOR transfer to complete before issuing a new
        // command; the device must be done (BSY=0, DRQ=0, ERR=0, DF=0).
        if (!ata_wait_ready(commandPort, "READ")) return false;

        // Write the merged sector back. Re-select the device and restore the
        // LBA registers; the preceding READ may have left the controller in a
        // different state.
        devicePort.Write((master ? 0xE0 : 0xF0) | ((sectorNum & 0x0F000000) >> 24));
        errorPort.Write(0);
        sectorCountPort.Write(1);
        lbaLowPort.Write(sectorNum & 0x000000FF);
        lbaMidPort.Write((sectorNum & 0x0000FF00) >> 8);
        lbaHiPort.Write((sectorNum & 0x00FF0000) >> 16);
        commandPort.Write(0x30);  // WRITE SECTOR.
        if (!ata_wait_drq(commandPort, "WRITE")) return false;
        outsw(dataPort.getPortNumber(), sectorBuffer, 256);
    }

    // Wait for the WRITE SECTOR transfer to complete before issuing FLUSH.
    if (!ata_wait_ready(commandPort, "WRITE")) return false;

    return Flush();
}

/**
 * AdvancedTechnologyAttachment::Flush() - Issue FLUSH CACHE and wait for idle.
 *
 * Return: True when the drive reports a clean, ready completion state.
 */
bool AdvancedTechnologyAttachment::Flush() {
    devicePort.Write(master ? 0xE0 : 0xF0);
    commandPort.Write(0xE7);
    uint8_t status = commandPort.Read();
    if (status == 0x00) {
        KDBG1("FLUSH ERROR: device returned status 0x00");
        return false;
    }
    uint32_t flushWait = 0;
    while ((status & 0x80) == 0x80) {
        if ((status & 0x01) == 0x01) {
            KDBG1("FLUSH ERROR: ERR set while waiting for BSY");
            return false;
        }
        if (flushWait++ > 1000000) {
            KDBG1("FLUSH ERROR: BSY timeout");
            return false;
        }
        status = commandPort.Read();
    }

    // Completion requires BSY=0, ERR=0, DF=0, DRQ=0 and DRDY=1.
    if ((status & 0x01) == 0x01) {
        KDBG1("FLUSH ERROR: ERR set after BSY");
        return false;
    }
    if ((status & 0x20) == 0x20) {
        KDBG1("FLUSH ERROR: DF set after BSY");
        return false;
    }
    if ((status & 0x08) == 0x08) {
        KDBG1("FLUSH ERROR: DRQ set after BSY");
        return false;
    }
    if ((status & 0x40) != 0x40) {
        KDBG1("FLUSH ERROR: DRDY not set after BSY");
        return false;
    }
    return true;
}
