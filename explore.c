#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include "mapfile.h"
#include "nodetypes.h"

int main(int argc, char ** argv) {
    char * nodeData = NULL;
    int nodeDataLength = 0;
    int nodeOffset = 0;
    if (argc<3) {
        fprintf(stderr, "Usage: %s <node file> <title file>\n", argv[0]);
        return 1;
    }
    nodeData = mapFile(argv[1], O_RDONLY, PROT_READ, MAP_PRIVATE, &nodeDataLength);
    printf("Mapped %d bytes\n", nodeDataLength);
    printf("\nFirst node:\n");
    do {
        struct wikiNode * node = (struct wikiNode *) nodeData + nodeOffset;
        printf("Length: %u\n", sizeof(*node) + (node->forward_length+node->backward_length)*sizeof(node->references[0]));
        printf("Links to:\n");
        for (int i=0;i < node->forward_length;i++) {
            printf("%u\n", node->references[i]);
        }
        printf("Linked by:\n");
        for (int i=0;i < node->backward_length;i++) {
            printf("%u\n", node->forward_length+node->references[i]);
        }
    } while (scanf("%u", &nodeOffset)==1);
    return 0;
}
