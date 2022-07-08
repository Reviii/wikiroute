#include <stdio.h>
#include "buffer.h"

extern void printlinks(const unsigned char *, size_t);

int main() {
    struct buffer text = bufferCreate();
    while (fread(bufferAdd(&text, 16384), 16384, 1, stdin)) {;}
    printlinks((const unsigned char*)text.content, text.used);
    putchar('\n');
    return 0;
}
