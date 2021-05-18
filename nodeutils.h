uint32_t * getNodeOffsets(char * nodeData, size_t nodeDataLength, size_t * nodeCount);
size_t * getTitleOffsets(FILE * f, size_t * titleCount);
char * getTitle(FILE * titles, size_t * titleOffsets, size_t id);
char * nodeOffsetToTitle(FILE * titles, uint32_t * nodeOffsets, size_t * titleOffsets, size_t nodeCount, uint32_t nodeOffset);
void normalizeTitle(char * title);
uint32_t titleToNodeOffset(FILE * titles, uint32_t * nodeOffsets, size_t * titleOffsets, size_t nodeCount, char * title);
