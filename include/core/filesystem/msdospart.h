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

#ifndef FILE_SYSTEM_MSDOS
#define FILE_SYSTEM_MSDOS

#include <core/drivers/ata.h>
#include <core/filesystem/FileSystem.h>
#include <types.h>

/**
 * struct PartitionTableEntry - One 16-byte entry in the MBR partition table.
 * @bootable: Bootable flag (0x80 = active).
 * @start_head: CHS start head (legacy, unused).
 * @start_sector: CHS start sector; its top 2 bits extend the cylinder.
 * @start_cylinder: CHS start cylinder.
 * @partition_id: File system type (e.g. 0x0C = FAT32 LBA).
 * @end_head: CHS end head (legacy, unused).
 * @end_sector: CHS end sector; its top 2 bits extend the cylinder.
 * @end_cylinder: CHS end cylinder.
 * @start_lba: 32-bit LBA of the first sector.
 * @length: Number of sectors in the partition.
 */
struct PartitionTableEntry {
    uint8_t bootable;

    uint8_t start_head;
    uint8_t start_sector : 6;
    uint16_t start_cylinder : 10;

    uint8_t partition_id;

    uint8_t end_head;
    uint8_t end_sector : 6;
    uint16_t end_cylinder : 10;

    uint32_t start_lba;
    uint32_t length;
} __attribute__((packed));

/**
 * struct MasterBootRecord - The 512-byte legacy MBR sector layout.
 * @bootloader: x86 boot code area.
 * @signature: Disk signature.
 * @unused: Reserved field.
 * @primaryPartition: The four primary partition entries.
 * @magicnumber: 0x55AA boot signature.
 */
struct MasterBootRecord {
    uint8_t bootloader[440];
    uint32_t signature;
    uint16_t unused;
    PartitionTableEntry primaryPartition[4];
    uint16_t magicnumber;
} __attribute__((packed));

/**
 * class MSDOSPartitionTable - Parse and mount partitions from a legacy MBR.
 */
class MSDOSPartitionTable {
public:
    /**
     * MSDOSPartitionTable() - Bind the partition table to an ATA device.
     * @ata: The ATA device backing the disk.
     */
    MSDOSPartitionTable(AdvancedTechnologyAttachment* ata);
    /**
     * ~MSDOSPartitionTable() - Destroy the partition table.
     */
    ~MSDOSPartitionTable();
    /**
     * Initialize() - Create and format a fresh MBR layout.
     */
    void Initialize();
    /**
     * ReadPartitions() - Mount the partitions recorded in the MBR.
     */
    void ReadPartitions();
    static FileSystem* partitions[4];
    static MSDOSPartitionTable* activeInstance;

private:
    uint32_t ata_size;
    AdvancedTechnologyAttachment* ata;
    uint32_t partitionsCounter = 0;
};

#endif  // FILE_SYSTEM_MSDOS_H
