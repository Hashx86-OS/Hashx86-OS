/**
 * @file        ata.cpp
 * @brief       ATA Driver Implementation
 *
 * @date        01/02/2026
 * @version     1.0.0
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

// Returns the total number of sectors on the drive (LBA28)
// Returns 0 if error or drive not present
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

    devicePort.Write(master ? 0xA0 : 0xB0);
    sectorCountPort.Write(0);
    lbaLowPort.Write(0);
    lbaMidPort.Write(0);
    lbaHiPort.Write(0);
    commandPort.Write(0xEC);  // Identify command

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
        // Poll for DRQ before each word, with timeout/error handling
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

        // Words 60 and 61 contain the total sector count for LBA28
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

void AdvancedTechnologyAttachment::Read28(uint32_t sectorNum, uint8_t* data, int count) {
    if (sectorNum > 0x0FFFFFFF) return;

    devicePort.Write((master ? 0xE0 : 0xF0) | ((sectorNum & 0x0F000000) >> 24));
    errorPort.Write(0);
    sectorCountPort.Write(1);
    lbaLowPort.Write(sectorNum & 0x000000FF);
    lbaMidPort.Write((sectorNum & 0x0000FF00) >> 8);
    lbaHiPort.Write((sectorNum & 0x00FF0000) >> 16);
    commandPort.Write(0x20);

    uint8_t status = commandPort.Read();
    uint8_t status2 = commandPort.Read();
    uint8_t status3 = commandPort.Read();
    uint8_t status4 = commandPort.Read();

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

    // --- OPTIMIZED READ ---
    if (count == 512) {
        // Fast Path
        insw(dataPort.getPortNumber(), data, 256);
    } else {
        // Slow Path
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

void AdvancedTechnologyAttachment::Write28(uint32_t sectorNum, uint8_t* data, uint32_t count) {
    if (sectorNum > 0x0FFFFFFF) return;
    if (count == 0) return;  // No-op: reject zero-length writes
    if (count > 512) count = 512;

    devicePort.Write((master ? 0xE0 : 0xF0) | ((sectorNum & 0x0F000000) >> 24));
    errorPort.Write(0);
    sectorCountPort.Write(1);
    lbaLowPort.Write(sectorNum & 0x000000FF);
    lbaMidPort.Write((sectorNum & 0x0000FF00) >> 8);
    lbaHiPort.Write((sectorNum & 0x00FF0000) >> 16);
    commandPort.Write(0x30);

    uint8_t status = commandPort.Read();
    uint8_t status2 = commandPort.Read();
    uint8_t status3 = commandPort.Read();
    uint8_t status4 = commandPort.Read();

    uint32_t bsyWait = 0;
    while ((status & 0x80) == 0x80) {
        if (bsyWait++ > 1000000) {
            KDBG1("READ ERROR: BSY timeout");
            return;
        }
        status = commandPort.Read();
    }
    uint32_t drqWait = 0;
    while ((status & 0x08) != 0x08) {
        if ((status & 0x01) == 0x01) {
            KDBG1("WRITE ERROR: ERR set while waiting for DRQ");
            return;
        }
        if ((status & 0x20) == 0x20) {
            KDBG1("WRITE ERROR: DF set while waiting for DRQ");
            return;
        }
        if (drqWait++ > 1000000) {
            KDBG1("WRITE ERROR: DRQ timeout");
            return;
        }
        status = commandPort.Read();
    }

    // --- OPTIMIZED WRITE ---
    // If we are writing a full sector, use outsw
    if (count == 512) {
        outsw(dataPort.getPortNumber(), data, 256);
    } else {
        // Partial write: Copy to padded buffer first
        uint8_t sectorBuffer[512];
        for (int i = 0; i < 512; i++) {
            if (i < (int)count)
                sectorBuffer[i] = data[i];
            else
                sectorBuffer[i] = 0;
        }
        outsw(dataPort.getPortNumber(), sectorBuffer, 256);
    }

    Flush();
}

void AdvancedTechnologyAttachment::Flush() {
    devicePort.Write(master ? 0xE0 : 0xF0);
    commandPort.Write(0xE7);
    uint8_t status = commandPort.Read();
    if (status == 0x00) return;
    uint32_t flushWait = 0;
    while ((status & 0x80) == 0x80) {
        if ((status & 0x01) == 0x01) {
            KDBG1("FLUSH ERROR: ERR set while waiting for BSY");
            return;
        }
        if (flushWait++ > 1000000) {
            KDBG1("FLUSH ERROR: BSY timeout");
            return;
        }
        status = commandPort.Read();
    }

    // Check for error flags after BSY clears
    if ((status & 0x01) == 0x01) {
        KDBG1("FLUSH ERROR: ERR set after BSY");
        return;
    }
    if ((status & 0x20) == 0x20) {
        KDBG1("FLUSH ERROR: DF set after BSY");
        return;
    }
}
