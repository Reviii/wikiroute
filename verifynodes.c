#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "buffer.h"
#include "iteratefile.h"
#include "mapfile.h"
#include "nodetypes.h"
#include "nodeutils.h"

struct wikiNode  ** getNodesAndCheck(char * nodeData, size_t nodeDataLength, size_t * nodeCount) {
    struct wikiNode * node = (struct wikiNode *) nodeData;
    struct buffer nodeBuf = bufferCreate();
    *nodeCount = 0;
    while ((char *)node+sizeof(struct wikiNode)<=nodeData+nodeDataLength) {
        if (node->redirect) {
            printf("Node with id %zu and offset %zu does not start with null bytes\n", *nodeCount, getNodeOffset(nodeData,node));
            return NULL;
        }
        *(struct wikiNode **)bufferAdd(&nodeBuf, sizeof(struct wikiNode *)) = node;
        (*nodeCount)++;
        node = (struct wikiNode *) ((char *) node + sizeof(*node) + (node->forward_length+node->backward_length)*sizeof(node->references[0]));
    }
    *(struct wikiNode **)bufferAdd(&nodeBuf, sizeof(struct wikiNode *)) = node;
    return (struct wikiNode **) nodeBuf.content;
}

size_t * getTitleOffsetsAndCheck(FILE * f, size_t * titleCount) {
    size_t offset = 0;
    struct buffer offsetBuf = bufferCreate();
    *titleCount = 0;
    *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = 0;
    char lastC = '\0';
    iterateFile(f, c,
        if (c=='\n') {
            *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = offset+1;
            (*titleCount)++;
        }
        lastC = c;
        offset++;
    )
    if (lastC!='\n') {
        printf("Title file does not end with a newline\n");
        return NULL;
    }
    return (size_t *) offsetBuf.content;
}

