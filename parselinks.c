#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "iteratefile.h"
#include "nodetypes.h"

static FILE * openFileAndCheck(char * path, char * options, char * name, FILE * std) {
    FILE * f = NULL;
    if (strcmp(path, "-")==0) {
        f = std;
    } else {
        f = fopen(path, options);
    }
    if (!f) {
        fprintf(stderr, "Failed to open %s file: %s\n", name, strerror(errno));
        errno = 0;
        exit(1);
    }
    return f;
}

static char uppercaseChar(char c) {
    if (c>='a'&&c<='z') return c-32;
    return c;
}

static char ** getTitleListFromFile(FILE * f, size_t * titleCount) {
    // everything between newline and \0 is a title
    struct buffer stringBuf = bufferCreate();
    struct buffer offsetBuf = bufferCreate();
    bool inTitle = true;
    bool firstChar = true;
    size_t* offsets = NULL;
    char ** res = NULL;

    *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = stringBuf.used;
    iterateFile(f, c,
        if (inTitle) {
            *bufferAdd(&stringBuf, sizeof(char)) = firstChar ? uppercaseChar(c) : c;
            firstChar = false;
            if (!c) {
                inTitle = false;
                *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = stringBuf.used;
            }
        } else {
            if (c=='\n') {
                inTitle = true;
            }
        }
    )
    // uncomment the following line to reduce virtual memory usage
    //bufferCompact(&stringBuf);

    offsets = (size_t *)offsetBuf.content;
    res = malloc(offsetBuf.used/sizeof(offsets[0])*sizeof(char *));
    for (size_t i=0;i<offsetBuf.used/sizeof(offsets[0])-1;i++) {
        res[i] = stringBuf.content + offsets[i];
        if (i>0&&strcmp(res[i-1], res[i])==0) fprintf(stderr, "Warning: duplicate title at index %zu\n", i);
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

static struct wikiNode * addReference(struct wikiNode * node, nodeRef ref, bool backward) {
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
    int id = 0;
    bool inLink = false;
    struct buffer titleBuf = bufferCreate();
    struct wikiNode ** nodes = malloc(titleCount*sizeof(struct wikiNode *));
    rewind(f);
    nodes[0] = calloc(sizeof(struct wikiNode), 1);
    iterateFile(f, c,
        if (inLink) {
            *(bufferAdd(&titleBuf, 1)) = c;
            if (c=='\0') {
                titleBuf.content[1] = uppercaseChar(titleBuf.content[1]); // make first char case insensitive
                nodeRef ref = title2id(id2title, titleCount, titleBuf.content+1); // titleBuf.content+1, becauce the first char needs to be ignored
                titleBuf.used = 0;
                if (ref==-1) continue;
                if (titleBuf.content[0]=='r'&&!nodes[id]->redirect) nodes[id]->redirect = nodes[id]->forward_length+1;
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
    )
    return nodes;
}

static struct wikiNode ** applyRedirects(struct wikiNode ** nodes, size_t titleCount) {
    nodeRef * redirects = malloc(titleCount*sizeof(nodeRef));
    for (size_t i=0;i<titleCount;i++) {
        if (nodes[i]->redirect) {
            if (nodes[i]->forward_length!=1) {
                fprintf(stderr, "Redirecting page with id %zu links to %u other pages, redirect has index %u\n", i, nodes[i]->forward_length, nodes[i]->redirect-1);
            }
            redirects[i] = nodes[i]->references[nodes[i]->redirect-1];
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

static size_t removeUselessRedirectionPages(struct wikiNode ** nodes, size_t titleCount) {
    size_t removed = 0;
    for (size_t i=0;i<titleCount;i++) {
        if (nodes[i]->redirect&&nodes[i]->backward_length==0&&nodes[i]->forward_length<2) {
            free(nodes[i]);
            nodes[i] = NULL;
            removed++;
        }
    }
    return removed;
}

static size_t removeSomeBackwardRefs(struct wikiNode ** nodes, size_t titleCount) {
    // remove backward refs to unreachable nodes
    const nodeRef unreachable = -1;
    size_t removed = 0;
    for (size_t i=0;i<titleCount;i++) {
        struct wikiNode * node = nodes[i];
        if (!node) continue;
        for (size_t j=0;j<node->backward_length;j++) {
            struct wikiNode * target = nodes[node->references[node->forward_length+j]];
            if (!target||target->backward_length==0) {
                node->references[node->forward_length+j] = unreachable;
                removed++;
            }
        }
    }
    for (size_t i=0;i<titleCount;i++) {
        struct wikiNode * node = nodes[i];
        size_t write = 0;
        if (!node) continue;
        for (size_t read=0;read<node->backward_length;read++) {
            if (node->references[read+node->forward_length]!=unreachable) {
                node->references[write+node->forward_length] = node->references[read+node->forward_length];
                write++;
            }
        }
        node->backward_length = write;
    }
    return removed;
}
static nodeRef * getNodeOffsets(struct wikiNode ** nodes, size_t titleCount) {
    nodeRef offset = 0;
    nodeRef * offsets = malloc(sizeof(nodeRef)*titleCount);
    for (size_t i=0;i<titleCount;i++) {
        offsets[i] = offset;
        if (!nodes[i]) continue;
        offset += sizeof(struct wikiNode)+(nodes[i]->forward_length+nodes[i]->backward_length)*sizeof(nodes[i]->references[0]);
    }
    fprintf(stderr, "Total node size: %u\n", offset);
    return offsets;
}

static struct wikiNode ** replaceIdsWithOffsets(struct wikiNode ** nodes, size_t titleCount, nodeRef * offsets) {
    for (size_t i=0;i<titleCount;i++) {
        struct wikiNode * node = nodes[i];
        if (!node) continue;
        for (size_t j=0;j<node->forward_length+node->backward_length;j++) {
            node->references[j] = offsets[node->references[j]];
        }
    }
    return nodes;
}

static void writeTitlesToFile(FILE * in, FILE * out, struct wikiNode ** nodes) {
    int c;
    size_t i = 0;
    bool inTitle = true;
    rewind(in);
    while ((c = getc_unlocked(in)) != EOF) {
        if (inTitle) {
            if (c) {
                if (nodes[i]) putc_unlocked(c, out);
            } else {
                inTitle = false;
                if (nodes[i]) putc_unlocked('\n', out);
            }
        } else if (c=='\n') {
                inTitle = true;
                i++;
        }
    }
}

static void writeNodesToFile(struct wikiNode ** nodes, size_t titleCount, FILE * f) {
    for (size_t i=0;i<titleCount;i++) {
        struct wikiNode * node = nodes[i];
        if (!node) continue;
        node->dist_a = 0;
        node->dist_b = 0;
        fwrite(node, 1, sizeof(*node)+(node->forward_length+node->backward_length)*sizeof(node->references[0]), f);
    }
}

int main(int argc, char **argv) {
    FILE * in       = NULL;
    FILE * nodeOut  = NULL;
    FILE * titleOut = NULL;
    char ** id2title;
    size_t titleCount;
    struct wikiNode ** id2node;
    nodeRef * id2nodeOffset;
    //char search[256];
    if (argc<4) {
        fprintf(stderr, "Usage: %s <input file> <node output file> <title output file>\n", argv[0]);
        return 1;
    }
    in       = openFileAndCheck(argv[1], "r", "input",        stdin );
    nodeOut  = openFileAndCheck(argv[2], "w", "node output",  stdout);
    titleOut = openFileAndCheck(argv[3], "w", "title output", stdout);

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

    fprintf(stderr, "Removed %zu useless redirection pages\n", removeUselessRedirectionPages(id2node, titleCount));
    fprintf(stderr, "Removed %zu backward references to unreachable node\n", removeSomeBackwardRefs(id2node, titleCount));

    id2nodeOffset = getNodeOffsets(id2node, titleCount);
    id2node = replaceIdsWithOffsets(id2node, titleCount, id2nodeOffset);
    fprintf(stderr, "Replaced ids with offsets\n");
    free(id2nodeOffset);

    writeNodesToFile(id2node, titleCount, nodeOut);
    writeTitlesToFile(in, titleOut, id2node);

    return 0;
}
