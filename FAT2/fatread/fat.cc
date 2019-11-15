// Austin Baney // ab5ep
// Based on my S19 implementation
#include "fat_internal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>


int Disk = -1;  // the FAT disk image
Fat32BPB BPB;   // the header with info like the locations of FAT and root directory
uint32_t* FAT;   // the File Allocation Table - stores next cluster number
DirEntry* FileTable[128];   // tracks open files, indexed by FDs
uint32_t FirstDataSector, DataOffset, ClusterSize, EntsPerCluster;

// read a cluster chain and return its directory entries
DirEntry* readClusterChain(uint32_t clusterNum, uint32_t* sizePtr) {
    // count the number of clusters in the chain
    uint32_t curClus = clusterNum;
    uint32_t clusCount = 0;
    while (curClus < 0x0FFFFFF8) {
        curClus = FAT[curClus] & 0x0FFFFFFF;
        clusCount++;
    }

    // allocate an array to store the directory entries
    DirEntry* entries = (DirEntry*)malloc(ClusterSize * clusCount);
    uint32_t clusterOffset, clusterIndex;
    uint32_t i = 0;

    while (clusterNum < 0x0FFFFFF8) {
        clusterOffset = (clusterNum - 2) * ClusterSize + DataOffset;
        clusterIndex = i * EntsPerCluster;

        // read this cluster
        lseek(Disk, clusterOffset, 0);
        if (read(Disk, &(entries[clusterIndex]), ClusterSize) == -1) {
            std::cerr << "Failed to read cluster #" + clusterNum;
            return 0;
        }

        // get the next cluster number
        clusterNum = FAT[clusterNum] & 0x0FFFFFFF;
        i++;
    }

    // supply the number of entries in the cluster chain
    *sizePtr = clusCount * EntsPerCluster;
    return entries;
}

bool fat_mount(const std::string &path) {
    // read the BPB from the disk into the supplied struct
    const char* cpath = path.c_str();
    Disk = open(cpath, O_RDWR, 0);
    lseek(Disk, 0, 0);
    if(read(Disk, (char*)&BPB, sizeof(BPB)) == -1) {
        std::cerr << "Failed to read BPB";
        return false;
    }

    // read the File Allocation Table from the disk
    FAT = (uint32_t*)malloc(BPB.BPB_BytsPerSec * BPB.BPB_FATSz32);
    lseek(Disk, BPB.BPB_RsvdSecCnt * BPB.BPB_BytsPerSec, 0);
    if(read(Disk, FAT, BPB.BPB_BytsPerSec * BPB.BPB_FATSz32) == -1) {
        std::cerr << "Failed to read FAT";
        return false;
    }

    // useful calculations
    FirstDataSector = BPB.BPB_RsvdSecCnt + (BPB.BPB_NumFATs * BPB.BPB_FATSz32);
    DataOffset = FirstDataSector * BPB.BPB_BytsPerSec;
    ClusterSize = BPB.BPB_BytsPerSec * BPB.BPB_SecPerClus;
    EntsPerCluster = ClusterSize / sizeof(DirEntry);

    return true;
}

int fat_open(const std::string &path) {
    return -1;
}

bool fat_close(int fd) {
    return false;
}

int fat_pread(int fd, void *buffer, int count, int offset) {
    return -1;
}

std::vector<AnyDirEntry> fat_readdir(const std::string &path) {
    std::vector<AnyDirEntry> result;
    uint32_t* entryCount;
    *entryCount = 0;

    if (path.c_str() == '/') {  // root dir
        DirEntry* entries = readClusterChain(BPB.BPB_RootClus, entryCount);
        if (entries == 0 || *entryCount == 0) return result;

        for (uint32_t i = 0; i < *entryCount; i++) {
            AnyDirEntry entry;
            entry.dir = entries[i];
            result.push_back(entry);
        }
    }

    return result;
}
