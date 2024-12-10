#include <stdio.h>
#include "parsesql.h"

static void printRecord(const union fieldData * record, void * data) {
    (void) data;
    if (record[1].integer==0) printf("%d %d\n", record[0].integer, record[2].integer);
}

static char * startStr = "INSERT INTO `pagelinks` VALUES ";
int main(int argc, char ** argv) {
    if (argc>1) {
        fprintf(stderr, "Usage: feed sql data from stdin\n");
        return 1;
    }
                              // from  namespace to
    enum fieldType types[] = {TYPE_INT,TYPE_INT,TYPE_INT};
    parseSql(stdin, startStr, types, sizeof(types)/sizeof(types[0]), &printRecord, NULL);
    return 0;
}
