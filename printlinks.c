#include <stdio.h>
#include <stdbool.h>
static void printlink(const char * link, int length, bool redirect, bool ignore) {
    if (ignore) {
        return;
    }
    if (redirect) {
        putchar('r');
    } else {
        putchar('l');
    }
    for (int i=0;i<length;i++) {
        if (link[i]=='_') {
            putchar(' ');
        } else if (link[i]!='\n'&&link[i]!='\0') {
            putchar(link[i]);
        }
    }
    putchar('\0');
}

static const char * langParts[] = {
"REDIRECT",
"DOORVERWIJZING"};

void printlinks(const char * wikiText, size_t length) {
    enum State {STATE_TEXT, STATE_HASH, STATE_R_EN, STATE_R_NL, STATE_REDIRECT, STATE_ALMOSTLINK, STATE_LINK, STATE_LINKEND};
    enum State state = STATE_TEXT;
    bool redirect = false;
    const char * link = NULL;
    size_t correctChars = 0;
    for (size_t i=0;i<length;i++) {
        char c = wikiText[i];
//        printf("Encountered '%c' at state %u correctChars: %u redirect: %s\n", c, state, correctChars, redirect ? "true" : "false");
        switch (state) {
        case STATE_REDIRECT:
            if (c!=' '&&c!='[') {
                redirect = false;
                state = STATE_TEXT;
            }
            /* fall through */
        case STATE_TEXT:
            if (c=='#') {
                state = STATE_HASH;
            } else if (c=='[') {
                state = STATE_ALMOSTLINK;
            }
            break;
        case STATE_HASH:
            if (c=='R') {
                state = STATE_R_EN;
                correctChars = 1;
            } else if (c=='D') {
                state = STATE_R_NL;
                correctChars = 1;
            } else {
                state = STATE_TEXT;
            }
            break;
        case STATE_R_EN:
        case STATE_R_NL:
            if (c==langParts[state-STATE_R_EN][correctChars]) {
                correctChars++;
                if (langParts[state-STATE_R_EN][correctChars]=='\0') {
                    redirect = true;
                    state = STATE_REDIRECT;
                }
            } else {
                state = STATE_TEXT;
            }
            break;
        case STATE_ALMOSTLINK:
            if (c!='[') {
                redirect = false;
                state = STATE_TEXT;
                break;
            }
            state = STATE_LINK;
            link = wikiText+i+1;
            correctChars = 0;
            break;
        case STATE_LINK:
            if (c==']'||c=='|') {
                printlink(link, correctChars, redirect, false);
                redirect = false;
                state = STATE_LINKEND;
                correctChars = (c==']');
                break;
            }
            if (c==':') {
                state = STATE_LINKEND;
                correctChars = 0;
                break;
            }
            correctChars++;
            break;
        case STATE_LINKEND:
            if (c==']') {
                correctChars++;
            } else {
                correctChars = 0;
            }
            if (correctChars==2)
                state = STATE_TEXT;
            break;
        }
    }
}
