#include <stdio.h>
#include <stdbool.h>
static void printlink(const unsigned char * link, int length, bool redirect, bool ignore) {
    if (ignore) {
        return;
    }
    if (redirect) {
        putchar('r');
    } else {
        putchar('l');
    }
    for (int i=0;i<length;i++) {
        if (link[i]!='\n'&&link[i]!='\0') putchar(link[i]);
    }
    putchar('\0');
}
static const unsigned char * syntax = "#REDIRECT [[";

// TODO: split state into several parts:
// the global state and how many characters have been correct
// something like STATE_HASH STATE_REDIRECT STATE_REDIRECTINOTHERLANGUAGE and STATE_LINK
// syntax should be split up in a similar way

void printlinks(const unsigned char * wikiText, int length) {
    int state = 0;
    bool redirect = true;
    bool ignore = false;
    unsigned char * link = NULL;
    for (int i=0;i<length;i++) {
        if (state<12) {
            if (wikiText[i]==syntax[state]) {
                state++;
            } else if (wikiText[i]=='[') {
                state = 11;
                redirect = false;
            } else {
                state = 0;
                redirect = true;
                ignore = false;
            }
        } else {
            if (!link) link = (unsigned char *) (wikiText+i);
            if (wikiText[i]==']'||wikiText[i]=='|'||wikiText[i]=='#') {
                printlink(link, (wikiText+i)-link, redirect, ignore);
                link = NULL;
                state = 0;
                redirect = true;
                ignore = false;
            } else if (wikiText[i]==':') {
                ignore = true;
            }
        }
    }
}
