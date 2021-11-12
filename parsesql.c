#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "buffer.h"
#include "iteratefile.h"

static char * startStr = "INSERT INTO `pagelinks` VALUES ";
void parseSql(FILE * in) {
    int bracketLevel = 0;
    int len = 0;
    bool escaped = false;
    int i = 0;
    bool started = false;

    int res = 0;
    char resStr[256];
    char prevStr[256];
    int link = 0;

    struct buffer links = bufferCreate();
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
        switch (i) {
        case 0:
        case 1:
        case 3:
            if (c>='0'&&c<='9') {
               res = res*10 + (c-'0');
            } else if (c==(i==3 ? ')' : ',' )) {
                if (i==0) {
                    link = res;
                }
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
            if (c=='\\'&&!escaped) {
                escaped = true;
                break;
            }
            if (c=='\''&&!escaped) {
                resStr[len-1] = 0;
                if (strcmp(prevStr, resStr)) {
                    printf("title = '%s'\n", prevStr);
                    for (int i=0;i<links.used/sizeof(int);i++) {
                        printf("%d\n", ((int *)links.content)[i]);
                    }
                    links.used = 0;
                    static_assert(sizeof(prevStr)>=sizeof(resStr), "");
                    strcpy(prevStr, resStr);
                }
                *(int *) bufferAdd(&links, sizeof(int)) = link;
                len = -1;
                break;
            }
            assert(len<sizeof(resStr)+1-1); // minus one for one extra byte reserved for the null byte
            resStr[len-1] = c;
            len++;
            escaped = false;
            break;
        }
    )
}

int main() {
    parseSql(stdin);
    return 0;
}
