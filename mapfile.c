#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "mapfile.h"

void * mapFile(const char * path, int openflags, int mmapprot, int mmapflags, size_t * length) {
    int fd;
    struct stat sb;
    void * ptr = NULL;
    fd = open(path, openflags);
    if (fd==-1) {
        perror("fopen");
        return NULL;
    }
    if (fstat(fd, &sb)==-1) {
        perror("fstat");
        return NULL;
    }
    if (length!=NULL) *length = sb.st_size;
    ptr = mmap(NULL, sb.st_size, mmapprot, mmapflags, fd, 0);
    if (ptr==MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    if (close(fd)==-1) {
        perror("close");
        return NULL;
    }
    return ptr;
}
