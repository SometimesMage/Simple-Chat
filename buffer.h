#ifndef CHATSERVER_BUFFER_H
#define CHATSERVER_BUFFER_H
#define Byte unsigned char

typedef struct buffer {
    int size;
    Byte *buffer;
    int position;
    int limit;
} Buffer;

Buffer *buffer_create(int size);

Buffer *buffer_copy(Buffer *buffer);

Byte buffer_get(Buffer *buffer);

Byte buffer_get_at(Buffer *buffer, int index);

void buffer_set(Buffer *buffer, int index, Byte byte);

void buffer_put(Buffer *buffer, Byte byte);

void buffer_flip(Buffer *buffer);

void buffer_reset(Buffer *buffer);

void buffer_free(Buffer *buffer);

#endif //CHATSERVER_BUFFER_H
