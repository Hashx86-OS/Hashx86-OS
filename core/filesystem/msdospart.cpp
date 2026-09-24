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

#define KDBG_COMPONENT "MSDOSPART"
#include <core/filesystem/FatFs/ff.h>
#include <core/filesystem/FatFsWrapper.h>
#include <core/filesystem/msdospart.h>
#include <debug.h>

FileSystem* MSDOSPartitionTable::partitions[4] = {0, 0, 0, 0};
MSDOSPartitionTable* MSDOSPartitionTable::activeInstance = nullptr;

/**
 * MSDOSPartitionTable::MSDOSPartitionTable() - Bind the table to an ATA device.
 * @ata: The ATA device to manage.
 */
MSDOSPartitionTable::MSDOSPartitionTable(AdvancedTechnologyAttachment* ata) {
    this->ata = ata;
    this->activeInstance = this;
};
MSDOSPartitionTable::~MSDOSPartitionTable(){};

/**
 * MSDOSPartitionTable::Initialize() - Create and format a fresh MBR layout.
 *
 * Splits the disk into two FAT32 partitions (63 sectors header/allocation
 * reserve) and formats both with FatFs. ATAPI (CD-ROM) devices are skipped.
 */
void MSDOSPartitionTable::Initialize() {
    KDBG1("Initializing Disk...");

    // CD-ROMs (ATAPI) have no partition table.
    if (ata->isAtapi) {
        KDBG1("Skipping ATAPI device (CD-ROM), no partition table present.");
        return;
    }

    // Query the drive size from the ATA device.
    uint32_t totalSectors = ata->GetSizeInSectors();
    if (totalSectors == 0) {
        KDBG1("Error: Could not identify drive size.");
        return;
    }

    // Reject tiny disks: need at least 63 reserved sectors plus data area.
    if (totalSectors <= 63) {
        KDBG1("Error: Disk too small (totalSectors=%u). Cannot partition.", totalSectors);
        return;
    }

    // Require at least two sectors so both partitions end up non-empty.
    uint32_t available = totalSectors - 63;
    if (available < 2) {
        KDBG1("Error: Disk too small (available=%u). Need >= 2 for two non-empty partitions.",
              available);
        return;
    }

    // Split the disk in two, reserving 63 sectors for the MBR and alignment.
    uint32_t p1_size = available / 2;
    uint32_t p2_size = available - p1_size;  // Remainder.

    uint32_t p1_start = 63;
    uint32_t p2_start = 63 + p1_size;

    KDBG2("Partition 1: Start %d, Size %d", (int32_t)p1_start, (int32_t)p1_size);
    KDBG2("Partition 2: Start %d, Size %d", (int32_t)p2_start, (int32_t)p2_size);

    if (p1_size < 128 || p2_size < 128) {
        KDBG1("Error: Partition too small (need >=128 sectors, got %u and %u).", p1_size, p2_size);
        return;
    }

    // Create the MBR, fully zeroed before filling any fields.
    MasterBootRecord mbr;
    memset(&mbr, 0, sizeof(MasterBootRecord));

    mbr.magicnumber = 0xAA55;

    // Partition 1 entry.
    mbr.primaryPartition[0].bootable = 0x80;      // Active.
    mbr.primaryPartition[0].partition_id = 0x0C;  // FAT32 LBA.
    mbr.primaryPartition[0].start_lba = p1_start;
    mbr.primaryPartition[0].length = p1_size;
    mbr.primaryPartition[0].start_head = 0;  // Legacy CHS fields, unused.
    mbr.primaryPartition[0].end_head = 0;

    // Partition 2 entry.
    mbr.primaryPartition[1].bootable = 0x00;
    mbr.primaryPartition[1].partition_id = 0x0C;  // FAT32 LBA.
    mbr.primaryPartition[1].start_lba = p2_start;
    mbr.primaryPartition[1].length = p2_size;
    mbr.primaryPartition[1].start_head = 0;
    mbr.primaryPartition[1].end_head = 0;

    // Write the MBR.
    if (!ata->Write28(0, (uint8_t*)&mbr, 512)) {
        KDBG1("Error: MBR write failed.");
        return;
    }

    // Format each partition with FatFs f_mkfs to build a valid FAT32 volume.
    {
        MKFS_PARM opt;
        memset(&opt, 0, sizeof(opt));
        opt.fmt = FM_FAT32 | FM_SFD;
        opt.au_size = 0;  // Auto cluster size so small partitions work.
        uint8_t work[4096];

        // Format partition 1 on pdrv=0.
        fatfs_init(0, ata, p1_start, p1_size);
        FRESULT res = f_mkfs("0:", &opt, work, sizeof(work));
        if (res != FR_OK) {
            KDBG1("f_mkfs(pdrv=0) failed: %d", res);
            return;
        }

        // Format partition 2 on pdrv=1.
        fatfs_init(1, ata, p2_start, p2_size);
        res = f_mkfs("1:", &opt, work, sizeof(work));
        if (res != FR_OK) {
            KDBG1("f_mkfs(pdrv=1) failed: %d", res);
            return;
        }
    }

    KDBG1("Initialization Complete.");

    return;
}

