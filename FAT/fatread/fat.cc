#include "fat_internal.h"
#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

int initialized = 0;
int clusterSize;
int fatSize;
int tempCombine;
int fd = -1;
int firstDataSector;

int* fatTable;

Fat32BPB fat;

// Reference to the root directory and the current working directory.
DirEntry *dirRoot;
DirEntry *cwd;

// Stores pointers for each cluster.
DirEntry* dirTable[128];

char* getFirstElement(char *path) {
  if (path == NULL || strcmp(path, "") == 0) {
    return NULL;
  }
	
  int start = path[0] == '/' ? 1 : 0;
	
  uint i;
  for (i = start; i < strlen(path); i++) {
    if (path[i] == '/') {
      break;
    }
  }
  char* pathCopy = new char[i-start+1];
  strncpy(pathCopy, path+start, i-start);
  pathCopy[i-start] = '\0';
  return pathCopy;
}

char* getRemaining(char *path) {
	
  int end;
	
  if (path[strlen(path) -1] == '/')
    end = strlen(path)-1;
  else
    end = strlen(path);
	
  int i;
  for (i = 1; i < end; i++) {
    if (path[i] == '/') {
      return path + i;
    }
  }
  return NULL;
}

int compareDirNames(char* dir1, char* dir2) {
  // dir1 = name of the directory we are searching for
	
  char dir2Temp[11+100];
	
  uint j = 0;
  while (dir2[j] != 0) {
    dir2Temp[j] = dir2[j];
    j++;
  }
	
  while (j < strlen(dir2)) {
    dir2Temp[j] = 0x20;
    j++;
  }
	
  uint i;
  for (i = 0; i < strlen(dir1) - 1; i++) {
	 
    if (dir1[i] != dir2Temp[i])  
      return 0;
  }
	
  return 1;
}



DirEntry* readClusters(uint32_t combine) {
  
  uint32_t numClusters = 0;
  uint32_t tmp = combine;
  uint32_t clusterSize = fat.BPB_SecPerClus * fat.BPB_BytsPerSec;

  // Determine overall number of clusters
  do {
    tmp = fatTable[tmp] & 0x0FFFFFFF;
    numClusters++;
  } while (tmp < 0x0FFFFFF8);

  char* clusterBlock = (char*)malloc(numClusters * clusterSize);
  uint32_t clusterItr = 1;

  // Iterate through the clusters. Seek to the last one. 
  do {
    uint32_t FirstSectorofCluster = ((combine - 2) * fat.BPB_SecPerClus) + firstDataSector;
    int dataOffset = FirstSectorofCluster * fat.BPB_BytsPerSec;
    lseek(fd, dataOffset, 0);
    int temp = read(fd, &clusterBlock[(clusterItr - 1) * clusterSize], clusterSize);
    if(temp == -1){
      std::cerr << "Read interrupted\n";
    }
    combine = fatTable[combine] & 0x0FFFFFFF;
    clusterItr++;
    
  } while (combine < 0x0FFFFFF8);

  return (DirEntry*)clusterBlock;
}

bool fat_mount(const std::string &path) {
  /**
     open the specified FAT disk image and use it for all subsequent FAT_* calls. In this function only path is a path on the underlying OS’s 
     filesystem instead of a path in the FAT volume. This should return true if the disk image is successfully opened, and false on any error.   
  **/
  const char* cpath = path.c_str();
  
  fd = open(cpath, O_RDWR, 0);
  
  if(read(fd, (char*)&fat, sizeof(fat)) == -1){
    return false;
  }
  
  if (fat.BPB_totSec16 != 0) {
    fatSize = fat.BPB_FATSz16;
  }
  else {
    fatSize = fat.BPB_FATSz32;
  }
  
  int rootDirSectors = ((fat.BPB_rootEntCnt * 32) + (fat.BPB_BytsPerSec - 1)) / fat.BPB_BytsPerSec;
  
  // Start of the data region.
  firstDataSector = fat.BPB_RsvdSecCnt + (fat.BPB_NumFATs * fatSize) + rootDirSectors;
 
  int dataOffset = firstDataSector - rootDirSectors;
  clusterSize = fat.BPB_BytsPerSec * fat.BPB_SecPerClus;
  dataOffset = dataOffset * fat.BPB_BytsPerSec;
  
  // Seek the file descriptor past the reserved section and read in the fat table.
  lseek(fd, fat.BPB_RsvdSecCnt * fat.BPB_BytsPerSec, 0);
  fatTable = (int*)malloc(fat.BPB_BytsPerSec * fatSize);
  int temp2 = read(fd, fatTable, fat.BPB_BytsPerSec*fatSize);
    if(temp2 == -1){
      std::cerr << "Read interrupted 2\n";
    }
  
  dirRoot = readClusters(fat.BPB_RootClus);
  
  // Set the current working directory to root.
  cwd = dirRoot;
  
  initialized = 1;
  return true;
}

