#include <inttypes.h>
struct wikiNode {
    uint8_t dist_a;
    uint8_t dist_b;
    uint16_t forward_length;
    uint32_t backward_length;
    uint32_t references[];
};
// this struct should be made @ runtime by combining title and node data
// title variable type might change to int32_t
// titles are sorted
// struct should probably be packed
struct wikiArticle {
    int32_t node;
    char * title;
};
