#include <inttypes.h>
struct wikiNode {
    int8_t dist_a;
    int8_t dist_b;
    int16_t forward_length;
    int16_t backward_length;
    int32_t references[];
};
// this struct should be made @ runtime by combining title and node data
// title variable type might change to int32_t
// titles are sorted
// struct should probably be packed
struct wikiArticle {
    int32_t node;
    char * title;
};