bool fat_cd(const std::string &path) {
  if (initialized == 0) return false;
  DirEntry * tempDir;
  char* tempPath = (char*) path.c_str();
  // if absolute path, set tempDir to root
  if (tempPath[0] == '/') {
    tempDir = dirRoot;
  }
  else{
    tempDir = cwd;
  }
  char* firstElement = getFirstElement(tempPath);

  // if empty or trivial path, just cd to tempDir
  printf("firstElement = %s \n", firstElement);
  if(strcmp(tempPath, "") == 0 || firstElement == NULL){
    cwd = tempDir;
    delete[] firstElement; // dealloc the copied str
    return true;
  }

  // find the directory
  int found = -1;
  unsigned int i = 0;

  while (firstElement != NULL)
  {
    found = -1;

    // While there are still directories in the cluster.
    while (tempDir[i].DIR_Name[0] != '\0')
    {
      if (tempDir[i].DIR_Name[0] == 0xE5)
      {
        i++;
      }
      else if (compareDirNames(firstElement, (char *)tempDir[i].DIR_Name) && tempDir[i].DIR_Attr & 0x10) // only directories
      {
        found = 1;
        tempPath = getRemaining(tempPath);
        delete[] firstElement;                   // dealloc the copied str
        firstElement = getFirstElement(tempPath); // alloc a new str with the 1st element of the remaining path
        break;
      }
      i++;
    }

    if (found == -1)
    {
      if (tempDir != dirRoot && tempDir != cwd)
        free(tempDir);       // deallocate dir
      delete[] firstElement; // dealloc the copied str
      return false;
    }

    // Get pointer to where the next cluster is.
    
    uint32_t combine = ((unsigned int)tempDir[i].DIR_FstClusHI << 16) + ((unsigned int)tempDir[i].DIR_FstClusLO);
    if (tempDir != dirRoot && tempDir != cwd)
      free(tempDir); // deallocate dir
    tempDir = readClusters(combine);
  }

  // change to found directory
  if (cwd != dirRoot) free(cwd);
  cwd = tempDir;
  delete[] firstElement; // dealloc the copied str
  return true;
}

int fat_open(const std::string &path) {
    if (initialized == 0){
        return -1;
    }
    if(path.c_str() == NULL || path.c_str()[0] == 0x00){
        return -1;
    }
    int absPath = path.c_str()[0] == '/' ? 1 : 0;
    //printf("state of abs is %i \n", absPath);
    DirEntry* tempDir;
    if(absPath)
        tempDir = dirRoot;
    else
        tempDir = cwd;
    //   char* tmpPath = new char[strlen(path.c_str())+100];
    //   tmpPath = strcpy(tmpPath, path.c_str());
    char* tmpPath = (char*)path.c_str();
    //printf("current path %s \n", tempPath);
    unsigned int i = 0;

    int found = -1;

    char* firstElement = getFirstElement(tmpPath);
    // While there are still directories in the path.
    while(firstElement != NULL) {
        found = -1;
        
        // While there are still directories in the cluster.
        while (tempDir[i].DIR_Name[0] != '\0') {
            if (tempDir[i].DIR_Attr != 0x0F && tempDir[i].DIR_Attr != 0x10 && tempDir[i].DIR_Attr != 0x08 && compareDirNames(firstElement, (char *)tempDir[i].DIR_Name)) { 
                for (int j = 0; j < 128; j++) {
                    if (dirTable[j] == NULL) {
                        dirTable[j] = &tempDir[i];
                        delete[] firstElement;   // dealloc the copied str
                        return j;
                    }
                }
            }
            if (tempDir[i].DIR_Name[0] == 0xE5) {
                i++;
            }
            else {
                if (compareDirNames(firstElement, (char *) tempDir[i].DIR_Name)) {
                    found = 1;
                    tmpPath = getRemaining(tmpPath);
                    delete[] firstElement;                   // dealloc the copied str
                    firstElement = getFirstElement(tmpPath);    // alloc a new str with the 1st element of the remaining path 
                    break;
                }
                i++;
            }
        }

        if (found == -1) {
            if (tempDir != dirRoot && tempDir != cwd) free(tempDir);    // deallocate dir
            delete[] firstElement;   // dealloc the copied str
            return -1;
        }

        // Get pointer to where the next cluster is.
        uint32_t combine = ((unsigned int) tempDir[i].DIR_FstClusHI << 16) + ((unsigned int) tempDir[i].DIR_FstClusLO);
        if (tempDir != dirRoot && tempDir != cwd) free(tempDir);    // deallocate dir
        tempDir = readClusters(combine);
    }
    if (tempDir != dirRoot && tempDir != cwd) free(tempDir);    // deallocate dir
    delete[] firstElement;   // dealloc the copied str
    return -1;
}


