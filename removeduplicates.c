#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "buffer.h"
#include "iteratefile.h"

int main(int argc, char ** argv) {
    FILE * in = NULL;
    FILE * out = NULL;
    if (argc<2) {
        in = stdin;
        out = stdout;
    } else {
        in = fopen(argv[1], "r");
        out = fopen(argv[1], "r+");
    }
    if (!in||!out) {
        perror("Failed to open file");
        return 1;
    }
    struct buffer prevLine = bufferCreate();
    struct buffer curLine = bufferCreate();
    *bufferAdd(&prevLine, 1) = '\0';
    prevLine.used--;
    iterateFile(in, c,
        *bufferAdd(&curLine, 1) = c;
        if (c=='\n') {
            *bufferAdd(&curLine, 1) = '\0';
            curLine.used--;
            if (strcmp(prevLine.content, curLine.content)==0) {
                fprintf(stderr, "Removed duplicate: '%s'\n", prevLine.content);
            } else {
                fwrite_unlocked(prevLine.content, prevLine.used, 1, out);
            }
            struct buffer tmp = prevLine;
            prevLine = curLine;
            curLine = tmp;
            curLine.used = 0;
        }
    )
    fwrite_unlocked(prevLine.content, prevLine.used, 1, out);
    if (curLine.used) {
        fprintf(stderr, "Error: last line not terminated with newline\n");
        fwrite_unlocked(curLine.content, curLine.used, 1, out);
    }
    if (argc<2) return 0;
    int fd = fileno(out);
    fprintf(stderr, "Truncating to %ld/%ld bytes\n", ftell(out), ftell(in));
    if (ftruncate(fd, ftell(out))<0) perror("Failed to truncate output file");
    if (fclose(in)<0) perror("Failed to close input file");
    if (fclose(out)<0) perror("Failed to close output file");
    return 0;
}
