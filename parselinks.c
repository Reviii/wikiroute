#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "nodetypes.h"

char normalizeTitleChar(char c) {
    if (c>='A'&&c<='Z') return c+'a'-'A';
    if (c=='_') return ' ';
    return c;
}

char ** getTitleListFromFile(FILE * f, size_t * titleCount) {
    // everything between newline and \0 is a title
    struct buffer stringBuf = bufferCreate();
    struct buffer offsetBuf = bufferCreate();
    bool inTitle = true;
    int c;
    size_t* offsets = NULL;
    char ** res = NULL;

    *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = stringBuf.used;
    while ((c=fgetc(f))!=EOF) {
        if (inTitle) {
            *bufferAdd(&stringBuf, sizeof(char)) = normalizeTitleChar(c);
            if (!c) {
                inTitle = false;
                *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = stringBuf.used;
            }
        } else {
            if (c=='\n') {
                inTitle = true;
            }
        }
    }
    // uncomment the following line to reduce virtual memory usage
    //bufferCompact(&stringBuf);

    offsets = (size_t *)offsetBuf.content;
    res = malloc(offsetBuf.used/sizeof(offsets[0])*sizeof(char *));
    for (int i=0;i<offsetBuf.used/sizeof(offsets[0])-1;i++) {
        res[i] = stringBuf.content + offsets[i];
    }
    res[offsetBuf.used/sizeof(offsets[0])-1] = NULL;
    *titleCount = offsetBuf.used/sizeof(offsets[0])-1;
    free(offsets);
    // not freeing stringBuf content, because it is referenced by res
    return res;
}

size_t title2id(char ** id2title, size_t titleCount, char * title) {
    ssize_t first, last, middle;
    first = 0;
    last = titleCount - 1;
    middle = (first+last)/2;
    while (first <= last) {
//        fprintf(stderr, "Comparing to id %d/%d\n", middle, titleCount);
        int cmp = strcmp(id2title[middle], title);
        if (cmp<0) {
            first = middle+1; // overflow?
        } else if (cmp==0) {
            return middle;
        } else {
            last = middle-1;
        }
        middle = (first+last)/2;
    }
    return -1;
}
struct wikiNode * increaseNodeAllocSize(struct wikiNode * node) {
    return realloc(node, sizeof(struct wikiNode)+(node->forward_length+node->backward_length)*4+4);
}
struct wikiNode ** getNodes(FILE * f, char ** id2title, size_t titleCount) {
    int c;
    int id = 0;
    bool inLink = false;
    struct buffer titleBuf = bufferCreate();
    struct wikiNode ** nodes = malloc(titleCount*sizeof(struct wikiNode *));
    rewind(f);
    nodes[0] = calloc(sizeof(struct wikiNode), 1);
    while ((c=fgetc(f))!=EOF) {
        if (inLink) {
            *(bufferAdd(&titleBuf, 1)) = normalizeTitleChar(c);
            if (c=='\0') {
                size_t ref = title2id(id2title, titleCount, titleBuf.content+1); // titleBuf.content+1, becauce the first char needs to be ignored
                titleBuf.used = 0;
                if (ref==-1) continue;
                nodes[id] = increaseNodeAllocSize(nodes[id]);
                nodes[id]->references[nodes[id]->forward_length] = ref;
                nodes[id]->forward_length++;
            }
            if (c=='\n') {
                inLink = false;
                titleBuf.used = 0;
                id++;
                nodes[id] = calloc(sizeof(struct wikiNode), 1);
            }
        } else {
            if (c=='\0') {
                inLink = true;
            }
        }
    }
    return nodes;
}
struct wikiNode ** addBackwardRefs(struct wikiNode ** nodes, size_t titleCount) {
    for (int i=0;i<titleCount;i++) {
        struct wikiNode from = *nodes[i];
        for (int j=0;j<from.forward_length;j++) {
            nodes[from.references[i]] = increaseNodeAllocSize(nodes[from.references[i]]);
            struct wikiNode * to = nodes[from.references[i]];
            to->references[to->forward_length+to->backward_length] = i;
            to->backward_length++;
        }
    }
    return nodes;
}

int main(int argc, char **argv) {
    FILE * in = NULL;
    char ** id2title;
    size_t titleCount;
    struct wikiNode ** id2node;
    char search[256];
    if (argc<2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "-")==0) {
        in = stdin;
    } else {
        in = fopen(argv[1], "r");
    }
    if (!in) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
    }
    id2title = getTitleListFromFile(in, &titleCount);
    fprintf(stderr, "%zu pages have been given an unique id\n", titleCount);

    id2node = getNodes(in, id2title, titleCount);
    while (fgets(search, sizeof(search), stdin)) {
        search[strlen(search)-1] = '\0'; // remove last character
        size_t id = title2id(id2title, titleCount, search);
        if (id==-1) {
            printf("Could not find '%s'\n", search);
            continue;
        }
        printf("Id: %zu\nNearby pages:\n", id);
        for (int i=-2;i<3;i++) {
            if (id+i<titleCount && id+i>=0) printf("%zu: %s\n", id+i, id2title[id+i]);
        }
        printf("Links to:\n");
        for (int i=0;i < id2node[id]->forward_length;i++) {
            size_t linkedId = id2node[id]->references[i];
            printf("%zu: %s\n", linkedId, id2title[linkedId]);
        }
    }
    return 0;
}
