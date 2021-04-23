#include <assert.h>
#include <stdlib.h>

#include "buffer.h"
char * bufferAdd(struct buffer * buf, size_t amount) {
    size_t res = buf->used;
    while (buf->used+amount>buf->_size) {
        buf->_size *= 2;
        buf->content = realloc(buf->content, buf->_size);
        assert(buf->content);
    }
    buf->used += amount;
    return (buf->content + res);
}
struct buffer bufferCreate() {
    struct buffer res = {NULL, 0, 1024};
    res.content = malloc(1024);
    assert(res.content);
    return res;
}
void bufferCompact(struct buffer * buf) {
    buf->_size = buf->used;
    if (buf->_size==0) buf->_size=1;
    buf->content = realloc(buf->content, buf->_size);
    assert(buf->content);
}
