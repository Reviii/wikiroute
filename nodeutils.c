#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "buffer.h"
#include "iteratefile.h"
#include "nodetypes.h"
#include "nodeutils.h"

struct wikiNode ** getNodes(char * nodeData, size_t nodeDataLength, size_t * nodeCount) {
    struct wikiNode * node = (struct wikiNode *) nodeData;
    struct buffer nodeBuf = bufferCreate();
    *nodeCount = 0;
    while ((char *)node<nodeData+nodeDataLength) {
        *(struct wikiNode **)bufferAdd(&nodeBuf, sizeof(struct wikiNode *)) = node;
        (*nodeCount)++;
        node = (struct wikiNode *)((char *)node + sizeof(*node) + (node->forward_length+node->backward_length)*sizeof(node->references[0]));
    }
    return (struct wikiNode **) nodeBuf.content;
}

size_t * getTitleOffsets(char * titleData, size_t titleDataLength, size_t * titleCount) {
    size_t offset = 0;
    struct buffer offsetBuf = bufferCreate();
    *titleCount = 0;
    *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = 0;
    for (size_t i=0;i<titleDataLength;i++) {
        if (titleData[i]=='\n') {
            *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = offset+1;
            (*titleCount)++;
        }
        offset++;
    }
    return (size_t *) offsetBuf.content;
}

char * getTitle(char * titles, size_t * titleOffsets, size_t id) {
    size_t titleLength = titleOffsets[id+1]-titleOffsets[id];
    char * title = malloc(titleLength);
    memcpy(title, titles+titleOffsets[id], titleLength-1);
    title[titleLength-1] = '\0';
    return title;
}

char * getJSONTitle(char * titles, size_t * titleOffsets, size_t id) {
    size_t titleLength = titleOffsets[id+1]-titleOffsets[id];
    char * title = malloc(titleLength+3); // ,"title"
    memcpy(title+2, titles+titleOffsets[id], titleLength-1);
    size_t toEscape = 0;
    for (size_t i=0;i<titleLength-1;i++) {
        switch (title[i+2]) {
        case '\"':
        case '\\':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            toEscape++;
        }
    }
    if (toEscape==0) {
        title[0] = ',';
        title[1] = '\"';
        title[titleLength+1] = '\"';
        title[titleLength+2] = '\0';
        return title;
    }
    char * escapedTitle = malloc(titleLength+toEscape+3);
    char * ret = escapedTitle;
    *escapedTitle++ = ',';
    *escapedTitle++ = '\"';
    for (size_t i=0;i<titleLength-1;i++) {
        switch (title[i+2]) {
        case '\"':
        case '\\':
            *escapedTitle++ = '\\';
            *escapedTitle++ = title[i+2];
            break;
        case '\b':
            *escapedTitle++ = '\\';
            *escapedTitle++ = 'b';
            break;
        case '\f':
            *escapedTitle++ = '\\';
            *escapedTitle++ = 'f';
            break;
        case '\n':
            *escapedTitle++ = '\\';
            *escapedTitle++ = 'n';
            break;
        case '\r':
            *escapedTitle++ = '\\';
            *escapedTitle++ = 'r';
            break;
        case '\t':
            *escapedTitle++ = '\\';
            *escapedTitle++ = 't';
            break;
        default:
            *escapedTitle++ = title[i+2];
        }
    }
    *escapedTitle++ = '\"';
    *escapedTitle++ = '\0';
    assert(escapedTitle-ret==titleLength+toEscape+3);
    return ret;
}

nodeRef nodeOffsetToId(nodeRef * nodeOffsets, size_t nodeCount, nodeRef nodeOffset) {
    ssize_t first, middle, last;
    first = 0;
    last = nodeCount - 1;
    middle = (first+last)/2;
    while (first<=last) {
        if (nodeOffset>nodeOffsets[middle]) {
            first = middle+1;
        } else if (nodeOffset==nodeOffsets[middle]) {
            return middle;
        } else {
            last = middle-1;
        }
        middle = (first+last)/2;
    }
    return -1;
}

void normalizeTitle(char * title) {
    if (*title>='a'&&*title<='z') *title -= 32;
    for (;*title;title++) {
        if (*title=='_') *title = ' ';
    }
}

nodeRef titleToNodeId(char * titles, size_t * titleOffsets, size_t nodeCount, char * title) {
    ssize_t first, middle, last;
    first = 0;
    last = nodeCount - 1;
    middle = (first+last)/2;
    normalizeTitle(title);
    while (first<=last) {
        int cmp;
        char * cmpTitle = getTitle(titles, titleOffsets, middle);
        assert(cmpTitle);
        normalizeTitle(cmpTitle);
        cmp = strcmp(title, cmpTitle);
        free(cmpTitle);
        if (cmp>0) {
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
