/**
 * @file        FAT32.cpp
 * @brief       FAT32 File System Implementation
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "FAT32"
#include <core/filesystem/FAT32.h>

// Returns max valid cluster index from the BPB
static inline uint32_t fat_max_cluster(const BiosParameterBlock32& bpb) {
    return bpb.tableSize * 128;
}

// Returns true if cluster is a valid data cluster (2 .. maxCluster)
static inline bool fat_valid_cluster(uint32_t cluster, const BiosParameterBlock32& bpb) {
    return cluster >= 2 && cluster < fat_max_cluster(bpb);
}

FAT32::FAT32(AdvancedTechnologyAttachment* hd, uint32_t partitionOffset) {
    this->hd = hd;
    this->partitionOffset = partitionOffset;
    this->valid = false;

    uint8_t buffer[512];
    hd->Read28(partitionOffset, buffer, 512);
    BiosParameterBlock32* bpbPtr = (BiosParameterBlock32*)buffer;
    this->bpb = *bpbPtr;

    if (bpb.bootSignature != 0x28 && bpb.bootSignature != 0x29) {
        KDBG1("Error: Invalid Boot Signature");
        return;
    }

    // Validate BPB geometry before using any fields
    if (bpb.sectorsPerCluster == 0 ||
        (bpb.sectorsPerCluster & (bpb.sectorsPerCluster - 1)) != 0) {
        KDBG1("Error: Invalid sectorsPerCluster=%u", bpb.sectorsPerCluster);
        return;
    }
    if (bpb.fatCopies == 0) {
        KDBG1("Error: fatCopies=0");
        return;
    }
    if (bpb.tableSize == 0) {
        KDBG1("Error: tableSize=0");
        return;
    }
    uint32_t dataSectors = (bpb.totalSectorCount > bpb.reservedSectors + bpb.tableSize * bpb.fatCopies)
        ? bpb.totalSectorCount - bpb.reservedSectors - bpb.tableSize * bpb.fatCopies
        : 0;
    uint32_t maxCluster = 2 + (dataSectors / bpb.sectorsPerCluster);
    if (bpb.rootCluster < 2 || bpb.rootCluster >= maxCluster) {
        KDBG1("Error: rootCluster=%u out of range [2, %u)", bpb.rootCluster, maxCluster);
        return;
    }

    this->fatStart = partitionOffset + bpb.reservedSectors;
    this->dataStart = fatStart + (bpb.tableSize * bpb.fatCopies);
    this->rootStart = ClusterToSector(bpb.rootCluster);
    this->valid = true;

    KDBG2("Mounted.");
}

FAT32::~FAT32() {}

// --- Utils ---
// "test.txt" -> "TEST    TXT"
void FAT32::StringToFatName(char* input, char* outName, char* outExt) {
    memset(outName, ' ', 8);
    memset(outExt, ' ', 3);

    int i = 0;
    int j = 0;
    while (input[i] != '\0' && input[i] != '.' && j < 8) {
        char c = input[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        outName[j++] = c;
        i++;
    }
    while (input[i] != '\0' && input[i] != '.') i++;
    if (input[i] == '.') i++;
    j = 0;
    while (input[i] != '\0' && j < 3) {
        char c = input[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        outExt[j++] = c;
        i++;
    }
}

// --- Path Logic ---
uint32_t FAT32::ResolvePath(char* path) {
    if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) return bpb.rootCluster;  // Root

    uint32_t currentCluster = bpb.rootCluster;
    char nameBuffer[12];  // 8.3 name buffer
    int pathIdx = 0;
    if (path[0] == '/') pathIdx++;  // Skip leading slash

    while (path[pathIdx] != 0) {
        // Extract next token ("A" from "A/B")
        int nameIdx = 0;
        while (path[pathIdx] != '/' && path[pathIdx] != 0 && nameIdx < 11) {
            nameBuffer[nameIdx++] = path[pathIdx++];
        }
        nameBuffer[nameIdx] = 0;  // Null terminate token

        // Find this folder in currentCluster
        uint32_t s, o;
        DirectoryEntryFat32 entry;
        if (!FindEntryInCluster(currentCluster, nameBuffer, s, o, entry)) {
            return 0;  // Path segment not found
        }

        // Check if it's a directory
        if ((entry.attributes & 0x10) != 0x10) return 0;  // It's a file, not a dir

        // Move into it
        currentCluster = ((uint32_t)entry.firstClusterHi << 16) | entry.firstClusterLow;
        if (currentCluster == 0) currentCluster = bpb.rootCluster;

        if (path[pathIdx] == '/') pathIdx++;
    }
    return currentCluster;
}

uint32_t FAT32::ParsePath(char* path, char* filenameOut) {
    int len = strlen(path);
    int lastSlash = -1;

    // Find last slash
    for (int i = 0; i < len; i++) {
        if (path[i] == '/') lastSlash = i;
    }

    if (lastSlash == -1) {
        // No slash, "FILE.TXT" -> Root
        int copyLen = (len < 12) ? len : 12;
        for (int i = 0; i < copyLen; i++) filenameOut[i] = path[i];
        filenameOut[copyLen] = 0;
        return bpb.rootCluster;
    }

    // Copy directory portion to avoid mutating input
    char dirPath[256];
    int dirLen = lastSlash;
    if (dirLen > 255) dirLen = 255;
    for (int i = 0; i < dirLen; i++) dirPath[i] = path[i];
    dirPath[dirLen] = 0;
    uint32_t dirCluster = ResolvePath(dirPath);

    // Copy filename (max 12 chars + null = 13 byte buffer)
    int fIdx = 0;
    for (int i = lastSlash + 1; i < len && fIdx < 12; i++) {
        filenameOut[fIdx++] = path[i];
    }
    filenameOut[fIdx] = 0;  // Ensure null termination

    // Reject empty basename (e.g. trailing slash)
    if (fIdx == 0) {
        filenameOut[0] = 0;
        return 0;
    }

    return dirCluster;
}

// --- FAT Core ---
uint32_t FAT32::ClusterToSector(uint32_t cluster) {
    return dataStart + ((cluster - 2) * bpb.sectorsPerCluster);
}

uint32_t FAT32::GetFATEntry(uint32_t cluster) {
    uint32_t fatOffset = cluster * 4;
    uint32_t fatSector = fatStart + (fatOffset / 512);
    uint32_t entOffset = fatOffset % 512;
    uint8_t buffer[512];
    hd->Read28(fatSector, buffer, 512);
    uint32_t tableValue = 0;
    memcpy(&tableValue, &buffer[entOffset], sizeof(uint32_t));
    return tableValue & 0x0FFFFFFF;
}

void FAT32::SetFATEntry(uint32_t cluster, uint32_t value) {
    uint32_t fatOffset = cluster * 4;
    uint32_t entOffset = fatOffset % 512;
    uint8_t buffer[512];

    // Update all FAT copies to keep them synchronized
    for (uint32_t copy = 0; copy < bpb.fatCopies; copy++) {
        uint32_t fatSector = fatStart + (copy * bpb.tableSize) + (fatOffset / 512);
        hd->Read28(fatSector, buffer, 512);
        memcpy(&buffer[entOffset], &value, sizeof(uint32_t));
        hd->Write28(fatSector, buffer, 512);
    }
}

uint32_t FAT32::AllocateCluster() {
    // Compute maximum valid cluster from partition geometry
    // Total data sectors = bpb.totalSectorCount - reservedSectors - (tableSize * fatCopies)
    uint32_t dataSectors = (bpb.totalSectorCount > (bpb.reservedSectors + bpb.tableSize * bpb.fatCopies))
        ? bpb.totalSectorCount - bpb.reservedSectors - (bpb.tableSize * bpb.fatCopies)
        : 0;
    uint32_t maxCluster = 2 + (dataSectors / bpb.sectorsPerCluster);

    uint8_t buffer[512];
    for (int i = 0; i < bpb.tableSize; i++) {
        uint32_t sector = fatStart + i;
        hd->Read28(sector, buffer, 512);
        for (int j = 0; j < 128; j++) {
            uint32_t* entries = (uint32_t*)buffer;
            if (i == 0 && j < 2) continue;
            if ((entries[j] & 0x0FFFFFFF) == 0) {
                uint32_t clusterIdx = (i * 128) + j;
                // Bounds check against real data area
                if (clusterIdx >= maxCluster) return 0;
                SetFATEntry(clusterIdx, 0x0FFFFFFF);
                // Zero Disk
                uint32_t sectorStart = ClusterToSector(clusterIdx);
                uint8_t zeros[512];
                memset(zeros, 0, 512);
                for (int k = 0; k < bpb.sectorsPerCluster; k++)
                    hd->Write28(sectorStart + k, zeros, 512);
                return clusterIdx;
            }
        }
    }
    return 0;
}

void FAT32::FreeChain(uint32_t startCluster) {
    uint32_t current = startCluster;
    uint32_t visited = 0;
    uint32_t maxClusters = bpb.tableSize * 128;  // Max valid cluster index
    while (current >= 0x00000002 && current < 0x0FFFFFF8) {
        if (visited++ > maxClusters) {
            KDBG1("FAT corruption detected: cyclic chain or too many entries in FreeChain");
            return;
        }
        uint32_t next = GetFATEntry(current);
        SetFATEntry(current, 0x00000000);
        current = next;
    }
}

// --- Directory Internal ---
bool FAT32::FindEntryInCluster(uint32_t cluster, char* name, uint32_t& sectorOut,
                               uint32_t& offsetOut, DirectoryEntryFat32& entryOut) {
    char targetName[8];
    char targetExt[3];
    StringToFatName(name, targetName, targetExt);

    uint32_t currentCluster = cluster;
    uint8_t buffer[512];
    uint32_t visited = 0;
    uint32_t maxClusters = bpb.tableSize * 128;

    if (currentCluster < 2 || currentCluster >= 0x0FFFFFF8) {
        KDBG1("FAT corruption: invalid initial cluster %u in FindEntryInCluster", currentCluster);
        return false;
    }

    while (currentCluster < 0x0FFFFFF8) {
        if (visited++ > maxClusters) {
            KDBG1("FAT corruption: cyclic chain in FindEntryInCluster");
            return false;
        }
        uint32_t sector = ClusterToSector(currentCluster);
        for (int s = 0; s < bpb.sectorsPerCluster; s++) {
            hd->Read28(sector + s, buffer, 512);
            DirectoryEntryFat32* dirents = (DirectoryEntryFat32*)buffer;

            for (int i = 0; i < 16; i++) {
                if (dirents[i].name[0] == 0x00) return false;
                if (dirents[i].name[0] == 0xE5) continue;

                bool match = true;
                for (int k = 0; k < 8; k++)
                    if (dirents[i].name[k] != targetName[k]) match = false;
                for (int k = 0; k < 3; k++)
                    if (dirents[i].ext[k] != targetExt[k]) match = false;

                if (match) {
                    sectorOut = sector + s;
                    offsetOut = i * sizeof(DirectoryEntryFat32);
                    entryOut = dirents[i];
                    return true;
                }
            }
        }
        currentCluster = GetFATEntry(currentCluster);
        if (!fat_valid_cluster(currentCluster, this->bpb)) {
            KDBG1("FAT corruption: invalid cluster %u in FindEntryInCluster chain", currentCluster);
            return false;
        }
    }
    return false;
}

bool FAT32::FindFreeEntryInCluster(uint32_t dirCluster, uint32_t& sectorOut, uint32_t& offsetOut) {
    uint32_t currentCluster = dirCluster;
    uint8_t buffer[512];
    uint32_t visited = 0;
    uint32_t maxClusters = bpb.tableSize * 128;

    if (!fat_valid_cluster(currentCluster, this->bpb)) {
        KDBG1("FAT corruption: invalid initial cluster %u in FindFreeEntryInCluster", currentCluster);
        return false;
    }

    while (true) {
        if (visited++ > maxClusters) {
            KDBG1("FAT corruption: cyclic chain in FindFreeEntryInCluster");
            return false;
        }
        uint32_t sector = ClusterToSector(currentCluster);

        for (int i = 0; i < bpb.sectorsPerCluster; i++) {
            hd->Read28(sector + i, buffer, 512);
            DirectoryEntryFat32* dirent = (DirectoryEntryFat32*)buffer;
            for (int j = 0; j < 16; j++) {
                if (dirent[j].name[0] == 0x00 || dirent[j].name[0] == 0xE5) {
                    sectorOut = sector + i;
                    offsetOut = j * sizeof(DirectoryEntryFat32);
                    return true;
                }
            }
        }
        uint32_t next = GetFATEntry(currentCluster);
        if (next >= 0x0FFFFFF8) {
            uint32_t newCluster = AllocateCluster();
            if (newCluster == 0) return false;
            SetFATEntry(currentCluster, newCluster);
            currentCluster = newCluster;
            sectorOut = ClusterToSector(newCluster);
            offsetOut = 0;
            return true;
        }
        if (!fat_valid_cluster(next, this->bpb)) {
            KDBG1("FAT corruption: invalid cluster %u in FindFreeEntryInCluster chain", next);
            return false;
        }
        currentCluster = next;
    }
}

bool FAT32::IsDirectoryEmpty(uint32_t dirCluster) {
    uint32_t currentCluster = dirCluster;
    uint8_t buffer[512];
    uint32_t visited = 0;
    uint32_t maxClusters = bpb.tableSize * 128;

    if (!fat_valid_cluster(currentCluster, this->bpb)) {
        KDBG1("FAT corruption: invalid initial cluster %u in IsDirectoryEmpty", currentCluster);
        return true;
    }

    while (currentCluster < 0x0FFFFFF8) {
        if (visited++ > maxClusters) {
            KDBG1("FAT corruption: cyclic chain in IsDirectoryEmpty");
            return true;
        }
        uint32_t sector = ClusterToSector(currentCluster);
        for (int s = 0; s < bpb.sectorsPerCluster; s++) {
            hd->Read28(sector + s, buffer, 512);
            DirectoryEntryFat32* dirents = (DirectoryEntryFat32*)buffer;

            for (int i = 0; i < 16; i++) {
                if (dirents[i].name[0] == 0x00) return true;
                if (dirents[i].name[0] == 0xE5) continue;
                if (dirents[i].name[0] == '.') continue;
                return false;
            }
        }
        currentCluster = GetFATEntry(currentCluster);
        if (currentCluster >= 0x0FFFFFF8) break;
        if (!fat_valid_cluster(currentCluster, this->bpb)) {
            KDBG1("FAT corruption: invalid cluster %u in IsDirectoryEmpty chain", currentCluster);
            return true;
        }
    }
    return true;
}

// --- Public API ---
File* FAT32::Open(char* path) {
    if (!path || path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
        File* root = new File();
        if (!root) {
            HALT("CRITICAL: Failed to allocate File object!\n");
        }

        // Construct a root dir file
        root->name[0] = '/';
        root->name[1] = 0;
        root->size = 0;
        root->id = bpb.rootCluster;
        root->position = 0;
        root->filesystem = this;
        root->flags = 1;  // Directory
        return root;
    }

    char filename[13];
    uint32_t parentCluster = ParsePath(path, filename);

    if (parentCluster == 0) return 0;

    uint32_t s, o;
    DirectoryEntryFat32 entry;
    if (!FindEntryInCluster(parentCluster, filename, s, o, entry)) {
        return 0;  // File Not Found
    }

    // Create the File Object
    File* file = new File();
    if (!file) {
        HALT("CRITICAL: Failed to allocate File object!\n");
    }

    // Copy Name
    int i = 0;
    while (path[i] && i < 127) {
        file->name[i] = path[i];
        i++;
    }
    file->name[i] = 0;

    // Set Metadata
    file->size = entry.size;
    file->id = ((uint32_t)entry.firstClusterHi << 16) | entry.firstClusterLow;
    file->position = 0;
    file->filesystem = this;

    if (entry.attributes & 0x10) {
        file->flags = 1;  // Directory Flag
        if (file->id == 0) file->id = bpb.rootCluster;
    }

    return file;
}

// Reads from the file's CURRENT position (offset) using the Cluster ID directly
uint32_t FAT32::ReadStream(File* file, uint8_t* buffer, uint32_t length) {
    if (!file || !buffer || length == 0) return 0;
    if (file->id < 2) return 0;

    // Clamp length to remaining bytes in file (skip for directories - size is 0)
    if (!(file->flags & 1)) {
        if (file->position >= file->size) return 0;
        uint32_t remaining = file->size - file->position;
        if (length > remaining) length = remaining;
    }

    uint32_t currentCluster = file->id;
    uint32_t offset = file->position;
    uint32_t clusterSize = bpb.sectorsPerCluster * 512;

    // Skip clusters to reach the offset
    uint32_t clustersToSkip = offset / clusterSize;
    uint32_t clusterOffset = offset % clusterSize;

    for (uint32_t i = 0; i < clustersToSkip; i++) {
        if (currentCluster >= 0x0FFFFFF8) return 0;  // End reached before offset
        currentCluster = GetFATEntry(currentCluster);
        if (!fat_valid_cluster(currentCluster, this->bpb)) {
            KDBG1("FAT corruption: invalid cluster %u in ReadStream skip", currentCluster);
            return 0;
        }
    }

    // Read Loop
    uint32_t bytesRead = 0;
    uint8_t secBuff[512];
    uint32_t visited = 0;
    uint32_t maxClusters = bpb.tableSize * 128;

    while (bytesRead < length && currentCluster < 0x0FFFFFF8) {
        if (visited++ > maxClusters) {
            KDBG1("FAT corruption: cyclic chain in ReadStream");
            break;
        }
        uint32_t sector = ClusterToSector(currentCluster);

        // Read all sectors in this cluster
        for (int i = 0; i < bpb.sectorsPerCluster; i++) {
            hd->Read28(sector + i, secBuff, 512);

            for (int b = 0; b < 512; b++) {
                // If we are still skipping the initial offset bytes within this cluster
                if (clusterOffset > 0) {
                    clusterOffset--;
                    continue;
                }

                if (bytesRead < length) {
                    buffer[bytesRead++] = secBuff[b];
                } else {
                    return bytesRead;  // Done
                }
            }
        }
        currentCluster = GetFATEntry(currentCluster);
        if (currentCluster >= 0x0FFFFFF8) break;
        if (!fat_valid_cluster(currentCluster, this->bpb)) {
            KDBG1("FAT corruption: invalid cluster %u in ReadStream chain", currentCluster);
            break;
        }
    }

    return bytesRead;
}

void FAT32::ListRoot() {
    ListDir((char*)"/");
}

// Simple directory listing (kernel debug) - iterates all entries in root dir
void FAT32::ListDir(char* path) {
    uint32_t dirCluster = ResolvePath(path);
    if (dirCluster == 0) {
        KDBG1("Path not found: %s", path);
        return;
    }

    uint32_t currentCluster = dirCluster;
    uint8_t buffer[512];
    uint32_t visited = 0;
    uint32_t maxClusters = bpb.tableSize * 128;

    if (!fat_valid_cluster(currentCluster, this->bpb)) {
        KDBG1("FAT corruption: invalid initial cluster %u in ListDir", currentCluster);
        return;
    }

    KDBG2("Listing: %s", path);

    while (currentCluster < 0x0FFFFFF8) {
        if (visited++ > maxClusters) {
            KDBG1("FAT corruption: cyclic chain in ListDir");
            return;
        }
        uint32_t sector = ClusterToSector(currentCluster);
        for (int s = 0; s < bpb.sectorsPerCluster; s++) {
            hd->Read28(sector + s, buffer, 512);
            DirectoryEntryFat32* dirents = (DirectoryEntryFat32*)buffer;

            for (int i = 0; i < 16; i++) {
                if (dirents[i].name[0] == 0x00) return;
                if (dirents[i].name[0] == 0xE5) continue;
                if ((dirents[i].attributes & 0x0F) == 0x0F) continue;

                char name[9];
                for (int k = 0; k < 8; k++) name[k] = dirents[i].name[k];
                name[8] = 0;
                for (int k = 7; k >= 0; k--) {
                    if (name[k] == ' ')
                        name[k] = 0;
                    else
                        break;
                }
                char logBuf[64];
                int len = 0;
                for (int x = 0; name[x] != 0 && len < 63; x++) logBuf[len++] = name[x];
                if ((dirents[i].attributes & 0x10)) {
                    logBuf[len++] = '/';
                }
                logBuf[len] = 0;
                KDBG2(" %s", logBuf);
            }
        }
        currentCluster = GetFATEntry(currentCluster);
        if (currentCluster >= 0x0FFFFFF8) return;
        if (!fat_valid_cluster(currentCluster, this->bpb)) {
            KDBG1("FAT corruption: invalid cluster %u in ListDir chain", currentCluster);
            return;
        }
    }
}

void FAT32::CreateFile(char* path) {
    char filename[13];
    uint32_t parentCluster = ParsePath(path, filename);
    if (parentCluster == 0) {
        KDBG1("Error: Parent directory not found.");
        return;
    }

    KDBG2("Create File: %s", filename);

    uint32_t s, o;
    DirectoryEntryFat32 e;
    if (FindEntryInCluster(parentCluster, filename, s, o, e)) {
        KDBG1("Error: Exists.");
        return;
    }

    uint32_t newCluster = AllocateCluster();
    if (newCluster == 0) {
        KDBG1("Disk Full");
        return;
    }

    if (!FindFreeEntryInCluster(parentCluster, s, o)) {
        KDBG1("Dir Full");
        FreeChain(newCluster);
        return;
    }

    DirectoryEntryFat32 newEntry;
    memset(&newEntry, 0, sizeof(DirectoryEntryFat32));
    StringToFatName(filename, (char*)newEntry.name, (char*)newEntry.ext);
    newEntry.attributes = 0x20;
    newEntry.firstClusterLow = newCluster & 0xFFFF;
    newEntry.firstClusterHi = (newCluster >> 16) & 0xFFFF;

    uint8_t buffer[512];
    hd->Read28(s, buffer, 512);
    uint8_t* dest = buffer + o;
    uint8_t* src = (uint8_t*)&newEntry;
    for (int i = 0; i < sizeof(DirectoryEntryFat32); i++) dest[i] = src[i];
    hd->Write28(s, buffer, 512);

    KDBG2("Created.");
}

void FAT32::DeleteFile(char* path) {
    char filename[13];
    uint32_t parentCluster = ParsePath(path, filename);
    if (parentCluster == 0) {
        KDBG1("Invalid Path");
        return;
    }

    KDBG2("Deleting: %s... ", filename);

    uint32_t s, o;
    DirectoryEntryFat32 entry;

    if (!FindEntryInCluster(parentCluster, filename, s, o, entry)) {
        KDBG1("Not Found.");
        return;
    }

    if ((entry.attributes & 0x10) == 0x10) {
        KDBG1("Is Dir. Use DeleteDir.");
        return;
    }

    // Mark Deleted
    uint8_t buffer[512];
    hd->Read28(s, buffer, 512);
    buffer[o] = 0xE5;
    hd->Write28(s, buffer, 512);

    uint32_t startCluster = ((uint32_t)entry.firstClusterHi << 16) | entry.firstClusterLow;
    if (startCluster != 0) FreeChain(startCluster);

    KDBG2("Deleted.");
}

void FAT32::DeleteDirectory(char* path) {
    char dirname[13];
    uint32_t parentCluster = ParsePath(path, dirname);
    if (parentCluster == 0) {
        KDBG1("Invalid Path");
        return;
    }

    KDBG2("Deleting Dir: %s", dirname);

    uint32_t s, o;
    DirectoryEntryFat32 entry;

    if (!FindEntryInCluster(parentCluster, dirname, s, o, entry)) {
        KDBG1("Not Found.");
        return;
    }

    if ((entry.attributes & 0x10) != 0x10) {
        KDBG1("Not a Dir.");
        return;
    }

    uint32_t startCluster = ((uint32_t)entry.firstClusterHi << 16) | entry.firstClusterLow;
    if (!IsDirectoryEmpty(startCluster)) {
        KDBG1("Not Empty.");
        return;
    }

    uint8_t buffer[512];
    hd->Read28(s, buffer, 512);
    buffer[o] = 0xE5;
    hd->Write28(s, buffer, 512);

    if (startCluster != 0) FreeChain(startCluster);

    KDBG2("Dir Deleted");
}

void FAT32::MakeDirectory(char* path) {
    char dirname[13];
    uint32_t parentCluster = ParsePath(path, dirname);

    if (parentCluster == 0) {
        KDBG1("Invalid Path");
        return;
    }

    KDBG2("Create Dir: %s", dirname);

    uint32_t s, o;
    DirectoryEntryFat32 e;
    if (FindEntryInCluster(parentCluster, dirname, s, o, e)) return;

    uint32_t newCluster = AllocateCluster();
    if (newCluster == 0) return;

    if (!FindFreeEntryInCluster(parentCluster, s, o)) {
        FreeChain(newCluster);
        return;
    }

    DirectoryEntryFat32 newEntry;
    memset(&newEntry, 0, sizeof(DirectoryEntryFat32));
    StringToFatName(dirname, (char*)newEntry.name, (char*)newEntry.ext);
    newEntry.attributes = 0x10;
    newEntry.firstClusterLow = newCluster & 0xFFFF;
    newEntry.firstClusterHi = (newCluster >> 16) & 0xFFFF;

    uint8_t buffer[512];
    hd->Read28(s, buffer, 512);
    uint8_t* dest = buffer + o;
    uint8_t* src = (uint8_t*)&newEntry;
    for (int i = 0; i < sizeof(DirectoryEntryFat32); i++) dest[i] = src[i];
    hd->Write28(s, buffer, 512);

    // Init . and ..
    memset(buffer, 0, 512);
    DirectoryEntryFat32* dot = (DirectoryEntryFat32*)buffer;
    DirectoryEntryFat32* dotdot = (DirectoryEntryFat32*)(buffer + sizeof(DirectoryEntryFat32));

    memset(dot->name, ' ', 11);
    dot->name[0] = '.';
    dot->attributes = 0x10;
    dot->firstClusterLow = newCluster & 0xFFFF;
    dot->firstClusterHi = (newCluster >> 16) & 0xFFFF;

    memset(dotdot->name, ' ', 11);
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    dotdot->attributes = 0x10;
    if (parentCluster == bpb.rootCluster) parentCluster = 0;
    dotdot->firstClusterLow = parentCluster & 0xFFFF;
    dotdot->firstClusterHi = (parentCluster >> 16) & 0xFFFF;

    hd->Write28(ClusterToSector(newCluster), buffer, 512);
    KDBG2("Dir Created");
}

void FAT32::Format() {
    KDBG2("Formatting Drive (Quick Format)... ");

    uint8_t zeros[512];
    memset(zeros, 0, 512);
    uint8_t fatHeader[512];  // Preserved template with FAT markers

    uint32_t* fatStartArr = (uint32_t*)zeros;
    fatStartArr[0] = 0x0FFFFFF8;
    fatStartArr[1] = 0x0FFFFFFF;
    fatStartArr[2] = 0x0FFFFFFF;

    // Preserve the first-sector template before zeroing
    memcpy(fatHeader, zeros, 512);

    // Write full FAT for all copies
    for (uint32_t copy = 0; copy < bpb.fatCopies; copy++) {
        uint32_t fatBase = this->fatStart + (copy * bpb.tableSize);
        // First sector has the special markers — use the preserved template
        hd->Write28(fatBase, fatHeader, 512);
        // Remaining sectors are zeroed
        memset(zeros, 0, 512);
        for (uint32_t i = 1; i < bpb.tableSize; i++) {
            hd->Write28(fatBase + i, zeros, 512);
        }
    }

    uint32_t rootSec = ClusterToSector(bpb.rootCluster);
    memset(zeros, 0, 512);
    for (int i = 0; i < bpb.sectorsPerCluster; i++) {
        hd->Write28(rootSec + i, zeros, 512);
    }

    KDBG2("Done. Please Reboot.");
}

void FAT32::ReadFile(char* path, uint8_t* buffer, uint32_t length) {
    char filename[13];
    uint32_t parentCluster = ParsePath(path, filename);
    if (parentCluster == 0) {
        KDBG1("Invalid path");
        return;
    }

    uint32_t s, o;
    DirectoryEntryFat32 entry;
    if (!FindEntryInCluster(parentCluster, filename, s, o, entry)) {
        KDBG1("File not found");
        return;
    }

    if (length > entry.size) length = entry.size;

    uint32_t currentCluster = ((uint32_t)entry.firstClusterHi << 16) | entry.firstClusterLow;
    if (entry.size == 0 || currentCluster < 2) return;
    uint32_t bytesRead = 0;
    uint8_t secBuff[512];
    uint32_t visited = 0;
    uint32_t maxClusters = bpb.tableSize * 128;

    while (bytesRead < length && currentCluster < 0x0FFFFFF8) {
        if (visited++ > maxClusters) {
            KDBG1("FAT corruption: cyclic chain in ReadFile");
            break;
        }
        uint32_t sector = ClusterToSector(currentCluster);
        for (int i = 0; i < bpb.sectorsPerCluster; i++) {
            hd->Read28(sector + i, secBuff, 512);
            for (int b = 0; b < 512; b++) {
                if (bytesRead < length)
                    buffer[bytesRead++] = secBuff[b];
                else
                    break;
            }
            if (bytesRead >= length) break;
        }
        currentCluster = GetFATEntry(currentCluster);
        if (currentCluster >= 0x0FFFFFF8) break;
        if (!fat_valid_cluster(currentCluster, this->bpb)) {
            KDBG1("FAT corruption: invalid cluster %u in ReadFile chain", currentCluster);
            break;
        }
    }
}

void FAT32::WriteFile(char* path, uint8_t* buffer, uint32_t length) {
    char filename[13];
    uint32_t parentCluster = ParsePath(path, filename);
    if (parentCluster == 0) {
        KDBG1("Invalid Path");
        return;
    }

    uint32_t dirSector, dirOffset;
    DirectoryEntryFat32 entry;

    // Find Entry
    if (!FindEntryInCluster(parentCluster, filename, dirSector, dirOffset, entry)) {
        KDBG1("Not Found.");
        return;
    }

    // Allocate First Cluster if Empty
    uint32_t currentCluster = ((uint32_t)entry.firstClusterHi << 16) | entry.firstClusterLow;
    if (currentCluster == 0) {
        currentCluster = AllocateCluster();
        if (currentCluster == 0) return;

        entry.firstClusterLow = currentCluster & 0xFFFF;
        entry.firstClusterHi = (currentCluster >> 16) & 0xFFFF;

        uint8_t dirBuff[512];
        hd->Read28(dirSector, dirBuff, 512);
        DirectoryEntryFat32* onDisk = (DirectoryEntryFat32*)(dirBuff + dirOffset);
        onDisk->firstClusterLow = entry.firstClusterLow;
        onDisk->firstClusterHi = entry.firstClusterHi;
        hd->Write28(dirSector, dirBuff, 512);
    }

    // Write Data
    uint32_t bytesWritten = 0;
    uint8_t secBuff[512];
    uint32_t visited = 0;
    uint32_t maxClusters = bpb.tableSize * 128;

    while (bytesWritten < length) {
        if (visited++ > maxClusters) {
            KDBG1("FAT corruption: cyclic chain in WriteFile");
            break;
        }
        uint32_t sector = ClusterToSector(currentCluster);

        for (int i = 0; i < bpb.sectorsPerCluster; i++) {
            memset(secBuff, 0, 512);
            for (int b = 0; b < 512; b++) {
                if (bytesWritten < length) {
                    secBuff[b] = buffer[bytesWritten++];
                }
            }
            hd->Write28(sector + i, secBuff, 512);
            if (bytesWritten >= length) break;
        }

        if (bytesWritten >= length) break;

        uint32_t next = GetFATEntry(currentCluster);
        if (next >= 0x0FFFFFF8) {
            next = AllocateCluster();
            if (next == 0) break;
            SetFATEntry(currentCluster, next);
        } else if (!fat_valid_cluster(next, this->bpb)) {
            KDBG1("FAT corruption: invalid cluster %u in WriteFile chain", next);
            break;
        }
        currentCluster = next;
    }

    // Free any trailing clusters left from a previous larger file
    if (currentCluster >= 2 && fat_valid_cluster(currentCluster, this->bpb)) {
        uint32_t tail = GetFATEntry(currentCluster);
        if (tail >= 2 && tail < 0x0FFFFFF8) {
            SetFATEntry(currentCluster, 0x0FFFFFFF);
            FreeChain(tail);
        }
    }

    uint8_t dirBuff[512];
    hd->Read28(dirSector, dirBuff, 512);
    DirectoryEntryFat32* onDisk = (DirectoryEntryFat32*)(dirBuff + dirOffset);
    onDisk->size = bytesWritten;
    hd->Write28(dirSector, dirBuff, 512);

    KDBG2("Written.");
}

// Returns the size of a file in bytes. Returns 0 if file not found.
uint32_t FAT32::GetFileSize(char* path) {
    char filename[13];

    // Parse the path ("DIR/FILE.TXT")
    uint32_t parentCluster = ParsePath(path, filename);

    if (parentCluster == 0) {
        return 0;  // Parent directory not found
    }

    uint32_t s, o;
    DirectoryEntryFat32 entry;

    // Find the file entry within the parent cluster
    if (!FindEntryInCluster(parentCluster, filename, s, o, entry)) {
        return 0;  // File Not Found
    }

    return entry.size;
}

void FAT32::FormatRaw(AdvancedTechnologyAttachment* hd, uint32_t startSector,
                      uint32_t sizeSectors) {
    KDBG2("Formatting Raw Partition at %d (Size: %d)... ", startSector, sizeSectors);

    // Calculate FAT32 Geometry
    uint16_t bytesPerSec = 512;
    uint8_t secPerClus = 8;  // Standard 4KB cluster
    uint16_t reserved = 32;
    uint8_t fats = 2;

    // Calculate FAT Size
    // TotalSectors = Reserved + (FatSize * Fats) + (TotalClusters * SecPerClus)
    uint32_t usableSectors = sizeSectors - reserved;

    // FatSizeSectors = (usableSectors / SecPerClus) * 4 / 512
    uint32_t sectorsPerFat = (usableSectors / secPerClus * 4) / 512 + 1;  // +1 for safety

    // Create BPB
    uint8_t buffer[512];
    memset(buffer, 0, 512);
    BiosParameterBlock32* bpb = (BiosParameterBlock32*)buffer;

    // Jump Code
    bpb->jump[0] = 0xEB;
    bpb->jump[1] = 0x58;
    bpb->jump[2] = 0x90;

    // OEM Name
    char* oem = (char*)"HASHX86 ";
    for (int i = 0; i < 8; i++) bpb->softName[i] = oem[i];

    bpb->bytesPerSector = bytesPerSec;
    bpb->sectorsPerCluster = secPerClus;
    bpb->reservedSectors = reserved;
    bpb->fatCopies = fats;
    bpb->rootDirEntries = 0;  // FAT32 doesn't use this
    bpb->totalSectors = 0;    // FAT32 uses totalSectorCount (LBA)
    bpb->mediaType = 0xF8;
    bpb->fatSectorCount = 0;  // FAT32 uses tableSize
    bpb->sectorsPerTrack = 63;
    bpb->headCount = 255;
    bpb->hiddenSectors = startSector;
    bpb->totalSectorCount = sizeSectors;

    // FAT32 Specific
    bpb->tableSize = sectorsPerFat;
    bpb->extFlags = 0;
    bpb->fatVersion = 0;
    bpb->rootCluster = 2;  // Standard Root Cluster
    bpb->fatInfo = 1;      // FSInfo Sector
    bpb->backupSector = 6;
    bpb->driveNumber = 0x80;
    bpb->bootSignature = 0x29;
    bpb->volumeID = 0x12345678;
    char* volLabel = (char*)"NO NAME    ";
    for (int i = 0; i < 11; i++) bpb->fatTypeLabel[i] = volLabel[i];
    char* sysType = (char*)"FAT32   ";
    for (int i = 0; i < 8; i++) bpb->fileSystemType[i] = sysType[i];

    // Write Boot Sector
    hd->Write28(startSector, buffer, 512);

    // Initialize FAT Tables
    memset(buffer, 0, 512);

    // First sector of FAT needs special markers
    uint32_t* fatEntries = (uint32_t*)buffer;
    fatEntries[0] = 0x0FFFFFF8;  // Media Type
    fatEntries[1] = 0x0FFFFFFF;  // EOC
    fatEntries[2] = 0x0FFFFFFF;  // EOC (End of Root Dir Chain)

    // Preserve the first-sector template
    uint8_t markerBuffer[512];
    memcpy(markerBuffer, buffer, 512);

    // Write all FAT sectors for each FAT copy
    for (uint32_t copy = 0; copy < fats; copy++) {
        uint32_t fatBaseSector = startSector + reserved + (copy * sectorsPerFat);
        // First sector has the special markers — use preserved template
        hd->Write28(fatBaseSector, markerBuffer, 512);
        // Remaining sectors are zeroed
        memset(buffer, 0, 512);
        for (uint32_t i = 1; i < sectorsPerFat; i++) {
            hd->Write28(fatBaseSector + i, buffer, 512);
        }
    }

    // Clear Root Directory Cluster (Cluster 2)
    // Data Start = Reserved + (FatSize * Fats)
    uint32_t dataStart = startSector + reserved + (sectorsPerFat * fats);
    // Cluster 2 is the very first cluster in Data Area
    for (int i = 0; i < secPerClus; i++) {
        hd->Write28(dataStart + i, buffer, 512);
    }

    KDBG2("Raw Format Done. Please reboot.");
}
