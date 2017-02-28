#include <memory.h>
#include <malloc.h>
#include <stdio.h>
#include "../list.h"
#include "../buffer.h"
#include "client.h"

Client *client_create(int socket_fd) {
    Client *client = malloc(sizeof(Client));
    client->id = socket_fd;
    client->channel = DEFAULT_CHANNEL;
    memset(client->name, 0, sizeof(client->name));
    client->readPacket = NULL;
    client->writeQueue = list_create();
    return client;
}

void client_set_name(Client *client, char *name) {
    if (strlen(name) > 15) {
        fprintf(stderr, "Name needs to be 15 characters or less (client_set_name).");
        return;
    }

    memset(client->name, 0, sizeof(client->name));
    strcpy(client->name, name);
}

void client_add_write(Client *client, Buffer *buffer) {
    Buffer *writeBuffer = buffer_copy(buffer);
    list_add(client->writeQueue, writeBuffer);
}

Buffer *client_peek_write(Client *client) {
    return list_get(client->writeQueue, 0);
}

Buffer *client_poll_write(Client *client) {
    return list_remove(client->writeQueue, 0);
}

void client_print(Client *client) {
    printf("Client: {id: %d, name: %s, buffer: %p, writeQueue: %p}\n", client->id, client->name, client->readPacket,
           client->writeQueue);
}

void client_free(Client *client) {
    if (client == NULL) {
        fprintf(stderr, "Passed in client was NULL (client_free).\n");
        return;
    }

    if (client->readPacket != NULL) {
        buffer_free(client->readPacket);
    }

    if (client->writeQueue != NULL) {
        list_free(client->writeQueue, (void (*)(void *)) &buffer_free);
    }

    free(client);
}