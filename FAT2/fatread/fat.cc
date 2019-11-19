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

// return a formatted copy of the supplied DirEntry name
// (return value must be free()'d)
char* formatDirName(char* dirName) {
    // copy name portion
    char* formattedName = (char*)malloc(13);

    unsigned int i;
    for (i = 0; i < 8; i++) {
        if (dirName[i] != ' ') {
            formattedName[i] = dirName[i];
        } 
        else break;
    }

    // check whether dirName contains a file extension
    if (dirName[8] != ' ') {
        formattedName[i] = '.';
        i++;
    } else {
        // terminate string & return
        formattedName[i] = '\0';
        return formattedName;
    }

    // copy extension
    for (unsigned int j = 8; j < 11; j++) {
        if (dirName[j] != ' ') {
            formattedName[i] = dirName[j];
            i++;
        } 
        else break;
    }
    
    // terminate string & return
    formattedName[i] = '\0';
    return formattedName;
}

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

// given a path in the filesystem: if it leads to a directory, return its entries
// if it leads to a file, return the file's entry
// also, store the number of returned entries in sizePtr
DirEntry* traversePath(const std::string &path, uint32_t* sizePtr) {
    DirEntry* result = 0;
    char* cpath = strdup(path.c_str());
    uint32_t entryCount[1];
    *entryCount = 0;
    uint32_t clusterNum = BPB.BPB_RootClus;
    char* token = strtok(cpath, "/");
    char* entryDirName;
    bool found = false;

    // first, read the root directory
    DirEntry* entries = readClusterChain(clusterNum, entryCount);
    if (entries == 0 || *entryCount == 0) goto bad;

    // traverse subdirectories
    while (token != NULL) {
        // printf("NEXT DIR: %s\n", token);

        // find the DirEntry for the next dir in path
        clusterNum = 0;
        for (uint32_t i = 0; i < *entryCount; i++) {
            // printf("----- DIR_Name: %s -----\n", (char*)(entries[i].DIR_Name));
            // format dir name to compare
            entryDirName = formatDirName((char*)(entries[i].DIR_Name));
            // printf("----- FORMATTED DIR NAME: %s -----\n", entryDirName);

            // find the matching entry, get its cluster # if needed
            if (strcasecmp((const char*)entryDirName, (const char*)token) == 0) {
                found = true;
                free(entryDirName);
                entryDirName = NULL;
                // printf("FOUND DIRECTORY: %s\n", token);

                // if this entry is for a file, return the single entry
                if (entries[i].DIR_Attr != DirEntryAttributes::DIRECTORY) {
                    // copy the entry to free the others
                    result = (DirEntry*)malloc(sizeof(DirEntry));
                    *result = entries[i];
                    *sizePtr = 1;
                    free(entries);
                    free(cpath);
                    return result;
                }

                clusterNum = ((unsigned int)entries[i].DIR_FstClusHI << 16) + ((unsigned int)entries[i].DIR_FstClusLO);
                break;
            }
            free(entryDirName);
            entryDirName = NULL;
        }

        if (!found) {
            std::cerr << "FILE OR DIRECTORY NOT FOUND\n";
            goto bad;
        }

        // handle case of '..' to root dir (FAT says clus 0, but isn't stored there)
        if (clusterNum == 0) {
            clusterNum = BPB.BPB_RootClus;
        }

        // deallocate the DirEntry array
        free(entries);
        entries = NULL;

        // read the next directory
        *entryCount = 0;
        entries = readClusterChain(clusterNum, entryCount);
        if (entries == 0 || *entryCount == 0) goto bad;
        found = false;
        token = strtok(NULL, "/");
    }

    result = entries;
    *sizePtr = *entryCount;
    free(cpath);
    return result;

bad:
    free(entries);
    free(cpath);
    return 0;
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
    // check whether a disk image has been mounted
    if (Disk == -1) {
        std::cerr << "No disk image has been mounted.\n";
        return -1;
    }

    uint32_t sizePtr[1];
    *sizePtr = 0;
    
    // retrieve the file's dirEntry
    DirEntry* entry = traversePath(path, sizePtr);
    if (entry == 0 || *sizePtr != 1) {
        std::cerr << "Failed to find file.\n";
        return -1;
    }

    // printf("FOUND FILE: %s\n", (char*)entry->DIR_Name);

    // store the entry in the file table and return its file descriptor
    for (int i = 0; i < 128; i++) {
        if (FileTable[i] == NULL) {
            FileTable[i] = entry;
            return i;
        }
    }

    std::cerr << "Open file limit (128) reached.\n";
    free(entry);
    return -1;
}