int main(int argc, char ** argv) {
    char * nodeData = NULL;
    size_t nodeDataLength = 0;
    size_t nodeCount = 0;
    FILE * titleFile = NULL;
    size_t titleCount = 0;
    struct wikiNode ** nodes = NULL;
    size_t * titleOffsets = NULL;

    if (argc<3) {
        fprintf(stderr, "Usage: %s <node file> <title file>\n", argv[0]);
        return 1;
    }

    nodeData = mapFile(argv[1], O_RDONLY, PROT_READ|PROT_WRITE, MAP_PRIVATE, &nodeDataLength);
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

    nodes = getNodesAndCheck(nodeData, nodeDataLength, &nodeCount);
    if (!nodes) return 1;
    printf("Calculated offsets for %zu nodes\n", nodeCount);
    titleOffsets = getTitleOffsets(titleFile, &titleCount);
    if (!titleOffsets) return 2;
    printf("Calculated offsets for %zu titles\n", titleCount);
    if (titleCount!=nodeCount) {
        printf("Title and node count are not equal\n");
        return 3;
    }
    assert(titleCount == nodeCount);
    for (size_t i=0;i<nodeCount;i++) {
        struct wikiNode * node = nodes[i];
        nodeRef * refs = node->references;
        size_t reflen = node->forward_length+node->backward_length;
        for (size_t j=0;j<reflen;j++) {
            nodeRef ref = refs[j];
            if (ref<0||ref>=nodeCount) {
                printf("Reference out of bounds\n");
                printf("node id: %zu, node offset: %zu, reference index: %zu, reference offset: %zu\n", i, getNodeOffset(nodeData,nodes[i]), j, getNodeOffset(nodeData,nodes[i])+sizeof(struct wikiNode)+j*sizeof(nodeRef));
                return 4;
            }
        }
    }
    for (size_t i=0;i<nodeCount;i++) {
        struct wikiNode * node = nodes[i];
        node->linked_by = (nodeRef) -1;
    }
    for (size_t i=0;i<nodeCount;i++) {
        struct wikiNode * node = nodes[i];
        nodeRef * refs = node->references;
        size_t reflen = node->forward_length;
        for (size_t j=0;j<reflen;j++) {
            nodeRef ref = refs[j];
            struct wikiNode * target = nodes[ref];
            target->linked = true;
            if (target->linked_by==i) {
                printf("Duplicate reference\n");
                printf("node id: %zu, node offset: %zu, reference index: %zu, reference offset: %zu\n", i, getNodeOffset(nodeData,nodes[i]), j, getNodeOffset(nodeData,nodes[i])+sizeof(struct wikiNode)+j*sizeof(nodeRef));
                return 9;
            }
            target->linked_by = i;
        }
    }
    printf("Marked nodes that are linked by other nodes\n");
    for (size_t i=0;i<nodeCount;i++) {
        struct wikiNode * node = nodes[i];
        node->backward_length = 0;
    }
    bool backwardremoved = false;
    bool backwardkept = false;
    for (size_t i=0;i<nodeCount;i++) {
        struct wikiNode * node = nodes[i];
        nodeRef * refs = node->references;
        size_t reflen = node->forward_length;
        for (size_t j=0;j<reflen;j++) {
            nodeRef ref = refs[j];
            struct wikiNode * target = nodes[ref];
            if (target==node) continue;
            // backward_length has a different meaning here then in original file
            size_t target_size = getNodeOffset(nodeData,nodes[ref+1])-getNodeOffset(nodeData,nodes[ref]);
            if (sizeof(struct wikiNode)+target->forward_length*sizeof(nodeRef)+target->backward_length*sizeof(nodeRef)>=target_size
              || target->references[target->forward_length+target->backward_length]>i) {
                if (node->linked) {
                    printf("Missing backward ref from (id: %zu, offset: %zu, ref index: %zu, ref offset: %zu) to (id: %zu, offset: %zu, ref index: %zu, ref offset: %zu)\n", (size_t) ref, getNodeOffset(nodeData,nodes[ref]), (size_t) target->forward_length+target->backward_length, getNodeOffset(nodeData,nodes[ref])+sizeof(struct wikiNode)+target->forward_length*sizeof(nodeRef)+target->backward_length*sizeof(nodeRef), i, getNodeOffset(nodeData,nodes[i]), j, getNodeOffset(nodeData,nodes[i])+sizeof(struct wikiNode)+j*sizeof(nodeRef));
                    return 7;
                }
                backwardremoved = true;
            } else if (target->references[target->forward_length+target->backward_length]<i) {
                printf("Unexpected backward reference (id: %zu, offset: %zu, ref index: %zu, ref offset: %zu)\n", (size_t) ref, getNodeOffset(nodeData,nodes[ref]), (size_t) target->forward_length+target->backward_length, getNodeOffset(nodeData,nodes[ref])+sizeof(struct wikiNode)+target->forward_length*sizeof(nodeRef)+target->backward_length*sizeof(nodeRef));
                return 8;
            } else {
                if (!node->linked) {
                    backwardkept = true;
                }
                target->backward_length++;
            }
        }
    }
    for (size_t i=0;i<nodeCount;i++) {
        struct wikiNode * node = nodes[i];
        size_t target_size = getNodeOffset(nodeData,nodes[i+1])-getNodeOffset(nodeData,nodes[i]);
        if (sizeof(struct wikiNode)+node->forward_length*sizeof(nodeRef)+node->backward_length*sizeof(nodeRef)!=target_size) {
            printf("Unexpected backward reference near end of node (id: %zu, offset: %zu, ref index: %zu, ref offset: %zu)\n", i, getNodeOffset(nodeData,nodes[i]), (size_t) node->forward_length+node->backward_length, getNodeOffset(nodeData,nodes[i])+sizeof(struct wikiNode)+node->forward_length*sizeof(nodeRef)+node->backward_length*sizeof(nodeRef));
            return 8;
        }
    }
    printf("Checked forward/backward ref matching\n");
    if (backwardkept&&backwardremoved) {
        printf("The node file has had some unnecessary backward refs removed\n");
    } else if (backwardremoved) {
        printf("The node file has had all unnecessary backward refs removed\n");
    } else if (backwardkept) {
        printf("The node file still has all unnecessary backward refs\n");
    } else {
        printf("The node file does not contain any unnecessary backward refs\n");
    }
    printf("Did not check title ordering\n");
    printf("Verification succes\n");
    return 0;
}

