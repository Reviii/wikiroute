#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

void * mapFile(int mmapprot, int mmapflags, int fd) {
    struct stat sb;
    void * ptr = NULL;
    if (fstat(fd, &sb)==-1) {
        perror("fstat");
    }
    ptr = mmap(NULL, sb.st_size, mmapprot, mmapflags, fd, 0);
    if (ptr==MAP_FAILED)
        perror("mmap");
    return ptr;
}
