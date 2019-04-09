#include "fat_internal.h"
#include <fstream>

//stores BPB info
struct Fat32BPB * bpb;
//stores FAT
uint16_t * fat;

uint32_t FATOffset;
uint32_t rootDirOffset;
uint32_t CWDOffset;
struct DirEntry * rootDir;
struct DirEntry * cwd;
struct DirEntry * fileDescTable[128];


bool fat_mount(const std::string &path) {
    if(path.empty()) {
        exit(0);
        return false;
    }
    else {
        std::fstream diskFD;
        diskFD.open(path, std::fstream::in | std::fstream::out);

        //read BPB header into memory
        bpb = (Fat32BPB*) malloc(sizeof(struct Fat32BPB)); // "incomplete types" ???
        diskFD.seekg(0);
        diskFD.read((char*)bpb, sizeof(struct Fat32BPB));
        
        FATOffset = bpb->BPB_BytsPerSec * bpb->BPB_RsvdSecCnt;
        rootDirOffset = FATOffset + (bpb->BPB_NumFATs * bpb->BPB_FATSz16)*bpb->BPB_BytsPerSec;

        //read root entry into memory
        rootDir = (DirEntry *) malloc(sizeof(struct DirEntry));
        diskFD.seekg(rootDirOffset);
        diskFD.read((char*)rootDir, sizeof(struct DirEntry));
        
        //read FAT into memory
        fat = (uint16_t *) malloc(bpb->BPB_FATSz16*bpb->BPB_BytsPerSec);
        diskFD.seekg(FATOffset);
        diskFD.read((char*)fat, bpb->BPB_FATSz16*bpb->BPB_BytsPerSec);
        
        //set CWD
        cwd = rootDir;
        CWDOffset = rootDirOffset;

        return true;
    }
    return false;
}

bool fat_cd(const std::string &path) {
    return false;
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

    DirEntry * curDir;
    if (path == "/"){
        curDir = rootDir;
    }
    else curDir = cwd;

    //for now...
    AnyDirEntry* rdir = (AnyDirEntry*)rootDir;
    result.push_back(*rdir);
    return result;
}
