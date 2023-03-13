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
#define STATS


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
//    return true;
    return A.used<=B.used;
}

static void nodeRoute(struct buffer oA, struct buffer oB, unsigned char * distAs, unsigned char * distBs, FILE * titles, struct wikiNode ** nodes, size_t * titleOffsets, size_t nodeCount) {
    size_t distA =1;
    size_t distB =1;
    bool match = false;
    struct buffer matches = bufferCreate();
    struct buffer New = bufferCreate();
    struct buffer A = bufferDup(oA);
    struct buffer B = bufferDup(oB);
    struct buffer originalA = bufferDup(oA);
    struct buffer toCleanA = bufferCreate();
    size_t aIndex[256];
    aIndex[0] = 0;
    #ifdef JSON
    printf("{\"sources\":[");
    if (A.used)
        freePrinto("%s", getJSONTitle(titles, titleOffsets, *(nodeRef *)A.content));
    for (size_t i=sizeof(nodeRef);i<A.used;i+=sizeof(nodeRef)) {
        freePrint("%s", getJSONTitle(titles, titleOffsets, *(nodeRef *)(A.content+i)));
    }
    printf("],\"destinations\":[");
    if (B.used)
        freePrinto("%s", getJSONTitle(titles, titleOffsets, *(nodeRef *)B.content));
    for (size_t i=sizeof(nodeRef);i<B.used;i+=sizeof(nodeRef)) {
        freePrint("%s", getJSONTitle(titles, titleOffsets, *(nodeRef *)(B.content+i)));
    }
    #else
    for (size_t i=0;i<A.used;i+=sizeof(nodeRef)) {
        printf("A: %s\n", getTitle(titles, titleOffsets, *(nodeRef *)(A.content+i)));
    }
    for (size_t i=0;i<B.used;i+=sizeof(nodeRef)) {
        printf("B: %s\n", getTitle(titles, titleOffsets, *(nodeRef *)(B.content+i)));
    }
    #endif
    while (!match&&distA+distB<=500) {
#ifdef STATS
        fprintf(stderr, "A:%zu B:%zu\n", A.used/sizeof(nodeRef), B.used/sizeof(nodeRef));
#endif
        size_t newcount = 0;
        if (shouldChooseSideA(distA, distB, A, B)) {
            nodeRef * content = (nodeRef *)A.content;
            if (!A.used) break;
            for (size_t i=0;i<A.used/sizeof(nodeRef);i++) {
                nodeRef ref = content[i];
                if (distBs[ref]) {
                    match = true;
                    break;
                }
            }
            for (size_t i=0;i<A.used/sizeof(nodeRef);i++) {
                nodeRef ref = content[i];
                if (distAs[ref]) {
                    continue;
                }
                if (distBs[ref]) {
                    *(nodeRef *)bufferAdd(&matches, sizeof(nodeRef)) = ref;
                }
                newcount++;
                distAs[ref] = distA;
                if (match) continue;
                struct wikiNode * node = nodes[ref];
                memcpy(
                    __builtin_assume_aligned(bufferAdd(&New, node->forward_length*sizeof(nodeRef)), sizeof(nodeRef)),
                    __builtin_assume_aligned(node->references, sizeof(nodeRef)),
                    node->forward_length*sizeof(nodeRef)
                );
            }
            aIndex[distA] = toCleanA.used/sizeof(nodeRef);
            distA++;
            memcpy(
                __builtin_assume_aligned(bufferAdd(&toCleanA, A.used), sizeof(nodeRef)),
                __builtin_assume_aligned(A.content, sizeof(nodeRef)),
                A.used
            );
            struct buffer tmp = A;
            A = New;
            New = tmp;
            New.used=0;
        } else {
            nodeRef * content = (nodeRef *)B.content;
            if (!B.used) break;
            for (size_t i=0;i<B.used/sizeof(nodeRef);i++) {
                nodeRef ref = content[i];
                if (distAs[ref]) {
                    match = true;
                    break;
                }
            }
            for (size_t i=0;i<B.used/sizeof(nodeRef);i++) {
                nodeRef ref = content[i];
                if (distBs[ref]) {
                    continue;
                }
                if (distAs[ref]) {
                    *(nodeRef *)bufferAdd(&matches, sizeof(nodeRef)) = content[i];
                }
                newcount++;
                distBs[ref] = distB;
                if (match) continue;
                struct wikiNode * node = nodes[ref];
                memcpy(
                    __builtin_assume_aligned(bufferAdd(&New, node->backward_length*sizeof(nodeRef)), sizeof(nodeRef)),
                    __builtin_assume_aligned(node->references+node->forward_length, sizeof(nodeRef)),
                    node->backward_length*sizeof(nodeRef)
                );
            }
            distB++;
            struct buffer tmp = B;
            B = New;
            New = tmp;
            New.used=0;
        }
#ifdef STATS
        fprintf(stderr, "Checked %zu new articles\n", newcount);
#endif
    }
    if (!match) {
        #ifdef JSON
        printf("],\"route\":null}\n");
        #else
        printf("No route found\n");
        #endif
        free(originalA.content);
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
        while (distA>1) {
            for (size_t i=aIndex[distA-1];i<aIndex[distA];i++) {
                nodeRef ref = toCleanA.u32content[i];
                struct wikiNode * node = nodes[ref];
                if (!distAs[ref]) continue;
                nodeRef * refs = node->references;
                nodeRef * refMax = refs+node->forward_length;
                for (;refs<refMax;refs++) { // this loop was hot
                    if (distBs[*refs]==distB) {
                        distAs[ref] = 0;
                        distBs[ref] = distB+1;
                        break;
                    }
                }
            }
            distA--;
            distB++;
        }

        free(matches.content);
        matches = originalA;

        #ifdef JSON
        bool firstIteration = true;
        #endif
        while (distB>1) {
            #ifdef JSON
            if (!firstIteration) printf("], ");
            firstIteration = false;
            bool firstNestedIteration = true;
            #endif
            for (size_t i=0;i<matches.used/sizeof(nodeRef);i++) {
                nodeRef ref = matches.u32content[i];
                struct wikiNode * node = nodes[ref];
                if (!distBs[ref]) continue;
                distBs[ref] = 0;
                #ifdef JSON
                if (!firstNestedIteration) printf("],");
                firstNestedIteration = false;
                freePrinto("%s:[", getJSONTitle(titles, titleOffsets, ref));
                bool first = true;
                #else
                freePrint("%s\n", getTitle(titles, titleOffsets, ref));
                #endif
                for (size_t j=0;j<node->forward_length;j++) {
                    nodeRef target = node->references[j];
                    if (distBs[target]==distB-1) {
                        #ifdef JSON
                        char * title = getJSONTitle(titles, titleOffsets, target);
                        printf("%s", title+first);
                        first = false;
                        free(title);
                        #else
                        freePrint("\t%s\n", getTitle(titles, titleOffsets, target));
                        #endif
                        *(nodeRef *)bufferAdd(&New, sizeof(nodeRef)) = target;
                    }
                }
            }
            distA++;
            distB--;
            assert(New.used);
            struct buffer tmp = matches;
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
    memset(distAs, 0, nodeCount);
    free(toCleanA.content);
    memset(distBs, 0, nodeCount);
}

int main(int argc, char ** argv) {
    char * nodeData = NULL;
    size_t nodeDataLength = 0;
    size_t nodeCount = 0;
    FILE * titleFile = NULL;
    size_t titleCount = 0;
    struct wikiNode ** nodes = NULL;
    size_t * titleOffsets = NULL;

    setvbuf(stdin, NULL, _IOLBF, BUFSIZ);
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

    if (argc<3) {
        fprintf(stderr, "Usage: %s <node file> <title file>\n", argv[0]);
        return 1;
    }

    nodeData = mapFile(argv[1], O_RDONLY, PROT_READ, MAP_PRIVATE, &nodeDataLength);
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

    nodes = getNodes(nodeData, nodeDataLength, &nodeCount);
    fprintf(stderr, "Calculated offsets for %zu nodes\n", nodeCount);
    titleOffsets = getTitleOffsets(titleFile, &titleCount);
    fprintf(stderr, "Calculated offsets for %zu titles\n", titleCount);
    assert(titleCount == nodeCount);


    struct buffer A = bufferCreate();
    struct buffer B = bufferCreate();
    unsigned char * distAs = calloc(1,nodeCount);
    unsigned char * distBs = calloc(1,nodeCount);


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
            res = titleToNodeId(titleFile, titleOffsets, nodeCount, str+2);
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
            nodeRoute(A, B, distAs, distBs, titleFile, nodes, titleOffsets, nodeCount);
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
