#ifndef CHATSERVER_CLIENT_H
#define CHATSERVER_CLIENT_H

#define DEFAULT_CHANNEL GLOBAL_CHANNEL
#define GLOBAL_CHANNEL 'g'
#define IOS_CHANNEL 'i'
#define ANDROID_CHANNEL 'a'
#define PRIVATE_CHANNEL 'p'
#define SERVER_CHANNEL 's'

#include "../list.h"
#include "../buffer.h"

typedef struct client {
    //Also the File Descriptor
    int id;
    char channel;
    char name[15];
    Buffer *readPacket;
    List *writeQueue;
} Client;

Client *client_create(int socket_fd);

void client_set_name(Client *client, char *name);

void client_add_write(Client *client, Buffer *buffer);

Buffer *client_peek_write(Client *client);

Buffer *client_poll_write(Client *client);

void client_print(Client *client);

void client_free(Client *client);

#endif //CHATSERVER_CLIENT_H
