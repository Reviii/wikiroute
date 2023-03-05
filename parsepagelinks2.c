#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "buffer.h"
#include "iteratefile.h"
#include "parsesql.h"

// based on version in parselinks
static char ** getTitleListFromFile(FILE * f, size_t * titleCount) {
    struct buffer stringBuf = bufferCreate();
    struct buffer offsetBuf = bufferCreate();
    bool inTitle = false;
    int pageID = 0;
    int lastPageID = -1;
    size_t* offsets = NULL;
    char ** res = NULL;

    iterateFile(f, c,
        if (inTitle) {
            *bufferAdd(&stringBuf, sizeof(char)) = c =='\n' ? '\0' : c;
            if (c=='\n') {
                inTitle = false;
            }
        } else {
            if (c>='0'&&c<='9') {
                pageID = 10*pageID + (c-'0');
            } else if (c==' ') {
                inTitle = true;
                assert(pageID>lastPageID);
                lastPageID++;
                for (;lastPageID<pageID;lastPageID++) {
                    *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = (size_t)-1;
                }
                lastPageID = pageID;
                pageID = 0;
                *(size_t *)bufferAdd(&offsetBuf, sizeof(size_t)) = stringBuf.used;
            } else {
                fprintf(stderr, "Unexpected char: '%c'\n",c);
                assert(false);
            }
        }
    )
    // uncomment the following line to reduce virtual memory usage
    //bufferCompact(&stringBuf);

    offsets = (size_t *)offsetBuf.content;
    res = malloc(offsetBuf.used/sizeof(offsets[0])*sizeof(char *));
    for (size_t i=0;i<offsetBuf.used/sizeof(offsets[0])-1;i++) {
        if (offsets[i]==(size_t)-1) {
            res[i] = NULL;
            continue;
        }
        res[i] = stringBuf.content + offsets[i];
    }
    res[offsetBuf.used/sizeof(offsets[0])-1] = NULL;
    *titleCount = offsetBuf.used/sizeof(offsets[0])-1;
    free(offsets);
    // not freeing stringBuf content, because it is referenced by res
    return res;
}

char ** titles;
size_t titleCount;
static void printRecord(const union fieldData * record, const enum fieldType * fieldTypes, int recordSize) {
    (void) fieldTypes;
    (void) recordSize;
    if (record[1].integer==0&&record[0].integer<titleCount&&titles[record[0].integer]) {
        printf("%s", titles[record[0].integer]);
        putchar('\0');
        putchar('r');
        printf("%s", record[2].string);
        putchar('\0');
        putchar('\n');
    }
}

int main(int argc, char ** argv) {
    if (argc<4) {
        fprintf(stderr, "Usage: %s <pagetitles> <pagelinks> <redirectlinks>\n",argv[0]);
        return 1;
    }
    FILE * titleFile = NULL;
    if (strcmp(argv[1],"-")) {
        titleFile = fopen(argv[1], "r");
    } else {
        titleFile = stdin;
    }
    if (!titleFile) {
        perror("Failed to open title file");
        return -1;
    }
    FILE * linkFile = NULL;
    if (strcmp(argv[2],"-")) {
        linkFile = fopen(argv[2], "r");
    } else {
        linkFile = stdin;
    }
    if (!linkFile) {
        perror("Failed to open link file");
        return -1;
    }
    FILE * redirectFile = NULL;
    if (strcmp(argv[3],"-")) {
        redirectFile = fopen(argv[3], "r");
    } else {
        redirectFile = stdin;
    }
    if (!redirectFile) {
        perror("Failed to open redirect file");
        return -1;
    }
    titles = getTitleListFromFile(titleFile,&titleCount);

    bool inLink = false;
    bool ignoreLine = false;
    bool first = true;
    int pageID = 0;
    int lastPageID = -1;
    iterateFile(linkFile, c,
        if (inLink) {
            if (c=='\n') {
                inLink = false;
                if (!ignoreLine) putchar('\0');
                continue;
            }
            if (!ignoreLine) putchar(c);
        } else {
            if (c>='0'&&c<='9') {
                pageID = 10*pageID + (c-'0');
            } else if (c==' ') {
                inLink = true;
                if (pageID!=lastPageID) {
                    if ((size_t)pageID<titleCount&&titles[pageID]) {
                        if (!first) putchar('\n');
                        printf("%s", titles[pageID]);
                        ignoreLine = false;
                        first = false;
                        putchar('\0');
                    } else {
                        ignoreLine = true;
                    }
                }
                lastPageID = pageID;
                pageID = 0;
                if (!ignoreLine) putchar('l');
            }
        }
    )

    // print redirect links
    static char * startStr = "INSERT INTO `redirect` VALUES ";
    // from, ns, title, interwiki, fragment
    enum fieldType types[] = {TYPE_INT,TYPE_INT,TYPE_STR,TYPE_STR,TYPE_STR};
    parseSql(redirectFile, startStr, types, sizeof(types)/sizeof(types[0]), &printRecord);
    return 0;
}
