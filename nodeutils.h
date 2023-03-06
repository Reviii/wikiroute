struct wikiNode ** getNodes(char * nodeData, size_t nodeDataLength, size_t * nodeCount);
size_t * getTitleOffsets(FILE * f, size_t * titleCount);
char * getTitle(FILE * titles, size_t * titleOffsets, size_t id);
char * getJSONTitle(FILE * titles, size_t * titleOffsets, size_t id);
nodeRef nodeOffsetToId(nodeRef * nodeOffsets, size_t nodeCount, nodeRef nodeOffset);
void normalizeTitle(char * title);
nodeRef titleToNodeId(FILE * titles, size_t * titleOffsets, size_t nodeCount, char * title);
static size_t getNodeOffset(char * nodeData, struct wikiNode * node) {
    return (char *)node - nodeData;
}
