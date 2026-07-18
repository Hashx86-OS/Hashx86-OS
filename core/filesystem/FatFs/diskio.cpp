/*-----------------------------------------------------------------------/
/  Low level disk I/O module for Hashx86-OS using ATA PIO driver        /
/-----------------------------------------------------------------------*/

#define KDBG_COMPONENT "DISKIO"

#include <core/filesystem/FatFs/ff.h>         /* FatFs types: BYTE, LBA_t, UINT, DWORD */
#include <core/filesystem/FatFs/diskio.h>
#include <core/drivers/ata.h>
#include <debug.h>

/*--------------------------------------------------------------------------
 * Static state: one ATA device + partition offset per physical drive (pdrv)
 * fatfs_init(pdrv, ata, startLBA) must be called before mounting.
 *--------------------------------------------------------------------------*/

static AdvancedTechnologyAttachment* g_ata[FF_VOLUMES] = {0, 0};
static uint32_t g_partLBA[FF_VOLUMES] = {0, 0};
static uint32_t g_partSize[FF_VOLUMES] = {0, 0};

void fatfs_init(BYTE pdrv, AdvancedTechnologyAttachment* ata, uint32_t partitionStartLBA,
                uint32_t partitionSizeSectors) {
    if (pdrv >= FF_VOLUMES) return;
    g_ata[pdrv] = ata;
    g_partLBA[pdrv] = partitionStartLBA;
    g_partSize[pdrv] = partitionSizeSectors;
}

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv >= FF_VOLUMES || !g_ata[pdrv]) return STA_NOINIT;
    return 0;
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv >= FF_VOLUMES || !g_ata[pdrv]) return STA_NOINIT;
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv >= FF_VOLUMES || !g_ata[pdrv]) return RES_PARERR;
    if ((uint32_t)sector >= g_partSize[pdrv] || count > g_partSize[pdrv] - (uint32_t)sector) return RES_PARERR;
    AdvancedTechnologyAttachment* ata = g_ata[pdrv];
    uint32_t start = g_partLBA[pdrv] + (uint32_t)sector;

    for (UINT i = 0; i < count; i++) {
        ata->Read28(start + i, buff + (i * 512), 512);
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv >= FF_VOLUMES || !g_ata[pdrv]) return RES_PARERR;
    if ((uint32_t)sector >= g_partSize[pdrv] || count > g_partSize[pdrv] - (uint32_t)sector) return RES_PARERR;
    AdvancedTechnologyAttachment* ata = g_ata[pdrv];
    uint32_t start = g_partLBA[pdrv] + (uint32_t)sector;

    for (UINT i = 0; i < count; i++) {
        /* const cast required by ATA interface; underlying Write28 does not mutate */
        ata->Write28(start + i, const_cast<BYTE*>(buff + (i * 512)), 512);
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv >= FF_VOLUMES || !g_ata[pdrv]) return RES_PARERR;

    switch (cmd) {
    case CTRL_SYNC:
        g_ata[pdrv]->Flush();
        return RES_OK;

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
