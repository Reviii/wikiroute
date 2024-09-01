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
                size_t len = stringBuf.used-1-*(size_t *)(offsetBuf.content+offsetBuf.used-sizeof(size_t));
                for (;len<2;len++) *bufferAdd(&stringBuf, sizeof(char)) = '\0'; // reduce edge cases in title2id
                *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = stringBuf.used;
            }
        } else {
            if (c=='\n') {
                inTitle = true;
                firstChar = true;
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

static size_t titleHash(char const * title) {
    unsigned char const * t = (unsigned char const *) title;
    // note that an empty string can give any result between 0 and 255
    // and that there must at least be 2 allocated bytes for the title
    // in order to prevent UB
    return t[0]*256+t[1];
}

static size_t * getTitleMap(char ** id2title, size_t titleCount) {
    size_t * map = malloc((256*256+1)*sizeof(size_t));
    size_t i = 0;
    for (size_t id=0;id<titleCount;id++) {
        for (;titleHash(id2title[id])>=i;i++) {
            assert(i<256*256);
            map[i] = id;
        }
    }
    for (;i<=256*256;i++) {
        map[i] = titleCount;
    }
    for (size_t j=0;j<256;j++) {
        assert(map[j]==0); // make sure there are no empty strings in the title map
    }
    return map;
}

static size_t title2id(char ** id2title, size_t * map, char * title) {
    ssize_t first, last, middle;
    size_t correct = 2;
    first = map[titleHash(title)];
    last = map[titleHash(title)+1] - 1;
    middle = (first+last)/2;
    for (size_t i=0;i<correct;i++) { // skip the first correct characters, unless the title is too short
        if (!*title) {
            correct = i;
            break;
        }
        title++;
    }
    while (first <= last) {

        // inlined version of int cmp = strcmp(id2title[middle]+correct, title);
        unsigned char * stra = (unsigned char *)id2title[middle]+correct; // edge case (len<2) is handled in getTitleListFromFile
        unsigned char * strb = (unsigned char *)title;
        while (*stra==*strb&&*stra) {
            stra++;
            strb++;
        }
        int cmp = *stra-*strb;

        if (cmp<0) {
            first = middle+1;
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

static struct wikiNode ** getNodes(FILE * f, char ** id2title, size_t titleCount, size_t * titleMap) {
    nodeRef id = 0;
    bool inLink = false;
    struct buffer titleBuf = bufferCreate();
    struct wikiNode ** nodes = malloc((titleCount+1)*sizeof(struct wikiNode *));
    rewind(f);
    nodes[0] = calloc(sizeof(struct wikiNode), 1);
    iterateFile(f, c,
        if (inLink) {
            *(bufferAdd(&titleBuf, 1)) = c;
            if (c=='\0') {
                titleBuf.content[1] = uppercaseChar(titleBuf.content[1]); // make first char case insensitive
                nodeRef ref = title2id(id2title, titleMap, titleBuf.content+1); // titleBuf.content+1, becauce the first char needs to be ignored
                titleBuf.used = 0;
                if (ref==(nodeRef)-1||ref==id) continue;
                assert(titleBuf.content[0]=='r'||titleBuf.content[0]=='l');
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
        size_t write = 0;
        for (size_t read=0;read<node->forward_length;read++) {
            if (redirects[node->references[read]]==i) continue;
            nodeRef newRef = redirects[node->references[read]];
            for (size_t j=0;j<read;j++) {
                if (node->references[j]==newRef) goto afterwrite;
            }
            node->references[write] = newRef;
            write++;
afterwrite:
        }
        node->forward_length = write;
        // maybe realloc?
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
    // remove backward refs to removed redirects
    for (size_t i=0;i<titleCount;i++) {
        struct wikiNode * node = nodes[i];
        if (!node) continue;
        size_t write = 0;
        for (size_t read=0;read<node->backward_length;read++) {
            if (nodes[node->references[node->forward_length+read]]) {
                node->references[node->forward_length+write] = node->references[node->forward_length+read];
                write++;
            } else {
//                removed++;
            }
        }
        node->backward_length = write;
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
static nodeRef * getNodeIds(struct wikiNode ** nodes, size_t titleCount) {
    nodeRef offset = 0;
    nodeRef id = 0;
    nodeRef * ids = malloc(sizeof(nodeRef)*titleCount);
    for (size_t i=0;i<titleCount;i++) {
        ids[i] = id;
        if (!nodes[i]) continue;
        offset += sizeof(struct wikiNode)+(nodes[i]->forward_length+nodes[i]->backward_length)*sizeof(nodes[i]->references[0]);
        id++;
    }
    fprintf(stderr, "Total node size: %u\n", offset);
    return ids;
}

static struct wikiNode ** replaceIdsWithNewIds(struct wikiNode ** nodes, size_t titleCount, nodeRef * ids) {
    for (size_t i=0;i<titleCount;i++) {
        struct wikiNode * node = nodes[i];
        if (!node) continue;
        for (size_t j=0;j<node->forward_length+node->backward_length;j++) {
            node->references[j] = ids[node->references[j]];
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
    size_t * titleMap;
    nodeRef * id2newId;
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

    titleMap = getTitleMap(id2title, titleCount);
    fprintf(stderr, "Created title mapping\n");

    id2node = getNodes(in, id2title, titleCount, titleMap);
    fprintf(stderr, "Created nodes\n");
    free(id2title[0]);
    free(id2title);

    id2node = applyRedirects(id2node, titleCount);
    fprintf(stderr, "Applied redirects\n");

    id2node = addBackwardRefs(id2node, titleCount);
    fprintf(stderr, "Added backward references\n");

    fprintf(stderr, "Removed %zu useless redirection pages\n", removeUselessRedirectionPages(id2node, titleCount));
    fprintf(stderr, "Removed %zu backward references to unreachable nodes\n", removeSomeBackwardRefs(id2node, titleCount));

    id2newId = getNodeIds(id2node, titleCount);
    id2node = replaceIdsWithNewIds(id2node, titleCount, id2newId);
    free(id2newId);

    writeNodesToFile(id2node, titleCount, nodeOut);
    writeTitlesToFile(in, titleOut, id2node);

    return 0;
}
