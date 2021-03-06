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

#define JSON

static void cleanNodes(char * nodeData, nodeRef * nodeOffsets, size_t nodeCount) {
    for (size_t i=0;i<nodeCount;i++) {
        struct wikiNode * node = getNode(nodeData, nodeOffsets[i]);
        if (!node->redirect) continue;
        node->dist_a = 0;
        node->dist_b = 0;
    }
}

static void cleanNodesA(char * nodeData, size_t distAMax, struct wikiNode * node) {
    uint8_t distA = node->dist_a;
    if (distA<distAMax) {
        for (size_t i=0;i<node->forward_length;i++) {
            struct wikiNode * target = getNode(nodeData, node->references[i]);
            if (target->dist_a==distA+1) cleanNodesA(nodeData, distAMax, target);
        }
    }
    node->dist_a = 0;
    node->dist_b = 0;
}

static void cleanNodesB(char * nodeData, size_t distBMax, struct wikiNode * node) {
    uint8_t distB = node->dist_b;
    if (distB<distBMax) {
        size_t len = node->forward_length+node->backward_length;
        for (size_t i=node->forward_length;i<len;i++) {
            struct wikiNode * target = getNode(nodeData, node->references[i]);
            if (!target->dist_b) target->dist_b = distB+1;
            if (target->dist_b==distB+1) cleanNodesB(nodeData, distBMax, target);
        }
    }
    node->dist_a = 0;
    node->dist_b = 0;
}

static void freePrint(char * format, char * str) {
    printf(format, str);
    free(str);
}
static void freePrinto(char * format, char * str) {
    printf(format, str+1);
    free(str);
}

static bool shouldChooseSideA(int distA, int distB, struct buffer A, struct buffer B) {
    if (distA==1) return true;
    if (distB==1) return false;
    if (distA==2) return true;
    return A.used<=B.used;
}

static void nodeRoute(struct buffer oA, struct buffer oB, FILE * titles, char * nodeData, nodeRef * nodeOffsets, size_t * titleOffsets, size_t nodeCount) {
    size_t distA =1;
    size_t distB =1;
    bool match = false;
    struct buffer matches = bufferCreate();
    struct buffer New = bufferCreate();
    struct buffer originalA = bufferDup(oA);
    struct buffer A = bufferDup(oA);
    struct buffer B = bufferDup(oB);
    #ifdef JSON
    printf("{\"sources\":[");
    if (A.used)
        freePrinto("%s", nodeOffsetToJSONTitle(titles, nodeOffsets, titleOffsets, nodeCount, *(nodeRef *)A.content));
    for (size_t i=sizeof(nodeRef);i<A.used;i+=sizeof(nodeRef)) {
        freePrint("%s", nodeOffsetToJSONTitle(titles, nodeOffsets, titleOffsets, nodeCount, *(nodeRef *)(A.content+i)));
    }
    printf("],\"destinations\":[");
    if (B.used)
        freePrinto("%s", nodeOffsetToJSONTitle(titles, nodeOffsets, titleOffsets, nodeCount, *(nodeRef *)B.content));
    for (size_t i=sizeof(nodeRef);i<B.used;i+=sizeof(nodeRef)) {
        freePrint("%s", nodeOffsetToJSONTitle(titles, nodeOffsets, titleOffsets, nodeCount, *(nodeRef *)(B.content+i)));
    }
    #else
    for (size_t i=0;i<A.used;i+=sizeof(nodeRef)) {
        printf("A: %s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, *(nodeRef *)(A.content+i)));
    }
    for (size_t i=0;i<B.used;i+=sizeof(nodeRef)) {
        printf("B: %s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, *(nodeRef *)(B.content+i)));
    }
    #endif
    while (!match) {
#ifdef STATS
        printf("A:%zu B:%zu\n", A.used/sizeof(nodeRef), B.used/sizeof(nodeRef));
#endif
        size_t newcount = 0;
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
                    match = true;
                    *(nodeRef *)bufferAdd(&matches, sizeof(nodeRef)) = content[i];
                }
                newcount++;
                node->dist_a = distA;
                if (match) continue;
                memcpy(
                    __builtin_assume_aligned(bufferAdd(&New, node->forward_length*sizeof(nodeRef)), sizeof(nodeRef)),
                    __builtin_assume_aligned(node->references, sizeof(nodeRef)),
                    node->forward_length*sizeof(nodeRef)
                );
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
                    match = true;
                    *(nodeRef *)bufferAdd(&matches, sizeof(nodeRef)) = content[i];
                }
                newcount++;
                node->dist_b = distB;
                if (match) continue;
                memcpy(
                    __builtin_assume_aligned(bufferAdd(&New, node->backward_length*sizeof(nodeRef)), sizeof(nodeRef)),
                    __builtin_assume_aligned(node->references+node->forward_length, sizeof(nodeRef)),
                    node->backward_length*sizeof(nodeRef)
                );
            }
            distB++;
            tmp = B;
            B = New;
            New = tmp;
            New.used=0;
        }
