nodeRef * getNodeOffsets(char * nodeData, size_t nodeDataLength, size_t * nodeCount);
size_t * getTitleOffsets(FILE * f, size_t * titleCount);
char * getTitle(FILE * titles, size_t * titleOffsets, size_t id);
char * getJSONTitle(FILE * titles, size_t * titleOffsets, size_t id);
nodeRef nodeOffsetToId(nodeRef * nodeOffsets, size_t nodeCount, nodeRef nodeOffset);
char * nodeOffsetToTitle(FILE * titles, nodeRef * nodeOffsets, size_t * titleOffsets, size_t nodeCount, nodeRef nodeOffset);
char * nodeOffsetToJSONTitle(FILE * titles, nodeRef * nodeOffsets, size_t * titleOffsets, size_t nodeCount, nodeRef nodeOffset);
void normalizeTitle(char * title);
nodeRef titleToNodeId(FILE * titles, size_t * titleOffsets, size_t nodeCount, char * title);
nodeRef titleToNodeOffset(FILE * titles, nodeRef * nodeOffsets, size_t * titleOffsets, size_t nodeCount, char * title);
static inline struct wikiNode * getNode(char * nodeData, nodeRef offset) {
    return (struct wikiNode *) (nodeData + offset);
}
