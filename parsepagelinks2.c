#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "buffer.h"
#include "iteratefile.h"
#include "parsesql.h"

struct buffers {
    struct buffer strings;
    uint32_t * offsets;
    size_t offsetsSize;
    int32_t maxID;
};
static void addTitle(const union fieldData * record, void * datavoid) {
    struct buffers * data = datavoid;
    if (record[1].integer!=0) return; // wrong namespace
    size_t id = record[0].integer;

    if ((int)id>data->maxID) data->maxID = id;
    size_t oldBufSize = data->offsetsSize;
    while (id>=oldBufSize) {
        size_t newBufSize = oldBufSize*2;
        if (newBufSize<1000) newBufSize=1000;
        data->offsets = realloc(data->offsets,sizeof(data->offsets[0])*newBufSize);
        memset(data->offsets+oldBufSize,255,(sizeof(data->offsets[0]))*(newBufSize-oldBufSize));
        data->offsetsSize = newBufSize;
        oldBufSize = data->offsetsSize;
    }


    char * title = bufferAdd(&data->strings,strlen(record[2].string)+1);
    strcpy(title,record[2].string);
    data->offsets[id] = title-data->strings.content;
}


static void printRecord(const union fieldData * record, void * datavoid) {
    struct buffers * data = datavoid;
    if (record[1].integer==0&&record[0].integer<=data->maxID&&data->offsets[record[0].integer]!=(uint32_t)-1) {
        printf("%s", data->strings.content+data->offsets[record[0].integer]);
        putchar('\0');
        putchar('r');
        printf("%s", record[2].string);
        putchar('\0');
        putchar('\n');
    }
}

int main(int argc, char ** argv) {
    if (argc<5) {
        fprintf(stderr, "Usage: %s <pages.sql> <linktarget.sql> <pagelinks> <redirectlinks.sql>\n",argv[0]);
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
    FILE * ltFile = NULL;
    if (strcmp(argv[2],"-")) {
        ltFile = fopen(argv[2], "r");
    } else {
        ltFile = stdin;
    }
    if (!ltFile) {
        perror("Failed to open link target file");
        return -1;
    }
    FILE * linkFile = NULL;
    if (strcmp(argv[3],"-")) {
        linkFile = fopen(argv[3], "r");
    } else {
        linkFile = stdin;
    }
    if (!linkFile) {
        perror("Failed to open link file");
        return -1;
    }
    FILE * redirectFile = NULL;
    if (strcmp(argv[4],"-")) {
        redirectFile = fopen(argv[4], "r");
    } else {
        redirectFile = stdin;
    }
    if (!redirectFile) {
        perror("Failed to open redirect file");
        return -1;
    }
    fprintf(stderr,"Parsing pages.sql\n");
    struct buffers titleData;
    {
        // id, ns, title, redirect?, new?, random, touched, links_updated, latest, len, content_model, lang
        enum fieldType types[] = {TYPE_INT,TYPE_INT,TYPE_STR,TYPE_INT,TYPE_INT,TYPE_IGNORE,TYPE_STR,TYPE_STR,TYPE_INT,TYPE_INT,TYPE_STR,TYPE_NULL};
        titleData.strings = bufferCreate();
        titleData.offsets = NULL;
        titleData.offsetsSize = 0;
        titleData.maxID = 0;
        parseSql(titleFile, "INSERT INTO `page` VALUES ", types, sizeof(types)/sizeof(types[0]), &addTitle, &titleData);
    }
    char * titleBuf = titleData.strings.content;
    uint32_t * titleOffsets = titleData.offsets;
    int maxTitleId = titleData.maxID;

    fprintf(stderr,"Parsing linktarget.sql\n");
    struct buffers ltData;
    {
        // id, ns, title
        enum fieldType types[] = {TYPE_INT,TYPE_INT,TYPE_STR};
        ltData.strings = bufferCreate();
        ltData.offsets = NULL;
        ltData.offsetsSize = 0;
        ltData.maxID = 0;
        void (*addLt) (const union fieldData *,void *) = addTitle; // begin link target and pages record is the same
        parseSql(ltFile, "INSERT INTO `linktarget` VALUES ", types, sizeof(types)/sizeof(types[0]), addLt, &ltData);
    }
    char * ltBuf = ltData.strings.content;
    uint32_t * ltOffsets = ltData.offsets;
    int maxLtId = ltData.maxID;
    fprintf(stderr,"Outputting normal links\n");

    bool inLink = false;
    bool ignoreLine = false;
    bool first = true;
    int pageID = 0;
    int lastPageID = -1;
    int linkPageID = 0;
    iterateFile(linkFile, c,
        if (inLink) {
            if (c=='\n') {
                inLink = false;
                if (!ignoreLine&&linkPageID>=0&&linkPageID<=maxLtId&&ltOffsets[linkPageID]!=(uint32_t)-1) {
                    printf("l%s",ltBuf+ltOffsets[linkPageID]);
                    putchar('\0');
                }
                linkPageID = 0;
                continue;
            }
            if (c>='0'&&c<='9') {
                linkPageID = 10*linkPageID + (c-'0');
            } else {
                assert(0);
            }
        } else {
            if (c>='0'&&c<='9') {
                pageID = 10*pageID + (c-'0');
            } else if (c==' ') {
                inLink = true;
                if (pageID!=lastPageID) {
                    if (pageID<=maxTitleId&&titleOffsets[pageID]!=(uint32_t)-1) {
                        if (!first) putchar('\n');
                        printf("%s", titleBuf+titleOffsets[pageID]);
                        ignoreLine = false;
                        first = false;
                        putchar('\0');
                    } else {
                        ignoreLine = true;
                    }
                }
                lastPageID = pageID;
                pageID = 0;
//                if (!ignoreLine) putchar('l');
            }
        }
    )
    putchar('\n');
    // print redirect links
    fprintf(stderr,"Printing redirect links (this will cause a lot of duplicates)\n");
    {
        static char * startStr = "INSERT INTO `redirect` VALUES ";
        // from, ns, title, interwiki, fragment
        enum fieldType types[] = {TYPE_INT,TYPE_INT,TYPE_STR,TYPE_STR,TYPE_STR};
        parseSql(redirectFile, startStr, types, sizeof(types)/sizeof(types[0]), &printRecord, &titleData);
    }
    return 0;
}