#ifdef STATS
        printf("Checked %zu new articles\n", newcount);
#endif
    }
    size_t distBMax = distB-1;
    size_t distAMax = distA-1;
    if (!match) {
        #ifdef JSON
        printf("],\"route\":null}\n");
        #else
        printf("No route found\n");
        #endif
    } else {
        // distA and distB are for the next layer, but we want the one for the matches layer
        distA--;
        distB--;
        #ifdef JSON
        printf("],\"dist\":{\"a\":%zu,\"b\":%zu},\"route\":{", distA-1, distB-1);
        #else
        printf("distA: %zu, distB: %zu\n", distA, distB);
        #endif
        assert(!New.used);
        while (distA>2) {
            struct buffer tmp;
            for (size_t i=0;i<matches.used/sizeof(nodeRef);i++) {
                struct wikiNode * node = getNode(nodeData, matches.u32content[i]);
                assert(node->dist_a==distA);
                size_t len = node->backward_length+node->forward_length;
                for (size_t j=node->forward_length;j<len;j++) {
                    struct wikiNode * target = getNode(nodeData, node->references[j]);
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
        if (distA==2) {
            for (size_t i=0;i<matches.used/sizeof(nodeRef);i++) {
                struct wikiNode * node = getNode(nodeData, matches.u32content[i]);
                node->dist_b = distB+1;
            }
            distA--;
            distB++;
        }
        #ifdef JSON
        bool firstIteration = true;
        #endif
        while (distB>1) {
            struct buffer tmp;
            #ifdef JSON
            if (!firstIteration) printf("], ");
            firstIteration = false;
            #endif
            for (size_t i=0;i<matches.used/sizeof(nodeRef);i++) {
                struct wikiNode * node = getNode(nodeData, matches.u32content[i]);
                if (!node->dist_b) continue;
                node->dist_b = 0;
                #ifdef JSON
                if (i>0) printf("],");
                freePrinto("%s:[", nodeOffsetToJSONTitle(titles, nodeOffsets, titleOffsets, nodeCount, matches.u32content[i]));
                bool first = true;
                #else
                freePrint("%s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, matches.u32content[i]));
                #endif
                for (size_t j=0;j<node->forward_length;j++) {
                    struct wikiNode * target = getNode(nodeData, node->references[j]);
                    if (target->dist_b==distB-1) {
                        #ifdef JSON
                        char * title = nodeOffsetToJSONTitle(titles, nodeOffsets, titleOffsets, nodeCount, (char *)target-nodeData);
                        printf("%s", title+first);
                        first = false;
                        free(title);
                        #else
                        freePrint("\t%s\n", nodeOffsetToTitle(titles, nodeOffsets, titleOffsets, nodeCount, (char *)target-nodeData));
                        #endif
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
            #ifdef JSON
            #else
            putchar('\n');
            #endif
        }
        #ifdef JSON
        printf("%s}}\n", distA>1 ? "]":"");
        #else
        putchar('\n');
        #endif
    }


    free(A.content);
    free(B.content);
    free(New.content);
    free(matches.content);
    for (size_t i=0;i<oA.used/sizeof(nodeRef);i++) {
        cleanNodesA(nodeData, distAMax, getNode(nodeData, oA.u32content[i]));
    }
    for (size_t i=0;i<oB.used/sizeof(nodeRef);i++) {
        cleanNodesB(nodeData, distBMax, getNode(nodeData, oB.u32content[i]));
    }
    // in some very complex routes then old cleanNodes function might be faster than
    // the cleanNodesA and cleanNodesB functions
    // TODO: use cleanNodes function when that is likely to be the case
    if (0) cleanNodes(nodeData, nodeOffsets, nodeCount);
}

int main(int argc, char ** argv) {
    char * nodeData = NULL;
    size_t nodeDataLength = 0;
    size_t nodeCount = 0;
    FILE * titleFile = NULL;
    size_t titleCount = 0;
    nodeRef * nodeOffsets = NULL;
    size_t * titleOffsets = NULL;

    setvbuf(stdin, NULL, _IOLBF, BUFSIZ);
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

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
            if (res==(nodeRef)-1) {
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
