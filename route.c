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

static void normalizeTitle(char * title) {
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

static void nodeRoute(FILE * titles, char * nodeData, uint32_t * nodeOffsets, size_t * titleOffsets, size_t nodeCount) {
    struct buffer A = bufferCreate();
    struct buffer B = bufferCreate();
    struct buffer New = bufferCreate();
    // somehow fill the buffers A and B with nodeOffset
    while (true) {
        char * str = NULL;
        size_t len = 0;
        uint32_t res;
        int c, d;
        size_t distA =1;
        size_t distB =1;
        bool match;
        // TODO Make this more line based
        switch (c=getchar_unlocked()) {
        case EOF:
            return;
        case 'A':
        case 'B':
            if ((d=getchar())!=' ') {
                fprintf(stderr, "Invalid input\n");
                if (d!='\n') skipLine(stdin);
                break;
            }
            getline(&str, &len, stdin);
            len = strlen(str);
            if (len>0&&str[len-1]=='\n') str[len-1] = '\0';
            res = titleToNodeOffset(titles, nodeOffsets, titleOffsets, nodeCount, str);
            if (res==-1) {
                fprintf(stderr, "Could not find %s\n", str);
                break;
            }
            if (c=='A') {
                *(size_t *)bufferAdd(&A, sizeof(size_t)) = res;
            } else {
                *(size_t *)bufferAdd(&B, sizeof(size_t)) = res;
            }
            break;
        case 'R':
            if (getchar()!='\n') {
                fprintf(stderr, "Invalid input\n");
                skipLine(stdin);
                break;
            }
            for (size_t i=0;i<A.used;i+=sizeof(size_t)) {
                printf("A: %s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, *(size_t *)(A.content+i)));
            }
            for (size_t i=0;i<B.used;i+=sizeof(size_t)) {
                printf("B: %s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, *(size_t *)(B.content+i)));
            }
            match = false;
            while (!match) {
               printf("A:%zu B:%zu\n", A.used/sizeof(size_t), B.used/sizeof(size_t));
                if (A.used<=B.used) {
                    size_t * content = (size_t *)A.content;
                    struct buffer tmp;
                    printf("Checking A\n");
                    if (!A.used) break;
                    for (size_t i=0;i<A.used/sizeof(size_t);i++) {
                        struct wikiNode * node = (struct wikiNode *) (nodeData + content[i]);
                        if (node->dist_a) {
                            putchar('.');
                            continue;
                        }
                        if (node->dist_b) {
                            printf("Match @ %s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, content[i]));
                            match = true;
                        }
                        node->dist_a = distA;
                        for (size_t j=0;j<node->forward_length;j++) {
                            *(size_t *)bufferAdd(&New, sizeof(size_t)) = node->references[j];
                        }
                    }
                    printf("Checked A\n");
                    distA++;
                    tmp = A;
                    A = New;
                    New = tmp;
                    New.used=0;
                } else {
                    size_t * content = (size_t *)B.content;
                    struct buffer tmp;
                    printf("Checking B\n");
                    if (!B.used) break;
                    for (size_t i=0;i<B.used/sizeof(size_t);i++) {
                        struct wikiNode * node = (struct wikiNode *) (nodeData + content[i]);
                        if (node->dist_b) {
                            putchar(',');
                            continue;
                        }
                        if (node->dist_a) {
                            printf("Match @ %s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, content[i]));
                            match = true;
                        }
                        node->dist_b = distB;
                        for (size_t j=0;j<node->backward_length;j++) {
                            *(size_t *)bufferAdd(&New, sizeof(size_t)) = node->references[j+node->forward_length];
                        }
                    }
                    printf("Checked B\n");
                    distB++;
                    tmp = B;
                    B = New;
                    New = tmp;
                    New.used=0;
                }
                putchar('\n');
            }
            return;
        default:
            fprintf(stderr, "Invalid input\n");
            skipLine(stdin);
            break;
        }
    }
    // loop
        // pick one A/B with the smallest size
        // iterate over refernced nodes
            // mark dist
            // add references to new buffers
        // old = new
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

    nodeData = mapFile(argv[1], O_RDONLY, PROT_READ | PROT_WRITE, MAP_PRIVATE, &nodeDataLength);
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

    nodeRoute(titleFile, nodeData, nodeOffsets, titleOffsets, nodeCount);
    return 0;
}
