#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <dirent.h>

#define PAK_MAGIC "PACK"
#define PAK_NAME_LEN 56

struct PakHeader {
    char magic[4];
    uint32_t dirOffset;
    uint32_t dirSize;
} __attribute__((packed));

struct PakDirEntry {
    char name[PAK_NAME_LEN];
    uint32_t offset;
    uint32_t size;
} __attribute__((packed));

struct FileEntry {
    std::string name;
    std::vector<uint8_t> data;
};

static bool readFile(const std::string& path, std::vector<uint8_t>& data) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    data.resize(sz);
    if (sz > 0 && fread(data.data(), 1, sz, f) != (size_t)sz) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static void collectFiles(const std::string& baseDir, const std::string& subDir,
                         std::vector<FileEntry>& entries) {
    std::string dirPath = baseDir + "/" + subDir;
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        std::string relPath = subDir.empty() ? ent->d_name : subDir + "/" + ent->d_name;
        std::string fullPath = baseDir + "/" + relPath;
        struct stat st;
        // lstat so symlinks are detected and skipped rather than followed.
        if (lstat(fullPath.c_str(), &st) != 0) continue;
        if (S_ISLNK(st.st_mode)) continue;
        if (S_ISDIR(st.st_mode)) {
            collectFiles(baseDir, relPath, entries);
        } else if (S_ISREG(st.st_mode)) {
            FileEntry fe;
            fe.name = relPath;
            if (readFile(fullPath, fe.data)) {
                entries.push_back(fe);
                fprintf(stderr, "  pack: %s (%zu bytes)\n", fe.name.c_str(), fe.data.size());
            } else {
                fprintf(stderr, "  WARN: could not read %s\n", fullPath.c_str());
            }
        }
    }
    closedir(dir);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_dir> <output.pak>\n", argv[0]);
        return 1;
    }
    const char* inputDir = argv[1];
    const char* outputPak = argv[2];

    std::vector<FileEntry> entries;
    collectFiles(inputDir, "", entries);

    // Build PAK: header + file data + directory
    std::vector<uint8_t> pak;
    PakHeader hdr;
    memcpy(hdr.magic, PAK_MAGIC, 4);
    hdr.dirOffset = 0;
    hdr.dirSize = 0;

    pak.resize(sizeof(PakHeader));

    std::vector<PakDirEntry> dir;
    for (auto& e : entries) {
        // Reject names the PAK format cannot store rather than truncating them.
        size_t nameLen = e.name.size();
        if (nameLen >= PAK_NAME_LEN) {
            fprintf(stderr, "ERROR: entry name exceeds %d chars: %s\n", PAK_NAME_LEN,
                    e.name.c_str());
            return 1;
        }
        if (e.data.size() > UINT32_MAX) {
            fprintf(stderr, "ERROR: entry %s too large to represent (%zu bytes)\n",
                    e.name.c_str(), e.data.size());
            return 1;
        }
        if (pak.size() > UINT32_MAX) {
            fprintf(stderr, "ERROR: PAK data offset would overflow at entry: %s\n",
                    e.name.c_str());
            return 1;
        }
        PakDirEntry de;
        memset(&de, 0, sizeof(de));
        memcpy(de.name, e.name.c_str(), nameLen);
        de.offset = (uint32_t)pak.size();
        de.size = (uint32_t)e.data.size();
        dir.push_back(de);
        pak.insert(pak.end(), e.data.begin(), e.data.end());
    }

    hdr.dirOffset = (uint32_t)pak.size();
    if ((size_t)hdr.dirOffset != pak.size()) {
        fprintf(stderr, "ERROR: PAK directory offset exceeds format limit\n");
        return 1;
    }
    if ((uint64_t)dir.size() * sizeof(PakDirEntry) > UINT32_MAX) {
        fprintf(stderr, "ERROR: PAK directory size exceeds format limit\n");
        return 1;
    }
    hdr.dirSize = (uint32_t)(dir.size() * sizeof(PakDirEntry));
    for (auto& de : dir) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&de);
        pak.insert(pak.end(), ptr, ptr + sizeof(de));
    }

    memcpy(pak.data(), &hdr, sizeof(hdr));

    FILE* out = fopen(outputPak, "wb");
    if (!out) {
        fprintf(stderr, "Error: cannot write %s\n", outputPak);
        return 1;
    }
    size_t written = fwrite(pak.data(), 1, pak.size(), out);
    if (written != pak.size()) {
        fprintf(stderr, "Error: short write to %s (%zu of %zu bytes)\n", outputPak, written,
                pak.size());
        fclose(out);
        return 1;
    }
    if (fclose(out) == EOF) {
        fprintf(stderr, "Error: failed to flush/close %s\n", outputPak);
        return 1;
    }

    fprintf(stderr, "Wrote %s: %zu files, %zu bytes\n", outputPak, entries.size(), pak.size());
    return 0;
}
