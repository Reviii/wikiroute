#include <assert.h>
// buffer is an automatically resizing array of chars
struct buffer {
    union {
        char * content;
        uint32_t * u32content;
    };
    size_t used;
    size_t _size;
};
struct buffer bufferCreate();
struct buffer bufferCopy(struct buffer buf);
void bufferCompact(struct buffer * buf);
static inline char * bufferAdd(struct buffer * buf, size_t amount) {
    size_t res = buf->used;
    while (buf->used+amount>buf->_size) {
        buf->_size *= 2;
        buf->content = realloc(buf->content, buf->_size);
        assert(buf->content);
    }
    buf->used += amount;
    return (buf->content + res);
}

/* examples
1. copy string to buffer and print it
int ref = buffer.used;
strcpy(bufferAdd(&buffer, strlen(string)+1), string);
puts(buffer.content+ref);
*/
