#include <stdio.h>
#include "parsesql.h"

static void printRecord(const union fieldData * record, const enum fieldType * fieldTypes, int recordSize) {
    (void) fieldTypes;
    (void) recordSize;
    if (record[1].integer==0&&record[3].integer==0) printf("%d %s\n", record[0].integer, record[2].string);
}

static char * startStr = "INSERT INTO `pagelinks` VALUES ";
int main(int argc, char ** argv) {
    if (argc>1) {
        fprintf(stderr, "Usage: feed sql data from stdin\n");
        return 1;
    }
    enum fieldType types[] = {TYPE_INT,TYPE_INT,TYPE_STR,TYPE_INT,TYPE_IGNORE};
    parseSql(stdin, startStr, types, sizeof(types)/sizeof(types[0]), &printRecord);
    return 0;
}
