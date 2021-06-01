#define ITERATEFILEBUFSIZE 1024
#define iterateFile(FILE, VARNAME, CODE) \
if (1) {\
    char iterateFileBuf[ITERATEFILEBUFSIZE];\
    int iterateFileLen;\
    while ((iterateFileLen=fread(iterateFileBuf, 1, ITERATEFILEBUFSIZE, FILE))) {\
        for (int iterateFileI=0;iterateFileI<iterateFileLen;iterateFileI++) {\
            char VARNAME = iterateFileBuf[iterateFileI];\
            CODE\
        }\
    }\
}
