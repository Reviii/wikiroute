enum fieldType {TYPE_INT, TYPE_STR,TYPE_IGNORE,TYPE_NULL,TYPE_INT_IGNORE};
union fieldData {
    char * string;
    int integer;
};
void parseSql(FILE * in, const char * startStr, const enum fieldType * fieldTypes, int recordSize, void (*fp) (const union fieldData *, void *), void * data);
