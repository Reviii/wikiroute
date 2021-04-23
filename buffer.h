// buffer is an automatically resizing array of chars
struct buffer {
    char * content;
    size_t used;
    size_t _size;
};
struct buffer bufferCreate();
char * bufferAdd(struct buffer * buf, size_t amount);
void bufferCompact(struct buffer * buf);


/* examples
1. copy string to buffer and print it
int ref = buffer.used;
strcpy(bufferAdd(&buffer, strlen(string)+1), string);
puts(buffer.content+ref);
*/
