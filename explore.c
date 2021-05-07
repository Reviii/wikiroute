#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "buffer.h"
#include "mapfile.h"
#include "nodetypes.h"

static uint32_t * getNodeOffsets(char * nodeData, size_t nodeDataLength, size_t * nodeCount) {
    uint32_t offset = 0;
    struct buffer offsetBuf = bufferCreate();
    *nodeCount = 0;
    while (offset<nodeDataLength) {
        struct wikiNode * node = (struct wikiNode *) (nodeData + offset);
        *(uint32_t *)bufferAdd(&offsetBuf, sizeof(uint32_t)) = offset;
        (*nodeCount)++;
        offset += sizeof(*node) + (node->forward_length+node->backward_length)*sizeof(node->references[0]);
    }
    return (uint32_t *) offsetBuf.content;
}

static int skipLine(FILE * f) {
    int c;
    while ((c = getc_unlocked(f)) != '\n') {
        if (c==EOF) return EOF;
    }
    return 0;
}

static size_t * getTitleOffsets(FILE * f, size_t * titleCount) {
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

static char * getTitle(FILE * titles, size_t * titleOffsets, size_t id) {
    char * title = malloc(256);
    size_t titleLength = 256;
    fseek(titles, titleOffsets[id], SEEK_SET);
    getline(&title, &titleLength, titles);
    title[strlen(title)-1] = '\0';
    return title;
}

static char * nodeOffsetToTitle(FILE * titles, uint32_t * nodeOffsets, size_t * titleOffsets, size_t nodeCount, uint32_t nodeOffset) {
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

static uint32_t titleToNodeOffset(FILE * titles, uint32_t * nodeOffsets, size_t * titleOffsets, size_t nodeCount, char * title) {
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
int main(int argc, char ** argv) {
    char * nodeData = NULL;
    size_t nodeDataLength = 0;
    size_t nodeCount = 0;
    FILE * titleFile = NULL;
    size_t titleCount = 0;
    uint32_t * nodeOffsets = NULL;
    size_t * titleOffsets = NULL;
    uint32_t nodeOffset = 0;

    if (argc<3) {
        fprintf(stderr, "Usage: %s <node file> <title file>\n", argv[0]);
        return 1;
    }

    nodeData = mapFile(argv[1], O_RDONLY, PROT_READ, MAP_PRIVATE, &nodeDataLength);
    if (!nodeData) {
        fprintf(stderr, "Failed to mmap node file\n");
        return -1;
    }
    printf("Mmapped %d bytes\n", nodeDataLength);

    titleFile = fopen(argv[2], "r");
    if (!titleFile) {
        perror("Failed to open title file");
        return -1;
    }

    nodeOffsets = getNodeOffsets(nodeData, nodeDataLength, &nodeCount);
    printf("Calculated offsets for %zu nodes\n", nodeCount);
    titleOffsets = getTitleOffsets(titleFile, &titleCount);
    printf("Calculated offsets for %zu titles\n", titleCount);
    assert(titleCount == nodeCount);

    while (true) {
        char search[256];
        struct wikiNode * node;
        char * title;

        if (!fgets(search, 256, stdin)) continue;
        search[strlen(search)-1] = '\0';
        nodeOffset = titleToNodeOffset(titleFile, nodeOffsets, titleOffsets, nodeCount, search);
        if (nodeOffset==(size_t)-1) {
            printf("Could not find %s\n", search);
            continue;
        }

        node = (struct wikiNode *) (nodeData + nodeOffset);
        printf("Offset: %u\n", nodeOffset);
        printf("Length: %u\n", sizeof(*node) + (node->forward_length+node->backward_length)*sizeof(node->references[0]));
        title = nodeOffsetToTitle(titleFile, nodeOffsets, titleOffsets, nodeCount, nodeOffset);
        printf("Title: %s\n", title);
        if (node->forward_length+node->backward_length>500) {
            printf("Links not shown\n");
        } else {
            printf("Links to:\n");
            for (size_t i=0;i < node->forward_length;i++) {
                uint32_t offset = node->references[i];
                char * title = nodeOffsetToTitle(titleFile, nodeOffsets, titleOffsets, nodeCount, offset);
                printf("%u %s\n", offset, title);
                if (title) free(title);
            }
            printf("\nLinked by:\n");
            for (size_t i=0;i < node->backward_length;i++) {
                uint32_t offset = node->references[i+node->forward_length];
                char * title = nodeOffsetToTitle(titleFile, nodeOffsets, titleOffsets, nodeCount, offset);
                printf("%u %s\n", offset, title);
                if (title) free(title);
            }
        }
        putchar('\n');
    }
    return 0;
}
