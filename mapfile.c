#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "mapfile.h"

void * mapFile(const char * path, int openflags, int mmapprot, int mmapflags, int * length) {
    int fd;
    struct stat sb;
    void * ptr = NULL;
    fd = open(path, openflags);
    if (fd==-1)
        perror("fopen");
    if (fstat(fd, &sb)==-1)
        perror("fstat");
    *length = sb.st_size;
    ptr = mmap(NULL, sb.st_size, mmapprot, mmapflags, fd, 0);
    if (ptr==MAP_FAILED)
        perror("mmap");
   if (close(fd)==-1)
        perror("close");
    return ptr;
}