/**
 * MSDOSPartitionTable::ReadPartitions() - Mount the partitions in the MBR.
 *
 * Clears any previously mounted partitions, validates each entry's extent
 * against the device, and mounts FAT12/16/32 partitions through FatFs.
 * Invalid MBR signatures are logged and never auto-formatted.
 */
void MSDOSPartitionTable::ReadPartitions() {
    // Reset partition state so repeated calls don't append into old data.
    for (int i = 0; i < 4; i++) {
        if (partitions[i]) {
            delete partitions[i];
            partitions[i] = nullptr;
        }
    }
    partitionsCounter = 0;

    // Query the drive size from the ATA device.
    uint32_t totalSectors = ata->GetSizeInSectors();
    if (totalSectors == 0) {
        KDBG1("Error: Could not identify drive size.");
        return;
    }
    if (totalSectors <= 63) {
        KDBG1("Error: Disk too small (totalSectors=%u). Cannot read partitions.", totalSectors);
        return;
    }

    MasterBootRecord mbr;
    memset(&mbr, 0, sizeof(MasterBootRecord));
    ata->Read28(0, (uint8_t*)&mbr, sizeof(MasterBootRecord));

    // Reject an invalid signature; never auto-format here.
    if (mbr.magicnumber != 0xAA55) {
        KDBG1("Error: Invalid MBR signature (got 0x%x, expected 0xAA55).", mbr.magicnumber);
        KDBG1("Use FormatRaw() explicitly to format the drive.");
        return;
    }

    for (int i = 0; i < 4; i++) {
        if (mbr.primaryPartition[i].partition_id == 0) continue;

        KDBG2("Partition %d %sType 0x%x Start %d", i,
              (mbr.primaryPartition[i].bootable == 0x80) ? "[Bootable] " : "",
              mbr.primaryPartition[i].partition_id, mbr.primaryPartition[i].start_lba);

        // Bounds check before mounting.
        if (partitionsCounter >= 4) {
            KDBG1("Warning: Too many partitions; max 4 supported.");
            break;
        }

        // Validate partition extent fits within the device.
        uint32_t start = mbr.primaryPartition[i].start_lba;
        uint32_t length = mbr.primaryPartition[i].length;
        if (length == 0 || start >= totalSectors || length > totalSectors - start) {
            KDBG1(
                "Warning: Partition %d extent [%u, %u+%u) outside device (totalSectors=%u); "
                "skipping",
                i, start, start, length, totalSectors);
            continue;
        }

        // Mount the FAT32 volume.
        if (mbr.primaryPartition[i].partition_id == 0x0C ||
            mbr.primaryPartition[i].partition_id == 0x0B) {
            if (partitionsCounter >= FF_VOLUMES) {
                KDBG1("Warning: Reached FatFs volume limit (%d), skipping remaining partitions",
                      FF_VOLUMES);
                break;
            }
            FatFsWrapper* fs =
                new FatFsWrapper(ata, mbr.primaryPartition[i].start_lba, (BYTE)partitionsCounter,
                                 mbr.primaryPartition[i].length);
            if (!fs) {
                HALT("CRITICAL: Failed to allocate FatFsWrapper!\n");
            }
            this->partitions[partitionsCounter++] = fs;
        }
    }
}
