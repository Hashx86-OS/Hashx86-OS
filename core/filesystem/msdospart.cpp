/**
 * @file        msdospart.cpp
 * @brief       MSDOS Partition Table Implementation
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "MSDOSPART"
#include <core/filesystem/msdospart.h>
#include <core/filesystem/FatFsWrapper.h>
#include <core/filesystem/FatFs/diskio.h>
#include <core/filesystem/FatFs/ff.h>
#include <debug.h>

FileSystem* MSDOSPartitionTable::partitions[4] = {0, 0, 0, 0};
MSDOSPartitionTable* MSDOSPartitionTable::activeInstance = nullptr;

MSDOSPartitionTable::MSDOSPartitionTable(AdvancedTechnologyAttachment* ata) {
    this->ata = ata;
    this->activeInstance = this;
};
MSDOSPartitionTable::~MSDOSPartitionTable(){};

void MSDOSPartitionTable::Initialize() {
    KDBG1("Initializing Disk...");

    // Get Drive Size from ATA
    uint32_t totalSectors = ata->GetSizeInSectors();
    if (totalSectors == 0) {
        KDBG1("Error: Could not identify drive size.");
        return;
    }

    // Check for tiny disks: need at least 63 reserved sectors + some data area
    if (totalSectors <= 63) {
        KDBG1("Error: Disk too small (totalSectors=%u). Cannot partition.", totalSectors);
        return;
    }

    // Need at least 2 sectors available so both partitions have non-zero size
    uint32_t available = totalSectors - 63;
    if (available < 2) {
        KDBG1("Error: Disk too small (available=%u). Need >= 2 for two non-empty partitions.",
              available);
        return;
    }

    // Calculate Partitions (Split in 2)
    // Reserve 63 sectors for MBR and alignment
    uint32_t p1_size = available / 2;
    uint32_t p2_size = available - p1_size;  // Remainder

    uint32_t p1_start = 63;
    uint32_t p2_start = 63 + p1_size;

    KDBG2("Partition 1: Start %d, Size %d", (int32_t)p1_start, (int32_t)p1_size);
    KDBG2("Partition 2: Start %d, Size %d", (int32_t)p2_start, (int32_t)p2_size);

    // Create MBR — fully zero the entire structure before setting fields
    MasterBootRecord mbr;
    memset(&mbr, 0, sizeof(MasterBootRecord));

    mbr.magicnumber = 0xAA55;

    // Partition 1 Entry
    mbr.primaryPartition[0].bootable = 0x80;      // Active
    mbr.primaryPartition[0].partition_id = 0x0C;  // FAT32 LBA
    mbr.primaryPartition[0].start_lba = p1_start;
    mbr.primaryPartition[0].length = p1_size;
    mbr.primaryPartition[0].start_head = 0;  // Legacy unused
    mbr.primaryPartition[0].end_head = 0;

    // Partition 2 Entry
    mbr.primaryPartition[1].bootable = 0x00;
    mbr.primaryPartition[1].partition_id = 0x0C;  // FAT32 LBA
    mbr.primaryPartition[1].start_lba = p2_start;
    mbr.primaryPartition[1].length = p2_size;
    mbr.primaryPartition[1].start_head = 0;
    mbr.primaryPartition[1].end_head = 0;

    // Write MBR
    ata->Write28(0, (uint8_t*)&mbr, 512);

    // FORMAT Partitions using FatFs f_mkfs (creates a valid FAT32 that FatFs recognizes)
    {
        MKFS_PARM opt;
        memset(&opt, 0, sizeof(opt));
        opt.fmt = FM_FAT32 | FM_SFD;
        opt.au_size = 0;  /* auto-choose cluster size so small partitions work */
        uint8_t work[4096];

        // Partition 1 on pdrv=0
        fatfs_init(0, ata, p1_start, p1_size);
        FRESULT res = f_mkfs("0:", &opt, work, sizeof(work));
        if (res != FR_OK)
            KDBG1("f_mkfs(pdrv=0) failed: %d", res);

        // Partition 2 on pdrv=1
        fatfs_init(1, ata, p2_start, p2_size);
        res = f_mkfs("1:", &opt, work, sizeof(work));
        if (res != FR_OK)
            KDBG1("f_mkfs(pdrv=1) failed: %d", res);
    }

    KDBG1("Initialization Complete.");

    return;
}

void MSDOSPartitionTable::ReadPartitions() {
    // Reset partition state so repeated calls don't append into old data
    for (int i = 0; i < 4; i++) {
        if (partitions[i]) {
            delete partitions[i];
            partitions[i] = nullptr;
        }
    }
    partitionsCounter = 0;

    // Get Drive Size from ATA
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

    // Check Signature. If invalid, log error and return — no auto-format.
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

        // Bounds check before mounting
        if (partitionsCounter >= 4) {
            KDBG1("Warning: Too many partitions; max 4 supported.");
            break;
        }

        // Validate partition extent fits within the device
        uint32_t start = mbr.primaryPartition[i].start_lba;
        uint32_t length = mbr.primaryPartition[i].length;
        if (length == 0 || start >= totalSectors || length > totalSectors - start) {
            KDBG1(
                "Warning: Partition %d extent [%u, %u+%u) outside device (totalSectors=%u); "
                "skipping",
                i, start, start, length, totalSectors);
            continue;
        }

        // Mount FAT32
        if (mbr.primaryPartition[i].partition_id == 0x0C ||
            mbr.primaryPartition[i].partition_id == 0x0B) {
            FatFsWrapper* fs = new FatFsWrapper(ata, mbr.primaryPartition[i].start_lba,
                                                (BYTE)partitionsCounter,
                                                mbr.primaryPartition[i].length);
            if (!fs) {
                HALT("CRITICAL: Failed to allocate FatFsWrapper!\n");
            }
            this->partitions[partitionsCounter++] = fs;
        }
    }
}
