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
    nodeRef * nodeOffsets = NULL;
    size_t * titleOffsets = NULL;

    if (argc<3) {
        fprintf(stderr, "Usage: %s <node file> <title file>\n", argv[0]);
        return 1;
    }

    nodeData = mapFile(argv[1], O_RDONLY, PROT_READ, MAP_PRIVATE, &nodeDataLength);
    if (!nodeData) {
        fprintf(stderr, "Failed to mmap node file\n");
        return -1;
    }
    printf("Mmapped %zu bytes\n", nodeDataLength);

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
        nodeRef id = -1;
        nodeRef nodeOffset = -1;

        if (!fgets(search, 256, stdin)) break;
        search[strlen(search)-1] = '\0';
        if (search[0]==' '&&(search[1]=='#'||search[1]=='$')) {
            long long input = strtoll(search+2, NULL, 0);
            // don't check whether this is a valid offset
            if (search[1]=='#') {
                nodeOffset = input;
                id = nodeOffsetToId(nodeOffsets, nodeCount, nodeOffset);
            } else if (search[1]=='$') {
                id = input;
                nodeOffset = nodeOffsets[id];
            }
        } else {
            id = titleToNodeId(titleFile, titleOffsets, nodeCount, search);
            if (id==-1) {
                nodeOffset = -1;
            } else {
                nodeOffset = nodeOffsets[id];
            }
        }
        if (nodeOffset==(nodeRef)-1) {
            printf("Could not find %s\n", search);
            continue;
        }

        node = getNode(nodeData, nodeOffset);
        if (node->redirect) printf("Corrupted node\n");
        if (id!=(nodeRef)-1) printf("Id: %u\n", id);
        printf("Offset: %u\n", nodeOffset);
        printf("Size: %zu\n", sizeof(*node) + (node->forward_length+node->backward_length)*sizeof(node->references[0]));
        title = nodeOffsetToTitle(titleFile, nodeOffsets, titleOffsets, nodeCount, nodeOffset);
        printf("Title: %s\n", title);
        printf("Links to %zu pages", (size_t) node->forward_length);
        if (node->forward_length<=500) {
            printf(":\n");
            for (size_t i=0;i < node->forward_length;i++) {
                uint32_t offset = node->references[i];
                char * title = nodeOffsetToTitle(titleFile, nodeOffsets, titleOffsets, nodeCount, offset);
                printf("%u %s\n", offset, title);
                if (title) free(title);
            }
        }
        putchar('\n');
        printf("Linked by %zu pages", (size_t) node->backward_length);
        if (node->backward_length<=500) {
            printf(":\n");
            for (size_t i=0;i < node->backward_length;i++) {
                uint32_t offset = node->references[i+node->forward_length];
                char * title = nodeOffsetToTitle(titleFile, nodeOffsets, titleOffsets, nodeCount, offset);
                printf("%u %s\n", offset, title);
                if (title) free(title);
            }
        } else {
            putchar('\n');
        }
        putchar('\n');
    }
    return 0;
}
