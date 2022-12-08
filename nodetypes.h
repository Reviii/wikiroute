#include <inttypes.h>
typedef uint32_t nodeRef;
struct wikiNode {
    union {
        struct {
            uint8_t dist_a;
            uint8_t dist_b;
        };
        uint16_t redirect;
        bool linked;
    };
    uint16_t forward_length;
    union {
        uint32_t backward_length;
        nodeRef id;
        nodeRef linked_by;
    };
    nodeRef references[];
};
