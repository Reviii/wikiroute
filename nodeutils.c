#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "buffer.h"
#include "nodetypes.h"
#include "nodeutils.h"

nodeRef * getNodeOffsets(char * nodeData, size_t nodeDataLength, size_t * nodeCount) {
    nodeRef offset = 0;
    struct buffer offsetBuf = bufferCreate();
    *nodeCount = 0;
    while (offset<nodeDataLength) {
        struct wikiNode * node = (struct wikiNode *) (nodeData + offset);
        *(nodeRef *)bufferAdd(&offsetBuf, sizeof(nodeRef)) = offset;
        (*nodeCount)++;
        offset += sizeof(*node) + (node->forward_length+node->backward_length)*sizeof(node->references[0]);
    }
    return (nodeRef *) offsetBuf.content;
}

static int skipLine(FILE * f) {
    int c;
    while ((c = getc_unlocked(f)) != '\n') {
        if (c==EOF) return EOF;
    }
    return 0;
}

size_t * getTitleOffsets(FILE * f, size_t * titleCount) {
    size_t offset = 0;
    struct buffer offsetBuf = bufferCreate();
    *titleCount = 0;
    while (skipLine(f) != EOF) {
        *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = offset;
        (*titleCount)++;
        offset = ftello(f);
        assert(offset!=-1);
    }
    return (size_t *) offsetBuf.content;
}

char * getTitle(FILE * titles, size_t * titleOffsets, size_t id) {
    char * title = malloc(256);
    size_t titleLength = 256;
    fseek(titles, titleOffsets[id], SEEK_SET);
    getline(&title, &titleLength, titles);
    title[strlen(title)-1] = '\0';
    return title;
}

char * nodeOffsetToTitle(FILE * titles, nodeRef * nodeOffsets, size_t * titleOffsets, size_t nodeCount, nodeRef nodeOffset) {
    ssize_t first, middle, last;
    first = 0;
    last = nodeCount - 1;
    middle = (first+last)/2;
    while (first<=last) {
        if (nodeOffset>nodeOffsets[middle]) {
            first = middle+1;
        } else if (nodeOffset==nodeOffsets[middle]) {
            return getTitle(titles, titleOffsets, middle);
        } else {
            last = middle-1;
        }
        middle = (first+last)/2;
    }
    return NULL;
}

void normalizeTitle(char * title) {
    for (;*title;title++) {
        if (*title>='a'&&*title<='z') *title -= 32;
        if (*title=='_') *title = ' ';
    }
}

nodeRef titleToNodeOffset(FILE * titles, nodeRef * nodeOffsets, size_t * titleOffsets, size_t nodeCount, char * title) {
    ssize_t first, middle, last;
    first = 0;
    last = nodeCount - 1;
    middle = (first+last)/2;
    normalizeTitle(title);
    while (first<=last) {
        int cmp;
        char * cmpTitle = getTitle(titles, titleOffsets, middle);
        assert(cmpTitle);
        normalizeTitle(cmpTitle);
        cmp = strcmp(title, cmpTitle);
        free(cmpTitle);
        if (cmp>0) {
            first = middle+1;
        } else if (cmp==0) {
            return nodeOffsets[middle];
        } else {
            last = middle-1;
        }
        middle = (first+last)/2;
    }
    return -1;
}
