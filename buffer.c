#include <memory.h>
#include <malloc.h>
#include <stdio.h>
#include "buffer.h"

Buffer *buffer_create(int size) {
    Buffer *buffer = malloc(sizeof(Buffer));
    buffer->buffer = malloc(size * sizeof(Byte));
    memset(buffer->buffer, 0, size * sizeof(Byte));
    buffer->size = size;
    buffer->position = 0;
    buffer->limit = buffer->size;
    return buffer;
}

Buffer *buffer_copy(Buffer *buffer) {
    Buffer *result = buffer_create(buffer->size);
    result->position = buffer->position;
    result->limit = buffer->limit;
    memcpy(result->buffer, buffer->buffer, result->size * sizeof(Byte));
    return result;
}

Byte buffer_get(Buffer *buffer) {
    if (buffer->position == buffer->limit) {
        fprintf(stderr, "Buffer's position is at it's limit (buffer_get).\n");
        return 0x00;
    }

    Byte result = buffer->buffer[buffer->position];
    buffer->position++;
    return result;
}

Byte buffer_get_at(Buffer *buffer, int index) {
    if (buffer->size <= index) {
        fprintf(stderr, "The passed in index is out of bounds (buffer_get_at).\n");
        return 0x00;
    }

    return buffer->buffer[index];
}

void buffer_set(Buffer *buffer, int index, Byte byte) {
    if(index >= buffer->size) {
        return;
    }

    buffer->buffer[index] = byte;
}

void buffer_put(Buffer *buffer, Byte byte) {
    if (buffer->position == buffer->limit) {
        fprintf(stderr, "Buffer's position is at it's limit (buffer_put).\n");
        return;
    }

    buffer->buffer[buffer->position] = byte;
    buffer->position++;
}

void buffer_flip(Buffer *buffer) {
    buffer->limit = buffer->position;
    buffer->position = 0;
}

void buffer_reset(Buffer *buffer) {
    buffer->position = 0;
    buffer->limit = buffer->size;
    memset(buffer->buffer, 0, buffer->size * sizeof(Byte));
}

void buffer_free(Buffer *buffer) {
    if(buffer) {
        free(buffer->buffer);
        free(buffer);
    }
}



