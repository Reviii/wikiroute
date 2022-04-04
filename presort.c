#include <stdbool.h>
#include <stdio.h>

#include "iteratefile.h"

static char uppercaseChar(char c) {
    if (c>='a'&&c<='z') return c-32;
    return c;
}

int main(int argc, char ** argv) {
    bool firstChar = true;
    iterateFile(stdin, c,
        if (firstChar) {
            putc_unlocked(uppercaseChar(c), stdout);
            firstChar = false;
        }
        if (c=='\n') firstChar = true;
        putc_unlocked(c, stdout);
    )
    return 0;
}
