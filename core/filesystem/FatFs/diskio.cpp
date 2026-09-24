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

#define KDBG_COMPONENT "DISKIO"

#include <core/filesystem/FatFs/ff.h>

#include <core/drivers/ata.h>
#include <core/filesystem/FatFs/diskio.h>
#include <debug.h>

/*
 * Per-volume state: one ATA device plus its partition offset and size. The
 * sizes come from MSDOSPartitionTable::Initialize(); fatfs_init() must be
 * called for a volume before FatFs mounts it.
 */
static AdvancedTechnologyAttachment* g_ata[FF_VOLUMES] = {0, 0};
static uint32_t g_partLBA[FF_VOLUMES] = {0, 0};
static uint32_t g_partSize[FF_VOLUMES] = {0, 0};

/**
 * fatfs_init() - Bind a physical drive to an ATA device and partition.
 * @pdrv: FatFs physical drive number (0-based, < FF_VOLUMES).
 * @ata: ATA device backing the drive.
 * @partitionStartLBA: First sector of the FAT32 partition on the device.
 * @partitionSizeSectors: Size of the partition in 512-byte sectors.
 *
 * Context: Called during filesystem bring-up, before the volume is mounted.
 */
void fatfs_init(BYTE pdrv, AdvancedTechnologyAttachment* ata, uint32_t partitionStartLBA,
                uint32_t partitionSizeSectors) {
    if (pdrv >= FF_VOLUMES) return;
    g_ata[pdrv] = ata;
    g_partLBA[pdrv] = partitionStartLBA;
    g_partSize[pdrv] = partitionSizeSectors;
}

/**
 * disk_initialize() - Power up and prepare a physical drive.
 * @pdrv: FatFs physical drive number.
 *
 * Return: 0 on success, STA_NOINIT if the drive was never bound.
 */
DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv >= FF_VOLUMES || !g_ata[pdrv]) return STA_NOINIT;
    return 0;
}

/**
 * disk_status() - Report the status of a physical drive.
 * @pdrv: FatFs physical drive number.
 *
 * Return: 0 (drive ready) or STA_NOINIT if the drive was never bound.
 */
DSTATUS disk_status(BYTE pdrv) {
    if (pdrv >= FF_VOLUMES || !g_ata[pdrv]) return STA_NOINIT;
    return 0;
}

/**
 * disk_read() - Read sectors from a partition into a buffer.
 * @pdrv: FatFs physical drive number.
 * @buff: Destination buffer, 512-byte sector-multiple sized.
 * @sector: Partition-relative starting sector number.
 * @count: Number of sectors to read.
 *
 * The partition-relative sector is added to the bound partition offset before
 * issuing the ATA PIO read.
 *
 * Return: RES_OK on success, RES_PARERR on an invalid range.
 */
DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv >= FF_VOLUMES || !g_ata[pdrv]) return RES_PARERR;
    if ((uint32_t)sector >= g_partSize[pdrv] || count > g_partSize[pdrv] - (uint32_t)sector)
        return RES_PARERR;
    AdvancedTechnologyAttachment* ata = g_ata[pdrv];
    uint32_t start = g_partLBA[pdrv] + (uint32_t)sector;

    for (UINT i = 0; i < count; i++) {
        ata->Read28(start + i, buff + (i * 512), 512);
    }
    return RES_OK;
}

/**
 * disk_write() - Write sectors to a partition from a buffer.
 * @pdrv: FatFs physical drive number.
 * @buff: Source buffer, 512-byte sector-multiple sized.
 * @sector: Partition-relative starting sector number.
 * @count: Number of sectors to write.
 *
 * Return: RES_OK on success, RES_PARERR on an invalid range, RES_ERROR if a
 *         sector write fails.
 */
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv >= FF_VOLUMES || !g_ata[pdrv]) return RES_PARERR;
    if ((uint32_t)sector >= g_partSize[pdrv] || count > g_partSize[pdrv] - (uint32_t)sector)
        return RES_PARERR;
    AdvancedTechnologyAttachment* ata = g_ata[pdrv];
    uint32_t start = g_partLBA[pdrv] + (uint32_t)sector;

    for (UINT i = 0; i < count; i++) {
        // Cast required by the ATA interface; Write28() does not mutate its buffer.
        if (!ata->Write28(start + i, const_cast<BYTE*>(buff + (i * 512)), 512)) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

/**
 * disk_ioctl() - Control device-dependent features.
 * @pdrv: FatFs physical drive number.
 * @cmd: Control command (CTRL_SYNC or one of the GET_* queries).
 * @buff: In/out buffer for the command.
 *
 * Return: RES_OK on success, RES_PARERR for an invalid drive or command.
 */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv >= FF_VOLUMES || !g_ata[pdrv]) return RES_PARERR;

    switch (cmd) {
        case CTRL_SYNC:
            return g_ata[pdrv]->Flush() ? RES_OK : RES_ERROR;

        case GET_SECTOR_COUNT: {
            *(DWORD*)buff = g_partSize[pdrv];
            return RES_OK;
        }

        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            return RES_OK;

        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}
