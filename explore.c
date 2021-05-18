#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "buffer.h"
#include "mapfile.h"
#include "nodetypes.h"
#include "nodeutils.h"

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

        if (!fgets(search, 256, stdin)) break;
        search[strlen(search)-1] = '\0';
        nodeOffset = titleToNodeOffset(titleFile, nodeOffsets, titleOffsets, nodeCount, search);
        if (nodeOffset==(size_t)-1) {
            printf("Could not find %s\n", search);
            continue;
        }

        node = getNode(nodeData, nodeOffset);
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
