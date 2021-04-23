#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"

char ** getTitleListFromFile(FILE * f, size_t * titleCount) {
    // everything between newline and \0 is a title
    struct buffer stringBuf = bufferCreate();
    struct buffer offsetBuf = bufferCreate();
    bool inTitle = true;
    int c;
    size_t* offsets = NULL;
    char ** res = NULL;

    *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = stringBuf.used;
    while ((c=fgetc(f))!=EOF) {
        if (inTitle) {
            *bufferAdd(&stringBuf, sizeof(char)) = c;
            if (!c) {
                inTitle = false;
                *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = stringBuf.used;
            }
        } else {
            if (c=='\n') {
                inTitle = true;
            }
        }
    }
    bufferCompact(&stringBuf);
    offsets = (size_t *)offsetBuf.content;
    res = malloc(offsetBuf.used/sizeof(offsets[0])*sizeof(char *));
    for (int i=0;i<offsetBuf.used/sizeof(offsets[0])-1;i++) {
        res[i] = stringBuf.content + offsets[i];
    }
    res[offsetBuf.used/sizeof(offsets[0])-1] = NULL;
    *titleCount = offsetBuf.used/sizeof(offsets[0])-1;
    free(offsets);
    // not freeing stringBuf content, because it is referenced by res
    return res;
}

size_t title2id(char ** id2title, size_t titleCount, char * title) {
    ssize_t first, last, middle;
    first = 0;
    last = titleCount - 1;
    middle = (first+last)/2;
    while (first <= last) {
//        fprintf(stderr, "Comparing to id %d/%d\n", middle, titleCount);
        int cmp = strcmp(id2title[middle], title);
        if (cmp<0) {
            first = middle+1; // overflow?
        } else if (cmp==0) {
            return middle+1;
        } else {
            last = middle-1;
        }
        middle = (first+last)/2;
    }
    return -1;
}
int main(int argc, char **argv) {
    FILE * in = NULL;
    char ** id2title;
    size_t titleCount;
    char search[256];
    if (argc<2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "-")==0) {
        in = stdin;
    } else {
        in = fopen(argv[1], "r");
    }
    if (!in) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
    }
    id2title = getTitleListFromFile(in, &titleCount);
 /*   for (int i=0;id2title[i];i++) {
        printf("%s\n", id2title[i]);
    }*/
    fprintf(stderr, "%d pages have been given an unique id\n", titleCount);
    while (fgets(search, sizeof(search), stdin)) {
        search[strlen(search)-1] = '\0'; // remove last character
        size_t id = title2id(id2title, titleCount, search);
        if (id==-1) {
            printf("Could not find '%s'\n", search);
            continue;
        }
        printf("Id: %u\n", id);
    }
    return 0;
}
