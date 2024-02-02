#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "buffer.h"
#include "iteratefile.h"
#include "parsesql.h"

void parseSql(FILE * in, const char * startStr, const enum fieldType * fieldTypes, int recordSize, void (*fp) (const union fieldData *, const enum fieldType *, int)) {
    int bracketLevel = 0;
    int len = 0; // length of current string being parsed, -1 indicates end of string
    bool nullstring = false;
    bool escaped = false;
    int record_i = 0;
    bool started = false;

    union fieldData record[recordSize];
    const int stringSize = 256;
    for (int i=0;i<recordSize;i++) {
        if (fieldTypes[i]==TYPE_STR) {
            record[i].string = malloc(stringSize);
        }
    }
    int res = 0;
    int sign = 1;

    iterateFile(in, c,
        if (!started) {
            if (c==startStr[len]) {
                len++;
            } else {
                len = 0;
            }
            if (len==(int)strlen(startStr)) {
                started = true;
                len = 1; // The first entry doesn't have a preceding comma
//                fprintf(stderr, "started new line\n");
            }
            continue;
        }
        if (record_i>=recordSize) {
            assert(record_i==recordSize);
            fp(record, fieldTypes, recordSize);
            record_i = 0;
            bracketLevel--;
        }
//        printf("char: '%c', bracketLevel: %d, i: %d, type: %s, len: %d\n", c, bracketLevel, record_i, fieldTypes[record_i]==TYPE_INT ? "int" : "str", len);
        if (bracketLevel==0) {
            assert(len<2);
            if (len==0) {
                if (c==';') {
                    started = false;
                    continue;
                }
                assert(c==',');
                len++;
            } else if (len==1) {
                assert(c=='(');
                bracketLevel++;
            }
            continue;
        }
        char endChar = (record_i==recordSize-1 ? ')' : ',' );
        if (fieldTypes[record_i]==TYPE_INT) {
            if (c>='0'&&c<='9') {
               res = res*10 + (c-'0');
            } else if (c==endChar) {
                record[record_i].integer = res*sign;
                record_i++;
                res = 0;
                sign = 1;
                len = 0; // this needed?
            } else if (c=='-'&&res==0) {
                sign = -sign;
            } else {
                printf("Unexpected char: %c\n", c);
                assert(false);
            }
        } else if (fieldTypes[record_i]==TYPE_STR&&!nullstring) {
            if (len==-1) {
                assert(c==endChar);
                record_i++;
                len = 0;
                continue;
            }
            if (len==0) {
                if (c=='N') {
                    nullstring = true;
                    record[record_i].string[0] = '\0'; // treat null strings as empty strings
                }
                assert(c=='\''||c=='N');
                len++;
                continue;
            }
            if (c=='\\'&&!escaped) {
                escaped = true;
                continue;
            }
            if (c=='\''&&!escaped) {
                record[record_i].string[len-1] = 0;
                len = -1;
                continue;
            }
            assert(len<stringSize+1-1); // minus one for one extra byte reserved for the null byte
            record[record_i].string[len-1] = c;
            len++;
            escaped = false;
            continue;
        } else if (fieldTypes[record_i]==TYPE_IGNORE) {
            if (c==endChar) {
                record_i++;
                len = 0; // this needed?
            }
        } else if (nullstring||fieldTypes[record_i]==TYPE_NULL) {
            if (len<4) {
                assert(c=="NULL"[len]);
                len++;
            } else {
               assert(c==endChar);
               record_i++;
               len = 0;
               nullstring=false;
            }
        } else if (fieldTypes[record_i]==TYPE_INT_IGNORE) {
            if (c>='0'&&c<='9') {
                ;
            } else if (c==endChar) {
                record_i++;
                len = 0; // this needed?
            } else {
                printf("Unexpected char: %c\n", c);
                assert(false);
            }
        }
    )
}