// given a file descriptor, clear the corresponding entry from the file table
bool fat_close(int fd) {
    // check whether a disk image has been mounted
    if (Disk == -1) {
        std::cerr << "No disk image has been mounted.\n";
        return false;
    }

    if (fd < 0 || fd > 127) {
        std::cerr << "File descriptor must be in range 0-127.\n";
        return false;
    }

    if (FileTable[fd] != NULL) {
        free(FileTable[fd]);
        FileTable[fd] = NULL;
    } else {
        std::cerr << "File is not currently open.\n";
        return false;
    }

    return true;
}

// read up to <count> bytes from an open file into a buffer, and return the # of bytes read
int fat_pread(int fd, void *buffer, int count, int offset) {
    // check whether a disk image is mounted and the file is open
    if (Disk == -1) {
        std::cerr << "No disk image has been mounted.\n";
        return -1;
    }
    if (fd < 0 || fd > 127) {
        std::cerr << "File descriptor must be in range 0-127.\n";
        return -1;
    }
    if (FileTable[fd] == NULL) {
        std::cerr << "File is not currently open.\n";
        return -1;
    }

    uint32_t fileSize = FileTable[fd]->DIR_FileSize;
    uint32_t clusterNum = ((unsigned int)FileTable[fd]->DIR_FstClusHI << 16) + ((unsigned int)FileTable[fd]->DIR_FstClusLO);

    // count the number of clusters in the chain
    uint32_t curClus = clusterNum;
    uint32_t clusCount = 0;
    while (curClus < 0x0FFFFFF8) {
        curClus = FAT[curClus] & 0x0FFFFFFF;
        clusCount++;
    }

    uint32_t clusterOffset, clusterIndex, readSize, bytesRemaining;
    uint32_t bytesRead = 0;
    uint32_t offsetRemaining = offset;

    if (offset > fileSize) return 0;
    if (offset + count > fileSize) {
        bytesRemaining = fileSize - offset;
    } else {
        bytesRemaining = count;
    }

    while (clusterNum < 0x0FFFFFF8) {
        // if the offset is big enough, need to skip this cluster
        if (offsetRemaining >= ClusterSize) {
            offsetRemaining -= ClusterSize;
            clusterNum = FAT[clusterNum] & 0x0FFFFFFF;
            continue;
        }

        // determine how much of this cluster to read
        if (offsetRemaining + bytesRemaining < ClusterSize) {
            readSize = bytesRemaining;
        } else readSize = ClusterSize - offsetRemaining;

        clusterOffset = (clusterNum - 2) * ClusterSize + DataOffset + offsetRemaining;

        // read from this cluster
        lseek(Disk, clusterOffset, 0);
        if (read(Disk, &(buffer[bytesRead]), readSize) == -1) {
            std::cerr << "Failed to read cluster\n";
            return -1;
        }

        bytesRead += readSize;
        bytesRemaining -= readSize;
        offsetRemaining = 0;

        // get the next cluster number
        clusterNum = FAT[clusterNum] & 0x0FFFFFFF;
    }

    return bytesRead;
}

// given a path to a directory, retrieve all entries within.
// (assume absolute paths)
std::vector<AnyDirEntry> fat_readdir(const std::string &path) {
    std::vector<AnyDirEntry> result;

    // check whether a disk image has been mounted
    if (Disk == -1) {
        std::cerr << "No disk image has been mounted.\n";
        return result;
    }

    uint32_t sizePtr[1];
    *sizePtr = 0;
    
    // retrieve the directory's entries
    DirEntry* entries = traversePath(path, sizePtr);
    if (entries == 0 || *sizePtr == 0) {
        std::cerr << "Failed to read directory.\n";
        return result;
    }

    // push each entry to the vector
    for (uint32_t i = 0; i < *sizePtr; i++) {
        AnyDirEntry entry;
        entry.dir = entries[i];
        result.push_back(entry);
    }

    free(entries);
    return result;
}
