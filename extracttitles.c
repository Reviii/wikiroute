#include <stdbool.h>
#include <stdio.h>

int main(int argc, char ** argv) {
    FILE * f = NULL;
    int c;
    bool inTitle = true;
    if (argc<2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }
    f = fopen(argv[1], "r");
    if (!f) {
        perror("Failed to open file");
        return 1;
    }
    while ((c = fgetc(f)) != EOF) {
        if (inTitle) {
            if (c) {
                putchar(c);
            } else {
                inTitle = false;
                putchar('\n');
            }
        } else {
            if (c=='\n')
                inTitle = true;
        }
    }
    return 0;
}
