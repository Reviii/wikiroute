struct wikiNode ** getNodes(char * nodeData, size_t nodeDataLength, size_t * nodeCount);
size_t * getTitleOffsets(char * titleData, size_t titleDataLength, size_t * titleCount);
char * getTitle(char * titles, size_t * titleOffsets, size_t id);
char * getJSONTitle(char * titles, size_t * titleOffsets, size_t id);
nodeRef nodeOffsetToId(nodeRef * nodeOffsets, size_t nodeCount, nodeRef nodeOffset);
void normalizeTitle(char * title);
nodeRef titleToNodeId(char * titles, size_t * titleOffsets, size_t nodeCount, char * title);
static size_t getNodeOffset(char * nodeData, struct wikiNode * node) {
    return (char *)node - nodeData;
}
