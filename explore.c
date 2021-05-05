#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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

int main(int argc, char ** argv) {
    char * nodeData = NULL;
    size_t nodeDataLength = 0;
    uint32_t nodeOffset = 0;
    if (argc<3) {
        fprintf(stderr, "Usage: %s <node file> <title file>\n", argv[0]);
        return 1;
    }
    nodeData = mapFile(argv[1], O_RDONLY, PROT_READ, MAP_PRIVATE, &nodeDataLength);
    printf("Mapped %d bytes\n", nodeDataLength);
    printf("\nFirst node:\n");
    do {
        struct wikiNode * node = (struct wikiNode *) (nodeData + nodeOffset);
        printf("Offset: %u\n", nodeOffset);
        printf("Length: %u\n", sizeof(*node) + (node->forward_length+node->backward_length)*sizeof(node->references[0]));
        if (node->forward_length+node->backward_length>100) {
            printf("Links not shown\n");
        } else {
            printf("Links to:\n");
            for (size_t i=0;i < node->forward_length;i++) {
                printf("%u\n", node->references[i]);
            }
            printf("\nLinked by:\n");
            for (size_t i=0;i < node->backward_length;i++) {
                printf("%u\n", node->forward_length+node->references[i]);
            }
        }
        putchar('\n');
    } while (scanf("%u", &nodeOffset)==1);
    return 0;
}
