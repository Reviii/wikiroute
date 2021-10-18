#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "iteratefile.h"

static char * startStr = "INSERT INTO `pagelinks` VALUES ";
void parseSql(FILE * in) {
    int bracketLevel = 0;
    int len = 0;
    int i = 0;
    bool started = false;

    int res = 0;
    char resStr[256];
    iterateFile(in, c,
        if (!started) {
            if (c==startStr[len]) {
                len++;
            } else {
                len = 0;
            }
            if (len==strlen(startStr)) {
                started = true;
                len = 1; // The first entry doesn't have a preceding comma
                printf("started\n");
            }
            continue;
        }
//        printf("char: '%c', bracketLevel: %d, i: %d, len: %d\n", c, bracketLevel, i, len);
        if (bracketLevel==0) {
            assert(len<2);
            if (len==0) {
                assert(c==',');
                len++;
            } else if (len==1) {
                assert(c=='(');
                bracketLevel++;
            }
            continue;
        }
        switch (i) {
        case 0:
        case 1:
        case 3:
            if (c>='0'&&c<='9') {
               res = res*10 + (c-'0');
            } else if (c==(i==3 ? ')' : ',' )) {
                printf("value[%d] = %d\n", i, res);
                i++;
                res = 0;
                len = 0;
                if (i==4) { // after i++
                    bracketLevel = 0;
                    i = 0;
                }
            } else {
                printf("Unexpected char: %c\n", c);
                assert(false);
            }
            break;
        case 2:
            if (len==-1) {
                assert(c==',');
                i++;
                len = 0;
                break;
            }
            if (len==0) {
                assert(c=='\'');
                len++;
                break;
            }
            // TODO: handle escaping
            if (c=='\'') {
                resStr[len-1] = 0;
                printf("value[%d] = '%s'\n", i, resStr);
                len = -1;
                break;
            }
            assert(len<sizeof(resStr)+1-1); // minus one for one extra byte reserved for the null byte
            resStr[len-1] = c;
            len++;
            break;
        }
    )
}

int main() {
    parseSql(stdin);
    return 0;
}
