#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <libxml/xmlreader.h>

#ifdef LIBXML_READER_ENABLED

#define STATE_NULL 0
#define STATE_MEDIAWIKI 1
#define STATE_PAGE 2
#define STATE_TITLE 3
#define STATE_REVISION 4
#define STATE_FORMAT 5
#define STATE_TEXT 6

extern void printlinks(const unsigned char * wikiText, int length);
static int state = STATE_NULL;
static char title[256];
bool hasTitle = false;
bool correctFormat = false;
/**
 * processNode:
 * @reader: the xmlReader
 *
 * Dump information about the current node
 */
static bool noColon(const unsigned char * str) {
    for (int i=0;str[i];i++) {
        if (str[i]==':') {
            return false;
        }
    }
    return true;
}
static void
processNode(xmlTextReaderPtr reader) {
    const xmlChar *name, *value;
    int depth, nodeType;
    name = xmlTextReaderConstName(reader);
    if (name == NULL)
	name = BAD_CAST "--";
    value = xmlTextReaderConstValue(reader);
    depth = xmlTextReaderDepth(reader);
    nodeType = xmlTextReaderNodeType(reader);
    switch (state) {
    case STATE_NULL:
        if (depth==0&&nodeType==1&&strcmp(name, "mediawiki")==0) {
            state = STATE_MEDIAWIKI;
        }
        break;
    case STATE_MEDIAWIKI:
        if (depth==1&&nodeType==1&&strcmp(name, "page")==0) {
            state = STATE_PAGE;
        } else if (depth==0) state = STATE_NULL;
        break;
    case STATE_PAGE:
        if (depth==2&&nodeType==1) {
            if (strcmp(name, "title")==0) {
                state = STATE_TITLE;
            } else if (strcmp(name, "revision")==0) {
                state = STATE_REVISION;
            }
        } else if (depth==1) {
            state = STATE_MEDIAWIKI;
            hasTitle = false;
            correctFormat = false;
        }
        break;
    case STATE_TITLE:
        if (depth==3&&nodeType==3&&strcmp(name, "#text")==0) {
            if (value) {
                strncpy(title, value, 256);
                title[255] = '\0';
                hasTitle = true;
            }
            state = STATE_PAGE;
        } else if (depth==2) state = STATE_PAGE;
        break;
    case STATE_REVISION:
        if (depth==3&&nodeType==1) {
            if (strcmp(name, "format")==0) {
                state = STATE_FORMAT;
            } else if (strcmp(name, "text")==0) {
                state = STATE_TEXT;
            }
        } else if (depth==2) state = STATE_PAGE;
        break;
    case STATE_FORMAT:
        if (depth==4&&nodeType==3&&strcmp(name, "#text")==0) {
            if (value) {
                correctFormat = !strcmp(value, "text/x-wiki");
                if (!correctFormat) {
                    fprintf(stderr, "Unexpected format: '%s', expected 'text/x-wiki'\n", value);
                }
            }
            state = STATE_REVISION;
        } else if (depth==3) state = STATE_REVISION;
        break;
    case STATE_TEXT:
        if (depth==4&&nodeType==3&&strcmp(name, "#text")==0) {
            if (hasTitle&&correctFormat&&value&&noColon(title)) {
                printf("%s", title);
                putchar('\0');
                printlinks(value, strlen(value));
                putchar('\n');
            } else if (hasTitle) {
//                fprintf(stderr, "Ignored page called '%s'\n", title);
            } else {
                fprintf(stderr, "Ignored page due to lack of title\n");
            }
            state = STATE_REVISION;
        } else if (depth==3) state = STATE_REVISION;
        break;
    }


/*    printf("%d %d %s %d %d", 
	    depth,
	    nodeType,
	    name,
	    xmlTextReaderIsEmptyElement(reader),
	    xmlTextReaderHasValue(reader));
    if (value == NULL)
	printf("\n");
    else {
        if (xmlStrlen(value) > 40)
            printf(" %.40s...\n", value);
        else
	    printf(" %s\n", value);
    }*/
}

/**
 * streamFile:
 * @filename: the file name to parse
 *
 * Parse and print information about an XML file.
 */
static void
streamFile(const char *filename) {
    xmlTextReaderPtr reader;
    int ret;

    reader = xmlReaderForFile(filename, NULL, 0);
    if (reader != NULL) {
        ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            processNode(reader);
            ret = xmlTextReaderRead(reader);
        }
        xmlFreeTextReader(reader);
        if (ret != 0) {
            fprintf(stderr, "%s : failed to parse\n", filename);
        }
    } else {
        fprintf(stderr, "Unable to open %s\n", filename);
    }
}

int main(int argc, char **argv) {
    if (argc != 2)
        return(1);

    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION

    streamFile(argv[1]);

    /*
     * Cleanup function for the XML library.
     */
    xmlCleanupParser();
    /*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();
    return(0);
}

#else
int main(void) {
    fprintf(stderr, "XInclude support not compiled in\n");
    exit(1);
}
#endif
