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

nodeRef * getNodeOffsets(char * nodeData, size_t nodeDataLength, size_t * nodeCount) {
    nodeRef offset = 0;
    struct buffer offsetBuf = bufferCreate();
    *nodeCount = 0;
    while (offset<nodeDataLength) {
        struct wikiNode * node = (struct wikiNode *) (nodeData + offset);
        *(nodeRef *)bufferAdd(&offsetBuf, sizeof(nodeRef)) = offset;
        (*nodeCount)++;
        offset += sizeof(*node) + (node->forward_length+node->backward_length)*sizeof(node->references[0]);
    }
    return (nodeRef *) offsetBuf.content;
}

size_t * getTitleOffsets(FILE * f, size_t * titleCount) {
    size_t offset = 0;
    struct buffer offsetBuf = bufferCreate();
    *titleCount = 0;
    *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = 0;
    iterateFile(f, c,
        if (c=='\n') {
            *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = offset+1;
            (*titleCount)++;
        }
        offset++;
    )
    return (size_t *) offsetBuf.content;
}

char * getTitle(FILE * titles, size_t * titleOffsets, size_t id) {
    char * title = malloc(256);
    size_t titleLength = 256;
    fseek(titles, titleOffsets[id], SEEK_SET);
    getline(&title, &titleLength, titles);
    title[strlen(title)-1] = '\0';
    return title;
}

char * getJSONTitle(FILE * titles, size_t * titleOffsets, size_t id) {
    size_t titleLength = titleOffsets[id+1]-titleOffsets[id];
    char * title = malloc(titleLength+2);
    fseek(titles, titleOffsets[id], SEEK_SET);
    fread(title+1, titleLength-1, 1, titles);
    size_t toEscape = 0;
    for (size_t i=0;i<titleLength-1;i++) {
        switch (title[i+1]) {
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
        title[0] = '\"';
        title[titleLength] = '\"';
        title[titleLength+1] = '\0';
        return title;
    }
    char * escapedTitle = malloc(titleLength+toEscape+2);
    char * ret = escapedTitle;
    *escapedTitle++ = '\"';
    for (size_t i=0;i<titleLength-1;i++) {
        switch (title[i+1]) {
        case '\"':
        case '\\':
            *escapedTitle++ = '\\';
            *escapedTitle++ = title[i+1];
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
            *escapedTitle++ = title[i+1];
        }
    }
    *escapedTitle++ = '\"';
    *escapedTitle++ = '\0';
    assert(escapedTitle-ret==titleLength+toEscape+2);
    return ret;
}

char * nodeOffsetToTitle(FILE * titles, nodeRef * nodeOffsets, size_t * titleOffsets, size_t nodeCount, nodeRef nodeOffset) {
    nodeRef id = nodeOffsetToId(nodeOffsets, nodeCount, nodeOffset);
    if (id==(nodeRef)-1) return NULL;
    return getTitle(titles, titleOffsets, id);
}

char * nodeOffsetToJSONTitle(FILE * titles, nodeRef * nodeOffsets, size_t * titleOffsets, size_t nodeCount, nodeRef nodeOffset) {
    nodeRef id = nodeOffsetToId(nodeOffsets, nodeCount, nodeOffset);
    if (id==(nodeRef)-1) {
        char * ret = malloc(sizeof("null"));
        strcpy(ret, "null");
        return ret;
    }
    return getJSONTitle(titles, titleOffsets, id);
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

nodeRef titleToNodeOffset(FILE * titles, nodeRef * nodeOffsets, size_t * titleOffsets, size_t nodeCount, char * title) {
    nodeRef id = titleToNodeId(titles, titleOffsets, nodeCount, title);
    if (id==(nodeRef)-1) return id;
    return nodeOffsets[id];
}

nodeRef titleToNodeId(FILE * titles, size_t * titleOffsets, size_t nodeCount, char * title) {
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
