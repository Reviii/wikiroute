#include <inttypes.h>
typedef uint32_t nodeRef;
struct wikiNode {
    union {
        struct {
            uint8_t dist_a;
            uint8_t dist_b;
        };
        uint16_t redirect;
    };
    uint16_t forward_length;
    uint32_t backward_length;
    nodeRef references[];
};
