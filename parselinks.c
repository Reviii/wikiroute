#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "nodetypes.h"

static char uppercaseChar(char c) {
    if (c>='a'&&c<='z') return c-32;
    return c;
}

static char ** getTitleListFromFile(FILE * f, size_t * titleCount) {
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
            *bufferAdd(&stringBuf, sizeof(char)) = uppercaseChar(c);
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

static size_t title2id(char ** id2title, size_t titleCount, char * title) {
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

static struct wikiNode * addReference(struct wikiNode * node, size_t ref, bool backward) {
    struct wikiNode * res;
    if (!backward) {
        for (int i=0;i<node->forward_length;i++)
            if (node->references[i]==ref) return node;
    }
    res = realloc(node, sizeof(struct wikiNode)+(node->forward_length+node->backward_length)*4+4);
    if (!res) {
        fprintf(stderr, "addReference: realloc(%p, %zu) failed\n", node, sizeof(struct wikiNode)+(node->forward_length+node->backward_length)*4+4);
        assert(res);
    }
    res->references[res->forward_length+res->backward_length] = ref;
    if (backward) {
        res->backward_length++;
    } else {
        res->forward_length++;
    }
    return res;
}

static struct wikiNode ** getNodes(FILE * f, char ** id2title, size_t titleCount) {
    int c;
    int id = 0;
    bool inLink = false;
    struct buffer titleBuf = bufferCreate();
    struct wikiNode ** nodes = malloc(titleCount*sizeof(struct wikiNode *));
    rewind(f);
    nodes[0] = calloc(sizeof(struct wikiNode), 1);
    while ((c=fgetc(f))!=EOF) {
        if (inLink) {
            *(bufferAdd(&titleBuf, 1)) = uppercaseChar(c);
            if (c=='\0') {
                size_t ref = title2id(id2title, titleCount, titleBuf.content+1); // titleBuf.content+1, becauce the first char needs to be ignored
                titleBuf.used = 0;
                if (ref==-1) continue;
                if (titleBuf.content[0]==uppercaseChar('r')) nodes[id]->isRedirect = true;
                nodes[id] = addReference(nodes[id], ref, false);
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

static struct wikiNode ** applyRedirects(struct wikiNode ** nodes, size_t titleCount) {
    size_t * redirects = malloc(titleCount*sizeof(size_t));
    for (size_t i=0;i<titleCount;i++) {
        if (nodes[i]->isRedirect) {
            if (nodes[i]->forward_length!=1) {
                fprintf(stderr, "Redirecting page with id %zu links to %u other pages, assuming redirect is the first one\n", i, nodes[i]->forward_length);
            }
            redirects[i] = nodes[i]->references[0];
        } else {
            redirects[i] = i;
        }
    }
    for (size_t i=0;i<titleCount;i++) {
        struct wikiNode * node = nodes[i];
        for (size_t j=0;j<node->forward_length;j++) {
            node->references[j] = redirects[node->references[j]];
        }
    }
    free(redirects);
    return nodes;
}

static struct wikiNode ** addBackwardRefs(struct wikiNode ** nodes, size_t titleCount) {
    for (size_t i=0;i<titleCount;i++) {
        struct wikiNode * const from = nodes[i];
        for (int j=0;j<(from->forward_length);j++) {
            if (from==nodes[from->references[j]]) continue;
            nodes[from->references[j]] = addReference(nodes[from->references[j]], i, true);
        }
    }
    return nodes;
}

static size_t * getNodeOffsets(struct wikiNode ** nodes, size_t titleCount) {
    size_t offset = 0;
    size_t * offsets = malloc(sizeof(size_t)*titleCount);
    for (size_t i=0;i<titleCount;i++) {
        offsets[i] = offset;
        offset += sizeof(struct wikiNode)+(nodes[i]->forward_length+nodes[i]->backward_length)*sizeof(nodes[i]->references[0]);
    }
    fprintf(stderr, "Total node size: %zu\n", offset);
    return offsets;
}

static struct wikiNode ** replaceIdsWithOffsets(struct wikiNode ** nodes, size_t titleCount, size_t * offsets) {
    for (size_t i=0;i<titleCount;i++) {
        struct wikiNode * node = nodes[i];
        for (size_t j=0;j<node->forward_length+node->backward_length;j++) {
            node->references[j] = offsets[node->references[j]];
        }
    }
    return nodes;
}

static void writeNodesToFile(struct wikiNode ** nodes, size_t titleCount, FILE * f) {
    for (size_t i=0;i<titleCount;i++) {
        struct wikiNode * node = nodes[i];
        assert(node->dist_b==0);
        node->dist_a = 0;
        fwrite(node, 1, sizeof(*node)+(node->forward_length+node->backward_length)*sizeof(node->references[0]), f);
    }
}

int main(int argc, char **argv) {
    FILE * in = NULL;
    char ** id2title;
    size_t titleCount;
    struct wikiNode ** id2node;
    size_t * id2nodeOffset;
    //char search[256];
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
    fprintf(stderr, "Created nodes\n");
    free(id2title[0]);
    free(id2title);

    id2node = applyRedirects(id2node, titleCount);
    fprintf(stderr, "Applied redirects\n");

    id2node = addBackwardRefs(id2node, titleCount);
    fprintf(stderr, "Added backward references\n");

    id2nodeOffset = getNodeOffsets(id2node, titleCount);
    id2node = replaceIdsWithOffsets(id2node, titleCount, id2nodeOffset);
    fprintf(stderr, "Replaced ids with offsets\n");
    free(id2nodeOffset);

    writeNodesToFile(id2node, titleCount, stdout);
    /*while (fgets(search, sizeof(search), stdin)) {
        size_t id;
        search[strlen(search)-1] = '\0'; // remove last character
        for (int i=0;search[i];i++) {
            search[i] = uppercaseChar(search[i]);
        }
        id = title2id(id2title, titleCount, search);
        if (id==-1) {
            printf("Could not find '%s'\n", search);
            continue;
        }
        printf("Id: %zu\nOffset:%zu\nNearby pages:\n", id, id2nodeOffset[id]);
        for (int i=-2;i<3;i++) {
            if (id+i<titleCount && id+i>=0) printf("%zu: %s\n", id+i, id2title[id+i]);
        }
        printf("isRedirect: %s\n", id2node[id]->isRedirect ? "true" : "false");
        printf("Links to:\n");
        for (int i=0;i < id2node[id]->forward_length;i++) {
            size_t linkedId = id2node[id]->references[i];
            printf("%zu: %s\n", linkedId, id2title[linkedId]);
        }
        printf("Linked by:\n");
        for (int i=0;i < id2node[id]->backward_length;i++) {
            size_t linkedId = id2node[id]->references[id2node[id]->forward_length+i];
            printf("%zu: %s\n", linkedId, id2title[linkedId]);
        }
    }*/
    return 0;
}
