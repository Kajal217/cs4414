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
uint32_t clusterSize, dataOffset, firstDataSector, fatSize;
int fd = -1;

uint32_t* fatTable;

Fat32BPB fat;

// Cluster number of the current working directory
uint32_t rootClus, CWDClus;

// Stores pointers for each cluster.
DirEntry* dirTable[128];

char* getFirstElement(char *path) {
  if (path == NULL || strcmp(path, "") == 0 || strcmp(path, "/") == 0) {
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


char * formatFilename(char * filename, uint8_t isFile){
  char *formattedName = (char *) malloc(11); 
  char * period;
  uint8_t periodIndex;
  uint32_t i;
  //format differently for file and dir names
  if(isFile){
    period = strchr(filename,'.');
    //if there isn't a period in a filename
    if(period == NULL){
      for(i = 0; i < strlen(filename); i++)
	formattedName[i] = filename[i];
      for(i = i; i < 11; i++)
	formattedName[i] = ' ';
    }
    //if there is a period in the name
    else{
      periodIndex = period - filename;
      for(i = 0; i < periodIndex; i++)
	formattedName[i] = filename[i];
      for(i = i; i < 8; i++)
	formattedName[i] = ' ';
      for(i = 0; i < (strlen(filename) - (periodIndex + 1)); i++)
	formattedName[8+i] = filename[periodIndex+1+i];
    }
  }
  else{
    for(i = 0; i < strlen(filename); i++)
      formattedName[i] = filename[i];
    for(i = i; i < 11; i++)
      formattedName[i] = ' ';
  }
  return formattedName;
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


DirEntry* readClusters(uint32_t combine, uint32_t* sizePtr) {
  
  uint32_t numClusters = 0;
  uint32_t clusNum = combine;

  // Determine number of clusters in chain
  while (clusNum < 0x0FFFFFF8){
    clusNum = fatTable[clusNum] & 0x0FFFFFFF;
    numClusters++;
  }

  DirEntry* clusterBlock = (DirEntry*)malloc(numClusters * clusterSize + sizeof(DirEntry));
  uint32_t entPerClus = clusterSize / sizeof(DirEntry);
  uint32_t clusterOffset, clusterIndex;
  clusNum = combine;
  uint32_t i = 0;

  // Read in each cluster
  while (clusNum < 0x0FFFFFF8){
    clusterOffset = (clusNum - 2) * clusterSize + dataOffset;
    clusterIndex = i * entPerClus;

    lseek(fd, clusterOffset, 0);
    int temp = read(fd, &(clusterBlock[clusterIndex]), clusterSize);
    if(temp == -1){
      std::cerr << "Read interrupted\n";
    }

    clusNum = fatTable[clusNum] & 0x0FFFFFFF;
    i++;
  }

  *sizePtr = numClusters * entPerClus;  // # of entries we return
  return clusterBlock;
}


// given a directory, read in and return all of its entries
DirEntry* getAllEntries(DirEntry* dir, uint32_t* sizePtr) {
  DirEntry* myEntries;
  
  uint32_t combine = ((unsigned int) dir->DIR_FstClusHI << 16) + ((unsigned int) dir->DIR_FstClusLO);
  myEntries = readClusters(combine, sizePtr);

  return myEntries;
}

// given a dir entry, return all of its entries that are directories
DirEntry* getDirs(DirEntry* dir, uint32_t* sizePtr) {
  DirEntry * myEntries,* myDirs,* currEnt;
  uint32_t numDirs=0;
  uint32_t numEntries[1];
  // numEntries[1] = 0;
  myEntries = getAllEntries(dir, numEntries);
  currEnt = myEntries;
  uint32_t i = 0;
  //count dirs
  while(currEnt->DIR_Name[0] != 0){  // null term or just 0?? check i < *numEntries??
    if (currEnt->DIR_Attr & DirEntryAttributes::DIRECTORY || currEnt->DIR_Attr & DirEntryAttributes::VOLUME_ID) 
      numDirs++;
    i++;
    currEnt = &(myEntries[i]);
  }

  //allocate memory according to count
  myDirs = (DirEntry*) malloc((numDirs)*sizeof(DirEntry));
  *sizePtr = numDirs;
  uint32_t dirIndex = 0;
  currEnt = myEntries;
  i=0;
  //find and copy dirs into new array
  while(currEnt->DIR_Name[0] != 0){  // ^^
    if (currEnt->DIR_Attr & DirEntryAttributes::DIRECTORY || currEnt->DIR_Attr & DirEntryAttributes::VOLUME_ID){
      memcpy(&(myDirs[dirIndex]),currEnt,sizeof(DirEntry));
      dirIndex++;
    }
    i++;
    currEnt = &(myEntries[i]);
  }  
  free (myEntries); // deallocate data copied by getAllEntries()
  return myDirs;
}

// given a CLUSTER NUMBER, return all of its entries that are directories
DirEntry* getClusDirs(uint32_t clusNum, uint32_t* sizePtr) {
  DirEntry * myEntries,* myDirs,* currEnt;
  uint32_t numDirs=0;
  uint32_t numEntries[1];
  // numEntries[1] = 0;
  myEntries = readClusters(clusNum, numEntries);
  currEnt = myEntries;
  uint32_t i = 0;
  //count dirs
  while(currEnt->DIR_Name[0] != 0){  // null term or just 0?? check i < *numEntries??
    if (currEnt->DIR_Attr & DirEntryAttributes::DIRECTORY || currEnt->DIR_Attr & DirEntryAttributes::VOLUME_ID) 
      numDirs++;
    i++;
    currEnt = &(myEntries[i]);
  }

  //allocate memory according to count
  myDirs = (DirEntry*) malloc((numDirs)*sizeof(DirEntry));
  *sizePtr = numDirs;
  uint32_t dirIndex = 0;
  currEnt = myEntries;
  i=0;
  //find and copy dirs into new array
  while(currEnt->DIR_Name[0] != 0){  // ^^
    if (currEnt->DIR_Attr & DirEntryAttributes::DIRECTORY || currEnt->DIR_Attr & DirEntryAttributes::VOLUME_ID){
      memcpy(&(myDirs[dirIndex]),currEnt,sizeof(DirEntry));
      dirIndex++;
    }
    i++;
    currEnt = &(myEntries[i]);
  }  
  free (myEntries); // deallocate data copied by getAllEntries()
  return myDirs;
}


int unterm_strcmpi(char * first, char * second){
  char firstTerm[12], secondTerm[12];
  memcpy(firstTerm, first, 11);
  memcpy(secondTerm, second, 11);
  firstTerm[11] = '\0';
  secondTerm[11] = '\0';
  int i;
  for(i = 0; i < 11; i++){
    firstTerm[i] = tolower(firstTerm[i]);
    secondTerm[i] = tolower(secondTerm[i]);
  }
  return strcmp(firstTerm, secondTerm);
}


//dirEntries is an array of dirEnts, of length numEnt, to be searched
//name is the file name as tokenized
//returns a COPY of the entry
DirEntry * findEntry(DirEntry * dirEntries,uint32_t numEnt, char * name, uint8_t isFile){
  DirEntry* myEntry = (DirEntry*) malloc(sizeof(DirEntry));
  //if not root
  if(numEnt && dirEntries[0].DIR_Name[0] == '.'){
    //if name is . or ..
    if(name[0] == '.'){
      //if ..
      if(name[1] == '.'){
        memcpy(myEntry, &(dirEntries[1]), sizeof(DirEntry));
        return myEntry;
      }
      //else .
      else{
        memcpy(myEntry, &(dirEntries[0]), sizeof(DirEntry));
        return myEntry;
      }
    }
  }
  //if root !!! DEAL WITH CASE
  // else if (numEnt && dirEntries[0].DIR_FstClusLO == 0){
  //   if(name[0] == '.') {
  //     memcpy(myEntry, dirRoot, sizeof(DirEntry));
  //     return myEntry;
  //   }
  // }

  uint32_t i;
  char * formatName = formatFilename(name, isFile);
  for(i = 0; i < numEnt; i++){
    if(unterm_strcmpi((char *)dirEntries[i].DIR_Name, formatName)==0){
      memcpy(myEntry, &(dirEntries[i]), sizeof(DirEntry));
      return myEntry;
    }
  }
  free(myEntry);
  return NULL;
}


bool fat_mount(const std::string &path) {
  /**
     open the specified FAT disk image and use it for all subsequent FAT_* calls. In this function only path is a path on the underlying OS’s 
     filesystem instead of a path in the FAT volume. This should return true if the disk image is successfully opened, and false on any error.   
  **/
  const char* cpath = path.c_str();
  
  fd = open(cpath, O_RDWR, 0);
  lseek(fd, 0, 0);
  if(read(fd, (char*)&fat, sizeof(fat)) == -1){
    return false;
  }
  
  // if (fat.BPB_totSec16 != 0) {
  //   fatSize = fat.BPB_FATSz16;
  // }
  // else {
  //   fatSize = fat.BPB_FATSz32;
  // }
  
  fatSize = fat.BPB_FATSz32;  // # of sectors per FAT
  clusterSize = fat.BPB_BytsPerSec * fat.BPB_SecPerClus;

  // rootDirOffset = (fat.BPB_BytsPerSec * fat.BPB_RsvdSecCnt) + (fat.BPB_NumFATs * fat.BPB_FATSz32)*fat.BPB_BytsPerSec;
  // int rootDirSectors = ((fat.BPB_rootEntCnt * 32) + (fat.BPB_BytsPerSec - 1)) / fat.BPB_BytsPerSec;
  int rootDirSectors = 0; // for FAT32
  
  // Start of the data region.
  //  # of DataSectors = BPB_TotSec32 – (BPB_ResvdSecCnt + (BPB_NumFATs * FATSz) + RootDirSectors);
  // CountofClusters (starting at clus 2)(rounds down) = DataSec / BPB_SecPerClus;
  // max valid clus # is CountofClusters + 1
  firstDataSector = fat.BPB_RsvdSecCnt + (fat.BPB_NumFATs * fatSize) + rootDirSectors;

  dataOffset = firstDataSector * fat.BPB_BytsPerSec;
  
  // Seek the file descriptor past the reserved section and read in the fat table.
  lseek(fd, fat.BPB_RsvdSecCnt * fat.BPB_BytsPerSec, 0);
  fatTable = (uint32_t*)malloc(fat.BPB_BytsPerSec * fatSize);
  int temp2 = read(fd, fatTable, fat.BPB_BytsPerSec*fatSize);
  if(temp2 == -1){
    std::cerr << "Read interrupted 2\n";
  }
  
  uint32_t sizePtr[1];
  *sizePtr = 0;

  // dirRoot = (DirEntry*) malloc(sizeof(DirEntry));
  // lseek(fd, rootDirOffset, 0);
  // int temp = read(fd, dirRoot, sizeof(DirEntry));
  // if(temp == -1) std::cerr << "Read interrupted\n";

  
  // Set the current working directory to root.
  rootClus = fat.BPB_RootClus;
  CWDClus = rootClus;
  
  initialized = 1;
  return true;
}

bool fat_cd(const std::string &path) {
  if(initialized == 0){
      return false;
  }
  int absPath = path.c_str()[0] == '/' ? 1 : 0;
  //printf("state of abs is %i \n", absPath);
  int start = 1;
  DirEntry* tempDir=NULL;
  uint32_t startClus;
  if(absPath)
    startClus = rootClus;
  else
    startClus = CWDClus;
  
  char* pathCopy = strdup(path.c_str()); // free this later
  char* tempPath = pathCopy;
  char* firstElement = getFirstElement(tempPath);
  uint32_t numEnts[1];
  DirEntry * myDirs;
  DirEntry * myEntry;

  // printf("pathCopy = '%s'\n", pathCopy);
  // printf("firstElement = '%s'\n", firstElement);
  //if the string is empty or '.' (CWD) or only / or /// etc (root)
  if(strcmp(pathCopy, ".") == 0 || firstElement==NULL){
    CWDClus = startClus;
    free(pathCopy);
    delete[] firstElement;
    return true;
  }

  do{
    delete[] firstElement;  // deallocate each copy before replacing
    firstElement = getFirstElement(tempPath);
    if (start==1) {
      myDirs = getClusDirs(startClus, numEnts);
      start = 0;
    }
    else {
      myDirs = getDirs(tempDir, numEnts);
      free(myEntry);
    }
    myEntry = findEntry(myDirs, *numEnts, firstElement, 0);
    if(myEntry == NULL){
      delete[] firstElement;
      free(myDirs);
      free(pathCopy);
      // if (tempDir!=NULL) free(tempDir);
      return false;
    }
    tempDir = myEntry;
    tempPath = getRemaining(tempPath);
    free(myDirs);
  } while (tempPath != NULL);

  delete[] firstElement;  // deallocate copied strings
  free(pathCopy);

  // CD to entry by saving cluster number
  CWDClus = ((unsigned int) tempDir->DIR_FstClusHI << 16) + ((unsigned int) tempDir->DIR_FstClusLO);
  free(tempDir);
  return true;
}

int fat_open(const std::string &path) {
  if(initialized == 0){
      return -1;
  }
  int absPath = path.c_str()[0] == '/' ? 1 : 0;
  //printf("state of abs is %i \n", absPath);
  int start = 1;
  DirEntry* tempDir=NULL;
  uint32_t startClus;
  if(absPath)
    startClus = rootClus;
  else
    startClus = CWDClus;
  
  char* pathCopy = strdup(path.c_str()); // free this later
  char* tempPath = pathCopy;
  char* firstElement = getFirstElement(tempPath);
  uint32_t numEnts[1];
  DirEntry * myDirs;
  DirEntry * myEntry;
  uint8_t isFile = 0;

  do{
    delete[] firstElement;  // deallocate each copy before replacing
    firstElement = getFirstElement(tempPath);
    if (start==1) {
      myDirs = getClusDirs(startClus, numEnts);
      start = 0;
    }
    else {
      myDirs = getDirs(tempDir, numEnts);
      free(myEntry);
    }
    if (getRemaining(tempPath)==NULL) { //look for file now
      isFile = 1;
    }
    myEntry = findEntry(myDirs, *numEnts, firstElement, isFile);
    if(myEntry == NULL){
      delete[] firstElement;
      free(myDirs);
      free(pathCopy);
      // if (tempDir!=NULL) free(tempDir);
      return -1;
    }
    tempDir = myEntry;
    tempPath = getRemaining(tempPath);
    free(myDirs);
  } while (tempPath != NULL);

  delete[] firstElement;  // deallocate copied strings
  free(pathCopy);

  if (tempDir->DIR_Attr != 0x0F && tempDir->DIR_Attr != 0x10 && tempDir->DIR_Attr != 0x08) {
    for (int j = 0; j < 128; j++) {
      if (dirTable[j] == NULL) {
        dirTable[j] = tempDir;
        return j;
      }
    }
  }
  
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
    // FATOffset = N * 4; (for FAT32)?
    // ThisFATSecNum = BPB_ResvdSecCnt + (FATOffset / BPB_BytsPerSec);
    // ThisFATEntOffset = FATOffset % BPB_BytsPerSec;


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
  int start = 1;
  DirEntry* tempDir=NULL;
  uint32_t startClus;
  if(absPath)
      startClus = rootClus;
  else
      startClus = CWDClus;
      
  // char* tempPath = new char[strlen(path.c_str())+100];
  // tempPath = strcpy(tempPath, path.c_str());
  // char* originalPtr = tempPath;
  char* pathCopy = strdup(path.c_str()); // free this later
  char* tempPath = pathCopy;
  char* firstElement = getFirstElement(tempPath);
  uint32_t numEnts[1];
  DirEntry * myDirs;
  DirEntry * myEntry;
  uint32_t i = 0;

  // printf("pathCopy = '%s'\n", pathCopy);
  // printf("firstElement = '%s'\n", firstElement);
  //if the string is empty or '.' (CWD) or only / or /// etc (root) read the respective clusters
  if(strcmp(pathCopy, ".") == 0 || firstElement==NULL){
    myDirs = readClusters(startClus, numEnts);
    // printf("*numEnts = '%i'\n", *numEnts);
    for(i=0; i<*numEnts; i++){  // use numEnts or DIR_Name[0] for condition?
      // printf("Dir name is '%s'\n",(char *) myDirs[i].DIR_Name);
      AnyDirEntry curr;
      curr.dir = myDirs[i];
      result.push_back(curr);
    }
    free(myDirs);
    free(pathCopy);
    delete[] firstElement;
    return result;
  }

  do{
    delete[] firstElement;  // deallocate each copy before replacing
    firstElement = getFirstElement(tempPath);
    if (start==1) {
      myDirs = getClusDirs(startClus, numEnts);
      start = 0;
    }
    else {
      myDirs = getDirs(tempDir, numEnts);
      free(myEntry);
    }
    myEntry = findEntry(myDirs, *numEnts, firstElement, 0);
    if(myEntry == NULL){
      delete[] firstElement;
      free(myDirs);
      free(pathCopy);
      // if (tempDir!=NULL) free(tempDir);
      return result;
    }
    tempDir = myEntry;
    tempPath = getRemaining(tempPath);
    free(myDirs);
  } while (tempPath != NULL);

  delete[] firstElement;  // deallocate copied strings
  free(pathCopy);

  myDirs = getAllEntries(tempDir, numEnts);
  if (tempDir!=NULL) free(tempDir);
  for(i=0; i<*numEnts; i++){  // ^^^
    AnyDirEntry curr;
    curr.dir = myDirs[i];
    result.push_back(curr);
  }
  return result;
}