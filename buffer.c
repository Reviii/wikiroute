#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
struct buffer bufferCreate() {
    struct buffer res = {{NULL}, 0, 1024};
    res.content = malloc(1024);
    assert(res.content);
    return res;
}
struct buffer bufferDup(struct buffer buf) {
    struct buffer newBuf = buf;
    newBuf.content = malloc(newBuf._size);
    memcpy(newBuf.content, buf.content, newBuf.used);
    return newBuf;
}
void bufferCompact(struct buffer * buf) {
    buf->_size = buf->used;
    if (buf->_size==0) buf->_size=1;
    buf->content = realloc(buf->content, buf->_size);
    assert(buf->content);
}
void bufferExpand(struct buffer * buf, size_t amount) {
    size_t newsize_min = buf->used+amount;
    if (buf->_size<newsize_min) {
        while (buf->_size<newsize_min) buf->_size *= 2;
        buf->content = realloc(buf->content,buf->_size);
    }
}
