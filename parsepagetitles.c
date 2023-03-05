#include <stdio.h>
#include "parsesql.h"


static void printRecord(const union fieldData * record, const enum fieldType * fieldTypes, int recordSize) {
    (void) fieldTypes;
    (void) recordSize;
    if (record[1].integer!=0) return; // wrong namespace
    // TODO: maybe prevent conversion to decimal here?
    // it is currently needed for sort
    printf("%d %s\n", record[0].integer, record[2].string);
}

static char * startStr = "INSERT INTO `page` VALUES ";
int main(int argc, char ** argv) {
    if (argc>1) {
        fprintf(stderr, "Feed sql from stdin\n");
    }
    // id, ns, title, redirect?, new?, random, touched, links_updated, latest, len, content_model, lang
    // TODO: test performance with different ignore types instead of int
    enum fieldType types[] = {TYPE_INT,TYPE_INT,TYPE_STR,TYPE_INT,TYPE_INT,TYPE_IGNORE,TYPE_STR,TYPE_STR,TYPE_INT,TYPE_INT,TYPE_STR,TYPE_NULL};
    parseSql(stdin, startStr, types, sizeof(types)/sizeof(types[0]), &printRecord);
    return 0;
}
