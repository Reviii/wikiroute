#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>

static char hex2int(char c) {
    if (c>='0'&&c<='9') return c-'0'+1;
    if (c>='A'&&c<='F') return c-'A'+11;
    if (c>='a'&&c<='f') return c-'a'+11;
    return 0;
}
static char hexLookup[256];
static void __attribute__((constructor)) createHexLookup() {
    for (int i=0;i<256;i++) {
        hexLookup[i] = hex2int(i);
    }
}


static void printlink(const char * link, int length, bool redirect) {
    if (redirect) {
        putchar('r');
    } else {
        putchar('l');
    }
    const unsigned char * ulink = (const unsigned char *) link;
    for (int i=0;i<length;i++) {
        if (link[i]=='_') {
            putchar(' ');
        } else if (link[i]=='%'&&i+2<length&&hexLookup[ulink[i+1]]&&hexLookup[ulink[i+2]]) {
            int c = (hexLookup[ulink[i+1]]*16+hexLookup[ulink[i+2]]-17);
            i+=2;
            if (c>0&&c!='\0'&&c!='\n') putchar(c);
        } else {
            putchar(link[i]);
        }
    }
    putchar('\0');
}

static const char * langParts[] = {
"REDIRECT",
"DOORVERWIJZING"};

void printlinks(const char * wikiText, size_t length) {
    enum State {STATE_TEXT, STATE_HASH, STATE_R_EN, STATE_R_NL, STATE_REDIRECT, STATE_ALMOSTLINK, STATE_LINKSTART, STATE_LINK};
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
            if (c=='[') {
                state = STATE_ALMOSTLINK;
            } else if (c=='#') {
                state = STATE_HASH;
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
                i--;
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
                i--;
            }
            break;
        case STATE_ALMOSTLINK:
            if (c!='[') {
                redirect = false;
                state = STATE_TEXT;
                i--;
                break;
            }
            state = STATE_LINKSTART;
            link = wikiText+i+1;
            correctChars = 0;
            break;
        case STATE_LINKSTART:
            if (c==' '||c=='[') {
                link++;
                break;
            } else if (c=='#') {
                redirect = false;
                state = STATE_TEXT;
                break;
            } else {
                state = STATE_LINK;
            }
            /* fall through */
        case STATE_LINK:
            if (c==']'||c=='|'||c=='#') {
                while (link[correctChars-1]==' ') correctChars--;
                printlink(link, correctChars, redirect);
                redirect = false;
                state = STATE_TEXT;
                break;
            }
            if (c==':'||c=='\n'||c=='\0') {
                state = STATE_TEXT;
                break;
            }
            correctChars++;
            break;
        }
    }
}
