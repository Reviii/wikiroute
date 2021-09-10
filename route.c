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

static void cleanNodes(char * nodeData, nodeRef * nodeOffsets, size_t nodeCount) {
    for (size_t i=0;i<nodeCount;i++) {
        struct wikiNode * node = getNode(nodeData, nodeOffsets[i]);
        node->dist_a = 0;
        node->dist_b = 0;
    }
}

static void freePrint(char * format, char * str) {
    printf(format, str);
    free(str);
}

static bool shouldChooseSideA(int distA, int distB, struct buffer A, struct buffer B) {
    if (distA==1) return true;
    if (distB==1) return false;
    if (distA==2) return true;
    return A.used<=B.used;
}

static void nodeRoute(struct buffer A, struct buffer B, FILE * titles, char * nodeData, nodeRef * nodeOffsets, size_t * titleOffsets, size_t nodeCount) {
    size_t distA =1;
    size_t distB =1;
    bool match = false;
    struct buffer matches = bufferCreate();
    struct buffer New = bufferCreate();
    struct buffer originalA = bufferDup(A);
    A = bufferDup(A);
    B = bufferDup(B);
    for (size_t i=0;i<A.used;i+=sizeof(nodeRef)) {
        printf("A: %s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, *(nodeRef *)(A.content+i)));
    }
    for (size_t i=0;i<B.used;i+=sizeof(nodeRef)) {
        printf("B: %s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, *(nodeRef *)(B.content+i)));
    }
    while (!match) {
       printf("A:%zu B:%zu\n", A.used/sizeof(nodeRef), B.used/sizeof(nodeRef));
        if (shouldChooseSideA(distA, distB, A, B)) {
            nodeRef * content = (nodeRef *)A.content;
            struct buffer tmp;
            if (!A.used) break;
            for (size_t i=0;i<A.used/sizeof(nodeRef);i++) {
                struct wikiNode * node = getNode(nodeData, content[i]);
                if (node->dist_a) {
                    continue;
                }
                if (node->dist_b) {
                    freePrint("Match @ %s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, content[i]));
                    match = true;
                    *(nodeRef *)bufferAdd(&matches, sizeof(nodeRef)) = content[i];
                }
                node->dist_a = distA;
                if (match) continue;
                for (size_t j=0;j<node->forward_length;j++) {
                    *(nodeRef *)bufferAdd(&New, sizeof(nodeRef)) = node->references[j];
                }
            }
            distA++;
            tmp = A;
            A = New;
            New = tmp;
            New.used=0;
        } else {
            nodeRef * content = (nodeRef *)B.content;
            struct buffer tmp;
            if (!B.used) break;
            for (size_t i=0;i<B.used/sizeof(nodeRef);i++) {
                struct wikiNode * node = getNode(nodeData, content[i]);
                if (node->dist_b) {
                    continue;
                }
                if (node->dist_a) {
                    freePrint("Match @ %s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, content[i]));
                    match = true;
                    *(nodeRef *)bufferAdd(&matches, sizeof(nodeRef)) = content[i];
                }
                node->dist_b = distB;
                if (match) continue;
                for (size_t j=0;j<node->backward_length;j++) {
                    *(nodeRef *)bufferAdd(&New, sizeof(nodeRef)) = node->references[j+node->forward_length];
                }
            }
            distB++;
            tmp = B;
            B = New;
            New = tmp;
            New.used=0;
        }
    }
    if (!match) {
        printf("No route found\n");
    } else {
        // distA and distB are for the next layer, but we want the one for the matches layer
        distA--;
        distB--;
        printf("%zu matches found\ndistA: %zu, distB: %zu\n", matches.used/4, distA, distB);
        assert(!New.used);
        while (distA>2) {
            struct buffer tmp;
            for (size_t i=0;i<matches.used/sizeof(nodeRef);i++) {
                struct wikiNode * node = getNode(nodeData, matches.u32content[i]);
                assert(node->dist_a==distA);
                for (size_t j=0;j<node->backward_length;j++) {
                    struct wikiNode * target = getNode(nodeData, node->references[node->forward_length+j]);
                    if (target->dist_a==distA-1) {
                        target->dist_b = distB+1;
                        *(nodeRef *)bufferAdd(&New, sizeof(nodeRef)) = (char *)target-nodeData;
                    }
                }
            }
            distA--;
            distB++;
            tmp = matches;
            matches = New;
            New = tmp;
            New.used = 0;
        }
        free(matches.content);
        matches = originalA;
        for (size_t i=0;i<matches.used/sizeof(nodeRef);i++) {
            struct wikiNode * node = getNode(nodeData, matches.u32content[i]);
            node->dist_b = distB+1;
        }
        distA--;
        distB++;
        while (distB>1) {
            struct buffer tmp;
            for (size_t i=0;i<matches.used/sizeof(nodeRef);i++) {
                struct wikiNode * node = getNode(nodeData, matches.u32content[i]);
                if (!node->dist_b) continue;
                node->dist_b = 0;
                freePrint("%s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, matches.u32content[i]));
                for (size_t j=0;j<node->forward_length;j++) {
                    struct wikiNode * target = getNode(nodeData, node->references[j]);
                    if (target->dist_b==distB-1) {
                        freePrint("\t%s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, (char *)target-nodeData));
                        *(nodeRef *)bufferAdd(&New, sizeof(nodeRef)) = (char *)target-nodeData;
                    }
                }
            }
            distA++;
            distB--;
            assert(New.used);
            tmp = matches;
            matches = New;
            New = tmp;
            New.used = 0;
            putchar('\n');
        }
        putchar('\n');
    }


    free(A.content);
    free(B.content);
    free(New.content);
    free(matches.content);
    cleanNodes(nodeData, nodeOffsets, nodeCount);
}

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

    nodeData = mapFile(argv[1], O_RDONLY, PROT_READ | PROT_WRITE, MAP_PRIVATE, &nodeDataLength);
    if (!nodeData) {
        fprintf(stderr, "Failed to mmap node file\n");
        return -1;
    }
    fprintf(stderr, "Mmapped %zu bytes\n", nodeDataLength);

    titleFile = fopen(argv[2], "r");
    if (!titleFile) {
        perror("Failed to open title file");
        return -1;
    }

    nodeOffsets = getNodeOffsets(nodeData, nodeDataLength, &nodeCount);
    fprintf(stderr, "Calculated offsets for %zu nodes\n", nodeCount);
    titleOffsets = getTitleOffsets(titleFile, &titleCount);
    fprintf(stderr, "Calculated offsets for %zu titles\n", titleCount);
    assert(titleCount == nodeCount);


    struct buffer A = bufferCreate();
    struct buffer B = bufferCreate();


    while (true) {
        char * str = NULL;
        size_t len = 0;
        uint32_t res;
        int c;


        if (getline(&str, &len, stdin)==-1)
            break;
        len = strlen(str);
        if (len>0&&str[len-1]=='\n')
            str[--len] = '\0';
        if (len==0) continue;
        switch (c=str[0]) {
        case 'A':
        case 'B':
            if (len<2||str[1]!=' ') {
                fprintf(stderr, "Invalid input\n");
                break;
            }
            res = titleToNodeOffset(titleFile, nodeOffsets, titleOffsets, nodeCount, str+2);
            if (res==-1) {
                fprintf(stderr, "Could not find %s\n", str+2);
                break;
            }
            if (c=='A') {
                *(nodeRef *)bufferAdd(&A, sizeof(nodeRef)) = res;
            } else {
                *(nodeRef *)bufferAdd(&B, sizeof(nodeRef)) = res;
            }
            break;
        case 'R':
            nodeRoute(A, B, titleFile, nodeData, nodeOffsets, titleOffsets, nodeCount);
            A.used = 0;
            B.used = 0;
            break;
        default:
            fprintf(stderr, "Invalid input\n");
            break;
        }
        free(str);
        str = NULL;
        len = 0;
    }

    return 0;
}
