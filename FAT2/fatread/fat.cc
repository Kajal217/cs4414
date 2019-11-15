// Austin Baney // ab5ep
// Based on my S19 implementation
#include "fat_internal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <strings.h>

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
            std::cerr << "Failed to read cluster\n";
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
        std::cerr << "Failed to read BPB\n";
        return false;
    }

    // read the File Allocation Table from the disk
    FAT = (uint32_t*)malloc(BPB.BPB_BytsPerSec * BPB.BPB_FATSz32);
    lseek(Disk, BPB.BPB_RsvdSecCnt * BPB.BPB_BytsPerSec, 0);
    if(read(Disk, FAT, BPB.BPB_BytsPerSec * BPB.BPB_FATSz32) == -1) {
        std::cerr << "Failed to read FAT\n";
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
    char* cpath = strdup(path.c_str());
    uint32_t entryCount[1];
    *entryCount = 0;
    uint32_t clusterNum = BPB.BPB_RootClus;
    char* token = strtok(cpath, "/");
    char* nextDirName;

    // first, read the root directory
    DirEntry* entries = readClusterChain(clusterNum, entryCount);
    if (entries == 0 || *entryCount == 0) goto bad;

    // traverse subdirectories
    while (token != NULL) {
        // if (strcmp(token, ".") == 0) {
        //     token = strtok(NULL, "/");
        //     continue;
        // }
        nextDirName = strdup(token);
        printf("NEXT DIR: %s\n", nextDirName);

        // find the DirEntry for the next dir in path
        clusterNum = 0;
        for (uint32_t i = 0; i < *entryCount; i++) {
            // get the matching entry's cluster number
            if (strcasecmp((const char*)entries[i].DIR_Name, (const char*)nextDirName) == 0) {
                printf("FOUND DIRECTORY: %s\n", nextDirName);
                clusterNum = ((unsigned int)entries[i].DIR_FstClusHI << 16) + ((unsigned int)entries[i].DIR_FstClusLO);
                break;
            }
        }
        free(nextDirName);
        nextDirName = NULL;

        if (clusterNum == 0) {
            std::cerr << "DIRECTORY NOT FOUND\n";
            goto bad;
        }

        // deallocate the DirEntry array
        free(entries);
        entries = NULL;

        // read the next directory
        *entryCount = 0;
        entries = readClusterChain(clusterNum, entryCount);
        if (entries == 0 || *entryCount == 0) goto bad;

        token = strtok(NULL, "/");
    }

    // push each entry to the vector
    for (uint32_t i = 0; i < *entryCount; i++) {
        AnyDirEntry entry;
        entry.dir = entries[i];
        result.push_back(entry);
    }
    free(entries);
    free(cpath);
    return result;

bad:
    free(entries);
    free(cpath);
    return result;
}