bool fat_close(int fd) {
  if(fd < 0){
    return false;
  }
  else if(fd < 128 && dirTable[fd]){
    free(dirTable[fd]); // deallocate dir
    dirTable[fd] = NULL;
    return true;
  }
  else{
    return false;
  }
}

int fat_pread(int fd, void *buffer, int count, int offset) {
  if (initialized == 0 || fd < 0){
    return -1;
  }

  if (dirTable[fd]==NULL){
    return -1;
  }

  DirEntry* dir = dirTable[fd];
	
  int clusterNum = dir->DIR_FstClusLO;
  int firstSecOfCluster = ((clusterNum -2) *fat.BPB_SecPerClus) + firstDataSector;
	
  int maxClusSize = fat.BPB_BytsPerSec * fat.BPB_SecPerClus;
	
  int fatSecNum;
  int fatOffset; 
	
  char *fatBuffer = new char[fat.BPB_BytsPerSec];
	
  char *file = new char[dir->DIR_FileSize];
	
  uint fileIndex = 0;
	
  int bytesRead = -1;
	
  int bytesRemaining = dir->DIR_FileSize;
	
  int hexSector;
	
  while (bytesRemaining > 0) {
		
    if (bytesRemaining > maxClusSize)
      bytesRead = maxClusSize;
    else
      bytesRead = bytesRemaining;

    // Load data
    lseek(fd, firstSecOfCluster * fat.BPB_BytsPerSec, 0);
    int temp3 = read(fd, &file[fileIndex], bytesRead);
    if(temp3 == -1){
      std::cerr << "Read interrupted 2\n";
    }
    

    bytesRemaining -= bytesRead;
    fileIndex += bytesRead;

    // Determine next sector
    fatSecNum = ((clusterNum * 4) / fat.BPB_BytsPerSec) + fat.BPB_RsvdSecCnt;
    fatOffset = (clusterNum * 4) % fat.BPB_BytsPerSec;

    lseek(fd, fatSecNum * fat.BPB_BytsPerSec, 0);
    int temp4 = read(fd, fatBuffer, fat.BPB_BytsPerSec);
    if(temp4 == -1){
      std::cerr << "Read interrupted 2\n";
    }
    

    hexSector = fatBuffer[fatOffset + 1] << 16 | fatBuffer[fatOffset];
    clusterNum = (int)hexSector;

    firstSecOfCluster = ((clusterNum - 2) * fat.BPB_SecPerClus) + firstDataSector;
  }

  if (fileIndex < dir->DIR_FileSize){
      // delete fatBuffer and file?
      return -1;
  }
	
  memcpy(buffer, (void*)&file[offset], count);
	
  return fileIndex;
}

std::vector<AnyDirEntry> fat_readdir(const std::string &path) {
    std::vector<AnyDirEntry> result;
    if(initialized == 0){
        return result;
    }
    int absPath = path.c_str()[0] == '/' ? 1 : 0;
    //printf("state of abs is %i \n", absPath);
    DirEntry* tempDir;
    if(absPath)
        tempDir = dirRoot;
    else
        tempDir = cwd;
//   char* tempPath = new char[strlen(path.c_str())+100];
//   tempPath = strcpy(tempPath, path.c_str());
    char* tempPath = (char*) path.c_str();

  //printf("current path %s \n", tempPath);
    int i = 0;
    char* firstElement = getFirstElement(tempPath);
    while(firstElement != NULL || strcmp(tempPath, "/") == 0){
        while(tempDir[i].DIR_Name[0] != '\0'){
            if(compareDirNames(firstElement, (char *) tempDir[i].DIR_Name) || strcmp(tempPath, "/")) { //strcmp==0?
                //printf("Dir name is %s \n",(char *) tempDir[i].DIR_Name);
                AnyDirEntry curr;
                curr.dir = tempDir[i];
                result.push_back(curr);
                //printf("Reached");
                //tempPath = getRemaining(tempPath);
            }
            else{
                uint32_t combine = ((unsigned int) tempDir[i].DIR_FstClusHI << 16) + ((unsigned int) tempDir[i].DIR_FstClusLO);
                if (tempDir != dirRoot && tempDir != cwd) free(tempDir);    // deallocate dir
                tempDir = readClusters(combine);
            }
            i++;
        }
        tempPath = getRemaining(tempPath);
        delete[] firstElement;                       // dealloc old str, alloc new
        firstElement = getFirstElement(tempPath);
    }

    if (tempDir != dirRoot && tempDir != cwd) free(tempDir);    // deallocate dir
    delete[] firstElement;   // dealloc the copied str
    return result;
}
